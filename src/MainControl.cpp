#include "MainControl.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <cstring>
#include <sys/ioctl.h>

MainControl::MainControl(std::shared_ptr<Logger> logger,
                         std::shared_ptr<MessageQueue<Message>> videoControlQueue,
                         std::shared_ptr<MySqlComm> dbComm)
    : logger_(logger), videoControlQueue_(videoControlQueue), dbComm_(dbComm),
      running_(false), serialFd_(-1)
{
    // Initialize status to default (all doors open, power off)
    currentStatus_ = SystemStatus_t();
    previousStatus_ = SystemStatus_t();
}

MainControl::~MainControl()
{
    stop();
}

void MainControl::updateSettings(int stopBeginDelay, int stopEndDelay)
{
    stopBeginDelay_ = stopBeginDelay;
    stopEndDelay_ = stopEndDelay;
    logger_->log("MainControl: Updated delays - stopBeginDelay=" +
                 std::to_string(stopBeginDelay_) + "s, stopEndDelay=" +
                 std::to_string(stopEndDelay_) + "s");
}

bool MainControl::findCH340Device()
{
    // Search for CH340 USB-Serial device
    DIR *dir = opendir("/sys/bus/usb-serial/devices");
    if (!dir)
    {
        logger_->logError("Cannot open /sys/bus/usb-serial/devices");
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;

        std::string devicePath = "/dev/" + std::string(entry->d_name);

        // Check if this is a CH340 device by reading driver info
        std::string driverPath = "/sys/bus/usb-serial/devices/" +
                                 std::string(entry->d_name) + "/driver";

        char linkPath[256];
        ssize_t len = readlink(driverPath.c_str(), linkPath, sizeof(linkPath) - 1);
        if (len != -1)
        {
            linkPath[len] = '\0';
            if (strstr(linkPath, "ch341") != nullptr)
            {
                serialPort_ = devicePath;
                closedir(dir);
                logger_->log("Found CH340 device: " + serialPort_);
                return true;
            }
        }
    }

    closedir(dir);

    // Fallback: try common ttyUSB devices
    for (int i = 0; i < 10; i++)
    {
        std::string device = "/dev/ttyUSB" + std::to_string(i);
        if (access(device.c_str(), F_OK) == 0)
        {
            serialPort_ = device;
            logger_->log("Using fallback device: " + serialPort_);
            return true;
        }
    }

    logger_->logError("CH340 device not found");
    return false;
}

bool MainControl::openSerialPort()
{
    serialFd_ = open(serialPort_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serialFd_ < 0)
    {
        logger_->logError("Failed to open serial port: " + serialPort_);
        return false;
    }

    logger_->log("Serial port opened: " + serialPort_);
    return true;
}

void MainControl::configureSerialPort()
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serialFd_, &tty) != 0)
    {
        logger_->logError("Error getting serial port attributes");
        return;
    }

    // Set baud rate to 115200
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // 8N1 mode
    tty.c_cflag &= ~PARENB; // No parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

    // Raw mode
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // Set timeout
    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(serialFd_, TCSANOW, &tty) != 0)
    {
        logger_->logError("Error setting serial port attributes");
    }

    logger_->log("Serial port configured: 115200 8N1");
}

bool MainControl::initialize()
{
    if (!findCH340Device())
    {
        return false;
    }

    if (!openSerialPort())
    {
        return false;
    }

    configureSerialPort();
    return true;
}

void MainControl::start()
{
    running_ = true;

    receiverThread_ = std::thread(&MainControl::receiverLoop, this);
    senderThread_ = std::thread(&MainControl::senderLoop, this);

    logger_->log("MainControl started");
}

void MainControl::stop()
{
    if (running_)
    {
        running_ = false;
        outgoingQueue_.requestShutdown();

        if (receiverThread_.joinable())
        {
            receiverThread_.join();
        }

        if (senderThread_.joinable())
        {
            senderThread_.join();
        }

        if (serialFd_ >= 0)
        {
            close(serialFd_);
            serialFd_ = -1;
        }

        logger_->log("MainControl stopped");
    }
}

bool MainControl::validateStatusMessage(uint8_t status, uint8_t invStatus)
{
    // The second byte should be the bitwise NOT of the first byte
    return (status ^ invStatus) == 0xFF;
}

