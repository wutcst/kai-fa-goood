#pragma once

#include "CharacterAnimator.hpp"

#include <SFML/Graphics.hpp>
#include <array>
#include <string>
#include <vector>

namespace fireice {

// 标题界面单层视差背景及其横向滚动速度
struct TitleParallaxLayer {
    sf::Texture texture;
    float scrollSpeed = 0.f;
};

class AssetManager {
public:
    bool load();

    bool ready() const { return ready_; }

    const CharacterAnimator& character(PlayerRole role) const;
    const sf::Texture& gemRed() const { return gemRed_; }
    const sf::Texture& gemBlue() const { return gemBlue_; }
    bool hasTitleParallax() const { return titleParallaxReady_; }
    const std::vector<TitleParallaxLayer>& titleParallaxLayers() const { return titleParallax_; }
    const sf::Texture& gameBackground() const { return gameBackground_; }
    const sf::Texture& winScreen() const { return winScreen_; }
    const sf::Texture& winScreenPartial() const { return winScreenPartial_; }
    const sf::Texture& loseScreen() const { return loseScreen_; }
    const sf::Texture& pauseMenu() const { return pauseMenu_; }
    const sf::Texture& playButton() const { return playButton_; }
    const sf::Texture& retryButton() const { return retryButton_; }
    const sf::Texture& continueButton() const { return continueButton_; }
    const sf::Texture& menuButton() const { return menuButton_; }
    const sf::Texture& pauseIcon() const { return pauseIcon_; }
    const sf::Texture& victoryMenu() const { return victoryMenu_; }
    const sf::Texture& victoryButtonMenu() const { return victoryButtonMenu_; }
    const sf::Texture& victoryButtonRetry() const { return victoryButtonRetry_; }
    const sf::Texture& victoryButtonNext() const { return victoryButtonNext_; }

    bool hasButtons() const { return buttonsReady_; }
    bool hasPauseIcon() const { return pauseIconReady_; }
    bool hasPauseMenu() const { return pauseMenuReady_; }
    bool hasVictoryUi() const { return victoryUiReady_; }
    bool hasMapIcons() const { return mapIconsReady_; }

    const sf::Texture& mapLevelIcon(uint8_t levelIndex, bool unlocked, bool completed) const;
    const sf::Texture& mapBossIcon(bool unlocked, bool completed) const;

    std::string musicPath() const { return musicPath_; }

private:
    bool loadTexture(sf::Texture& texture, const std::string& fileName);
    bool loadPixelTexture(sf::Texture& texture, const std::string& fileName);
    bool loadMapIcons();
    bool loadTitleParallax();

    CharacterAnimator playerFire_;
    CharacterAnimator playerWater_;
    CharacterAnimator playerPoison_;
    sf::Texture gemRed_;
    sf::Texture gemBlue_;
    std::vector<TitleParallaxLayer> titleParallax_;
    sf::Texture gameBackground_;
    sf::Texture winScreen_;
    sf::Texture winScreenPartial_;
    sf::Texture loseScreen_;
    sf::Texture pauseMenu_;
    sf::Texture playButton_;
    sf::Texture retryButton_;
    sf::Texture continueButton_;
    sf::Texture menuButton_;
    sf::Texture pauseIcon_;
    sf::Texture victoryMenu_;
    sf::Texture victoryButtonMenu_;
    sf::Texture victoryButtonRetry_;
    sf::Texture victoryButtonNext_;
    std::array<sf::Texture, 7> mapLevelAvailable_{};
    std::array<sf::Texture, 7> mapLevelLocked_{};
    std::array<sf::Texture, 7> mapLevelCompleted_{};
    sf::Texture mapBossAvailable_;
    sf::Texture mapBossLocked_;
    sf::Texture mapBossCompleted_;
    std::string musicPath_;
    bool ready_ = false;
    bool titleParallaxReady_ = false;
    bool buttonsReady_ = false;
    bool mapIconsReady_ = false;
    bool pauseIconReady_ = false;
    bool pauseMenuReady_ = false;
    bool victoryUiReady_ = false;
};

}  // namespace fireice
