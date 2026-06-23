#include "Room.hpp"

#include "LevelCatalog.hpp"
#include "LevelMechanics.hpp"
#include "LevelProgress.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace fireice {

namespace {

constexpr float COUNTDOWN_SECONDS = 3.0f;
constexpr float COYOTE_TIME = 0.12f;

}  // namespace

void Room::selectLevel(uint8_t index, bool keepMapSelect) {
    const LevelCatalog& catalog = LevelCatalog::instance();
    if (index >= catalog.count()) {
        index = 0;
    }

    selectedLevelIndex = index;
    mapPath = catalog.resolvePath(index);
    visualMapPath = catalog.resolveVisualPath(index);
    if (!map.loadFromFile(mapPath)) {
        std::cerr << "[Room " << code << "] Failed to load level: " << mapPath << std::endl;
        return;
    }

    pickups = visualMapPath.empty() ? std::vector<Pickup>{} : loadPickupsFromTmx(visualMapPath);
    levelRuntime.mudSpawners = visualMapPath.empty() ? std::vector<Vec2>{} : loadMudSpawnsFromTmx(visualMapPath);
    levelRuntime.fanZones = visualMapPath.empty() ? std::vector<FanZone>{}
                                                  : loadFanZonesFromTmx(visualMapPath, 16, &levelRuntime.fanTileCoords);
    levelRuntime.powerUpSpawns =
        visualMapPath.empty() ? std::vector<PowerUpSpawn>{} : loadPowerUpSpawnsFromTmx(visualMapPath, 16);
    levelRuntime.sawTraps = visualMapPath.empty() ? std::vector<SawTrap>{} : loadSawTrapsFromTmx(visualMapPath, 16);
    alignSawTrapsToMap(map, levelRuntime.sawTraps);
    configureSawTravelBounds(map, levelRuntime.sawTraps);
    levelRuntime.rockHeads =
        visualMapPath.empty() ? std::vector<RockHeadTrap>{} : loadRockHeadsFromTmx(visualMapPath, 16);
    levelRuntime.pendulums =
        visualMapPath.empty() ? std::vector<PendulumTrap>{} : loadPendulumsFromTmx(visualMapPath, 16);
    configureRockHeadTravelBounds(map, levelRuntime.rockHeads);
    initLevelRuntime(map, levelRuntime);
    initFlyingEnemiesForLevel(levelRuntime, index, visualMapPath, 16);

    applyLevelMetadata();
    resetWorld();
    world.phase = GamePhase::Lobby;
    world.lobbyStep = keepMapSelect ? 1 : 0;
    world.countdown = 0;
    countdownTimer = 0.0f;

    for (ClientSlot& client : clients) {
        client.ready = false;
        if (!keepMapSelect) {
            client.waitingReady = false;
        }
        client.pendingInput = InputFlags::None;
    }

    syncConnectedCount();
}

void Room::applyLevelMetadata() {
    const LevelCatalog& catalog = LevelCatalog::instance();
    const LevelInfo& info = catalog.at(selectedLevelIndex);

    const uint8_t playerCount = std::max(uint8_t{1}, world.connectedCount);
    world.levelIndex = catalog.globalIndexToFilteredIndex(selectedLevelIndex, playerCount);
    world.levelCount = catalog.countForPlayerCount(playerCount);
    world.totalGems = static_cast<uint8_t>(std::min(255, map.countGems() + static_cast<int>(pickups.size())));
    std::snprintf(world.levelName, MAX_LEVEL_NAME, "%s", info.title);
    std::snprintf(world.roomCode, MAX_ROOM_CODE, "%s", code.c_str());
    syncProgressToWorld();
}

void Room::syncProgressToWorld() {
    world.unlockedMask = unlockedMask;
    world.completedMask = completedMask;
}

bool Room::isLevelUnlocked(uint8_t index) const {
    return fireice::isLevelUnlocked(unlockedMask, index);
}

