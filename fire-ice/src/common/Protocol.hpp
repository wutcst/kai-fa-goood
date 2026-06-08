#pragma once

#include "Types.hpp"

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
    Action = 7
};

#pragma pack(push, 1)

struct PacketHeader {
    PacketType type = PacketType::ConnectRequest;
    uint8_t version = 1;
};

struct ConnectRequestPacket {
    PacketHeader header{PacketType::ConnectRequest, 1};
    PlayerRole preferredRole = PlayerRole::None;
    char playerName[16]{};
};

struct ConnectAcceptPacket {
    PacketHeader header{PacketType::ConnectAccept, 1};
    uint8_t slot = 0;
    PlayerRole role = PlayerRole::None;
    uint32_t worldSeed = 0;
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

#pragma pack(pop)

template <typename T>
bool packPacket(const T& packet, std::array<char, 512>& buffer, std::size_t& size) {
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

inline const char* roleName(PlayerRole role) {
    switch (role) {
    case PlayerRole::Fire: return "Fire";
    case PlayerRole::Water: return "Water";
    default: return "None";
    }
}

} // namespace fireice
