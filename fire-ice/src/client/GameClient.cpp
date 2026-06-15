#include "GameClient.hpp"
#include "LevelCatalog.hpp"
#include "LevelMapLayout.hpp"
#include "LevelMechanics.hpp"
#include "LevelProgress.hpp"
#include "LocaleText.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <iostream>

namespace fireice {

namespace {

constexpr float kPreviewMargin = 12.0f;
constexpr float kTitleCenterX = 512.0f;
constexpr float kMenuStartY = 300.0f;
constexpr float kMenuItemStep = 52.0f;

constexpr float kRoomPanelW = 200.0f;
constexpr float kRoomPanelH = 250.0f;
constexpr float kRoomPanelY = 84.0f;
constexpr float kRoomPanelGap = 12.0f;
constexpr float kRoomBottomBarH = 64.0f;
constexpr float kRoomActionBtnW = 180.0f;
constexpr float kRoomActionBtnH = 44.0f;
constexpr float kRoomActionBtnGap = 20.0f;

float roomBottomBarY() {
    return static_cast<float>(LOBBY_WINDOW_HEIGHT) - kRoomBottomBarH;
}

std::array<sf::FloatRect, 3> roomActionButtonAreas() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float totalW = kRoomActionBtnW * 3.0f + kRoomActionBtnGap * 2.0f;
    const float startX = (w - totalW) / 2.0f;
    const float btnY = roomBottomBarY() + (kRoomBottomBarH - kRoomActionBtnH) / 2.0f;
    return {
        sf::FloatRect{startX, btnY, kRoomActionBtnW, kRoomActionBtnH},
        sf::FloatRect{startX + kRoomActionBtnW + kRoomActionBtnGap, btnY, kRoomActionBtnW, kRoomActionBtnH},
        sf::FloatRect{startX + (kRoomActionBtnW + kRoomActionBtnGap) * 2.0f, btnY, kRoomActionBtnW, kRoomActionBtnH},
    };
}

}  // namespace

bool GameClient::initialize(const std::string& host, PlayerRole preferredRole, bool autoConnect) {
    host_ = host;
    serverAddress_ = sf::IpAddress(host);
    if (serverAddress_ == sf::IpAddress::None) {
        serverAddress_ = sf::IpAddress::LocalHost;
    }
    preferredRole_ = preferredRole;
    role_ = preferredRole;
    playerName_ = preferredRole == PlayerRole::Fire ? "FireBoy" : "WaterGirl";
    roomAnimTimer_ = 0.0f;
    clientScreen_ = ClientScreen::Title;
    titleMenuIndex_ = 0;
    titleHoverIndex_ = -1;
    connectRequested_ = false;
    connected_ = false;

    if (!map_.loadFromFile(LevelCatalog::instance().resolvePath(0))) {
        std::cerr << "[Client] Failed to load default level preview" << std::endl;
        return false;
    }
    renderWorld_.levelCount = LevelCatalog::instance().count();
    renderWorld_.levelIndex = 0;
    renderWorld_.totalGems = static_cast<uint8_t>(std::min(255, map_.countGems()));
    std::snprintf(renderWorld_.levelName, MAX_LEVEL_NAME, "%s", LevelCatalog::instance().at(0).title);
    handleLevelChange(0, false);

    if (socket_.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
        std::cerr << "[Client] Failed to bind local UDP port" << std::endl;
        return false;
    }
    localPort_ = socket_.getLocalPort();
    socket_.setBlocking(false);

    ui_.loadFont();
    if (assets_.load()) {
        if (lobbyMusic_.openFromFile(assets_.musicPath())) {
            lobbyMusic_.setLoop(true);
            lobbyMusic_.setVolume(35.0f);
            musicEnabled_ = true;
        }
    } else {
        std::cerr << "[Client] Texture assets not found, using fallback shapes" << std::endl;
    }
    useTitleLayout();

    if (musicEnabled_) {
        lobbyMusic_.play();
    }

    lastConnectRetry_ = std::chrono::steady_clock::now();
    lastInputSend_ = lastConnectRetry_;

    std::cout << "[Client] Title screen ready. Server: " << serverAddress_.toString() << ":" << SERVER_PORT
              << "  Role: " << roleName(preferredRole_) << std::endl;

    if (autoConnect && host_ != "127.0.0.1") {
        std::cout << "[Client] Auto-connecting to " << host_ << "..." << std::endl;
        typedRoomCode_.clear();
        beginConnect();
    }

    return true;
}

void GameClient::useTitleLayout() {
    lobbyLayout_ = true;
    const sf::Vector2u size(LOBBY_WINDOW_WIDTH, LOBBY_WINDOW_HEIGHT);
    if (!window_.isOpen()) {
        window_.create(sf::VideoMode(size.x, size.y), text::windowTitle());
    } else {
        window_.setSize(size);
        window_.setTitle(text::windowTitle());
    }
    window_.setFramerateLimit(60);
    window_.setVerticalSyncEnabled(true);
    window_.setView(sf::View(sf::FloatRect(0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y))));
}

void GameClient::beginConnect() {
    if (connected_ || connectRequested_) {
        return;
    }

    role_ = preferredRole_;
    clientScreen_ = ClientScreen::Connecting;
    connectRequested_ = true;
    lastConnectRetry_ = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    sendConnectRequest();
    std::cout << "[Client] Connecting to " << serverAddress_.toString() << ":" << SERVER_PORT << std::endl;
}

void GameClient::sendConnectRequest() {
    ConnectRequestPacket request{};
    request.preferredRole = preferredRole_;
    std::snprintf(request.playerName, sizeof(request.playerName), "%s", playerName_.c_str());
    if (!typedRoomCode_.empty()) {
        std::snprintf(request.roomCode, sizeof(request.roomCode), "%s", typedRoomCode_.c_str());
    }

    std::array<char, 768> buffer{};
    std::size_t size = 0;
    packPacket(request, buffer, size);
    socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT);
    lastConnectRetry_ = std::chrono::steady_clock::now();
}

std::vector<sf::FloatRect> GameClient::titleMenuHitAreas() const {
    const auto& items = text::titleMenuItems();
    std::vector<sf::FloatRect> areas;
    areas.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        const float y = kMenuStartY + static_cast<float>(i) * kMenuItemStep;
        areas.push_back({kTitleCenterX - 220.0f, y - 8.0f, 440.0f, 44.0f});
    }
    return areas;
}

void GameClient::handleTitleMenuSelect(int index) {
    switch (index) {
        case 0:
            typedRoomCode_.clear();
            host_ = "127.0.0.1";
            serverAddress_ = sf::IpAddress::LocalHost;
            if (startHosting()) {
                beginConnect();
            } else {
                clientScreen_ = ClientScreen::Title;
            }
            break;
        case 1:
            typedRoomCode_.clear();
            discoveredRooms_.clear();
            selectedDiscoveredRoom_ = -1;
            clientScreen_ = ClientScreen::JoinRoom;
            broadcastDiscovery();
            break;
        case 2:
            clientScreen_ = ClientScreen::Help;
            break;
        case 3:
            if (preferredRole_ == PlayerRole::Fire) {
                preferredRole_ = PlayerRole::Water;
            } else if (preferredRole_ == PlayerRole::Water) {
                preferredRole_ = PlayerRole::Poison;
            } else {
                preferredRole_ = PlayerRole::Fire;
            }
            role_ = preferredRole_;
            playerName_ = preferredRole_ == PlayerRole::Fire    ? "FireBoy"
                          : preferredRole_ == PlayerRole::Water ? "WaterGirl"
                                                                : "PoisonKid";
            break;
        case 4:
            clientScreen_ = ClientScreen::Credits;
            break;
        case 5:
            window_.close();
            break;
        default:
            break;
    }
}

void GameClient::handleTitleInput(const sf::Event& event) {
    if (clientScreen_ == ClientScreen::Help || clientScreen_ == ClientScreen::Credits) {
        if (event.key.code == sf::Keyboard::Escape || event.key.code == sf::Keyboard::Enter) {
            clientScreen_ = ClientScreen::Title;
        }
        return;
    }

    if (clientScreen_ == ClientScreen::JoinRoom) {
        if (event.key.code == sf::Keyboard::Escape) {
            typedRoomCode_.clear();
            discoveredRooms_.clear();
            clientScreen_ = ClientScreen::Title;
        } else if (event.key.code == sf::Keyboard::F5) {
            broadcastDiscovery();
        } else if (event.key.code == sf::Keyboard::Up) {
            if (!discoveredRooms_.empty() && selectedDiscoveredRoom_ > 0) {
                selectedDiscoveredRoom_--;
                typedRoomCode_.clear();
            }
        } else if (event.key.code == sf::Keyboard::Down) {
            if (!discoveredRooms_.empty() && selectedDiscoveredRoom_ < static_cast<int>(discoveredRooms_.size()) - 1) {
                selectedDiscoveredRoom_++;
                typedRoomCode_.clear();
            }
        } else if (event.key.code == sf::Keyboard::Enter) {
            if (selectedDiscoveredRoom_ >= 0 && selectedDiscoveredRoom_ < static_cast<int>(discoveredRooms_.size())) {
                const auto& room = discoveredRooms_[selectedDiscoveredRoom_];
                host_ = room.address;
                serverAddress_ = sf::IpAddress(host_);
                typedRoomCode_ = room.roomCode;
                beginConnect();
            } else if (typedRoomCode_.size() == 6 || typedRoomCode_.empty()) {
                beginConnect();
            }
        } else if (event.key.code == sf::Keyboard::Backspace && !typedRoomCode_.empty()) {
            typedRoomCode_.pop_back();
            selectedDiscoveredRoom_ = -1;
        }
        return;
    }

    if (clientScreen_ == ClientScreen::Connecting) {
        if (event.key.code == sf::Keyboard::Escape) {
            connectRequested_ = false;
            clientScreen_ = ClientScreen::Title;
        }
        return;
    }

    if (event.key.code == sf::Keyboard::Up || event.key.code == sf::Keyboard::W) {
        const int count = static_cast<int>(text::titleMenuItems().size());
        titleMenuIndex_ = (titleMenuIndex_ - 1 + count) % count;
        return;
    }
    if (event.key.code == sf::Keyboard::Down || event.key.code == sf::Keyboard::S) {
        const int count = static_cast<int>(text::titleMenuItems().size());
        titleMenuIndex_ = (titleMenuIndex_ + 1) % count;
        return;
    }
    if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space) {
        handleTitleMenuSelect(titleMenuIndex_);
    }
}

void GameClient::handleTitleMouseMove(const sf::Event& event) {
    if (clientScreen_ != ClientScreen::Title) {
        titleHoverIndex_ = -1;
        return;
    }

    const sf::Vector2f mouse(static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y));
    titleHoverIndex_ = -1;
    const std::vector<sf::FloatRect> areas = titleMenuHitAreas();
    for (std::size_t i = 0; i < areas.size(); ++i) {
        if (areas[i].contains(mouse)) {
            titleHoverIndex_ = static_cast<int>(i);
            titleMenuIndex_ = static_cast<int>(i);
            break;
        }
    }
}

void GameClient::handleTitleMouseClick(const sf::Event& event) {
    if (event.mouseButton.button != sf::Mouse::Left) {
        return;
    }

    if (clientScreen_ == ClientScreen::Help || clientScreen_ == ClientScreen::Credits) {
        clientScreen_ = ClientScreen::Title;
        return;
    }

    if (clientScreen_ != ClientScreen::Title) {
        return;
    }

    const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
    const std::vector<sf::FloatRect> areas = titleMenuHitAreas();
    for (std::size_t i = 0; i < areas.size(); ++i) {
        if (areas[i].contains(mouse)) {
            titleMenuIndex_ = static_cast<int>(i);
            handleTitleMenuSelect(static_cast<int>(i));
            break;
        }
    }
}

