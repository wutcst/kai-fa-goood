#include "TiledMapRenderer.hpp"

#include "ClientVisuals.hpp"
#include "Paths.hpp"
#include "Pickup.hpp"
#include "TmxUtil.hpp"
#include "Types.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace fireice {

namespace {

bool tryLoadTextureFromPaths(sf::Texture& texture, const std::vector<std::filesystem::path>& candidates) {
    for (const std::filesystem::path& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        const std::string path = candidate.lexically_normal().string();
        if (texture.loadFromFile(path)) {
            return true;
        }
    }
    return false;
}

}  // namespace

void TiledMapRenderer::parseTileAnimationsFromTsx(const std::string& xml, TilesetRef& tileset) {
    tileset.animations.clear();
    tileset.animationOwnerByFrame.clear();

    const std::string tileOpen = "<tile";
    std::size_t pos = 0;
    while (true) {
        pos = xml.find(tileOpen, pos);
        if (pos == std::string::npos) {
            break;
        }
        const std::size_t tileEnd = xml.find("</tile>", pos);
        if (tileEnd == std::string::npos) {
            break;
        }
        const std::string tileBlock = xml.substr(pos, tileEnd - pos + 7);
        pos = tileEnd + 7;

        const auto idAttr = tmx::attributeValue(tileBlock.substr(0, tileBlock.find('>') + 1), "id");
        if (!idAttr) {
            continue;
        }
        const int baseId = std::stoi(*idAttr);
        const std::size_t animPos = tileBlock.find("<animation>");
        if (animPos == std::string::npos) {
            continue;
        }
        const std::size_t animEnd = tileBlock.find("</animation>", animPos);
        if (animEnd == std::string::npos) {
            continue;
        }
        const std::string animBlock = tileBlock.substr(animPos, animEnd - animPos);

        TileAnimation anim;
        const std::string frameOpen = "<frame";
        std::size_t framePos = 0;
        while (true) {
            framePos = animBlock.find(frameOpen, framePos);
            if (framePos == std::string::npos) {
                break;
            }
            const std::size_t frameTagEnd = animBlock.find("/>", framePos);
            if (frameTagEnd == std::string::npos) {
                break;
            }
            const std::string frameTag = animBlock.substr(framePos, frameTagEnd - framePos + 2);
            framePos = frameTagEnd + 2;

            const auto frameId = tmx::attributeValue(frameTag, "tileid");
            const auto duration = tmx::attributeValue(frameTag, "duration");
            if (!frameId || !duration) {
                continue;
            }
            anim.frames.push_back(std::stoi(*frameId));
            anim.durationsMs.push_back(std::stoi(*duration));
            anim.totalMs += std::stoi(*duration);
        }
        if (anim.frames.empty()) {
            continue;
        }

        tileset.animations[baseId] = std::move(anim);
        for (int frameId : tileset.animations[baseId].frames) {
            tileset.animationOwnerByFrame[frameId] = baseId;
        }
    }
}

