#pragma once

#include "Map.hpp"
#include "Protocol.hpp"

#include <SFML/Network.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string>

namespace fireice {

struct ClientSlot {
    bool connected = false;
    bool ready = false;
    PlayerRole role = PlayerRole::None;
    sf::IpAddress address;
    unsigned short port = 0;
    InputFlags pendingInput = InputFlags::None;
    std::string name;
};

class GameServer {
public:
    bool start(uint8_t initialLevel = 0);
    void run();
    void stop();

private:
    void selectLevel(uint8_t index);
    void applyLevelMetadata();
    void reloadMap();
    void resetWorld();
    void syncConnectedCount();
    void beginCountdown();
    void beginPlaying();
    void returnToLobby();
    bool allConnectedReady() const;
    void processPackets();
    void handleAction(uint8_t slot, PlayerAction action, uint8_t value);
    void simulateTick();
    void updatePhase();
    void broadcastState();
    std::optional<uint8_t> findOpenSlot(PlayerRole preferred) const;
    std::optional<uint8_t> findSlotByEndpoint(const sf::IpAddress& address, unsigned short port) const;
    PlayerRole roleForSlot(uint8_t slot) const;
    void acceptClient(const sf::IpAddress& address, unsigned short port, const ConnectRequestPacket& request);
    void rejectClient(const sf::IpAddress& address, unsigned short port, const char* reason);

    sf::UdpSocket socket_;
    GameMap map_;
    std::string mapPath_;
    uint8_t selectedLevelIndex_ = 0;
    WorldState world_{};
    std::array<ClientSlot, MAX_PLAYERS> clients_{};
    char roomCode_[MAX_ROOM_CODE]{};
    bool running_ = false;
    float countdownTimer_ = 0.0f;
    std::chrono::steady_clock::time_point lastTick_;
    std::chrono::steady_clock::time_point lastBroadcast_;
};

} // namespace fireice
