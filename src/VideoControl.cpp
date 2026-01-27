#include "VideoControl.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <algorithm>

// CameraRecorder Implementation

CameraRecorder::CameraRecorder(const CameraConfig &config, std::shared_ptr<Logger> logger)
    : config_(config), logger_(logger), running_(false), ffmpegProcess_(nullptr)
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
    // cmd << "ffmpeg -rtsp_transport tcp -i \"" << config_.rtspUrl << "\" ";
    cmd << "ffmpeg  -i \"" << config_.rtspUrl << "\" ";
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
    }
}

void CameraRecorder::processStartStopMessage(const StartStopMessage &msg)
{
    std::string oldFile;

    {
        std::lock_guard<std::mutex> lock(fileMutex_);
        oldFile = currentVideoFile_;
    }

    // Stop current recording and start new one
    stopFFmpeg();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    startFFmpeg();

    // Extract and process segment in background thread
    std::thread([this, oldFile, msg]()
                { extractAndProcessSegment(oldFile, msg.startTime, msg.stopTime); })
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

    // Calculate time offsets
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

    // Generate output filename
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
                           std::shared_ptr<MessageQueue<Message>> messageQueue)
    : logger_(logger), messageQueue_(messageQueue), running_(false)
{
}

VideoControl::~VideoControl()
{
    stop();
}

bool VideoControl::loadConfiguration()
{
    // For now, use hardcoded configuration
    // In production, this should read from a config file

    CameraConfig cam0;
    cam0.id = 0;
    cam0.ipAddress = "192.168.1.100";
    cam0.rtspUrl = "udp://@:8080 ";
    cam0.enabled = true;

    CameraConfig cam1;
    cam1.id = 1;
    cam1.ipAddress = "192.168.1.101";
    cam1.rtspUrl = "rtsp://admin:password@192.168.1.101:554/stream1";
    cam1.enabled = false; // Set to true if second camera exists

    // Create camera recorders
    if (cam0.enabled)
    {
        cameras_.push_back(std::make_unique<CameraRecorder>(cam0, logger_));
        logger_->log("Camera 0 configured: " + cam0.rtspUrl);
    }

    if (cam1.enabled)
    {
        cameras_.push_back(std::make_unique<CameraRecorder>(cam1, logger_));
        logger_->log("Camera 1 configured: " + cam1.rtspUrl);
    }

    return !cameras_.empty();
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

                if (camId >= 0 && camId < cameras_.size())
                {
                    cameras_[camId]->processStartStopMessage(startStop);

                    logger_->log("Processed StartStop for Camera " +
                                 std::to_string(camId));
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
