#include "RoomNetwork.hpp"

#include "Types.hpp"

#include <array>
#include <iostream>

namespace fireice {

void RoomNetwork::broadcastState(ServerRoom& room, sf::UdpSocket& socket) {
    Room& sim = room.simulation;
    sim.syncConnectedCount();

    StatePacket packet{};
    packet.world = sim.world;

    std::array<char, PACKET_BUFFER_SIZE> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return;
    }

    for (std::size_t i = 0; i < sim.clients.size(); ++i) {
        if (!sim.clients[i].connected || !room.endpoints[i].active) {
            continue;
        }
        const ClientEndpoint& endpoint = room.endpoints[i];
        socket.send(buffer.data(), size, endpoint.address, endpoint.port);
    }
}

std::optional<uint8_t> RoomNetwork::findSlotByEndpoint(const ServerRoom& room, const sf::IpAddress& address,
                                                       unsigned short port) {
    for (std::size_t i = 0; i < room.endpoints.size(); ++i) {
        const ClientEndpoint& endpoint = room.endpoints[i];
        if (room.simulation.clients[i].connected && endpoint.active && endpoint.address == address &&
            endpoint.port == port) {
            return static_cast<uint8_t>(i);
        }
    }
    return std::nullopt;
}

void RoomNetwork::acceptClient(ServerRoom& room, sf::UdpSocket& socket, const sf::IpAddress& address,
                               unsigned short port, const ConnectRequestPacket& request) {
    Room& sim = room.simulation;

    const auto existing = findSlotByEndpoint(room, address, port);
    if (existing.has_value()) {
        ConnectAcceptPacket accept{};
        accept.slot = existing.value();
        accept.role = sim.clients[existing.value()].role;
        std::snprintf(sim.world.roomCode, MAX_ROOM_CODE, "%s", sim.code.c_str());
        std::snprintf(accept.roomCode, MAX_ROOM_CODE, "%s", sim.code.c_str());

        std::array<char, PACKET_BUFFER_SIZE> buf{};
        std::size_t sz = 0;
        packPacket(accept, buf, sz);
        socket.send(buf.data(), sz, address, port);
        broadcastState(room, socket);
        return;
    }

    const auto slot = sim.findOpenSlot(request.preferredRole);
    if (!slot.has_value()) {
        rejectClient(socket, address, port, "Room is full");
        return;
    }

    const PlayerRole role = sim.roleForSlot(slot.value());
    const uint8_t slotIndex = slot.value();

    ClientSlot& client = sim.clients[slotIndex];
    client.connected = true;
    client.ready = false;
    client.waitingReady = false;
    client.role = role;
    client.pendingInput = InputFlags::None;
    client.name = request.playerName;

    ClientEndpoint& endpoint = room.endpoints[slotIndex];
    endpoint.address = address;
    endpoint.port = port;
    endpoint.active = true;

    ConnectAcceptPacket accept{};
    accept.slot = slotIndex;
    accept.role = role;
    std::snprintf(sim.world.roomCode, MAX_ROOM_CODE, "%s", sim.code.c_str());
    std::snprintf(accept.roomCode, MAX_ROOM_CODE, "%s", sim.code.c_str());

    std::array<char, PACKET_BUFFER_SIZE> buf{};
    std::size_t sz = 0;
    packPacket(accept, buf, sz);
    socket.send(buf.data(), sz, address, port);

    sim.syncConnectedCount();
    sim.syncWaitingReadyMask();
    broadcastState(room, socket);

    std::cout << "[Room " << sim.code << "] Client joined slot " << static_cast<int>(slotIndex) << " as "
              << roleName(role) << " from " << address << ":" << port << std::endl;
}

void RoomNetwork::rejectClient(sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port,
                               const char* reason) {
    ConnectRejectPacket reject{};
    std::snprintf(reject.reason, sizeof(reject.reason), "%s", reason);

    std::array<char, PACKET_BUFFER_SIZE> buf{};
    std::size_t sz = 0;
    packPacket(reject, buf, sz);
    socket.send(buf.data(), sz, address, port);
}

void RoomNetwork::disconnectClient(ServerRoom& room, uint8_t slot, sf::UdpSocket& socket) {
    Room& sim = room.simulation;
    if (slot >= sim.clients.size()) {
        return;
    }

    sim.clients[slot] = ClientSlot{};
    room.endpoints[slot] = ClientEndpoint{};
    sim.syncConnectedCount();
    sim.syncWaitingReadyMask();

    if (sim.world.phase != GamePhase::Lobby) {
        sim.returnToLobby();
    }

    broadcastState(room, socket);
    std::cout << "[Room " << sim.code << "] Client slot " << static_cast<int>(slot) << " disconnected" << std::endl;
}

}  // namespace fireice
