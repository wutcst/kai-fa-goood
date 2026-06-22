#pragma once

#include "GameServer.hpp"
#include "Types.hpp"

#include <string>

namespace fireice {

class LocalGameSession {
public:
    bool start(PlayerRole role, const std::string& playerName);
    void stop();
    bool active() const { return active_; }

    void simulate(float dt);
    void setInput(uint8_t slot, InputFlags input);
    void handleAction(uint8_t slot, PlayerAction action, uint8_t value = 0);

    const WorldState& world() const { return room_.world; }
    uint8_t playerSlot() const { return playerSlot_; }

private:
    static uint8_t slotForRole(PlayerRole role);

    Room room_{};
    bool active_ = false;
    uint8_t playerSlot_ = 0;
    float tickAccumulator_ = 0.0f;
};

}  // namespace fireice
