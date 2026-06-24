#include "GameClient.hpp"
#include "ClientVisuals.hpp"
#include "LevelCatalog.hpp"
#include "LevelMapLayout.hpp"
#include "LevelMechanics.hpp"
#include "LevelProgress.hpp"
#include "LocalGameSession.hpp"
#include "LocaleText.hpp"
#include "Paths.hpp"
#include "Physics.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

std::filesystem::file_time_type assetFileTime(const std::string& assetPath) {
    const std::string resolved = fireice::resolveAssetPath(assetPath);
    if (!std::filesystem::exists(resolved)) {
        return std::filesystem::file_time_type::min();
    }
    return std::filesystem::last_write_time(resolved);
}

}  // namespace

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fireice {

namespace {

#ifdef _WIN32
void setWindowTitleUtf8(sf::Window& window, const std::string& title) {
    window.setTitle(title);
    if (HWND hwnd = static_cast<HWND>(window.getSystemHandle())) {
        const int wideLen = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
        if (wideLen > 0) {
            std::vector<wchar_t> wide(static_cast<std::size_t>(wideLen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide.data(), wideLen);
            SetWindowTextW(hwnd, wide.data());
        }
    }
}
#else
void setWindowTitleUtf8(sf::Window& window, const std::string& title) {
    window.setTitle(title);
}
#endif

constexpr float kPreviewMargin = 12.0f;
constexpr float kTitleFooterHeight = 86.0f;

// 标题界面各区域位置与尺寸，按窗口比例缩放
struct TitleLayoutMetrics {
    float headerY = 0.f;
    float menuStartY = 0.f;
    float menuStep = 0.f;
    float menuItemWidth = 0.f;
    float menuMidY = 0.f;
    float footerTop = 0.f;
    float footerHeight = 0.f;
};

inline unsigned titleMenuFontSize() {
    return static_cast<unsigned>(std::clamp(36.0f * static_cast<float>(LOBBY_WINDOW_WIDTH) / 1280.0f, 36.0f, 52.0f));
}

TitleLayoutMetrics titleLayoutMetrics() {
    TitleLayoutMetrics metrics{};
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);
    metrics.headerY = lobbyScaled(78.0f);
    metrics.menuStep = lobbyScaled(54.0f);
    metrics.menuItemWidth = lobbyScaled(500.0f);
    metrics.footerHeight = lobbyScaled(kTitleFooterHeight);
    metrics.footerTop = h - metrics.footerHeight;

    constexpr float kTitleCharScale = 3.2f;
    const float charH = 32.0f * kTitleCharScale;
    const float charBaseY = metrics.footerTop - charH - lobbyScaled(12.0f);

    constexpr int kMenuItemCount = 5;
    const float menuBlockH = metrics.menuStep * static_cast<float>(kMenuItemCount - 1) +
                             static_cast<float>(titleMenuFontSize()) + lobbyScaled(24.0f);
    metrics.menuStartY = charBaseY + charH - menuBlockH - lobbyScaled(6.0f);
    metrics.menuMidY = metrics.menuStartY + metrics.menuStep * 2.0f + static_cast<float>(titleMenuFontSize()) * 0.45f;
    return metrics;
}

inline float titleMenuItemStep() {
    return titleLayoutMetrics().menuStep;
}

inline float titleMenuStartY() {
    return titleLayoutMetrics().menuStartY;
}

inline float titleCenterX() {
    return static_cast<float>(LOBBY_WINDOW_WIDTH) * 0.5f;
}

// 横向平铺绘制单视差层，scrollPx 控制滚动偏移
void drawTiledParallaxLayer(sf::RenderTarget& target, const sf::Texture& texture, float scrollPx, float viewW,
                            float viewH, float yOffset = 0.f) {
    const float texW = static_cast<float>(texture.getSize().x);
    const float texH = static_cast<float>(texture.getSize().y);
    if (texW <= 0.f || texH <= 0.f) {
        return;
    }

    const float scale = viewH / texH;
    const float scaledW = texW * scale;
    const float wrap = scaledW > 0.f ? scaledW : 1.f;
    const float offset = std::fmod(scrollPx, wrap);

    sf::Sprite sprite(texture);
    sprite.setScale(scale, scale);
    for (float x = -offset - wrap; x < viewW + wrap; x += wrap) {
        sprite.setPosition(x, yOffset);
        target.draw(sprite);
    }
}

constexpr float kLobbyBottomBarLeft = 32.0f;
constexpr float kLobbyBottomBarHeight = 64.0f;
constexpr float kLobbyPlayBtnWidth = 220.0f;
constexpr float kLobbyPlayBtnHeight = 48.0f;
constexpr float kLobbyPlayBtnMargin = 16.0f;

inline float lobbyBottomBarHeight() {
    return lobbyScaled(kLobbyBottomBarHeight);
}

inline float lobbyPlayBtnWidth() {
    return lobbyScaled(kLobbyPlayBtnWidth);
}

inline float lobbyPlayBtnHeight() {
    return lobbyScaled(kLobbyPlayBtnHeight);
}

inline float lobbyBottomBarTop() {
    return static_cast<float>(LOBBY_WINDOW_HEIGHT) - lobbyBottomBarHeight() - lobbyScaled(16.0f);
}

inline float lobbyBottomBarWidth() {
    return static_cast<float>(LOBBY_WINDOW_WIDTH) - kLobbyBottomBarLeft * 2.0f;
}

struct PauseMenuMetrics {
    float panelTop = 0.0f;
    float panelDrawW = 0.0f;
    float panelDrawH = 0.0f;
    sf::FloatRect menuBtn;
    sf::FloatRect retryBtn;
    sf::FloatRect continueBtn;
};

PauseMenuMetrics computePauseMenuMetrics(float mapH, float centerX) {
    PauseMenuMetrics metrics;
    constexpr float targetW = 640.0f;
    constexpr float targetH = 440.0f;
    metrics.panelDrawW = targetW;
    metrics.panelDrawH = targetH;
    metrics.panelTop = mapH / 2.0f - targetH / 2.0f;
    const float panelLeft = centerX - targetW / 2.0f;
    const float row1Y = metrics.panelTop + targetH * 0.535f;
    const float row2Y = metrics.panelTop + targetH * 0.705f;
    const float btnW = targetW * 0.36f;
    const float btnH = targetH * 0.105f;
    const float gap = targetW * 0.06f;
    metrics.menuBtn = {panelLeft + gap, row1Y, btnW, btnH};
    metrics.retryBtn = {panelLeft + targetW - gap - btnW, row1Y, btnW, btnH};
    metrics.continueBtn = {centerX - btnW / 2.0f, row2Y, btnW, btnH};
    return metrics;
}

struct PauseMenuPanelDraw {
    PauseMenuMetrics metrics;
    float centerX = 0.0f;
    float panelLeft = 0.0f;
    float panelTop = 0.0f;
    float drawW = 0.0f;
    float drawH = 0.0f;
    bool usesTexture = false;

    sf::FloatRect mapRect(const sf::FloatRect& rect) const {
        if (!usesTexture) {
            return rect;
        }
        const float baseLeft = centerX - metrics.panelDrawW / 2.0f;
        return {panelLeft + (rect.left - baseLeft) / metrics.panelDrawW * drawW,
                panelTop + (rect.top - metrics.panelTop) / metrics.panelDrawH * drawH,
                rect.width / metrics.panelDrawW * drawW, rect.height / metrics.panelDrawH * drawH};
    }

    sf::FloatRect menuBtn() const { return mapRect(metrics.menuBtn); }
    sf::FloatRect retryBtn() const { return mapRect(metrics.retryBtn); }
    sf::FloatRect continueBtn() const { return mapRect(metrics.continueBtn); }
};

PauseMenuPanelDraw computePauseMenuPanelDraw(float mapH, float centerX, const sf::Texture* texture) {
    PauseMenuPanelDraw draw;
    draw.centerX = centerX;
    draw.metrics = computePauseMenuMetrics(mapH, centerX);
    if (texture != nullptr && texture->getSize().x > 0 && texture->getSize().y > 0) {
        draw.usesTexture = true;
        const float scale = std::min(draw.metrics.panelDrawW / static_cast<float>(texture->getSize().x),
                                     draw.metrics.panelDrawH / static_cast<float>(texture->getSize().y));
        draw.drawW = static_cast<float>(texture->getSize().x) * scale;
        draw.drawH = static_cast<float>(texture->getSize().y) * scale;
        draw.panelLeft = centerX - draw.drawW / 2.0f;
        draw.panelTop = mapH / 2.0f - draw.drawH / 2.0f;
    }
    return draw;
}

void drawPauseMenuTexture(sf::RenderWindow& window, const PauseMenuPanelDraw& draw, const sf::Texture& texture) {
    sf::Sprite screen(texture);
    const float scale = draw.drawW / static_cast<float>(texture.getSize().x);
    screen.setScale(scale, scale);
    screen.setPosition(draw.panelLeft, draw.panelTop);
    window.draw(screen);
}

float resultOverlayMapHeight(int mapRows, int tiledMapRows, bool tiledReady, uint8_t levelIndex) {
    int overlayRows = mapRows;
    if (levelIndex == 0 && tiledReady) {
        overlayRows = std::max(overlayRows, tiledMapRows);
    }
    return static_cast<float>(overlayRows) * TILE_SIZE;
}

void drawPauseStyleMenuPanel(sf::RenderWindow& window, const fireice::UiHelper& ui, float centerX, float mapH) {
    const PauseMenuMetrics metrics = computePauseMenuMetrics(mapH, centerX);

    ui.drawPanel(window,
                 {centerX - metrics.panelDrawW / 2.0f, metrics.panelTop, metrics.panelDrawW, metrics.panelDrawH},
                 sf::Color(180, 180, 180), 235.0f);
    ui.drawOutlinedCenteredText(window, fireice::text::pauseTitle(), centerX,
                                metrics.panelTop + metrics.panelDrawH * 0.18f, 42, sf::Color(255, 220, 80),
                                sf::Color(40, 30, 20), 3.0f);
    ui.drawButton(window, metrics.menuBtn, fireice::text::pauseMenuButton(), true, sf::Color(120, 120, 130));
    ui.drawButton(window, metrics.retryBtn, fireice::text::resultRetryButton(), true, sf::Color(180, 120, 50));
    ui.drawButton(window, metrics.continueBtn, fireice::text::pauseContinueButton(), true, sf::Color(80, 150, 90));
}

struct GameOverLayout {
    float panelTop = 0.0f;
    float panelDrawW = 0.0f;
    float panelDrawH = 0.0f;
    float titleY = 0.0f;
    float scoreY = 0.0f;
    float uiScale = 1.0f;
    sf::FloatRect menuBtn;
    sf::FloatRect retryBtn;
};

inline float gameOverlayUiScale(float windowW) {
    return std::clamp(windowW / 1280.0f, 1.0f, 1.85f);
}

GameOverLayout computeGameOverLayout(float mapH, float centerX, float windowW) {
    GameOverLayout layout;
    layout.uiScale = gameOverlayUiScale(windowW);

    layout.panelDrawW = 640.0f * layout.uiScale;
    layout.panelDrawH = 500.0f * layout.uiScale;
    layout.panelTop = mapH / 2.0f - layout.panelDrawH / 2.0f;
    layout.titleY = layout.panelTop + layout.panelDrawH * 0.20f;
    layout.scoreY = layout.panelTop + layout.panelDrawH * 0.38f;

    const float btnW = 210.0f * layout.uiScale;
    const float btnH = 68.0f * layout.uiScale;
    const float gap = 44.0f * layout.uiScale;
    const float rowY = layout.panelTop + layout.panelDrawH * 0.60f;
    layout.menuBtn = {centerX - btnW - gap / 2.0f, rowY, btnW, btnH};
    layout.retryBtn = {centerX + gap / 2.0f, rowY, btnW, btnH};
    return layout;
}

struct VictoryLayout {
    float panelTop = 0.0f;
    float panelDrawW = 0.0f;
    float panelDrawH = 0.0f;
    float titleY = 0.0f;
    float scoreY = 0.0f;
    float uiScale = 1.0f;
    sf::FloatRect menuBtn;
    sf::FloatRect retryBtn;
    sf::FloatRect nextBtn;
};

VictoryLayout computeVictoryLayout(float mapH, float centerX, float windowW) {
    VictoryLayout layout;
    layout.uiScale = gameOverlayUiScale(windowW);

    layout.panelDrawW = 640.0f * layout.uiScale;
    layout.panelDrawH = 440.0f * layout.uiScale;
    layout.panelTop = mapH / 2.0f - layout.panelDrawH / 2.0f;
    layout.titleY = layout.panelTop + layout.panelDrawH * 0.13f;
    layout.scoreY = layout.panelTop + layout.panelDrawH * 0.36f;

    const float btnW = 210.0f * layout.uiScale;
    const float btnH = 68.0f * layout.uiScale;
    const float gap = 44.0f * layout.uiScale;
    const float row1Y = layout.panelTop + layout.panelDrawH * 0.54f;
    const float row2Y = layout.panelTop + layout.panelDrawH * 0.74f;
    layout.menuBtn = {centerX - btnW - gap / 2.0f, row1Y, btnW, btnH};
    layout.retryBtn = {centerX + gap / 2.0f, row1Y, btnW, btnH};
    layout.nextBtn = {centerX - btnW / 2.0f, row2Y, btnW, btnH};
    return layout;
}

void drawVictoryLabelButton(sf::RenderWindow& window, const fireice::UiHelper& ui, const sf::FloatRect& area,
                            const sf::Texture& texture, const std::string& label, bool enabled, float uiScale,
                            sf::Color tint) {
    if (texture.getSize().x > 0 && texture.getSize().y > 0) {
        sf::Sprite sprite(texture);
        sprite.setScale(area.width / static_cast<float>(texture.getSize().x),
                        area.height / static_cast<float>(texture.getSize().y));
        sprite.setPosition(area.left, area.top);
        sprite.setColor(enabled ? tint : sf::Color(220, 220, 218, 170));
        window.draw(sprite);
    }

    const unsigned fontSize = static_cast<unsigned>(22.0f * uiScale);
    const float textY = area.top + (area.height - static_cast<float>(fontSize)) * 0.40f;
    ui.drawOutlinedCenteredText(window, label, area.left + area.width / 2.0f, textY, fontSize,
                                enabled ? sf::Color(248, 246, 240) : sf::Color(200, 198, 192), sf::Color(110, 95, 80),
                                1.5f);
}

constexpr float kRoomPanelRefW = 300.0f;
constexpr float kRoomPanelRefH = 360.0f;
constexpr float kRoomPanelGapRef = 24.0f;
constexpr float kRoomBottomBarHRef = 80.0f;
constexpr float kRoomActionBtnWRef = 260.0f;
constexpr float kRoomActionBtnHRef = 56.0f;
constexpr float kRoomActionBtnGapRef = 28.0f;

inline float roomPanelWidth() {
    return lobbyScaled(kRoomPanelRefW);
}

inline float roomPanelHeight() {
    return lobbyScaled(kRoomPanelRefH);
}

inline float roomPanelGap() {
    return lobbyScaled(kRoomPanelGapRef);
}

inline float roomHeaderHeight() {
    return lobbyScaled(110.0f);  // 等待室顶栏加高，给标题留出居中空间
}

inline float roomPanelTop() {
    return roomHeaderHeight() + lobbyScaled(48.0f);
}

inline float roomBottomBarHeight() {
    return lobbyScaled(kRoomBottomBarHRef);
}

sf::FloatRect lobbyBottomBarRect() {
    return {kLobbyBottomBarLeft, lobbyBottomBarTop(), lobbyBottomBarWidth(), lobbyBottomBarHeight()};
}

sf::FloatRect lobbyPlayButtonRect() {
    const sf::FloatRect bar = lobbyBottomBarRect();
    return {bar.left + bar.width - lobbyScaled(kLobbyPlayBtnMargin) - lobbyPlayBtnWidth(),
            bar.top + (bar.height - lobbyPlayBtnHeight()) / 2.0f, lobbyPlayBtnWidth(), lobbyPlayBtnHeight()};
}

float roomBottomBarY() {
    return static_cast<float>(LOBBY_WINDOW_HEIGHT) - roomBottomBarHeight();
}

std::array<sf::FloatRect, 3> roomActionButtonAreas() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float btnW = lobbyScaled(kRoomActionBtnWRef);
    const float btnH = lobbyScaled(kRoomActionBtnHRef);
    const float gap = lobbyScaled(kRoomActionBtnGapRef);
    const float totalW = btnW * 3.0f + gap * 2.0f;
    const float startX = (w - totalW) / 2.0f;
    const float btnY = roomBottomBarY() + (roomBottomBarHeight() - btnH) / 2.0f;
    return {
        sf::FloatRect{startX, btnY, btnW, btnH},
        sf::FloatRect{startX + btnW + gap, btnY, btnW, btnH},
        sf::FloatRect{startX + (btnW + gap) * 2.0f, btnY, btnW, btnH},
    };
}

}  // namespace

