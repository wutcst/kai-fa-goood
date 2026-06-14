#include "GameClient.hpp"
#include "LevelCatalog.hpp"
#include "LocaleText.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace fireice {

namespace {

constexpr float kPreviewMargin = 12.0f;
constexpr float kTitleCenterX = 512.0f;
constexpr float kMenuStartY = 300.0f;
constexpr float kMenuItemStep = 52.0f;

}  // namespace

bool GameClient::initialize(const std::string& host, PlayerRole preferredRole) {
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
    loadedLevelIndex_ = 0;
    renderWorld_.levelCount = LevelCatalog::instance().count();
    renderWorld_.levelIndex = 0;
    renderWorld_.totalGems = static_cast<uint8_t>(std::min(255, map_.countGems()));
    std::snprintf(renderWorld_.levelName, MAX_LEVEL_NAME, "%s", LevelCatalog::instance().at(0).title);

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

    std::array<char, 512> buffer{};
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
            serverAddress_ = sf::IpAddress(host_);
            if (startHosting()) {
                beginConnect();
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
            if (!discoveredRooms_.empty() &&
                selectedDiscoveredRoom_ < static_cast<int>(discoveredRooms_.size()) - 1) {
                selectedDiscoveredRoom_++;
                typedRoomCode_.clear();
            }
        } else if (event.key.code == sf::Keyboard::Enter) {
            if (selectedDiscoveredRoom_ >= 0 &&
                selectedDiscoveredRoom_ < static_cast<int>(discoveredRooms_.size())) {
                const auto& room = discoveredRooms_[selectedDiscoveredRoom_];
                host_ = room.address;
                serverAddress_ = sf::IpAddress(host_);
                typedRoomCode_ = room.roomCode;
                beginConnect();
            } else if (typedRoomCode_.size() == 6 || typedRoomCode_.empty()) {
                // Empty room code = quick join (server assigns room)
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
    window_.setFramerateLimit(0);
    window_.setVerticalSyncEnabled(false);
    window_.setView(sf::View(sf::FloatRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))));
}

void GameClient::handleLevelChange(uint8_t levelIndex, bool resizeWindow) {
    if (levelIndex == loadedLevelIndex_) {
        return;
    }

    const std::string path = LevelCatalog::instance().resolvePath(levelIndex);
    if (!map_.loadFromFile(path)) {
        std::cerr << "[Client] Failed to load level preview: " << path << std::endl;
        return;
    }

    loadedLevelIndex_ = levelIndex;
    if (resizeWindow && !lobbyLayout_) {
        useGameLayout();
    }
}

void GameClient::onPhaseChanged(GamePhase previous, GamePhase current) {
    if (current == GamePhase::Lobby && previous != GamePhase::Lobby) {
        localReady_ = false;
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
        if (isHosting_) {
            stopHosting();
        }
        return;
    }

    DisconnectPacket packet{};
    packet.slot = slot_;
    std::array<char, 512> buffer{};
    std::size_t size = 0;
    packPacket(packet, buffer, size);
    socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT);
    connected_ = false;

    if (isHosting_) {
        stopHosting();
    }
}

bool GameClient::startHosting() {
    if (isHosting_) return true;

    server_ = std::make_unique<GameServer>();
    if (!server_->start(0)) {
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
    if (!isHosting_) return;

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
    if (discoveryActive_) return;

    discoveryActive_ = true;
    discoveredRooms_.clear();
    selectedDiscoveredRoom_ = -1;

    const std::string targetCode = typedRoomCode_;

    std::thread([this, targetCode]() {
#ifdef _WIN32
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            discoveryActive_ = false;
            return;
        }

        int broadcast = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

        int timeout = 1500;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(SERVER_PORT);
        dest.sin_addr.s_addr = INADDR_BROADCAST;

        DiscoveryPacket disc{};
        disc.isResponse = 0;
        if (!targetCode.empty()) {
            std::snprintf(disc.roomCode, MAX_ROOM_CODE, "%s", targetCode.c_str());
        }

        std::array<char, 512> buf{};
        std::size_t sz = 0;
        packPacket(disc, buf, sz);
        sendto(sock, buf.data(), static_cast<int>(sz), 0, reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));

        char recvBuf[512];
        sockaddr_in from{};
        int fromLen = sizeof(from);

        while (discoveryActive_) {
            int n = recvfrom(sock, recvBuf, sizeof(recvBuf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
            if (n <= 0) break;

            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));

            DiscoveryPacket resp{};
            if (unpackPacket(recvBuf, static_cast<std::size_t>(n), resp) && resp.isResponse == 1) {
                DiscoveredRoom room;
                room.address = ipStr;
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
                    if (selectedDiscoveredRoom_ < 0) selectedDiscoveredRoom_ = 0;
                }
            }
        }

        closesocket(sock);
#else
        (void)targetCode;
#endif
        discoveryActive_ = false;
    }).detach();
}

