#pragma once

#include "Room.hpp"

#include <SFML/Network.hpp>

#include <array>

namespace fireice {

struct ClientEndpoint {
    sf::IpAddress address;
    unsigned short port = 0;
    bool active = false;
};

// 服务端房间：仿真 + 各槽位 UDP 端点
struct ServerRoom {
    Room simulation;
    std::array<ClientEndpoint, MAX_PLAYERS> endpoints{};
};

}  // namespace fireice