bool GameClient::initialize(const std::string& host, PlayerRole preferredRole, bool autoConnect, bool autoSolo) {
    host_ = host.empty() ? DEFAULT_SERVER_HOST : host;
    network_.setServerHost(host_);
    preferredRole_ = preferredRole;
    role_ = preferredRole;
    localServerMode_ = (host_ == "127.0.0.1" || host_ == "localhost");
    playerName_ = preferredRole == PlayerRole::Fire ? "NinjaFrog" : "PinkMan";
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

    if (!network_.bindLocal()) {
        return false;
    }

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

    if (localServerMode_) {
        std::cout << "[Client] Title screen ready. Local server: " << host_ << ":" << SERVER_PORT
                  << "  Role: " << roleName(preferredRole_) << std::endl;
    } else {
        std::cout << "[Client] Title screen ready. Public server: " << DEFAULT_SERVER_HOST << ":" << SERVER_PORT
                  << "  Role: " << roleName(preferredRole_) << std::endl;
    }

    if (autoSolo) {
        startSoloPlay();
    } else if (autoConnect && host_ != DEFAULT_SERVER_HOST) {
        std::cout << "[Client] Auto-connecting to " << host_ << "..." << std::endl;
        quickJoin();
    }

    return true;
}

void applyFullWindowView(sf::RenderWindow& window, unsigned width, unsigned height) {
    sf::View view(sf::FloatRect(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)));
    view.setViewport(sf::FloatRect(0.0f, 0.0f, 1.0f, 1.0f));
    window.setView(view);
}

void GameClient::useTitleLayout() {
    lobbyLayout_ = true;
    const sf::Vector2u size(LOBBY_WINDOW_WIDTH, LOBBY_WINDOW_HEIGHT);
    if (!window_.isOpen()) {
        window_.create(sf::VideoMode(size.x, size.y), text::windowTitle());
    } else {
        window_.setSize(size);
    }
    setWindowTitleUtf8(window_, text::windowTitle());
    window_.setVerticalSyncEnabled(true);
    applyFullWindowView(window_, size.x, size.y);
}

void GameClient::startSoloPlay() {
    if (soloMode_ || connected_) {
        return;
    }

    if (connectRequested_) {
        connectRequested_ = false;
    }

    soloSession_ = std::make_unique<LocalGameSession>();
    if (!soloSession_->start(preferredRole_, playerName_)) {
        soloSession_.reset();
        std::cerr << "[Client] Failed to start solo session" << std::endl;
        return;
    }

    soloMode_ = true;
    connected_ = true;
    slot_ = soloSession_->playerSlot();
    role_ = preferredRole_;
    host_ = "本地";
    localReady_ = false;
    roomAnimTimer_ = 0.0f;

    syncWorldFromSession();
    useLobbyLayout();
    updateMusic(GamePhase::Lobby);
    scrollLevelMapToNode(renderWorld_.levelIndex);

    std::cout << "[Client] Solo mode started as " << roleName(role_) << " (slot " << static_cast<int>(slot_) << ")"
              << std::endl;
}

void GameClient::stopSoloPlay() {
    if (!soloMode_) {
        return;
    }

    soloSession_.reset();
    soloMode_ = false;
    connected_ = false;
    localReady_ = false;
    clientScreen_ = ClientScreen::Title;
    useTitleLayout();
    std::cout << "[Client] Solo mode stopped" << std::endl;
}

void GameClient::syncWorldFromSession() {
    if (!soloSession_) {
        return;
    }

    const GamePhase previousPhase = world_.phase;
    const uint8_t previousLobbyStep = world_.lobbyStep;
    const uint8_t previousLevelIndex = world_.levelIndex;

    world_ = soloSession_->world();
    renderWorld_ = world_;

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t globalLevelIndex =
        catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, std::max(uint8_t{1}, renderWorld_.connectedCount));
    if (globalLevelIndex != loadedLevelIndex_) {
        handleLevelChange(renderWorld_.levelIndex, !lobbyLayout_);
    }

    if (renderWorld_.lobbyStep == 0 && previousLobbyStep == 1) {
        localReady_ = false;
        levelMapScrollY_ = 0.0f;
    }
    if (renderWorld_.lobbyStep == 1 && previousLobbyStep == 0) {
        levelMapScrollY_ = 0.0f;
        scrollLevelMapToNode(renderWorld_.levelIndex);
    }
    if (renderWorld_.lobbyStep == 1 && renderWorld_.levelIndex != previousLevelIndex) {
        localReady_ = false;
        scrollLevelMapToNode(renderWorld_.levelIndex);
    }

    if (previousPhase != renderWorld_.phase) {
        onPhaseChanged(previousPhase, renderWorld_.phase);
    }
}

void GameClient::updateSoloSession(float dt) {
    if (!soloMode_ || !soloSession_) {
        return;
    }

    soloSession_->simulate(dt);
    syncWorldFromSession();
}

void GameClient::quickJoin() {
    typedRoomCode_.clear();
    if (!localServerMode_) {
        usePublicServer();
    }
    beginConnect();
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
    std::cout << "[Client] Connecting to " << network_.serverAddress().toString() << ":" << SERVER_PORT << std::endl;
}

void GameClient::sendConnectRequest() {
    ConnectRequestPacket request{};
    request.preferredRole = preferredRole_;
    std::snprintf(request.playerName, sizeof(request.playerName), "%s", playerName_.c_str());
    if (!typedRoomCode_.empty()) {
        std::snprintf(request.roomCode, sizeof(request.roomCode), "%s", typedRoomCode_.c_str());
    }

    network_.sendConnectRequest(request);
    lastConnectRetry_ = std::chrono::steady_clock::now();
}

std::vector<sf::FloatRect> GameClient::titleMenuHitAreas() const {
    const TitleLayoutMetrics layout = titleLayoutMetrics();
    const auto& items = text::titleMenuItems();
    const float rowH = static_cast<float>(titleMenuFontSize()) + lobbyScaled(22.0f);
    std::vector<sf::FloatRect> areas;
    areas.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        const float y = layout.menuStartY + static_cast<float>(i) * layout.menuStep;
        areas.push_back(
            {titleCenterX() - layout.menuItemWidth * 0.5f, y - lobbyScaled(8.0f), layout.menuItemWidth, rowH});
    }
    return areas;
}

void GameClient::handleTitleMenuSelect(int index) {
    switch (index) {
        case 0:
            startSoloPlay();
            break;
        case 1:
            quickJoin();
            break;
        case 2:
            typedRoomCode_.clear();
            clientScreen_ = ClientScreen::JoinRoom;
            break;
        case 3:
            clientScreen_ = ClientScreen::Help;
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

    if (clientScreen_ == ClientScreen::JoinRoom) {
        if (event.key.code == sf::Keyboard::Escape) {
            typedRoomCode_.clear();
            clientScreen_ = ClientScreen::Title;
        } else if (event.key.code == sf::Keyboard::Enter) {
            if (typedRoomCode_.size() == 6) {
                if (!localServerMode_) {
                    usePublicServer();
                }
                beginConnect();
            }
        } else if (event.key.code == sf::Keyboard::Backspace && !typedRoomCode_.empty()) {
            typedRoomCode_.pop_back();
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

    const sf::Vector2f mouse = mapMousePos(event.mouseMove.x, event.mouseMove.y);
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

    const sf::Vector2f mouse = mapMousePos(event.mouseButton.x, event.mouseButton.y);
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
        window_.create(sf::VideoMode(size.x, size.y), text::windowTitle());
    } else {
        window_.setSize(size);
    }
    setWindowTitleUtf8(window_, text::windowTitle() + " - Lobby");
    window_.setVerticalSyncEnabled(true);
    applyFullWindowView(window_, size.x, size.y);
}

void GameClient::useGameLayout() {
    lobbyLayout_ = false;
    int mapW = map_.width();
    int mapH = map_.height();
    // Level 1 apples are object pickups; keep window/overlay aligned with the visual map.
    if (loadedLevelIndex_ == 0 && tiledMap_.ready()) {
        mapW = std::max(mapW, tiledMap_.mapWidth());
        mapH = std::max(mapH, tiledMap_.mapHeight());
    }
    const unsigned width = static_cast<unsigned>(std::max(960, mapW * static_cast<int>(TILE_SIZE)));
    const unsigned height = static_cast<unsigned>(std::max(540, mapH * static_cast<int>(TILE_SIZE)));
    window_.setSize({width, height});
    setWindowTitleUtf8(window_, text::windowTitle());
    applyFullWindowView(window_, width, height);
}

void GameClient::ensureLayoutView() {
    const bool showLobbyUi = !connected_ || renderWorld_.phase == GamePhase::Lobby;
    if (showLobbyUi) {
        const sf::Vector2u winSize = window_.getSize();
        const sf::View view = window_.getView();
        const sf::FloatRect viewport = view.getViewport();
        const bool sizeMismatch = winSize.x != LOBBY_WINDOW_WIDTH || winSize.y != LOBBY_WINDOW_HEIGHT ||
                                  static_cast<unsigned>(view.getSize().x + 0.5f) != LOBBY_WINDOW_WIDTH ||
                                  static_cast<unsigned>(view.getSize().y + 0.5f) != LOBBY_WINDOW_HEIGHT;
        const bool viewportMismatch =
            viewport.left != 0.0f || viewport.top != 0.0f || viewport.width != 1.0f || viewport.height != 1.0f;
        if (sizeMismatch || viewportMismatch || !lobbyLayout_) {
            useLobbyLayout();
        }
        return;
    }

    if (lobbyLayout_) {
        useGameLayout();
    }
}

void GameClient::handleLevelChange(uint8_t levelIndex, bool resizeWindow) {
    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
    const uint8_t globalIndex = catalog.filteredIndexToGlobalIndex(levelIndex, playerCount);
    const std::string visualPath = catalog.resolveVisualPath(globalIndex);
    const std::string collisionPath = catalog.resolvePath(globalIndex);
    const auto visualMtime = assetFileTime(visualPath);
    const auto collisionMtime = assetFileTime(collisionPath);
    if (globalIndex == loadedLevelIndex_ && visualPath == loadedVisualPath_ && tiledMap_.ready() &&
        visualMtime == loadedVisualFileTime_ && collisionMtime == loadedCollisionFileTime_) {
        return;
    }

    const std::string resolvedCollision = resolveAssetPath(collisionPath);
    if (!map_.loadFromFile(collisionPath)) {
        std::cerr << "[Client] Failed to load level preview: " << resolvedCollision << std::endl;
        return;
    }

    if (!visualPath.empty() && tiledMap_.load(visualPath)) {
        tiledMap_.bake();
        fanZones_ = loadFanZonesFromTmx(visualPath, 16);
        const std::string resolvedVisual = resolveAssetPath(visualPath);
        if (globalIndex == 0 && (map_.width() != tiledMap_.mapWidth() || map_.height() != tiledMap_.mapHeight())) {
            std::cerr << "[Client] Level 1 collision/visual size mismatch: collision " << map_.width() << "x"
                      << map_.height() << " vs visual " << tiledMap_.mapWidth() << "x" << tiledMap_.mapHeight()
                      << " — re-export with tools/export_level.py" << std::endl;
        }
        std::cout << "[Client] Level " << static_cast<int>(globalIndex + 1) << " loaded\n"
                  << "  collision: " << resolvedCollision << " (" << map_.width() << "x" << map_.height() << ")\n"
                  << "  visual:    " << resolvedVisual << " (" << tiledMap_.mapWidth() << "x" << tiledMap_.mapHeight()
                  << " tiles)"
                  << " customBg=" << (tiledMap_.hasCustomBackground() ? "yes" : "no") << " fans=" << fanZones_.size()
                  << std::endl;
    } else if (!visualPath.empty()) {
        std::cerr << "[Client] Failed to load tiled visual map: " << visualPath << std::endl;
        fanZones_.clear();
    } else {
        fanZones_.clear();
    }

    loadedLevelIndex_ = globalIndex;
    loadedVisualPath_ = visualPath;
    loadedVisualFileTime_ = visualMtime;
    loadedCollisionFileTime_ = collisionMtime;
    if (resizeWindow && !lobbyLayout_) {
        useGameLayout();
    }
}

void GameClient::onPhaseChanged(GamePhase previous, GamePhase current) {
    if (current == GamePhase::Lobby && previous != GamePhase::Lobby) {
        localReady_ = false;
        paused_ = false;
        levelMapScrollY_ = 0.0f;
        useLobbyLayout();
        handleLevelChange(renderWorld_.levelIndex, false);
    } else if (previous == GamePhase::Lobby && current != GamePhase::Lobby) {
        handleLevelChange(renderWorld_.levelIndex, false);
        useGameLayout();
    } else if (current == GamePhase::Victory || current == GamePhase::GameOver) {
        paused_ = false;
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
        const float frameDt = animClock_.restart().asSeconds();
        animTime_ += frameDt;

        sf::Event event{};
        while (window_.pollEvent(event)) {
            handleWindowEvent(event);
        }

        if (soloMode_) {
            updateSoloSession(frameDt);
        } else {
            pollNetwork();

            if (connectRequested_ && !connected_) {
                const auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration<float>(now - lastConnectRetry_).count() > 1.0f) {
                    sendConnectRequest();
                }
            }

            retryPendingLobbyActions();
        }

        if (connected_ && !paused_ && renderWorld_.phase == GamePhase::Playing) {
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

    if (soloMode_) {
        stopSoloPlay();
        return;
    }

    if (!connected_) {
        return;
    }

    network_.sendDisconnect(slot_);
    connected_ = false;
    acceptedRoomCode_[0] = '\0';
    waitingReadyIntent_.reset();
    roomHoverButton_ = -1;
}

void GameClient::usePublicServer() {
    host_ = DEFAULT_SERVER_HOST;
    network_.setServerHost(host_);
}

void GameClient::applyNetworkWorldState(const WorldState& incoming) {
    char preservedRoom[MAX_ROOM_CODE]{};
    if (incoming.roomCode[0] != '\0') {
        std::snprintf(preservedRoom, MAX_ROOM_CODE, "%s", incoming.roomCode);
    } else if (acceptedRoomCode_[0] != '\0') {
        std::snprintf(preservedRoom, MAX_ROOM_CODE, "%s", acceptedRoomCode_);
    } else if (renderWorld_.roomCode[0] != '\0') {
        std::snprintf(preservedRoom, MAX_ROOM_CODE, "%s", renderWorld_.roomCode);
    }

    const GamePhase previousPhase = world_.phase;
    const uint8_t previousLobbyStep = world_.lobbyStep;
    const uint8_t previousLevelIndex = world_.levelIndex;

    world_ = incoming;
    renderWorld_ = incoming;
    if (preservedRoom[0] != '\0') {
        std::snprintf(world_.roomCode, MAX_ROOM_CODE, "%s", preservedRoom);
        std::snprintf(renderWorld_.roomCode, MAX_ROOM_CODE, "%s", preservedRoom);
        std::snprintf(acceptedRoomCode_, MAX_ROOM_CODE, "%s", preservedRoom);
    }

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t globalLevelIndex =
        catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, std::max(uint8_t{1}, renderWorld_.connectedCount));
    if (globalLevelIndex != loadedLevelIndex_) {
        handleLevelChange(renderWorld_.levelIndex, !lobbyLayout_);
    }

    if (renderWorld_.lobbyStep == 0 && previousLobbyStep == 1) {
        localReady_ = false;
        levelMapScrollY_ = 0.0f;
    }
    if (renderWorld_.lobbyStep == 1 && previousLobbyStep == 0) {
        levelMapScrollY_ = 0.0f;
        scrollLevelMapToNode(renderWorld_.levelIndex);
    }
    if (renderWorld_.lobbyStep == 1 && renderWorld_.levelIndex != previousLevelIndex) {
        localReady_ = false;
        scrollLevelMapToNode(renderWorld_.levelIndex);
    }

    if (previousPhase != renderWorld_.phase) {
        onPhaseChanged(previousPhase, renderWorld_.phase);
    }

    applyWaitingReadyIntent();
}

sf::Vector2f GameClient::mapMousePos(int pixelX, int pixelY) const {
    return window_.mapPixelToCoords(sf::Vector2i(pixelX, pixelY));
}

bool GameClient::hitButtonArea(const sf::FloatRect& area, sf::Vector2f mouse) const {
    constexpr float kPad = 12.0f;
    const sf::FloatRect padded{area.left - kPad, area.top - kPad, area.width + kPad * 2.0f, area.height + kPad * 2.0f};
    return padded.contains(mouse);
}

void GameClient::requestWaitingReadyToggle() {
    toggleWaitingReadyLocal();
    waitingReadyIntent_ = localWaitingReady();
    waitingReadyIntentTime_ = std::chrono::steady_clock::now();
    sendAction(PlayerAction::WaitingReady);
}

void GameClient::applyWaitingReadyIntent() {
    if (!waitingReadyIntent_.has_value() || renderWorld_.lobbyStep != 0 || !connected_) {
        return;
    }

    const uint8_t bit = static_cast<uint8_t>(1u << slot_);
    const bool serverReady = (world_.waitingReadyMask & bit) != 0;
    if (serverReady == *waitingReadyIntent_) {
        waitingReadyIntent_.reset();
        return;
    }

    if (*waitingReadyIntent_) {
        renderWorld_.waitingReadyMask |= bit;
    } else {
        renderWorld_.waitingReadyMask &= static_cast<uint8_t>(~bit);
    }
    world_.waitingReadyMask = renderWorld_.waitingReadyMask;
}

void GameClient::retryPendingLobbyActions() {
    if (!waitingReadyIntent_.has_value() || soloMode_ || !connected_ || renderWorld_.lobbyStep != 0) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - waitingReadyIntentTime_).count() < 0.8f) {
        return;
    }

    waitingReadyIntentTime_ = now;
    sendAction(PlayerAction::WaitingReady);
}

