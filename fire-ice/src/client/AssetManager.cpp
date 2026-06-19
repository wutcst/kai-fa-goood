#include "AssetManager.hpp"
#include "Paths.hpp"

namespace fireice {

bool AssetManager::loadTexture(sf::Texture& texture, const std::string& fileName) {
    return texture.loadFromFile(resolveAssetPath("textures/" + fileName));
}

bool AssetManager::loadMapIcons() {
    for (std::size_t i = 0; i < mapLevelAvailable_.size(); ++i) {
        const std::string prefix = "map/level_" + std::to_string(i + 1) + "_";
        if (!loadTexture(mapLevelAvailable_[i], prefix + "available.png") ||
            !loadTexture(mapLevelLocked_[i], prefix + "locked.png") ||
            !loadTexture(mapLevelCompleted_[i], prefix + "completed.png")) {
            return false;
        }
    }
    return loadTexture(mapBossAvailable_, "map/boss_available.png") &&
           loadTexture(mapBossLocked_, "map/boss_locked.png") &&
           loadTexture(mapBossCompleted_, "map/boss_completed.png");
}

const sf::Texture& AssetManager::mapLevelIcon(uint8_t levelIndex, bool unlocked, bool completed) const {
    const std::size_t iconIndex = levelIndex % mapLevelAvailable_.size();
    if (!unlocked) {
        return mapLevelLocked_[iconIndex];
    }
    if (completed) {
        return mapLevelCompleted_[iconIndex];
    }
    return mapLevelAvailable_[iconIndex];
}

const sf::Texture& AssetManager::mapBossIcon(bool unlocked, bool completed) const {
    if (!unlocked) {
        return mapBossLocked_;
    }
    if (completed) {
        return mapBossCompleted_;
    }
    return mapBossAvailable_;
}

bool AssetManager::load() {
    ready_ = loadTexture(fireBoy_, "fireman.png") && loadTexture(waterGirl_, "watergirl.png") &&
             loadTexture(gemRed_, "red.png") && loadTexture(gemBlue_, "blue.png") &&
             loadTexture(lobbyBackground_, "start.png") && loadTexture(gameBackground_, "gamenew.jpg") &&
             loadTexture(winScreen_, "winend.png") && loadTexture(winScreenPartial_, "winend2.png") &&
             loadTexture(loseScreen_, "loseend.png");

    buttonsReady_ = loadTexture(playButton_, "play2.png") && loadTexture(retryButton_, "retry_button.png") &&
                    loadTexture(continueButton_, "continue_button.png") && loadTexture(menuButton_, "menu_button.png");

    pauseIconReady_ = loadTexture(pauseIcon_, "pause_icon.png");
    pauseMenuReady_ = loadTexture(pauseMenu_, "pause_menu.png");

    mapIconsReady_ = loadMapIcons();

    musicPath_ = resolveAssetPath("textures/LevelMusic.wav");
    return ready_;
}

}  // namespace fireice
