#pragma once

#include "Types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace fireice {

enum class PacketType : uint8_t {
    ConnectRequest = 1,
    ConnectAccept = 2,
    ConnectReject = 3,
    Input = 4,
    State = 5,
    Disconnect = 6,
    Action = 7,
    Discovery = 8
};

#pragma pack(push, 1)

struct PacketHeader {
    PacketType type = PacketType::ConnectRequest;
    uint8_t version = 1;
};

struct ConnectRequestPacket {
    PacketHeader header{PacketType::ConnectRequest, 1};
    PlayerRole preferredRole = PlayerRole::None;
    char playerName[MAX_PLAYER_NAME]{};
    char roomCode[MAX_ROOM_CODE]{};
};

struct ConnectAcceptPacket {
    PacketHeader header{PacketType::ConnectAccept, 1};
    uint8_t slot = 0;
    PlayerRole role = PlayerRole::None;
    uint32_t worldSeed = 0;
    char roomCode[MAX_ROOM_CODE]{};
};

struct ConnectRejectPacket {
    PacketHeader header{PacketType::ConnectReject, 1};
    char reason[64]{};
};

struct InputPacket {
    PacketHeader header{PacketType::Input, 1};
    uint8_t slot = 0;
    uint32_t tick = 0;
    uint8_t flags = 0;
};

struct StatePacket {
    PacketHeader header{PacketType::State, 1};
    WorldState world{};
};

struct DisconnectPacket {
    PacketHeader header{PacketType::Disconnect, 1};
    uint8_t slot = 0;
};

struct ActionPacket {
    PacketHeader header{PacketType::Action, 1};
    uint8_t slot = 0;
    PlayerAction action = PlayerAction::None;
    uint8_t value = 0;
};

struct DiscoveryPacket {
    PacketHeader header{PacketType::Discovery, 1};
    uint8_t isResponse = 0;
    char roomCode[MAX_ROOM_CODE]{};
    uint8_t playerCount = 0;
    uint8_t maxPlayers = 0;
    char levelName[MAX_LEVEL_NAME]{};
};

#pragma pack(pop)

constexpr std::size_t PACKET_BUFFER_SIZE = 1600;

static_assert(sizeof(StatePacket) <= PACKET_BUFFER_SIZE, "StatePacket exceeds UDP buffer");

template <typename T>
bool packPacket(const T& packet, std::array<char, PACKET_BUFFER_SIZE>& buffer, std::size_t& size) {
    static_assert(std::is_trivially_copyable_v<T>, "Packet must be trivially copyable");
    if (sizeof(T) > buffer.size()) {
        return false;
    }
    std::memcpy(buffer.data(), &packet, sizeof(T));
    size = sizeof(T);
    return true;
}

template <typename T>
bool unpackPacket(const char* data, std::size_t size, T& packet) {
    static_assert(std::is_trivially_copyable_v<T>, "Packet must be trivially copyable");
    if (size < sizeof(T)) {
        return false;
    }
    std::memcpy(&packet, data, sizeof(T));
    return packet.header.type == T{}.header.type;
}

inline bool unpackStatePacketCompatible(const char* data, std::size_t size, StatePacket& packet) {
    if (size < sizeof(PacketHeader)) {
        return false;
    }
    packet = StatePacket{};
    const std::size_t copySize = std::min(size, sizeof(StatePacket));
    std::memcpy(&packet, data, copySize);
    return packet.header.type == PacketType::State;
}

inline const char* roleName(PlayerRole role) {
    switch (role) {
        case PlayerRole::Fire:
            return "Fire";
        case PlayerRole::Water:
            return "Water";
        case PlayerRole::Poison:
            return "Poison";
        default:
            return "None";
    }
}

}  // namespace fireice