void TiledMapRenderer::parseTileAnimationsFromTsj(const std::string& json, TilesetRef& tileset) {
    tileset.animations.clear();
    tileset.animationOwnerByFrame.clear();

    const std::string tilesKey = "\"tiles\":";
    std::size_t tilesPos = json.find(tilesKey);
    if (tilesPos == std::string::npos) {
        return;
    }
    const std::size_t arrayStart = json.find('[', tilesPos);
    if (arrayStart == std::string::npos) {
        return;
    }

    std::size_t pos = arrayStart + 1;
    int depth = 1;
    while (pos < json.size() && depth > 0) {
        const std::size_t objStart = json.find('{', pos);
        if (objStart == std::string::npos || depth == 0) {
            break;
        }
        std::size_t objEnd = objStart + 1;
        int objDepth = 1;
        while (objEnd < json.size() && objDepth > 0) {
            if (json[objEnd] == '{') {
                ++objDepth;
            } else if (json[objEnd] == '}') {
                --objDepth;
            }
            ++objEnd;
        }
        const std::string tileObj = json.substr(objStart, objEnd - objStart);

        const auto baseId = tmx::jsonIntField(tileObj, "id");
        const std::size_t animPos = tileObj.find("\"animation\":");
        if (baseId && animPos != std::string::npos) {
            const std::size_t animArrayStart = tileObj.find('[', animPos);
            const std::size_t animArrayEnd = tileObj.find(']', animArrayStart);
            if (animArrayStart != std::string::npos && animArrayEnd != std::string::npos) {
                const std::string animArray = tileObj.substr(animArrayStart, animArrayEnd - animArrayStart + 1);
                TileAnimation anim;
                std::size_t framePos = 0;
                while (true) {
                    framePos = animArray.find('{', framePos);
                    if (framePos == std::string::npos) {
                        break;
                    }
                    const std::size_t frameEnd = animArray.find('}', framePos);
                    if (frameEnd == std::string::npos) {
                        break;
                    }
                    const std::string frameObj = animArray.substr(framePos, frameEnd - framePos + 1);
                    framePos = frameEnd + 1;

                    const auto frameId = tmx::jsonIntField(frameObj, "tileid");
                    const auto duration = tmx::jsonIntField(frameObj, "duration");
                    if (!frameId || !duration) {
                        continue;
                    }
                    anim.frames.push_back(*frameId);
                    anim.durationsMs.push_back(*duration);
                    anim.totalMs += *duration;
                }
                if (!anim.frames.empty()) {
                    tileset.animations[*baseId] = std::move(anim);
                    for (int frameId : tileset.animations[*baseId].frames) {
                        tileset.animationOwnerByFrame[frameId] = *baseId;
                    }
                }
            }
        }

        pos = objEnd;
        if (json[objEnd] == ']') {
            break;
        }
    }
}

int TiledMapRenderer::resolveAnimatedLocalId(const TilesetRef& tileset, int localId, float animTimeSec) {
    int ownerId = localId;
    if (const auto it = tileset.animationOwnerByFrame.find(localId); it != tileset.animationOwnerByFrame.end()) {
        ownerId = it->second;
    } else if (tileset.animations.find(localId) == tileset.animations.end()) {
        return localId;
    }

    const auto animIt = tileset.animations.find(ownerId);
    if (animIt == tileset.animations.end() || animIt->second.totalMs <= 0) {
        return localId;
    }

    const TileAnimation& anim = animIt->second;
    int elapsedMs = static_cast<int>(animTimeSec * 1000.0f) % anim.totalMs;
    for (std::size_t i = 0; i < anim.frames.size(); ++i) {
        elapsedMs -= anim.durationsMs[i];
        if (elapsedMs < 0) {
            return anim.frames[i];
        }
    }
    return anim.frames.back();
}

bool TiledMapRenderer::loadTilesetFromTsxFile(const std::string& tsxPath, int firstGid, TilesetRef& out) {
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
    out.texture.setSmooth(false);
    if (out.columns <= 0) {
        const unsigned texWidth = out.texture.getSize().x;
        out.columns = static_cast<int>(texWidth / static_cast<unsigned>(out.tileWidth));
    }
    parseTileAnimationsFromTsx(xml, out);
    return true;
}

bool TiledMapRenderer::loadTilesetFromTsjFile(const std::string& tsjPath, int firstGid, TilesetRef& out) {
    const std::string json = tmx::readFile(tsjPath);
    if (json.empty()) {
        return false;
    }

    out.firstGid = firstGid;
    if (const auto tw = tmx::jsonIntField(json, "tilewidth")) {
        out.tileWidth = *tw;
    }
    if (const auto th = tmx::jsonIntField(json, "tileheight")) {
        out.tileHeight = *th;
    }
    if (const auto columns = tmx::jsonIntField(json, "columns")) {
        out.columns = *columns;
    }

    const auto image = tmx::jsonStringField(json, "image");
    if (!image) {
        return false;
    }

    const std::string imagePath = resolveAssetPath(tmx::joinPath(tmx::parentDirectory(tsjPath), *image));
    if (!out.texture.loadFromFile(imagePath)) {
        std::cerr << "[TiledMapRenderer] Failed to load tileset image: " << imagePath << std::endl;
        return false;
    }
    out.texture.setSmooth(false);
    if (out.columns <= 0) {
        const unsigned texWidth = out.texture.getSize().x;
        out.columns = static_cast<int>(texWidth / static_cast<unsigned>(out.tileWidth));
    }
    parseTileAnimationsFromTsj(json, out);
    return true;
}