void GameClient::useLobbyLayout() {
    lobbyLayout_ = true;
    const sf::Vector2u size(LOBBY_WINDOW_WIDTH, LOBBY_WINDOW_HEIGHT);
    if (!window_.isOpen()) {
        window_.create(sf::VideoMode(size.x, size.y), "Fire-Ice Online - Lobby");
    } else {
        window_.setSize(size);
        window_.setTitle("Fire-Ice Online - Lobby");
    }
    window_.setFramerateLimit(60);
    window_.setVerticalSyncEnabled(true);
    window_.setView(sf::View(sf::FloatRect(0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y))));
}

void GameClient::useGameLayout() {
    lobbyLayout_ = false;
    const unsigned width = static_cast<unsigned>(std::max(960, static_cast<int>(map_.width() * TILE_SIZE)));
    const unsigned height = static_cast<unsigned>(std::max(540, static_cast<int>(map_.height() * TILE_SIZE + 80)));
    window_.setSize({width, height});
    window_.setTitle("Fire-Ice Online");
    window_.setView(sf::View(sf::FloatRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))));
}

void GameClient::handleLevelChange(uint8_t levelIndex, bool resizeWindow) {
    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
    const uint8_t globalIndex = catalog.filteredIndexToGlobalIndex(levelIndex, playerCount);
    const std::string visualPath = catalog.resolveVisualPath(globalIndex);
    if (globalIndex == loadedLevelIndex_) {
        if (visualPath.empty() || tiledMap_.ready()) {
            return;
        }
    }

    const std::string path = catalog.resolvePath(globalIndex);
    if (!map_.loadFromFile(path)) {
        std::cerr << "[Client] Failed to load level preview: " << path << std::endl;
        return;
    }

    if (!visualPath.empty() && tiledMap_.load(visualPath)) {
        tiledMap_.bake();
    } else if (!visualPath.empty()) {
        std::cerr << "[Client] Failed to load tiled visual map: " << visualPath << std::endl;
    }

    loadedLevelIndex_ = globalIndex;
    if (resizeWindow && !lobbyLayout_) {
        useGameLayout();
    }
}

void GameClient::onPhaseChanged(GamePhase previous, GamePhase current) {
    if (current == GamePhase::Lobby && previous != GamePhase::Lobby) {
        localReady_ = false;
        levelMapScrollY_ = 0.0f;
        if (clientScreen_ == ClientScreen::Room) {
            useLobbyLayout();
        }
        handleLevelChange(renderWorld_.levelIndex, false);
    } else if (previous == GamePhase::Lobby && current != GamePhase::Lobby) {
        handleLevelChange(renderWorld_.levelIndex, false);
        useGameLayout();
    }
    updateMusic(current);
}

void GameClient::updateMusic(GamePhase phase) {
    if (!musicEnabled_) {
        return;
    }

    const bool titleOrLobby = !connected_ || phase == GamePhase::Lobby;
    if (titleOrLobby) {
        if (lobbyMusic_.getStatus() != sf::Music::Playing) {
            lobbyMusic_.play();
        }
    } else if (lobbyMusic_.getStatus() == sf::Music::Playing) {
        lobbyMusic_.pause();
    }
}

void GameClient::run() {
    while (window_.isOpen()) {
        sf::Event event{};
        while (window_.pollEvent(event)) {
            handleWindowEvent(event);
        }

        pollNetwork();

        if (connectRequested_ && !connected_) {
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<float>(now - lastConnectRetry_).count() > 1.0f) {
                sendConnectRequest();
            }
        } else if (connected_ && renderWorld_.phase == GamePhase::Playing) {
            currentInput_ = readLocalInput();
            sendInput();
        }

        render();
    }

    disconnect();
}

void GameClient::disconnect() {
    if (lobbyMusic_.getStatus() == sf::Music::Playing) {
        lobbyMusic_.stop();
    }

    if (!connected_) {
        return;
    }

    DisconnectPacket packet{};
    packet.slot = slot_;
    std::array<char, 768> buffer{};
    std::size_t size = 0;
    packPacket(packet, buffer, size);
    socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT);
    connected_ = false;

    if (isHosting_) {
        stopHosting();
    }
}

bool GameClient::startHosting() {
    if (isHosting_) {
        return true;
    }

    server_ = std::make_unique<GameServer>();
    if (!server_->start()) {
        std::cerr << "[Client] Failed to start internal server (port " << SERVER_PORT << " may be in use)" << std::endl;
        server_.reset();
        return false;
    }

    isHosting_ = true;

    localIp_ = sf::IpAddress::getLocalAddress().toString();
    if (localIp_ == "0.0.0.0" || localIp_.empty()) {
        localIp_ = "127.0.0.1";
    }

    serverThread_ = std::thread([this]() { server_->run(); });

    std::cout << "[Client] Internal server started. Local IP: " << localIp_ << ":" << SERVER_PORT << std::endl;
    return true;
}

void GameClient::stopHosting() {
    if (!isHosting_) {
        return;
    }

    server_->stop();
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    server_.reset();
    isHosting_ = false;
    localIp_.clear();

    std::cout << "[Client] Internal server stopped" << std::endl;
}

void GameClient::broadcastDiscovery() {
    if (discoveryActive_)
        return;

    discoveryActive_ = true;
    discoveredRooms_.clear();
    selectedDiscoveredRoom_ = -1;

    const std::string targetCode = typedRoomCode_;

    std::thread([this, targetCode]() {
        sf::UdpSocket scanSocket;
        if (scanSocket.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
            discoveryActive_ = false;
            return;
        }
        scanSocket.setBlocking(false);

        DiscoveryPacket disc{};
        disc.isResponse = 0;
        if (!targetCode.empty()) {
            std::snprintf(disc.roomCode, MAX_ROOM_CODE, "%s", targetCode.c_str());
        }

        std::array<char, 768> buf{};
        std::size_t sz = 0;
        packPacket(disc, buf, sz);

        for (int i = 0; i < 3; ++i) {
            scanSocket.send(buf.data(), sz, sf::IpAddress::Broadcast, SERVER_PORT);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        auto startTime = std::chrono::steady_clock::now();
        while (discoveryActive_) {
            char recvBuf[512];
            std::size_t received = 0;
            sf::IpAddress sender;
            unsigned short senderPort = 0;

            if (scanSocket.receive(recvBuf, sizeof(recvBuf), received, sender, senderPort) == sf::Socket::Done) {
                DiscoveryPacket resp{};
                if (unpackPacket(recvBuf, received, resp) && resp.isResponse == 1) {
                    DiscoveredRoom room;
                    room.address = sender.toString();
                    room.roomCode = resp.roomCode;
                    room.levelName = resp.levelName[0] ? resp.levelName : "???";
                    room.playerCount = resp.playerCount;
                    room.maxPlayers = resp.maxPlayers;

                    bool duplicate = false;
                    for (const auto& dr : discoveredRooms_) {
                        if (dr.address == room.address && dr.roomCode == room.roomCode) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate && room.roomCode[0]) {
                        discoveredRooms_.push_back(room);
                        if (selectedDiscoveredRoom_ < 0)
                            selectedDiscoveredRoom_ = 0;
                    }
                }
            }

            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime)
                    .count();
            if (elapsed > 2000)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        discoveryActive_ = false;
    }).detach();
}

void GameClient::handleDiscoveryResponse(const DiscoveryPacket& packet, const sf::IpAddress& sender) {
    if (!discoveryActive_ || !packet.isResponse) {
        return;
    }

    DiscoveredRoom room;
    room.address = sender.toString();
    room.roomCode = packet.roomCode;
    room.levelName = packet.levelName[0] ? packet.levelName : "???";
    room.playerCount = packet.playerCount;
    room.maxPlayers = packet.maxPlayers;

    for (const auto& dr : discoveredRooms_) {
        if (dr.address == room.address && dr.roomCode == room.roomCode) {
            return;
        }
    }
    if (room.roomCode[0]) {
        discoveredRooms_.push_back(room);
        if (selectedDiscoveredRoom_ < 0) {
            selectedDiscoveredRoom_ = 0;
        }
    }
}

void GameClient::renderJoinRoomScanResults() {
    // 扫描结果已内嵌在 renderJoinRoomScreen() 中绘制
}

void GameClient::pollNetwork() {
    std::array<char, 768> buffer{};
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
                if (!unpackPacket(buffer.data(), received, packet)) {
                    break;
                }
                connected_ = true;
                connectRequested_ = false;
                clientScreen_ = ClientScreen::Room;
                slot_ = packet.slot;
                role_ = packet.role;
                preferredRole_ = packet.role;
                localReady_ = false;
                roomAnimTimer_ = 0.0f;
                std::snprintf(renderWorld_.roomCode, MAX_ROOM_CODE, "%s", packet.roomCode);
                std::snprintf(world_.roomCode, MAX_ROOM_CODE, "%s", packet.roomCode);
                useLobbyLayout();
                updateMusic(GamePhase::Lobby);
                std::cout << "[Client] Connected as " << roleName(role_) << " (slot " << static_cast<int>(slot_) << ")"
                          << std::endl;
                break;
            }
            case PacketType::ConnectReject: {
                ConnectRejectPacket packet{};
                if (!unpackPacket(buffer.data(), received, packet)) {
                    break;
                }
                connectRequested_ = false;
                clientScreen_ = ClientScreen::Title;
                std::cerr << "[Client] Rejected: " << packet.reason << std::endl;
                break;
            }
            case PacketType::State: {
                StatePacket packet{};
                if (!unpackPacket(buffer.data(), received, packet)) {
                    break;
                }

                const GamePhase previousPhase = world_.phase;
                const uint8_t previousLobbyStep = world_.lobbyStep;

                world_ = packet.world;
                renderWorld_ = packet.world;

                const LevelCatalog& catalog = LevelCatalog::instance();
                const uint8_t globalLevelIndex = catalog.filteredIndexToGlobalIndex(
                    renderWorld_.levelIndex, std::max(uint8_t{1}, renderWorld_.connectedCount));
                if (globalLevelIndex != loadedLevelIndex_) {
                    handleLevelChange(renderWorld_.levelIndex, !lobbyLayout_);
                }

                if (renderWorld_.lobbyStep == 0 && previousLobbyStep == 1) {
                    localReady_ = false;
                    levelMapScrollY_ = 0.0f;
                }
                if (renderWorld_.lobbyStep == 1 && previousLobbyStep == 0) {
                    levelMapScrollY_ = 0.0f;
                }

                if (previousPhase != renderWorld_.phase) {
                    onPhaseChanged(previousPhase, renderWorld_.phase);
                }
                break;
            }
            default:
                break;
        }
    }
}

bool GameClient::sendInput() {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - lastInputSend_).count() < 1.0f / 30.0f) {
        return false;
    }

    InputPacket packet{};
    packet.slot = slot_;
    packet.tick = ++inputTick_;
    packet.flags = static_cast<uint8_t>(currentInput_);

    std::array<char, 768> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return false;
    }

    if (socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT) != sf::Socket::Done) {
        return false;
    }

    lastInputSend_ = now;
    return true;
}

