#ifndef MYSQL_COMM_H
#define MYSQL_COMM_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
// Try different include paths for MySQL/MariaDB
#if __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#elif __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#elif __has_include(<mysql.h>)
#include <mysql.h>
#else
#error "MySQL/MariaDB header not found. Install libmariadb-dev or libmysqlclient-dev"
#endif
#include "Logger.h"

// Application settings read from database
struct AppSettings
{
    int doors;                                  // Number of doors (and cameras)
    int stopBeginDelay;                         // Seconds before DOOR_OPEN to start video
    int stopEndDelay;                           // Seconds after DOOR_CLOSE to stop video
    int daysBeforeDeleteVideo;                  // Days to keep video files
    std::string cam0String;                     // Connection string for Camera 0
    std::string cam1String;                     // Connection string for Camera 1
    std::vector<std::string> remoteDBAddresses; // Remote DB addresses for replication
};

// SystemStatus structure matching the USB protocol
// Sent as 2 bytes: SystemStatus followed by ~SystemStatus
// #pragma pack(push, 1)
// struct SystemStatus_t {
//     uint8_t door_0      : 1;    // Door 0 state: 0=OPENED, 1=CLOSED
//     uint8_t door_1      : 1;    // Door 1 state: 0=OPENED, 1=CLOSED
//     uint8_t cover_0     : 1;    // Cover 0 state: 0=OPENED, 1=CLOSED
//     uint8_t cover_1     : 1;    // Cover 1 state: 0=OPENED, 1=CLOSED
//     uint8_t mainSupply  : 1;    // Main supply: 0=OFF, 1=ON
//     uint8_t ignition    : 1;    // Ignition: 0=OFF, 1=ON
//     uint8_t reserved    : 2;    // Reserved bits for future use / padding
// };
#pragma pack(pop)

// Event types for logging to database
enum class EventType
{
    Door0Open,
    Door0Close,
    Door1Open,
    Door1Close,
    Cover0Open,
    Cover0Close,
    Cover1Open,
    Cover1Close,
    MainSupplyOn,
    MainSupplyOff,
    IgnitionOn,
    IgnitionOff
};

class MySqlComm
{
private:
    std::shared_ptr<Logger> logger_;
    MYSQL *connection_;
    mutable std::mutex mutex_;

    // Database connection parameters
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    unsigned int port_;

    // Cached settings
    AppSettings settings_;
    bool settingsLoaded_;

    // Private methods
    bool connect();
    void disconnect();
    bool executeQuery(const std::string &query);
    MYSQL_RES *executeSelectQuery(const std::string &query);
    std::string escapeString(const std::string &str);
    std::string eventTypeToString(EventType event);

public:
    MySqlComm(std::shared_ptr<Logger> logger);
    ~MySqlComm();

    // Initialize connection to database
    bool initialize();

    // Load all settings from database
    bool loadSettings();

    // Get cached settings
    const AppSettings &getSettings() const { return settings_; }

    // Log event to database
    bool logEvent(EventType event, const std::string &timestamp);

    // Log video segment info to database
    bool logVideoSegment(int cameraId,
                         const std::string &startTime,
                         const std::string &stopTime,
                         const std::string &filename);

    // Check connection status
    bool isConnected() const;

    // Reconnect if connection lost
    bool reconnect();
};

#endif // MYSQL_COMM_H
