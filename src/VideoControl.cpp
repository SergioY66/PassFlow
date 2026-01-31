#include "VideoControl.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <algorithm>

// CameraRecorder Implementation

CameraRecorder::CameraRecorder(const CameraConfig &config, 
                               std::shared_ptr<Logger> logger,
                               std::shared_ptr<MySqlComm> dbComm)
    : config_(config), logger_(logger), dbComm_(dbComm), 
      running_(false), ffmpegProcess_(nullptr), daysBeforeDeleteVideo_(30)
{
    // Setup directories
    const char *home = getenv("HOME");
    std::string homeDir = home ? home : "";

    sourceDir_ = homeDir + "/PassFlow/Cam" + std::to_string(config_.id) + "Source";
    outputDir_ = homeDir + "/PassFlow/Cam" + std::to_string(config_.id);

    std::filesystem::create_directories(sourceDir_);
    std::filesystem::create_directories(outputDir_);
}

CameraRecorder::~CameraRecorder()
{
    stop();
}

std::string CameraRecorder::generateFilename()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_cam" << config_.id << ".mp4";

    return sourceDir_ + "/" + ss.str();
}

bool CameraRecorder::startFFmpeg()
{
    std::lock_guard<std::mutex> lock(fileMutex_);

    currentVideoFile_ = generateFilename();
    currentFileStartTime_ = std::chrono::system_clock::now();

    // Build ffmpeg command
    std::stringstream cmd;
    cmd << "ffmpeg -i \"" << config_.rtspUrl << "\" ";
    cmd << "-c:v copy -c:a copy -f mp4 -y \"" << currentVideoFile_ << "\" ";
    cmd << "2>&1 &"; // Run in background

    logger_->log("Starting FFmpeg for Camera " + std::to_string(config_.id) +
                 ": " + currentVideoFile_);

    int result = system(cmd.str().c_str());

    if (result != 0)
    {
        logger_->logError("Failed to start FFmpeg for Camera " +
                          std::to_string(config_.id));
        return false;
    }

    return true;
}

void CameraRecorder::stopFFmpeg()
{
    std::lock_guard<std::mutex> lock(fileMutex_);

    if (!currentVideoFile_.empty())
    {
        // Kill ffmpeg processes for this camera
        std::string killCmd = "pkill -f \"" + currentVideoFile_ + "\"";
        system(killCmd.c_str());

        // Wait a bit for graceful shutdown
        std::this_thread::sleep_for(std::chrono::seconds(1));

        logger_->log("Stopped recording: " + currentVideoFile_);
        currentVideoFile_.clear();
    }
}