bool GameClient::sendAction(PlayerAction action, uint8_t value) {
    ActionPacket packet{};
    packet.slot = slot_;
    packet.action = action;
    packet.value = value;

    std::array<char, 768> buffer{};
    std::size_t size = 0;
    if (!packPacket(packet, buffer, size)) {
        return false;
    }

    return socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT) == sf::Socket::Done;
}

void GameClient::handleWindowEvent(const sf::Event& event) {
    if (event.type == sf::Event::Closed) {
        window_.close();
        return;
    }

    if (!connected_) {
        if (event.type == sf::Event::KeyPressed) {
            handleTitleInput(event);
        } else if (event.type == sf::Event::MouseMoved) {
            handleTitleMouseMove(event);
        } else if (event.type == sf::Event::MouseButtonPressed) {
            handleTitleMouseClick(event);
        } else if (event.type == sf::Event::TextEntered && clientScreen_ == ClientScreen::JoinRoom) {
            const uint32_t unicode = event.text.unicode;
            if (unicode >= '0' && unicode <= '9' && typedRoomCode_.size() < 6) {
                typedRoomCode_.push_back(static_cast<char>(unicode));
            }
        }
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && connected_ && renderWorld_.phase == GamePhase::Lobby &&
        renderWorld_.lobbyStep == 0) {
        if (event.mouseButton.button != sf::Mouse::Left) {
            return;
        }
        const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
        const auto buttons = roomActionButtonAreas();
        const sf::FloatRect& readyArea = buttons[0];
        const sf::FloatRect& nextArea = buttons[1];
        const sf::FloatRect& leaveArea = buttons[2];

        if (readyArea.contains(mouse)) {
            sendAction(PlayerAction::WaitingReady);
            return;
        }

        if (nextArea.contains(mouse) && allPlayersWaitingReady()) {
            sendAction(PlayerAction::ProceedToMapSelect);
            return;
        }

        if (leaveArea.contains(mouse)) {
            disconnect();
            connected_ = false;
            connectRequested_ = false;
            clientScreen_ = ClientScreen::Title;
            useTitleLayout();
            updateMusic(GamePhase::Lobby);
            return;
        }
        return;
    }

    if (renderWorld_.phase == GamePhase::Lobby && renderWorld_.lobbyStep == 1) {
        if (event.type == sf::Event::MouseMoved) {
            lobbyHoverNode_ =
                levelNodeAtPosition(static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y));
        } else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
            const sf::FloatRect playArea{740.0f, 576.0f, 260.0f, 48.0f};
            if (playArea.contains(mouse)) {
                if (!localReady_) {
                    const LevelCatalog& catalog = LevelCatalog::instance();
                    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
                    const uint8_t globalSelected =
                        catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, playerCount);
                    if (isLevelUnlocked(renderWorld_.unlockedMask, globalSelected)) {
                        localReady_ = true;
                        sendAction(PlayerAction::Ready);
                    }
                } else {
                    localReady_ = false;
                    sendAction(PlayerAction::ReturnToLobby);
                }
                return;
            }
            handleLobbyMouseClick(event);
        } else if (event.type == sf::Event::MouseWheelScrolled) {
            handleLobbyMouseWheel(event);
        }
    }

    if (event.type != sf::Event::KeyPressed) {
        return;
    }

    if (event.key.code == sf::Keyboard::Escape && connected_ && renderWorld_.phase == GamePhase::Lobby) {
        if (renderWorld_.lobbyStep == 1) {
            localReady_ = false;
            sendAction(PlayerAction::BackToWaitingRoom);
            return;
        }
        disconnect();
        connected_ = false;
        connectRequested_ = false;
        clientScreen_ = ClientScreen::Title;
        useTitleLayout();
        updateMusic(GamePhase::Lobby);
        return;
    }

    if (event.key.code == sf::Keyboard::Escape && renderWorld_.phase != GamePhase::Lobby) {
        localReady_ = false;
        sendAction(PlayerAction::ReturnToLobby);
        return;
    }

    if (connected_ && renderWorld_.phase == GamePhase::Lobby && renderWorld_.lobbyStep == 0) {
        if (event.key.code == sf::Keyboard::Enter && !localWaitingReady()) {
            sendAction(PlayerAction::WaitingReady);
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && localWaitingReady() && allPlayersWaitingReady()) {
            sendAction(PlayerAction::ProceedToMapSelect);
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && localWaitingReady() && !allPlayersWaitingReady()) {
            sendAction(PlayerAction::WaitingReady);
            return;
        }
        return;
    }

    if (renderWorld_.phase == GamePhase::Lobby && renderWorld_.lobbyStep == 1) {
        if ((event.key.code == sf::Keyboard::Left || event.key.code == sf::Keyboard::Up ||
             event.key.code == sf::Keyboard::Q) &&
            !localReady_) {
            sendAction(PlayerAction::PrevLevel);
            return;
        }
        if ((event.key.code == sf::Keyboard::Right || event.key.code == sf::Keyboard::Down ||
             event.key.code == sf::Keyboard::E) &&
            !localReady_) {
            sendAction(PlayerAction::NextLevel);
            return;
        }
        if (event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num8 && !localReady_) {
            const uint8_t level = static_cast<uint8_t>(event.key.code - sf::Keyboard::Num1);
            trySelectLevel(level);
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && !localReady_) {
            const LevelCatalog& catalog = LevelCatalog::instance();
            const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
            const uint8_t globalSelected = catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, playerCount);
            if (isLevelUnlocked(renderWorld_.unlockedMask, globalSelected)) {
                localReady_ = true;
                sendAction(PlayerAction::Ready);
            }
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && localReady_) {
            localReady_ = false;
            sendAction(PlayerAction::ReturnToLobby);
            return;
        }
    }

    if (renderWorld_.phase == GamePhase::Victory) {
        if (event.key.code == sf::Keyboard::N && renderWorld_.levelIndex + 1 < renderWorld_.levelCount &&
            isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(renderWorld_.levelIndex + 1))) {
            localReady_ = false;
            sendAction(PlayerAction::NextLevel);
            return;
        }
        if (event.key.code == sf::Keyboard::R) {
            localReady_ = false;
            sendAction(PlayerAction::Restart);
        }
        return;
    }

    if (renderWorld_.phase == GamePhase::GameOver && event.key.code == sf::Keyboard::R) {
        localReady_ = false;
        sendAction(PlayerAction::Restart);
    }
}

InputFlags GameClient::readLocalInput() const {
    InputFlags input = InputFlags::None;

    if (role_ == PlayerRole::Fire) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
            input = input | InputFlags::Left;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
            input = input | InputFlags::Right;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
            input = input | InputFlags::Jump;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
            input = input | InputFlags::Down;
        }
    } else if (role_ == PlayerRole::Water) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
            input = input | InputFlags::Left;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
            input = input | InputFlags::Right;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
            input = input | InputFlags::Jump;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
            input = input | InputFlags::Down;
        }
    } else if (role_ == PlayerRole::Poison) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::J)) {
            input = input | InputFlags::Left;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::L)) {
            input = input | InputFlags::Right;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::I)) {
            input = input | InputFlags::Jump;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::K)) {
            input = input | InputFlags::Down;
        }
    }

    return input;
}

void GameClient::render() {
    if (tiledMap_.ready() && connected_ && renderWorld_.phase != GamePhase::Lobby) {
        window_.clear(sf::Color(216, 189, 155));
    } else {
        window_.clear(sf::Color(12, 18, 14));
    }

    if (!connected_) {
        renderTitleScreen();
        if (clientScreen_ == ClientScreen::Help) {
            renderHelpOverlay();
        } else if (clientScreen_ == ClientScreen::Credits) {
            renderCreditsOverlay();
        } else if (clientScreen_ == ClientScreen::JoinRoom) {
            renderJoinRoomScreen();
        } else if (clientScreen_ == ClientScreen::Connecting) {
            drawConnectingScreen(window_);
        }
    } else if (connected_ && renderWorld_.phase == GamePhase::Lobby) {
        if (renderWorld_.lobbyStep == 0) {
            renderRoomScreen();
        } else {
            renderLobbyScreen();
        }
    } else {
        renderGameScreen();
    }

    window_.display();
}

void GameClient::renderTitleSpotlight() {
    sf::CircleShape beam(260.0f);
    beam.setOrigin(260.0f, 0.0f);
    beam.setPosition(kTitleCenterX, 0.0f);
    beam.setFillColor(sf::Color(255, 255, 220, 28));
    window_.draw(beam);

    sf::CircleShape glow(180.0f);
    glow.setOrigin(180.0f, 0.0f);
    glow.setPosition(kTitleCenterX, 20.0f);
    glow.setFillColor(sf::Color(255, 255, 240, 42));
    window_.draw(glow);
}

void GameClient::renderTitleCharacters() {
    if (!assets_.ready()) {
        return;
    }

    const sf::Texture& waterTex = assets_.waterGirl();
    sf::Sprite waterGirl(waterTex);
    const float waterScale = 3.2f;
    waterGirl.setScale(waterScale, waterScale);
    waterGirl.setPosition(48.0f, static_cast<float>(LOBBY_WINDOW_HEIGHT) - waterTex.getSize().y * waterScale - 36.0f);
    window_.draw(waterGirl);

    const sf::Texture& fireTex = assets_.fireBoy();
    sf::Sprite fireBoy(fireTex);
    const float fireScale = 3.2f;
    fireBoy.setScale(-fireScale, fireScale);
    fireBoy.setPosition(static_cast<float>(LOBBY_WINDOW_WIDTH) - 48.0f,
                        static_cast<float>(LOBBY_WINDOW_HEIGHT) - fireTex.getSize().y * fireScale - 36.0f);
    window_.draw(fireBoy);
}

void GameClient::renderTitleScreen() {
    if (assets_.ready()) {
        drawBackgroundSprite(window_, assets_.lobbyBackground());
    } else {
        sf::RectangleShape fallback({static_cast<float>(LOBBY_WINDOW_WIDTH), static_cast<float>(LOBBY_WINDOW_HEIGHT)});
        fallback.setFillColor(sf::Color(28, 42, 34));
        window_.draw(fallback);
    }

    renderTitleSpotlight();
    renderTitleCharacters();

    ui_.drawOutlinedCenteredText(window_, text::fireBoy(), kTitleCenterX - 118.0f, 72.0f, 46, sf::Color(255, 90, 40),
                                 sf::Color(120, 30, 10), 3.0f);
    ui_.drawOutlinedCenteredText(window_, "&", kTitleCenterX, 72.0f, 40, sf::Color(255, 230, 80),
                                 sf::Color(100, 70, 10), 3.0f);
    ui_.drawOutlinedCenteredText(window_, text::waterGirl(), kTitleCenterX + 118.0f, 72.0f, 46, sf::Color(80, 180, 255),
                                 sf::Color(20, 60, 120), 3.0f);
    ui_.drawOutlinedCenteredText(window_, text::venture(), kTitleCenterX, 132.0f, 54, sf::Color(255, 230, 70),
                                 sf::Color(100, 70, 10), 3.5f);
    ui_.drawOutlinedCenteredText(window_, text::forestTemple(), kTitleCenterX, 188.0f, 58, sf::Color(90, 210, 90),
                                 sf::Color(30, 90, 30), 4.0f);

    ui_.drawOutlinedCenteredText(window_, "Forest Temple Online", kTitleCenterX, 248.0f, 18, sf::Color(210, 220, 200),
                                 sf::Color(40, 50, 40), 1.5f);

    const auto& menuItems = text::titleMenuItems();
    const int activeIndex = titleHoverIndex_ >= 0 ? titleHoverIndex_ : titleMenuIndex_;
    for (std::size_t i = 0; i < menuItems.size(); ++i) {
        const float y = kMenuStartY + static_cast<float>(i) * kMenuItemStep;
        ui_.drawTitleMenuItem(window_, menuItems[i], kTitleCenterX, y, 34, static_cast<int>(i) == activeIndex);
    }

    const std::string roleLine = text::currentRolePrefix() + roleChineseName() + "     " + text::serverPrefix() + host_;
    ui_.drawCenteredText(window_, roleLine, kTitleCenterX, 548.0f, 18, sf::Color(220, 230, 210));
    ui_.drawCenteredText(window_, text::titleControlsHint(), kTitleCenterX, 578.0f, 15, sf::Color(180, 190, 170));
    ui_.drawCenteredText(window_, "WHUT SEPT 2026  Forest Temple Co-op", kTitleCenterX, 610.0f, 14,
                         sf::Color(140, 150, 130));
}