void Room::reloadMap() {
    map.loadFromFile(mapPath);
    applyLevelMetadata();
    alignSawTrapsToMap(map, levelRuntime.sawTraps);
    configureSawTravelBounds(map, levelRuntime.sawTraps);
    configureRockHeadTravelBounds(map, levelRuntime.rockHeads);
    initLevelRuntime(map, levelRuntime);
}

void Room::resetWorld() {
    reloadMap();

    world.tick = 0;
    world.fireDoorOpen = false;
    world.waterDoorOpen = false;
    world.poisonDoorOpen = false;
    world.levelComplete = false;
    world.collectedPickupsMask = 0;
    world.collectedPickupsMaskHi = 0;
    world.collectedPickupsMaskExt = 0;
    resetLevelRuntime(levelRuntime);
    initFlyingEnemiesForLevel(levelRuntime, selectedLevelIndex, visualMapPath, 16);
    syncVanishingMask(levelRuntime, world);
    updateRockHeads(levelRuntime, map, world, 0.0f);
    updatePendulums(levelRuntime, world, 0.0f);
    updateSawTraps(levelRuntime, world, 0.0f);

    for (PlayerState& player : world.players) {
        player = PlayerState{};
    }

    for (ClientSlot& client : clients) {
        client.jumpHeld = false;
        client.airJumpUsedThisHold = false;
        client.groundJumpConsumed = false;
        client.coyoteTimer = 0.0f;
    }

    for (const SpawnPoint& spawn : map.spawns()) {
        if (spawn.role == PlayerRole::Fire && clients[0].connected) {
            world.players[0].role = PlayerRole::Fire;
            world.players[0].x = spawn.x;
            world.players[0].y = spawn.y;
            world.players[0].alive = true;
        } else if (spawn.role == PlayerRole::Water && clients[1].connected) {
            world.players[1].role = PlayerRole::Water;
            world.players[1].x = spawn.x;
            world.players[1].y = spawn.y;
            world.players[1].alive = true;
        } else if (spawn.role == PlayerRole::Poison && clients[2].connected) {
            world.players[2].role = PlayerRole::Poison;
            world.players[2].x = spawn.x;
            world.players[2].y = spawn.y;
            world.players[2].alive = true;
        }
    }
}

void Room::syncConnectedCount() {
    uint8_t count = 0;
    uint8_t readyMask = 0;

    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected) {
            world.playerNames[i][0] = '\0';
            continue;
        }
        ++count;
        if (clients[i].ready) {
            readyMask |= static_cast<uint8_t>(1u << i);
        }
        std::snprintf(world.playerNames[i], MAX_PLAYER_NAME, "%s", clients[i].name.c_str());
    }

    world.connectedCount = count;
    world.readyMask = readyMask;
}

void Room::beginCountdown() {
    world.phase = GamePhase::Countdown;
    countdownTimer = COUNTDOWN_SECONDS;
    world.countdown = static_cast<uint8_t>(std::ceil(countdownTimer));
    std::cout << "[Room " << code << "] Countdown started" << std::endl;
}

void Room::beginPlaying() {
    resetWorld();
    world.phase = GamePhase::Playing;
    world.countdown = 0;

    for (ClientSlot& client : clients) {
        client.pendingInput = InputFlags::None;
    }

    std::cout << "[Room " << code << "] Game started - " << world.levelName << std::endl;
}

void Room::returnToLobby() {
    resetWorld();
    world.phase = GamePhase::Lobby;
    world.lobbyStep = 0;
    world.countdown = 0;
    countdownTimer = 0.0f;

    for (ClientSlot& client : clients) {
        client.ready = false;
        client.waitingReady = false;
        client.pendingInput = InputFlags::None;
    }

    syncConnectedCount();
    syncWaitingReadyMask();
    std::cout << "[Room " << code << "] Returned to lobby" << std::endl;
}

bool Room::allConnectedReady() const {
    uint8_t connected = 0;
    uint8_t ready = 0;

    for (const ClientSlot& client : clients) {
        if (!client.connected)
            continue;
        ++connected;
        if (client.ready)
            ++ready;
    }

    return connected > 0 && ready >= connected;
}

