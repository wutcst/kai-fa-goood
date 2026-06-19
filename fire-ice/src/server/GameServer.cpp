#include "GameServer.hpp"

#include "LevelCatalog.hpp"
#include "LevelMechanics.hpp"
#include "LevelProgress.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <thread>

namespace fireice {

namespace {

constexpr float COUNTDOWN_SECONDS = 3.0f;

bool pickupTaken(const WorldState& world, const Pickup& pickup) {
    const uint32_t bit = 1u << (pickup.index % 32u);
    const uint8_t word = pickup.index / 32u;
    if (word == 0) {
        return (world.collectedPickupsMask & bit) != 0;
    }
    if (word == 1) {
        return (world.collectedPickupsMaskHi & bit) != 0;
    }
    return (world.collectedPickupsMaskExt & bit) != 0;
}

void markPickupTaken(WorldState& world, PlayerState& player, const Pickup& pickup) {
    const uint32_t bit = 1u << (pickup.index % 32u);
    const uint8_t word = pickup.index / 32u;
    if (word == 0) {
        world.collectedPickupsMask |= bit;
    } else if (word == 1) {
        world.collectedPickupsMaskHi |= bit;
    } else {
        world.collectedPickupsMaskExt |= bit;
    }
    ++player.gems;
}

void syncMagnetToWorld(const LevelRuntime& runtime, WorldState& world) {
    world.magnetActive = runtime.magnetActive ? 1 : 0;
    world.magnetFalling = runtime.magnetFalling ? 1 : 0;
    world.magnetOwnerSlot = runtime.magnetOwnerSlot;
    world.magnetX = runtime.magnetX;
    world.magnetY = runtime.magnetY;
    world.magnetTimer = runtime.magnetTimer;
}

void spawnMagnet(LevelRuntime& runtime, const GameMap& map) {
    const float left = TILE_SIZE * 2.0f;
    const float right = std::max(left, static_cast<float>(map.width()) * TILE_SIZE - TILE_SIZE * 2.0f);
    const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    runtime.magnetX = left + (right - left) * t;
    runtime.magnetY = -MAGNET_PICKUP_SIZE;
    runtime.magnetFalling = true;
    runtime.magnetActive = false;
    runtime.magnetOwnerSlot = 255;
    runtime.magnetTimer = 0.0f;
}

void updateMagnet(LevelRuntime& runtime, GameMap& map, WorldState& world, std::vector<Pickup>& pickups, float dt) {
    if (!runtime.magnetEnabled) {
        runtime.magnetFalling = false;
        runtime.magnetActive = false;
        syncMagnetToWorld(runtime, world);
        return;
    }

    runtime.magnetElapsed += dt;
    if (!runtime.magnetFalling && !runtime.magnetActive && runtime.magnetElapsed >= runtime.magnetNextDropTime) {
        spawnMagnet(runtime, map);
    }

    if (runtime.magnetFalling) {
        runtime.magnetY += MAGNET_FALL_SPEED * dt;
        const float magnetLeft = runtime.magnetX - MAGNET_PICKUP_SIZE * 0.5f;
        const float magnetTop = runtime.magnetY - MAGNET_PICKUP_SIZE * 0.5f;
        const AABB magnetBox{magnetLeft, magnetTop, MAGNET_PICKUP_SIZE, MAGNET_PICKUP_SIZE};
        for (std::size_t i = 0; i < MAX_PLAYERS; ++i) {
            PlayerState& player = world.players[i];
            if (!player.alive || player.role == PlayerRole::None) {
                continue;
            }
            if (!playerBounds(player).intersects(magnetBox)) {
                continue;
            }
            runtime.magnetFalling = false;
            runtime.magnetActive = true;
            runtime.magnetOwnerSlot = static_cast<uint8_t>(i);
            runtime.magnetTimer = MAGNET_EFFECT_SECONDS;
            break;
        }
        if (runtime.magnetY > static_cast<float>(map.height()) * TILE_SIZE + TILE_SIZE) {
            runtime.magnetFalling = false;
            runtime.magnetNextDropTime += MAGNET_DROP_INTERVAL;
        }
    }

    if (runtime.magnetActive) {
        runtime.magnetTimer = std::max(0.0f, runtime.magnetTimer - dt);
        if (runtime.magnetOwnerSlot < MAX_PLAYERS) {
            PlayerState& owner = world.players[runtime.magnetOwnerSlot];
            runtime.magnetX = owner.x + PLAYER_WIDTH * 0.5f;
            runtime.magnetY = owner.y;
            const float targetX = owner.x + PLAYER_WIDTH * 0.5f;
            const float targetY = owner.y + PLAYER_HEIGHT * 0.5f;
            for (Pickup& pickup : pickups) {
                if (pickupTaken(world, pickup)) {
                    continue;
                }
                const float centerX = pickup.x + pickup.w * 0.5f;
                const float centerY = pickup.y + pickup.h * 0.5f;
                const float dx = targetX - centerX;
                const float dy = targetY - centerY;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > MAGNET_ATTRACT_RADIUS || dist <= 0.01f) {
                    continue;
                }
                const float step = std::min(dist, MAGNET_ATTRACT_SPEED * dt);
                pickup.x += dx / dist * step;
                pickup.y += dy / dist * step;
                const AABB pickupBox{pickup.x, pickup.y, pickup.w, pickup.h};
                if (playerBounds(owner).intersects(pickupBox)) {
                    markPickupTaken(world, owner, pickup);
                }
            }
        }
        if (runtime.magnetTimer <= 0.0f) {
            runtime.magnetActive = false;
            runtime.magnetOwnerSlot = 255;
            runtime.magnetNextDropTime += MAGNET_DROP_INTERVAL;
        }
    }