void GameClient::pollNetwork() {
    network_.poll(
        [this](const ConnectAcceptPacket& packet) {
            connected_ = true;
            connectRequested_ = false;
            clientScreen_ = ClientScreen::Room;
            slot_ = packet.slot;
            role_ = packet.role;
            preferredRole_ = packet.role;
            localReady_ = false;
            roomAnimTimer_ = 0.0f;
            std::snprintf(acceptedRoomCode_, MAX_ROOM_CODE, "%s", packet.roomCode);
            std::snprintf(renderWorld_.roomCode, MAX_ROOM_CODE, "%s", packet.roomCode);
            std::snprintf(world_.roomCode, MAX_ROOM_CODE, "%s", packet.roomCode);
            useLobbyLayout();
            updateMusic(GamePhase::Lobby);
            std::cout << "[Client] Connected as " << roleName(role_) << " (slot " << static_cast<int>(slot_) << ")"
                      << std::endl;
        },
        [this](const ConnectRejectPacket& packet) {
            connectRequested_ = false;
            clientScreen_ = ClientScreen::Title;
            std::cerr << "[Client] Rejected: " << packet.reason << std::endl;
        },
        [this](const StatePacket& packet) { applyNetworkWorldState(packet.world); });
}

bool GameClient::sendInput() {
    if (soloMode_ && soloSession_) {
        soloSession_->setInput(slot_, currentInput_);
        lastSentInput_ = currentInput_;
        lastInputSend_ = std::chrono::steady_clock::now();
        return true;
    }

    if (!network_.sendInput(slot_, currentInput_, ++inputTick_)) {
        return false;
    }

    lastSentInput_ = currentInput_;
    lastInputSend_ = std::chrono::steady_clock::now();
    return true;
}

bool GameClient::sendAction(PlayerAction action, uint8_t value) {
    if (soloMode_ && soloSession_) {
        soloSession_->handleAction(slot_, action, value);
        syncWorldFromSession();
        return true;
    }

    return network_.sendAction(slot_, action, value);
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

    if (connected_ && renderWorld_.phase == GamePhase::Lobby && renderWorld_.lobbyStep == 0) {
        if (event.type == sf::Event::MouseMoved) {
            const sf::Vector2f mouse = mapMousePos(event.mouseMove.x, event.mouseMove.y);
            const auto buttons = roomActionButtonAreas();
            roomHoverButton_ = -1;
            for (int i = 0; i < 3; ++i) {
                if (hitButtonArea(buttons[static_cast<std::size_t>(i)], mouse)) {
                    roomHoverButton_ = i;
                    break;
                }
            }
        }

        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            const sf::Vector2f mouse = mapMousePos(event.mouseButton.x, event.mouseButton.y);
            const auto buttons = roomActionButtonAreas();
            const sf::FloatRect& readyArea = buttons[0];
            const sf::FloatRect& nextArea = buttons[1];
            const sf::FloatRect& leaveArea = buttons[2];

            if (hitButtonArea(readyArea, mouse)) {
                requestWaitingReadyToggle();
                return;
            }

            if (hitButtonArea(nextArea, mouse) && allPlayersWaitingReady()) {
                sendAction(PlayerAction::ProceedToMapSelect);
                return;
            }

            if (hitButtonArea(leaveArea, mouse)) {
                disconnect();
                connected_ = false;
                connectRequested_ = false;
                waitingReadyIntent_.reset();
                clientScreen_ = ClientScreen::Title;
                useTitleLayout();
                updateMusic(GamePhase::Lobby);
                return;
            }
        }
    }

    if (renderWorld_.phase == GamePhase::Lobby && renderWorld_.lobbyStep == 1) {
        if (event.type == sf::Event::MouseMoved) {
            const sf::Vector2f mouse = mapMousePos(event.mouseMove.x, event.mouseMove.y);
            lobbyHoverNode_ = levelNodeAtPosition(mouse.x, mouse.y);
        } else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            const sf::Vector2f mouse = mapMousePos(event.mouseButton.x, event.mouseButton.y);
            const sf::FloatRect playArea = lobbyPlayButtonRect();
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

    const bool inGamePausePhase =
        renderWorld_.phase == GamePhase::Playing || renderWorld_.phase == GamePhase::Countdown;
    if (connected_ && inGamePausePhase) {
        if (paused_) {
            if (handlePauseMenuClick(event)) {
                return;
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                paused_ = false;
                return;
            }
            return;
        }
        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
            if (pauseButtonRect().contains(mouse)) {
                paused_ = true;
                return;
            }
        }
    }

    if (connected_ && (renderWorld_.phase == GamePhase::Victory || renderWorld_.phase == GamePhase::GameOver)) {
        if (handleResultOverlayClick(event)) {
            return;
        }
    }

    if (event.type != sf::Event::KeyPressed) {
        return;
    }

    if (event.key.code == sf::Keyboard::Escape && connected_ && renderWorld_.phase == GamePhase::Lobby) {
        if (soloMode_) {
            disconnect();
            return;
        }
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
        if (renderWorld_.phase == GamePhase::Playing || renderWorld_.phase == GamePhase::Countdown) {
            paused_ = true;
            return;
        }
        localReady_ = false;
        sendAction(PlayerAction::ReturnToLobby);
        return;
    }

    if (connected_ && renderWorld_.phase == GamePhase::Lobby && renderWorld_.lobbyStep == 0 && !soloMode_) {
        if (event.key.code == sf::Keyboard::Enter && !localWaitingReady()) {
            requestWaitingReadyToggle();
            return;
        }
        if (event.key.code == sf::Keyboard::Enter && localWaitingReady() && allPlayersWaitingReady()) {
            sendAction(PlayerAction::ProceedToMapSelect);
            return;
        }
        if (event.key.code == sf::Keyboard::Space && !localWaitingReady()) {
            requestWaitingReadyToggle();
            return;
        }
        if (event.key.code == sf::Keyboard::Space && localWaitingReady() && allPlayersWaitingReady()) {
            sendAction(PlayerAction::ProceedToMapSelect);
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
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
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
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up) || sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
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
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::I) || sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
            input = input | InputFlags::Jump;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::K)) {
            input = input | InputFlags::Down;
        }
    }

    return input;
}

void GameClient::render() {
    ensureLayoutView();

    if (tiledMap_.ready() && connected_ && renderWorld_.phase != GamePhase::Lobby) {
        window_.clear(tiledMap_.hasCustomBackground() ? sf::Color(24, 32, 28) : sf::Color(88, 140, 72));
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
        if (renderWorld_.lobbyStep == 0 && !soloMode_) {
            renderRoomScreen();
        } else {
            renderLobbyScreen();
        }
    } else {
        renderGameScreen();
    }

    window_.display();
}

// 多层视差滚动背景；无资源时回退为渐变色
void GameClient::renderTitleBackground() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    if (assets_.ready() && assets_.hasTitleParallax()) {
        const auto& layers = assets_.titleParallaxLayers();
        for (std::size_t i = 0; i < layers.size(); ++i) {
            const TitleParallaxLayer& layer = layers[i];
            const float depth = static_cast<float>(i);
            const float driftY = std::sin(animTime_ * (0.14f + depth * 0.03f)) * (1.5f + depth * 0.8f);
            const float scroll = animTime_ * layer.scrollSpeed * (h / 160.0f);
            drawTiledParallaxLayer(window_, layer.texture, scroll, w, h, driftY);
        }
        return;
    }

    drawLobbyBackdrop(sf::Color(72, 52, 96), sf::Color(28, 20, 44));
}

void GameClient::renderTitleEffects() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);
    const float pulse = 0.45f + 0.55f * std::sin(animTime_ * 0.75f);
    const float moonX = w - lobbyScaled(150.0f) + std::sin(animTime_ * 0.08f) * 6.0f;
    const float moonY = lobbyScaled(96.0f) + std::sin(animTime_ * 0.11f) * 3.0f;

    sf::CircleShape moonGlow(96.0f);
    moonGlow.setOrigin(96.0f, 96.0f);
    moonGlow.setPosition(moonX, moonY);
    moonGlow.setFillColor(sf::Color(255, 210, 120, static_cast<sf::Uint8>(18.0f + pulse * 16.0f)));
    window_.draw(moonGlow);

    sf::CircleShape moon(34.0f);
    moon.setOrigin(34.0f, 34.0f);
    moon.setPosition(moonX, moonY);
    moon.setFillColor(sf::Color(255, 236, 170, static_cast<sf::Uint8>(210.0f + pulse * 30.0f)));
    window_.draw(moon);

    sf::VertexArray rays(sf::Triangles, 6);
    const sf::Color rayColor(255, 210, 140, static_cast<sf::Uint8>(14.0f + pulse * 18.0f));
    rays[0].position = {moonX - 120.0f, moonY - 80.0f};
    rays[0].color = rayColor;
    rays[1].position = {moonX + 180.0f, moonY - 40.0f};
    rays[1].color = sf::Color(255, 210, 140, 0);
    rays[2].position = {moonX - 40.0f, h * 0.55f};
    rays[2].color = sf::Color(255, 210, 140, 0);

    rays[3].position = {moonX - 80.0f, moonY - 60.0f};
    rays[3].color = rayColor;
    rays[4].position = {moonX + 140.0f, moonY + 20.0f};
    rays[4].color = sf::Color(255, 210, 140, 0);
    rays[5].position = {moonX - 120.0f, h * 0.62f};
    rays[5].color = sf::Color(255, 210, 140, 0);
    window_.draw(rays);

    constexpr int kParticleCount = 24;
    for (int i = 0; i < kParticleCount; ++i) {
        const float phase = animTime_ * 0.28f + static_cast<float>(i) * 1.65f;
        const float x = std::fmod(phase * 48.0f + static_cast<float>(i) * 83.0f, w);
        const float y = 120.0f + std::sin(phase * 1.2f) * 110.0f + static_cast<float>((i * 67) % 480);
        const float radius = 1.2f + static_cast<float>(i % 3) * 0.6f;
        const sf::Uint8 alpha =
            static_cast<sf::Uint8>(std::clamp(60.0f + 100.0f * std::sin(phase * 2.1f), 35.0f, 180.0f));
        const sf::Color color = (i % 3 == 0) ? sf::Color(180, 210, 255, alpha) : sf::Color(255, 230, 170, alpha);

        sf::CircleShape particle(radius);
        particle.setOrigin(radius, radius);
        particle.setPosition(x, y);
        particle.setFillColor(color);
        window_.draw(particle);
    }

    sf::RectangleShape vignette({w, h});
    vignette.setFillColor(sf::Color(12, 8, 24, 36));
    window_.draw(vignette);
}

void GameClient::renderTitleCharacters() {
    if (!assets_.ready()) {
        return;
    }

    const TitleLayoutMetrics layout = titleLayoutMetrics();
    const float charScale = 3.2f;
    const float charH = 32.0f * charScale;
    const float baseY = layout.footerTop - charH - lobbyScaled(12.0f);
    const float bobLeft = std::sin(animTime_ * 2.4f + 0.8f) * 5.0f;
    const float bobRight = std::sin(animTime_ * 2.4f + 2.1f) * 5.0f;
    const float leftX = lobbyScaled(48.0f);
    const float rightX = static_cast<float>(LOBBY_WINDOW_WIDTH) - lobbyScaled(48.0f) - charH;

    assets_.character(PlayerRole::Water).drawPortraitAnimated(window_, leftX, baseY + bobLeft, charH, false, animTime_);
    assets_.character(PlayerRole::Fire).drawPortraitAnimated(window_, rightX, baseY + bobRight, charH, true, animTime_);
}

