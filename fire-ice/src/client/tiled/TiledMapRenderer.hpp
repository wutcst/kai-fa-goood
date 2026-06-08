#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

namespace fireice {

class TiledMapRenderer {
public:
    bool load(const std::string& tmxPath);
    void bake();
    void drawStatic(sf::RenderWindow& window) const;

    bool ready() const { return isBaked_; }
    int mapWidth() const { return mapWidth_; }
    int mapHeight() const { return mapHeight_; }

private:
    struct TilesetRef {
        int firstGid = 0;
        int tileWidth = 32;
        int tileHeight = 32;
        int columns = 0;
        sf::Texture texture;
    };

    struct Layer {
        std::string name;
        std::vector<int> gids;
    };

    const TilesetRef* findTileset(int gid) const;
    void drawLayerToTarget(sf::RenderTexture& target, const Layer& layer) const;

    int mapWidth_ = 0;
    int mapHeight_ = 0;
    int tileWidth_ = 32;
    int tileHeight_ = 32;
    std::vector<TilesetRef> tilesets_;
    std::vector<Layer> visualLayers_;
    sf::RenderTexture bakedTexture_;
    sf::Sprite bakedSprite_;
    bool isBaked_ = false;
};

} // namespace fireice