bool TiledMapRenderer::loadTilesetFromFile(const std::string& path, int firstGid, TilesetRef& out) {
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".tsj") {
        return loadTilesetFromTsjFile(path, firstGid, out);
    }
    return loadTilesetFromTsxFile(path, firstGid, out);
}

bool TiledMapRenderer::isSpawnObjectName(const std::string& name) {
    std::string lower = name;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower == "player1" || lower == "player2" || lower == "player" || lower == "fire_spawn" ||
           lower == "water_spawn" || lower == "poison_spawn";
}

int TiledMapRenderer::decodeGid(int rawGid) {
    return rawGid & 0x1FFFFFFF;
}

bool TiledMapRenderer::load(const std::string& tmxPath) {
    isLoaded_ = false;
    isBaked_ = false;
    hasCachedGrass_ = false;
    tilesets_.clear();
    visualLayers_.clear();
    imageLayers_.clear();
    objectTiles_.clear();
    collectibleObjectTiles_.clear();
    tilesetInfo_.clear();
    mapDirectory_.clear();
    mapScale_ = 1.0f;

    const std::filesystem::path tmxFile(resolveAssetPath(tmxPath));
    const std::string resolvedTmx = tmxFile.generic_string();
    mapDirectory_ = tmxFile.parent_path().generic_string();
    const std::string mapDir = mapDirectory_;
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

    if (tileWidth_ > 0 && tileWidth_ != static_cast<int>(TILE_SIZE)) {
        // 例如 TMX tilewidth=16，游戏逻辑 TILE_SIZE=32
        mapScale_ = TILE_SIZE / static_cast<float>(tileWidth_);
    }

    tilesetInfo_ = loadTmxTilesetInfo(tmxPath);

    for (const std::string& tilesetTag : tmx::findTags(xml, "tileset")) {
        const auto source = tmx::attributeValue(tilesetTag, "source");
        if (!source) {
            continue;
        }
        const auto firstGid = tmx::attributeValue(tilesetTag, "firstgid");
        if (!firstGid) {
            continue;
        }

        TilesetRef tileset;
        const std::string tilesetPath = resolveAssetPath(tmx::joinPath(mapDir, *source));
        if (!loadTilesetFromFile(tilesetPath, std::stoi(*firstGid), tileset)) {
            continue;
        }
        tilesets_.push_back(std::move(tileset));
    }

    std::sort(tilesets_.begin(), tilesets_.end(),
              [](const TilesetRef& a, const TilesetRef& b) { return a.firstGid < b.firstGid; });

    for (const tmx::ImageLayerData& imageLayer : tmx::findAllImageLayers(xml)) {
        ImageLayer loaded;
        if (!loadImageLayerTexture(mapDir, imageLayer, loaded)) {
            std::cerr << "[TiledMapRenderer] Failed to load image layer: " << imageLayer.imageSource << std::endl;
            continue;
        }
        imageLayers_.push_back(std::move(loaded));
    }
    if (!imageLayers_.empty()) {
        std::cout << "[TiledMapRenderer] Loaded " << imageLayers_.size() << " background layer(s) for " << resolvedTmx
                  << std::endl;
    }

    for (const tmx::TileLayerData& tileLayer : tmx::findAllTileLayers(xml)) {
        if (static_cast<int>(tileLayer.gids.size()) != mapWidth_ * mapHeight_) {
            std::cerr << "[TiledMapRenderer] Layer size mismatch: " << tileLayer.name << std::endl;
            continue;
        }
        Layer layer;
        layer.name = tileLayer.name;
        layer.gids = tileLayer.gids;
        visualLayers_.push_back(std::move(layer));
    }

    uint8_t collectibleIndex = 0;
    for (const tmx::ObjectTileData& obj : tmx::findObjectTiles(xml)) {
        // 出生点 object 由服务端 collision 决定，视觉层跳过
        if (isSpawnObjectName(obj.name)) {
            continue;
        }
        if (obj.gid > 0) {
            const int decodedGid = decodeGid(obj.gid);
            if (isCollectibleGid(decodedGid, tilesetInfo_)) {
                if (collectibleIndex >= MAX_PICKUPS) {
                    continue;
                }
                CollectibleObjectTile tile;
                tile.gid = obj.gid;
                tile.x = obj.x;
                tile.y = obj.y;
                tile.width = obj.width;
                tile.height = obj.height;
                tile.index = collectibleIndex++;
                collectibleObjectTiles_.push_back(std::move(tile));
                continue;
            }
            if (isSawGid(decodedGid, tilesetInfo_) || isRockHeadGid(decodedGid, tilesetInfo_) ||
                isSpikedBallGid(decodedGid, tilesetInfo_) || isChainGid(decodedGid, tilesetInfo_)) {
                continue;
            }
        }

        ObjectTile tile;
        tile.gid = obj.gid;
        tile.x = obj.x;
        tile.y = obj.y;
        tile.width = obj.width;
        tile.height = obj.height;
        objectTiles_.push_back(std::move(tile));
    }

    if (visualLayers_.empty()) {
        std::cerr << "[TiledMapRenderer] No tile layers found in map: " << resolvedTmx << std::endl;
    } else if (tilesets_.empty()) {
        std::cerr << "[TiledMapRenderer] No tilesets loaded for map: " << resolvedTmx << std::endl;
    }

    tmx::ImageLayerData grassSource;
    grassSource.imageSource = "shared/grass_background.png";
    grassSource.repeatX = true;
    grassSource.repeatY = true;
    if (imageLayers_.empty()) {
        hasCachedGrass_ = loadImageLayerTexture(mapDirectory_, grassSource, cachedGrass_);
    } else {
        hasCachedGrass_ = false;
    }

    isLoaded_ = !tilesets_.empty() && !visualLayers_.empty();
    return isLoaded_;
}

