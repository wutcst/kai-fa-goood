#pragma once

#include "ServerRoom.hpp"

#include <SFML/Network.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace fireice {

class GameServer {
public:
    bool start();
    void run();
    void stop();

private:
    void processPackets();
    void simulateAllRooms();
    void broadcastAllRooms();
    std::string generateRoomCode();

    sf::UdpSocket socket_;
    bool running_ = false;
    std::unordered_map<std::string, std::unique_ptr<ServerRoom>> rooms_;
    std::chrono::steady_clock::time_point lastTick_;
    std::chrono::steady_clock::time_point lastBroadcast_;
};

}  // namespace fireice