void GameClient::handleDiscoveryResponse(const DiscoveryPacket& packet, const sf::IpAddress& sender) {
    if (!discoveryActive_ || !packet.isResponse) return;

    DiscoveredRoom room;
    room.address = sender.toString();
    room.roomCode = packet.roomCode;
    room.levelName = packet.levelName[0] ? packet.levelName : "???";
    room.playerCount = packet.playerCount;
    room.maxPlayers = packet.maxPlayers;

    for (const auto& dr : discoveredRooms_) {
        if (dr.address == room.address && dr.roomCode == room.roomCode) return;
    }
    if (room.roomCode[0]) {
        discoveredRooms_.push_back(room);
        if (selectedDiscoveredRoom_ < 0) selectedDiscoveredRoom_ = 0;
    }
}

void GameClient::pollNetwork() {
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
                if (packet.world.levelIndex != loadedLevelIndex_) {
                    handleLevelChange(packet.world.levelIndex, !lobbyLayout_);
                }

                world_ = packet.world;
                renderWorld_ = packet.world;

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
    if (std::chrono::duration<float>(now - lastInputSend_).count() < 1.0f / 60.0f) {
        return false;
    }

    InputPacket packet{};
    packet.slot = slot_;
    packet.tick = ++inputTick_;
    packet.flags = static_cast<uint8_t>(currentInput_);

    std::array<char, 512> buffer{};
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

    std::array<char, 512> buffer{};
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

    if (event.type == sf::Event::MouseMoved && clientScreen_ == ClientScreen::Room) {
        return;
    }
    if (event.type == sf::Event::MouseButtonPressed && clientScreen_ == ClientScreen::Room) {
        if (event.mouseButton.button != sf::Mouse::Left) {
            return;
        }
        const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
        const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);

        // Ready / cancel ready button
        const float btnX = w - 172.0f;
        const float readyBtnW = 140.0f;
        const sf::FloatRect readyArea{btnX, 554.0f, readyBtnW, 48.0f};
        if (readyArea.contains(mouse)) {
            if (!localReady_) {
                localReady_ = true;
                sendAction(PlayerAction::Ready);
            } else {
                localReady_ = false;
                sendAction(PlayerAction::ReturnToLobby);
            }
            return;
        }

        // Back / leave room button
        const float backBtnY = renderWorld_.connectedCount >= 2 ? 610.0f : 590.0f;
        const sf::FloatRect backArea{btnX, backBtnY, readyBtnW, 36.0f};
        if (backArea.contains(mouse)) {
            disconnect();
            connected_ = false;
            connectRequested_ = false;
            clientScreen_ = ClientScreen::Title;
            useTitleLayout();
            updateMusic(GamePhase::Lobby);
            return;
        }

        // Level selector bar: left/right arrows
        const float levelBarX = 148.0f;
        const float levelBarY = 344.0f;
        const float levelBarW = w - levelBarX * 2.0f;
        const float levelBarH = 48.0f;
        const sf::FloatRect levelBar{levelBarX, levelBarY, levelBarW, levelBarH};
        if (levelBar.contains(mouse) && !localReady_) {
            const float relX = mouse.x - levelBarX;
            const float arrowWidth = 36.0f;
            if (relX < arrowWidth) {
                sendAction(PlayerAction::PrevLevel);
            } else if (relX > levelBarW - arrowWidth) {
                sendAction(PlayerAction::NextLevel);
            }
            return;
        }

        // Level dots: direct level selection
        const float dotStartX = levelBarX + levelBarW / 2.0f - (renderWorld_.levelCount * 16.0f) / 2.0f;
        const float dotY = levelBarY + levelBarH - 14.0f;
        for (uint8_t i = 0; i < renderWorld_.levelCount; ++i) {
            const sf::FloatRect dotArea{dotStartX + i * 16.0f - 6.0f, dotY - 6.0f, 12.0f, 12.0f};
            if (dotArea.contains(mouse) && !localReady_) {
                sendAction(PlayerAction::SelectLevel, i);
                return;
            }
        }
        return;
    }

    if (event.type != sf::Event::KeyPressed) {
        return;
    }

    if (event.key.code == sf::Keyboard::Escape && clientScreen_ == ClientScreen::Room) {
        disconnect();
        connected_ = false;
        connectRequested_ = false;
        clientScreen_ = ClientScreen::Title;
        useTitleLayout();
        updateMusic(GamePhase::Lobby);
        return;
    }

    if (event.key.code == sf::Keyboard::Escape && renderWorld_.phase != GamePhase::Lobby &&
        clientScreen_ != ClientScreen::Room) {
        localReady_ = false;
        sendAction(PlayerAction::ReturnToLobby);
        return;
    }

    if (clientScreen_ == ClientScreen::Room) {
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
        {
            const auto keyCode = static_cast<int>(event.key.code);
            int level = -1;
            if (keyCode >= static_cast<int>(sf::Keyboard::Num1) && keyCode <= static_cast<int>(sf::Keyboard::Num8)) {
                level = keyCode - static_cast<int>(sf::Keyboard::Num1);
            } else if (keyCode >= static_cast<int>(sf::Keyboard::Numpad1) &&
                       keyCode <= static_cast<int>(sf::Keyboard::Numpad8)) {
                level = keyCode - static_cast<int>(sf::Keyboard::Numpad1);
            }
            if (level >= 0 && static_cast<uint8_t>(level) < renderWorld_.levelCount && !localReady_) {
                sendAction(PlayerAction::SelectLevel, static_cast<uint8_t>(level));
                return;
            }
        }
        if (event.key.code == sf::Keyboard::Enter && !localReady_) {
            localReady_ = true;
            sendAction(PlayerAction::Ready);
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && localReady_) {
            localReady_ = false;
            sendAction(PlayerAction::ReturnToLobby);
            return;
        }
        return;
    }

    if (renderWorld_.phase == GamePhase::Lobby) {
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
            if (level < renderWorld_.levelCount) {
                sendAction(PlayerAction::SelectLevel, level);
            }
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && !localReady_) {
            localReady_ = true;
            sendAction(PlayerAction::Ready);
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && localReady_) {
            localReady_ = false;
            sendAction(PlayerAction::ReturnToLobby);
            return;
        }
    }

    if (renderWorld_.phase == GamePhase::Victory) {
        if (event.key.code == sf::Keyboard::N && renderWorld_.levelIndex + 1 < renderWorld_.levelCount) {
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
    window_.clear(sf::Color(12, 18, 14));

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
    } else if (clientScreen_ == ClientScreen::Room && renderWorld_.phase == GamePhase::Lobby) {
        renderRoomScreen();
    } else if (renderWorld_.phase == GamePhase::Lobby) {
        renderLobbyScreen();
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
    const float charW = 32.0f;
    const float charH = 56.0f;

    sf::RectangleShape fireBoy({charW, charH});
    fireBoy.setPosition(48.0f + 20.0f, static_cast<float>(LOBBY_WINDOW_HEIGHT) - charH - 36.0f);
    fireBoy.setFillColor(sf::Color(255, 90, 40));
    window_.draw(fireBoy);

    sf::RectangleShape waterGirl({charW, charH});
    waterGirl.setPosition(static_cast<float>(LOBBY_WINDOW_WIDTH) - 48.0f - 20.0f - charW,
                          static_cast<float>(LOBBY_WINDOW_HEIGHT) - charH - 36.0f);
    waterGirl.setFillColor(sf::Color(60, 140, 255));
    window_.draw(waterGirl);
}

void GameClient::renderTitleScreen() {
    sf::RectangleShape fallback({static_cast<float>(LOBBY_WINDOW_WIDTH), static_cast<float>(LOBBY_WINDOW_HEIGHT)});
    fallback.setFillColor(sf::Color(28, 42, 34));
    window_.draw(fallback);

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

void GameClient::renderJoinRoomScreen() {
    roomAnimTimer_ += 0.016f;
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    sf::RectangleShape fallback({w, h});
    fallback.setFillColor(sf::Color(12, 20, 30));
    window_.draw(fallback);

    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 100));
    window_.draw(dim);

    const float panelW = 620.0f;
    const float panelH = 480.0f;
    const float panelX = (w - panelW) / 2.0f;
    const float panelY = 80.0f;
    const sf::FloatRect dialog{panelX, panelY, panelW, panelH};
    ui_.drawPanel(window_, dialog, sf::Color(20, 28, 44, 240), 240.0f);
    ui_.drawOutlinedCenteredText(window_, "加入房间  (局域网自动发现)", w / 2.0f, dialog.top + 16.0f, 26,
                                 sf::Color(255, 230, 100), sf::Color(80, 50, 10), 2.5f);

    // --- Discovered rooms list ---
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
        for (int i = 0; i < dots; ++i) scanMsg += ".";
        ui_.drawCenteredText(window_, scanMsg, w / 2.0f, listY + listH / 2.0f - 14.0f, 22, sf::Color(180, 200, 255));
        ui_.drawCenteredText(window_, "自动发现同一网络下的游戏房间", w / 2.0f, listY + listH / 2.0f + 18.0f, 14,
                             sf::Color(120, 140, 180));
    } else if (discoveredRooms_.empty()) {
        ui_.drawCenteredText(window_, "未发现局域网房间", w / 2.0f, listY + listH / 2.0f - 14.0f, 20,
                             sf::Color(180, 160, 120));
        ui_.drawCenteredText(window_, "请确认主机在同一网络下已创建房间  或手动输入房间号", w / 2.0f,
                             listY + listH / 2.0f + 18.0f, 14, sf::Color(140, 150, 170));
    } else {
        ui_.drawText(window_,
                     "发现 " + std::to_string(discoveredRooms_.size()) + " 个房间  [↑↓] 选择  [Enter] 加入",
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
            const std::string label = "房间 " + room.roomCode + "  |  " + room.levelName +
                                      "  |  玩家 " + std::to_string(room.playerCount) + "/" +
                                      std::to_string(room.maxPlayers) + "  |  " + room.address;
            ui_.drawText(window_, label, listX + 18.0f, rowY2 + 6.0f, 16,
                         sel ? sf::Color::White : sf::Color(200, 210, 220));
        }
    }

    // --- Manual entry divider ---
    const float dividerY = listY + listH + 12.0f;
    ui_.drawCenteredText(window_, "—— 或手动输入房间号 ——", w / 2.0f, dividerY, 14, sf::Color(140, 155, 180));

    // Room code input box
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
        if (i < 5) displayCode.push_back(' ');
    }
    ui_.drawOutlinedCenteredText(window_, displayCode, w / 2.0f, boxY + 7.0f, 26, sf::Color(255, 255, 255),
                                 sf::Color(40, 40, 80), 2.0f);

    if (static_cast<int>(roomAnimTimer_ * 2.0f) % 2 == 0 && typedRoomCode_.size() < 6) {
        const float cursorX = boxX + 16.0f + static_cast<float>(typedRoomCode_.size()) * 25.0f;
        sf::RectangleShape cursor({2.0f, 24.0f});
        cursor.setPosition(cursorX, boxY + 10.0f);
        cursor.setFillColor(sf::Color(255, 255, 255, 200));
        window_.draw(cursor);
    }

    ui_.drawCenteredText(window_, "[↑↓] 选房间  [数字键] 输房间号  [Enter] 加入  [Esc] 返回  [F5] 刷新", w / 2.0f,
                         dialog.top + panelH - 24.0f, 14, sf::Color(160, 170, 190));
}