void GameClient::renderTitleScreen() {
    const TitleLayoutMetrics layout = titleLayoutMetrics();
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);

    renderTitleBackground();
    renderTitleEffects();

    const unsigned titleSize = static_cast<unsigned>(std::clamp(177.0f * w / 1785.0f, 156.0f, 207.0f));
    const float titleY = layout.headerY + lobbyScaled(24.0f);

    ui_.drawArtTitleCentered(window_, text::gameTitle(), titleCenterX(), titleY, titleSize, animTime_);

    renderTitleCharacters();

    const auto& menuItems = text::titleMenuItems();
    const int activeIndex = titleHoverIndex_ >= 0 ? titleHoverIndex_ : titleMenuIndex_;
    const float rowH = static_cast<float>(titleMenuFontSize()) + lobbyScaled(20.0f);
    for (std::size_t i = 0; i < menuItems.size(); ++i) {
        const float y = layout.menuStartY + static_cast<float>(i) * layout.menuStep;
        const bool selected = static_cast<int>(i) == activeIndex;

        sf::RectangleShape row({layout.menuItemWidth, rowH});
        row.setOrigin(layout.menuItemWidth * 0.5f, 0.0f);
        row.setPosition(titleCenterX(), y - lobbyScaled(8.0f));
        row.setFillColor(selected ? sf::Color(24, 18, 36, 170) : sf::Color(12, 10, 22, 72));
        row.setOutlineThickness(selected ? 2.0f : 1.0f);
        row.setOutlineColor(selected ? sf::Color(255, 220, 120, 180) : sf::Color(255, 220, 140, 45));
        window_.draw(row);

        ui_.drawTitleMenuItem(window_, menuItems[i], titleCenterX(), y, titleMenuFontSize(), selected);
    }

    sf::RectangleShape footerBar({w, layout.footerHeight});
    footerBar.setPosition(0.0f, layout.footerTop);
    footerBar.setFillColor(sf::Color(8, 6, 16, 185));
    footerBar.setOutlineThickness(0.0f);
    window_.draw(footerBar);

    sf::RectangleShape footerLine({w, 2.0f});
    footerLine.setPosition(0.0f, layout.footerTop);
    footerLine.setFillColor(sf::Color(255, 220, 140, 90));
    window_.draw(footerLine);

    const float footerTextY = layout.footerTop + lobbyScaled(18.0f);
    const std::string roleLine = text::currentRolePrefix() + roleChineseName() + "     " + text::serverPrefix() + host_;
    ui_.drawCenteredText(window_, roleLine, titleCenterX(), footerTextY, static_cast<unsigned>(lobbyScaled(20.0f)),
                         sf::Color(230, 235, 210));
    ui_.drawCenteredText(window_, text::titleControlsHint(), titleCenterX(), footerTextY + lobbyScaled(30.0f),
                         static_cast<unsigned>(lobbyScaled(17.0f)), sf::Color(190, 195, 175));
}

void GameClient::drawLobbyBackdrop(sf::Color top, sf::Color bottom) {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    sf::VertexArray gradient(sf::Quads, 4);
    gradient[0].position = {0.0f, 0.0f};
    gradient[1].position = {w, 0.0f};
    gradient[2].position = {w, h};
    gradient[3].position = {0.0f, h};
    gradient[0].color = top;
    gradient[1].color = top;
    gradient[2].color = bottom;
    gradient[3].color = bottom;
    window_.draw(gradient);

    for (int i = 0; i < 7; ++i) {
        const float radius = 42.0f + static_cast<float>((i % 3) * 18);
        sf::CircleShape glow(radius);
        glow.setOrigin(radius, radius);
        glow.setPosition(80.0f + static_cast<float>(i) * 150.0f, 96.0f + static_cast<float>((i * 67) % 420));
        glow.setFillColor(i % 2 == 0 ? sf::Color(255, 140, 80, 18) : sf::Color(80, 180, 255, 16));
        window_.draw(glow);
    }
}

void GameClient::renderHelpOverlay() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);
    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window_.draw(dim);

    const float panelW = lobbyScaled(740.0f);
    const float panelH = lobbyScaled(640.0f);
    const sf::FloatRect panel{titleCenterX() - panelW / 2.0f, (h - panelH) / 2.0f, panelW, panelH};
    ui_.drawPanel(window_, panel, sf::Color(24, 34, 28), 235.0f);
    ui_.drawOutlinedCenteredText(window_, text::helpTitle(), titleCenterX(), panel.top + lobbyScaled(20.0f),
                                 static_cast<unsigned>(lobbyScaled(36.0f)), sf::Color(255, 230, 100),
                                 sf::Color(80, 50, 10), 3.0f);

    const float textX = panel.left + lobbyScaled(32.0f);
    const float indentX = textX + lobbyScaled(16.0f);
    float y = panel.top + lobbyScaled(72.0f);
    for (const text::HelpSection& section : text::helpSections()) {
        ui_.drawText(window_, section.title, textX, y, static_cast<unsigned>(lobbyScaled(22.0f)),
                     sf::Color(255, 210, 80));
        y += lobbyScaled(30.0f);
        for (const std::string& line : section.lines) {
            ui_.drawText(window_, line, indentX, y, static_cast<unsigned>(lobbyScaled(18.0f)),
                         sf::Color(230, 235, 220));
            y += lobbyScaled(26.0f);
        }
        y += lobbyScaled(6.0f);
    }

    ui_.drawCenteredText(window_, text::backToTitleHint(), titleCenterX(),
                         panel.top + panel.height - lobbyScaled(32.0f), static_cast<unsigned>(lobbyScaled(18.0f)),
                         sf::Color(255, 220, 120));
}

void GameClient::renderCreditsOverlay() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);
    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window_.draw(dim);

    const float panelW = lobbyScaled(640.0f);
    const float panelH = lobbyScaled(480.0f);
    const sf::FloatRect panel{titleCenterX() - panelW / 2.0f, (h - panelH) / 2.0f, panelW, panelH};
    ui_.drawPanel(window_, panel, sf::Color(24, 34, 28), 235.0f);
    ui_.drawOutlinedCenteredText(window_, text::creditsTitle(), titleCenterX(), panel.top + lobbyScaled(24.0f),
                                 static_cast<unsigned>(lobbyScaled(36.0f)), sf::Color(255, 230, 100),
                                 sf::Color(80, 50, 10), 3.0f);

    const float textX = panel.left + lobbyScaled(40.0f);
    float y = panel.top + lobbyScaled(96.0f);
    for (const std::string& line : text::creditLines()) {
        if (line.empty()) {
            y += lobbyScaled(16.0f);
            continue;
        }
        ui_.drawText(window_, line, textX, y, static_cast<unsigned>(lobbyScaled(20.0f)), sf::Color(230, 235, 220));
        y += lobbyScaled(36.0f);
    }

    ui_.drawCenteredText(window_, text::backToTitleHint(), titleCenterX(),
                         panel.top + panel.height - lobbyScaled(36.0f), static_cast<unsigned>(lobbyScaled(18.0f)),
                         sf::Color(255, 220, 120));
}

const char* GameClient::roleChineseName() const {
    switch (preferredRole_) {
        case PlayerRole::Fire:
            return "忍者蛙";
        case PlayerRole::Water:
            return "粉红侠";
        case PlayerRole::Poison:
            return "面具侠";
        default:
            return "未知";
    }
}

bool GameClient::isPlayerSlotConnected(uint8_t slot) const {
    return slot < MAX_PLAYERS && renderWorld_.playerNames[slot][0] != '\0';
}

void GameClient::toggleWaitingReadyLocal() {
    const uint8_t bit = static_cast<uint8_t>(1u << slot_);
    renderWorld_.waitingReadyMask ^= bit;
    world_.waitingReadyMask = renderWorld_.waitingReadyMask;
}

bool GameClient::localWaitingReady() const {
    // waitingReadyMask：等待室准备状态（与选关 readyMask 分开）
    return (renderWorld_.waitingReadyMask & (1u << slot_)) != 0;
}

bool GameClient::allPlayersWaitingReady() const {
    if (renderWorld_.connectedCount == 0) {
        return false;
    }

    uint8_t connected = 0;
    uint8_t ready = 0;
    for (uint8_t i = 0; i < MAX_PLAYERS; ++i) {
        if (!isPlayerSlotConnected(i)) {
            continue;
        }
        ++connected;
        if ((renderWorld_.waitingReadyMask & (1u << i)) != 0) {
            ++ready;
        }
    }
    return connected > 0 && ready >= connected;
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

void GameClient::scrollLevelMapToNode(uint8_t index) {
    if (index >= renderWorld_.levelCount) {
        return;
    }
    const sf::FloatRect viewport = levelMapViewportRect();
    const float nodeY = levelNodeLocalCenter(index).y;
    const float nodeRadius = levelNodeRadius(index, renderWorld_.levelCount);
    const float nodeTop = nodeY - nodeRadius - 8.0f;
    const float nodeBottom = nodeY + nodeRadius + 8.0f;

    if (nodeTop < levelMapScrollY_) {
        levelMapScrollY_ = nodeTop;
    } else if (nodeBottom > levelMapScrollY_ + viewport.height) {
        levelMapScrollY_ = nodeBottom - viewport.height;
    }
    levelMapScrollY_ = clampLevelMapScroll(levelMapScrollY_);
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

    // 二次贝塞尔曲线：控制点在中点法线方向，相邻段交替左右弯曲
    const sf::Vector2f mid = (from + to) * 0.5f;
    const sf::Vector2f perp(-dir.y, dir.x);
    const float curveSign = (fromIndex % 2 == 0) ? 1.0f : -1.0f;
    const float curveAmount = std::clamp(segmentLength * 0.32f, 32.0f, 96.0f) * curveSign;
    const sf::Vector2f control = mid + perp * curveAmount;

    const sf::Color pathColor = unlocked ? sf::Color(170, 130, 72, 220) : sf::Color(70, 62, 48, 160);
    const float halfWidth = unlocked ? 3.5f : 2.5f;
    // 沿曲线采样，用 TriangleStrip 拼成有宽度的路径
    constexpr int segments = 28;
    sf::VertexArray strip(sf::TriangleStrip, static_cast<std::size_t>((segments + 1) * 2));

    auto sampleQuadratic = [&](float t) {
        const float u = 1.0f - t;
        return from * (u * u) + control * (2.0f * u * t) + to * (t * t);
    };

    for (int i = 0; i <= segments; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(segments);
        const float t1 = std::min(1.0f, t0 + 1.0f / static_cast<float>(segments));
        const sf::Vector2f point = sampleQuadratic(t0);
        const sf::Vector2f tangent = sampleQuadratic(t1) - point;
        const float tangentLen = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        sf::Vector2f normal(-1.0f, 0.0f);
        if (tangentLen > 0.001f) {
            normal = sf::Vector2f(-tangent.y / tangentLen, tangent.x / tangentLen);
        }
        const std::size_t base = static_cast<std::size_t>(i * 2);
        strip[base] = sf::Vertex(point + normal * halfWidth, pathColor);
        strip[base + 1] = sf::Vertex(point - normal * halfWidth, pathColor);
    }
    window_.draw(strip);
}

void GameClient::drawLevelNode(uint8_t index, uint8_t levelCount, bool selected, bool unlocked, bool completed) {
    const sf::Vector2f center = levelNodeLocalCenter(index);
    const bool isBoss = isFinalLevelNode(index, levelCount);
    const float radius = levelNodeRadius(index, levelCount);

    if (selected) {
        const float layoutScale = levelMapLayoutScale();
        const float glowPad = (isBoss ? 22.0f : 14.0f) * layoutScale;
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
        const unsigned labelSize = static_cast<unsigned>(std::max(18.0f, 24.0f * levelMapLayoutScale()));
        ui_.drawCenteredText(window_, "?", center.x, center.y - labelSize * 0.55f, labelSize, sf::Color(110, 105, 100));
    } else {
        const unsigned labelSize = static_cast<unsigned>(std::max(16.0f, 22.0f * levelMapLayoutScale()));
        ui_.drawCenteredText(window_, isBoss ? "终" : std::to_string(index + 1), center.x, center.y - labelSize * 0.55f,
                             labelSize, sf::Color(35, 40, 48));
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

    sf::RectangleShape viewportBg({viewport.width, viewport.height});
    viewportBg.setPosition(viewport.left, viewport.top);
    viewportBg.setFillColor(sf::Color(88, 64, 44));
    viewportBg.setOutlineThickness(2.0f);
    viewportBg.setOutlineColor(sf::Color(60, 44, 30));
    window_.draw(viewportBg);

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

    sf::View restoredView = previousView;
    restoredView.setViewport(sf::FloatRect(0.0f, 0.0f, 1.0f, 1.0f));
    window_.setView(restoredView);

    ui_.drawText(window_, text::lobbyMapTitle(), mapArea.left + levelMapContentPadding(),
                 mapArea.top + (levelMapHeaderHeight() - lobbyScaled(22.0f)) * 0.38f,
                 static_cast<unsigned>(lobbyScaled(22.0f)), sf::Color(255, 235, 190));

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

    renderTitleBackground();
    renderTitleEffects();

    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 100));
    window_.draw(dim);

    const float panelW = std::min(w * 0.72f, lobbyScaled(680.0f));
    const float panelH = std::min(h * 0.62f, lobbyScaled(420.0f));
    const float panelX = (w - panelW) / 2.0f;
    const float panelY = (h - panelH) / 2.0f;
    const sf::FloatRect dialog{panelX, panelY, panelW, panelH};
    ui_.drawPanel(window_, dialog, sf::Color(20, 28, 44, 240), 240.0f);
    ui_.drawOutlinedCenteredText(window_, "加入房间", w / 2.0f, dialog.top + lobbyScaled(20.0f),
                                 static_cast<unsigned>(lobbyScaled(28.0f)), sf::Color(255, 230, 100),
                                 sf::Color(80, 50, 10), 2.5f);

    ui_.drawCenteredText(window_, "请输入房主提供的 6 位房间号", w / 2.0f, dialog.top + lobbyScaled(72.0f), 18,
                         sf::Color(180, 200, 230));
    ui_.drawCenteredText(window_, std::string("联机服务器: ") + DEFAULT_SERVER_HOST, w / 2.0f,
                         dialog.top + lobbyScaled(102.0f), 15, sf::Color(140, 170, 210));
    ui_.drawCenteredText(window_, "支持不同网络下的玩家加入同一房间", w / 2.0f, dialog.top + lobbyScaled(128.0f), 14,
                         sf::Color(120, 150, 180));

    const float boxW = lobbyScaled(280.0f);
    const float boxH = lobbyScaled(52.0f);
    const float boxX = w / 2.0f - boxW / 2.0f;
    const float boxY = dialog.top + panelH * 0.46f;
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
    ui_.drawOutlinedCenteredText(window_, displayCode, w / 2.0f, boxY + lobbyScaled(10.0f),
                                 static_cast<unsigned>(lobbyScaled(28.0f)), sf::Color(255, 255, 255),
                                 sf::Color(40, 40, 80), 2.0f);

    const bool readyToJoin = typedRoomCode_.size() == 6;
    ui_.drawCenteredText(
        window_,
        readyToJoin ? "按 Enter 加入房间" : "还需输入 " + std::to_string(6 - typedRoomCode_.size()) + " 位数字",
        w / 2.0f, boxY + boxH + lobbyScaled(24.0f), 16,
        readyToJoin ? sf::Color(120, 255, 160) : sf::Color(160, 170, 190));

    ui_.drawCenteredText(window_, "[数字键] 输入  [Backspace] 删除  [Enter] 加入  [Esc] 返回", w / 2.0f,
                         dialog.top + panelH - lobbyScaled(34.0f), static_cast<unsigned>(lobbyScaled(15.0f)),
                         sf::Color(160, 170, 190));
}