void GameClient::renderHelpOverlay() {
    sf::RectangleShape dim({static_cast<float>(LOBBY_WINDOW_WIDTH), static_cast<float>(LOBBY_WINDOW_HEIGHT)});
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window_.draw(dim);

    const sf::FloatRect panel{172.0f, 96.0f, 680.0f, 448.0f};
    ui_.drawPanel(window_, panel, sf::Color(24, 34, 28), 235.0f);
    ui_.drawOutlinedCenteredText(window_, text::helpTitle(), kTitleCenterX, panel.top + 24.0f, 36,
                                 sf::Color(255, 230, 100), sf::Color(80, 50, 10), 3.0f);

    const float textX = panel.left + 36.0f;
    float y = panel.top + 88.0f;
    for (const std::string& line : text::helpLines()) {
        if (line.empty()) {
            y += 18.0f;
            continue;
        }
        ui_.drawText(window_, line, textX, y, 20, sf::Color(230, 235, 220));
        y += 34.0f;
    }

    ui_.drawCenteredText(window_, text::backToTitleHint(), kTitleCenterX, panel.top + panel.height - 36.0f, 18,
                         sf::Color(255, 220, 120));
}

void GameClient::renderCreditsOverlay() {
    sf::RectangleShape dim({static_cast<float>(LOBBY_WINDOW_WIDTH), static_cast<float>(LOBBY_WINDOW_HEIGHT)});
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window_.draw(dim);

    const sf::FloatRect panel{192.0f, 110.0f, 640.0f, 420.0f};
    ui_.drawPanel(window_, panel, sf::Color(24, 34, 28), 235.0f);
    ui_.drawOutlinedCenteredText(window_, text::creditsTitle(), kTitleCenterX, panel.top + 24.0f, 36,
                                 sf::Color(255, 230, 100), sf::Color(80, 50, 10), 3.0f);

    const float textX = panel.left + 40.0f;
    float y = panel.top + 96.0f;
    for (const std::string& line : text::creditLines()) {
        if (line.empty()) {
            y += 16.0f;
            continue;
        }
        ui_.drawText(window_, line, textX, y, 20, sf::Color(230, 235, 220));
        y += 36.0f;
    }

    ui_.drawCenteredText(window_, text::backToTitleHint(), kTitleCenterX, panel.top + panel.height - 36.0f, 18,
                         sf::Color(255, 220, 120));
}

const char* GameClient::roleChineseName() const {
    switch (preferredRole_) {
        case PlayerRole::Fire:
            return "火娃";
        case PlayerRole::Water:
            return "冰娃";
        case PlayerRole::Poison:
            return "毒娃";
        default:
            return "未知";
    }
}

void GameClient::drawBackgroundSprite(sf::RenderWindow& window, const sf::Texture& texture) const {
    const sf::Vector2u winSize = window.getSize();
    if (winSize.x == 0 || winSize.y == 0 || texture.getSize().x == 0) {
        return;
    }

    sf::Sprite sprite(texture);
    const float scaleX = static_cast<float>(winSize.x) / static_cast<float>(texture.getSize().x);
    const float scaleY = static_cast<float>(winSize.y) / static_cast<float>(texture.getSize().y);
    const float scale = std::max(scaleX, scaleY);
    sprite.setScale(scale, scale);
    sprite.setPosition((static_cast<float>(winSize.x) - static_cast<float>(texture.getSize().x) * scale) / 2.0f,
                       (static_cast<float>(winSize.y) - static_cast<float>(texture.getSize().y) * scale) / 2.0f);
    window.draw(sprite);
}

bool GameClient::localWaitingReady() const {
    // waitingReadyMask：等待室准备状态（与选关 readyMask 分开）
    return (renderWorld_.waitingReadyMask & (1u << slot_)) != 0;
}

bool GameClient::allPlayersWaitingReady() const {
    if (renderWorld_.connectedCount == 0) {
        return false;
    }
    uint8_t readyCount = 0;
    for (uint8_t i = 0; i < renderWorld_.connectedCount; ++i) {
        if ((renderWorld_.waitingReadyMask & (1u << i)) != 0) {
            ++readyCount;
        }
    }
    return readyCount >= renderWorld_.connectedCount;
}

void GameClient::trySelectLevel(uint8_t index) {
    if (localReady_ || index >= renderWorld_.levelCount) {
        return;
    }
    sendAction(PlayerAction::SelectLevel, index);
}

