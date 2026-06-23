#pragma once

#include "AssetManager.hpp"
#include "ClientNetwork.hpp"
#include "LevelMechanics.hpp"
#include "LocalGameSession.hpp"
#include "Map.hpp"
#include "Protocol.hpp"
#include "Pickup.hpp"
#include "TiledMapRenderer.hpp"
#include "UiHelper.hpp"

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fireice {

enum class ClientScreen : uint8_t {
    Title = 0,
    Help,
    Credits,
    Connecting,
    Room,
    JoinRoom,
};

class GameClient {
public:
    bool initialize(const std::string& host, PlayerRole preferredRole, bool autoConnect = false, bool autoSolo = false);
    void run();
    void disconnect();
    void usePublicServer();
    void toggleWaitingReadyLocal();
    bool isPlayerSlotConnected(uint8_t slot) const;

private:
    void applyNetworkWorldState(const WorldState& incoming);
    void pollNetwork();
    void onPhaseChanged(GamePhase previous, GamePhase current);
    void updateMusic(GamePhase phase);
    void beginConnect();
    void quickJoin();
    void startSoloPlay();
    void stopSoloPlay();
    void updateSoloSession(float dt);
    void syncWorldFromSession();
    void sendConnectRequest();
    void handleTitleMenuSelect(int index);
    void handleTitleInput(const sf::Event& event);
    void handleTitleMouseMove(const sf::Event& event);
    void handleTitleMouseClick(const sf::Event& event);
    std::vector<sf::FloatRect> titleMenuHitAreas() const;
    bool sendInput();
    bool sendAction(PlayerAction action, uint8_t value = 0);
    void handleWindowEvent(const sf::Event& event);
    void handleLevelChange(uint8_t levelIndex, bool resizeWindow);
    void useTitleLayout();
    void useLobbyLayout();
    void useGameLayout();
    void ensureLayoutView();
    InputFlags readLocalInput() const;
    void render();
    void renderTitleScreen();
    void renderTitleBackground();
    void renderTitleEffects();
    void renderTitleCharacters();
    void drawLobbyBackdrop(sf::Color top, sf::Color bottom);
    void renderHelpOverlay();
    void renderCreditsOverlay();
    void renderLobbyScreen();
    void drawLevelProgressMap();
    void drawLevelNode(uint8_t index, uint8_t levelCount, bool selected, bool unlocked, bool completed);
    void drawLevelPath(uint8_t fromIndex, uint8_t toIndex, uint8_t levelCount, bool unlocked);
    void handleLobbyMouseClick(const sf::Event& event);
    void handleLobbyMouseWheel(const sf::Event& event);
    void scrollLevelMapToNode(uint8_t index);
    int levelNodeAtPosition(float x, float y) const;
    void trySelectLevel(uint8_t index);
    void renderRoomScreen();
    void renderRoomPlayerPanel(float panelX, float panelY, float panelW, float panelH, int playerSlot,
                               PlayerRole expectedRole);
    void renderJoinRoomScreen();
    void renderGameScreen();
    void drawMap(sf::RenderWindow& window) const;
    void drawFanWindEffects(sf::RenderWindow& window) const;
    void drawMagnetDrops(sf::RenderWindow& window) const;
    void drawMagnetPulls(sf::RenderWindow& window) const;
    void drawMagnetAura(sf::RenderWindow& window, const PlayerState& player) const;
    void drawSpeedBoostAura(sf::RenderWindow& window, const PlayerState& player) const;
    void drawMagnetItem(sf::RenderWindow& window, float cx, float cy, float size, float spin) const;
    void drawSpeedBoostItem(sf::RenderWindow& window, float cx, float cy, float size, float spin) const;
    bool isFruitMagnetPulled(uint8_t pickupIndex) const;
    bool isGemMagnetPulled(int tx, int ty) const;
    void drawMudParticles(sf::RenderWindow& window) const;
    void drawSawTraps(sf::RenderWindow& window) const;
    void drawRockHeads(sf::RenderWindow& window) const;
    void drawFlyingEnemies(sf::RenderWindow& window) const;
    void drawTridentProjectiles(sf::RenderWindow& window) const;
    void drawPendulums(sf::RenderWindow& window) const;
    void drawDynamicTiles(sf::RenderWindow& window) const;
    void drawMapPreview(sf::RenderWindow& window, const sf::FloatRect& area) const;
    void drawPlayer(sf::RenderWindow& window, const PlayerState& player) const;
    void drawHud(sf::RenderWindow& window) const;
    void drawPowerUpStatus(sf::RenderWindow& window, float hudY) const;
    void drawCountdownOverlay(sf::RenderWindow& window, float centerX) const;
    void drawResultOverlay(sf::RenderWindow& window, float centerX, bool victory) const;
    void drawPauseButton(sf::RenderWindow& window) const;
    void drawPauseOverlay(sf::RenderWindow& window, float centerX) const;
    sf::FloatRect pauseButtonRect() const;
    bool handlePauseMenuClick(const sf::Event& event);
    bool handleResultOverlayClick(const sf::Event& event);
    void drawConnectingScreen(sf::RenderWindow& window) const;
    sf::Color tileColor(TileType type) const;
    const char* roleDisplayName() const;
    const char* roleChineseName() const;
    bool localWaitingReady() const;
    bool allPlayersWaitingReady() const;
    sf::Vector2f mapMousePos(int pixelX, int pixelY) const;
    bool hitButtonArea(const sf::FloatRect& area, sf::Vector2f mouse) const;
    void requestWaitingReadyToggle();
    void applyWaitingReadyIntent();
    void retryPendingLobbyActions();

    ClientNetwork network_;
    sf::RenderWindow window_;
    GameMap map_;
    TiledMapRenderer tiledMap_;
    std::vector<FanZone> fanZones_;
    UiHelper ui_;
    AssetManager assets_;
    sf::Music lobbyMusic_;
    WorldState world_{};
    WorldState renderWorld_{};
    uint8_t loadedLevelIndex_ = 255;
    std::string loadedVisualPath_;
    std::filesystem::file_time_type loadedVisualFileTime_{};
    std::filesystem::file_time_type loadedCollisionFileTime_{};
    bool lobbyLayout_ = true;
    bool musicEnabled_ = false;

    ClientScreen clientScreen_ = ClientScreen::Title;
    int titleMenuIndex_ = 0;
    bool connectRequested_ = false;
    int titleHoverIndex_ = -1;
    int lobbyHoverNode_ = -1;
    float levelMapScrollY_ = 0.0f;

    std::string host_;
    PlayerRole preferredRole_ = PlayerRole::Fire;

    bool connected_ = false;
    bool localReady_ = false;
    bool paused_ = false;
    uint8_t slot_ = 0;
    PlayerRole role_ = PlayerRole::None;
    std::string playerName_;
    std::string typedRoomCode_;
    char acceptedRoomCode_[MAX_ROOM_CODE]{};
    std::optional<bool> waitingReadyIntent_;
    std::chrono::steady_clock::time_point waitingReadyIntentTime_{};
    int roomHoverButton_ = -1;
    float roomAnimTimer_ = 0.0f;
    float animTime_ = 0.0f;
    sf::Clock animClock_;

    InputFlags currentInput_ = InputFlags::None;
    InputFlags lastSentInput_ = InputFlags::None;
    uint32_t inputTick_ = 0;

    std::chrono::steady_clock::time_point lastInputSend_;
    std::chrono::steady_clock::time_point lastConnectRetry_;

    bool localServerMode_ = false;
    bool soloMode_ = false;
    std::unique_ptr<LocalGameSession> soloSession_;
};

}  // namespace fireice
