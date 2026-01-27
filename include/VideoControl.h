#ifndef VIDEO_CONTROL_H
#define VIDEO_CONTROL_H

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <chrono>
#include <map>
#include "Common.h"
#include "MessageQueue.h"
#include "Logger.h"

struct CameraConfig {
    int id;
    std::string ipAddress;
    std::string rtspUrl;
    bool enabled;
};

class CameraRecorder {
private:
    CameraConfig config_;
    std::shared_ptr<Logger> logger_;
    
    std::atomic<bool> running_;
    std::thread recordThread_;
    
    std::string currentVideoFile_;
    std::chrono::system_clock::time_point currentFileStartTime_;
    
    FILE* ffmpegProcess_;
    std::mutex fileMutex_;
    
    std::string sourceDir_;
    std::string outputDir_;
    
    void recordLoop();
    bool startFFmpeg();
    void stopFFmpeg();
    std::string generateFilename();
    
public:
    CameraRecorder(const CameraConfig& config, std::shared_ptr<Logger> logger);
    ~CameraRecorder();
    
    void start();
    void stop();
    bool isRunning() const { return running_; }
    
    void processStartStopMessage(const StartStopMessage& msg);
    
private:
    void extractAndProcessSegment(const std::string& sourceFile,
                                  std::chrono::system_clock::time_point startTime,
                                  std::chrono::system_clock::time_point stopTime);
};

class VideoControl {
private:
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<MessageQueue<Message>> messageQueue_;
    
    std::vector<std::unique_ptr<CameraRecorder>> cameras_;
    std::thread messageThread_;
    std::atomic<bool> running_;
    
    void messageLoop();
    bool loadConfiguration();
    
public:
    VideoControl(std::shared_ptr<Logger> logger,
                std::shared_ptr<MessageQueue<Message>> messageQueue);
    ~VideoControl();
    
    bool initialize();
    void start();
    void stop();
};

#endif // VIDEO_CONTROL_H
