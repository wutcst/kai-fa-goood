#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <string>

namespace fireice {

class AssetManager {
public:
    bool load();

    bool ready() const { return ready_; }

    const sf::Texture& fireBoy() const { return fireBoy_; }
    const sf::Texture& waterGirl() const { return waterGirl_; }
    const sf::Texture& gemRed() const { return gemRed_; }
    const sf::Texture& gemBlue() const { return gemBlue_; }
    const sf::Texture& lobbyBackground() const { return lobbyBackground_; }
    const sf::Texture& gameBackground() const { return gameBackground_; }
    const sf::Texture& winScreen() const { return winScreen_; }
    const sf::Texture& winScreenPartial() const { return winScreenPartial_; }
    const sf::Texture& loseScreen() const { return loseScreen_; }
    const sf::Texture& playButton() const { return playButton_; }
    const sf::Texture& retryButton() const { return retryButton_; }
    const sf::Texture& continueButton() const { return continueButton_; }
    const sf::Texture& menuButton() const { return menuButton_; }

    bool hasButtons() const { return buttonsReady_; }
    bool hasMapIcons() const { return mapIconsReady_; }

    const sf::Texture& mapLevelIcon(uint8_t levelIndex, bool unlocked, bool completed) const;
    const sf::Texture& mapBossIcon(bool unlocked, bool completed) const;

    std::string musicPath() const { return musicPath_; }

private:
    bool loadTexture(sf::Texture& texture, const std::string& fileName);
    bool loadMapIcons();

    sf::Texture fireBoy_;
    sf::Texture waterGirl_;
    sf::Texture gemRed_;
    sf::Texture gemBlue_;
    sf::Texture lobbyBackground_;
    sf::Texture gameBackground_;
    sf::Texture winScreen_;
    sf::Texture winScreenPartial_;
    sf::Texture loseScreen_;
    sf::Texture playButton_;
    sf::Texture retryButton_;
    sf::Texture continueButton_;
    sf::Texture menuButton_;
    std::array<sf::Texture, 7> mapLevelAvailable_{};
    std::array<sf::Texture, 7> mapLevelLocked_{};
    std::array<sf::Texture, 7> mapLevelCompleted_{};
    sf::Texture mapBossAvailable_;
    sf::Texture mapBossLocked_;
    sf::Texture mapBossCompleted_;
    std::string musicPath_;
    bool ready_ = false;
    bool buttonsReady_ = false;
    bool mapIconsReady_ = false;
};

}  // namespace fireice
