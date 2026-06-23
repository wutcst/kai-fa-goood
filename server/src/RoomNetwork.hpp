#pragma once

#include "Protocol.hpp"
#include "ServerRoom.hpp"

#include <SFML/Network.hpp>

#include <optional>

namespace fireice {

// 房间网络层：仅服务端使用，负责 UDP 收发
class RoomNetwork {
public:
    static void broadcastState(ServerRoom& room, sf::UdpSocket& socket);
    static std::optional<uint8_t> findSlotByEndpoint(const ServerRoom& room, const sf::IpAddress& address,
                                                     unsigned short port);
    static void acceptClient(ServerRoom& room, sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port,
                             const ConnectRequestPacket& request);
    static void rejectClient(sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port,
                             const char* reason);
    static void disconnectClient(ServerRoom& room, uint8_t slot, sf::UdpSocket& socket);
};

}  // namespace fireice
