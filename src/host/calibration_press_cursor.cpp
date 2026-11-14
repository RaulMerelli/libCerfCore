#include <cerfcore/host/calibration_press_cursor.h>

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

REGISTER_SERVICE(CalibrationPressCursor);

uint64_t CalibrationPressCursor::Pack(int x, int y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
}

void CalibrationPressCursor::Show(int x, int y) { point_.store(Pack(x, y), std::memory_order_release); }

void CalibrationPressCursor::Hide() { point_.store(kHidden, std::memory_order_release); }

bool CalibrationPressCursor::Point(int& x, int& y) const {
    const uint64_t packed = point_.load(std::memory_order_acquire);
    if (packed == kHidden) return false;
    x = static_cast<int>(static_cast<uint32_t>(packed >> 32));
    y = static_cast<int>(static_cast<uint32_t>(packed));
    return true;
}

}