bool Room::allConnectedWaitingReady() const {
    uint8_t connected = 0;
    uint8_t waitingReady = 0;

    for (const ClientSlot& client : clients) {
        if (!client.connected)
            continue;
        ++connected;
        if (client.waitingReady)
            ++waitingReady;
    }

    return connected > 0 && waitingReady >= connected;
}

void Room::syncWaitingReadyMask() {
    uint8_t mask = 0;
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (clients[i].connected && clients[i].waitingReady) {
            mask |= static_cast<uint8_t>(1u << i);
        }
    }
    world.waitingReadyMask = mask;
}

void Room::proceedToMapSelect() {
    world.lobbyStep = 1;
    for (ClientSlot& client : clients) {
        client.waitingReady = false;
        client.ready = false;
    }
    syncConnectedCount();
    syncWaitingReadyMask();
    std::cout << "[Room " << code << "] Advanced to map select" << std::endl;
}

void Room::backToWaitingRoom() {
    world.lobbyStep = 0;
    for (ClientSlot& client : clients) {
        client.waitingReady = false;
        client.ready = false;
    }
    syncConnectedCount();
    syncWaitingReadyMask();
    std::cout << "[Room " << code << "] Back to waiting room" << std::endl;
}

void Room::handleAction(uint8_t slot, PlayerAction action, uint8_t value) {
    if (!clients[slot].connected)
        return;

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, world.connectedCount);
    const uint8_t filteredCount = catalog.countForPlayerCount(playerCount);

    if (world.phase == GamePhase::Lobby) {
        if (world.lobbyStep == 0) {
            if (action == PlayerAction::ReturnToLobby) {
                clients[slot].waitingReady = false;
                syncWaitingReadyMask();
                return;
            }
            if (action == PlayerAction::WaitingReady) {
                clients[slot].waitingReady = !clients[slot].waitingReady;
                syncWaitingReadyMask();
                std::cout << "[Room " << code << "] Slot " << static_cast<int>(slot)
                          << " waitingReady=" << (clients[slot].waitingReady ? "true" : "false") << " mask=0x"
                          << std::hex << world.waitingReadyMask << std::dec << std::endl;
                return;
            }
            if (action == PlayerAction::ProceedToMapSelect && allConnectedWaitingReady()) {
                proceedToMapSelect();
                return;
            }
            return;
        }

        // lobbyStep == 1: map select
        if (action == PlayerAction::BackToWaitingRoom) {
            backToWaitingRoom();
            return;
        }
        if (action == PlayerAction::ReturnToLobby) {
            clients[slot].ready = false;
            syncConnectedCount();
            return;
        }
        if (action == PlayerAction::PrevLevel || action == PlayerAction::NextLevel) {
            const uint8_t currentFiltered = catalog.globalIndexToFilteredIndex(selectedLevelIndex, playerCount);
            uint8_t newFiltered = currentFiltered;
            if (action == PlayerAction::PrevLevel && currentFiltered > 0)
                newFiltered = currentFiltered - 1;
            else if (action == PlayerAction::NextLevel && currentFiltered + 1 < filteredCount)
                newFiltered = currentFiltered + 1;
            if (newFiltered != currentFiltered) {
                selectLevel(catalog.filteredIndexToGlobalIndex(newFiltered, playerCount));
                world.lobbyStep = 1;
            }
            return;
        }
        if (action == PlayerAction::SelectLevel) {
            if (value < filteredCount) {
                selectLevel(catalog.filteredIndexToGlobalIndex(value, playerCount));
                world.lobbyStep = 1;
                clients[slot].ready = true;
                syncConnectedCount();
                std::cout << "[Room " << code << "] Slot " << static_cast<int>(slot) << " selected level and ready"
                          << std::endl;
                if (allConnectedReady())
                    beginCountdown();
            }
            return;
        }
        if (action == PlayerAction::Ready) {
            clients[slot].ready = true;
            syncConnectedCount();
            std::cout << "[Room " << code << "] Slot " << static_cast<int>(slot) << " ready" << std::endl;
            if (allConnectedReady())
                beginCountdown();
            return;
        }
    }

    if (action == PlayerAction::ReturnToLobby && world.phase != GamePhase::Lobby) {
        returnToLobby();
        return;
    }
    if (action == PlayerAction::Restart) {
        if (world.phase == GamePhase::Victory || world.phase == GamePhase::GameOver) {
            returnToLobby();
            return;
        }
        if (world.phase == GamePhase::Playing || world.phase == GamePhase::Countdown) {
            resetWorld();
            world.phase = GamePhase::Countdown;
            countdownTimer = COUNTDOWN_SECONDS;
            world.countdown = static_cast<uint8_t>(std::ceil(countdownTimer));
            for (ClientSlot& client : clients) {
                client.pendingInput = InputFlags::None;
            }
            return;
        }
    }
    if (action == PlayerAction::NextLevel && world.phase == GamePhase::Victory) {
        const uint8_t currentFiltered = catalog.globalIndexToFilteredIndex(selectedLevelIndex, playerCount);
        if (currentFiltered + 1 < filteredCount)
            selectLevel(catalog.filteredIndexToGlobalIndex(currentFiltered + 1, playerCount));
        else
            returnToLobby();
    }
}

