#pragma once

#include "../core/service.h"

#include <atomic>
#include <cstdint>

namespace cerfcore {

class CalibrationPressCursor : public Service {
public:
    using Service::Service;

    void Show(int x, int y);
    void Hide();
    bool Point(int& x, int& y) const;

private:
    static uint64_t Pack(int x, int y);

    static constexpr uint64_t kHidden = ~0ull;
    std::atomic<uint64_t>     point_{kHidden};
};

}
