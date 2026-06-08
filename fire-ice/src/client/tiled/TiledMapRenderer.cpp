#include "TiledMapRenderer.hpp"

#include "Paths.hpp"
#include "TmxUtil.hpp"

#include <algorithm>
#include <iostream>

namespace fireice {

namespace {

struct TilesetLoadData {
    int firstGid = 0;
    int tileWidth = 32;
    int tileHeight = 32;
    int columns = 0;
    sf::Texture texture;
};

bool loadTilesetFromTsx(const std::string& tsxPath, int firstGid, TilesetLoadData& out) {
    const std::string xml = tmx::readFile(tsxPath);
    if (xml.empty()) {
        return false;
    }

    const auto tags = tmx::findTags(xml, "tileset");
    if (tags.empty()) {
        return false;
    }
    const std::string& header = tags.front();
    out.firstGid = firstGid;
    if (const auto tw = tmx::attributeValue(header, "tilewidth")) {
        out.tileWidth = std::stoi(*tw);
    }
    if (const auto th = tmx::attributeValue(header, "tileheight")) {
        out.tileHeight = std::stoi(*th);
    }
    if (const auto columns = tmx::attributeValue(header, "columns")) {
        out.columns = std::stoi(*columns);
    }

    const auto imageTags = tmx::findTags(xml, "image");
    if (imageTags.empty()) {
        return false;
    }
    const auto source = tmx::attributeValue(imageTags.front(), "source");
    if (!source) {
        return false;
    }

    const std::string imagePath = resolveAssetPath(tmx::joinPath(tmx::parentDirectory(tsxPath), *source));
    if (!out.texture.loadFromFile(imagePath)) {
        std::cerr << "[TiledMapRenderer] Failed to load tileset image: " << imagePath << std::endl;
        return false;
    }
    if (out.columns <= 0) {
        const unsigned texWidth = out.texture.getSize().x;
        out.columns = static_cast<int>(texWidth / static_cast<unsigned>(out.tileWidth));
    }
    return true;
}

} // namespace

bool TiledMapRenderer::load(const std::string& tmxPath) {
    isBaked_ = false;
    tilesets_.clear();
    visualLayers_.clear();

    const std::string resolvedTmx = resolveAssetPath(tmxPath);
    const std::string xml = tmx::readFile(resolvedTmx);
    if (xml.empty()) {
        std::cerr << "[TiledMapRenderer] Failed to read map: " << resolvedTmx << std::endl;
        return false;
    }

    const auto mapTags = tmx::findTags(xml, "map");
    if (mapTags.empty()) {
        return false;
    }
    const std::string& mapTag = mapTags.front();
    if (const auto w = tmx::attributeValue(mapTag, "width")) {
        mapWidth_ = std::stoi(*w);
    }
    if (const auto h = tmx::attributeValue(mapTag, "height")) {
        mapHeight_ = std::stoi(*h);
    }
    if (const auto tw = tmx::attributeValue(mapTag, "tilewidth")) {
        tileWidth_ = std::stoi(*tw);
    }
    if (const auto th = tmx::attributeValue(mapTag, "tileheight")) {
        tileHeight_ = std::stoi(*th);
    }

    const std::string mapDir = tmx::parentDirectory(resolvedTmx);
    for (const std::string& tilesetTag : tmx::findTags(xml, "tileset")) {
        const auto source = tmx::attributeValue(tilesetTag, "source");
        if (!source) {
            continue;
        }
        const auto firstGid = tmx::attributeValue(tilesetTag, "firstgid");
        if (!firstGid) {
            continue;
        }

        TilesetLoadData loaded;
        const std::string tsxPath = resolveAssetPath(tmx::joinPath(mapDir, *source));
        if (!loadTilesetFromTsx(tsxPath, std::stoi(*firstGid), loaded)) {
            continue;
        }
        TilesetRef tileset;
        tileset.firstGid = loaded.firstGid;
        tileset.tileWidth = loaded.tileWidth;
        tileset.tileHeight = loaded.tileHeight;
        tileset.columns = loaded.columns;
        tileset.texture = std::move(loaded.texture);
        tilesets_.push_back(std::move(tileset));
    }

    std::sort(tilesets_.begin(), tilesets_.end(),
        [](const TilesetRef& a, const TilesetRef& b) { return a.firstGid < b.firstGid; });

    static const char* kVisualLayers[] = {"Background", "Walls", "Decor"};
    for (const char* layerName : kVisualLayers) {
        const auto csv = tmx::findLayerData(xml, layerName);
        if (!csv) {
            continue;
        }
        Layer layer;
        layer.name = layerName;
        layer.gids = tmx::parseCsvInts(*csv);
        if (static_cast<int>(layer.gids.size()) != mapWidth_ * mapHeight_) {
            std::cerr << "[TiledMapRenderer] Layer size mismatch: " << layerName << std::endl;
            continue;
        }
        visualLayers_.push_back(std::move(layer));
    }

    return !tilesets_.empty() && !visualLayers_.empty();
}

const TiledMapRenderer::TilesetRef* TiledMapRenderer::findTileset(int gid) const {
    if (gid <= 0) {
        return nullptr;
    }
    const TilesetRef* chosen = nullptr;
    for (const TilesetRef& tileset : tilesets_) {
        if (gid >= tileset.firstGid) {
            chosen = &tileset;
        } else {
            break;
        }
    }
    return chosen;
}

void TiledMapRenderer::drawLayerToTarget(sf::RenderTexture& target, const Layer& layer) const {
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            const int gid = layer.gids[static_cast<std::size_t>(y * mapWidth_ + x)];
            if (gid <= 0) {
                continue;
            }
            const TilesetRef* tileset = findTileset(gid);
            if (tileset == nullptr) {
                continue;
            }
            const int localId = gid - tileset->firstGid;
            if (localId < 0) {
                continue;
            }
            const int columns = std::max(1, tileset->columns);
            const int sx = (localId % columns) * tileset->tileWidth;
            const int sy = (localId / columns) * tileset->tileHeight;

            sf::Sprite sprite(tileset->texture);
            sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
            sprite.setPosition(static_cast<float>(x * tileWidth_), static_cast<float>(y * tileHeight_));
            target.draw(sprite);
        }
    }
}

void TiledMapRenderer::bake() {
    isBaked_ = false;
    if (mapWidth_ <= 0 || mapHeight_ <= 0 || visualLayers_.empty()) {
        return;
    }

    const unsigned width = static_cast<unsigned>(mapWidth_ * tileWidth_);
    const unsigned height = static_cast<unsigned>(mapHeight_ * tileHeight_);
    if (!bakedTexture_.create(width, height)) {
        std::cerr << "[TiledMapRenderer] Failed to create render texture" << std::endl;
        return;
    }

    bakedTexture_.clear(sf::Color(20, 30, 20));
    for (const Layer& layer : visualLayers_) {
        drawLayerToTarget(bakedTexture_, layer);
    }
    bakedTexture_.display();

    bakedSprite_.setTexture(bakedTexture_.getTexture());
    bakedSprite_.setPosition(0.0f, 0.0f);
    isBaked_ = true;
}

void TiledMapRenderer::drawStatic(sf::RenderWindow& window) const {
    if (!isBaked_) {
        return;
    }
    window.draw(bakedSprite_);
}

} // namespace fireice
