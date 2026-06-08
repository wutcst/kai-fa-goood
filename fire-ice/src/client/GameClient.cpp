#include "GameClient.hpp"
#include "LevelCatalog.hpp"
#include "LevelMapLayout.hpp"
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

} // namespace

bool GameClient::initialize(const std::string& host, PlayerRole preferredRole) {
    host_ = host;
    serverAddress_ = sf::IpAddress(host);
    if (serverAddress_ == sf::IpAddress::None) {
        serverAddress_ = sf::IpAddress::LocalHost;
    }
    preferredRole_ = preferredRole;
    role_ = preferredRole;
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
    std::snprintf(request.playerName, sizeof(request.playerName), "%s", roleName(preferredRole_));

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
        beginConnect();
        break;
    case 1:
        clientScreen_ = ClientScreen::Help;
        break;
    case 2:
        preferredRole_ = preferredRole_ == PlayerRole::Fire ? PlayerRole::Water : PlayerRole::Fire;
        role_ = preferredRole_;
        break;
    case 3:
        clientScreen_ = ClientScreen::Credits;
        break;
    case 4:
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
    if (levelIndex == loadedLevelIndex_) {
        return;
    }

    const LevelCatalog& catalog = LevelCatalog::instance();
    const std::string path = catalog.resolvePath(levelIndex);
    if (!map_.loadFromFile(path)) {
        std::cerr << "[Client] Failed to load level preview: " << path << std::endl;
        return;
    }

    const std::string visualPath = catalog.resolveVisualPath(levelIndex);
    if (!visualPath.empty() && tiledMap_.load(visualPath)) {
        tiledMap_.bake();
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
            connectRequested_ = false;
            clientScreen_ = ClientScreen::Title;
            slot_ = packet.slot;
            role_ = packet.role;
            preferredRole_ = packet.role;
            localReady_ = false;
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

    if (!connected_) {
        if (event.type == sf::Event::KeyPressed) {
            handleTitleInput(event);
        } else if (event.type == sf::Event::MouseMoved) {
            handleTitleMouseMove(event);
        } else if (event.type == sf::Event::MouseButtonPressed) {
            handleTitleMouseClick(event);
        }
        return;
    }

    if (renderWorld_.phase == GamePhase::Lobby) {
        if (event.type == sf::Event::MouseMoved) {
            lobbyHoverNode_ = levelNodeAtPosition(static_cast<float>(event.mouseMove.x),
                static_cast<float>(event.mouseMove.y));
        } else if (event.type == sf::Event::MouseButtonPressed) {
            handleLobbyMouseClick(event);
        } else if (event.type == sf::Event::MouseWheelScrolled) {
            handleLobbyMouseWheel(event);
        }
    }

    if (event.type != sf::Event::KeyPressed) {
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
            trySelectLevel(level);
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
            && renderWorld_.levelIndex + 1 < renderWorld_.levelCount
            && isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(renderWorld_.levelIndex + 1))) {
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
    window_.clear(sf::Color(12, 18, 14));

    if (!connected_) {
        renderTitleScreen();
        if (clientScreen_ == ClientScreen::Help) {
            renderHelpOverlay();
        } else if (clientScreen_ == ClientScreen::Credits) {
            renderCreditsOverlay();
        } else if (clientScreen_ == ClientScreen::Connecting) {
            drawConnectingScreen(window_);
        }
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

    const std::string roleLine = text::currentRolePrefix() + roleChineseName()
        + "     " + text::serverPrefix() + host_;
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
    return preferredRole_ == PlayerRole::Fire ? text::fireBoy().c_str() : text::waterGirl().c_str();
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
    sprite.setPosition(
        (static_cast<float>(winSize.x) - static_cast<float>(texture.getSize().x) * scale) / 2.0f,
        (static_cast<float>(winSize.y) - static_cast<float>(texture.getSize().y) * scale) / 2.0f);
    window.draw(sprite);
}

void GameClient::trySelectLevel(uint8_t index) {
    if (localReady_ || index >= renderWorld_.levelCount) {
        return;
    }
    if (!isLevelUnlocked(renderWorld_.unlockedMask, index)) {
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
        if (levelNodeLocalHitArea(i).contains(local)) {
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
    const int node = levelNodeAtPosition(static_cast<float>(event.mouseButton.x),
        static_cast<float>(event.mouseButton.y));
    if (node >= 0) {
        trySelectLevel(static_cast<uint8_t>(node));
    }
}

void GameClient::drawLevelPath(uint8_t fromIndex, uint8_t toIndex, bool unlocked) {
    const sf::Vector2f from = levelNodeLocalCenter(fromIndex);
    const sf::Vector2f to = levelNodeLocalCenter(toIndex);
    const sf::Vector2f delta = to - from;
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 1.0f) {
        return;
    }

    sf::RectangleShape line({length, unlocked ? 9.0f : 6.0f});
    line.setOrigin(0.0f, line.getSize().y / 2.0f);
    line.setPosition(from);
    line.setRotation(std::atan2(delta.y, delta.x) * 180.0f / 3.14159265f);
    line.setFillColor(unlocked ? sf::Color(196, 150, 72) : sf::Color(70, 62, 48, 180));
    window_.draw(line);
}

void GameClient::drawLevelNode(uint8_t index, bool selected, bool unlocked, bool completed) {
    const sf::Vector2f center = levelNodeLocalCenter(index);
    const float radius = kLevelNodeRadius;

    if (selected) {
        sf::CircleShape glow(radius + 12.0f);
        glow.setOrigin(radius + 12.0f, radius + 12.0f);
        glow.setPosition(center);
        glow.setFillColor(sf::Color(255, 220, 80, 70));
        glow.setOutlineThickness(3.0f);
        glow.setOutlineColor(sf::Color(255, 240, 120, 180));
        window_.draw(glow);
    }

    sf::ConvexShape diamond(4);
    diamond.setPoint(0, {center.x, center.y - radius});
    diamond.setPoint(1, {center.x + radius * 0.82f, center.y});
    diamond.setPoint(2, {center.x, center.y + radius});
    diamond.setPoint(3, {center.x - radius * 0.82f, center.y});

    if (!unlocked) {
        diamond.setFillColor(sf::Color(58, 54, 50));
        diamond.setOutlineColor(sf::Color(35, 32, 28));
    } else if (completed) {
        diamond.setFillColor(sf::Color(210, 185, 95));
        diamond.setOutlineColor(sf::Color(130, 100, 35));
    } else {
        diamond.setFillColor(sf::Color(150, 158, 168));
        diamond.setOutlineColor(sf::Color(95, 100, 110));
    }
    diamond.setOutlineThickness(selected ? 3.0f : 2.0f);
    window_.draw(diamond);

    if (!unlocked) {
        ui_.drawCenteredText(window_, "?", center.x, center.y - 14.0f, 24, sf::Color(110, 105, 100));
    } else {
        ui_.drawCenteredText(window_, std::to_string(index + 1), center.x, center.y - 14.0f, 22,
            completed ? sf::Color(70, 45, 10) : sf::Color(35, 40, 48));
        if (completed) {
            ui_.drawCenteredText(window_, "*", center.x + radius + 8.0f, center.y - 10.0f, 18,
                sf::Color(255, 230, 120));
        }
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
        ui_.drawText(window_, text::lobbyScrollHint(), mapArea.left + mapArea.width - 168.0f, mapArea.top + 12.0f,
            13, sf::Color(220, 200, 160));
    }

    const sf::Vector2u winSize = window_.getSize();
    const sf::View previousView = window_.getView();
    sf::View contentView(sf::FloatRect(0.0f, levelMapScrollY_, viewport.width, viewport.height));
    contentView.setViewport(sf::FloatRect(
        viewport.left / static_cast<float>(winSize.x),
        viewport.top / static_cast<float>(winSize.y),
        viewport.width / static_cast<float>(winSize.x),
        viewport.height / static_cast<float>(winSize.y)));
    window_.setView(contentView);

    const uint8_t levelCount = renderWorld_.levelCount;
    for (uint8_t i = 0; i + 1 < levelCount; ++i) {
        const bool pathUnlocked = isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(i + 1));
        drawLevelPath(i, static_cast<uint8_t>(i + 1), pathUnlocked);
    }

    for (uint8_t i = 0; i < levelCount; ++i) {
        const bool unlocked = isLevelUnlocked(renderWorld_.unlockedMask, i);
        const bool completed = isLevelCompleted(renderWorld_.completedMask, i);
        const bool selected = i == renderWorld_.levelIndex;
        const bool hovered = static_cast<int>(i) == lobbyHoverNode_;
        drawLevelNode(i, selected || hovered, unlocked, completed);
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

void GameClient::renderLobbyScreen() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);

    const sf::FloatRect viewport = levelMapViewportRect();
    const float nodeY = levelNodeLocalCenter(renderWorld_.levelIndex).y;
    const float nodeTop = nodeY - kLevelNodeRadius - 8.0f;
    const float nodeBottom = nodeY + kLevelNodeRadius + 8.0f;
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
    ui_.drawCenteredText(window_, text::lobbySelectHint(), w / 2.0f, 48.0f, 16, sf::Color(200, 190, 170));

    drawLevelProgressMap();

    const sf::FloatRect detailPanel{740.0f, 88.0f, 260.0f, 420.0f};
    ui_.drawPanel(window_, detailPanel, sf::Color(36, 30, 24, 230), 230.0f);

    const LevelCatalog& catalog = LevelCatalog::instance();
    const LevelInfo& selectedInfo = catalog.at(renderWorld_.levelIndex);
    const bool selectedUnlocked = isLevelUnlocked(renderWorld_.unlockedMask, renderWorld_.levelIndex);
    const bool selectedCompleted = isLevelCompleted(renderWorld_.completedMask, renderWorld_.levelIndex);

    ui_.drawText(window_, "Level " + std::to_string(renderWorld_.levelIndex + 1), detailPanel.left + 16.0f,
        detailPanel.top + 14.0f, 24, sf::Color(255, 230, 170));
    ui_.drawText(window_, selectedInfo.title, detailPanel.left + 16.0f, detailPanel.top + 44.0f, 18,
        sf::Color(230, 225, 210));
    ui_.drawText(window_, selectedInfo.subtitle, detailPanel.left + 16.0f, detailPanel.top + 70.0f, 14,
        sf::Color(180, 170, 155));

    if (!selectedUnlocked) {
        ui_.drawText(window_, text::lobbyLockedHint(), detailPanel.left + 16.0f, detailPanel.top + 98.0f, 16,
            sf::Color(255, 140, 100));
    } else if (selectedCompleted) {
        ui_.drawText(window_, text::lobbyClearedHint(), detailPanel.left + 16.0f, detailPanel.top + 98.0f, 16,
            sf::Color(255, 220, 100));
    }

    const sf::FloatRect previewArea{
        detailPanel.left + 12.0f,
        detailPanel.top + 124.0f,
        detailPanel.width - 24.0f,
        detailPanel.height - 160.0f
    };
    sf::RectangleShape previewBg({previewArea.width, previewArea.height});
    previewBg.setPosition(previewArea.left, previewArea.top);
    previewBg.setFillColor(sf::Color(18, 16, 14));
    window_.draw(previewBg);
    if (selectedUnlocked) {
        drawMapPreview(window_, previewArea);
    }

    const std::string detail = std::string(renderWorld_.levelName) + "  |  Gems: "
        + std::to_string(renderWorld_.totalGems);
    ui_.drawCenteredText(window_, detail, detailPanel.left + detailPanel.width / 2.0f,
        detailPanel.top + detailPanel.height - 28.0f, 16, sf::Color(255, 220, 120));

    const sf::FloatRect statusPanel{24.0f, 524.0f, 976.0f, 96.0f};
    ui_.drawPanel(window_, statusPanel, sf::Color(36, 30, 24, 220), 220.0f);

    const std::string roleLine = std::string("You: ") + roleDisplayName() + "     Players: "
        + std::to_string(renderWorld_.connectedCount) + "/2";
    ui_.drawText(window_, roleLine, statusPanel.left + 20.0f, statusPanel.top + 14.0f, 20, sf::Color(220, 220, 220));

    const bool fireReady = (renderWorld_.readyMask & 0x01) != 0;
    const bool waterReady = (renderWorld_.readyMask & 0x02) != 0;
    ui_.drawText(window_, std::string("Fire: ") + (fireReady ? "Ready" : "Waiting"), statusPanel.left + 20.0f,
        statusPanel.top + 42.0f, 18, fireReady ? sf::Color(255, 140, 80) : sf::Color(150, 150, 150));
    ui_.drawText(window_, std::string("Water: ") + (waterReady ? "Ready" : "Waiting"), statusPanel.left + 180.0f,
        statusPanel.top + 42.0f, 18, waterReady ? sf::Color(100, 180, 255) : sf::Color(150, 150, 150));

    ui_.drawText(window_, text::lobbyControlsHint(), statusPanel.left + 360.0f, statusPanel.top + 42.0f, 16,
        sf::Color(180, 170, 150));

    const sf::FloatRect playArea{760.0f, statusPanel.top + 8.0f, 220.0f, 72.0f};
    if (assets_.hasButtons()) {
        if (!localReady_) {
            ui_.drawImageButtonWithHint(window_, playArea, assets_.playButton(), "[Enter] Start", true, true);
        } else {
            ui_.drawImageButtonWithHint(window_, playArea, assets_.playButton(), "Ready! [Esc]", true, true);
        }
    } else if (!localReady_) {
        ui_.drawButton(window_, playArea, "ENTER - Start", true, sf::Color(60, 130, 210));
    } else {
        ui_.drawButton(window_, playArea, "Ready! (Esc cancel)", true, sf::Color(50, 140, 90));
    }
}

void GameClient::renderGameScreen() {
    const float mapW = static_cast<float>(map_.width()) * TILE_SIZE;
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;

    if (assets_.ready()) {
        sf::Sprite bg(assets_.gameBackground());
        const sf::Vector2u texSize = assets_.gameBackground().getSize();
        if (texSize.x > 0 && texSize.y > 0) {
            bg.setScale(mapW / static_cast<float>(texSize.x), mapH / static_cast<float>(texSize.y));
            bg.setPosition(0.0f, 0.0f);
            window_.draw(bg);
        }
    }

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

            if (type == TileType::FireDoor || type == TileType::WaterDoor
                || type == TileType::FireExit || type == TileType::WaterExit
                || type == TileType::Button) {
                sf::RectangleShape tile({tileSize, tileSize});
                tile.setPosition(px, py);
                tile.setFillColor(tileColor(type));
                window.draw(tile);
            }
        }
    }
}

