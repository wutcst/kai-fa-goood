#include "Map.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace fireice {

TileType GameMap::charToTile(char c) const {
    switch (c) {
    case '.': return TileType::Empty;
    case '#': return TileType::Solid;
    case 'L': return TileType::Lava;
    case 'W': return TileType::Water;
    case 'F': return TileType::FireDoor;
    case 'I': return TileType::WaterDoor;
    case 'E': return TileType::FireExit;
    case 'X': return TileType::WaterExit;
    case 'G': return TileType::Gem;
    case 'B': return TileType::Button;
    case 'f':
    case 'w':
        return TileType::Empty;
    default:
        return TileType::Solid;
    }
}

char GameMap::tileToChar(TileType type) const {
    switch (type) {
    case TileType::Empty: return '.';
    case TileType::Solid: return '#';
    case TileType::Lava: return 'L';
    case TileType::Water: return 'W';
    case TileType::FireDoor: return 'F';
    case TileType::WaterDoor: return 'I';
    case TileType::FireExit: return 'E';
    case TileType::WaterExit: return 'X';
    case TileType::Gem: return 'G';
    case TileType::Button: return 'B';
    default: return '#';
    }
}

bool GameMap::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return loadFromText(buffer.str());
}

bool GameMap::loadFromText(const std::string& text) {
    std::vector<std::string> rows;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            rows.push_back(line);
        }
    }

    if (rows.empty()) {
        return false;
    }

    height_ = static_cast<int>(rows.size());
    width_ = 0;
    for (const std::string& row : rows) {
        width_ = std::max(width_, static_cast<int>(row.size()));
    }
    tiles_.assign(static_cast<std::size_t>(width_ * height_), TileType::Empty);
    spawns_.clear();

    for (int y = 0; y < height_; ++y) {
        std::string row = rows[static_cast<std::size_t>(y)];
        if (static_cast<int>(row.size()) < width_) {
            row.append(static_cast<std::size_t>(width_ - static_cast<int>(row.size())), '.');
        }
        for (int x = 0; x < width_; ++x) {
            const char c = row[static_cast<std::size_t>(x)];
            tiles_[static_cast<std::size_t>(y * width_ + x)] = charToTile(c);

            if (c == 'f') {
                spawns_.push_back({PlayerRole::Fire,
                    x * TILE_SIZE + TILE_SIZE * 0.25f,
                    y * TILE_SIZE + TILE_SIZE * 0.25f});
            } else if (c == 'w') {
                spawns_.push_back({PlayerRole::Water,
                    x * TILE_SIZE + TILE_SIZE * 0.25f,
                    y * TILE_SIZE + TILE_SIZE * 0.25f});
            }
        }
    }

    return true;
}

TileType GameMap::tileAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return TileType::Solid;
    }
    return tiles_[static_cast<std::size_t>(y * width_ + x)];
}

TileType GameMap::tileAtWorld(float wx, float wy) const {
    const int tx = static_cast<int>(wx / TILE_SIZE);
    const int ty = static_cast<int>(wy / TILE_SIZE);
    return tileAt(tx, ty);
}

void GameMap::setTile(int x, int y, TileType type) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    tiles_[static_cast<std::size_t>(y * width_ + x)] = type;
}

bool GameMap::isSolid(TileType type) const {
    return type == TileType::Solid;
}

bool GameMap::blocksPlayer(TileType type, PlayerRole role, bool fireDoorOpen, bool waterDoorOpen) const {
    (void)role;
    if (type == TileType::Solid) {
        return true;
    }
    if (type == TileType::FireDoor) {
        return !fireDoorOpen;
    }
    if (type == TileType::WaterDoor) {
        return !waterDoorOpen;
    }
    return false;
}

bool GameMap::isHazardFor(TileType type, PlayerRole role) const {
    if (type == TileType::Lava) {
        return role == PlayerRole::Water;
    }
    if (type == TileType::Water) {
        return role == PlayerRole::Fire;
    }
    return false;
}

bool GameMap::isExitFor(TileType type, PlayerRole role) const {
    if (type == TileType::FireExit) {
        return role == PlayerRole::Fire;
    }
    if (type == TileType::WaterExit) {
        return role == PlayerRole::Water;
    }
    return false;
}

int GameMap::countGems() const {
    int count = 0;
    for (TileType tile : tiles_) {
        if (tile == TileType::Gem) {
            ++count;
        }
    }
    return count;
}

} // namespace fireice
