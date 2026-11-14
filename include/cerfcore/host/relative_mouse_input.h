#pragma once

#include "../core/service.h"
#include "host_pointer_input_lock.h"
#include "pointer_source.h"

#include <atomic>
#include <cstdint>

namespace cerfcore {

constexpr uint32_t kRelMouseLeft  = 0x1u;
constexpr uint32_t kRelMouseRight = 0x2u;

class RelativeMouseInput : public Service, public PointerSource {
public:
    using Service::Service;
    ~RelativeMouseInput() override = default;

    void OnRelativeMove(int dx, int dy, uint32_t button_mask);
    void ReleaseHeld(const HostPointerInputLock::Hold& hold);

    std::string    SourceName()       const override { return "Stock mouse"; }
    const char*    IconResourceName() const override { return "cursor"; }
    int            SourcePriority()   const override { return 0; }
    PointerKind    Kind()             const override { return PointerKind::Relative; }

protected:
    virtual void RelativeMove(int dx, int dy, uint32_t button_mask) = 0;

private:
    std::atomic<uint32_t> buttons_{0};
};

}
