#include "GameServer.hpp"

#include <algorithm>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace {

std::mutex gMutex;
std::condition_variable gCv;
bool gShutdown = false;

void signalHandler(int /*sig*/) {
    std::lock_guard<std::mutex> lock(gMutex);
    gShutdown = true;
    gCv.notify_one();
}

}  // namespace

int main(int argc, char* argv[]) {
    uint8_t initialLevel = 0;
    if (argc > 1) {
        initialLevel = static_cast<uint8_t>(std::max(0, std::stoi(argv[1]) - 1));
    }

    fireice::GameServer server;
    if (!server.start(initialLevel)) {
        return 1;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "Fire-Ice Online Server" << std::endl;
    std::cout << "Server running. Send SIGINT/SIGTERM to stop." << std::endl;

    std::thread serverThread([&server]() { server.run(); });

    {
        std::unique_lock<std::mutex> lock(gMutex);
        gCv.wait(lock, [] { return gShutdown; });
    }

    std::cout << "Shutting down..." << std::endl;
    server.stop();
    serverThread.join();

    return 0;
}
