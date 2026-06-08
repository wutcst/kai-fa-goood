#pragma once

#include "Map.hpp"
#include "Protocol.hpp"
#include "UiHelper.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string>

namespace fireice {

class GameClient {
public:
    bool connect(const std::string& host, PlayerRole preferredRole);
    void run();
    void disconnect();

private:
    void pollNetwork();
    void onPhaseChanged(GamePhase previous, GamePhase current);
    bool sendInput();
    bool sendAction(PlayerAction action, uint8_t value = 0);
    void handleWindowEvent(const sf::Event& event);
    void handleLevelChange(uint8_t levelIndex, bool resizeWindow);
    void useLobbyLayout();
    void useGameLayout();
    InputFlags readLocalInput() const;
    void render();
    void renderLobbyScreen();
    void renderGameScreen();
    void drawMap(sf::RenderWindow& window) const;
    void drawMapPreview(sf::RenderWindow& window, const sf::FloatRect& area) const;
    void drawPlayer(sf::RenderWindow& window, const PlayerState& player) const;
    void drawHud(sf::RenderWindow& window) const;
    void drawCountdownOverlay(sf::RenderWindow& window, float centerX) const;
    void drawResultOverlay(sf::RenderWindow& window, float centerX, bool victory) const;
    void drawConnectingScreen(sf::RenderWindow& window) const;
    sf::Color tileColor(TileType type) const;
    const char* roleDisplayName() const;

    sf::UdpSocket socket_;
    sf::RenderWindow window_;
    GameMap map_;
    UiHelper ui_;
    WorldState world_{};
    WorldState renderWorld_{};
    uint8_t loadedLevelIndex_ = 255;
    bool lobbyLayout_ = true;

    std::string host_;
    sf::IpAddress serverAddress_;
    unsigned short localPort_ = 0;

    bool connected_ = false;
    bool localReady_ = false;
    uint8_t slot_ = 0;
    PlayerRole role_ = PlayerRole::None;

    InputFlags currentInput_ = InputFlags::None;
    uint32_t inputTick_ = 0;

    std::chrono::steady_clock::time_point lastInputSend_;
    std::chrono::steady_clock::time_point lastConnectRetry_;
};

} // namespace fireice
