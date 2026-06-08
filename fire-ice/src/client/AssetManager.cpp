#include "AssetManager.hpp"
#include "Paths.hpp"

namespace fireice {

bool AssetManager::loadTexture(sf::Texture& texture, const std::string& fileName) {
    return texture.loadFromFile(resolveAssetPath("textures/" + fileName));
}

bool AssetManager::load() {
    ready_ = loadTexture(fireBoy_, "fireman.png")
        && loadTexture(waterGirl_, "watergirl.png")
        && loadTexture(gemRed_, "red.png")
        && loadTexture(gemBlue_, "blue.png")
        && loadTexture(lobbyBackground_, "start.png")
        && loadTexture(gameBackground_, "gamenew.jpg")
        && loadTexture(winScreen_, "winend.png")
        && loadTexture(winScreenPartial_, "winend2.png")
        && loadTexture(loseScreen_, "loseend.png");

    buttonsReady_ = loadTexture(playButton_, "play2.png")
        && loadTexture(retryButton_, "retry_button.png")
        && loadTexture(continueButton_, "continue_button.png")
        && loadTexture(menuButton_, "menu_button.png");

    musicPath_ = resolveAssetPath("textures/LevelMusic.wav");
    return ready_;
}

} // namespace fireice