bool TiledMapRenderer::loadImageLayerTexture(const std::string& mapDir, const tmx::ImageLayerData& source,
                                             ImageLayer& out) const {
    std::vector<std::filesystem::path> candidates;
    if (!mapDirectory_.empty()) {
        candidates.emplace_back(std::filesystem::path(mapDirectory_) / source.imageSource);
    }
    if (!mapDir.empty()) {
        candidates.emplace_back(resolveAssetPath(tmx::joinPath(mapDir, source.imageSource)));
    }
    candidates.emplace_back(resolveAssetPath(std::string("maps/") + source.imageSource));

    if (!tryLoadTextureFromPaths(out.texture, candidates)) {
        for (const std::filesystem::path& candidate : candidates) {
            if (!candidate.empty()) {
                std::cerr << "[TiledMapRenderer] Failed to load image layer texture: "
                          << candidate.lexically_normal().string() << std::endl;
            }
        }
        return false;
    }

    out.texture.setSmooth(false);
    const sf::Vector2u texSize = out.texture.getSize();
    out.imageWidth = source.imageWidth > 0 ? source.imageWidth : static_cast<int>(texSize.x);
    out.imageHeight = source.imageHeight > 0 ? source.imageHeight : static_cast<int>(texSize.y);
    out.offsetX = source.offsetX;
    out.offsetY = source.offsetY;
    out.repeatX = source.repeatX;
    out.repeatY = source.repeatY;
    return true;
}