void GameClient::drawMap(sf::RenderWindow& window) const {
    if (tiledMap_.ready()) {
        tiledMap_.drawStatic(window);
        drawDynamicTiles(window);
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
        const sf::Texture& screenTex = victory
            ? ((renderWorld_.players[0].gems + renderWorld_.players[1].gems >= renderWorld_.totalGems
                   && renderWorld_.totalGems > 0)
                  ? assets_.winScreen()
                  : assets_.winScreenPartial())
            : assets_.loseScreen();

        sf::Sprite screen(screenTex);
        const float targetW = 520.0f;
        const float targetH = 280.0f;
        const float scale = std::min(targetW / static_cast<float>(screenTex.getSize().x),
            targetH / static_cast<float>(screenTex.getSize().y));
        screen.setScale(scale, scale);
        screen.setPosition(centerX - screenTex.getSize().x * scale / 2.0f, mapH / 2.0f - screenTex.getSize().y * scale / 2.0f);
        window.draw(screen);
    } else {
        const sf::Color panelColor = victory ? sf::Color(20, 60, 35) : sf::Color(70, 20, 20);
        ui_.drawPanel(window, {centerX - 260.0f, mapH / 2.0f - 140.0f, 520.0f, 280.0f}, panelColor, 235.0f);
    }

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

    const float btnW = 150.0f;
    const float btnH = 52.0f;
    const float gap = 28.0f;
    const float rowY = mapH / 2.0f + 130.0f;
    const bool hasNextLevel = victory && renderWorld_.levelIndex + 1 < renderWorld_.levelCount
        && isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(renderWorld_.levelIndex + 1));

    if (assets_.hasButtons()) {
        if (hasNextLevel) {
            const float totalW = btnW * 3.0f + gap * 2.0f;
            const float startX = centerX - totalW / 2.0f;
            ui_.drawImageButtonWithHint(window, {startX, rowY, btnW, btnH}, assets_.retryButton(), "[R] Retry");
            ui_.drawImageButtonWithHint(window, {startX + btnW + gap, rowY, btnW, btnH}, assets_.continueButton(),
                "[N] Next");
            ui_.drawImageButtonWithHint(window, {startX + (btnW + gap) * 2.0f, rowY, btnW, btnH},
                assets_.menuButton(), "[Esc] Lobby");
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
