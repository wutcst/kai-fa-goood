#include "GameServer.hpp"

#include "LevelProgress.hpp"
#include "LevelCatalog.hpp"
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

std::string generateRoomCode() {
    static const char kChars[] = "0123456789";
    std::string code;
    code.reserve(6);
    for (int i = 0; i < 6; ++i) {
        code.push_back(kChars[std::rand() % 10]);
    }
    return code;
}

} // namespace

bool GameServer::start(uint8_t initialLevel) {
    if (socket_.bind(SERVER_PORT) != sf::Socket::Done) {
        std::cerr << "[Server] Failed to bind UDP port " << SERVER_PORT << std::endl;
        return false;
    }

    socket_.setBlocking(false);

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    const std::string code = generateRoomCode();
    std::snprintf(roomCode_, MAX_ROOM_CODE, "%s", code.c_str());
    std::snprintf(world_.roomCode, MAX_ROOM_CODE, "%s", code.c_str());

    selectLevel(initialLevel);
    running_ = true;
    lastTick_ = std::chrono::steady_clock::now();
    lastBroadcast_ = lastTick_;

    std::cout << "[Server] Room code: " << roomCode_ << "  Listening on port " << SERVER_PORT << std::endl;
    std::cout << "[Server] Levels loaded: " << static_cast<int>(LevelCatalog::instance().count()) << std::endl;
    return true;
}

void GameServer::selectLevel(uint8_t index) {
    const LevelCatalog& catalog = LevelCatalog::instance();
    if (index >= catalog.count()) {
        index = 0;
    }

    if (!isLevelUnlocked(index)) {
        std::cout << "[Server] Level " << static_cast<int>(index + 1) << " is locked" << std::endl;
        broadcastState();
        return;
    }

    selectedLevelIndex_ = index;
    mapPath_ = catalog.resolvePath(index);
    if (!map_.loadFromFile(mapPath_)) {
        std::cerr << "[Server] Failed to load level: " << mapPath_ << std::endl;
        return;
    }

    applyLevelMetadata();
    resetWorld();
    world_.phase = GamePhase::Lobby;
    world_.countdown = 0;
    countdownTimer_ = 0.0f;

    for (ClientSlot& client : clients_) {
        client.ready = false;
        client.pendingInput = InputFlags::None;
    }

    syncConnectedCount();
    broadcastState();
    std::cout << "[Server] Selected level " << static_cast<int>(index + 1) << ": " << world_.levelName << std::endl;
}

void GameServer::applyLevelMetadata() {
    const LevelCatalog& catalog = LevelCatalog::instance();
    const LevelInfo& info = catalog.at(selectedLevelIndex_);

    const uint8_t playerCount = std::max(uint8_t{1}, world_.connectedCount);
    world_.levelIndex = catalog.globalIndexToFilteredIndex(selectedLevelIndex_, playerCount);
    world_.levelCount = catalog.countForPlayerCount(playerCount);
    world_.totalGems = static_cast<uint8_t>(std::min(255, map_.countGems()));
    std::snprintf(world_.levelName, MAX_LEVEL_NAME, "%s", info.title);
    syncProgressToWorld();
}

void GameServer::syncProgressToWorld() {
    world_.unlockedMask = unlockedMask_;
    world_.completedMask = completedMask_;
}

bool GameServer::isLevelUnlocked(uint8_t index) const {
    return fireice::isLevelUnlocked(unlockedMask_, index);
}

void GameServer::reloadMap() {
    map_.loadFromFile(mapPath_);
    applyLevelMetadata();
}

void GameServer::resetWorld() {
    reloadMap();

    world_.tick = 0;
    world_.fireDoorOpen = false;
    world_.waterDoorOpen = false;
    world_.poisonDoorOpen = false;
    world_.levelComplete = false;

    for (PlayerState& player : world_.players) {
        player = PlayerState{};
    }

    for (const SpawnPoint& spawn : map_.spawns()) {
        if (spawn.role == PlayerRole::Fire && clients_[0].connected) {
            world_.players[0].role = PlayerRole::Fire;
            world_.players[0].x = spawn.x;
            world_.players[0].y = spawn.y;
            world_.players[0].alive = true;
        } else if (spawn.role == PlayerRole::Water && clients_[1].connected) {
            world_.players[1].role = PlayerRole::Water;
            world_.players[1].x = spawn.x;
            world_.players[1].y = spawn.y;
            world_.players[1].alive = true;
        } else if (spawn.role == PlayerRole::Poison && clients_[2].connected) {
            world_.players[2].role = PlayerRole::Poison;
            world_.players[2].x = spawn.x;
            world_.players[2].y = spawn.y;
            world_.players[2].alive = true;
        }
    }
}