    syncMagnetToWorld(runtime, world);
}

}  // namespace

// ============================================================================
// Room implementation
// ============================================================================

void Room::selectLevel(uint8_t index) {
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
    initLevelRuntime(map, levelRuntime);

    applyLevelMetadata();
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
    levelRuntime.magnetEnabled = selectedLevelIndex == 1;
    syncMagnetToWorld(levelRuntime, world);
    syncVanishingMask(levelRuntime, world);

    for (PlayerState& player : world.players) {
        player = PlayerState{};
    }

    for (ClientSlot& client : clients) {
        client.jumpHeld = false;
        client.airJumpUsedThisHold = false;
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
                const uint8_t currentFiltered = catalog.globalIndexToFilteredIndex(selectedLevelIndex, playerCount);
                if (value != currentFiltered) {
                    selectLevel(catalog.filteredIndexToGlobalIndex(value, playerCount));
                    world.lobbyStep = 1;
                    clients[slot].ready = true;
                    syncConnectedCount();
                } else if (!clients[slot].ready) {
                    clients[slot].ready = true;
                    syncConnectedCount();
                    std::cout << "[Room " << code << "] Slot " << static_cast<int>(slot) << " selected level"
                              << std::endl;
                } else {
                    std::cout << "[Room " << code << "] Slot " << static_cast<int>(slot) << " confirmed level"
                              << std::endl;
                    if (allConnectedReady())
                        beginCountdown();
                }
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
    if (action == PlayerAction::Restart && (world.phase == GamePhase::Victory || world.phase == GamePhase::GameOver)) {
        returnToLobby();
        return;
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

    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected)
            continue;

        PlayerState& player = world.players[i];
        const bool jumpNow = hasFlag(clients[i].pendingInput, InputFlags::Jump);
        const bool jumpPressed = jumpNow && !clients[i].jumpHeld;
        if (!jumpNow)
            clients[i].airJumpUsedThisHold = false;
        clients[i].jumpHeld = jumpNow;
        applyInput(player, clients[i].pendingInput, TICK_DT, jumpPressed, jumpNow, clients[i].airJumpUsedThisHold);
        applyFanZones(player, levelRuntime.fanZones, TICK_DT);
        integratePlayer(player, map, world, TICK_DT);
        triggerVanishingForPlayer(player, map, levelRuntime);

        if (sampleHazard(map, player, world))
            player.alive = false;
        if (sampleSpikeHazard(map, player))
            player.alive = false;
        if (sampleMudHazard(levelRuntime, player))
            player.alive = false;
        collectGems(player, map);
        collectPickups(player, pickups, world.collectedPickupsMask, world.collectedPickupsMaskHi,
                       world.collectedPickupsMaskExt);
        player.atExit = sampleExit(map, player);
    }

    updateMagnet(levelRuntime, map, world, pickups, TICK_DT);
    updateLevelMechanics(levelRuntime, map, world, TICK_DT);
    updatePhase();
    ++world.tick;
}

void Room::updatePhase() {
    bool anyDead = false;
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (!clients[i].connected)
            continue;
        if (!world.players[i].alive) {
            anyDead = true;
            break;
        }
    }

    if (anyDead) {
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

void Room::broadcastState(sf::UdpSocket& socket) {
    syncConnectedCount();

    StatePacket packet{};
    packet.world = world;

    std::array<char, 768> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size))
        return;

    for (const ClientSlot& client : clients) {
        if (!client.connected)
            continue;
        socket.send(buffer.data(), size, client.address, client.port);
    }
}

std::optional<uint8_t> Room::findOpenSlot(PlayerRole preferred) const {
    if (preferred == PlayerRole::Fire && !clients[0].connected)
        return static_cast<uint8_t>(0);
    if (preferred == PlayerRole::Water && !clients[1].connected)
        return static_cast<uint8_t>(1);
    for (std::size_t i = 0; i < clients.size(); ++i)
        if (!clients[i].connected)
            return static_cast<uint8_t>(i);
    return std::nullopt;
}

