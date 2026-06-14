#pragma once

#include <cstdint>

namespace fireice {

constexpr uint8_t kInitialUnlockedMask = INITIAL_UNLOCKED_LEVEL_MASK;

inline bool isLevelUnlocked(uint8_t unlockedMask, uint8_t index) {
    return (unlockedMask & static_cast<uint8_t>(1u << index)) != 0;
}

inline bool isLevelCompleted(uint8_t completedMask, uint8_t index) {
    return (completedMask & static_cast<uint8_t>(1u << index)) != 0;
}

inline uint8_t findPrevUnlockedLevel(uint8_t unlockedMask, uint8_t current) {
    if (current == 0) {
        return 0;
    }
    for (int i = static_cast<int>(current) - 1; i >= 0; --i) {
        if (isLevelUnlocked(unlockedMask, static_cast<uint8_t>(i))) {
            return static_cast<uint8_t>(i);
        }
    }
    return current;
}

inline uint8_t findNextUnlockedLevel(uint8_t unlockedMask, uint8_t current, uint8_t levelCount) {
    for (uint8_t i = static_cast<uint8_t>(current + 1); i < levelCount; ++i) {
        if (isLevelUnlocked(unlockedMask, i)) {
            return i;
        }
    }
    return current;
}

}  // namespace fireice
