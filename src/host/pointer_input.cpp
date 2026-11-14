#include <cerfcore/host/pointer_input.h>

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

HostPointerInputLock& PointerInput::Lock() const { return emu_.Get<HostPointerInputLock>(); }

void PointerInput::Release() {
    buttons_.store(0, std::memory_order_release);
    CaptureLost();
}

void PointerInput::OnMove(int sx, int sy, uint32_t button_mask) {
    Lock().RunUnlessHeld([&] {
        buttons_.store(button_mask, std::memory_order_release);
        Move(sx, sy, button_mask);
    });
}

void PointerInput::OnWheel(int sx, int sy, int delta) {
    Lock().RunUnlessHeld([&] { Wheel(sx, sy, delta); });
}

void PointerInput::OnCaptureLost() {
    Lock().RunUnlessHeld([&] { Release(); });
}

void PointerInput::ReleaseHeld(const HostPointerInputLock::Hold& hold) {
    if (Lock().HeldBy(hold) && buttons_.load(std::memory_order_acquire) != 0) Release();
}

}