void GameClient::renderRoomScreen() {
    roomAnimTimer_ += 0.016f;
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);
    const float h = static_cast<float>(LOBBY_WINDOW_HEIGHT);

    // 与标题页一致的多层视差背景，替代旧 title_menu_bg
    renderTitleBackground();

    sf::RectangleShape dim({w, h});
    dim.setFillColor(sf::Color(0, 0, 0, 110));
    window_.draw(dim);

    const float headerH = roomHeaderHeight();
    ui_.drawPanel(window_, {0.0f, 0.0f, w, headerH}, sf::Color(16, 22, 40, 230), 230.0f);

    // 标题字号随窗口高度缩放，并在顶栏内垂直居中
    const unsigned titleFontSize =
        static_cast<unsigned>(std::clamp(h * 0.052f, lobbyScaled(48.0f), lobbyScaled(72.0f)));
    const unsigned subtitleFontSize =
        static_cast<unsigned>(std::clamp(h * 0.022f, lobbyScaled(18.0f), lobbyScaled(26.0f)));
    const float titleGap = lobbyScaled(10.0f);
    const float titleBlockH = static_cast<float>(titleFontSize) + titleGap + static_cast<float>(subtitleFontSize);
    const float titleY = (headerH - titleBlockH) * 0.5f;
    const float subtitleY = titleY + static_cast<float>(titleFontSize) + titleGap;
    ui_.drawOutlinedCenteredText(window_, text::gameTitle(), w / 2.0f, titleY, titleFontSize, sf::Color(255, 230, 100),
                                 sf::Color(80, 50, 10), 2.5f);
    ui_.drawCenteredText(window_, text::roomWaitingTitle(), w / 2.0f, subtitleY, subtitleFontSize,
                         sf::Color(180, 195, 220));

    const std::string roomCodeStr =
        renderWorld_.roomCode[0] ? std::string("房间号: ") + std::string(renderWorld_.roomCode) : "房间号: ------";
    const float infoRight = w - lobbyScaled(20.0f);
    // 右侧房间信息右对齐，避免长 IP 被裁切
    const unsigned infoTitleSize =
        static_cast<unsigned>(std::clamp(h * 0.020f, lobbyScaled(16.0f), lobbyScaled(22.0f)));
    const unsigned infoLineSize = static_cast<unsigned>(std::clamp(h * 0.016f, lobbyScaled(13.0f), lobbyScaled(18.0f)));
    const float infoLineGap = lobbyScaled(6.0f);
    const float infoBlockH =
        static_cast<float>(infoTitleSize) + infoLineGap * 2.0f + static_cast<float>(infoLineSize) * 2.0f;
    const float infoY = (headerH - infoBlockH) / 2.0f;
    ui_.drawRightAlignedText(window_, roomCodeStr, infoRight, infoY, infoTitleSize, sf::Color(100, 255, 140));

    const std::string ipStr = std::string("联机服务器: ") + DEFAULT_SERVER_HOST + ":" + std::to_string(SERVER_PORT);
    const float ipY = infoY + static_cast<float>(infoTitleSize) + infoLineGap;
    ui_.drawRightAlignedText(window_, ipStr, infoRight, ipY, infoLineSize, sf::Color(180, 200, 255));

    const float onlineY = ipY + static_cast<float>(infoLineSize) + infoLineGap;
    ui_.drawRightAlignedText(
        window_,
        std::string("在线: ") + std::to_string(renderWorld_.connectedCount) + "/" + std::to_string(MAX_PLAYERS),
        infoRight, onlineY, infoLineSize, sf::Color(160, 200, 140));

    const float panelW = roomPanelWidth();
    const float panelH = roomPanelHeight();
    const float panelGap = roomPanelGap();
    const float panelY = roomPanelTop();
    const float totalPanelW = panelW * 3.0f + panelGap * 2.0f;
    const float panelStartX = (w - totalPanelW) / 2.0f;
    renderRoomPlayerPanel(panelStartX, panelY, panelW, panelH, 0, PlayerRole::Fire);
    renderRoomPlayerPanel(panelStartX + panelW + panelGap, panelY, panelW, panelH, 1, PlayerRole::Water);
    renderRoomPlayerPanel(panelStartX + (panelW + panelGap) * 2.0f, panelY, panelW, panelH, 2, PlayerRole::Poison);

    const float statusY = panelY + panelH + lobbyScaled(20.0f);
    const float statusH = roomBottomBarY() - statusY - lobbyScaled(12.0f);
    const float statusMargin = lobbyScaled(36.0f);
    ui_.drawPanel(window_, {statusMargin, statusY, w - statusMargin * 2.0f, statusH}, sf::Color(20, 26, 44, 220),
                  220.0f);

    // Players connected status
    const bool fireConnected = isPlayerSlotConnected(0);
    const bool waterConnected = isPlayerSlotConnected(1);
    const bool poisonConnected = isPlayerSlotConnected(2);
    const bool fireReady = (renderWorld_.waitingReadyMask & 0x01) != 0;
    const bool waterReady = (renderWorld_.waitingReadyMask & 0x02) != 0;
    const bool poisonReady = (renderWorld_.waitingReadyMask & 0x04) != 0;

    const unsigned statusTitleSize = static_cast<unsigned>(lobbyScaled(22.0f));
    const unsigned statusRowSize = static_cast<unsigned>(lobbyScaled(18.0f));
    const unsigned statusHintSize = static_cast<unsigned>(lobbyScaled(16.0f));
    const unsigned waitMsgSize = static_cast<unsigned>(lobbyScaled(20.0f));
    const float statusTextX = statusMargin + lobbyScaled(20.0f);
    const float statusRowStep = lobbyScaled(30.0f);  // 三名玩家纵向逐行排列，避免重叠

    ui_.drawText(window_, "玩家状态", statusTextX, statusY + lobbyScaled(14.0f), statusTitleSize,
                 sf::Color(200, 210, 230));

    const std::string fireName = renderWorld_.playerNames[0][0] ? renderWorld_.playerNames[0] : "---";
    const std::string waterName = renderWorld_.playerNames[1][0] ? renderWorld_.playerNames[1] : "---";
    const std::string poisonName = renderWorld_.playerNames[2][0] ? renderWorld_.playerNames[2] : "---";

    auto drawStatus = [&](float rowY, const std::string& label, const std::string& name, bool connected, bool ready,
                          sf::Color roleColor) {
        sf::Color color = connected ? (ready ? roleColor : sf::Color(200, 180, 160)) : sf::Color(100, 100, 100);
        ui_.drawText(window_, label + ": " + name, statusTextX, rowY, statusRowSize, color);
        const std::string state = connected ? (ready ? " [已准备]" : " [等待中]") : " [未连接]";
        sf::Color stateCol =
            connected ? (ready ? sf::Color(120, 255, 140) : sf::Color(180, 180, 180)) : sf::Color(140, 80, 80);
        ui_.drawText(window_, state, statusTextX + lobbyScaled(180.0f), rowY, statusRowSize, stateCol);
    };

    float rowY = statusY + lobbyScaled(44.0f);
    drawStatus(rowY, "忍者蛙", fireName, fireConnected, fireReady, sf::Color(120, 200, 80));
    rowY += statusRowStep;
    drawStatus(rowY, "粉红侠", waterName, waterConnected, waterReady, sf::Color(255, 120, 180));
    rowY += statusRowStep;
    drawStatus(rowY, "面具侠", poisonName, poisonConnected, poisonReady, sf::Color(255, 180, 60));

    ui_.drawText(window_, text::roomWaitingHint(), statusTextX, rowY + statusRowStep + lobbyScaled(4.0f),
                 statusHintSize, sf::Color(140, 150, 170));

    const bool waitingReady = localWaitingReady();
    const bool canProceed = allPlayersWaitingReady();
    const float bottomBarY = roomBottomBarY();
    // 等待提示锚定在底栏上方，避免与操作按钮重叠
    const unsigned subWaitMsgSize = static_cast<unsigned>(lobbyScaled(14.0f));
    const float waitMsgGapAboveBar = lobbyScaled(16.0f);
    const float waitMsgLineGap = lobbyScaled(10.0f);
    const bool showSubWaitMsg = !canProceed && renderWorld_.connectedCount < MAX_PLAYERS;
    const float subWaitMsgY = bottomBarY - waitMsgGapAboveBar - static_cast<float>(subWaitMsgSize);
    const float mainWaitMsgY = showSubWaitMsg ? subWaitMsgY - waitMsgLineGap - static_cast<float>(waitMsgSize)
                                              : bottomBarY - waitMsgGapAboveBar - static_cast<float>(waitMsgSize);

    if (!canProceed) {
        ui_.drawOutlinedCenteredText(window_, "请先点击「我已准备」，等待全员准备后可进入选关", w / 2.0f, mainWaitMsgY,
                                     waitMsgSize, sf::Color(255, 220, 100), sf::Color(80, 50, 10), 2.0f);
        if (showSubWaitMsg) {
            ui_.drawCenteredText(window_,
                                 "当前 " + std::to_string(renderWorld_.connectedCount) + "/" +
                                     std::to_string(MAX_PLAYERS) + " 人，可继续等待或直接开始",
                                 w / 2.0f, subWaitMsgY, subWaitMsgSize, sf::Color(160, 180, 210));
        }
    } else {
        ui_.drawOutlinedCenteredText(window_, "全员已准备，请点击「下一步」选择关卡", w / 2.0f, mainWaitMsgY,
                                     waitMsgSize, sf::Color(120, 255, 160), sf::Color(40, 80, 50), 2.0f);
    }
    ui_.drawPanel(window_, {0.0f, bottomBarY, w, roomBottomBarHeight()}, sf::Color(16, 22, 40, 240), 240.0f);

    const auto buttons = roomActionButtonAreas();
    const sf::FloatRect& readyArea = buttons[0];
    const sf::FloatRect& nextArea = buttons[1];
    const sf::FloatRect& leaveArea = buttons[2];

    if (waitingReady) {
        ui_.drawButton(window_, readyArea, text::roomCancelReady(), true, sf::Color(50, 160, 90));
    } else {
        ui_.drawButton(window_, readyArea, text::roomWaitingReady(), true, sf::Color(60, 130, 210));
    }

    // 「下一步」仅在全员已准备后可点，未满足条件时显示灰色属正常
    ui_.drawButton(window_, nextArea, text::roomNextStep(), canProceed,
                   canProceed ? sf::Color(210, 150, 50) : sf::Color(80, 80, 90));
    ui_.drawButton(window_, leaveArea, "离开房间", true, sf::Color(120, 60, 50));
}