std::optional<uint8_t> Room::findSlotByEndpoint(const sf::IpAddress& address, unsigned short port) const {
    for (std::size_t i = 0; i < clients.size(); ++i)
        if (clients[i].connected && clients[i].address == address && clients[i].port == port)
            return static_cast<uint8_t>(i);
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

void Room::acceptClient(sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port,
                        const ConnectRequestPacket& request) {
    const auto existing = findSlotByEndpoint(address, port);
    if (existing.has_value()) {
        ConnectAcceptPacket accept{};
        accept.slot = existing.value();
        accept.role = clients[existing.value()].role;
        std::snprintf(accept.roomCode, MAX_ROOM_CODE, "%s", code.c_str());

        std::array<char, 768> buf{};
        std::size_t sz = 0;
        packPacket(accept, buf, sz);
        socket.send(buf.data(), sz, address, port);
        broadcastState(socket);
        return;
    }

    const auto slot = findOpenSlot(request.preferredRole);
    if (!slot.has_value()) {
        rejectClient(socket, address, port, "Room is full");
        return;
    }

    const PlayerRole role = roleForSlot(slot.value());

    ClientSlot& client = clients[slot.value()];
    client.connected = true;
    client.ready = false;
    client.waitingReady = false;
    client.role = role;
    client.address = address;
    client.port = port;
    client.pendingInput = InputFlags::None;
    client.name = request.playerName;

    ConnectAcceptPacket accept{};
    accept.slot = slot.value();
    accept.role = role;
    std::snprintf(accept.roomCode, MAX_ROOM_CODE, "%s", code.c_str());

    std::array<char, 768> buf{};
    std::size_t sz = 0;
    packPacket(accept, buf, sz);
    socket.send(buf.data(), sz, address, port);

    syncConnectedCount();
    syncWaitingReadyMask();
    broadcastState(socket);

    std::cout << "[Room " << code << "] Client joined slot " << static_cast<int>(slot.value()) << " as "
              << roleName(role) << " from " << address << ":" << port << std::endl;
}

void Room::rejectClient(sf::UdpSocket& socket, const sf::IpAddress& address, unsigned short port, const char* reason) {
    ConnectRejectPacket reject{};
    std::snprintf(reject.reason, sizeof(reject.reason), "%s", reason);

    std::array<char, 768> buf{};
    std::size_t sz = 0;
    packPacket(reject, buf, sz);
    socket.send(buf.data(), sz, address, port);
}

void Room::disconnectClient(uint8_t slot, sf::UdpSocket& socket) {
    if (slot >= clients.size())
        return;

    clients[slot] = ClientSlot{};
    syncConnectedCount();
    syncWaitingReadyMask();

    if (world.phase != GamePhase::Lobby)
        returnToLobby();

    broadcastState(socket);
    std::cout << "[Room " << code << "] Client slot " << static_cast<int>(slot) << " disconnected" << std::endl;
}

// ============================================================================
// GameServer implementation (multi-room manager)
// ============================================================================

std::string GameServer::generateRoomCode() {
    static const char kChars[] = "0123456789";
    std::string code;
    code.reserve(6);
    for (int i = 0; i < 6; ++i)
        code.push_back(kChars[std::rand() % 10]);
    return code;
}

bool GameServer::start() {
    if (socket_.bind(SERVER_PORT) != sf::Socket::Done) {
        std::cerr << "[Server] Failed to bind UDP port " << SERVER_PORT << std::endl;
        return false;
    }
    socket_.setBlocking(false);

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    running_ = true;
    lastTick_ = std::chrono::steady_clock::now();
    lastBroadcast_ = lastTick_;

    std::cout << "[Server] Multi-room server listening on port " << SERVER_PORT << std::endl;
    std::cout << "[Server] Levels loaded: " << static_cast<int>(LevelCatalog::instance().count()) << std::endl;
    return true;
}

void GameServer::run() {
    while (running_) {
        processPackets();

        const auto now = std::chrono::steady_clock::now();
        const float tickElapsed = std::chrono::duration<float>(now - lastTick_).count();
        if (tickElapsed >= TICK_DT) {
            simulateAllRooms();
            lastTick_ = now;
        }

        const float broadcastElapsed = std::chrono::duration<float>(now - lastBroadcast_).count();
        if (broadcastElapsed >= 1.0f / STATE_BROADCAST_HZ) {
            broadcastAllRooms();
            lastBroadcast_ = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void GameServer::stop() {
    running_ = false;
}

void GameServer::processPackets() {
    std::array<char, 768> buffer{};
    std::size_t received = 0;
    sf::IpAddress sender;
    unsigned short port = 0;

    while (socket_.receive(buffer.data(), buffer.size(), received, sender, port) == sf::Socket::Done) {
        if (received < sizeof(PacketHeader))
            continue;

        const auto* header = reinterpret_cast<const PacketHeader*>(buffer.data());

        switch (header->type) {
            case PacketType::ConnectRequest: {
                ConnectRequestPacket packet{};
                if (!unpackPacket(buffer.data(), received, packet))
                    break;

                // Find which room this client belongs to
                if (packet.roomCode[0] != '\0') {
                    const std::string targetCode(packet.roomCode);
                    auto it = rooms_.find(targetCode);
                    if (it != rooms_.end()) {
                        it->second->acceptClient(socket_, sender, port, packet);
                    } else {
                        // Room not found
                        ConnectRejectPacket reject{};
                        std::snprintf(reject.reason, sizeof(reject.reason), "Room not found");
                        std::array<char, 768> buf{};
                        std::size_t sz = 0;
                        packPacket(reject, buf, sz);
                        socket_.send(buf.data(), sz, sender, port);
                    }
                } else {
                    // No room code = create new room
                    std::string newCode;
                    do {
                        newCode = generateRoomCode();
                    } while (rooms_.find(newCode) != rooms_.end());

                    auto room = std::make_unique<Room>();
                    room->code = newCode;
                    room->lastTick = std::chrono::steady_clock::now();
                    room->lastBroadcast = room->lastTick;
                    room->selectLevel(0);

                    Room* raw = room.get();
                    rooms_[newCode] = std::move(room);
                    raw->acceptClient(socket_, sender, port, packet);

                    std::cout << "[Server] Created room " << newCode << std::endl;
                }
                break;
            }

            case PacketType::Input:
            case PacketType::Action:
            case PacketType::Disconnect: {
                // Route to the correct room by finding which room has this endpoint
                Room* foundRoom = nullptr;
                uint8_t foundSlot = 0;
                for (auto& [code, room] : rooms_) {
                    auto slot = room->findSlotByEndpoint(sender, port);
                    if (slot.has_value()) {
                        foundRoom = room.get();
                        foundSlot = slot.value();
                        break;
                    }
                }
                if (!foundRoom)
                    break;

                if (header->type == PacketType::Input) {
                    InputPacket pkt{};
                    if (unpackPacket(buffer.data(), received, pkt) && pkt.slot == foundSlot) {
                        if (foundRoom->world.phase == GamePhase::Playing)
                            foundRoom->clients[foundSlot].pendingInput = static_cast<InputFlags>(pkt.flags);
                    }
                } else if (header->type == PacketType::Action) {
                    ActionPacket pkt{};
                    if (unpackPacket(buffer.data(), received, pkt) && pkt.slot == foundSlot)
                        foundRoom->handleAction(foundSlot, pkt.action, pkt.value);
                } else if (header->type == PacketType::Disconnect) {
                    DisconnectPacket pkt{};
                    if (unpackPacket(buffer.data(), received, pkt) && pkt.slot == foundSlot) {
                        foundRoom->disconnectClient(foundSlot, socket_);
                        // Remove room if empty
                        bool empty = true;
                        for (const auto& c : foundRoom->clients)
                            if (c.connected) {
                                empty = false;
                                break;
                            }
                        if (empty) {
                            std::cout << "[Server] Removing empty room " << foundRoom->code << std::endl;
                            rooms_.erase(foundRoom->code);
                        }
                    }
                }
                break;
            }

            case PacketType::Discovery: {
                DiscoveryPacket disc{};
                if (!unpackPacket(buffer.data(), received, disc) || disc.isResponse != 0)
                    break;

                const std::string requested(disc.roomCode);
                for (auto& [code, room] : rooms_) {
                    if (requested.empty() || requested == code) {
                        DiscoveryPacket resp{};
                        resp.isResponse = 1;
                        std::snprintf(resp.roomCode, MAX_ROOM_CODE, "%s", code.c_str());
                        resp.playerCount = room->world.connectedCount;
                        resp.maxPlayers = MAX_PLAYERS;
                        const LevelCatalog& cat = LevelCatalog::instance();
                        if (room->selectedLevelIndex < cat.count())
                            std::snprintf(resp.levelName, MAX_LEVEL_NAME, "%s", cat.at(room->selectedLevelIndex).title);
                        std::array<char, 768> buf{};
                        std::size_t sz = 0;
                        packPacket(resp, buf, sz);
                        socket_.send(buf.data(), sz, sender, port);
                        if (!requested.empty())
                            break;  // specific match, stop
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void GameServer::simulateAllRooms() {
    for (auto& [code, room] : rooms_) {
        room->simulateTick();
    }
}

void GameServer::broadcastAllRooms() {
    for (auto& [code, room] : rooms_) {
        room->broadcastState(socket_);
    }
}

}  // namespace fireice