void GameClient::renderRoomScreen() {
    roomAnimTimer_ += 0.016f;
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    sf::RectangleShape fallback({w, h});
    fallback.setFillColor(sf::Color(12, 20, 30));
    window_.draw(fallback);

    // Top header bar
    ui_.drawPanel(window_, {0.0f, 0.0f, w, 68.0f}, sf::Color(16, 22, 40, 230), 230.0f);
    ui_.drawOutlinedCenteredText(window_, "Fire & Ice 森林神殿", w / 2.0f, 8.0f, 28, sf::Color(255, 230, 100),
                                 sf::Color(80, 50, 10), 2.5f);
    ui_.drawCenteredText(window_, "在线联机房", w / 2.0f, 42.0f, 16, sf::Color(180, 195, 220));

    // Room code display (prominent)
    const std::string roomCodeStr =
        renderWorld_.roomCode[0] ? std::string("房间号: ") + std::string(renderWorld_.roomCode) : "房间号: ------";
    ui_.drawOutlinedCenteredText(window_, roomCodeStr, w - 180.0f, 8.0f, 16, sf::Color(100, 255, 140),
                                 sf::Color(20, 60, 20), 2.0f);

    // Host IP display
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

    // --- Player panels ---
    const float panelW = 200.0f;
    const float panelH = 250.0f;
    const float panelY = 84.0f;

    if (renderWorld_.connectedCount <= 1) {
        renderRoomPlayerPanel(w / 2.0f - panelW / 2.0f, panelY, panelW, panelH, 0, PlayerRole::Fire);
    } else if (renderWorld_.connectedCount == 2) {
        renderRoomPlayerPanel(48.0f, panelY, panelW, panelH, 0, PlayerRole::Fire);
        renderRoomPlayerPanel(w - 48.0f - panelW, panelY, panelW, panelH, 1, PlayerRole::Water);
    } else {
        const float totalW = panelW * 3.0f + 24.0f;
        const float startX = (w - totalW) / 2.0f;
        renderRoomPlayerPanel(startX, panelY, panelW, panelH, 0, PlayerRole::Fire);
        renderRoomPlayerPanel(startX + panelW + 12.0f, panelY, panelW, panelH, 1, PlayerRole::Water);
        renderRoomPlayerPanel(startX + (panelW + 12.0f) * 2.0f, panelY, panelW, panelH, 2, PlayerRole::Poison);
    }

    // VS divider (only when 2 players)
    if (renderWorld_.connectedCount == 2) {
        const float vsCenterX = w / 2.0f;
        const float vsY = panelY + panelH / 2.0f - 8.0f;
        const sf::Color pulseCol(255,
                                 static_cast<sf::Uint8>(200 + static_cast<int>(55.0 * std::sin(roomAnimTimer_ * 2.5))),
                                 static_cast<sf::Uint8>(50 + static_cast<int>(50.0 * std::sin(roomAnimTimer_ * 2.5))));
        ui_.drawOutlinedCenteredText(window_, "VS", vsCenterX, vsY, 38, pulseCol, sf::Color(80, 40, 10), 3.0f);
    }

    // --- Map preview in center ---
    const float previewX = 172.0f;
    const float previewY = 84.0f;
    const float previewW = w - previewX * 2.0f;
    const float previewH = 210.0f;
    const sf::FloatRect previewPanel{previewX, previewY, previewW, previewH};
    ui_.drawPanel(window_, previewPanel, sf::Color(18, 24, 36, 210), 210.0f);

    // Map preview content
    const sf::FloatRect previewArea{previewPanel.left + 16.0f, previewPanel.top + 40.0f, previewPanel.width - 32.0f,
                                    previewPanel.height - 56.0f};
    sf::RectangleShape previewBg({previewArea.width, previewArea.height});
    previewBg.setPosition(previewArea.left, previewArea.top);
    previewBg.setFillColor(sf::Color(8, 10, 18));
    window_.draw(previewBg);
    drawMapPreview(window_, previewArea);

    ui_.drawOutlinedCenteredText(window_, "地图预览", previewPanel.left + previewPanel.width / 2.0f,
                                 previewPanel.top + 8.0f, 18, sf::Color(200, 220, 255), sf::Color(30, 50, 80), 1.5f);

    const std::string previewDetail =
        std::string(renderWorld_.levelName) + "  |  Gems: " + std::to_string(renderWorld_.totalGems) +
        "  |  Size: " + std::to_string(map_.width()) + "x" + std::to_string(map_.height());
    ui_.drawCenteredText(window_, previewDetail, previewPanel.left + previewPanel.width / 2.0f,
                         previewArea.top + previewArea.height + 12.0f, 15, sf::Color(200, 200, 180));

    // --- Level selector bar ---
    const float levelBarX = 148.0f;
    const float levelBarY = 344.0f;
    const float levelBarW = w - levelBarX * 2.0f;
    const float levelBarH = 48.0f;
    ui_.drawPanel(window_, {levelBarX, levelBarY, levelBarW, levelBarH}, sf::Color(22, 28, 44, 220), 220.0f);

    // Level navigation arrows
    ui_.drawCenteredText(window_, "\xe2\x97\x80", levelBarX + 20.0f, levelBarY + 4.0f, 28, sf::Color(180, 200, 240));
    ui_.drawCenteredText(window_, "\xe2\x96\xb6", levelBarX + levelBarW - 20.0f, levelBarY + 4.0f, 28,
                         sf::Color(180, 200, 240));

    const LevelCatalog& catalog = LevelCatalog::instance();
    const std::string levelTitle =
        "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + catalog.at(renderWorld_.levelIndex).title;
    ui_.drawOutlinedCenteredText(window_, levelTitle, levelBarX + levelBarW / 2.0f, levelBarY + 8.0f, 22,
                                 sf::Color(255, 240, 180), sf::Color(60, 40, 10), 2.0f);

    // Level dots
    const float dotStartX = levelBarX + levelBarW / 2.0f - (renderWorld_.levelCount * 16.0f) / 2.0f;
    for (uint8_t i = 0; i < renderWorld_.levelCount; ++i) {
        sf::CircleShape dot(4.0f);
        dot.setPosition(dotStartX + i * 16.0f, levelBarY + levelBarH - 14.0f);
        dot.setFillColor(i == renderWorld_.levelIndex ? sf::Color(255, 220, 80) : sf::Color(100, 110, 130));
        window_.draw(dot);
    }

    // --- Bottom status bar ---
    const float statusY = 404.0f;
    const float statusH = 248.0f;
    ui_.drawPanel(window_, {32.0f, statusY, w - 64.0f, statusH - 16.0f}, sf::Color(20, 26, 44, 220), 220.0f);

    // Players connected status
    const bool fireConnected = renderWorld_.connectedCount >= 1;
    const bool waterConnected = renderWorld_.connectedCount >= 2;
    const bool poisonConnected = renderWorld_.connectedCount >= 3;
    const bool fireReady = (renderWorld_.readyMask & 0x01) != 0;
    const bool waterReady = (renderWorld_.readyMask & 0x02) != 0;
    const bool poisonReady = (renderWorld_.readyMask & 0x04) != 0;

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

    // Map info line
    const std::string mapLine = "当前地图: " + std::to_string(renderWorld_.levelIndex + 1) + "/" +
                                std::to_string(renderWorld_.levelCount) +
                                "  宝石: " + std::to_string(renderWorld_.totalGems) +
                                "   尺寸: " + std::to_string(map_.width()) + "x" + std::to_string(map_.height());
    ui_.drawText(window_, mapLine, 52.0f, statusY + 72.0f, 15, sf::Color(170, 180, 200));

    // Controls hint
    ui_.drawText(window_, "[↑↓] 或 [1-8] 选关   [Enter] 准备 / 取消准备   [Esc] 返回", 52.0f, statusY + 100.0f, 14,
                 sf::Color(140, 150, 170));

    // Connection instructions for host
    if (isHosting_ && renderWorld_.connectedCount < MAX_PLAYERS) {
        const std::string roomCodeStr2 =
            renderWorld_.roomCode[0] ? std::string(renderWorld_.roomCode) : "------";
        const std::string connInfo = "其他玩家连接: fireice_client.exe " + localIp_ + " <角色>  房间号: " + roomCodeStr2;
        ui_.drawText(window_, connInfo, 52.0f, statusY + 124.0f, 13, sf::Color(120, 200, 160));
    }

    // --- Right side buttons ---
    const float btnX = w - 172.0f;

    // Ready / Start button
    const float readyBtnY = 554.0f;
    const sf::FloatRect readyArea{btnX, readyBtnY, 140.0f, 48.0f};

    if (!localReady_) {
        ui_.drawButton(window_, readyArea, "准备游戏", true, sf::Color(60, 130, 210));
    } else {
        ui_.drawButton(window_, readyArea, "已准备 (Esc取消)", true, sf::Color(50, 160, 90));
    }

    // Back button
    const float backBtnY = renderWorld_.connectedCount >= 2 ? 610.0f : 590.0f;

    // Waiting for players message (only when not all slots filled)
    if (renderWorld_.connectedCount < MAX_PLAYERS) {
        const std::string waitMsg = "等待其他玩家加入房间... (当前 " + std::to_string(renderWorld_.connectedCount) +
                                    "/" + std::to_string(MAX_PLAYERS) + ")";
        ui_.drawOutlinedCenteredText(window_, waitMsg, w / 2.0f, h - 28.0f, 17, sf::Color(255, 220, 100),
                                     sf::Color(80, 50, 10), 2.0f);
        const int dotCount = (static_cast<int>(roomAnimTimer_ * 2.0f) % 3) + 1;
        std::string dots;
        for (int i = 0; i < dotCount; ++i)
            dots += ".";
        ui_.drawText(window_, dots, w / 2.0f + 180.0f, h - 24.0f, 17, sf::Color(255, 220, 100));
    }
    const sf::FloatRect backArea{btnX, backBtnY, 140.0f, 36.0f};
    ui_.drawButton(window_, backArea, "离开房间", true, sf::Color(120, 60, 50));
}