void GameClient::renderRoomPlayerPanel(float panelX, float panelY, float panelW, float panelH, int playerSlot,
                                       PlayerRole expectedRole) {
    const bool isConnected = isPlayerSlotConnected(static_cast<uint8_t>(playerSlot));
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

    sf::RectangleShape innerGlow({panelW - 14.0f, panelH - 14.0f});
    innerGlow.setPosition(panelX + 7.0f, panelY + 7.0f);
    innerGlow.setFillColor(sf::Color::Transparent);
    innerGlow.setOutlineThickness(1.0f);
    innerGlow.setOutlineColor(isConnected ? sf::Color(255, 255, 255, 52) : sf::Color(255, 255, 255, 24));
    window_.draw(innerGlow);
    // Role title
    std::string roleTitle;
    sf::Color roleColor;
    std::string roleSub;
    if (expectedRole == PlayerRole::Fire) {
        roleTitle = "忍者蛙";
        roleColor = sf::Color(120, 200, 80);
        roleSub = "WASD 操控";
    } else if (expectedRole == PlayerRole::Water) {
        roleTitle = "粉红侠";
        roleColor = sf::Color(255, 120, 180);
        roleSub = "方向键 操控";
    } else {
        roleTitle = "面具侠";
        roleColor = sf::Color(255, 180, 60);
        roleSub = "IJKL 操控";
    }
    ui_.drawOutlinedCenteredText(window_, roleTitle, panelX + panelW / 2.0f, panelY + panelH * 0.05f,
                                 static_cast<unsigned>(lobbyScaled(24.0f)),
                                 isConnected ? roleColor : sf::Color(120, 120, 130), sf::Color(30, 20, 20), 2.0f);

    // Character portrait
    if (assets_.ready() && expectedRole != PlayerRole::None) {
        const float targetH = panelH * 0.38f;
        const float drawW = targetH;
        const float portraitX = panelX + (panelW - drawW) / 2.0f;
        const float portraitY = panelY + panelH * 0.16f;
        sf::Color tint = sf::Color::White;
        if (!isConnected) {
            tint = sf::Color(80, 80, 80, 140);
        }
        assets_.character(expectedRole).drawPortrait(window_, portraitX, portraitY, targetH, false, tint);
    } else {
        const float phW = lobbyScaled(56.0f);
        const float phH = panelH * 0.38f;
        sf::RectangleShape placeholder({phW, phH});
        placeholder.setPosition(panelX + (panelW - phW) / 2.0f, panelY + panelH * 0.16f);
        placeholder.setFillColor(isConnected ? roleColor : sf::Color(80, 80, 80));
        window_.draw(placeholder);
    }

    if (isSelf) {
        const float indicatorR = lobbyScaled(6.0f);
        sf::CircleShape indicator(indicatorR);
        indicator.setPosition(panelX + panelW / 2.0f + lobbyScaled(40.0f), panelY + panelH * 0.18f);
        indicator.setFillColor(sf::Color(80, 255, 120));
        window_.draw(indicator);
    }

    // Player name
    const std::string playerName =
        renderWorld_.playerNames[playerSlot][0] ? renderWorld_.playerNames[playerSlot] : "---";
    ui_.drawCenteredText(window_, playerName, panelX + panelW / 2.0f, panelY + panelH * 0.68f,
                         static_cast<unsigned>(lobbyScaled(16.0f)),
                         isConnected ? sf::Color(240, 240, 240) : sf::Color(120, 120, 120));

    ui_.drawCenteredText(window_, roleSub, panelX + panelW / 2.0f, panelY + panelH * 0.76f,
                         static_cast<unsigned>(lobbyScaled(13.0f)),
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

    const float badgeW = lobbyScaled(100.0f);
    const float badgeH = lobbyScaled(28.0f);
    const float badgeX = panelX + (panelW - badgeW) / 2.0f;
    const float badgeY = panelY + panelH * 0.84f;
    sf::RectangleShape badge({badgeW, badgeH});
    badge.setPosition(badgeX, badgeY);
    sf::Color badgeFill = statusColor;
    badgeFill.a = isConnected ? 200 : 100;
    badge.setFillColor(badgeFill);
    badge.setOutlineThickness(1.5f);
    badge.setOutlineColor(sf::Color(255, 255, 255, 80));
    window_.draw(badge);
    ui_.drawCenteredText(window_, statusText, panelX + panelW / 2.0f, badgeY + lobbyScaled(4.0f),
                         static_cast<unsigned>(lobbyScaled(15.0f)), sf::Color::White);

    if (isReady) {
        ui_.drawOutlinedCenteredText(window_, "\xe2\x9c\x93", panelX + panelW / 2.0f, panelY + 16.0f, 16,
                                     sf::Color(80, 255, 120), sf::Color(20, 60, 20), 1.5f);
    }
}

void GameClient::renderLobbyScreen() {
    const float w = static_cast<float>(LOBBY_WINDOW_WIDTH);

    drawLobbyBackdrop(sf::Color(38, 31, 44), sf::Color(62, 46, 30));

    const float headerH = roomHeaderHeight();
    ui_.drawPanel(window_, {0.0f, 0.0f, w, headerH}, sf::Color(48, 36, 28, 220), 220.0f);
    ui_.drawCenteredText(window_, text::forestTemple(), w / 2.0f, lobbyScaled(12.0f),
                         static_cast<unsigned>(lobbyScaled(34.0f)), sf::Color(255, 230, 170));
    ui_.drawCenteredText(window_, text::levelSelectTitle(), w / 2.0f, lobbyScaled(48.0f),
                         static_cast<unsigned>(lobbyScaled(16.0f)), sf::Color(200, 190, 170));

    drawLevelProgressMap();

    const float previewTop = levelMapPanelTop();
    const float bottomReserve = lobbyBottomBarHeight() + lobbyScaled(24.0f);
    const float previewHeight = lobbyBottomBarTop() - previewTop - bottomReserve;
    const float previewW = previewPanelWidth();
    const float previewMargin = lobbyScaled(kPreviewPanelMargin);
    const sf::FloatRect previewPanel{w - previewMargin - previewW, previewTop, previewW, previewHeight};
    ui_.drawPanel(window_, previewPanel, sf::Color(28, 24, 20, 220), 220.0f);

    const float previewPad = lobbyScaled(14.0f);
    const unsigned previewTitleSize = static_cast<unsigned>(lobbyScaled(20.0f));
    const float previewTitleH = lobbyScaled(44.0f);
    ui_.drawText(window_, text::lobbyPreviewTitle(), previewPanel.left + previewPad,
                 previewPanel.top + (previewTitleH - static_cast<float>(previewTitleSize)) * 0.35f, previewTitleSize,
                 sf::Color(255, 230, 170));

    const float previewZoneTop = previewPanel.top + previewTitleH;
    const float previewZoneH = previewPanel.height * 0.38f;
    const sf::FloatRect mapPreviewArea{previewPanel.left + previewPad, previewZoneTop,
                                       previewPanel.width - previewPad * 2.0f, previewZoneH};

    const float infoPanelTop = mapPreviewArea.top + mapPreviewArea.height + lobbyScaled(10.0f);
    const float infoPanelH = previewPanel.top + previewPanel.height - infoPanelTop - previewPad;
    ui_.drawPanel(window_,
                  {previewPanel.left + previewPad, infoPanelTop, previewPanel.width - previewPad * 2.0f, infoPanelH},
                  sf::Color(18, 16, 14, 210), 210.0f);
    if (tiledMap_.ready()) {
        tiledMap_.drawPreview(window_, mapPreviewArea);
    } else {
        drawMapPreview(window_, mapPreviewArea);
    }

    const LevelCatalog& catalog = LevelCatalog::instance();
    const uint8_t playerCount = std::max(uint8_t{1}, renderWorld_.connectedCount);
    const uint8_t globalSelected = catalog.filteredIndexToGlobalIndex(renderWorld_.levelIndex, playerCount);
    const LevelInfo& selectedInfo = catalog.at(globalSelected);
    const bool selectedUnlocked = isLevelUnlocked(renderWorld_.unlockedMask, globalSelected);
    const bool selectedCompleted = isLevelCompleted(renderWorld_.completedMask, globalSelected);

    const float infoX = previewPanel.left + previewPad + lobbyScaled(8.0f);
    const float textWidth = previewPanel.width - previewPad * 2.0f - lobbyScaled(16.0f);
    float infoY = infoPanelTop + lobbyScaled(12.0f);
    ui_.drawText(window_, selectedInfo.title, infoX, infoY, static_cast<unsigned>(lobbyScaled(17.0f)),
                 sf::Color(255, 230, 170));
    infoY += lobbyScaled(24.0f);
    infoY = ui_.drawWrappedText(window_, text::levelPreviewSummary(globalSelected), infoX, infoY,
                                static_cast<unsigned>(lobbyScaled(13.0f)), sf::Color(180, 175, 165), textWidth,
                                lobbyScaled(4.0f)) +
            lobbyScaled(6.0f);
    infoY =
        ui_.drawWrappedText(window_, text::lobbyPreviewElementsLabel() + text::levelPreviewElementList(globalSelected),
                            infoX, infoY, static_cast<unsigned>(lobbyScaled(13.0f)), sf::Color(150, 200, 170),
                            textWidth, lobbyScaled(4.0f)) +
        lobbyScaled(6.0f);

    const std::string requirement =
        text::lobbyPreviewRequirementLabel() +
        (mapHasExitTiles(map_) ? text::lobbyPreviewReachExits() : text::lobbyPreviewCollectAllGems());
    infoY = ui_.drawWrappedText(window_, requirement, infoX, infoY, static_cast<unsigned>(lobbyScaled(13.0f)),
                                sf::Color(255, 210, 120), textWidth, lobbyScaled(4.0f)) +
            lobbyScaled(6.0f);
    ui_.drawWrappedText(window_,
                        text::lobbyPreviewGemLabel() + std::to_string(renderWorld_.totalGems) + "  |  " +
                            text::lobbyPreviewPlayerLabel() + std::to_string(selectedInfo.minPlayers) + "-" +
                            std::to_string(selectedInfo.maxPlayers),
                        infoX, infoY, static_cast<unsigned>(lobbyScaled(13.0f)), sf::Color(170, 190, 210), textWidth,
                        lobbyScaled(4.0f));

    const sf::FloatRect bottomBar = lobbyBottomBarRect();
    ui_.drawPanel(window_, bottomBar, sf::Color(36, 30, 24, 220), 220.0f);

    std::string levelLine = "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + selectedInfo.title;
    if (!selectedUnlocked) {
        levelLine += "  [" + text::lobbyLockedHint() + "]";
    } else if (selectedCompleted) {
        levelLine += "  [" + text::lobbyClearedHint() + "]";
    }
    ui_.drawText(window_, levelLine, bottomBar.left + lobbyScaled(20.0f), bottomBar.top + lobbyScaled(12.0f),
                 static_cast<unsigned>(lobbyScaled(18.0f)), sf::Color(255, 230, 170));
    ui_.drawText(window_, text::lobbyControlsHint(), bottomBar.left + lobbyScaled(20.0f),
                 bottomBar.top + lobbyScaled(36.0f), static_cast<unsigned>(lobbyScaled(14.0f)),
                 sf::Color(180, 170, 150));

    const sf::FloatRect playArea = lobbyPlayButtonRect();
    const bool canStart = selectedUnlocked && !localReady_;
    if (!localReady_) {
        ui_.drawButton(window_, playArea, canStart ? text::lobbyReadyButton() : text::lobbyLockedHint(), canStart,
                       canStart ? sf::Color(210, 150, 50) : sf::Color(80, 80, 90));
    } else {
        ui_.drawButton(window_, playArea, text::lobbyCancelReadyButton(), true, sf::Color(50, 140, 90));
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

    const bool inGamePausePhase =
        renderWorld_.phase == GamePhase::Playing || renderWorld_.phase == GamePhase::Countdown;

    if (renderWorld_.phase == GamePhase::Playing || renderWorld_.phase == GamePhase::Victory ||
        renderWorld_.phase == GamePhase::GameOver) {
        drawPlayer(window_, renderWorld_.players[0]);
        drawPlayer(window_, renderWorld_.players[1]);
    }

    drawFanWindEffects(window_);
    drawMagnetPulls(window_);

    if (inGamePausePhase && !paused_) {
        drawPauseButton(window_);
    }

    // 第一关 Tiled 大地图：结算全屏贴图时不叠 HUD 文字
    const bool hideHudOnResult = renderWorld_.levelIndex == 0 && (renderWorld_.phase == GamePhase::Victory ||
                                                                  renderWorld_.phase == GamePhase::GameOver);
    if (!hideHudOnResult) {
        drawHud(window_);
    }

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

    if (paused_ && inGamePausePhase) {
        drawPauseOverlay(window_, centerX);
    }
}

void GameClient::drawConnectingScreen(sf::RenderWindow& window) const {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float panelW = lobbyScaled(480.0f);
    const float panelH = lobbyScaled(140.0f);

    ui_.drawPanel(window, {w / 2.0f - panelW / 2.0f, h / 2.0f - panelH / 2.0f, panelW, panelH},
                  sf::Color(20, 30, 24, 220), 220.0f);
    ui_.drawOutlinedCenteredText(window, text::connectingTitle(), w / 2.0f, h / 2.0f - lobbyScaled(36.0f),
                                 static_cast<unsigned>(lobbyScaled(30.0f)), sf::Color(255, 240, 180),
                                 sf::Color(60, 40, 10), 2.5f);
    ui_.drawCenteredText(window, host_ + "  |  " + text::rolePrefixConnecting() + roleChineseName(), w / 2.0f,
                         h / 2.0f + lobbyScaled(8.0f), static_cast<unsigned>(lobbyScaled(18.0f)),
                         sf::Color(200, 210, 190));
    ui_.drawCenteredText(window, text::backToTitleHint(), w / 2.0f, h / 2.0f + lobbyScaled(40.0f),
                         static_cast<unsigned>(lobbyScaled(16.0f)), sf::Color(180, 190, 170));
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
                const float gemScale =
                    tileSize / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y)) * GEM_VISUAL_SCALE;
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
                const float gemScale =
                    tileSize / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y)) * GEM_VISUAL_SCALE;
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

void GameClient::drawSawTraps(sf::RenderWindow& window) const {
    if (!tiledMap_.ready()) {
        return;
    }

    const float timeSec = static_cast<float>(renderWorld_.tick) * TICK_DT;
    for (uint8_t i = 0; i < renderWorld_.sawCount; ++i) {
        const WorldState::SyncSaw& saw = renderWorld_.saws[i];
        if (saw.active == 0) {
            continue;
        }
        tiledMap_.drawAnimatedObjectGidAt(window, saw.gid, saw.x, saw.y, saw.w, saw.h, timeSec);
    }
}

void GameClient::drawRockHeads(sf::RenderWindow& window) const {
    if (!tiledMap_.ready()) {
        return;
    }
    for (uint8_t i = 0; i < renderWorld_.rockHeadCount; ++i) {
        const WorldState::SyncRockHead& rock = renderWorld_.rockHeads[i];
        if (rock.active == 0) {
            continue;
        }
        tiledMap_.drawObjectGidAt(window, rock.gid, rock.x, rock.y, rock.w, rock.h, animTime_);
    }
}

void GameClient::drawPendulums(sf::RenderWindow& window) const {
    if (!tiledMap_.ready()) {
        return;
    }
    for (uint8_t i = 0; i < renderWorld_.pendulumCount; ++i) {
        const WorldState::SyncPendulum& pendulum = renderWorld_.pendulums[i];
        if (pendulum.active == 0) {
            continue;
        }
        const float ballCenterX = pendulum.ballX + pendulum.ballW * 0.5f;
        const float ballCenterY = pendulum.ballY + pendulum.ballH * 0.5f;
        const uint8_t chainCount = std::max<uint8_t>(1, pendulum.chainCount);
        for (uint8_t chain = 1; chain <= chainCount; ++chain) {
            const float t = static_cast<float>(chain) / static_cast<float>(chainCount + 1);
            const float cx = pendulum.pivotX + (ballCenterX - pendulum.pivotX) * t;
            const float cy = pendulum.pivotY + (ballCenterY - pendulum.pivotY) * t;
            constexpr float chainSize = TILE_SIZE * 0.5f;
            tiledMap_.drawObjectGidAt(window, pendulum.chainGid, cx - chainSize * 0.5f, cy - chainSize * 0.5f,
                                      chainSize, chainSize, animTime_);
        }
        tiledMap_.drawObjectGidAt(window, pendulum.ballGid, pendulum.ballX, pendulum.ballY, pendulum.ballW,
                                  pendulum.ballH, animTime_);
    }
}
void GameClient::drawMudParticles(sf::RenderWindow& window) const {
    for (uint8_t i = 0; i < renderWorld_.mudParticleCount; ++i) {
        const WorldState::SyncMudParticle& particle = renderWorld_.mudParticles[i];
        if (particle.active == 0) {
            continue;
        }
        sf::CircleShape mud(MUD_HITBOX * 0.5f * MUD_VISUAL_SCALE);
        mud.setFillColor(sf::Color(50, 150, 60));
        mud.setOutlineColor(sf::Color(30, 100, 40));
        mud.setOutlineThickness(1.0f);
        mud.setOrigin(MUD_HITBOX * 0.5f * MUD_VISUAL_SCALE, MUD_HITBOX * 0.5f * MUD_VISUAL_SCALE);
        mud.setPosition(particle.x, particle.y);
        window.draw(mud);
    }
}

void GameClient::drawFlyingEnemies(sf::RenderWindow& window) const {
    for (uint8_t i = 0; i < renderWorld_.flyingEnemyCount; ++i) {
        const WorldState::SyncFlyingEnemy& enemy = renderWorld_.flyingEnemies[i];
        if (enemy.active == 0) {
            continue;
        }

        const float wingLift = std::sin(animTime_ * 5.0f + static_cast<float>(i) * 1.8f) * 4.0f;
        const sf::Color bodyColor(92, 48, 128);
        const sf::Color wingColor(130, 72, 170);
        const sf::Color eyeColor(255, 230, 90);

        sf::CircleShape body(ENEMY_HITBOX_H * 0.42f);
        body.setFillColor(bodyColor);
        body.setOutlineColor(sf::Color(40, 18, 58));
        body.setOutlineThickness(2.0f);
        body.setOrigin(body.getRadius(), body.getRadius());
        body.setPosition(enemy.x, enemy.y);
        window.draw(body);

        sf::CircleShape eye(4.0f);
        eye.setFillColor(eyeColor);
        eye.setOrigin(4.0f, 4.0f);
        eye.setPosition(enemy.x + static_cast<float>(enemy.facing) * 8.0f, enemy.y - 4.0f);
        window.draw(eye);

        sf::CircleShape horn(3.0f, 3);
        horn.setFillColor(sf::Color(180, 140, 200));
        horn.setOrigin(3.0f, 6.0f);
        horn.setPosition(enemy.x, enemy.y - ENEMY_HITBOX_H * 0.35f);
        window.draw(horn);

        const float wingSpread = ENEMY_HITBOX_W * 0.38f;
        sf::CircleShape wingL(wingSpread * 0.55f);
        wingL.setFillColor(wingColor);
        wingL.setOrigin(wingSpread * 0.55f, wingSpread * 0.55f);
        wingL.setPosition(enemy.x - wingSpread, enemy.y + wingLift);
        window.draw(wingL);

        sf::CircleShape wingR(wingSpread * 0.55f);
        wingR.setFillColor(wingColor);
        wingR.setOrigin(wingSpread * 0.55f, wingSpread * 0.55f);
        wingR.setPosition(enemy.x + wingSpread, enemy.y + wingLift);
        window.draw(wingR);
    }
}

void GameClient::drawTridentProjectiles(sf::RenderWindow& window) const {
    for (uint8_t i = 0; i < renderWorld_.projectileCount; ++i) {
        const WorldState::SyncProjectile& projectile = renderWorld_.projectiles[i];
        if (projectile.active == 0) {
            continue;
        }

        sf::ConvexShape trident;
        trident.setPointCount(5);
        const float half = TRIDENT_HITBOX * 0.45f;
        trident.setPoint(0, {0.0f, -half * 1.4f});
        trident.setPoint(1, {-half * 0.55f, half * 0.2f});
        trident.setPoint(2, {0.0f, half * 0.55f});
        trident.setPoint(3, {half * 0.55f, half * 0.2f});
        trident.setPoint(4, {0.0f, -half * 0.6f});
        trident.setFillColor(sf::Color(210, 180, 60));
        trident.setOutlineColor(sf::Color(120, 90, 30));
        trident.setOutlineThickness(1.5f);
        trident.setOrigin(0.0f, 0.0f);
        trident.setPosition(projectile.x, projectile.y);
        trident.setRotation(projectile.rotation);
        window.draw(trident);
    }
}

