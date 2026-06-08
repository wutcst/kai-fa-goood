#pragma once

#include <cstdint>

namespace fireice {

constexpr float TILE_SIZE = 32.0f;
constexpr float GRAVITY = 1800.0f;
constexpr float MOVE_SPEED = 220.0f;
constexpr float JUMP_SPEED = 520.0f;
constexpr float MAX_FALL_SPEED = 900.0f;

constexpr uint16_t SERVER_PORT = 24567;
constexpr float TICK_RATE = 60.0f;
constexpr float TICK_DT = 1.0f / TICK_RATE;
constexpr float STATE_BROADCAST_HZ = 20.0f;

enum class PlayerRole : uint8_t { None = 0, Fire = 1, Water = 2, Poison = 3 };

enum class TileType : uint8_t {
    Empty = 0,
    Solid,
    Lava,
    Water,
    FireDoor,
    WaterDoor,
    FireExit,
    WaterExit,
    Gem,
    Button,
    Acid,
    PoisonDoor,
    PoisonExit
};

enum class GamePhase : uint8_t { Lobby = 0, Countdown = 1, Playing = 2, Victory = 3, GameOver = 4 };

enum class InputFlags : uint8_t { None = 0, Left = 1 << 0, Right = 1 << 1, Jump = 1 << 2, Down = 1 << 3 };

enum class PlayerAction : uint8_t {
    None = 0,
    Ready = 1,
    Restart = 2,
    SelectLevel = 3,
    NextLevel = 4,
    PrevLevel = 5,
    ReturnToLobby = 6
};

constexpr unsigned LOBBY_WINDOW_WIDTH = 1024;
constexpr unsigned LOBBY_WINDOW_HEIGHT = 640;

constexpr uint8_t MAX_LEVEL_NAME = 32;
constexpr uint8_t MAX_PLAYER_NAME = 16;
constexpr uint8_t MAX_PLAYERS = 3;
constexpr uint8_t MAX_ROOM_CODE = 8;

inline InputFlags operator|(InputFlags a, InputFlags b) {
    return static_cast<InputFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline InputFlags operator&(InputFlags a, InputFlags b) {
    return static_cast<InputFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool hasFlag(InputFlags value, InputFlags flag) {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }

    Vec2& operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
};

struct AABB {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float left() const { return x; }
    float right() const { return x + w; }
    float top() const { return y; }
    float bottom() const { return y + h; }

    bool intersects(const AABB& other) const {
        return left() < other.right() && right() > other.left() && top() < other.bottom() && bottom() > other.top();
    }
};

struct PlayerState {
    PlayerRole role = PlayerRole::None;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    bool onGround = false;
    bool alive = true;
    uint8_t gems = 0;
    bool atExit = false;
};

struct WorldState {
    uint32_t tick = 0;
    GamePhase phase = GamePhase::Lobby;
    uint8_t connectedCount = 0;
    uint8_t readyMask = 0;
    uint8_t countdown = 0;
    uint8_t levelIndex = 0;
    uint8_t levelCount = 0;
    uint8_t totalGems = 0;
    char levelName[MAX_LEVEL_NAME]{};
    char playerNames[MAX_PLAYERS][MAX_PLAYER_NAME]{};
    PlayerState players[MAX_PLAYERS]{};
    char roomCode[MAX_ROOM_CODE]{};
    bool fireDoorOpen = false;
    bool waterDoorOpen = false;
    bool poisonDoorOpen = false;
    bool levelComplete = false;
};

inline const char* phaseName(GamePhase phase) {
    switch (phase) {
        case GamePhase::Lobby:
            return "Lobby";
        case GamePhase::Countdown:
            return "Countdown";
        case GamePhase::Playing:
            return "Playing";
        case GamePhase::Victory:
            return "Victory";
        case GamePhase::GameOver:
            return "GameOver";
        default:
            return "Unknown";
    }
}

}  // namespace fireice
