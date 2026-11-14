#include <cerfcore/host/touch_input.h>

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

HostPointerInputLock& TouchInput::Lock() const { return emu_.Get<HostPointerInputLock>(); }

void TouchInput::Down(int x, int y) {
    pen_held_.store(true, std::memory_order_release);
    PenDown(x, y);
}

void TouchInput::Up(int x, int y) {
    pen_held_.store(false, std::memory_order_release);
    PenUp(x, y);
}

void TouchInput::Lost() {
    pen_held_.store(false, std::memory_order_release);
    CaptureLost();
}

void TouchInput::OnPenDown(int x, int y) {
    Lock().RunUnlessHeld([&] { Down(x, y); });
}

void TouchInput::OnPenMove(int x, int y) {
    Lock().RunUnlessHeld([&] { PenMove(x, y); });
}

void TouchInput::OnPenUp(int x, int y) {
    Lock().RunUnlessHeld([&] { Up(x, y); });
}

void TouchInput::OnCaptureLost() {
    Lock().RunUnlessHeld([&] { Lost(); });
}

void TouchInput::OnPenDown(const HostPointerInputLock::Hold& hold, int x, int y) {
    Lock().RunIfAdmitted(hold, [&] { Down(x, y); });
}

void TouchInput::OnPenMove(const HostPointerInputLock::Hold& hold, int x, int y) {
    Lock().RunIfAdmitted(hold, [&] { PenMove(x, y); });
}

void TouchInput::OnPenUp(const HostPointerInputLock::Hold& hold, int x, int y) {
    Lock().RunIfAdmitted(hold, [&] { Up(x, y); });
}

void TouchInput::ReleaseHeld(const HostPointerInputLock::Hold& hold) {
    if (Lock().HeldBy(hold) && pen_held_.load(std::memory_order_acquire)) Lost();
}

}