void GameClient::drawFanWindEffects(sf::RenderWindow& window) const {
    if (fanZones_.empty()) {
        return;
    }

    sf::RenderStates addBlend;
    addBlend.blendMode = sf::BlendAdd;

    constexpr int kStreamsPerFan = 11;
    for (const FanZone& fan : fanZones_) {
        const float streamHeight = std::max(32.0f, fan.top + fan.height - fan.targetFeetY);
        const float baseX = fan.emitterX > 0.0f ? fan.emitterX : fan.left + fan.width * 0.5f;
        const float baseY = fan.emitterY > 0.0f ? fan.emitterY : fan.top + fan.height;

        for (int i = 0; i < kStreamsPerFan; ++i) {
            const float laneOffset = (static_cast<float>(i) - static_cast<float>(kStreamsPerFan - 1) * 0.5f) * 6.0f;
            const float phase = animTime_ * 4.2f + static_cast<float>(i) * 0.45f + baseX * 0.004f;
            const float t = phase - std::floor(phase);
            const float y = baseY - t * streamHeight;
            const float wobble = std::sin(animTime_ * 11.0f + static_cast<float>(i) * 1.4f) * 4.0f;
            const float alpha = 0.35f + 0.55f * (1.0f - t);
            const float radius = 3.0f + t * 5.0f;

            sf::CircleShape puff(radius);
            puff.setOrigin(radius, radius);
            puff.setPosition(baseX + laneOffset + wobble, y);
            puff.setFillColor(sf::Color(200, 240, 255, static_cast<sf::Uint8>(alpha * 255.0f)));
            window.draw(puff, addBlend);
        }

        sf::VertexArray streaks(sf::Lines, 12);
        for (std::size_t i = 0; i < 6; ++i) {
            const float lane = (static_cast<float>(i) - 2.5f) * 7.0f;
            const float phase = animTime_ * 4.5f + static_cast<float>(i) * 0.75f;
            const float t = phase - std::floor(phase);
            const float y0 = baseY - t * streamHeight;
            const float y1 = y0 - 28.0f - t * 14.0f;
            const sf::Color color(235, 250, 255, static_cast<sf::Uint8>((0.25f + 0.45f * (1.0f - t)) * 255.0f));
            streaks[i * 2].position = {baseX + lane, y0};
            streaks[i * 2].color = color;
            streaks[i * 2 + 1].position = {baseX + lane + std::sin(animTime_ * 8.0f + i) * 3.0f, y1};
            streaks[i * 2 + 1].color = sf::Color(color.r, color.g, color.b, 0);
        }
        window.draw(streaks, addBlend);
    }
}

bool GameClient::isFruitMagnetPulled(uint8_t pickupIndex) const {
    for (uint8_t i = 0; i < renderWorld_.magnetPullCount; ++i) {
        const WorldState::SyncMagnetPull& pull = renderWorld_.magnetPulls[i];
        if (pull.active != 0 && pull.kind == 1 && pull.pickupIndex == pickupIndex) {
            return true;
        }
    }
    return false;
}

bool GameClient::isGemMagnetPulled(int tx, int ty) const {
    for (uint8_t i = 0; i < renderWorld_.magnetPullCount; ++i) {
        const WorldState::SyncMagnetPull& pull = renderWorld_.magnetPulls[i];
        if (pull.active != 0 && pull.kind == 0 && pull.gemTx == tx && pull.gemTy == ty) {
            return true;
        }
    }
    return false;
}

void GameClient::drawMagnetItem(sf::RenderWindow& window, float cx, float cy, float size, float spin) const {
    const float bob = std::sin(animTime_ * 5.0f + cx * 0.01f) * 2.0f;
    cy += bob;

    sf::CircleShape aura(size * 1.4f);
    aura.setOrigin(aura.getRadius(), aura.getRadius());
    aura.setPosition(cx, cy);
    aura.setFillColor(sf::Color(120, 180, 255, 45));
    window.draw(aura);

    sf::CircleShape body(size * 0.62f);
    body.setOrigin(body.getRadius(), body.getRadius());
    body.setPosition(cx, cy);
    body.setFillColor(sf::Color(70, 90, 120));
    body.setOutlineThickness(2.0f);
    body.setOutlineColor(sf::Color(210, 230, 255));
    window.draw(body);

    sf::CircleShape pole(size * 0.18f);
    pole.setOrigin(pole.getRadius(), pole.getRadius());
    pole.setPosition(cx, cy - size * 0.15f);
    pole.setFillColor(sf::Color(230, 240, 255));
    window.draw(pole);

    sf::CircleShape leftTip(size * 0.28f);
    leftTip.setOrigin(size * 0.28f, size * 0.28f);
    leftTip.setPosition(cx - size * 0.42f + std::sin(spin) * 1.5f, cy + size * 0.05f);
    leftTip.setFillColor(sf::Color(220, 70, 70));
    window.draw(leftTip);

    sf::CircleShape rightTip(size * 0.28f);
    rightTip.setOrigin(size * 0.28f, size * 0.28f);
    rightTip.setPosition(cx + size * 0.42f + std::sin(spin + 1.2f) * 1.5f, cy + size * 0.05f);
    rightTip.setFillColor(sf::Color(70, 120, 230));
    window.draw(rightTip);
}

void GameClient::drawSpeedBoostItem(sf::RenderWindow& window, float cx, float cy, float size, float spin) const {
    const float bob = std::sin(animTime_ * 6.0f + cx * 0.015f) * 2.5f;
    cy += bob;

    sf::CircleShape aura(size * 1.25f);
    aura.setOrigin(aura.getRadius(), aura.getRadius());
    aura.setPosition(cx, cy);
    aura.setFillColor(sf::Color(255, 200, 80, 40));
    window.draw(aura);

    sf::CircleShape core(size * 0.5f);
    core.setOrigin(core.getRadius(), core.getRadius());
    core.setPosition(cx, cy);
    core.setFillColor(sf::Color(255, 170, 40));
    core.setOutlineThickness(2.0f);
    core.setOutlineColor(sf::Color(255, 240, 180));
    window.draw(core);

    for (int i = 0; i < 3; ++i) {
        const float angle = spin + static_cast<float>(i) * 2.1f;
        sf::CircleShape bolt(size * 0.12f, 3);
        bolt.setOrigin(bolt.getRadius(), bolt.getRadius());
        bolt.setPosition(cx + std::cos(angle) * size * 0.55f, cy + std::sin(angle) * size * 0.35f);
        bolt.setFillColor(sf::Color(255, 255, 140));
        bolt.setRotation(angle * 57.3f);
        window.draw(bolt);
    }
}

void GameClient::drawMagnetDrops(sf::RenderWindow& window) const {
    for (uint8_t i = 0; i < renderWorld_.magnetDropCount; ++i) {
        const WorldState::SyncMagnetDrop& drop = renderWorld_.magnetDrops[i];
        if (drop.active == 0) {
            continue;
        }
        const float cx = drop.x;
        const float cy = drop.y + POWERUP_DROP_HEIGHT * 0.5f;
        const float spin = animTime_ * 6.0f + static_cast<float>(i);
        const float itemSize = POWERUP_VISUAL_SIZE;

        if (drop.falling != 0) {
            sf::VertexArray trail(sf::Lines, 6);
            for (int t = 0; t < 3; ++t) {
                const float offset = static_cast<float>(t) * 18.0f + std::fmod(animTime_ * 80.0f + i * 20.0f, 18.0f);
                trail[t * 2].position = {cx, cy - offset};
                trail[t * 2].color = sf::Color(255, 255, 255, static_cast<sf::Uint8>(150 - t * 40));
                trail[t * 2 + 1].position = {cx, cy - offset - 10.0f};
                trail[t * 2 + 1].color = sf::Color(255, 255, 255, 0);
            }
            window.draw(trail);
        }

        if (drop.kind == static_cast<uint8_t>(PowerUpKind::SpeedBoost)) {
            drawSpeedBoostItem(window, cx, cy, itemSize, spin);
        } else {
            drawMagnetItem(window, cx, cy, itemSize, spin);
        }
    }
}

void GameClient::drawMagnetPulls(sf::RenderWindow& window) const {
    constexpr int kBananaGid = 304;
    for (uint8_t i = 0; i < renderWorld_.magnetPullCount; ++i) {
        const WorldState::SyncMagnetPull& pull = renderWorld_.magnetPulls[i];
        if (pull.active == 0) {
            continue;
        }

        sf::CircleShape spark(3.0f);
        spark.setOrigin(3.0f, 3.0f);
        spark.setPosition(pull.x, pull.y);
        spark.setFillColor(sf::Color(220, 240, 255, 180));
        window.draw(spark);

        if (pull.kind == 0) {
            if (assets_.ready()) {
                const sf::Texture& gemTex = ((pull.gemTx + pull.gemTy) % 2 == 0) ? assets_.gemRed() : assets_.gemBlue();
                sf::Sprite gem(gemTex);
                const float gemScale =
                    (TILE_SIZE * 0.75f) / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y));
                gem.setScale(gemScale, gemScale);
                gem.setOrigin(gemTex.getSize().x * gemScale * 0.5f, gemTex.getSize().y * gemScale * 0.5f);
                gem.setPosition(pull.x, pull.y);
                window.draw(gem);
            }
        } else if (tiledMap_.ready()) {
            const float size = TILE_SIZE;
            tiledMap_.drawObjectGidAt(window, kBananaGid, pull.x - size * 0.5f, pull.y - size * 0.5f, size, size,
                                      animTime_ + static_cast<float>(pull.pickupIndex) * 0.07f);
        }
    }
}

void GameClient::drawMagnetAura(sf::RenderWindow& window, const PlayerState& player) const {
    if (!player.alive || player.magnetTimer <= 0.0f) {
        return;
    }

    const float cx = player.x + PLAYER_WIDTH * 0.5f;
    const float cy = player.y + PLAYER_HEIGHT * 0.5f;
    const float pulse = 0.85f + 0.15f * std::sin(animTime_ * 8.0f);
    const float radius = MAGNET_RADIUS * pulse;

    sf::CircleShape ring(radius);
    ring.setOrigin(radius, radius);
    ring.setPosition(cx, cy);
    ring.setFillColor(sf::Color(120, 180, 255, 18));
    ring.setOutlineThickness(2.0f);
    ring.setOutlineColor(sf::Color(180, 220, 255, 90));
    window.draw(ring);

    for (int i = 0; i < 6; ++i) {
        const float angle = animTime_ * 2.5f + static_cast<float>(i) * (6.2831853f / 6.0f);
        const float sparkR = radius * 0.72f;
        sf::CircleShape spark(2.5f);
        spark.setOrigin(2.5f, 2.5f);
        spark.setPosition(cx + std::cos(angle) * sparkR, cy + std::sin(angle) * sparkR);
        spark.setFillColor(sf::Color(220, 240, 255, 160));
        window.draw(spark);
    }
}

void GameClient::drawSpeedBoostAura(sf::RenderWindow& window, const PlayerState& player) const {
    if (!player.alive || player.speedBoostTimer <= 0.0f) {
        return;
    }

    const float cx = player.x + PLAYER_WIDTH * 0.5f;
    const float cy = player.y + PLAYER_HEIGHT * 0.5f;
    const float pulse = 0.7f + 0.3f * std::sin(animTime_ * 12.0f);

    for (int i = 0; i < 4; ++i) {
        const float trailX = cx - player.vx * 0.04f * static_cast<float>(i + 1);
        const float trailY = cy + std::sin(animTime_ * 10.0f + i) * 2.0f;
        sf::CircleShape trail(5.0f - static_cast<float>(i));
        trail.setOrigin(trail.getRadius(), trail.getRadius());
        trail.setPosition(trailX, trailY);
        trail.setFillColor(sf::Color(255, 210, 80, static_cast<sf::Uint8>((90 - i * 18) * pulse)));
        window.draw(trail);
    }

    sf::CircleShape ring(TILE_SIZE * 0.55f * pulse);
    ring.setOrigin(ring.getRadius(), ring.getRadius());
    ring.setPosition(cx, cy);
    ring.setFillColor(sf::Color(255, 190, 60, 22));
    ring.setOutlineThickness(2.0f);
    ring.setOutlineColor(sf::Color(255, 230, 120, 110));
    window.draw(ring);
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
        drawSawTraps(window);
        drawRockHeads(window);
        drawPendulums(window);
        drawFlyingEnemies(window);
        drawTridentProjectiles(window);
        drawMagnetDrops(window);
        tiledMap_.drawCollectibles(window, animTime_, renderWorld_.collectedPickupsMask,
                                   renderWorld_.collectedPickupsMaskHi, renderWorld_.collectedPickupsMaskExt,
                                   [this](uint8_t pickupIndex) { return isFruitMagnetPulled(pickupIndex); });
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
                const float gemScale =
                    tileSize / static_cast<float>(std::max(gemTex.getSize().x, gemTex.getSize().y)) * GEM_VISUAL_SCALE;
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

    drawMagnetAura(window, player);
    drawSpeedBoostAura(window, player);

    const float footLift = tiledMap_.ready() ? TILED_GROUND_FOOT_LIFT : 0.0f;

    if (assets_.ready() && assets_.character(player.role).ready()) {
        const InputFlags facingHint = player.role == role_ ? currentInput_ : InputFlags::None;
        assets_.character(player.role).draw(window, player, animTime_, facingHint, footLift);
        return;
    }

    const float visualH = PLAYER_VISUAL_HEIGHT;
    const float visualW = PLAYER_WIDTH * (visualH / PLAYER_HEIGHT);
    const float visualX = player.x + (PLAYER_WIDTH - visualW) * 0.5f;
    const float visualY = player.y + PLAYER_HEIGHT - visualH - footLift;

    sf::RectangleShape shadow({visualW, visualH});
    shadow.setPosition(visualX + 3.0f, visualY + 4.0f);
    shadow.setFillColor(sf::Color(0, 0, 0, 70));
    window.draw(shadow);

    sf::RectangleShape body({visualW, visualH});
    body.setPosition(visualX, visualY);

    if (!player.alive) {
        body.setFillColor(sf::Color(80, 80, 80, 120));
    } else if (player.role == PlayerRole::Fire) {
        body.setFillColor(sf::Color(255, 90, 40));
    } else if (player.role == PlayerRole::Water) {
        body.setFillColor(sf::Color(60, 140, 255));
    } else if (player.role == PlayerRole::Poison) {
        body.setFillColor(sf::Color(64, 190, 78));
    }
    body.setOutlineThickness(2.0f);
    body.setOutlineColor(player.role == PlayerRole::Poison ? sf::Color(20, 80, 30) : sf::Color(30, 30, 35));
    window.draw(body);

    if (player.role == PlayerRole::Poison && player.alive) {
        sf::CircleShape glow(12.0f);
        glow.setOrigin(12.0f, 12.0f);
        glow.setPosition(player.x + PLAYER_WIDTH / 2.0f, visualY + 12.0f);
        glow.setFillColor(sf::Color(160, 255, 120, 75));
        window.draw(glow);
    }
}

