#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

namespace fireice {

struct SpawnPoint {
    PlayerRole role = PlayerRole::None;
    float x = 0.0f;
    float y = 0.0f;
};

class GameMap {
public:
    bool loadFromFile(const std::string& path);
    bool loadFromText(const std::string& text);

    int width() const { return width_; }
    int height() const { return height_; }

    TileType tileAt(int x, int y) const;
    TileType tileAtWorld(float wx, float wy) const;
    void setTile(int x, int y, TileType type);

    bool isSolid(TileType type) const;
    bool blocksPlayer(TileType type, PlayerRole role, bool fireDoorOpen, bool waterDoorOpen,
                      bool poisonDoorOpen = false) const;
    bool isHazardFor(TileType type, PlayerRole role) const;
    bool isExitFor(TileType type, PlayerRole role) const;

    const std::vector<SpawnPoint>& spawns() const { return spawns_; }
    int countGems() const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<TileType> tiles_;
    std::vector<SpawnPoint> spawns_;

    TileType charToTile(char c) const;
    char tileToChar(TileType type) const;
};

} // namespace fireice
