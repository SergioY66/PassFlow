#ifndef MAIN_CONTROL_H
#define MAIN_CONTROL_H

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <map>
#include <chrono>
#include "Common.h"
#include "MessageQueue.h"
#include "Logger.h"

class MainControl {
private:
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<MessageQueue<Message>> videoControlQueue_;
    MessageQueue<PeripheralCommand> outgoingQueue_;
    
    std::thread receiverThread_;
    std::thread senderThread_;
    std::atomic<bool> running_;
    
    int serialFd_;
    std::string serialPort_;
    
    // Door state tracking
    std::chrono::system_clock::time_point door0OpenTime_;
    std::chrono::system_clock::time_point door1OpenTime_;
    bool door0Open_ = false;
    bool door1Open_ = false;
    
    // Private methods
    bool findCH340Device();
    bool openSerialPort();
    void configureSerialPort();
    void receiverLoop();
    void senderLoop();
    void processReceivedCommand(ReceivedCommand cmd);
    std::string getCommandName(ReceivedCommand cmd);
    
public:
    MainControl(std::shared_ptr<Logger> logger, 
                std::shared_ptr<MessageQueue<Message>> videoControlQueue);
    ~MainControl();
    
    bool initialize();
    void start();
    void stop();
    
    // Send command to peripheral
    void sendCommand(PeripheralCommand cmd);
};

#endif // MAIN_CONTROL_H