int GameClient::levelNodeAtPosition(float x, float y) const {
    const sf::FloatRect viewport = levelMapViewportRect();
    if (!viewport.contains({x, y})) {
        return -1;
    }

    const float localX = x - viewport.left;
    const float localY = y - viewport.top + levelMapScrollY_;
    const sf::Vector2f local(localX, localY);

    for (uint8_t i = 0; i < renderWorld_.levelCount; ++i) {
        if (levelNodeLocalHitArea(i, renderWorld_.levelCount).contains(local)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void GameClient::handleLobbyMouseWheel(const sf::Event& event) {
    const sf::FloatRect viewport = levelMapViewportRect();
    const sf::Vector2f mouse(static_cast<float>(event.mouseWheelScroll.x),
                             static_cast<float>(event.mouseWheelScroll.y));
    if (!viewport.contains(mouse)) {
        return;
    }

    levelMapScrollY_ = clampLevelMapScroll(levelMapScrollY_ - event.mouseWheelScroll.delta * kMapScrollStep);
}

void GameClient::handleLobbyMouseClick(const sf::Event& event) {
    if (localReady_ || event.mouseButton.button != sf::Mouse::Left) {
        return;
    }
    const int node =
        levelNodeAtPosition(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
    if (node >= 0) {
        trySelectLevel(static_cast<uint8_t>(node));
    }
}

void GameClient::drawLevelPath(uint8_t fromIndex, uint8_t toIndex, uint8_t levelCount, bool unlocked) {
    const sf::Vector2f fromCenter = levelNodeLocalCenter(fromIndex);
    const sf::Vector2f toCenter = levelNodeLocalCenter(toIndex);
    const sf::Vector2f delta = toCenter - fromCenter;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 1.0f) {
        return;
    }

    const sf::Vector2f dir = delta / length;
    const float fromRadius = levelNodeRadius(fromIndex, levelCount);
    const float toRadius = levelNodeRadius(toIndex, levelCount);
    const sf::Vector2f from = fromCenter + dir * fromRadius * 0.85f;
    const sf::Vector2f to = toCenter - dir * toRadius * 0.85f;
    const sf::Vector2f segment = to - from;
    const float segmentLength = std::sqrt(segment.x * segment.x + segment.y * segment.y);
    if (segmentLength <= 1.0f) {
        return;
    }

    sf::RectangleShape line({segmentLength, unlocked ? 7.0f : 5.0f});
    line.setOrigin(0.0f, line.getSize().y / 2.0f);
    line.setPosition(from);
    line.setRotation(std::atan2(segment.y, segment.x) * 180.0f / 3.14159265f);
    line.setFillColor(unlocked ? sf::Color(170, 130, 72, 220) : sf::Color(70, 62, 48, 160));
    window_.draw(line);
}

void GameClient::drawLevelNode(uint8_t index, uint8_t levelCount, bool selected, bool unlocked, bool completed) {
    const sf::Vector2f center = levelNodeLocalCenter(index);
    const bool isBoss = isFinalLevelNode(index, levelCount);
    const float radius = levelNodeRadius(index, levelCount);

    if (selected) {
        const float glowPad = isBoss ? 22.0f : 14.0f;
        sf::CircleShape glow(radius + glowPad);
        glow.setOrigin(radius + glowPad, radius + glowPad);
        glow.setPosition(center);
        glow.setFillColor(isBoss ? sf::Color(255, 180, 60, 90) : sf::Color(255, 220, 80, 75));
        glow.setOutlineThickness(isBoss ? 4.0f : 3.0f);
        glow.setOutlineColor(isBoss ? sf::Color(255, 210, 80, 210) : sf::Color(255, 240, 120, 190));
        window_.draw(glow);
    }

    if (assets_.hasMapIcons()) {
        const sf::Texture& texture =
            isBoss ? assets_.mapBossIcon(unlocked, completed) : assets_.mapLevelIcon(index, unlocked, completed);
        sf::Sprite sprite(texture);
        const sf::Vector2u texSize = texture.getSize();
        const float targetSize = radius * 2.05f;
        const float scale = targetSize / static_cast<float>(std::max(texSize.x, texSize.y));
        sprite.setScale(scale, scale);
        sprite.setOrigin(static_cast<float>(texSize.x) / 2.0f, static_cast<float>(texSize.y) / 2.0f);
        sprite.setPosition(center);
        window_.draw(sprite);
        return;
    }

    sf::CircleShape body(radius);
    body.setOrigin(radius, radius);
    body.setPosition(center);
    body.setFillColor(unlocked ? sf::Color(150, 158, 168) : sf::Color(58, 54, 50));
    body.setOutlineColor(unlocked ? sf::Color(95, 100, 110) : sf::Color(35, 32, 28));
    body.setOutlineThickness(selected ? 3.0f : 2.0f);
    window_.draw(body);

    if (!unlocked) {
        ui_.drawCenteredText(window_, "?", center.x, center.y - 14.0f, 24, sf::Color(110, 105, 100));
    } else {
        ui_.drawCenteredText(window_, isBoss ? "终" : std::to_string(index + 1), center.x, center.y - 14.0f, 22,
                             sf::Color(35, 40, 48));
    }
}

void GameClient::drawLevelProgressMap() {
    const sf::FloatRect mapArea = levelMapPanelRect();
    const sf::FloatRect viewport = levelMapViewportRect();

    sf::RectangleShape soil({mapArea.width, mapArea.height});
    soil.setPosition(mapArea.left, mapArea.top);
    soil.setFillColor(sf::Color(92, 68, 48));
    soil.setOutlineThickness(3.0f);
    soil.setOutlineColor(sf::Color(55, 40, 28));
    window_.draw(soil);

    sf::RectangleShape inner({mapArea.width - 24.0f, mapArea.height - 24.0f});
    inner.setPosition(mapArea.left + 12.0f, mapArea.top + 12.0f);
    inner.setFillColor(sf::Color(108, 82, 58, 220));
    window_.draw(inner);

    ui_.drawText(window_, text::lobbyMapTitle(), mapArea.left + 16.0f, mapArea.top + 10.0f, 22,
                 sf::Color(255, 235, 190));

    sf::RectangleShape viewportBg({viewport.width, viewport.height});
    viewportBg.setPosition(viewport.left, viewport.top);
    viewportBg.setFillColor(sf::Color(88, 64, 44));
    viewportBg.setOutlineThickness(2.0f);
    viewportBg.setOutlineColor(sf::Color(60, 44, 30));
    window_.draw(viewportBg);

    if (levelMapMaxScroll() > 0.0f) {
        ui_.drawText(window_, text::lobbyScrollHint(), mapArea.left + mapArea.width - 168.0f, mapArea.top + 12.0f, 13,
                     sf::Color(220, 200, 160));
    }

    const sf::Vector2u winSize = window_.getSize();
    const sf::View previousView = window_.getView();
    sf::View contentView(sf::FloatRect(0.0f, levelMapScrollY_, viewport.width, viewport.height));
    contentView.setViewport(
        sf::FloatRect(viewport.left / static_cast<float>(winSize.x), viewport.top / static_cast<float>(winSize.y),
                      viewport.width / static_cast<float>(winSize.x), viewport.height / static_cast<float>(winSize.y)));
    window_.setView(contentView);

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
    const uint8_t levelCount = renderWorld_.levelCount;
    for (uint8_t i = 0; i + 1 < levelCount; ++i) {
        const uint8_t nextGlobal = catalog.filteredIndexToGlobalIndex(static_cast<uint8_t>(i + 1), playerCount);
        const bool pathUnlocked = isLevelUnlocked(renderWorld_.unlockedMask, nextGlobal);
        drawLevelPath(i, static_cast<uint8_t>(i + 1), levelCount, pathUnlocked);
    }

    for (uint8_t i = 0; i < levelCount; ++i) {
        const uint8_t globalIndex = catalog.filteredIndexToGlobalIndex(i, playerCount);
        const bool unlocked = isLevelUnlocked(renderWorld_.unlockedMask, globalIndex);
        const bool completed = isLevelCompleted(renderWorld_.completedMask, globalIndex);
        const bool selected = i == renderWorld_.levelIndex;
        const bool hovered = static_cast<int>(i) == lobbyHoverNode_;
        drawLevelNode(i, levelCount, selected || hovered, unlocked, completed);
    }

    window_.setView(previousView);

    if (levelMapMaxScroll() > 0.0f) {
        const float trackX = mapArea.left + mapArea.width - 10.0f;
        const float trackTop = viewport.top + 4.0f;
        const float trackHeight = viewport.height - 8.0f;
        sf::RectangleShape track({4.0f, trackHeight});
        track.setPosition(trackX, trackTop);
        track.setFillColor(sf::Color(50, 38, 28, 180));
        window_.draw(track);

        const float thumbHeight = std::max(24.0f, trackHeight * (viewport.height / levelMapContentHeight()));
        const float scrollRatio = levelMapScrollY_ / levelMapMaxScroll();
        sf::RectangleShape thumb({4.0f, thumbHeight});
        thumb.setPosition(trackX, trackTop + (trackHeight - thumbHeight) * scrollRatio);
        thumb.setFillColor(sf::Color(220, 190, 120, 220));
        window_.draw(thumb);
    }
}

void GameClient::renderJoinRoomScreen() {
    roomAnimTimer_ += 0.016f;
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    if (assets_.ready()) {
        drawBackgroundSprite(window_, assets_.lobbyBackground());
    } else {
        sf::RectangleShape fallback({w, h});
        fallback.setFillColor(sf::Color(12, 20, 30));
        window_.draw(fallback);
    }

    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 120));
    window_.draw(dim);

    const float panelW = 620.0f;
    const float panelH = 480.0f;
    const float panelX = (w - panelW) / 2.0f;
    const float panelY = 80.0f;
    const sf::FloatRect dialog{panelX, panelY, panelW, panelH};
    ui_.drawPanel(window_, dialog, sf::Color(20, 28, 44, 240), 240.0f);
    ui_.drawOutlinedCenteredText(window_, "加入房间 (局域网自动发现)", w / 2.0f, dialog.top + 16.0f, 26,
                                 sf::Color(255, 230, 100), sf::Color(80, 50, 10), 2.5f);

    const float listX = panelX + 20.0f;
    const float listY = dialog.top + 56.0f;
    const float listW = panelW - 40.0f;
    const float listH = 240.0f;

    sf::RectangleShape listBg({listW, listH});
    listBg.setPosition(listX, listY);
    listBg.setFillColor(sf::Color(12, 16, 28, 230));
    listBg.setOutlineThickness(1.0f);
    listBg.setOutlineColor(sf::Color(60, 80, 120));
    window_.draw(listBg);

    const float roomRowH = 48.0f;
    if (discoveryActive_) {
        const int dots = (static_cast<int>(roomAnimTimer_ * 3.0f) % 3) + 1;
        std::string scanMsg = "正在扫描局域网";
        for (int i = 0; i < dots; ++i) {
            scanMsg += ".";
        }
        ui_.drawCenteredText(window_, scanMsg, w / 2.0f, listY + listH / 2.0f - 14.0f, 22, sf::Color(180, 200, 255));
        ui_.drawCenteredText(window_, "自动发现同一网络下的游戏房间", w / 2.0f, listY + listH / 2.0f + 18.0f, 14,
                             sf::Color(120, 140, 180));
    } else if (discoveredRooms_.empty()) {
        ui_.drawCenteredText(window_, "未发现局域网房间", w / 2.0f, listY + listH / 2.0f - 14.0f, 20,
                             sf::Color(180, 160, 120));
        ui_.drawCenteredText(window_, "请确认主机在同一网络下已创建房间，或手动输入房间号", w / 2.0f,
                             listY + listH / 2.0f + 18.0f, 14, sf::Color(140, 150, 170));
    } else {
        ui_.drawText(window_, "发现 " + std::to_string(discoveredRooms_.size()) + " 个房间  [↑↓] 选择  [Enter] 加入",
                     listX + 8.0f, listY + 6.0f, 14, sf::Color(160, 190, 230));

        const int visibleStart = std::max(0, selectedDiscoveredRoom_ - 3);
        const int maxVisible = static_cast<int>(listH / roomRowH) - 1;
        const int visibleEnd = std::min(static_cast<int>(discoveredRooms_.size()), visibleStart + maxVisible);

        for (int i = visibleStart; i < visibleEnd; ++i) {
            const float rowY2 = listY + 26.0f + (i - visibleStart) * roomRowH;
            const bool sel = i == selectedDiscoveredRoom_;

            sf::RectangleShape row({listW - 16.0f, roomRowH - 6.0f});
            row.setPosition(listX + 8.0f, rowY2);
            row.setFillColor(sel ? sf::Color(55, 95, 150) : sf::Color(30, 38, 55));
            row.setOutlineThickness(sel ? 2.0f : 1.0f);
            row.setOutlineColor(sel ? sf::Color(140, 200, 255) : sf::Color(60, 70, 100));
            window_.draw(row);

            const auto& room = discoveredRooms_[i];
            const std::string label = "房间 " + room.roomCode + "  |  " + room.levelName + "  |  玩家 " +
                                      std::to_string(room.playerCount) + "/" + std::to_string(room.maxPlayers) +
                                      "  |  " + room.address;
            ui_.drawText(window_, label, listX + 18.0f, rowY2 + 6.0f, 16,
                         sel ? sf::Color::White : sf::Color(200, 210, 220));
        }
    }

    const float dividerY = listY + listH + 12.0f;
    ui_.drawCenteredText(window_, "—— 或手动输入房间号 ——", w / 2.0f, dividerY, 14, sf::Color(140, 155, 180));

    const float boxW = 260.0f;
    const float boxH = 46.0f;
    const float boxX = w / 2.0f - boxW / 2.0f;
    const float boxY = dividerY + 26.0f;
    sf::RectangleShape inputBox({boxW, boxH});
    inputBox.setPosition(boxX, boxY);
    inputBox.setFillColor(sf::Color(12, 16, 28));
    inputBox.setOutlineThickness(2.0f);
    inputBox.setOutlineColor(sf::Color(100, 160, 255));
    window_.draw(inputBox);

    std::string displayCode;
    for (int i = 0; i < 6; ++i) {
        if (i < static_cast<int>(typedRoomCode_.size())) {
            displayCode.push_back(typedRoomCode_[static_cast<size_t>(i)]);
        } else {
            displayCode.push_back('_');
        }
        if (i < 5) {
            displayCode.push_back(' ');
        }
    }
    ui_.drawOutlinedCenteredText(window_, displayCode, w / 2.0f, boxY + 8.0f, 26, sf::Color(255, 255, 255),
                                 sf::Color(40, 40, 80), 2.0f);

    ui_.drawCenteredText(window_, "[数字键] 输入  [F5] 重新扫描  [Enter] 确认  [Esc] 返回", w / 2.0f,
                         dialog.top + panelH - 36.0f, 15, sf::Color(160, 170, 190));
}

void GameClient::renderRoomScreen() {
    roomAnimTimer_ += 0.016f;
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    if (assets_.ready()) {
        drawBackgroundSprite(window_, assets_.lobbyBackground());
    } else {
        sf::RectangleShape fallback({w, h});
        fallback.setFillColor(sf::Color(12, 20, 30));
        window_.draw(fallback);
    }

    // Top header bar
    ui_.drawPanel(window_, {0.0f, 0.0f, w, 68.0f}, sf::Color(16, 22, 40, 230), 230.0f);
    ui_.drawOutlinedCenteredText(window_, "Fire & Ice 森林神殿", w / 2.0f, 8.0f, 28, sf::Color(255, 230, 100),
                                 sf::Color(80, 50, 10), 2.5f);
    ui_.drawCenteredText(window_, text::roomWaitingTitle(), w / 2.0f, 42.0f, 16, sf::Color(180, 195, 220));

    // Room code display (prominent)
    const std::string roomCodeStr =
        renderWorld_.roomCode[0] ? std::string("房间号: ") + std::string(renderWorld_.roomCode) : "房间号: ------";
    ui_.drawOutlinedCenteredText(window_, roomCodeStr, w - 180.0f, 8.0f, 16, sf::Color(100, 255, 140),
                                 sf::Color(20, 60, 20), 2.0f);

    std::string ipStr;
    if (isHosting_ && !localIp_.empty()) {
        ipStr = "主机IP: " + localIp_ + ":" + std::to_string(SERVER_PORT);
    } else {
        ipStr = "服务器: " + host_ + ":" + std::to_string(SERVER_PORT);
    }
    ui_.drawCenteredText(window_, ipStr, w - 180.0f, 30.0f, 12, sf::Color(180, 200, 255));

    ui_.drawCenteredText(
        window_,
        std::string("在线: ") + std::to_string(renderWorld_.connectedCount) + "/" + std::to_string(MAX_PLAYERS),
        w - 180.0f, 48.0f, 13, sf::Color(160, 200, 140));

    // --- Player panels (always reserve 3 slots) ---
    const float totalPanelW = kRoomPanelW * 3.0f + kRoomPanelGap * 2.0f;
    const float panelStartX = (w - totalPanelW) / 2.0f;
    renderRoomPlayerPanel(panelStartX, kRoomPanelY, kRoomPanelW, kRoomPanelH, 0, PlayerRole::Fire);
    renderRoomPlayerPanel(panelStartX + kRoomPanelW + kRoomPanelGap, kRoomPanelY, kRoomPanelW, kRoomPanelH, 1,
                          PlayerRole::Water);
    renderRoomPlayerPanel(panelStartX + (kRoomPanelW + kRoomPanelGap) * 2.0f, kRoomPanelY, kRoomPanelW, kRoomPanelH, 2,
                          PlayerRole::Poison);

    // --- Middle status bar ---
    const float statusY = kRoomPanelY + kRoomPanelH + 14.0f;
    const float statusH = roomBottomBarY() - statusY - 8.0f;
    ui_.drawPanel(window_, {32.0f, statusY, w - 64.0f, statusH}, sf::Color(20, 26, 44, 220), 220.0f);

    // Players connected status
    const bool fireConnected = renderWorld_.connectedCount >= 1;
    const bool waterConnected = renderWorld_.connectedCount >= 2;
    const bool poisonConnected = renderWorld_.connectedCount >= 3;
    const bool fireReady = (renderWorld_.waitingReadyMask & 0x01) != 0;
    const bool waterReady = (renderWorld_.waitingReadyMask & 0x02) != 0;
    const bool poisonReady = (renderWorld_.waitingReadyMask & 0x04) != 0;

    ui_.drawText(window_, "玩家状态", 52.0f, statusY + 12.0f, 18, sf::Color(200, 210, 230));

    const std::string fireName = renderWorld_.playerNames[0][0] ? renderWorld_.playerNames[0] : "---";
    const std::string waterName = renderWorld_.playerNames[1][0] ? renderWorld_.playerNames[1] : "---";
    const std::string poisonName = renderWorld_.playerNames[2][0] ? renderWorld_.playerNames[2] : "---";

    auto drawStatus = [&](float x, const std::string& label, const std::string& name, bool connected, bool ready,
                          sf::Color roleColor) {
        sf::Color color = connected ? (ready ? roleColor : sf::Color(200, 180, 160)) : sf::Color(100, 100, 100);
        ui_.drawText(window_, label + ": " + name, x, statusY + 40.0f, 15, color);
        const std::string state = connected ? (ready ? " [已准备]" : " [等待中]") : " [未连接]";
        sf::Color stateCol =
            connected ? (ready ? sf::Color(120, 255, 140) : sf::Color(180, 180, 180)) : sf::Color(140, 80, 80);
        ui_.drawText(window_, state, x + 120.0f, statusY + 40.0f, 14, stateCol);
    };

    drawStatus(52.0f, "火娃", fireName, fireConnected, fireReady, sf::Color(255, 150, 80));
    drawStatus(300.0f, "冰娃", waterName, waterConnected, waterReady, sf::Color(100, 180, 255));
    drawStatus(548.0f, "毒娃", poisonName, poisonConnected, poisonReady, sf::Color(80, 200, 80));

    ui_.drawText(window_, text::roomWaitingHint(), 52.0f, statusY + 72.0f, 14, sf::Color(140, 150, 170));

    const bool waitingReady = localWaitingReady();
    const bool canProceed = allPlayersWaitingReady();
    const float waitMsgY = roomBottomBarY() - 30.0f;

    if (renderWorld_.connectedCount < MAX_PLAYERS) {
        const std::string waitMsg = "等待其他玩家加入房间... (当前 " + std::to_string(renderWorld_.connectedCount) +
                                    "/" + std::to_string(MAX_PLAYERS) + ")";
        ui_.drawOutlinedCenteredText(window_, waitMsg, w / 2.0f, waitMsgY, 17, sf::Color(255, 220, 100),
                                     sf::Color(80, 50, 10), 2.0f);
        const int dotCount = (static_cast<int>(roomAnimTimer_ * 2.0f) % 3) + 1;
        std::string dots;
        for (int i = 0; i < dotCount; ++i) {
            dots += ".";
        }
        ui_.drawText(window_, dots, w / 2.0f + 180.0f, waitMsgY + 4.0f, 17, sf::Color(255, 220, 100));
    } else if (canProceed) {
        ui_.drawOutlinedCenteredText(window_, "全员已准备，请点击下一步选择关卡", w / 2.0f, waitMsgY, 17,
                                     sf::Color(255, 220, 100), sf::Color(80, 50, 10), 2.0f);
    }

    // --- Bottom action bar ---
    const float bottomBarY = roomBottomBarY();
    ui_.drawPanel(window_, {0.0f, bottomBarY, w, kRoomBottomBarH}, sf::Color(16, 22, 40, 240), 240.0f);

    const auto buttons = roomActionButtonAreas();
    const sf::FloatRect& readyArea = buttons[0];
    const sf::FloatRect& nextArea = buttons[1];
    const sf::FloatRect& leaveArea = buttons[2];

    if (waitingReady) {
        ui_.drawButton(window_, readyArea, text::roomCancelReady(), true, sf::Color(50, 160, 90));
    } else {
        ui_.drawButton(window_, readyArea, text::roomWaitingReady(), true, sf::Color(60, 130, 210));
    }

    ui_.drawButton(window_, nextArea, text::roomNextStep(), canProceed,
                   canProceed ? sf::Color(210, 150, 50) : sf::Color(80, 80, 90));
    ui_.drawButton(window_, leaveArea, "离开房间", true, sf::Color(120, 60, 50));
}

void GameClient::renderRoomPlayerPanel(float panelX, float panelY, float panelW, float panelH, int playerSlot,
                                       PlayerRole expectedRole) {
    const bool isConnected = renderWorld_.connectedCount > static_cast<uint8_t>(playerSlot);
    const bool isReady = (renderWorld_.waitingReadyMask & (1u << playerSlot)) != 0;
    const bool isSelf = (playerSlot == static_cast<int>(slot_)) && connected_;

    // Panel background
    sf::Color panelBase;
    sf::Color panelHighlight;
    if (expectedRole == PlayerRole::Fire) {
        panelBase = sf::Color(40, 18, 14, 220);
        panelHighlight = sf::Color(60, 30, 22, 230);
    } else if (expectedRole == PlayerRole::Water) {
        panelBase = sf::Color(14, 22, 40, 220);
        panelHighlight = sf::Color(22, 32, 55, 230);
    } else {
        panelBase = sf::Color(14, 36, 16, 220);
        panelHighlight = sf::Color(22, 52, 28, 230);
    }
    ui_.drawPanel(window_, {panelX, panelY, panelW, panelH}, isConnected ? panelHighlight : panelBase,
                  isConnected ? 230.0f : 180.0f);

    // Role title
    std::string roleTitle;
    sf::Color roleColor;
    std::string roleSub;
    if (expectedRole == PlayerRole::Fire) {
        roleTitle = "火娃";
        roleColor = sf::Color(255, 120, 60);
        roleSub = "WASD 操控";
    } else if (expectedRole == PlayerRole::Water) {
        roleTitle = "冰娃";
        roleColor = sf::Color(80, 170, 255);
        roleSub = "方向键 操控";
    } else {
        roleTitle = "毒娃";
        roleColor = sf::Color(80, 200, 80);
        roleSub = "IJKL 操控";
    }
    ui_.drawOutlinedCenteredText(window_, roleTitle, panelX + panelW / 2.0f, panelY + 12.0f, 24,
                                 isConnected ? roleColor : sf::Color(120, 120, 130), sf::Color(30, 20, 20), 2.0f);

    // Character portrait
    if (assets_.ready()) {
        const sf::Texture* charTex = nullptr;
        if (expectedRole == PlayerRole::Fire) {
            charTex = &assets_.fireBoy();
        } else if (expectedRole == PlayerRole::Water) {
            charTex = &assets_.waterGirl();
        }
        if (charTex && charTex->getSize().y > 0) {
            sf::Sprite portrait(*charTex);
            const float targetH = 100.0f;
            const float scale = targetH / static_cast<float>(charTex->getSize().y);
            const float drawW = static_cast<float>(charTex->getSize().x) * scale;
            portrait.setScale(scale, scale);
            portrait.setPosition(panelX + (panelW - drawW) / 2.0f, panelY + 44.0f);

            if (!isConnected) {
                portrait.setColor(sf::Color(80, 80, 80, 140));
            } else if (isSelf) {
                portrait.setColor(sf::Color(255, 255, 255, 255));
            }
            window_.draw(portrait);
        } else {
            sf::RectangleShape placeholder({56.0f, 100.0f});
            placeholder.setPosition(panelX + (panelW - 56.0f) / 2.0f, panelY + 44.0f);
            placeholder.setFillColor(isConnected ? roleColor : sf::Color(80, 80, 80));
            window_.draw(placeholder);
        }

        if (isSelf) {
            sf::CircleShape indicator(6.0f);
            indicator.setPosition(panelX + panelW / 2.0f + 40.0f, panelY + 50.0f);
            indicator.setFillColor(sf::Color(80, 255, 120));
            window_.draw(indicator);
        }
    } else {
        sf::RectangleShape placeholder({56.0f, 100.0f});
        placeholder.setPosition(panelX + (panelW - 56.0f) / 2.0f, panelY + 44.0f);
        placeholder.setFillColor(isConnected ? roleColor : sf::Color(80, 80, 80));
        window_.draw(placeholder);
    }

    // Player name
    const std::string playerName =
        renderWorld_.playerNames[playerSlot][0] ? renderWorld_.playerNames[playerSlot] : "---";
    ui_.drawCenteredText(window_, playerName, panelX + panelW / 2.0f, panelY + 152.0f, 16,
                         isConnected ? sf::Color(240, 240, 240) : sf::Color(120, 120, 120));

    ui_.drawCenteredText(window_, roleSub, panelX + panelW / 2.0f, panelY + 174.0f, 13,
                         isConnected ? sf::Color(180, 180, 190) : sf::Color(100, 100, 110));

    // Connection / Ready status
    std::string statusText;
    sf::Color statusColor;
    if (!isConnected) {
        statusText = "等待加入...";
        statusColor = sf::Color(140, 140, 140);
    } else if (isReady) {
        statusText = "已准备";
        statusColor = sf::Color(100, 255, 140);
    } else {
        statusText = "未准备";
        statusColor = sf::Color(255, 200, 100);
    }

    const float badgeW = 100.0f;
    const float badgeX = panelX + (panelW - badgeW) / 2.0f;
    const float badgeY = panelY + panelH - 44.0f;
    sf::RectangleShape badge({badgeW, 28.0f});
    badge.setPosition(badgeX, badgeY);
    sf::Color badgeFill = statusColor;
    badgeFill.a = isConnected ? 200 : 100;
    badge.setFillColor(badgeFill);
    badge.setOutlineThickness(1.5f);
    badge.setOutlineColor(sf::Color(255, 255, 255, 80));
    window_.draw(badge);
    ui_.drawCenteredText(window_, statusText, panelX + panelW / 2.0f, badgeY + 4.0f, 15, sf::Color::White);

    if (isReady) {
        ui_.drawOutlinedCenteredText(window_, "\xe2\x9c\x93", panelX + panelW / 2.0f, panelY + 16.0f, 16,
                                     sf::Color(80, 255, 120), sf::Color(20, 60, 20), 1.5f);
    }
}

void GameClient::renderLobbyScreen() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);

    const sf::FloatRect viewport = levelMapViewportRect();
    const float nodeY = levelNodeLocalCenter(renderWorld_.levelIndex).y;
    const float nodeRadius = levelNodeRadius(renderWorld_.levelIndex, renderWorld_.levelCount);
    const float nodeTop = nodeY - nodeRadius - 8.0f;
    const float nodeBottom = nodeY + nodeRadius + 8.0f;
    if (nodeTop < levelMapScrollY_) {
        levelMapScrollY_ = nodeTop;
    }
    if (nodeBottom > levelMapScrollY_ + viewport.height) {
        levelMapScrollY_ = nodeBottom - viewport.height;
    }
    levelMapScrollY_ = clampLevelMapScroll(levelMapScrollY_);

    sf::RectangleShape backdrop({w, static_cast<float>(LOBBY_WINDOW_HEIGHT)});
    backdrop.setFillColor(sf::Color(36, 28, 22));
    window_.draw(backdrop);

    ui_.drawPanel(window_, {0.0f, 0.0f, w, 72.0f}, sf::Color(48, 36, 28, 220), 220.0f);
    ui_.drawCenteredText(window_, text::forestTemple(), w / 2.0f, 12.0f, 34, sf::Color(255, 230, 170));
    ui_.drawCenteredText(window_, text::levelSelectTitle(), w / 2.0f, 48.0f, 16, sf::Color(200, 190, 170));

    drawLevelProgressMap();

    const sf::FloatRect previewArea{676.0f, 136.0f, 308.0f, 408.0f};
    ui_.drawPanel(window_, previewArea, sf::Color(28, 24, 20, 220), 220.0f);
    ui_.drawText(window_, "地图预览", previewArea.left + 12.0f, previewArea.top + 8.0f, 16, sf::Color(255, 230, 170));
    if (tiledMap_.ready()) {
        tiledMap_.drawPreview(window_, previewArea);
    } else {
        // 无 TMX 时回退为 collision 色块预览
        drawMapPreview(window_, previewArea);
    }

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
    const uint8_t globalSelected = catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, playerCount);
    const LevelInfo& selectedInfo = catalog.at(globalSelected);
    const bool selectedUnlocked = isLevelUnlocked(renderWorld_.unlockedMask, globalSelected);
    const bool selectedCompleted = isLevelCompleted(renderWorld_.completedMask, globalSelected);

    const sf::FloatRect statusPanel{24.0f, 576.0f, 700.0f, 48.0f};
    ui_.drawPanel(window_, statusPanel, sf::Color(36, 30, 24, 220), 220.0f);

    std::string levelLine = "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + selectedInfo.title;
    if (!selectedUnlocked) {
        levelLine += "  [" + text::lobbyLockedHint() + "]";
    } else if (selectedCompleted) {
        levelLine += "  [" + text::lobbyClearedHint() + "]";
    }
    levelLine += "  |  Gems: " + std::to_string(renderWorld_.totalGems);
    ui_.drawText(window_, levelLine, statusPanel.left + 20.0f, statusPanel.top + 14.0f, 18, sf::Color(255, 230, 170));

    ui_.drawText(window_, text::lobbyControlsHint(), statusPanel.left + 360.0f, statusPanel.top + 16.0f, 14,
                 sf::Color(180, 170, 150));

    const sf::FloatRect playArea{740.0f, 576.0f, 260.0f, 48.0f};
    const bool canStart = selectedUnlocked && !localReady_;
    if (assets_.hasButtons()) {
        if (!localReady_) {
            ui_.drawImageButtonWithHint(window_, playArea, assets_.playButton(),
                                        canStart ? "[Enter] 准备" : text::lobbyLockedHint(), canStart, canStart);
        } else {
            ui_.drawImageButtonWithHint(window_, playArea, assets_.playButton(), "已准备", true, true);
        }
    } else if (!localReady_) {
        ui_.drawButton(window_, playArea, canStart ? "准备游戏" : text::lobbyLockedHint(), canStart,
                       canStart ? sf::Color(60, 130, 210) : sf::Color(80, 80, 90));
    } else {
        ui_.drawButton(window_, playArea, "已准备", true, sf::Color(50, 140, 90));
    }
}

