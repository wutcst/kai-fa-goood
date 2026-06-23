#include "ClientNetwork.hpp"

#include "Types.hpp"

#include <array>
#include <iostream>

namespace fireice {

bool ClientNetwork::bindLocal() {
    if (socket_.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
        std::cerr << "[Client] Failed to bind local UDP port" << std::endl;
        return false;
    }
    localPort_ = socket_.getLocalPort();
    socket_.setBlocking(false);
    return true;
}

void ClientNetwork::setServerHost(const std::string& host) {
    serverAddress_ = sf::IpAddress(host);
    if (serverAddress_ == sf::IpAddress::None) {
        serverAddress_ = sf::IpAddress(DEFAULT_SERVER_HOST);
    }
}

bool ClientNetwork::sendConnectRequest(const ConnectRequestPacket& request) {
    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    if (!packPacket(request, buffer, size)) {
        return false;
    }
    return socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT) == sf::Socket::Done;
}

bool ClientNetwork::sendInput(uint8_t slot, InputFlags input, uint32_t tick) {
    InputPacket packet{};
    packet.slot = slot;
    packet.tick = tick;
    packet.flags = static_cast<uint8_t>(input);

    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return false;
    }
    return socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT) == sf::Socket::Done;
}

bool ClientNetwork::sendAction(uint8_t slot, PlayerAction action, uint8_t value) {
    ActionPacket packet{};
    packet.slot = slot;
    packet.action = action;
    packet.value = value;

    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return false;
    }
    return socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT) == sf::Socket::Done;
}

bool ClientNetwork::sendDisconnect(uint8_t slot) {
    DisconnectPacket packet{};
    packet.slot = slot;

    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return false;
    }
    return socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT) == sf::Socket::Done;
}

void ClientNetwork::poll(const ConnectAcceptHandler& onAccept, const ConnectRejectHandler& onReject,
                         const StateHandler& onState) {
    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t received = 0;
    sf::IpAddress sender;
    unsigned short port = 0;

    while (socket_.receive(buffer.data(), buffer.size(), received, sender, port) == sf::Socket::Done) {
        if (received < sizeof(PacketHeader)) {
            continue;
        }

        const auto* header = reinterpret_cast<const PacketHeader*>(buffer.data());
        switch (header->type) {
            case PacketType::ConnectAccept: {
                ConnectAcceptPacket packet{};
                if (unpackPacket(buffer.data(), received, packet) && onAccept) {
                    onAccept(packet);
                }
                break;
            }
            case PacketType::ConnectReject: {
                ConnectRejectPacket packet{};
                if (unpackPacket(buffer.data(), received, packet) && onReject) {
                    onReject(packet);
                }
                break;
            }
            case PacketType::State: {
                StatePacket packet{};
                if (unpackStatePacketCompatible(buffer.data(), received, packet) && onState) {
                    onState(packet);
                }
                break;
            }
            default:
                break;
        }
    }
}

}  // namespace fireice