void GameServer::syncConnectedCount() {
    uint8_t count = 0;
    uint8_t readyMask = 0;

    for (std::size_t i = 0; i < clients_.size(); ++i) {
        if (!clients_[i].connected) {
            world_.playerNames[i][0] = '\0';
            continue;
        }
        ++count;
        if (clients_[i].ready) {
            readyMask |= static_cast<uint8_t>(1u << i);
        }
        std::snprintf(world_.playerNames[i], MAX_PLAYER_NAME, "%s", clients_[i].name.c_str());
    }

    world_.connectedCount = count;
    world_.readyMask = readyMask;
}

void GameServer::beginCountdown() {
    world_.phase = GamePhase::Countdown;
    countdownTimer_ = COUNTDOWN_SECONDS;
    world_.countdown = static_cast<uint8_t>(std::ceil(countdownTimer_));
    std::cout << "[Server] Countdown started" << std::endl;
}

void GameServer::beginPlaying() {
    resetWorld();
    world_.phase = GamePhase::Playing;
    world_.countdown = 0;

    for (ClientSlot& client : clients_) {
        client.pendingInput = InputFlags::None;
    }

    std::cout << "[Server] Game started - " << world_.levelName << std::endl;
}

void GameServer::returnToLobby() {
    resetWorld();
    world_.phase = GamePhase::Lobby;
    world_.countdown = 0;
    countdownTimer_ = 0.0f;

    for (ClientSlot& client : clients_) {
        client.ready = false;
        client.pendingInput = InputFlags::None;
    }

    syncConnectedCount();
    std::cout << "[Server] Returned to lobby" << std::endl;
}

bool GameServer::allConnectedReady() const {
    uint8_t connected = 0;
    uint8_t ready = 0;

    for (const ClientSlot& client : clients_) {
        if (!client.connected) {
            continue;
        }
        ++connected;
        if (client.ready) {
            ++ready;
        }
    }

    return connected > 0 && ready >= connected;
}

