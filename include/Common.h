#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <variant>

// Commands received from peripherals (1 byte each)
enum class ReceivedCommand : uint8_t {
    Door0_Open = 0x01,
    Door0_Close = 0x02,
    Door1_Open = 0x03,
    Door1_Close = 0x04,
    MainSupplyON = 0x05,
    MainSupplyOFF = 0x06,
    IgnitionON = 0x07,
    IgnitionOFF = 0x08,
    Cover0Opened = 0x09,
    Cover0Closed = 0x0A,
    Cover1Opened = 0x0B,
    Cover1Closed = 0x0C,
    Unknown = 0xFF
};

// Command codes sent to peripherals (1 byte each)
enum class PeripheralCommand : uint8_t {
    RedLedON = 0x10,
    RedLedOFF = 0x11,
    RedLedBlink = 0x12,
    GreenLedON = 0x13,
    GreenLedOFF = 0x14,
    GreenLedBlink = 0x15,
    BlueLedON = 0x16,
    BlueLedOFF = 0x17,
    BlueLedBlink = 0x18,
    Cam0ON = 0x19,
    Cam0OFF = 0x1A,
    Cam1ON = 0x1B,
    Cam1OFF = 0x1C,
    Light0ON = 0x1D,
    Light0OFF = 0x1E,
    Light1ON = 0x1F,
    Light1OFF = 0x20,
    FanON = 0x21,
    FanOFF = 0x22
};

// Inter-thread message types
enum class MessageType {
    StartStop,
    PeripheralCommand,
    Shutdown
};

// Structure for StartStop message
struct StartStopMessage {
    int cameraId;  // 0 or 1
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point stopTime;
};

// Generic message structure
struct Message {
    MessageType type;
    std::variant<std::monostate, StartStopMessage, PeripheralCommand> data;
    
    Message() : type(MessageType::Shutdown), data(std::monostate{}) {}
    
    static Message createStartStop(int camId, 
                                   std::chrono::system_clock::time_point start,
                                   std::chrono::system_clock::time_point stop) {
        Message msg;
        msg.type = MessageType::StartStop;
        StartStopMessage ssMsg;
        ssMsg.cameraId = camId;
        ssMsg.startTime = start;
        ssMsg.stopTime = stop;
        msg.data = ssMsg;
        return msg;
    }
    
    static Message createPeripheralCommand(PeripheralCommand cmd) {
        Message msg;
        msg.type = MessageType::PeripheralCommand;
        msg.data = cmd;
        return msg;
    }
    
    static Message createShutdown() {
        Message msg;
        msg.type = MessageType::Shutdown;
        msg.data = std::monostate{};
        return msg;
    }
};

// Utility function to format timestamp
inline std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Utility function to get current date string
inline std::string getCurrentDateString() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    return ss.str();
}

// Utility function to get datetime filename
inline std::string getDatetimeFilename(const std::string& suffix) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << suffix;
    return ss.str();
}

#endif // COMMON_H
