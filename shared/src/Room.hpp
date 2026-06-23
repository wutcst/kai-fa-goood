#pragma once

#include "LevelMechanics.hpp"
#include "Map.hpp"
#include "Pickup.hpp"
#include "Protocol.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace fireice {

// 玩家槽位状态（纯游戏逻辑，不含网络地址）
struct ClientSlot {
    bool connected = false;
    bool ready = false;
    bool waitingReady = false;
    PlayerRole role = PlayerRole::None;
    InputFlags pendingInput = InputFlags::None;
    bool jumpHeld = false;
    bool airJumpUsedThisHold = false;
    bool groundJumpConsumed = false;
    float coyoteTimer = 0.0f;
    std::string name;
};

// 房间仿真：关卡、物理、阶段流转（客户端单机与服务端共用）
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

    void selectLevel(uint8_t index, bool keepMapSelect = false);
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
    std::optional<uint8_t> findOpenSlot(PlayerRole preferred) const;
    PlayerRole roleForSlot(uint8_t slot) const;
};

}  // namespace fireice