void Room::simulateTick() {
    if (world.phase == GamePhase::Countdown) {
        countdownTimer = std::max(0.0f, countdownTimer - TICK_DT);
        world.countdown = static_cast<uint8_t>(std::ceil(countdownTimer));
        updatePendulums(levelRuntime, world, static_cast<float>(world.tick) * TICK_DT);
        updateSawTraps(levelRuntime, world, static_cast<float>(world.tick) * TICK_DT);
        if (countdownTimer <= 0.0f)
            beginPlaying();
        ++world.tick;
        return;
    }

    if (world.phase != GamePhase::Playing) {
        ++world.tick;
        return;
    }

    updateButtons(map, world);

    if (world.phase == GamePhase::Playing) {
        updatePowerUpDrops(levelRuntime, map, world, TICK_DT);
        syncMagnetState(levelRuntime, world);
    }

    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected)
            continue;

        PlayerState& player = world.players[i];
        ClientSlot& client = clients[i];
        const bool jumpNow = hasFlag(client.pendingInput, InputFlags::Jump);
        const bool jumpPressed = jumpNow && !client.jumpHeld;
        client.jumpHeld = jumpNow;

        if (!jumpNow) {
            client.airJumpUsedThisHold = false;
            client.groundJumpConsumed = false;
        }

        // Coyote Time：离地后短暂窗口内仍视为在地面，可触发一次跳跃
        const bool grounded = player.onGround || client.coyoteTimer > 0.0f;
        bool groundJump = false;
        if (jumpNow && grounded && !client.groundJumpConsumed) {
            groundJump = true;
            client.groundJumpConsumed = true;
        }

        const bool airJump = jumpPressed && !grounded;
        applyInput(player, client.pendingInput, TICK_DT, groundJump, airJump, client.airJumpUsedThisHold);
        integratePlayer(player, map, world, TICK_DT);
        applyFanZones(player, levelRuntime.fanZones, TICK_DT);
        if (player.onGround) {
            client.coyoteTimer = COYOTE_TIME;
        } else {
            client.coyoteTimer = std::max(0.0f, client.coyoteTimer - TICK_DT);
        }
        triggerVanishingForPlayer(player, map, levelRuntime);

        if (sampleHazard(map, player, world))
            player.alive = false;
        if (sampleSpikeHazard(map, player))
            player.alive = false;
        if (sampleSawHazard(levelRuntime.sawTraps, player, static_cast<float>(world.tick) * TICK_DT))
            player.alive = false;
        if (sampleMudHazard(levelRuntime, player))
            player.alive = false;
        if (samplePendulumHazard(levelRuntime.pendulums, player, static_cast<float>(world.tick) * TICK_DT))
            player.alive = false;
        if (sampleFlyingEnemyHazard(levelRuntime, player))
            player.alive = false;
        if (sampleProjectileHazard(levelRuntime, player))
            player.alive = false;
        collectGems(player, map);
        collectPickups(player, pickups, world.collectedPickupsMask, world.collectedPickupsMaskHi,
                       world.collectedPickupsMaskExt);
        collectMagnetDrops(player, levelRuntime);
        player.atExit = sampleExit(map, player);
    }

    if (world.phase == GamePhase::Playing) {
        removeUncollectedLandedPowerUps(levelRuntime);
        syncMagnetState(levelRuntime, world);
    }

    updateMagnetPulls(levelRuntime, map, world, pickups, TICK_DT);

    updateLevelMechanics(levelRuntime, map, world, TICK_DT);
    updateFlyingEnemiesAndProjectiles(levelRuntime, map, world, TICK_DT);
    updateRockHeads(levelRuntime, map, world, TICK_DT);
    updatePendulums(levelRuntime, world, static_cast<float>(world.tick) * TICK_DT);
    updateSawTraps(levelRuntime, world, static_cast<float>(world.tick) * TICK_DT);
    updatePhase();
    ++world.tick;
}

