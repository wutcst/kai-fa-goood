#include "GameServer.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    uint8_t initialLevel = 0;
    if (argc > 1) {
        initialLevel = static_cast<uint8_t>(std::max(0, std::stoi(argv[1]) - 1));
    }

    fireice::GameServer server;
    if (!server.start(initialLevel)) {
        return 1;
    }

    std::cout << "Fire-Ice Online Server" << std::endl;
    std::cout << "Press Enter to stop..." << std::endl;

    std::thread serverThread([&server]() { server.run(); });
    std::cin.get();
    server.stop();
    serverThread.join();

    return 0;
}
