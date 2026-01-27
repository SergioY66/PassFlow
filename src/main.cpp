#include <iostream>
#include <csignal>
#include <memory>
#include <atomic>
#include "Common.h"
#include "Logger.h"
#include "MessageQueue.h"
#include "MainControl.h"
#include "VideoControl.h"

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

        // Create message queue for VideoControl
        auto videoControlQueue = std::make_shared<MessageQueue<Message>>();

        // Create MainControl block
        auto mainControl = std::make_unique<MainControl>(logger, videoControlQueue);

        // Create VideoControl block
        auto videoControl = std::make_unique<VideoControl>(logger, videoControlQueue);

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
        // videoControl->start();
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
