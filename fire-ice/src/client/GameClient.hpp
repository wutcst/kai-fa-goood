#pragma once

#include "AssetManager.hpp"
#include "GameServer.hpp"
#include "Map.hpp"
#include "Protocol.hpp"
#include "UiHelper.hpp"

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
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
    bool initialize(const std::string& host, PlayerRole preferredRole);
    void run();
    void disconnect();
    bool startHosting();
    void stopHosting();

private:
    void pollNetwork();
    void onPhaseChanged(GamePhase previous, GamePhase current);
    void updateMusic(GamePhase phase);
    void beginConnect();
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
    InputFlags readLocalInput() const;
    void render();
    void renderTitleScreen();
    void renderTitleCharacters();
    void renderTitleSpotlight();
    void renderHelpOverlay();
    void renderCreditsOverlay();
    void renderLobbyScreen();
    void renderRoomScreen();
    void renderRoomPlayerPanel(float panelX, float panelY, float panelW, float panelH, int playerSlot,
                               PlayerRole expectedRole);
    void renderJoinRoomScreen();
    void renderJoinRoomScanResults();
    void renderGameScreen();
    void broadcastDiscovery();
    void handleDiscoveryResponse(const DiscoveryPacket& packet, const sf::IpAddress& sender);
    void drawBackgroundSprite(sf::RenderWindow& window, const sf::Texture& texture) const;
    void drawMap(sf::RenderWindow& window) const;
    void drawMapPreview(sf::RenderWindow& window, const sf::FloatRect& area) const;
    void drawPlayer(sf::RenderWindow& window, const PlayerState& player) const;
    void drawHud(sf::RenderWindow& window) const;
    void drawCountdownOverlay(sf::RenderWindow& window, float centerX) const;
    void drawResultOverlay(sf::RenderWindow& window, float centerX, bool victory) const;
    void drawConnectingScreen(sf::RenderWindow& window) const;
    sf::Color tileColor(TileType type) const;
    const char* roleDisplayName() const;
    const char* roleChineseName() const;

    sf::UdpSocket socket_;
    sf::RenderWindow window_;
    GameMap map_;
    UiHelper ui_;
    AssetManager assets_;
    sf::Music lobbyMusic_;
    WorldState world_{};
    WorldState renderWorld_{};
    uint8_t loadedLevelIndex_ = 255;
    bool lobbyLayout_ = true;
    bool musicEnabled_ = false;

    ClientScreen clientScreen_ = ClientScreen::Title;
    int titleMenuIndex_ = 0;
    bool connectRequested_ = false;
    int titleHoverIndex_ = -1;

    std::string host_;
    sf::IpAddress serverAddress_;
    unsigned short localPort_ = 0;
    PlayerRole preferredRole_ = PlayerRole::Fire;

    bool connected_ = false;
    bool localReady_ = false;
    uint8_t slot_ = 0;
    PlayerRole role_ = PlayerRole::None;
    std::string playerName_;
    std::string typedRoomCode_;
    float roomAnimTimer_ = 0.0f;

    InputFlags currentInput_ = InputFlags::None;
    uint32_t inputTick_ = 0;

    std::chrono::steady_clock::time_point lastInputSend_;
    std::chrono::steady_clock::time_point lastConnectRetry_;

    bool isHosting_ = false;
    std::unique_ptr<GameServer> server_;
    std::thread serverThread_;
    std::string localIp_;

    struct DiscoveredRoom {
        std::string address;
        std::string roomCode;
        std::string levelName;
        uint8_t playerCount = 0;
        uint8_t maxPlayers = 0;
    };
    std::vector<DiscoveredRoom> discoveredRooms_;
    bool discoveryActive_ = false;
    float discoveryTimer_ = 0.0f;
    int selectedDiscoveredRoom_ = -1;
};

}  // namespace fireice
