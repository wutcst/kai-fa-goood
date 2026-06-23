#include "LocalGameSession.hpp"

namespace fireice {

uint8_t LocalGameSession::slotForRole(PlayerRole role) {
    switch (role) {
        case PlayerRole::Water:
            return 1;
        case PlayerRole::Poison:
            return 2;
        default:
            return 0;
    }
}

bool LocalGameSession::start(PlayerRole role, const std::string& playerName) {
    room_ = Room{};
    room_.code = "LOCAL";
    room_.lastTick = std::chrono::steady_clock::now();
    room_.lastBroadcast = room_.lastTick;
    room_.selectLevel(0);

    playerSlot_ = slotForRole(role);
    ClientSlot& client = room_.clients[playerSlot_];
    client.connected = true;
    client.role = role;
    client.name = playerName;

    room_.syncConnectedCount();
    room_.world.lobbyStep = 1;
    std::snprintf(room_.world.roomCode, MAX_ROOM_CODE, "LOCAL");

    active_ = true;
    tickAccumulator_ = 0.0f;
    return true;
}

void LocalGameSession::stop() {
    active_ = false;
    room_ = Room{};
    playerSlot_ = 0;
    tickAccumulator_ = 0.0f;
}

void LocalGameSession::simulate(float dt) {
    if (!active_) {
        return;
    }

    tickAccumulator_ += dt;
    while (tickAccumulator_ >= TICK_DT) {
        room_.simulateTick();
        tickAccumulator_ -= TICK_DT;
    }
}

void LocalGameSession::setInput(uint8_t slot, InputFlags input) {
    if (!active_ || slot >= MAX_PLAYERS || !room_.clients[slot].connected) {
        return;
    }
    room_.clients[slot].pendingInput = input;
}

void LocalGameSession::handleAction(uint8_t slot, PlayerAction action, uint8_t value) {
    if (!active_) {
        return;
    }
    room_.handleAction(slot, action, value);
}

}  // namespace fireice
