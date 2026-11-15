#pragma once

#include <cerfcore/host/audio_format.h>

#include <atomic>
#include <cstdint>

namespace cerfcore {

class WaveInRequest {
public:
    bool Set(uint32_t sample_rate_hz, uint16_t channels, uint16_t bits);
    bool Pending() const;
    bool TakeChange(AudioFormat& format);

private:
    std::atomic<uint64_t> requested_{0};
    uint64_t              open_ = 0;
};

}
