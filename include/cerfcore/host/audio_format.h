#pragma once

#include <cstdint>

namespace cerfcore {

struct AudioFormat {
    uint32_t rate_hz  = 0;
    uint16_t channels = 0;
    uint16_t bits     = 0;

    uint32_t FrameBytes() const { return static_cast<uint32_t>(channels) * (bits / 8u); }
};

}
