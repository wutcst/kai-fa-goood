#include "GameClient.hpp"
#include "LevelCatalog.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <iostream>

namespace fireice {

namespace {

constexpr float kPreviewMargin = 12.0f;

} // namespace

bool GameClient::connect(const std::string& host, PlayerRole preferredRole) {
    host_ = host;
    serverAddress_ = sf::IpAddress(host);
    if (serverAddress_ == sf::IpAddress::None) {
        serverAddress_ = sf::IpAddress::LocalHost;
    }
    role_ = preferredRole;

    if (!map_.loadFromFile(LevelCatalog::instance().resolvePath(0))) {
        std::cerr << "[Client] Failed to load default level preview" << std::endl;
        return false;
    }
    loadedLevelIndex_ = 0;

    if (socket_.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
        std::cerr << "[Client] Failed to bind local UDP port" << std::endl;
        return false;
    }
    localPort_ = socket_.getLocalPort();
    socket_.setBlocking(false);

    ui_.loadFont();
    useLobbyLayout();

    ConnectRequestPacket request{};
    request.preferredRole = preferredRole;
    std::snprintf(request.playerName, sizeof(request.playerName), "%s", roleName(preferredRole));

    std::array<char, 512> buffer{};
    std::size_t size = 0;
    packPacket(request, buffer, size);
    socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT);

    lastConnectRetry_ = std::chrono::steady_clock::now();
    lastInputSend_ = lastConnectRetry_;

    std::cout << "[Client] Connecting to " << serverAddress_.toString() << ":" << SERVER_PORT << std::endl;
    return true;
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
        useLobbyLayout();
        handleLevelChange(renderWorld_.levelIndex, false);
    } else if (previous == GamePhase::Lobby && current != GamePhase::Lobby) {
        handleLevelChange(renderWorld_.levelIndex, false);
        useGameLayout();
    }
}

void GameClient::run() {
    while (window_.isOpen()) {
        sf::Event event{};
        while (window_.pollEvent(event)) {
            handleWindowEvent(event);
        }

        pollNetwork();

        if (!connected_) {
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<float>(now - lastConnectRetry_).count() > 1.0f) {
                ConnectRequestPacket request{};
                request.preferredRole = role_;
                std::snprintf(request.playerName, sizeof(request.playerName), "%s", roleName(role_));
                std::array<char, 512> buffer{};
                std::size_t outSize = 0;
                packPacket(request, buffer, outSize);
                socket_.send(buffer.data(), outSize, serverAddress_, SERVER_PORT);
                lastConnectRetry_ = now;
            }
        } else if (renderWorld_.phase == GamePhase::Playing) {
            currentInput_ = readLocalInput();
            sendInput();
        }

        render();
    }

    disconnect();
}