void MainControl::receiverLoop()
{
    uint8_t buffer[256];
    uint8_t pendingByte = 0;
    bool havePendingByte = false;

    while (running_)
    {
        int bytesRead = read(serialFd_, buffer, sizeof(buffer));

        if (bytesRead > 0)
        {
            for (int i = 0; i < bytesRead; i++)
            {
                if (!havePendingByte)
                {
                    // First byte of the pair - this is SystemStatus
                    pendingByte = buffer[i];
                    havePendingByte = true;
                }
                else
                {
                    // Second byte - this should be ~SystemStatus!!!
                    uint8_t invByte = buffer[i];

                    if (validateStatusMessage(pendingByte, invByte))
                    {
                        // Valid message received
                        SystemStatus_t newStatus = SystemStatus_t::fromByte(pendingByte);

                        logger_->logCommand("Received valid SystemStatus: 0x" +
                                            std::string(1, "0123456789ABCDEF"[pendingByte >> 4]) +
                                            std::string(1, "0123456789ABCDEF"[pendingByte & 0x0F]));

                        processSystemStatus(newStatus);
                    }
                    else
                    {
                        // Invalid message - validation failed
                        logger_->logError("Invalid SystemStatus message: status=0x" +
                                          std::string(1, "0123456789ABCDEF"[pendingByte >> 4]) +
                                          std::string(1, "0123456789ABCDEF"[pendingByte & 0x0F]) +
                                          " inv=0x" +
                                          std::string(1, "0123456789ABCDEF"[invByte >> 4]) +
                                          std::string(1, "0123456789ABCDEF"[invByte & 0x0F]));
                    }

                    havePendingByte = false;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void MainControl::senderLoop()
{
    while (running_)
    {
        auto cmdOpt = outgoingQueue_.tryPop(std::chrono::milliseconds(100));

        if (cmdOpt.has_value())
        {
            uint8_t byte = static_cast<uint8_t>(cmdOpt.value());
            ssize_t written = write(serialFd_, &byte, 1);

            if (written < 0)
            {
                logger_->logError("Failed to write to serial port");
            }
            else
            {
                logger_->log("Sent command: 0x" +
                             std::string(1, "0123456789ABCDEF"[byte >> 4]) +
                             std::string(1, "0123456789ABCDEF"[byte & 0x0F]));
            }
        }
    }
}

void MainControl::processSystemStatus(const SystemStatus_t &newStatus)
{
    auto now = std::chrono::system_clock::now();

    // Compare with previous status and log changes
    compareAndLogChanges(currentStatus_, newStatus);

    // Update previous status
    previousStatus_ = currentStatus_;
    currentStatus_ = newStatus;

    // Handle door state changes for video recording

    // Door 0: was closed (1), now open (0)
    if (previousStatus_.door_0 == 1 && newStatus.door_0 == 0)
    {
        door0OpenTime_ = now;
        door0Open_ = true;
        sendCommand(PeripheralCommand::Cam0ON);
        sendCommand(PeripheralCommand::Light0ON);
        logger_->log("Door 0 opened - camera and light ON");
    }
    // Door 0: was open (0), now closed (1)
    else if (previousStatus_.door_0 == 0 && newStatus.door_0 == 1)
    {
        if (door0Open_)
        {
            door0Open_ = false;

            // Calculate start and stop times with delays
            // start_date_time = datetime(Door open) - stopBeginDelay
            // stop_date_time = datetime(Door close) + stopEndDelay
            auto startTime = door0OpenTime_ - std::chrono::seconds(stopBeginDelay_);
            auto stopTime = now + std::chrono::seconds(stopEndDelay_);

            // Send single message with both start and stop times
            auto msg = Message::createStartStop(0, startTime, stopTime);
            videoControlQueue_->push(msg);

            // Turn off camera and light after the delay
            sendCommand(PeripheralCommand::Cam0OFF);
            sendCommand(PeripheralCommand::Light0OFF);

            logger_->log("Door 0 closed - sending video segment request with delays");
        }
    }

    // Door 1: was closed (1), now open (0)
    if (previousStatus_.door_1 == 1 && newStatus.door_1 == 0)
    {
        door1OpenTime_ = now;
        door1Open_ = true;
        sendCommand(PeripheralCommand::Cam1ON);
        sendCommand(PeripheralCommand::Light1ON);
        logger_->log("Door 1 opened - camera and light ON");
    }
    // Door 1: was open (0), now closed (1)
    else if (previousStatus_.door_1 == 0 && newStatus.door_1 == 1)
    {
        if (door1Open_)
        {
            door1Open_ = false;

            // Calculate start and stop times with delays
            auto startTime = door1OpenTime_ - std::chrono::seconds(stopBeginDelay_);
            auto stopTime = now + std::chrono::seconds(stopEndDelay_);

            // Send single message with both start and stop times
            auto msg = Message::createStartStop(1, startTime, stopTime);
            videoControlQueue_->push(msg);

            // Turn off camera and light after the delay
            sendCommand(PeripheralCommand::Cam1OFF);
            sendCommand(PeripheralCommand::Light1OFF);

            logger_->log("Door 1 closed - sending video segment request with delays");
        }
    }
}

void MainControl::compareAndLogChanges(const SystemStatus_t &oldStatus, const SystemStatus_t &newStatus)
{
    auto now = std::chrono::system_clock::now();
    std::string timestamp = formatTimestamp(now);

    // Check Door 0
    if (oldStatus.door_0 != newStatus.door_0)
    {
        EventType event = (newStatus.door_0 == 0) ? EventType::Door0Open : EventType::Door0Close;
        if (dbComm_)
        {
            dbComm_->logEvent(event, timestamp);
        }
        logger_->log("Status change: Door 0 " + std::string((newStatus.door_0 == 0) ? "OPENED" : "CLOSED"));
    }

    // Check Door 1
    if (oldStatus.door_1 != newStatus.door_1)
    {
        EventType event = (newStatus.door_1 == 0) ? EventType::Door1Open : EventType::Door1Close;
        if (dbComm_)
        {
            dbComm_->logEvent(event, timestamp);
        }
        logger_->log("Status change: Door 1 " + std::string((newStatus.door_1 == 0) ? "OPENED" : "CLOSED"));
    }

    // Check Cover 0
    if (oldStatus.cover_0 != newStatus.cover_0)
    {
        EventType event = (newStatus.cover_0 == 0) ? EventType::Cover0Open : EventType::Cover0Close;
        if (dbComm_)
        {
            dbComm_->logEvent(event, timestamp);
        }
        logger_->log("Status change: Cover 0 " + std::string((newStatus.cover_0 == 0) ? "OPENED" : "CLOSED"));
    }

    // Check Cover 1
    if (oldStatus.cover_1 != newStatus.cover_1)
    {
        EventType event = (newStatus.cover_1 == 0) ? EventType::Cover1Open : EventType::Cover1Close;
        if (dbComm_)
        {
            dbComm_->logEvent(event, timestamp);
        }
        logger_->log("Status change: Cover 1 " + std::string((newStatus.cover_1 == 0) ? "OPENED" : "CLOSED"));
    }

    // Check Main Supply
    if (oldStatus.mainSupply != newStatus.mainSupply)
    {
        EventType event = (newStatus.mainSupply == 1) ? EventType::MainSupplyOn : EventType::MainSupplyOff;
        if (dbComm_)
        {
            dbComm_->logEvent(event, timestamp);
        }
        logger_->log("Status change: Main Supply " + std::string((newStatus.mainSupply == 1) ? "ON" : "OFF"));
    }

    // Check Ignition
    if (oldStatus.ignition != newStatus.ignition)
    {
        EventType event = (newStatus.ignition == 1) ? EventType::IgnitionOn : EventType::IgnitionOff;
        if (dbComm_)
        {
            dbComm_->logEvent(event, timestamp);
        }
        logger_->log("Status change: Ignition " + std::string((newStatus.ignition == 1) ? "ON" : "OFF"));
    }
}

std::string MainControl::getCommandName(ReceivedCommand cmd)
{
    switch (cmd)
    {
    case ReceivedCommand::Door0_Open:
        return "Door0_Open";
    case ReceivedCommand::Door0_Close:
        return "Door0_Close";
    case ReceivedCommand::Door1_Open:
        return "Door1_Open";
    case ReceivedCommand::Door1_Close:
        return "Door1_Close";
    case ReceivedCommand::MainSupplyON:
        return "MainSupplyON";
    case ReceivedCommand::MainSupplyOFF:
        return "MainSupplyOFF";
    case ReceivedCommand::IgnitionON:
        return "IgnitionON";
    case ReceivedCommand::IgnitionOFF:
        return "IgnitionOFF";
    case ReceivedCommand::Cover0Opened:
        return "Cover0Opened";
    case ReceivedCommand::Cover0Closed:
        return "Cover0Closed";
    case ReceivedCommand::Cover1Opened:
        return "Cover1Opened";
    case ReceivedCommand::Cover1Closed:
        return "Cover1Closed";
    default:
        return "Unknown";
    }
}

void MainControl::processReceivedCommand(ReceivedCommand cmd)
{
    // Legacy command processing - kept for backward compatibility
    // This method is no longer used as we now process SystemStatus messages
    auto now = std::chrono::system_clock::now();

    switch (cmd)
    {
    case ReceivedCommand::Door0_Open:
        door0OpenTime_ = now;
        door0Open_ = true;
        sendCommand(PeripheralCommand::Cam0ON);
        sendCommand(PeripheralCommand::Light0ON);
        break;

    case ReceivedCommand::Door0_Close:
        if (door0Open_)
        {
            door0Open_ = false;
            auto startTime = door0OpenTime_ - std::chrono::seconds(stopBeginDelay_);
            auto stopTime = now + std::chrono::seconds(stopEndDelay_);
            auto msg = Message::createStartStop(0, startTime, stopTime);
            videoControlQueue_->push(msg);
            sendCommand(PeripheralCommand::Cam0OFF);
            sendCommand(PeripheralCommand::Light0OFF);
        }
        break;

    case ReceivedCommand::Door1_Open:
        door1OpenTime_ = now;
        door1Open_ = true;
        sendCommand(PeripheralCommand::Cam1ON);
        sendCommand(PeripheralCommand::Light1ON);
        break;

    case ReceivedCommand::Door1_Close:
        if (door1Open_)
        {
            door1Open_ = false;
            auto startTime = door1OpenTime_ - std::chrono::seconds(stopBeginDelay_);
            auto stopTime = now + std::chrono::seconds(stopEndDelay_);
            auto msg = Message::createStartStop(1, startTime, stopTime);
            videoControlQueue_->push(msg);
            sendCommand(PeripheralCommand::Cam1OFF);
            sendCommand(PeripheralCommand::Light1OFF);
        }
        break;

    default:
        break;
    }
}

void MainControl::sendCommand(PeripheralCommand cmd)
{
    outgoingQueue_.push(cmd);
}