void GameClient::renderGameScreen() {
    const float mapW = static_cast<float>(map_.width()) * TILE_SIZE;
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
    const uint8_t globalIndex = catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, playerCount);
    const LevelInfo& levelInfo = catalog.at(globalIndex);
    const bool hasTiledVisual = levelInfo.visualFileName != nullptr && levelInfo.visualFileName[0] != '\0';

    if (assets_.ready() && !hasTiledVisual) {
        sf::Sprite bg(assets_.gameBackground());
        const sf::Vector2u texSize = assets_.gameBackground().getSize();
        if (texSize.x > 0 && texSize.y > 0) {
            bg.setScale(mapW / static_cast<float>(texSize.x), mapH / static_cast<float>(texSize.y));
            bg.setPosition(0.0f, 0.0f);
            window_.draw(bg);
        }
    }

    drawMap(window_);

    if (renderWorld_.phase == GamePhase::Playing || renderWorld_.phase == GamePhase::Victory ||
        renderWorld_.phase == GamePhase::GameOver) {
        drawPlayer(window_, renderWorld_.players[0]);
        drawPlayer(window_, renderWorld_.players[1]);
    }

    drawHud(window_);

    const float centerX = static_cast<float>(window_.getSize().x) / 2.0f;
    switch (renderWorld_.phase) {
        case GamePhase::Countdown:
            drawCountdownOverlay(window_, centerX);
            break;
        case GamePhase::Victory:
            drawResultOverlay(window_, centerX, true);
            break;
        case GamePhase::GameOver:
            drawResultOverlay(window_, centerX, false);
            break;
        default:
            break;
    }
}

