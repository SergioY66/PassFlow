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
#include "MySqlComm.h"

class MainControl {
private:
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<MessageQueue<Message>> videoControlQueue_;
    std::shared_ptr<MySqlComm> dbComm_;
    MessageQueue<PeripheralCommand> outgoingQueue_;
    
    std::thread receiverThread_;
    std::thread senderThread_;
    std::atomic<bool> running_;
    
    int serialFd_;
    std::string serialPort_;
    
    // Door state tracking with timestamps
    std::chrono::system_clock::time_point door0OpenTime_;
    std::chrono::system_clock::time_point door1OpenTime_;
    bool door0Open_ = false;
    bool door1Open_ = false;
    
    // SystemStatus tracking
    SystemStatus_t currentStatus_;
    SystemStatus_t previousStatus_;
    
    // Delays for video recording (read from DB)
    int stopBeginDelay_ = 5;  // seconds before door open
    int stopEndDelay_ = 5;    // seconds after door close
    
    // Private methods
    bool findCH340Device();
    bool openSerialPort();
    void configureSerialPort();
    void receiverLoop();
    void senderLoop();
    
    // New SystemStatus processing
    void processSystemStatus(const SystemStatus_t& newStatus);
    void compareAndLogChanges(const SystemStatus_t& oldStatus, const SystemStatus_t& newStatus);
    bool validateStatusMessage(uint8_t status, uint8_t invStatus);
    
    // Legacy command processing (kept for compatibility)
    void processReceivedCommand(ReceivedCommand cmd);
    std::string getCommandName(ReceivedCommand cmd);
    
public:
    MainControl(std::shared_ptr<Logger> logger, 
                std::shared_ptr<MessageQueue<Message>> videoControlQueue,
                std::shared_ptr<MySqlComm> dbComm);
    ~MainControl();
    
    bool initialize();
    void start();
    void stop();
    
    // Send command to peripheral
    void sendCommand(PeripheralCommand cmd);
    
    // Update settings from database
    void updateSettings(int stopBeginDelay, int stopEndDelay);
};

#endif // MAIN_CONTROL_H
