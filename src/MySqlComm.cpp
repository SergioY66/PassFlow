#include "MySqlComm.h"
#include <iostream>
#include <sstream>
#include <string.h>

MySqlComm::MySqlComm(std::shared_ptr<Logger> logger)
    : logger_(logger), connection_(nullptr), settingsLoaded_(false)
{
    // Database connection parameters for local MariaDB
    host_ = "127.0.0.1";
    user_ = "bus";
    password_ = "njkmrjbus";
    database_ = "busLocal";
    port_ = 3306;
}

MySqlComm::~MySqlComm()
{
    disconnect();
}

bool MySqlComm::connect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ != nullptr)
    {
        return true; // Already connected
    }

    connection_ = mysql_init(nullptr);
    if (connection_ == nullptr)
    {
        logger_->logError("MySqlComm: mysql_init() failed");
        return false;
    }

    // Set connection timeout
    unsigned int timeout = 10;
    mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // Enable auto-reconnect
    bool reconnect = true;
    mysql_options(connection_, MYSQL_OPT_RECONNECT, &reconnect);

    if (mysql_real_connect(connection_,
                           host_.c_str(),
                           user_.c_str(),
                           password_.c_str(),
                           database_.c_str(),
                           port_,
                           nullptr,
                           0) == nullptr)
    {
        logger_->logError("MySqlComm: Connection failed - " +
                          std::string(mysql_error(connection_)));
        mysql_close(connection_);
        connection_ = nullptr;
        return false;
    }

    // Set character encoding
    mysql_set_character_set(connection_, "utf8mb4");

    logger_->log("MySqlComm: Connected to database " + database_ + " at " + host_);
    return true;
}

void MySqlComm::disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ != nullptr)
    {
        mysql_close(connection_);
        connection_ = nullptr;
        logger_->log("MySqlComm: Disconnected from database");
    }
}

bool MySqlComm::isConnected() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ == nullptr)
    {
        return false;
    }

    return mysql_ping(connection_) == 0;
}

bool MySqlComm::reconnect()
{
    disconnect();
    return connect();
}

bool MySqlComm::executeQuery(const std::string &query)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ == nullptr)
    {
        logger_->logError("MySqlComm: Not connected to database");
        return false;
    }

    if (mysql_query(connection_, query.c_str()) != 0)
    {
        logger_->logError("MySqlComm: Query failed - " +
                          std::string(mysql_error(connection_)) +
                          " Query: " + query);
        return false;
    }

    return true;
}

MYSQL_RES *MySqlComm::executeSelectQuery(const std::string &query)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ == nullptr)
    {
        logger_->logError("MySqlComm: Not connected to database");
        return nullptr;
    }

    if (mysql_query(connection_, query.c_str()) != 0)
    {
        logger_->logError("MySqlComm: Query failed - " +
                          std::string(mysql_error(connection_)) +
                          " Query: " + query);
        return nullptr;
    }

    return mysql_store_result(connection_);
}

std::string MySqlComm::escapeString(const std::string &str)
{
    if (connection_ == nullptr || str.empty())
    {
        return str;
    }

    std::vector<char> buffer(str.length() * 2 + 1);
    mysql_real_escape_string(connection_, buffer.data(), str.c_str(), str.length());
    return std::string(buffer.data());
}

bool MySqlComm::initialize()
{
    if (!connect())
    {
        return false;
    }

    if (!loadSettings())
    {
        logger_->logError("MySqlComm: Failed to load settings from database");
        return false;
    }

    return true;
}

