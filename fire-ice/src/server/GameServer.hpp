#pragma once

#include "LevelMechanics.hpp"
#include "Map.hpp"
#include "Pickup.hpp"
#include "Protocol.hpp"

#include <SFML/Network.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fireice {

struct ClientSlot {
    bool connected = false;
    bool ready = false;
    bool waitingReady = false;
    PlayerRole role = PlayerRole::None;
    sf::IpAddress address;
    unsigned short port = 0;
    InputFlags pendingInput = InputFlags::None;
    bool jumpHeld = false;
    bool airJumpUsedThisHold = false;
    std::string name;
};

struct Room {
    std::string code;
    GameMap map;
    std::string mapPath;
    std::string visualMapPath;
    std::vector<Pickup> pickups;
    LevelRuntime levelRuntime{};
    uint8_t selectedLevelIndex = 0;
    WorldState world{};
    std::array<ClientSlot, MAX_PLAYERS> clients{};
    float countdownTimer = 0.0f;
    uint8_t unlockedMask = INITIAL_UNLOCKED_LEVEL_MASK;
    uint8_t completedMask = 0;
    std::chrono::steady_clock::time_point lastTick;
    std::chrono::steady_clock::time_point lastBroadcast;

    void selectLevel(uint8_t index);
    void applyLevelMetadata();
    void syncProgressToWorld();
    bool isLevelUnlocked(uint8_t index) const;
    void reloadMap();
    void resetWorld();
    void syncConnectedCount();
    void beginCountdown();
    void beginPlaying();
    void returnToLobby();
    bool allConnectedReady() const;
    bool allConnectedWaitingReady() const;
    void syncWaitingReadyMask();
    void proceedToMapSelect();
    void backToWaitingRoom();
    void handleAction(uint8_t slot, PlayerAction action, uint8_t value);
    void simulateTick();
    void updatePhase();
    void broadcastState(sf::UdpSocket& socket);
    std::optional<uint8_t> findOpenSlot(PlayerRole preferred) const;
    std::optional<uint8_t> findSlotByEndpoint(const sf::IpAddress& address, unsigned short port) const;
    PlayerRole roleForSlot(uint8_t slot) const;
    void acceptClient(sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port,
                      const ConnectRequestPacket& request);
    void rejectClient(sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port, const char* reason);
    void disconnectClient(uint8_t slot, sf::UdpSocket& socket);
};

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
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms_;
    std::chrono::steady_clock::time_point lastTick_;
    std::chrono::steady_clock::time_point lastBroadcast_;
};

}  // namespace fireice
