#pragma once

#include <cstdint>

namespace cerfcore {

enum class HostButton : uint8_t { kNone, kLeft, kRight, kMiddle };

inline constexpr uint32_t kHeldLeft   = 1u << 0;
inline constexpr uint32_t kHeldRight  = 1u << 1;
inline constexpr uint32_t kHeldMiddle = 1u << 2;

uint32_t HostButtonBit(HostButton button);

struct HostPointerEvent {
    enum class Kind : uint8_t { kDown, kUp, kMove, kWheel, kCaptureLost };

    Kind       kind   = Kind::kMove;
    HostButton button = HostButton::kNone;
    int        x      = 0;
    int        y      = 0;
    bool       inside = false;
    uint32_t   held   = 0;
    int        wheel  = 0;
};

}