void GameClient::renderRoomPlayerPanel(float panelX, float panelY, float panelW, float panelH, int playerSlot,
                                       PlayerRole expectedRole) {
    const bool isConnected = renderWorld_.connectedCount > static_cast<uint8_t>(playerSlot);
    const bool isReady = (renderWorld_.readyMask & (1u << playerSlot)) != 0;
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
    sf::RectangleShape placeholder({60.0f, 100.0f});
    placeholder.setPosition(panelX + (panelW - 60.0f) / 2.0f, panelY + 44.0f);
    placeholder.setFillColor(isConnected ? roleColor : sf::Color(80, 80, 80));
    window_.draw(placeholder);

    if (isSelf) {
        sf::CircleShape indicator(6.0f);
        indicator.setPosition(panelX + panelW / 2.0f + 40.0f, panelY + 50.0f);
        indicator.setFillColor(sf::Color(80, 255, 120));
        window_.draw(indicator);
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

    sf::RectangleShape fallback({w, static_cast<float>(LOBBY_WINDOW_HEIGHT)});
    fallback.setFillColor(sf::Color(14, 20, 32));
    window_.draw(fallback);

    ui_.drawPanel(window_, {0.0f, 0.0f, w, 72.0f}, sf::Color(28, 36, 52, 200), 200.0f);
    ui_.drawCenteredText(window_, "Forest Temple Online", w / 2.0f, 14.0f, 34, sf::Color(255, 230, 170));
    ui_.drawCenteredText(window_, "Level Select Lobby", w / 2.0f, 48.0f, 18, sf::Color(180, 190, 210));

    const sf::FloatRect listPanel{32.0f, 88.0f, 420.0f, 420.0f};
    ui_.drawPanel(window_, listPanel, sf::Color(24, 30, 44, 210), 210.0f);
    ui_.drawText(window_, "LEVEL LIST", listPanel.left + 16.0f, listPanel.top + 12.0f, 20, sf::Color(220, 220, 220));

    const LevelCatalog& catalog = LevelCatalog::instance();
    const float rowHeight = 44.0f;
    const float listTop = listPanel.top + 44.0f;

    for (uint8_t i = 0; i < renderWorld_.levelCount; ++i) {
        const LevelInfo& info = catalog.at(i);
        const float rowY = listTop + static_cast<float>(i) * rowHeight;
        const bool selected = i == renderWorld_.levelIndex;

        sf::FloatRect rowBox{listPanel.left + 10.0f, rowY, listPanel.width - 20.0f, rowHeight - 6.0f};
        sf::RectangleShape row({rowBox.width, rowBox.height});
        row.setPosition(rowBox.left, rowBox.top);
        row.setFillColor(selected ? sf::Color(55, 95, 150) : sf::Color(35, 42, 58));
        row.setOutlineThickness(selected ? 2.0f : 1.0f);
        row.setOutlineColor(selected ? sf::Color(140, 200, 255) : sf::Color(70, 80, 100));
        window_.draw(row);

        const std::string title = std::to_string(i + 1) + ". " + info.title;
        ui_.drawText(window_, title, rowBox.left + 12.0f, rowBox.top + 6.0f, 20,
                     selected ? sf::Color::White : sf::Color(200, 200, 200));
        ui_.drawText(window_, info.subtitle, rowBox.left + 12.0f, rowBox.top + 24.0f, 14, sf::Color(150, 160, 180));
    }

    const sf::FloatRect previewPanel{472.0f, 88.0f, 520.0f, 420.0f};
    ui_.drawPanel(window_, previewPanel, sf::Color(20, 26, 38, 210), 210.0f);
    ui_.drawText(window_, "MAP PREVIEW", previewPanel.left + 16.0f, previewPanel.top + 12.0f, 20,
                 sf::Color(220, 220, 220));

    const sf::FloatRect previewArea{previewPanel.left + kPreviewMargin, previewPanel.top + 40.0f,
                                    previewPanel.width - kPreviewMargin * 2.0f, previewPanel.height - 52.0f};
    sf::RectangleShape previewBg({previewArea.width, previewArea.height});
    previewBg.setPosition(previewArea.left, previewArea.top);
    previewBg.setFillColor(sf::Color(12, 14, 20));
    window_.draw(previewBg);
    drawMapPreview(window_, previewArea);

    const std::string detail =
        std::string(renderWorld_.levelName) + "  |  Gems: " + std::to_string(renderWorld_.totalGems);
    ui_.drawCenteredText(window_, detail, previewPanel.left + previewPanel.width / 2.0f,
                         previewPanel.top + previewPanel.height - 28.0f, 18, sf::Color(255, 220, 120));

    const sf::FloatRect statusPanel{32.0f, 524.0f, 960.0f, 96.0f};
    ui_.drawPanel(window_, statusPanel, sf::Color(26, 32, 48, 210), 210.0f);

    const std::string roleLine = std::string("You: ") + roleDisplayName() +
                                 "     Players: " + std::to_string(renderWorld_.connectedCount) + "/2";
    ui_.drawText(window_, roleLine, statusPanel.left + 20.0f, statusPanel.top + 14.0f, 20, sf::Color(220, 220, 220));

    const bool fireReady = (renderWorld_.readyMask & 0x01) != 0;
    const bool waterReady = (renderWorld_.readyMask & 0x02) != 0;
    ui_.drawText(window_, std::string("Fire: ") + (fireReady ? "Ready" : "Waiting"), statusPanel.left + 20.0f,
                 statusPanel.top + 42.0f, 18, fireReady ? sf::Color(255, 140, 80) : sf::Color(150, 150, 150));
    ui_.drawText(window_, std::string("Water: ") + (waterReady ? "Ready" : "Waiting"), statusPanel.left + 180.0f,
                 statusPanel.top + 42.0f, 18, waterReady ? sf::Color(100, 180, 255) : sf::Color(150, 150, 150));

    ui_.drawText(window_, "[Up/Down] or [1-8] Select   [Enter] Ready   [Esc] Cancel ready", statusPanel.left + 360.0f,
                 statusPanel.top + 42.0f, 16, sf::Color(160, 170, 190));

    const sf::FloatRect playArea{760.0f, statusPanel.top + 8.0f, 220.0f, 72.0f};
    if (renderWorld_.connectedCount < 2) {
        ui_.drawButton(window_, playArea, "Wait Partner", false, sf::Color(120, 90, 40));
    } else if (!localReady_) {
        ui_.drawButton(window_, playArea, "ENTER - Start", true, sf::Color(60, 130, 210));
    } else {
        ui_.drawButton(window_, playArea, "Ready! (Esc cancel)", true, sf::Color(50, 140, 90));
    }
}

void GameClient::renderGameScreen() {
    const float mapW = static_cast<float>(map_.width()) * TILE_SIZE;
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;

    sf::RectangleShape bg({mapW, mapH});
    bg.setFillColor(sf::Color(8, 12, 18));
    window_.draw(bg);

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

            sf::RectangleShape tile({tileSize, tileSize});
            tile.setPosition(px, py);
            tile.setFillColor(tileColor(type));
            window.draw(tile);
        }
    }
}

