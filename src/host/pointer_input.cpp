#include <cerfcore/host/pointer_input.h>

#include "../core/deferred_jobs.h"

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

HostPointerInputLock& PointerInput::Lock() const { return emu_.Get<HostPointerInputLock>(); }

void PointerInput::Release() {
    buttons_.store(0, std::memory_order_release);
    emu_.Get<DeferredJobs>().PostToGuestThread([this] { CaptureLost(); });
}

void PointerInput::OnMove(int sx, int sy, uint32_t button_mask) {
    Lock().RunUnlessHeld([&] {
        buttons_.store(button_mask, std::memory_order_release);
        emu_.Get<DeferredJobs>().PostToGuestThread([this, sx, sy, button_mask] { Move(sx, sy, button_mask); });
    });
}

void PointerInput::OnWheel(int sx, int sy, int delta) {
    Lock().RunUnlessHeld(
        [&] { emu_.Get<DeferredJobs>().PostToGuestThread([this, sx, sy, delta] { Wheel(sx, sy, delta); }); });
}

void PointerInput::OnCaptureLost() {
    Lock().RunUnlessHeld([&] { Release(); });
}

void PointerInput::ReleaseHeld(const HostPointerInputLock::Hold& hold) {
    if (Lock().HeldBy(hold) && buttons_.load(std::memory_order_acquire) != 0) Release();
}

}