void GameServer::run() {
    while (running_) {
        processPackets();

        const auto now = std::chrono::steady_clock::now();
        const float tickElapsed = std::chrono::duration<float>(now - lastTick_).count();
        if (tickElapsed >= TICK_DT) {
            simulateTick();
            lastTick_ = now;
        }

        const float broadcastElapsed = std::chrono::duration<float>(now - lastBroadcast_).count();
        if (broadcastElapsed >= 1.0f / STATE_BROADCAST_HZ) {
            broadcastState();
            lastBroadcast_ = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void GameServer::stop() {
    running_ = false;
}

void GameServer::processPackets() {
    std::array<char, 512> buffer{};
    std::size_t received = 0;
    sf::IpAddress sender;
    unsigned short port = 0;

    while (socket_.receive(buffer.data(), buffer.size(), received, sender, port) == sf::Socket::Done) {
        if (received < sizeof(PacketHeader)) {
            continue;
        }

        const auto* header = reinterpret_cast<const PacketHeader*>(buffer.data());
        switch (header->type) {
        case PacketType::ConnectRequest: {
            ConnectRequestPacket packet{};
            if (!unpackPacket(buffer.data(), received, packet)) {
                break;
            }
            acceptClient(sender, port, packet);
            break;
        }
        case PacketType::Input: {
            InputPacket packet{};
            if (!unpackPacket(buffer.data(), received, packet)) {
                break;
            }
            const auto slot = findSlotByEndpoint(sender, port);
            if (!slot.has_value() || packet.slot != slot.value()) {
                break;
            }
            if (world_.phase == GamePhase::Playing) {
                clients_[slot.value()].pendingInput = static_cast<InputFlags>(packet.flags);
            }
            break;
        }
        case PacketType::Action: {
            ActionPacket packet{};
            if (!unpackPacket(buffer.data(), received, packet)) {
                break;
            }
            const auto slot = findSlotByEndpoint(sender, port);
            if (!slot.has_value() || packet.slot != slot.value()) {
                break;
            }
            handleAction(slot.value(), packet.action, packet.value);
            break;
        }
        case PacketType::Disconnect: {
            DisconnectPacket packet{};
            if (!unpackPacket(buffer.data(), received, packet)) {
                break;
            }
            if (packet.slot < clients_.size()) {
                clients_[packet.slot] = ClientSlot{};
                syncConnectedCount();

                if (world_.phase == GamePhase::Countdown || world_.phase == GamePhase::Playing) {
                    returnToLobby();
                }

                std::cout << "[Server] Client slot " << static_cast<int>(packet.slot) << " disconnected" << std::endl;
            }
            break;
        }
        default:
            break;
        }
    }
}

void GameServer::handleAction(uint8_t slot, PlayerAction action, uint8_t value) {
    if (!clients_[slot].connected) {
        return;
    }

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, world_.connectedCount);
    const uint8_t filteredCount = catalog.countForPlayerCount(playerCount);

    if (world_.phase == GamePhase::Lobby) {
        if (action == PlayerAction::ReturnToLobby) {
            clients_[slot].ready = false;
            syncConnectedCount();
            broadcastState();
            return;
        }

        if (action == PlayerAction::PrevLevel || action == PlayerAction::NextLevel) {
            const uint8_t currentFiltered = catalog.globalIndexToFilteredIndex(selectedLevelIndex_, playerCount);
            uint8_t newFiltered = currentFiltered;
            if (action == PlayerAction::PrevLevel && currentFiltered > 0) {
                newFiltered = currentFiltered - 1;
            } else if (action == PlayerAction::NextLevel && currentFiltered + 1 < filteredCount) {
                newFiltered = currentFiltered + 1;
            }
            if (newFiltered != currentFiltered) {
                selectLevel(catalog.filteredIndexToGlobalIndex(newFiltered, playerCount));
            }
            return;
        }

        if (action == PlayerAction::SelectLevel) {
            if (value < filteredCount) {
                selectLevel(catalog.filteredIndexToGlobalIndex(value, playerCount));
            }
            return;
        }

        if (action == PlayerAction::Ready) {
            clients_[slot].ready = true;
            syncConnectedCount();
            std::cout << "[Server] Slot " << static_cast<int>(slot) << " is ready" << std::endl;

            if (allConnectedReady()) {
                beginCountdown();
            }
            return;
        }
    }

    if (action == PlayerAction::ReturnToLobby && world_.phase != GamePhase::Lobby) {
        returnToLobby();
        return;
    }

    if (action == PlayerAction::Restart
        && (world_.phase == GamePhase::Victory || world_.phase == GamePhase::GameOver)) {
        returnToLobby();
        return;
    }

    if (action == PlayerAction::NextLevel && world_.phase == GamePhase::Victory) {
        const uint8_t currentFiltered = catalog.globalIndexToFilteredIndex(selectedLevelIndex_, playerCount);
        if (currentFiltered + 1 < filteredCount) {
            selectLevel(catalog.filteredIndexToGlobalIndex(currentFiltered + 1, playerCount));
        } else {
            returnToLobby();
        }
    }
}

void GameServer::simulateTick() {
    if (world_.phase == GamePhase::Countdown) {
        countdownTimer_ = std::max(0.0f, countdownTimer_ - TICK_DT);
        world_.countdown = static_cast<uint8_t>(std::ceil(countdownTimer_));
        if (countdownTimer_ <= 0.0f) {
            beginPlaying();
        }
        ++world_.tick;
        return;
    }

    if (world_.phase != GamePhase::Playing) {
        ++world_.tick;
        return;
    }

    updateButtons(map_, world_);

    for (std::size_t i = 0; i < clients_.size(); ++i) {
        if (!clients_[i].connected) {
            continue;
        }

        PlayerState& player = world_.players[i];
        applyInput(player, clients_[i].pendingInput, TICK_DT);
        integratePlayer(player, map_, world_, TICK_DT);

        if (sampleHazard(map_, player, world_)) {
            player.alive = false;
        }

        collectGems(player, map_);
        player.atExit = sampleExit(map_, player);
    }

    updatePhase();
    ++world_.tick;
}

void GameServer::updatePhase() {
    bool anyDead = false;
    for (std::size_t i = 0; i < clients_.size(); ++i) {
        if (!clients_[i].connected) {
            continue;
        }
        if (!world_.players[i].alive) {
            anyDead = true;
            break;
        }
    }

    if (anyDead) {
        world_.phase = GamePhase::GameOver;
        world_.levelComplete = false;
        std::cout << "[Server] Game over" << std::endl;
        return;
    }

    bool allDone = true;
    for (std::size_t i = 0; i < clients_.size(); ++i) {
        if (!clients_[i].connected) {
            continue;
        }
        if (!world_.players[i].atExit) {
            allDone = false;
            break;
        }
    }

    if (allDone && world_.connectedCount > 0) {
        world_.phase = GamePhase::Victory;
        world_.levelComplete = true;
        completedMask_ |= static_cast<uint8_t>(1u << selectedLevelIndex_);
        if (selectedLevelIndex_ + 1 < LevelCatalog::instance().count()) {
            unlockedMask_ |= static_cast<uint8_t>(1u << (selectedLevelIndex_ + 1));
        }
        syncProgressToWorld();
        std::cout << "[Server] Level complete" << std::endl;
    }
}

void GameServer::broadcastState() {
    syncConnectedCount();

    StatePacket packet{};
    packet.world = world_;

    std::array<char, 512> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return;
    }

    for (const ClientSlot& client : clients_) {
        if (!client.connected) {
            continue;
        }
        socket_.send(buffer.data(), size, client.address, client.port);
    }
}

std::optional<uint8_t> GameServer::findOpenSlot(PlayerRole preferred) const {
    const uint8_t fireSlot = 0;
    const uint8_t waterSlot = 1;

    if (preferred == PlayerRole::Fire && !clients_[fireSlot].connected) {
        return fireSlot;
    }
    if (preferred == PlayerRole::Water && !clients_[waterSlot].connected) {
        return waterSlot;
    }

    for (std::size_t i = 0; i < clients_.size(); ++i) {
        if (!clients_[i].connected) {
            return static_cast<uint8_t>(i);
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> GameServer::findSlotByEndpoint(const sf::IpAddress& address, unsigned short port) const {
    for (std::size_t i = 0; i < clients_.size(); ++i) {
        if (clients_[i].connected && clients_[i].address == address && clients_[i].port == port) {
            return static_cast<uint8_t>(i);
        }
    }
    return std::nullopt;
}

PlayerRole GameServer::roleForSlot(uint8_t slot) const {
    switch (slot) {
    case 0: return PlayerRole::Fire;
    case 1: return PlayerRole::Water;
    case 2: return PlayerRole::Poison;
    default: return PlayerRole::None;
    }
}

void GameServer::acceptClient(const sf::IpAddress& address, unsigned short port, const ConnectRequestPacket& request) {
    const auto existing = findSlotByEndpoint(address, port);
    if (existing.has_value()) {
        ConnectAcceptPacket accept{};
        accept.slot = existing.value();
        accept.role = clients_[existing.value()].role;

        std::array<char, 512> buffer{};
        std::size_t size = 0;
        packPacket(accept, buffer, size);
        socket_.send(buffer.data(), size, address, port);
        broadcastState();
        return;
    }

    if (request.roomCode[0] != '\0') {
        const std::string clientCode(request.roomCode);
        const std::string serverCode(roomCode_);
        if (clientCode != serverCode) {
            rejectClient(address, port, "Room code mismatch");
            return;
        }
    }

    const auto slot = findOpenSlot(request.preferredRole);
    if (!slot.has_value()) {
        rejectClient(address, port, "Room is full");
        return;
    }

    const PlayerRole role = roleForSlot(slot.value());

    ClientSlot& client = clients_[slot.value()];
    client.connected = true;
    client.ready = false;
    client.role = role;
    client.address = address;
    client.port = port;
    client.pendingInput = InputFlags::None;
    client.name = request.playerName;

    ConnectAcceptPacket accept{};
    accept.slot = slot.value();
    accept.role = role;
    accept.worldSeed = 1;

    std::array<char, 512> buffer{};
    std::size_t size = 0;
    packPacket(accept, buffer, size);
    socket_.send(buffer.data(), size, address, port);

    syncConnectedCount();
    broadcastState();

    std::cout << "[Server] Client joined slot " << static_cast<int>(slot.value())
              << " as " << roleName(role) << " from " << address << ":" << port << std::endl;
}

void GameServer::rejectClient(const sf::IpAddress& address, unsigned short port, const char* reason) {
    ConnectRejectPacket reject{};
    std::snprintf(reject.reason, sizeof(reject.reason), "%s", reason);

    std::array<char, 512> buffer{};
    std::size_t size = 0;
    packPacket(reject, buffer, size);
    socket_.send(buffer.data(), size, address, port);
}

} // namespace fireice