void GameClient::drawMap(sf::RenderWindow& window) const {
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
        ui_.drawText(window, "ESC - Back to Lobby", windowW - 220.0f, hudY + 16.0f, 18, sf::Color(255, 180, 100));
    }

    if (renderWorld_.phase == GamePhase::Countdown) {
        ui_.drawText(window, "ESC - Back to Lobby", windowW - 220.0f, hudY + 16.0f, 18, sf::Color(255, 180, 100));
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

    const sf::Color panelColor = victory ? sf::Color(20, 60, 35) : sf::Color(70, 20, 20);
    ui_.drawPanel(window, {centerX - 260.0f, mapH / 2.0f - 140.0f, 520.0f, 280.0f}, panelColor, 235.0f);

    const std::string levelLine =
        "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + renderWorld_.levelName;

    if (victory) {
        ui_.drawCenteredText(window, "Level Complete!", centerX, mapH / 2.0f - 92.0f, 36, sf::Color(140, 255, 170));
        ui_.drawCenteredText(window, levelLine, centerX, mapH / 2.0f - 48.0f, 22, sf::Color(220, 240, 220));
        ui_.drawCenteredText(window, "Both players reached the exit.", centerX, mapH / 2.0f - 16.0f, 20,
                             sf::Color(210, 230, 210));
    } else {
        ui_.drawCenteredText(window, "Game Over", centerX, mapH / 2.0f - 92.0f, 36, sf::Color(255, 120, 120));
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

    const bool hasNextLevel = victory && renderWorld_.levelIndex + 1 < renderWorld_.levelCount;

    ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 62.0f, 250.0f, 40.0f}, "R - Replay", true,
                   victory ? sf::Color(50, 130, 90) : sf::Color(170, 70, 70));
    ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 110.0f, 250.0f, 40.0f}, "ESC - Lobby", true,
                   sf::Color(90, 90, 120));
    if (hasNextLevel) {
        ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 158.0f, 250.0f, 40.0f}, "N - Next Level", true,
                       sf::Color(60, 120, 200));
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
