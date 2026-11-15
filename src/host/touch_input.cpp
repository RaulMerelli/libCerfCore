#include <cerfcore/host/touch_input.h>

#include "../core/deferred_jobs.h"

#include <cerfcore/core/cerf_emulator.h>

namespace cerfcore {

HostPointerInputLock& TouchInput::Lock() const { return emu_.Get<HostPointerInputLock>(); }

void TouchInput::Down(int x, int y) {
    pen_held_.store(true, std::memory_order_release);
    emu_.Get<DeferredJobs>().PostToGuestThread([this, x, y] { PenDown(x, y); });
}

void TouchInput::Move(int x, int y) {
    emu_.Get<DeferredJobs>().PostToGuestThread([this, x, y] { PenMove(x, y); });
}

void TouchInput::Up(int x, int y) {
    pen_held_.store(false, std::memory_order_release);
    emu_.Get<DeferredJobs>().PostToGuestThread([this, x, y] { PenUp(x, y); });
}

void TouchInput::Lost() {
    pen_held_.store(false, std::memory_order_release);
    emu_.Get<DeferredJobs>().PostToGuestThread([this] { CaptureLost(); });
}

void TouchInput::OnPenDown(int x, int y) {
    Lock().RunUnlessHeld([&] { Down(x, y); });
}

void TouchInput::OnPenMove(int x, int y) {
    Lock().RunUnlessHeld([&] { Move(x, y); });
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
    Lock().RunIfAdmitted(hold, [&] { Move(x, y); });
}

void TouchInput::OnPenUp(const HostPointerInputLock::Hold& hold, int x, int y) {
    Lock().RunIfAdmitted(hold, [&] { Up(x, y); });
}

void TouchInput::ReleaseHeld(const HostPointerInputLock::Hold& hold) {
    if (Lock().HeldBy(hold) && pen_held_.load(std::memory_order_acquire)) Lost();
}

}