void GameClient::drawConnectingScreen(sf::RenderWindow& window) const {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);

    ui_.drawPanel(window, {w / 2.0f - 240.0f, h / 2.0f - 70.0f, 480.0f, 140.0f}, sf::Color(20, 30, 24, 220), 220.0f);
    ui_.drawOutlinedCenteredText(window, text::connectingTitle(), w / 2.0f, h / 2.0f - 36.0f, 30,
                                 sf::Color(255, 240, 180), sf::Color(60, 40, 10), 2.5f);
    ui_.drawCenteredText(window, host_ + "  |  " + text::rolePrefixConnecting() + roleChineseName(), w / 2.0f,
                         h / 2.0f + 8.0f, 18, sf::Color(200, 210, 190));
    ui_.drawCenteredText(window, text::backToTitleHint(), w / 2.0f, h / 2.0f + 40.0f, 16, sf::Color(180, 190, 170));
}

void GameClient::drawMapPreview(sf::RenderWindow& window, const sf::FloatRect& area) const {
    const float mapW = static_cast<float>(map_.width()) * TILE_SIZE;
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;
    if (mapW <= 0.0f || mapH <= 0.0f) {
        return;
    }

    const float scale = std::min((area.width - 4.0f) / mapW, (area.height - 4.0f) / mapH);
    const float offsetX = area.left + (area.width - mapW * scale) / 2.0f;
    const float offsetY = area.top + (area.height - mapH * scale) / 2.0f;

    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const TileType type = map_.tileAt(x, y);
            if (type == TileType::Empty) {
                continue;
            }

            const float tileSize = TILE_SIZE * scale - 1.0f;
            const float px = offsetX + x * TILE_SIZE * scale;
            const float py = offsetY + y * TILE_SIZE * scale;

            if (type == TileType::Gem && assets_.ready()) {
                const sf::Texture& gemTex = ((x + y) % 2 == 0) ? assets_.gemRed() : assets_.gemBlue();
                sf::Sprite gem(gemTex);
                const float gemScale = tileSize / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y));
                gem.setScale(gemScale, gemScale);
                gem.setPosition(px + (tileSize - gemTex.getSize().x * gemScale) / 2.0f,
                                py + (tileSize - gemTex.getSize().y * gemScale) / 2.0f);
                window.draw(gem);
                continue;
            }

            sf::RectangleShape tile({tileSize, tileSize});
            tile.setPosition(px, py);
            tile.setFillColor(tileColor(type));
            window.draw(tile);
        }
    }
}

void GameClient::drawDynamicTiles(sf::RenderWindow& window) const {
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const TileType type = map_.tileAt(x, y);
            if (type == TileType::Empty) {
                continue;
            }
            if (type == TileType::FireDoor && renderWorld_.fireDoorOpen) {
                continue;
            }
            if (type == TileType::WaterDoor && renderWorld_.waterDoorOpen) {
                continue;
            }

            const float tileSize = TILE_SIZE - 1.0f;
            const float px = x * TILE_SIZE;
            const float py = y * TILE_SIZE;

            if (type == TileType::Gem && assets_.ready()) {
                const sf::Texture& gemTex = ((x + y) % 2 == 0) ? assets_.gemRed() : assets_.gemBlue();
                sf::Sprite gem(gemTex);
                const float gemScale = tileSize / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y));
                gem.setScale(gemScale, gemScale);
                gem.setPosition(px + (tileSize - gemTex.getSize().x * gemScale) / 2.0f,
                                py + (tileSize - gemTex.getSize().y * gemScale) / 2.0f);
                window.draw(gem);
                continue;
            }

            if (type == TileType::FireDoor || type == TileType::WaterDoor || type == TileType::FireExit ||
                type == TileType::WaterExit || type == TileType::Button) {
                sf::RectangleShape tile({tileSize, tileSize});
                tile.setPosition(px, py);
                tile.setFillColor(tileColor(type));
                window.draw(tile);
            }
        }
    }
}

void GameClient::drawMudParticles(sf::RenderWindow& window) const {
    for (uint8_t i = 0; i < renderWorld_.mudParticleCount; ++i) {
        const WorldState::SyncMudParticle& particle = renderWorld_.mudParticles[i];
        if (particle.active == 0) {
            continue;
        }
        sf::CircleShape mud(MUD_HITBOX * 0.5f);
        mud.setFillColor(sf::Color(50, 150, 60));
        mud.setOutlineColor(sf::Color(30, 100, 40));
        mud.setOutlineThickness(1.0f);
        mud.setOrigin(MUD_HITBOX * 0.5f, MUD_HITBOX * 0.5f);
        mud.setPosition(particle.x, particle.y);
        window.draw(mud);
    }
}

void GameClient::drawMap(sf::RenderWindow& window) const {
    if (tiledMap_.ready()) {
        const auto skipHiddenVanishing = [this](int x, int y) {
            if (map_.tileAt(x, y) != TileType::VanishingPlatform) {
                return false;
            }
            const int16_t slot = map_.vanishingSlotAt(x, y);
            if (slot < 0) {
                return false;
            }
            return isVanishingTileHidden(renderWorld_, static_cast<uint16_t>(slot));
        };
        tiledMap_.drawStatic(window, skipHiddenVanishing);
        tiledMap_.drawCollectibles(window, renderWorld_.collectedPickupsMask, renderWorld_.collectedPickupsMaskHi,
                                   renderWorld_.collectedPickupsMaskExt);
        drawMudParticles(window);
        return;
    }

    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            TileType type = map_.tileAt(x, y);
            if (type == TileType::Empty) {
                continue;
            }

            if (type == TileType::FireDoor && renderWorld_.fireDoorOpen) {
                continue;
            }
            if (type == TileType::WaterDoor && renderWorld_.waterDoorOpen) {
                continue;
            }

            const float tileSize = TILE_SIZE - 1.0f;
            const float px = x * TILE_SIZE;
            const float py = y * TILE_SIZE;

            if (type == TileType::Gem && assets_.ready()) {
                const sf::Texture& gemTex = ((x + y) % 2 == 0) ? assets_.gemRed() : assets_.gemBlue();
                sf::Sprite gem(gemTex);
                const float gemScale = tileSize / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y));
                gem.setScale(gemScale, gemScale);
                gem.setPosition(px + (tileSize - gemTex.getSize().x * gemScale) / 2.0f,
                                py + (tileSize - gemTex.getSize().y * gemScale) / 2.0f);
                window.draw(gem);
                continue;
            }

            sf::RectangleShape tile({tileSize, tileSize});
            tile.setPosition(px, py);
            tile.setFillColor(tileColor(type));
            window.draw(tile);
        }
    }
}