void GameClient::disconnect() {
    if (!connected_) {
        return;
    }

    DisconnectPacket packet{};
    packet.slot = slot_;
    std::array<char, 512> buffer{};
    std::size_t size = 0;
    packPacket(packet, buffer, size);
    socket_.send(buffer.data(), size, serverAddress_, SERVER_PORT);
    connected_ = false;
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
            slot_ = packet.slot;
            role_ = packet.role;
            localReady_ = false;
            std::cout << "[Client] Connected as " << roleName(role_) << " (slot " << static_cast<int>(slot_) << ")"
                      << std::endl;
            break;
        }
        case PacketType::ConnectReject: {
            ConnectRejectPacket packet{};
            if (!unpackPacket(buffer.data(), received, packet)) {
                break;
            }
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
    if (std::chrono::duration<float>(now - lastInputSend_).count() < 1.0f / 30.0f) {
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

    if (event.type != sf::Event::KeyPressed || !connected_) {
        return;
    }

    if (event.key.code == sf::Keyboard::Escape && renderWorld_.phase != GamePhase::Lobby) {
        localReady_ = false;
        sendAction(PlayerAction::ReturnToLobby);
        return;
    }

    if (renderWorld_.phase == GamePhase::Lobby) {
        if ((event.key.code == sf::Keyboard::Left || event.key.code == sf::Keyboard::Up
             || event.key.code == sf::Keyboard::Q) && !localReady_) {
            sendAction(PlayerAction::PrevLevel);
            return;
        }
        if ((event.key.code == sf::Keyboard::Right || event.key.code == sf::Keyboard::Down
             || event.key.code == sf::Keyboard::E) && !localReady_) {
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
        if (event.key.code == sf::Keyboard::N
            && renderWorld_.levelIndex + 1 < renderWorld_.levelCount) {
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
    }

    return input;
}

void GameClient::render() {
    window_.clear(sf::Color(18, 22, 32));

    if (!connected_) {
        drawConnectingScreen(window_);
    } else if (renderWorld_.phase == GamePhase::Lobby) {
        renderLobbyScreen();
    } else {
        renderGameScreen();
    }

    window_.display();
}

void GameClient::renderLobbyScreen() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    ui_.drawPanel(window_, {0.0f, 0.0f, w, 72.0f}, sf::Color(28, 36, 52), 255.0f);
    ui_.drawCenteredText(window_, "Forest Temple Online", w / 2.0f, 14.0f, 34, sf::Color(255, 230, 170));
    ui_.drawCenteredText(window_, "Level Select Lobby", w / 2.0f, 48.0f, 18, sf::Color(180, 190, 210));

    const sf::FloatRect listPanel{32.0f, 88.0f, 420.0f, 420.0f};
    ui_.drawPanel(window_, listPanel, sf::Color(24, 30, 44), 245.0f);
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
        ui_.drawText(window_, info.subtitle, rowBox.left + 12.0f, rowBox.top + 24.0f, 14,
            sf::Color(150, 160, 180));
    }

    const sf::FloatRect previewPanel{472.0f, 88.0f, 520.0f, 420.0f};
    ui_.drawPanel(window_, previewPanel, sf::Color(20, 26, 38), 245.0f);
    ui_.drawText(window_, "MAP PREVIEW", previewPanel.left + 16.0f, previewPanel.top + 12.0f, 20,
        sf::Color(220, 220, 220));

    const sf::FloatRect previewArea{
        previewPanel.left + kPreviewMargin,
        previewPanel.top + 40.0f,
        previewPanel.width - kPreviewMargin * 2.0f,
        previewPanel.height - 52.0f
    };
    sf::RectangleShape previewBg({previewArea.width, previewArea.height});
    previewBg.setPosition(previewArea.left, previewArea.top);
    previewBg.setFillColor(sf::Color(12, 14, 20));
    window_.draw(previewBg);
    drawMapPreview(window_, previewArea);

    const std::string detail = std::string(renderWorld_.levelName) + "  |  Gems: "
        + std::to_string(renderWorld_.totalGems);
    ui_.drawCenteredText(window_, detail, previewPanel.left + previewPanel.width / 2.0f,
        previewPanel.top + previewPanel.height - 28.0f, 18, sf::Color(255, 220, 120));

    const sf::FloatRect statusPanel{32.0f, 524.0f, 960.0f, 96.0f};
    ui_.drawPanel(window_, statusPanel, sf::Color(26, 32, 48), 245.0f);

    const std::string roleLine = std::string("You: ") + roleDisplayName() + "     Players: "
        + std::to_string(renderWorld_.connectedCount) + "/2";
    ui_.drawText(window_, roleLine, statusPanel.left + 20.0f, statusPanel.top + 14.0f, 20, sf::Color(220, 220, 220));

    const bool fireReady = (renderWorld_.readyMask & 0x01) != 0;
    const bool waterReady = (renderWorld_.readyMask & 0x02) != 0;
    ui_.drawText(window_, std::string("Fire: ") + (fireReady ? "Ready" : "Waiting"), statusPanel.left + 20.0f,
        statusPanel.top + 42.0f, 18, fireReady ? sf::Color(255, 140, 80) : sf::Color(150, 150, 150));
    ui_.drawText(window_, std::string("Water: ") + (waterReady ? "Ready" : "Waiting"), statusPanel.left + 180.0f,
        statusPanel.top + 42.0f, 18, waterReady ? sf::Color(100, 180, 255) : sf::Color(150, 150, 150));

    ui_.drawText(window_, "[Up/Down] or [1-8] Select   [Enter] Ready   [Esc] Cancel ready",
        statusPanel.left + 360.0f, statusPanel.top + 42.0f, 16, sf::Color(160, 170, 190));

    if (renderWorld_.connectedCount < 2) {
        ui_.drawButton(window_, {760.0f, statusPanel.top + 12.0f, 220.0f, 72.0f}, "Wait Partner", false,
            sf::Color(120, 90, 40));
    } else if (!localReady_) {
        ui_.drawButton(window_, {760.0f, statusPanel.top + 12.0f, 220.0f, 72.0f}, "ENTER - Start", true,
            sf::Color(60, 130, 210));
    } else {
        ui_.drawButton(window_, {760.0f, statusPanel.top + 12.0f, 220.0f, 72.0f}, "Ready! (Esc cancel)", true,
            sf::Color(50, 140, 90));
    }
}

void GameClient::renderGameScreen() {
    drawMap(window_);

    if (renderWorld_.phase == GamePhase::Playing || renderWorld_.phase == GamePhase::Victory
        || renderWorld_.phase == GamePhase::GameOver) {
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
    ui_.drawPanel(window, {w / 2.0f - 220.0f, h / 2.0f - 80.0f, 440.0f, 160.0f}, sf::Color(20, 20, 30), 230.0f);
    ui_.drawCenteredText(window, "Connecting to server...", w / 2.0f, h / 2.0f - 30.0f, 28, sf::Color::White);
    ui_.drawCenteredText(window, host_, w / 2.0f, h / 2.0f + 10.0f, 20, sf::Color(180, 180, 180));
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

            sf::RectangleShape tile({TILE_SIZE * scale - 1.0f, TILE_SIZE * scale - 1.0f});
            tile.setPosition(offsetX + x * TILE_SIZE * scale, offsetY + y * TILE_SIZE * scale);
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

            sf::RectangleShape tile({TILE_SIZE - 1.0f, TILE_SIZE - 1.0f});
            tile.setPosition(x * TILE_SIZE, y * TILE_SIZE);
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
    } else {
        body.setFillColor(sf::Color(60, 140, 255));
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
    ui_.drawText(window,
        "Gems: " + std::to_string(totalGems) + "/" + std::to_string(renderWorld_.totalGems),
        16.0f, hudY + 16.0f, 20, sf::Color(255, 220, 80));

    const std::string levelLine = "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": "
        + renderWorld_.levelName;
    ui_.drawText(window, levelLine, 180.0f, hudY + 16.0f, 18, sf::Color(220, 220, 220));

    if (renderWorld_.phase == GamePhase::Playing) {
        const char* controlText = role_ == PlayerRole::Fire ? "WASD" : "Arrows";
        ui_.drawText(window, std::string(roleDisplayName()) + " [" + controlText + "]",
            500.0f, hudY + 16.0f, 18, sf::Color(180, 180, 180));
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
    const sf::Color panelColor = victory ? sf::Color(20, 60, 35) : sf::Color(70, 20, 20);
    ui_.drawPanel(window, {centerX - 260.0f, mapH / 2.0f - 140.0f, 520.0f, 280.0f}, panelColor, 235.0f);

    const std::string levelLine = "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": "
        + renderWorld_.levelName;

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
    ui_.drawCenteredText(window,
        "Gems: " + std::to_string(totalGems) + " / " + std::to_string(renderWorld_.totalGems),
        centerX, mapH / 2.0f + 28.0f, 22, sf::Color(255, 220, 100));

    ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 62.0f, 250.0f, 40.0f}, "R - Replay", true,
        victory ? sf::Color(50, 130, 90) : sf::Color(170, 70, 70));
    ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 110.0f, 250.0f, 40.0f}, "ESC - Lobby", true,
        sf::Color(90, 90, 120));

    if (victory && renderWorld_.levelIndex + 1 < renderWorld_.levelCount) {
        ui_.drawButton(window, {centerX - 125.0f, mapH / 2.0f + 158.0f, 250.0f, 40.0f}, "N - Next Level", true,
            sf::Color(60, 120, 200));
    }
}

const char* GameClient::roleDisplayName() const {
    return role_ == PlayerRole::Fire ? "Fire Boy" : "Water Girl";
}

sf::Color GameClient::tileColor(TileType type) const {
    switch (type) {
    case TileType::Solid: return sf::Color(70, 55, 45);
    case TileType::Lava: return sf::Color(220, 70, 30);
    case TileType::Water: return sf::Color(40, 110, 220);
    case TileType::FireDoor: return sf::Color(180, 60, 30);
    case TileType::WaterDoor: return sf::Color(40, 90, 180);
    case TileType::FireExit: return sf::Color(255, 140, 60);
    case TileType::WaterExit: return sf::Color(100, 180, 255);
    case TileType::Gem: return sf::Color(255, 220, 60);
    case TileType::Button: return sf::Color(160, 160, 80);
    default: return sf::Color::White;
    }
}

} // namespace fireice
