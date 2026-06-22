#include "GameServer.hpp"

#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

std::mutex gMutex;
std::condition_variable gCv;
bool gShutdown = false;
std::string gPidFile;

void signalHandler(int /*sig*/) {
    std::lock_guard<std::mutex> lock(gMutex);
    gShutdown = true;
    gCv.notify_one();
}

void writePidFile(const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        std::cerr << "[Server] Failed to write pid file: " << path << std::endl;
        return;
    }
#ifdef _WIN32
    out << GetCurrentProcessId();
#else
    out << getpid();
#endif
}

void removePidFile() {
    if (gPidFile.empty()) {
        return;
    }
    std::remove(gPidFile.c_str());
}

bool redirectStreamsToLog(const std::string& logPath) {
    if (freopen(logPath.c_str(), "a", stdout) == nullptr) {
        return false;
    }
    if (freopen(logPath.c_str(), "a", stderr) == nullptr) {
        return false;
    }
    return true;
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [--pid-file <path>] [--log-file <path>]" << std::endl;
    std::cout << "  --pid-file   Write process id for background stop scripts" << std::endl;
    std::cout << "  --log-file   Append stdout/stderr to a log file" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string logFile;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "--pid-file" && i + 1 < argc) {
            gPidFile = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            logFile = argv[++i];
        } else {
            std::cerr << "[Server] Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (!logFile.empty() && !redirectStreamsToLog(logFile)) {
        std::cerr << "[Server] Failed to redirect output to log: " << logFile << std::endl;
        return 1;
    }

    fireice::GameServer server;
    if (!server.start()) {
        return 1;
    }

    if (!gPidFile.empty()) {
        writePidFile(gPidFile);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "Fire-Ice Online Server (multi-room)" << std::endl;
    std::cout << "Server running on UDP port 24567. Send SIGINT/SIGTERM to stop." << std::endl;
    if (!gPidFile.empty()) {
        std::cout << "PID file: " << gPidFile << std::endl;
    }
    if (!logFile.empty()) {
        std::cout << "Log file: " << logFile << std::endl;
    }

    std::thread serverThread([&server]() { server.run(); });

    {
        std::unique_lock<std::mutex> lock(gMutex);
        gCv.wait(lock, [] { return gShutdown; });
    }

    std::cout << "Shutting down..." << std::endl;
    server.stop();
    serverThread.join();
    removePidFile();

    return 0;
}