void CameraRecorder::cleanupOldVideos()
{
    // Delete video files older than daysBeforeDeleteVideo_
    auto now = std::chrono::system_clock::now();
    auto cutoffTime = now - std::chrono::hours(24 * daysBeforeDeleteVideo_);
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(outputDir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
                auto fileTime = std::filesystem::last_write_time(entry);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                
                if (sctp < cutoffTime) {
                    std::filesystem::remove(entry.path());
                    logger_->log("Deleted old video: " + entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        logger_->logError("Error during video cleanup: " + std::string(e.what()));
    }
}

void CameraRecorder::start()
{
    running_ = true;
    recordThread_ = std::thread(&CameraRecorder::recordLoop, this);
    logger_->log("Camera " + std::to_string(config_.id) + " recorder started");
}

void CameraRecorder::stop()
{
    if (running_)
    {
        running_ = false;
        stopFFmpeg();

        if (recordThread_.joinable())
        {
            recordThread_.join();
        }

        logger_->log("Camera " + std::to_string(config_.id) + " recorder stopped");
    }
}

void CameraRecorder::recordLoop()
{
    // Start initial recording
    startFFmpeg();
    
    int cleanupCounter = 0;

    while (running_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if ffmpeg is still running, restart if needed
        std::string checkCmd = "pgrep -f \"" + currentVideoFile_ + "\" > /dev/null";
        if (system(checkCmd.c_str()) != 0 && running_)
        {
            logger_->logError("FFmpeg stopped unexpectedly, restarting...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            startFFmpeg();
        }
        
        // Periodic cleanup of old videos (every hour)
        cleanupCounter++;
        if (cleanupCounter >= 3600) {
            cleanupOldVideos();
            cleanupCounter = 0;
        }
    }
}

void CameraRecorder::processStartStopMessage(const StartStopMessage &msg)
{
    std::string oldFile;
    std::chrono::system_clock::time_point oldFileStart;

    {
        std::lock_guard<std::mutex> lock(fileMutex_);
        oldFile = currentVideoFile_;
        oldFileStart = currentFileStartTime_;
    }

    // Stop current recording and start new one
    stopFFmpeg();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    startFFmpeg();

    // Extract and process segment in background thread
    // msg already contains the adjusted start/stop times with delays applied
    std::thread([this, oldFile, oldFileStart, msg]()
                { 
                    extractAndProcessSegment(oldFile, msg.startTime, msg.stopTime);
                    
                    // Log to database
                    if (dbComm_) {
                        std::string startTimeStr = formatTimestamp(msg.startTime);
                        std::string stopTimeStr = formatTimestamp(msg.stopTime);
                        
                        // Generate output filename for logging
                        std::string dateDir = outputDir_ + "/" + getCurrentDateString();
                        std::string outputFile = dateDir + "/" +
                                                 formatTimestamp(msg.startTime) + "_" +
                                                 formatTimestamp(msg.stopTime) + ".mp4";
                        std::replace(outputFile.begin(), outputFile.end(), ' ', '_');
                        std::replace(outputFile.begin(), outputFile.end(), ':', '-');
                        
                        dbComm_->logVideoSegment(config_.id, startTimeStr, stopTimeStr, outputFile);
                    }
                })
        .detach();
}

void CameraRecorder::extractAndProcessSegment(
    const std::string &sourceFile,
    std::chrono::system_clock::time_point startTime,
    std::chrono::system_clock::time_point stopTime)
{
    if (sourceFile.empty() || !std::filesystem::exists(sourceFile))
    {
        logger_->logError("Source file not found: " + sourceFile);
        return;
    }

    // Calculate time offsets from file start
    // Note: startTime and stopTime already have delays applied
    auto fileStart = currentFileStartTime_;
    auto startOffset = std::chrono::duration_cast<std::chrono::seconds>(
                           startTime - fileStart)
                           .count();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                        stopTime - startTime)
                        .count();

    if (startOffset < 0)
        startOffset = 0;
    if (duration <= 0)
        duration = 1;

    // Create output directory with current date
    std::string dateDir = outputDir_ + "/" + getCurrentDateString();
    std::filesystem::create_directories(dateDir);

    // Generate output filename with start and stop times
    std::string outputFile = dateDir + "/" +
                             formatTimestamp(startTime) + "_" +
                             formatTimestamp(stopTime) + ".mp4";

    // Replace spaces and colons in filename
    std::replace(outputFile.begin(), outputFile.end(), ' ', '_');
    std::replace(outputFile.begin(), outputFile.end(), ':', '-');

    // Build ffmpeg command to extract, resize and recolor
    std::stringstream cmd;
    cmd << "ffmpeg -i \"" << sourceFile << "\" ";
    cmd << "-ss " << startOffset << " -t " << duration << " ";
    cmd << "-vf \"scale=640:480,hue=s=0.8\" "; // Resize and adjust color
    cmd << "-c:v libx264 -preset fast -crf 23 ";
    cmd << "-c:a copy -y \"" << outputFile << "\" ";
    cmd << "2>&1";

    logger_->log("Extracting segment: " + outputFile);
    logger_->log("  Start time: " + formatTimestamp(startTime));
    logger_->log("  Stop time: " + formatTimestamp(stopTime));
    logger_->log("  Duration: " + std::to_string(duration) + " seconds");

    int result = system(cmd.str().c_str());

    if (result == 0)
    {
        logger_->log("Successfully created segment: " + outputFile);
    }
    else
    {
        logger_->logError("Failed to create segment: " + outputFile);
    }
}

// VideoControl Implementation

VideoControl::VideoControl(std::shared_ptr<Logger> logger,
                           std::shared_ptr<MessageQueue<Message>> messageQueue,
                           std::shared_ptr<MySqlComm> dbComm)
    : logger_(logger), messageQueue_(messageQueue), dbComm_(dbComm), running_(false)
{
}

VideoControl::~VideoControl()
{
    stop();
}

bool VideoControl::loadConfiguration()
{
    // Get settings from database via MySqlComm
    if (!dbComm_) {
        logger_->logError("VideoControl: No database connection available");
        return false;
    }
    
    const AppSettings& settings = dbComm_->getSettings();
    
    // Configure cameras based on database settings
    int numDoors = settings.doors;
    
    logger_->log("VideoControl: Configuring " + std::to_string(numDoors) + " camera(s) from database settings");
    
    // Camera 0
    if (numDoors >= 1 && !settings.cam0String.empty()) {
        CameraConfig cam0;
        cam0.id = 0;
        cam0.rtspUrl = settings.cam0String;
        cam0.enabled = true;
        
        auto recorder = std::make_unique<CameraRecorder>(cam0, logger_, dbComm_);
        recorder->setDaysBeforeDeleteVideo(settings.daysBeforeDeleteVideo);
        cameras_.push_back(std::move(recorder));
        
        logger_->log("Camera 0 configured from DB: " + cam0.rtspUrl);
    }
    
    // Camera 1
    if (numDoors >= 2 && !settings.cam1String.empty()) {
        CameraConfig cam1;
        cam1.id = 1;
        cam1.rtspUrl = settings.cam1String;
        cam1.enabled = true;
        
        auto recorder = std::make_unique<CameraRecorder>(cam1, logger_, dbComm_);
        recorder->setDaysBeforeDeleteVideo(settings.daysBeforeDeleteVideo);
        cameras_.push_back(std::move(recorder));
        
        logger_->log("Camera 1 configured from DB: " + cam1.rtspUrl);
    }
    
    if (cameras_.empty()) {
        logger_->logError("VideoControl: No cameras configured from database");
        return false;
    }

    return true;
}

bool VideoControl::initialize()
{
    return loadConfiguration();
}

void VideoControl::start()
{
    running_ = true;

    // Start all camera recorders
    for (auto &camera : cameras_)
    {
        camera->start();
    }

    // Start message processing thread
    messageThread_ = std::thread(&VideoControl::messageLoop, this);

    logger_->log("VideoControl started");
}

void VideoControl::stop()
{
    if (running_)
    {
        running_ = false;
        messageQueue_->requestShutdown();

        if (messageThread_.joinable())
        {
            messageThread_.join();
        }

        // Stop all camera recorders
        for (auto &camera : cameras_)
        {
            camera->stop();
        }

        logger_->log("VideoControl stopped");
    }
}

void VideoControl::messageLoop()
{
    while (running_)
    {
        auto msgOpt = messageQueue_->tryPop(std::chrono::milliseconds(100));

        if (msgOpt.has_value())
        {
            Message msg = msgOpt.value();

            switch (msg.type)
            {
            case MessageType::StartStop:
            {
                auto startStop = std::get<StartStopMessage>(msg.data);
                int camId = startStop.cameraId;

                if (camId >= 0 && static_cast<size_t>(camId) < cameras_.size())
                {
                    // The message now contains start_date_time and stop_date_time
                    // with delays already applied by MainControl
                    cameras_[camId]->processStartStopMessage(startStop);

                    logger_->log("Processed StartStop for Camera " +
                                 std::to_string(camId) + 
                                 " - Start: " + formatTimestamp(startStop.startTime) +
                                 " Stop: " + formatTimestamp(startStop.stopTime));
                }
                else
                {
                    logger_->logError("Invalid camera ID: " + std::to_string(camId));
                }
                break;
            }

            case MessageType::Shutdown:
                running_ = false;
                break;

            default:
                break;
            }
        }
    }
}
