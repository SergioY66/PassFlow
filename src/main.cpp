#include <iostream>
#include <csignal>
#include <memory>
#include <atomic>
#include "Common.h"
#include "Logger.h"
#include "MessageQueue.h"
#include "MainControl.h"
#include "VideoControl.h"
#include "MySqlComm.h"

std::atomic<bool> g_running(true);

void signalHandler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char *argv[])
{
    std::cout << "PassFlow System Starting..." << std::endl;

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try
    {
        // Create shared logger
        auto logger = std::make_shared<Logger>();
        logger->log("=== PassFlow System Started ===");

        // Create MySqlComm module for database communication
        // This reads all settings from MariaDB on startup
        std::cout << "Initializing MySqlComm..." << std::endl;
        auto dbComm = std::make_shared<MySqlComm>(logger);
        
        if (!dbComm->initialize())
        {
            std::cerr << "Failed to initialize MySqlComm - database connection failed" << std::endl;
            logger->logError("Failed to initialize database connection");
            return 1;
        }
        
        // Get settings from database
        const AppSettings& settings = dbComm->getSettings();
        logger->log("Settings loaded from database:");
        logger->log("  - doors: " + std::to_string(settings.doors));
        logger->log("  - stopBeginDelay: " + std::to_string(settings.stopBeginDelay) + "s");
        logger->log("  - stopEndDelay: " + std::to_string(settings.stopEndDelay) + "s");
        logger->log("  - daysBeforeDeleteVideo: " + std::to_string(settings.daysBeforeDeleteVideo));
        logger->log("  - cam0_string: " + settings.cam0String);
        logger->log("  - cam1_string: " + settings.cam1String);
        logger->log("  - Remote DB addresses: " + std::to_string(settings.remoteDBAddresses.size()));

        // Create message queue for VideoControl
        auto videoControlQueue = std::make_shared<MessageQueue<Message>>();

        // Create MainControl block with database connection
        auto mainControl = std::make_unique<MainControl>(logger, videoControlQueue, dbComm);
        
        // Update MainControl with delay settings from database
        mainControl->updateSettings(settings.stopBeginDelay, settings.stopEndDelay);

        // Create VideoControl block with database connection
        auto videoControl = std::make_unique<VideoControl>(logger, videoControlQueue, dbComm);

        // Initialize components
        std::cout << "Initializing MainControl..." << std::endl;
        if (!mainControl->initialize())
        {
            std::cerr << "Failed to initialize MainControl" << std::endl;
            return 1;
        }

        std::cout << "Initializing VideoControl..." << std::endl;
        if (!videoControl->initialize())
        {
            std::cerr << "Failed to initialize VideoControl" << std::endl;
            return 1;
        }

        // Start components
        std::cout << "Starting components..." << std::endl;
        videoControl->start();
        mainControl->start();

        logger->log("All components started successfully");
        std::cout << "PassFlow System running. Press Ctrl+C to stop." << std::endl;

        // Main loop
        while (g_running)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Shutdown
        std::cout << "Shutting down components..." << std::endl;
        logger->log("Shutdown initiated");

        mainControl->stop();
        videoControl->stop();

        logger->log("=== PassFlow System Stopped ===");
        std::cout << "PassFlow System stopped." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
