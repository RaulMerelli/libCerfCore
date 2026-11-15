#pragma once

#include "../core/service.h"
#include "host_pointer_input_lock.h"
#include "pointer_source.h"

#include <atomic>
#include <cstdint>

namespace cerfcore {

constexpr uint32_t kPointerLeft   = 0x1u;
constexpr uint32_t kPointerRight  = 0x2u;
constexpr uint32_t kPointerMiddle = 0x4u;

class PointerInput : public Service, public PointerSource {
public:
    using Service::Service;
    ~PointerInput() override = default;

    void OnMove (int sx, int sy, uint32_t button_mask);
    void OnWheel(int sx, int sy, int delta);
    void OnCaptureLost();
    void ReleaseHeld(const HostPointerInputLock::Hold& hold);

    static constexpr char kSourceName[] = "Guest Additions mouse";
    std::string    SourceId()         const override { return "guest_additions_pointer"; }
    std::string    SourceName()       const override { return kSourceName; }
    int            SourcePriority()   const override { return 100; }
    PointerKind    Kind()             const override { return PointerKind::Absolute; }

protected:
    virtual void Move (int sx, int sy, uint32_t button_mask) = 0;
    virtual void Wheel(int sx, int sy, int delta)            = 0;
    virtual void CaptureLost()                               = 0;

private:
    HostPointerInputLock& Lock() const;
    void                  Release();

    std::atomic<uint32_t> buttons_{0};
};

}