void TiledMapRenderer::drawImageLayerRepeating(sf::RenderWindow& window, const ImageLayer& layer, float mapPixelW,
                                               float mapPixelH) const {
    const sf::Vector2u texSize = layer.texture.getSize();
    if (texSize.x == 0 || texSize.y == 0) {
        return;
    }

    const float stampW = static_cast<float>(layer.imageWidth) * mapScale_;
    const float stampH = static_cast<float>(layer.imageHeight) * mapScale_;
    const bool fullRepeat = layer.repeatX && layer.repeatY;
    const float originX = fullRepeat ? 0.0f : layer.offsetX * mapScale_;
    const float originY = fullRepeat ? 0.0f : layer.offsetY * mapScale_;
    const float endX = layer.repeatX ? mapPixelW - std::max(0.0f, originX) : stampW;
    const float endY = layer.repeatY ? mapPixelH - std::max(0.0f, originY) : stampH;

    for (float y = 0.0f; y < endY; y += stampH) {
        for (float x = 0.0f; x < endX; x += stampW) {
            sf::Sprite sprite(layer.texture);
            sprite.setScale(stampW / static_cast<float>(texSize.x), stampH / static_cast<float>(texSize.y));
            sprite.setPosition(originX + x, originY + y);
            window.draw(sprite);
        }
    }
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

void TiledMapRenderer::drawGidSprite(sf::RenderTexture& target, int rawGid, float x, float y, float width,
                                     float height) const {
    const int gid = decodeGid(rawGid);
    const TilesetRef* tileset = findTileset(gid);
    if (tileset == nullptr) {
        return;
    }

    const int localId = gid - tileset->firstGid;
    if (localId < 0) {
        return;
    }

    const int columns = std::max(1, tileset->columns);
    const int rows =
        static_cast<int>(tileset->texture.getSize().y / static_cast<unsigned>(std::max(1, tileset->tileHeight)));
    const int maxTiles = rows * columns;
    if (localId >= maxTiles) {
        return;
    }
    const int sx = (localId % columns) * tileset->tileWidth;
    const int sy = (localId / columns) * tileset->tileHeight;

    const float drawWidth = width > 0.0f ? width : static_cast<float>(tileset->tileWidth);
    const float drawHeight = height > 0.0f ? height : static_cast<float>(tileset->tileHeight);
    const float drawX = x;
    const float drawY = y - drawHeight;

    sf::Sprite sprite(tileset->texture);
    sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
    sprite.setPosition(drawX, drawY);
    if (drawWidth != static_cast<float>(tileset->tileWidth) || drawHeight != static_cast<float>(tileset->tileHeight)) {
        sprite.setScale(drawWidth / static_cast<float>(tileset->tileWidth),
                        drawHeight / static_cast<float>(tileset->tileHeight));
    }
    target.draw(sprite);
}

void TiledMapRenderer::drawGidSpriteToWindow(sf::RenderWindow& window, int rawGid, float x, float y, float width,
                                             float height, float animTimeSec) const {
    const int gid = decodeGid(rawGid);
    const TilesetRef* tileset = findTileset(gid);
    if (tileset == nullptr) {
        return;
    }

    int localId = gid - tileset->firstGid;
    if (localId < 0) {
        return;
    }
    localId = resolveAnimatedLocalId(*tileset, localId, animTimeSec);

    const int columns = std::max(1, tileset->columns);
    const int rows =
        static_cast<int>(tileset->texture.getSize().y / static_cast<unsigned>(std::max(1, tileset->tileHeight)));
    const int maxTiles = rows * columns;
    if (localId >= maxTiles) {
        return;
    }
    const int sx = (localId % columns) * tileset->tileWidth;
    const int sy = (localId / columns) * tileset->tileHeight;

    const float baseDrawWidth = (width > 0.0f ? width : static_cast<float>(tileset->tileWidth)) * mapScale_;
    const float baseDrawHeight = (height > 0.0f ? height : static_cast<float>(tileset->tileHeight)) * mapScale_;
    float drawWidth = baseDrawWidth;
    float drawHeight = baseDrawHeight;
    float drawX = x * mapScale_;
    float drawY = y * mapScale_ - drawHeight;

    constexpr float kThinPlatformMinHeight = TILE_SIZE * 0.5f;
    if (tileset->tileWidth > tileset->tileHeight && tileset->tileHeight <= 8 && drawHeight < kThinPlatformMinHeight) {
        const float yBoost = kThinPlatformMinHeight / drawHeight;
        scaleVisualFromBottom(drawX, drawY, drawWidth, drawHeight, 1.0f, yBoost);
    }

    sf::Sprite sprite(tileset->texture);
    sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
    sprite.setPosition(drawX, drawY);
    sprite.setScale(drawWidth / static_cast<float>(tileset->tileWidth),
                    drawHeight / static_cast<float>(tileset->tileHeight));
    window.draw(sprite);
}

void TiledMapRenderer::drawObjectGidAt(sf::RenderWindow& window, int rawGid, float gameX, float gameY, float gameW,
                                       float gameH, float animTimeSec) const {
    const int gid = decodeGid(rawGid);
    const TilesetRef* tileset = findTileset(gid);
    if (tileset == nullptr) {
        return;
    }

    int localId = gid - tileset->firstGid;
    if (localId < 0) {
        return;
    }
    localId = resolveAnimatedLocalId(*tileset, localId, animTimeSec);

    const int columns = std::max(1, tileset->columns);
    const int rows =
        static_cast<int>(tileset->texture.getSize().y / static_cast<unsigned>(std::max(1, tileset->tileHeight)));
    const int maxTiles = rows * columns;
    if (localId >= maxTiles) {
        return;
    }
    const int sx = (localId % columns) * tileset->tileWidth;
    const int sy = (localId / columns) * tileset->tileHeight;

    const float drawWidth = gameW > 0.0f ? gameW : static_cast<float>(tileset->tileWidth) * mapScale_;
    const float drawHeight = gameH > 0.0f ? gameH : static_cast<float>(tileset->tileHeight) * mapScale_;

    sf::Sprite sprite(tileset->texture);
    sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
    sprite.setPosition(gameX, gameY);
    sprite.setScale(drawWidth / static_cast<float>(tileset->tileWidth),
                    drawHeight / static_cast<float>(tileset->tileHeight));
    window.draw(sprite);
}

void TiledMapRenderer::drawAnimatedObjectGidAt(sf::RenderWindow& window, int rawGid, float gameX, float gameY,
                                               float gameW, float gameH, float animTimeSec) const {
    const int gid = decodeGid(rawGid);
    const TilesetRef* tileset = findTileset(gid);
    if (tileset == nullptr) {
        return;
    }

    int localId = gid - tileset->firstGid;
    if (localId < 0) {
        return;
    }
    localId = resolveAnimatedLocalId(*tileset, localId, animTimeSec);

    const int columns = std::max(1, tileset->columns);
    const int rows =
        static_cast<int>(tileset->texture.getSize().y / static_cast<unsigned>(std::max(1, tileset->tileHeight)));
    const int maxTiles = rows * columns;
    if (localId >= maxTiles) {
        return;
    }
    const int sx = (localId % columns) * tileset->tileWidth;
    const int sy = (localId / columns) * tileset->tileHeight;

    const float drawWidth = gameW > 0.0f ? gameW : static_cast<float>(tileset->tileWidth) * mapScale_;
    const float drawHeight = gameH > 0.0f ? gameH : static_cast<float>(tileset->tileHeight) * mapScale_;

    sf::Sprite sprite(tileset->texture);
    sprite.setTextureRect(sf::IntRect(sx, sy, tileset->tileWidth, tileset->tileHeight));
    sprite.setOrigin(static_cast<float>(tileset->tileWidth) * 0.5f, static_cast<float>(tileset->tileHeight) * 0.5f);
    sprite.setPosition(gameX + drawWidth * 0.5f, gameY + drawHeight * 0.5f);
    sprite.setScale(drawWidth / static_cast<float>(tileset->tileWidth),
                    drawHeight / static_cast<float>(tileset->tileHeight));
    window.draw(sprite);
}

void TiledMapRenderer::drawLayerToTarget(sf::RenderTexture& target, const Layer& layer,
                                         const std::function<bool(int, int)>& excludeTile) const {
    for (int y = 0; y < mapHeight_; ++y) {
        for (int x = 0; x < mapWidth_; ++x) {
            if (excludeTile && excludeTile(x, y)) {
                continue;
            }
            const int gid = layer.gids[static_cast<std::size_t>(y * mapWidth_ + x)];
            if (gid <= 0) {
                continue;
            }
            drawGidSprite(target, gid, static_cast<float>(x * tileWidth_), static_cast<float>((y + 1) * tileHeight_),
                          static_cast<float>(tileWidth_), static_cast<float>(tileHeight_));
        }
    }
}

// 平铺绘制预缓存的草地背景，供 bake 与实时渲染共用
void TiledMapRenderer::drawCachedGrassToTarget(sf::RenderTexture& target, float mapPixelW, float mapPixelH) const {
    if (!hasCachedGrass_) {
        return;
    }

    const sf::Vector2u texSize = cachedGrass_.texture.getSize();
    if (texSize.x == 0 || texSize.y == 0) {
        return;
    }

    const float stampW = static_cast<float>(cachedGrass_.imageWidth) * 1.0f;
    const float stampH = static_cast<float>(cachedGrass_.imageHeight) * 1.0f;
    for (float y = 0.0f; y < mapPixelH; y += stampH) {
        for (float x = 0.0f; x < mapPixelW; x += stampW) {
            sf::Sprite sprite(cachedGrass_.texture);
            sprite.setScale(stampW / static_cast<float>(texSize.x), stampH / static_cast<float>(texSize.y));
            sprite.setPosition(x, y);
            target.draw(sprite);
        }
    }
}

void TiledMapRenderer::drawImageLayersToTarget(sf::RenderTexture& target, float scale) const {
    const float mapPixelW = static_cast<float>(mapWidth_ * tileWidth_) * scale;
    const float mapPixelH = static_cast<float>(mapHeight_ * tileHeight_) * scale;

    for (const ImageLayer& layer : imageLayers_) {
        const sf::Vector2u texSize = layer.texture.getSize();
        if (texSize.x == 0 || texSize.y == 0) {
            continue;
        }

        const float stampW =
            static_cast<float>(layer.imageWidth > 0 ? layer.imageWidth : static_cast<int>(texSize.x)) * scale;
        const float stampH =
            static_cast<float>(layer.imageHeight > 0 ? layer.imageHeight : static_cast<int>(texSize.y)) * scale;
        const float endX = layer.repeatX ? mapPixelW : stampW;
        const float endY = layer.repeatY ? mapPixelH : stampH;

        for (float y = 0.0f; y < endY; y += stampH) {
            for (float x = 0.0f; x < endX; x += stampW) {
                sf::Sprite sprite(layer.texture);
                sprite.setScale(stampW / static_cast<float>(texSize.x), stampH / static_cast<float>(texSize.y));
                sprite.setPosition(x, y);
                target.draw(sprite);
            }
        }
    }
}

void TiledMapRenderer::drawImageLayersToWindow(sf::RenderWindow& window) const {
    const float mapPixelW = static_cast<float>(mapWidth_ * tileWidth_) * mapScale_;
    const float mapPixelH = static_cast<float>(mapHeight_ * tileHeight_) * mapScale_;

    for (const ImageLayer& layer : imageLayers_) {
        drawImageLayerRepeating(window, layer, mapPixelW, mapPixelH);
    }
}

void TiledMapRenderer::drawObjectTilesToTarget(sf::RenderTexture& target) const {
    for (const ObjectTile& obj : objectTiles_) {
        drawGidSprite(target, obj.gid, obj.x, obj.y, obj.width, obj.height);
    }
}

void TiledMapRenderer::drawTileLayersToWindow(sf::RenderWindow& window,
                                              const std::function<bool(int, int)>& skipTile) const {
    for (const Layer& layer : visualLayers_) {
        for (int y = 0; y < mapHeight_; ++y) {
            for (int x = 0; x < mapWidth_; ++x) {
                if (skipTile && skipTile(x, y)) {
                    continue;
                }
                const int gid = layer.gids[static_cast<std::size_t>(y * mapWidth_ + x)];
                if (gid <= 0) {
                    continue;
                }
                drawGidSpriteToWindow(window, gid, static_cast<float>(x * tileWidth_),
                                      static_cast<float>((y + 1) * tileHeight_), static_cast<float>(tileWidth_),
                                      static_cast<float>(tileHeight_));
            }
        }
    }
}

void TiledMapRenderer::drawObjectTilesToWindow(sf::RenderWindow& window) const {
    for (const ObjectTile& obj : objectTiles_) {
        drawGidSpriteToWindow(window, obj.gid, obj.x, obj.y, obj.width, obj.height);
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

    bakedTexture_.clear(imageLayers_.empty() ? sf::Color(88, 140, 72) : sf::Color(24, 32, 28));
    const float mapPixelW = static_cast<float>(mapWidth_ * tileWidth_);
    const float mapPixelH = static_cast<float>(mapHeight_ * tileHeight_);
    if (imageLayers_.empty()) {
        drawCachedGrassToTarget(bakedTexture_, mapPixelW, mapPixelH);
    }
    drawImageLayersToTarget(bakedTexture_, 1.0f);
    for (const Layer& layer : visualLayers_) {
        drawLayerToTarget(bakedTexture_, layer);
    }
    drawObjectTilesToTarget(bakedTexture_);
    bakedTexture_.display();

    bakedSprite_.setTexture(bakedTexture_.getTexture());
    bakedSprite_.setPosition(0.0f, 0.0f);
    bakedSprite_.setScale(mapScale_, mapScale_);
    isBaked_ = true;
}

void TiledMapRenderer::drawGrassUnderlay(sf::RenderWindow& window, float mapPixelW, float mapPixelH) const {
    if (hasCachedGrass_) {
        drawImageLayerRepeating(window, cachedGrass_, mapPixelW, mapPixelH);
        return;
    }
    sf::RectangleShape bg({mapPixelW, mapPixelH});
    bg.setPosition(0.0f, 0.0f);
    bg.setFillColor(sf::Color(88, 140, 72));
    window.draw(bg);
}

void TiledMapRenderer::drawStatic(sf::RenderWindow& window, const std::function<bool(int, int)>& skipTile) const {
    if (!isLoaded_) {
        return;
    }

    const float mapPixelW = static_cast<float>(mapWidth_) * TILE_SIZE;
    const float mapPixelH = static_cast<float>(mapHeight_) * TILE_SIZE;
    if (imageLayers_.empty()) {
        drawGrassUnderlay(window, mapPixelW, mapPixelH);
    } else {
        drawImageLayersToWindow(window);
    }
    drawTileLayersToWindow(window, skipTile);
    drawObjectTilesToWindow(window);
}

void TiledMapRenderer::drawCollectibles(sf::RenderWindow& window, float animTimeSec, uint32_t collectedMask,
                                        uint32_t collectedMaskHi, uint32_t collectedMaskExt,
                                        const std::function<bool(uint8_t pickupIndex)>& skipPickup) const {
    for (const CollectibleObjectTile& obj : collectibleObjectTiles_) {
        const uint32_t bit = 1u << (obj.index % 32u);
        const uint8_t word = obj.index / 32u;
        const bool taken = word == 0 ? ((collectedMask & bit) != 0)
                                     : (word == 1 ? ((collectedMaskHi & bit) != 0) : ((collectedMaskExt & bit) != 0));
        if (taken) {
            continue;
        }
        if (skipPickup && skipPickup(obj.index)) {
            continue;
        }
        drawGidSpriteToWindow(window, obj.gid, obj.x, obj.y, obj.width, obj.height, animTimeSec);
    }
}

void TiledMapRenderer::drawPreview(sf::RenderWindow& window, const sf::FloatRect& area) const {
    // 等比缩放并居中，用于选关右侧面板
    sf::RectangleShape backdrop({area.width, area.height});
    backdrop.setPosition(area.left, area.top);
    backdrop.setFillColor(sf::Color(24, 32, 28, 230));
    backdrop.setOutlineThickness(2.0f);
    backdrop.setOutlineColor(sf::Color(80, 70, 50));
    window.draw(backdrop);

    if (!isBaked_) {
        return;
    }

    const sf::Vector2u texSize = bakedTexture_.getSize();
    if (texSize.x == 0 || texSize.y == 0) {
        return;
    }

    const float mapW = static_cast<float>(texSize.x);
    const float mapH = static_cast<float>(texSize.y);
    const float innerPad = 8.0f;
    const float scale = std::min((area.width - innerPad * 2.0f) / mapW, (area.height - innerPad * 2.0f) / mapH);
    const float drawW = mapW * scale;
    const float drawH = mapH * scale;

    sf::Sprite sprite(bakedTexture_.getTexture());
    sprite.setScale(scale, scale);
    sprite.setPosition(area.left + (area.width - drawW) / 2.0f, area.top + (area.height - drawH) / 2.0f);
    window.draw(sprite);
}

}  // namespace fireice
