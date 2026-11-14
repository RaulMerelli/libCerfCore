#include <cerfcore/host/relative_mouse_input.h>

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

void RelativeMouseInput::OnRelativeMove(int dx, int dy, uint32_t button_mask) {
    emu_.Get<HostPointerInputLock>().RunUnlessHeld([&] {
        buttons_.store(button_mask, std::memory_order_release);
        RelativeMove(dx, dy, button_mask);
    });
}

void RelativeMouseInput::ReleaseHeld(const HostPointerInputLock::Hold& hold) {
    if (!emu_.Get<HostPointerInputLock>().HeldBy(hold) || buttons_.load(std::memory_order_acquire) == 0) return;
    buttons_.store(0, std::memory_order_release);
    RelativeMove(0, 0, 0);
}

}