void Room::updatePhase() {
    bool anyAlive = false;
    bool anyConnected = false;
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected)
            continue;
        anyConnected = true;
        if (world.players[i].alive) {
            anyAlive = true;
            break;
        }
    }

    if (anyConnected && !anyAlive) {
        world.phase = GamePhase::GameOver;
        world.levelComplete = false;
        std::cout << "[Room " << code << "] Game over" << std::endl;
        return;
    }
    if (levelRuntime.collectVictory && world.totalGems > 0) {
        uint8_t collected = 0;
        for (const Pickup& pickup : pickups) {
            const uint32_t bit = 1u << (pickup.index % 32u);
            const uint8_t word = pickup.index / 32u;
            const bool taken = word == 0 ? ((world.collectedPickupsMask & bit) != 0)
                                         : (word == 1 ? ((world.collectedPickupsMaskHi & bit) != 0)
                                                      : ((world.collectedPickupsMaskExt & bit) != 0));
            if (taken) {
                ++collected;
            }
        }
        for (std::size_t i = 0; i < clients.size(); ++i) {
            if (!clients[i].connected) {
                continue;
            }
            collected =
                static_cast<uint8_t>(std::max(static_cast<int>(collected), static_cast<int>(world.players[i].gems)));
        }
        if (collected >= world.totalGems) {
            world.phase = GamePhase::Victory;
            world.levelComplete = true;
            completedMask |= static_cast<uint8_t>(1u << selectedLevelIndex);
            if (selectedLevelIndex + 1 < LevelCatalog::instance().count()) {
                unlockedMask |= static_cast<uint8_t>(1u << (selectedLevelIndex + 1));
            }
            syncProgressToWorld();
            std::cout << "[Room " << code << "] Level complete (all fruits collected)" << std::endl;
        }
        return;
    }

    bool allDone = true;
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected)
            continue;
        if (!world.players[i].atExit) {
            allDone = false;
            break;
        }
    }

    if (allDone && world.connectedCount > 0) {
        world.phase = GamePhase::Victory;
        world.levelComplete = true;
        completedMask |= static_cast<uint8_t>(1u << selectedLevelIndex);
        if (selectedLevelIndex + 1 < LevelCatalog::instance().count())
            unlockedMask |= static_cast<uint8_t>(1u << (selectedLevelIndex + 1));
        syncProgressToWorld();
        std::cout << "[Room " << code << "] Level complete" << std::endl;
    }
}

std::optional<uint8_t> Room::findOpenSlot(PlayerRole preferred) const {
    if (preferred == PlayerRole::Fire && !clients[0].connected) {
        return static_cast<uint8_t>(0);
    }
    if (preferred == PlayerRole::Water && !clients[1].connected) {
        return static_cast<uint8_t>(1);
    }
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected) {
            return static_cast<uint8_t>(i);
        }
    }
    return std::nullopt;
}

PlayerRole Room::roleForSlot(uint8_t slot) const {
    switch (slot) {
        case 0:
            return PlayerRole::Fire;
        case 1:
            return PlayerRole::Water;
        case 2:
            return PlayerRole::Poison;
        default:
            return PlayerRole::None;
    }
}

}  // namespace fireice
