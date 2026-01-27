#include "MainControl.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <cstring>
#include <sys/ioctl.h>

MainControl::MainControl(std::shared_ptr<Logger> logger,
                         std::shared_ptr<MessageQueue<Message>> videoControlQueue)
    : logger_(logger), videoControlQueue_(videoControlQueue),
      running_(false), serialFd_(-1)
{
}

MainControl::~MainControl()
{
    stop();
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

void MainControl::receiverLoop()
{
    uint8_t buffer[256];

    while (running_)
    {
        int bytesRead = read(serialFd_, buffer, sizeof(buffer));

        if (bytesRead > 0)
        {
            // Process each byte as a command
            for (int i = 0; i < bytesRead; i++)
            {
                uint8_t cmdByte = buffer[i];
                ReceivedCommand cmd = static_cast<ReceivedCommand>(cmdByte);

                // Validate command is in valid range
                if (cmdByte >= 0x01 && cmdByte <= 0x0C)
                {
                    logger_->logCommand("Received command: 0x" +
                                        std::string(1, "0123456789ABCDEF"[cmdByte >> 4]) +
                                        std::string(1, "0123456789ABCDEF"[cmdByte & 0x0F]) +
                                        " (" + getCommandName(cmd) + ")");
                    processReceivedCommand(cmd);
                }
                else
                {
                    logger_->logError("Unknown command byte: 0x" +
                                      std::string(1, "0123456789ABCDEF"[cmdByte >> 4]) +
                                      std::string(1, "0123456789ABCDEF"[cmdByte & 0x0F]));
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
    auto now = std::chrono::system_clock::now();

    switch (cmd)
    {
    case ReceivedCommand::Door0_Open:
        door0OpenTime_ = now;
        door0Open_ = true;
        // Send Cam0ON and Light0ON
        sendCommand(PeripheralCommand::Cam0ON);
        sendCommand(PeripheralCommand::Light0ON);
        break;

    case ReceivedCommand::Door0_Close:
        if (door0Open_)
        {
            door0Open_ = false;
            // Send StartStop message to VideoControl
            auto msg = Message::createStartStop(0, door0OpenTime_, now);
            videoControlQueue_->push(msg);
            // Turn off camera and light
            sendCommand(PeripheralCommand::Cam0OFF);
            sendCommand(PeripheralCommand::Light0OFF);
        }
        break;

    case ReceivedCommand::Door1_Open:
        door1OpenTime_ = now;
        door1Open_ = true;
        // Send Cam1ON and Light1ON
        sendCommand(PeripheralCommand::Cam1ON);
        sendCommand(PeripheralCommand::Light1ON);
        break;

    case ReceivedCommand::Door1_Close:
        if (door1Open_)
        {
            door1Open_ = false;
            // Send StartStop message to VideoControl
            auto msg = Message::createStartStop(1, door1OpenTime_, now);
            videoControlQueue_->push(msg);
            // Turn off camera and light
            sendCommand(PeripheralCommand::Cam1OFF);
            sendCommand(PeripheralCommand::Light1OFF);
        }
        break;

    default:
        // Log other commands but don't process them yet
        break;
    }
}

void MainControl::sendCommand(PeripheralCommand cmd)
{
    outgoingQueue_.push(cmd);
}