void GameClient::drawPlayer(sf::RenderWindow& window, const PlayerState& player) const {
    if (player.role == PlayerRole::None) {
        return;
    }

    if (assets_.ready()) {
        const sf::Texture& texture = player.role == PlayerRole::Fire ? assets_.fireBoy() : assets_.waterGirl();
        sf::Sprite sprite(texture);
        const sf::Vector2u texSize = texture.getSize();
        const float scaleX = PLAYER_WIDTH / static_cast<float>(texSize.x);
        const float scaleY = PLAYER_HEIGHT / static_cast<float>(texSize.y);
        sprite.setScale(scaleX, scaleY);
        if (player.vx < -1.0f) {
            sprite.setScale(-scaleX, scaleY);
            sprite.setPosition(player.x + PLAYER_WIDTH, player.y);
        } else {
            sprite.setPosition(player.x, player.y);
        }
        if (!player.alive) {
            sprite.setColor(sf::Color(120, 120, 120, 140));
        }
        window.draw(sprite);
        return;
    }

    sf::RectangleShape body({PLAYER_WIDTH, PLAYER_HEIGHT});
    body.setPosition(player.x, player.y);

    if (!player.alive) {
        body.setFillColor(sf::Color(80, 80, 80, 120));
    } else if (player.role == PlayerRole::Fire) {
        body.setFillColor(sf::Color(255, 90, 40));
    } else if (player.role == PlayerRole::Water) {
        body.setFillColor(sf::Color(60, 140, 255));
    } else if (player.role == PlayerRole::Poison) {
        body.setFillColor(sf::Color(34, 139, 34));
    }

    window.draw(body);
}

void GameClient::drawHud(sf::RenderWindow& window) const {
    const float hudY = static_cast<float>(map_.height()) * TILE_SIZE + 8.0f;
    const float windowW = static_cast<float>(window_.getSize().x);

    sf::RectangleShape hudBar({windowW, 72.0f});
    hudBar.setPosition(0.0f, hudY);
    hudBar.setFillColor(sf::Color(30, 30, 40));
    window.draw(hudBar);

    const int totalGems = renderWorld_.players[0].gems + renderWorld_.players[1].gems;
    ui_.drawText(window, "Gems: " + std::to_string(totalGems) + "/" + std::to_string(renderWorld_.totalGems), 16.0f,
                 hudY + 16.0f, 20, sf::Color(255, 220, 80));

    const std::string levelLine =
        "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + renderWorld_.levelName;
    ui_.drawText(window, levelLine, 180.0f, hudY + 16.0f, 18, sf::Color(220, 220, 220));

    if (renderWorld_.phase == GamePhase::Playing) {
        const char* controlText = role_ == PlayerRole::Fire ? "WASD" : "Arrows";
        ui_.drawText(window, std::string(roleDisplayName()) + " [" + controlText + "]", 500.0f, hudY + 16.0f, 18,
                     sf::Color(180, 180, 180));
        if (assets_.hasButtons()) {
            ui_.drawImageButton(window, {windowW - 168.0f, hudY + 6.0f, 152.0f, 48.0f}, assets_.menuButton(), true,
                                true);
            ui_.drawCenteredText(window, "[Esc] Lobby", windowW - 92.0f, hudY + 54.0f, 13, sf::Color(255, 200, 120));
        } else {
            ui_.drawText(window, "ESC - Back to Lobby", windowW - 220.0f, hudY + 16.0f, 18, sf::Color(255, 180, 100));
        }
    }

    if (renderWorld_.phase == GamePhase::Countdown) {
        if (assets_.hasButtons()) {
            ui_.drawImageButton(window, {windowW - 168.0f, hudY + 6.0f, 152.0f, 48.0f}, assets_.menuButton(), true,
                                true);
            ui_.drawCenteredText(window, "[Esc] Lobby", windowW - 92.0f, hudY + 54.0f, 13, sf::Color(255, 200, 120));
        } else {
            ui_.drawText(window, "ESC - Back to Lobby", windowW - 220.0f, hudY + 16.0f, 18, sf::Color(255, 180, 100));
        }
    }
}

void GameClient::drawCountdownOverlay(sf::RenderWindow& window, float centerX) const {
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;
    ui_.drawPanel(window, {centerX - 180.0f, mapH / 2.0f - 90.0f, 360.0f, 180.0f}, sf::Color(20, 50, 35), 210.0f);
    ui_.drawCenteredText(window, renderWorld_.levelName, centerX, mapH / 2.0f - 58.0f, 22, sf::Color(220, 240, 220));
    ui_.drawCenteredText(window, "Game Starting", centerX, mapH / 2.0f - 24.0f, 30, sf::Color::White);

    const std::string countText = std::to_string(std::max(1, static_cast<int>(renderWorld_.countdown)));
    ui_.drawCenteredText(window, countText, centerX, mapH / 2.0f + 24.0f, 64, sf::Color(120, 255, 150));
}

void GameClient::drawResultOverlay(sf::RenderWindow& window, float centerX, bool victory) const {
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;
    const float windowW = static_cast<float>(window.getSize().x);

    sf::RectangleShape dim({windowW, mapH});
    dim.setFillColor(sf::Color(0, 0, 0, 120));
    window.draw(dim);

    if (assets_.ready()) {
        // 图片已带标题文字，下方只补充关卡名/宝石等，避免重复叠字
        const sf::Texture& screenTex =
            victory ? ((renderWorld_.players[0].gems + renderWorld_.players[1].gems >= renderWorld_.totalGems &&
                        renderWorld_.totalGems > 0)
                           ? assets_.winScreen()
                           : assets_.winScreenPartial())
                    : assets_.loseScreen();

        sf::Sprite screen(screenTex);
        const float targetW = 520.0f;
        const float targetH = 280.0f;
        const float scale = std::min(targetW / static_cast<float>(screenTex.getSize().x),
                                     targetH / static_cast<float>(screenTex.getSize().y));
        screen.setScale(scale, scale);
        screen.setPosition(centerX - screenTex.getSize().x * scale / 2.0f,
                           mapH / 2.0f - screenTex.getSize().y * scale / 2.0f);
        window.draw(screen);
    } else {
        const sf::Color panelColor = victory ? sf::Color(20, 60, 35) : sf::Color(70, 20, 20);
        ui_.drawPanel(window, {centerX - 260.0f, mapH / 2.0f - 140.0f, 520.0f, 280.0f}, panelColor, 235.0f);
    }

    const std::string levelLine =
        "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + renderWorld_.levelName;

    if (victory) {
        if (!assets_.ready()) {
            ui_.drawCenteredText(window, "Level Complete!", centerX, mapH / 2.0f - 92.0f, 36, sf::Color(140, 255, 170));
        }
        ui_.drawCenteredText(window, levelLine, centerX, mapH / 2.0f - 48.0f, 22, sf::Color(220, 240, 220));
        ui_.drawCenteredText(window, "Both players reached the exit.", centerX, mapH / 2.0f - 16.0f, 20,
                             sf::Color(210, 230, 210));
    } else {
        if (!assets_.ready()) {
            ui_.drawCenteredText(window, "Game Over", centerX, mapH / 2.0f - 92.0f, 36, sf::Color(255, 120, 120));
        }
        ui_.drawCenteredText(window, levelLine, centerX, mapH / 2.0f - 48.0f, 22, sf::Color(240, 210, 210));
        if (!renderWorld_.players[0].alive && !renderWorld_.players[1].alive) {
            ui_.drawCenteredText(window, "Both players were eliminated.", centerX, mapH / 2.0f - 16.0f, 20,
                                 sf::Color(240, 210, 210));
        } else if (!renderWorld_.players[0].alive) {
            ui_.drawCenteredText(window, "Fire boy fell into danger.", centerX, mapH / 2.0f - 16.0f, 20,
                                 sf::Color(240, 210, 210));
        } else {
            ui_.drawCenteredText(window, "Water girl fell into danger.", centerX, mapH / 2.0f - 16.0f, 20,
                                 sf::Color(240, 210, 210));
        }
    }

    const int totalGems = renderWorld_.players[0].gems + renderWorld_.players[1].gems;
    ui_.drawCenteredText(window, "Gems: " + std::to_string(totalGems) + " / " + std::to_string(renderWorld_.totalGems),
                         centerX, mapH / 2.0f + 28.0f, 22, sf::Color(255, 220, 100));

    const float btnW = 150.0f;
    const float btnH = 52.0f;
    const float gap = 28.0f;
    const float rowY = mapH / 2.0f + 130.0f;
    const bool hasNextLevel =
        victory && renderWorld_.levelIndex + 1 < renderWorld_.levelCount &&
        isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(renderWorld_.levelIndex + 1));

    if (assets_.hasButtons()) {
        if (hasNextLevel) {
            const float totalW = btnW * 3.0f + gap * 2.0f;
            const float startX = centerX - totalW / 2.0f;
            ui_.drawImageButtonWithHint(window, {startX, rowY, btnW, btnH}, assets_.retryButton(), "[R] Retry");
            ui_.drawImageButtonWithHint(window, {startX + btnW + gap, rowY, btnW, btnH}, assets_.continueButton(),
                                        "[N] Next");
            ui_.drawImageButtonWithHint(window, {startX + (btnW + gap) * 2.0f, rowY, btnW, btnH}, assets_.menuButton(),
                                        "[Esc] Lobby");
        } else {
            const float startX = centerX - btnW - gap / 2.0f;
            ui_.drawImageButtonWithHint(window, {startX, rowY, btnW, btnH}, assets_.retryButton(), "[R] Retry");
            ui_.drawImageButtonWithHint(window, {startX + btnW + gap, rowY, btnW, btnH}, assets_.menuButton(),
                                        "[Esc] Lobby");
        }
    } else {
        ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 62.0f, 250.0f, 40.0f}, "R - Replay", true,
                       victory ? sf::Color(50, 130, 90) : sf::Color(170, 70, 70));
        ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 110.0f, 250.0f, 40.0f}, "ESC - Lobby", true,
                       sf::Color(90, 90, 120));
        if (hasNextLevel) {
            ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 158.0f, 250.0f, 40.0f}, "N - Next Level", true,
                           sf::Color(60, 120, 200));
        }
    }
}

const char* GameClient::roleDisplayName() const {
    switch (role_) {
        case PlayerRole::Fire:
            return "Fire Boy";
        case PlayerRole::Water:
            return "Water Girl";
        case PlayerRole::Poison:
            return "Poison Kid";
        default:
            return "Unknown";
    }
}

sf::Color GameClient::tileColor(TileType type) const {
    switch (type) {
        case TileType::Solid:
            return sf::Color(70, 55, 45);
        case TileType::Lava:
            return sf::Color(220, 70, 30);
        case TileType::Water:
            return sf::Color(40, 110, 220);
        case TileType::FireDoor:
            return sf::Color(180, 60, 30);
        case TileType::WaterDoor:
            return sf::Color(40, 90, 180);
        case TileType::FireExit:
            return sf::Color(255, 140, 60);
        case TileType::WaterExit:
            return sf::Color(100, 180, 255);
        case TileType::Gem:
            return sf::Color(255, 220, 60);
        case TileType::Acid:
            return sf::Color(34, 139, 34);
        case TileType::PoisonDoor:
            return sf::Color(40, 120, 40);
        case TileType::PoisonExit:
            return sf::Color(100, 200, 100);
        case TileType::Button:
            return sf::Color(160, 160, 80);
        default:
            return sf::Color::White;
    }
}

}  // namespace fireice