bool MySqlComm::loadSettings()
{
    // Read from 'settings' table
    std::string settingsQuery = "SELECT doors, stopBeginDelay, stopEndDelay, "
                                "daysBeforeDeliteVideo, cam0_string, cam1_string "
                                "FROM settings LIMIT 1";

    MYSQL_RES *result = executeSelectQuery(settingsQuery);
    if (result == nullptr)
    {
        logger_->logError("MySqlComm: Failed to query settings table");
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == nullptr)
    {
        logger_->logError("MySqlComm: No settings found in database");
        mysql_free_result(result);
        return false;
    }

    // Parse settings
    settings_.doors = row[0] ? std::stoi(row[0]) : 2;
    settings_.stopBeginDelay = row[1] ? std::stoi(row[1]) : 5;
    settings_.stopEndDelay = row[2] ? std::stoi(row[2]) : 5;
    settings_.daysBeforeDeleteVideo = row[3] ? std::stoi(row[3]) : 30;
    settings_.cam0String = row[4] ? row[4] : "";
    settings_.cam1String = row[5] ? row[5] : "";

    mysql_free_result(result);

    logger_->log("MySqlComm: Loaded settings - doors=" + std::to_string(settings_.doors) +
                 ", stopBeginDelay=" + std::to_string(settings_.stopBeginDelay) +
                 ", stopEndDelay=" + std::to_string(settings_.stopEndDelay) +
                 ", daysBeforeDeleteVideo=" + std::to_string(settings_.daysBeforeDeleteVideo));
    logger_->log("MySqlComm: Camera 0 URL: " + settings_.cam0String);
    logger_->log("MySqlComm: Camera 1 URL: " + settings_.cam1String);

    // Read from 'remoteDB' table
    std::string remoteDbQuery = "SELECT remoteDBAddress FROM remoteDB";

    result = executeSelectQuery(remoteDbQuery);
    if (result != nullptr)
    {
        settings_.remoteDBAddresses.clear();
        while ((row = mysql_fetch_row(result)) != nullptr)
        {
            if (row[0] != nullptr && strlen(row[0]) > 0)
            {
                settings_.remoteDBAddresses.push_back(row[0]);
                logger_->log("MySqlComm: Remote DB address: " + std::string(row[0]));
            }
        }
        mysql_free_result(result);
    }

    settingsLoaded_ = true;
    return true;
}

std::string MySqlComm::eventTypeToString(EventType event)
{
    switch (event)
    {
    case EventType::Door0Open:
        return "Door 0 open";
    case EventType::Door0Close:
        return "Door 0 closed";
    case EventType::Door1Open:
        return "Door 1 open";
    case EventType::Door1Close:
        return "Door 1 closed";
    case EventType::Cover0Open:
        return "Cover 0 open";
    case EventType::Cover0Close:
        return "Cover 0 closed";
    case EventType::Cover1Open:
        return "Cover 1 open";
    case EventType::Cover1Close:
        return "Cover 1 closed";
    case EventType::MainSupplyOn:
        return "Main supply ON";
    case EventType::MainSupplyOff:
        return "Main supply OFF";
    case EventType::IgnitionOn:
        return "Ignition ON";
    case EventType::IgnitionOff:
        return "Ignition OFF";
    default:
        return "Unknown event";
    }
}

bool MySqlComm::logEvent(EventType event, const std::string &timestamp)
{
    std::string eventStr = escapeString(eventTypeToString(event));
    std::string timestampStr = escapeString(timestamp);

    std::stringstream query;
    query << "INSERT INTO events (event, DateTime) VALUES ('"
          //   << static_cast<int>(event) << "', '"
          << eventStr << "', '"
          << timestampStr << "')";

    bool success = executeQuery(query.str());

    if (success)
    {
        logger_->log("MySqlComm: Logged event - " + eventTypeToString(event) +
                     " at " + timestamp);
    }

    return success;
}

bool MySqlComm::logVideoSegment(int cameraId,
                                const std::string &startTime,
                                const std::string &stopTime,
                                const std::string &filename)
{
    std::stringstream query;
    query << "INSERT INTO video_segments (camera_id, start_time, stop_time, filename) VALUES ("
          << cameraId << ", '"
          << escapeString(startTime) << "', '"
          << escapeString(stopTime) << "', '"
          << escapeString(filename) << "')";

    bool success = executeQuery(query.str());

    if (success)
    {
        logger_->log("MySqlComm: Logged video segment - Camera " + std::to_string(cameraId) +
                     " from " + startTime + " to " + stopTime);
    }

    return success;
}
