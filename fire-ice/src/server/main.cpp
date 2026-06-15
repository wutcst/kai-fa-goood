#include "GameServer.hpp"

#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
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

int main() {
    fireice::GameServer server;
    if (!server.start()) {
        return 1;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "Fire-Ice Online Server (multi-room)" << std::endl;
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