void GameClient::drawPowerUpStatus(sf::RenderWindow& window, float hudY) const {
    if (renderWorld_.phase != GamePhase::Playing || !connected_ || slot_ >= MAX_PLAYERS) {
        return;
    }

    const PlayerState& player = renderWorld_.players[slot_];
    float iconX = 16.0f;
    const float iconY = hudY + 42.0f;
    const float iconSize = 18.0f;

    if (player.magnetTimer > 0.0f) {
        const float cx = iconX + iconSize * 0.5f;
        const float cy = iconY + iconSize * 0.5f;
        drawMagnetItem(window, cx, cy, iconSize, animTime_ * 4.0f);
        const float barW = 52.0f;
        const float barH = 6.0f;
        sf::RectangleShape barBg({barW, barH});
        barBg.setPosition(iconX + iconSize + 4.0f, iconY + iconSize * 0.5f - barH * 0.5f);
        barBg.setFillColor(sf::Color(40, 40, 40, 180));
        window.draw(barBg);
        sf::RectangleShape barFill({barW * std::min(1.0f, player.magnetTimer / MAGNET_DURATION), barH});
        barFill.setPosition(barBg.getPosition());
        barFill.setFillColor(sf::Color(120, 180, 255));
        window.draw(barFill);
        ui_.drawText(window, "Magnet", iconX + iconSize + barW + 8.0f, iconY + 2.0f, 14, sf::Color(180, 210, 255));
        iconX += iconSize + barW + 72.0f;
    }

    if (player.speedBoostTimer > 0.0f) {
        const float cx = iconX + iconSize * 0.5f;
        const float cy = iconY + iconSize * 0.5f;
        drawSpeedBoostItem(window, cx, cy, iconSize, animTime_ * 4.0f);
        const float barW = 52.0f;
        const float barH = 6.0f;
        sf::RectangleShape barBg({barW, barH});
        barBg.setPosition(iconX + iconSize + 4.0f, iconY + iconSize * 0.5f - barH * 0.5f);
        barBg.setFillColor(sf::Color(40, 40, 40, 180));
        window.draw(barBg);
        sf::RectangleShape barFill({barW * std::min(1.0f, player.speedBoostTimer / SPEED_BOOST_DURATION), barH});
        barFill.setPosition(barBg.getPosition());
        barFill.setFillColor(sf::Color(255, 190, 60));
        window.draw(barFill);
        ui_.drawText(window, "Speed", iconX + iconSize + barW + 8.0f, iconY + 2.0f, 14, sf::Color(255, 220, 140));
    }
}

void GameClient::drawHud(sf::RenderWindow& window) const {
    const float hudY = static_cast<float>(map_.height()) * TILE_SIZE + 8.0f;
    const float windowW = static_cast<float>(window.getSize().x);

    sf::RectangleShape hudBar({windowW, 72.0f});
    hudBar.setPosition(0.0f, hudY);
    hudBar.setFillColor(sf::Color(18, 22, 34, 232));
    hudBar.setOutlineThickness(2.0f);
    hudBar.setOutlineColor(sf::Color(255, 255, 255, 35));
    window.draw(hudBar);

    sf::RectangleShape hudShine({windowW, 3.0f});
    hudShine.setPosition(0.0f, hudY + 3.0f);
    hudShine.setFillColor(sf::Color(255, 255, 255, 35));
    window.draw(hudShine);
    const int totalGems = renderWorld_.players[0].gems + renderWorld_.players[1].gems;
    ui_.drawText(window, "Gems: " + std::to_string(totalGems) + "/" + std::to_string(renderWorld_.totalGems), 16.0f,
                 hudY + 16.0f, 20, sf::Color(255, 220, 80));

    const std::string levelLine =
        "Level " + std::to_string(renderWorld_.levelIndex + 1) + ": " + renderWorld_.levelName;
    ui_.drawText(window, levelLine, 180.0f, hudY + 16.0f, 18, sf::Color(220, 220, 220));

    if (renderWorld_.phase == GamePhase::Playing) {
        const char* controlText = "IJKL/Space";
        if (role_ == PlayerRole::Fire) {
            controlText = "WASD/Space";
        } else if (role_ == PlayerRole::Water) {
            controlText = "Arrows/Space";
        }
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

    drawPowerUpStatus(window, hudY);
}

sf::FloatRect GameClient::pauseButtonRect() const {
    const float mapW = static_cast<float>(map_.width()) * TILE_SIZE;
    const float windowW = static_cast<float>(window_.getSize().x);
    const float x = std::min(mapW, windowW) - 52.0f;
    return {x, 12.0f, 40.0f, 40.0f};
}

void GameClient::drawPauseButton(sf::RenderWindow& window) const {
    const sf::FloatRect area = pauseButtonRect();
    if (assets_.hasPauseIcon()) {
        ui_.drawImageButton(window, area, assets_.pauseIcon(), true, true);
        return;
    }

    const float barW = 8.0f;
    const float barH = 24.0f;
    const float gap = 6.0f;
    const float cx = area.left + area.width / 2.0f;
    const float cy = area.top + area.height / 2.0f;
    sf::RectangleShape leftBar({barW, barH});
    leftBar.setFillColor(sf::Color(255, 220, 60));
    leftBar.setPosition(cx - gap / 2.0f - barW, cy - barH / 2.0f);
    window.draw(leftBar);
    sf::RectangleShape rightBar({barW, barH});
    rightBar.setFillColor(sf::Color(255, 220, 60));
    rightBar.setPosition(cx + gap / 2.0f, cy - barH / 2.0f);
    window.draw(rightBar);
}

void GameClient::drawPauseOverlay(sf::RenderWindow& window, float centerX) const {
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;
    const float windowW = static_cast<float>(window.getSize().x);

    sf::RectangleShape dim({windowW, mapH});
    dim.setFillColor(sf::Color(0, 0, 0, 120));
    window.draw(dim);

    const sf::Texture* panelTex = assets_.hasPauseMenu() ? &assets_.pauseMenu() : nullptr;
    if (panelTex != nullptr) {
        const PauseMenuPanelDraw panelDraw = computePauseMenuPanelDraw(mapH, centerX, panelTex);
        drawPauseMenuTexture(window, panelDraw, *panelTex);
        return;
    }

    drawPauseStyleMenuPanel(window, ui_, centerX, mapH);
}

bool GameClient::handlePauseMenuClick(const sf::Event& event) {
    if (event.type != sf::Event::MouseButtonPressed || event.mouseButton.button != sf::Mouse::Left) {
        return false;
    }

    const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
    const float centerX = static_cast<float>(window_.getSize().x) / 2.0f;
    const float mapH = static_cast<float>(map_.height()) * TILE_SIZE;
    const sf::Texture* panelTex = assets_.hasPauseMenu() ? &assets_.pauseMenu() : nullptr;
    const PauseMenuPanelDraw panelDraw = computePauseMenuPanelDraw(mapH, centerX, panelTex);

    const sf::FloatRect menuBtn = panelDraw.menuBtn();
    const sf::FloatRect retryBtn = panelDraw.retryBtn();
    const sf::FloatRect continueBtn = panelDraw.continueBtn();

    if (continueBtn.contains(mouse)) {
        paused_ = false;
        return true;
    }
    if (menuBtn.contains(mouse)) {
        paused_ = false;
        localReady_ = false;
        sendAction(PlayerAction::ReturnToLobby);
        return true;
    }
    if (retryBtn.contains(mouse)) {
        paused_ = false;
        sendAction(PlayerAction::Restart);
        return true;
    }
    return true;
}

bool GameClient::handleResultOverlayClick(const sf::Event& event) {
    if (event.type != sf::Event::MouseButtonPressed || event.mouseButton.button != sf::Mouse::Left) {
        return false;
    }

    const sf::Vector2f mouse(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
    const float centerX = static_cast<float>(window_.getSize().x) / 2.0f;
    const float mapH = resultOverlayMapHeight(map_.height(), tiledMap_.ready() ? tiledMap_.mapHeight() : map_.height(),
                                              tiledMap_.ready(), renderWorld_.levelIndex);

    if (renderWorld_.phase == GamePhase::GameOver) {
        const GameOverLayout layout = computeGameOverLayout(mapH, centerX, static_cast<float>(window_.getSize().x));

        if (layout.menuBtn.contains(mouse)) {
            localReady_ = false;
            sendAction(PlayerAction::ReturnToLobby);
            return true;
        }
        if (layout.retryBtn.contains(mouse)) {
            localReady_ = false;
            sendAction(PlayerAction::Restart);
            return true;
        }
        return false;
    }

    const bool hasNextLevel =
        renderWorld_.levelIndex + 1 < renderWorld_.levelCount &&
        isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(renderWorld_.levelIndex + 1));
    const VictoryLayout layout = computeVictoryLayout(mapH, centerX, static_cast<float>(window_.getSize().x));

    if (layout.retryBtn.contains(mouse)) {
        localReady_ = false;
        sendAction(PlayerAction::Restart);
        return true;
    }
    if (hasNextLevel && layout.nextBtn.contains(mouse)) {
        localReady_ = false;
        sendAction(PlayerAction::NextLevel);
        return true;
    }
    if (layout.menuBtn.contains(mouse)) {
        localReady_ = false;
        sendAction(PlayerAction::ReturnToLobby);
        return true;
    }
    return false;
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
    const float mapH = resultOverlayMapHeight(map_.height(), tiledMap_.ready() ? tiledMap_.mapHeight() : map_.height(),
                                              tiledMap_.ready(), renderWorld_.levelIndex);
    const float windowW = static_cast<float>(window.getSize().x);

    sf::RectangleShape dim({windowW, mapH});
    dim.setFillColor(sf::Color(0, 0, 0, victory ? 90 : 120));
    window.draw(dim);

    const int totalGems = renderWorld_.players[0].gems + renderWorld_.players[1].gems;
    int gemGoal = renderWorld_.totalGems;
    if (renderWorld_.levelIndex == 0 && gemGoal == 0 && tiledMap_.ready()) {
        gemGoal = static_cast<int>(std::min(255, tiledMap_.collectibleCount()));
    }

    if (!victory) {
        const GameOverLayout layout = computeGameOverLayout(mapH, centerX, windowW);
        const sf::FloatRect panelRect{centerX - layout.panelDrawW / 2.0f, layout.panelTop, layout.panelDrawW,
                                      layout.panelDrawH};

        ui_.drawPanel(window, panelRect, sf::Color(175, 175, 175), 240.0f);
        sf::RectangleShape innerFrame(
            {panelRect.width - layout.uiScale * 28.0f, panelRect.height - layout.uiScale * 28.0f});
        innerFrame.setPosition(panelRect.left + layout.uiScale * 14.0f, panelRect.top + layout.uiScale * 14.0f);
        innerFrame.setFillColor(sf::Color(165, 165, 165, 220));
        innerFrame.setOutlineThickness(3.0f);
        innerFrame.setOutlineColor(sf::Color(90, 90, 90));
        window.draw(innerFrame);

        const unsigned titleSize = static_cast<unsigned>(36.0f * layout.uiScale);
        const unsigned scoreSize = static_cast<unsigned>(24.0f * layout.uiScale);
        ui_.drawOutlinedCenteredText(window, text::resultGameOverTitle(), centerX, layout.titleY, titleSize,
                                     sf::Color(255, 220, 80), sf::Color(40, 30, 20), 3.0f);

        const std::string scoreLine = text::resultGemScore(totalGems, gemGoal);
        ui_.drawOutlinedCenteredText(window, scoreLine, centerX, layout.scoreY, scoreSize, sf::Color(255, 220, 80),
                                     sf::Color(40, 30, 20), 2.5f);

        const unsigned btnFontSize = static_cast<unsigned>(22.0f * layout.uiScale);
        const auto drawResultButton = [&](const sf::FloatRect& area, const std::string& label) {
            ui_.drawPanel(window, area, sf::Color(190, 190, 190), 255.0f);
            const float textY = area.top + (area.height - static_cast<float>(btnFontSize)) * 0.38f;
            ui_.drawOutlinedCenteredText(window, label, area.left + area.width / 2.0f, textY, btnFontSize,
                                         sf::Color(255, 220, 80), sf::Color(40, 30, 20), 2.0f);
        };
        drawResultButton(layout.menuBtn, text::resultMenuButton());
        drawResultButton(layout.retryBtn, text::resultRetryButton());
        return;
    }

    const bool hasNextLevel =
        renderWorld_.levelIndex + 1 < renderWorld_.levelCount &&
        isLevelUnlocked(renderWorld_.unlockedMask, static_cast<uint8_t>(renderWorld_.levelIndex + 1));
    const std::string scoreLine = text::resultGemScore(totalGems, gemGoal);
    const VictoryLayout layout = computeVictoryLayout(mapH, centerX, windowW);

    if (assets_.hasVictoryUi()) {
        sf::Sprite panel(assets_.victoryMenu());
        panel.setScale(layout.panelDrawW / static_cast<float>(panel.getTexture()->getSize().x),
                       layout.panelDrawH / static_cast<float>(panel.getTexture()->getSize().y));
        panel.setPosition(centerX - layout.panelDrawW / 2.0f, layout.panelTop);
        panel.setColor(sf::Color(255, 252, 245));
        window.draw(panel);

        const unsigned titleSize = static_cast<unsigned>(44.0f * layout.uiScale);
        const unsigned scoreSize = static_cast<unsigned>(26.0f * layout.uiScale);
        ui_.drawVictoryTitleCentered(window, text::resultVictoryTitle(), centerX, layout.titleY, titleSize, animTime_);
        ui_.drawVictoryScoreCentered(window, scoreLine, centerX, layout.scoreY, scoreSize);

        drawVictoryLabelButton(window, ui_, layout.menuBtn, assets_.victoryButtonMenu(), text::resultMenuButton(), true,
                               layout.uiScale, sf::Color(238, 242, 248));
        drawVictoryLabelButton(window, ui_, layout.retryBtn, assets_.victoryButtonRetry(), text::resultRetryButton(),
                               true, layout.uiScale, sf::Color(255, 246, 218));
        drawVictoryLabelButton(window, ui_, layout.nextBtn, assets_.victoryButtonNext(), text::resultNextLevelButton(),
                               hasNextLevel, layout.uiScale, sf::Color(228, 246, 232));
        return;
    }

    const sf::FloatRect panelRect{centerX - layout.panelDrawW / 2.0f, layout.panelTop, layout.panelDrawW,
                                  layout.panelDrawH};

    ui_.drawPanel(window, panelRect, sf::Color(218, 212, 200), 235.0f);
    sf::RectangleShape innerFrame(
        {panelRect.width - layout.uiScale * 28.0f, panelRect.height - layout.uiScale * 28.0f});
    innerFrame.setPosition(panelRect.left + layout.uiScale * 14.0f, panelRect.top + layout.uiScale * 14.0f);
    innerFrame.setFillColor(sf::Color(235, 230, 220, 220));
    innerFrame.setOutlineThickness(2.0f);
    innerFrame.setOutlineColor(sf::Color(180, 170, 155, 180));
    window.draw(innerFrame);

    const unsigned titleSize = static_cast<unsigned>(44.0f * layout.uiScale);
    const unsigned scoreSize = static_cast<unsigned>(26.0f * layout.uiScale);
    ui_.drawVictoryTitleCentered(window, text::resultVictoryTitle(), centerX, layout.titleY, titleSize, animTime_);
    ui_.drawVictoryScoreCentered(window, scoreLine, centerX, layout.scoreY, scoreSize);

    ui_.drawButton(window, layout.menuBtn, text::resultMenuButton(), true, sf::Color(200, 208, 216));
    ui_.drawButton(window, layout.retryBtn, text::resultRetryButton(), true, sf::Color(238, 218, 168));
    ui_.drawButton(window, layout.nextBtn, text::resultNextLevelButton(), hasNextLevel,
                   hasNextLevel ? sf::Color(188, 218, 198) : sf::Color(185, 188, 182));
}

const char* GameClient::roleDisplayName() const {
    switch (role_) {
        case PlayerRole::Fire:
            return "Ninja Frog";
        case PlayerRole::Water:
            return "Pink Man";
        case PlayerRole::Poison:
            return "Mask Dude";
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
