#include "calibration_run.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/calibration_host.h>
#include <cerfcore/host/calibration_press_cursor.h>
#include <cerfcore/host/touch_input.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/emulation_freeze.h>
#include <cerfcore/state/hibernation.h>
#include <cerfcore/state/state_stream.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace cerfcore {

namespace {

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::milliseconds;

constexpr Ms     kPoll{100};
constexpr Ms     kHold{500};
constexpr Ms     kPressLimit{3000};
constexpr Ms     kTurn{600};
constexpr double kCircleRadius = 2.0;
constexpr Ms     kReleaseWatch{3000};
constexpr Ms     kReleasePoll{250};
constexpr Ms     kSettleQuiet{800};
constexpr Ms     kSettleLimit{5000};
constexpr Ms     kSettlePoll{250};
constexpr double kTwoPi = 6.283185307179586;

Ms Since(Clock::time_point start) { return std::chrono::duration_cast<Ms>(Clock::now() - start); }

int Clamp(int v, uint32_t extent) { return std::clamp(v, 0, static_cast<int>(extent) - 1); }

}

REGISTER_SERVICE(CalibrationRun);

bool CalibrationRun::ShouldRegister() { return emu_.TryGet<TouchInput>() != nullptr; }

void CalibrationRun::OnReady() {
    touch_  = &emu_.Get<TouchInput>();
    freeze_ = &emu_.Get<EmulationFreeze>();
    lock_   = &emu_.Get<HostPointerInputLock>();
    host_   = &emu_.Get<CalibrationHost>();
    cursor_ = &emu_.Get<CalibrationPressCursor>();
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        if (!emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) Abort();
    });
    emu_.Get<Hibernation>().RegisterPostRestoreListener([this] { Abort(); });
}

uint64_t CalibrationRun::AbortGeneration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return abort_generation_;
}

void CalibrationRun::Abort() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++abort_generation_;
    wake_.notify_all();
}

bool CalibrationRun::Aborted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return abort_generation_ != run_generation_;
}

bool CalibrationRun::Wait(Ms span) {
    std::unique_lock<std::mutex> lock(mutex_);
    return !wake_.wait_for(lock, span, [this] { return abort_generation_ != run_generation_; });
}

CalibrationOutcome CalibrationRun::Perform(uint64_t generation) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        run_generation_ = generation;
        if (abort_generation_ != generation) return CalibrationOutcome::kAborted;
    }
    const HostPointerInputLock::Hold hold = [this] {
        auto freeze = freeze_->WorkerSection();
        return lock_->Acquire();
    }();
    hold_                                 = &hold;
    CalibrationFrame   frame;
    CalibrationCross   cross;
    CalibrationOutcome outcome = FindSettledTarget(frame, cross);
    if (outcome == CalibrationOutcome::kDone) outcome = Steps(frame, cross);
    LiftPen();
    hold_ = nullptr;
    return outcome;
}

CalibrationOutcome CalibrationRun::FindSettledTarget(CalibrationFrame& frame, CalibrationCross& cross) {
    CalibrationFrame settled;
    switch (Settle(settled)) {
        case SettleResult::kAborted: return CalibrationOutcome::kAborted;
        case SettleResult::kNoFrame: return CalibrationOutcome::kFailed;
        case SettleResult::kSettled: break;
    }
    const CrossScanStatus status = scan_.Find(settled, cross);
    if (status != CrossScanStatus::kFound) {
        LOG(Cerf, "no target on the settled screen: %s\n", CalibrationCrossScan::StatusName(status));
        return CalibrationOutcome::kFailed;
    }
    frame = std::move(settled);
    return CalibrationOutcome::kDone;
}

CalibrationOutcome CalibrationRun::Steps(CalibrationFrame& frame, CalibrationCross& cross) {
    int remaining = CalibrationCrossScan::RemainingAfter(cross.zone);
    for (;;) {
        LOG(Cerf, "pressing the %s target at (%d,%d), guest turn %d\n",
            CalibrationCrossScan::ZoneName(cross.zone), cross.press_x, cross.press_y, cross.quarter);
        switch (Press(frame, cross)) {
            case PressResult::kAborted: return CalibrationOutcome::kAborted;
            case PressResult::kIgnored:
                LOG(Cerf, "the %s target at (%d,%d) did not change after the press\n",
                    CalibrationCrossScan::ZoneName(cross.zone), cross.press_x, cross.press_y);
                return CalibrationOutcome::kFailed;
            case PressResult::kPassed: break;
        }
        if (remaining == 0) {
            LOG(Cerf, "the last target reacted, calibration complete\n");
            return CalibrationOutcome::kDone;
        }
        CalibrationFrame settled;
        switch (Settle(settled)) {
            case SettleResult::kAborted: return CalibrationOutcome::kAborted;
            case SettleResult::kNoFrame: return CalibrationOutcome::kFailed;
            case SettleResult::kSettled: break;
        }
        CalibrationCross next;
        if (!FindNext(frame, settled, cross, next)) return CalibrationOutcome::kFailed;
        frame = std::move(settled);
        cross = next;
        --remaining;
    }
}

CalibrationRun::PressResult CalibrationRun::Press(const CalibrationFrame& reference, const CalibrationCross& cross) {
    if (!Pen(PenStep::kDown, cross.press_x, cross.press_y)) return PressResult::kAborted;
    const Clock::time_point start = Clock::now();
    CalibrationFrame        now;
    for (;;) {
        if (!Wait(kPoll)) return PressResult::kAborted;
        switch (BoxReacted(reference, cross, now)) {
            case PressResult::kAborted: return PressResult::kAborted;
            case PressResult::kPassed:  LiftPen(); return PressResult::kPassed;
            case PressResult::kIgnored: break;
        }
        const Ms held = Since(start);
        if (held >= kPressLimit) break;
        if (held < kHold) continue;
        const double phase  = static_cast<double>((held - kHold).count());
        const double radius = kCircleRadius * phase / static_cast<double>((kPressLimit - kHold).count());
        const double angle  = kTwoPi * phase / static_cast<double>(kTurn.count());
        const int    x = Clamp(cross.press_x + static_cast<int>(std::lround(radius * std::cos(angle))), reference.width);
        const int    y = Clamp(cross.press_y + static_cast<int>(std::lround(radius * std::sin(angle))), reference.height);
        if (!Pen(PenStep::kMove, x, y)) return PressResult::kAborted;
    }
    LiftPen();
    return WatchAfterRelease(reference, cross);
}

CalibrationRun::PressResult CalibrationRun::WatchAfterRelease(const CalibrationFrame& reference,
                                                              const CalibrationCross& cross) {
    const Clock::time_point start = Clock::now();
    CalibrationFrame        now;
    while (Since(start) < kReleaseWatch) {
        if (!Wait(kReleasePoll)) return PressResult::kAborted;
        const PressResult reacted = BoxReacted(reference, cross, now);
        if (reacted != PressResult::kIgnored) return reacted;
    }
    return PressResult::kIgnored;
}

CalibrationRun::PressResult CalibrationRun::BoxReacted(const CalibrationFrame& reference, const CalibrationCross& cross,
                                                       CalibrationFrame& now) {
    switch (host_->CaptureSurface(now)) {
        case FrameGrab::kClosed:  return PressResult::kAborted;
        case FrameGrab::kNoFrame: return PressResult::kIgnored;
        case FrameGrab::kCaptured:
            return CalibrationCrossScan::BoxChanged(reference, now, cross) ? PressResult::kPassed
                                                                            : PressResult::kIgnored;
    }
    return PressResult::kIgnored;
}

CalibrationRun::SettleResult CalibrationRun::Settle(CalibrationFrame& settled) {
    const Clock::time_point start = Clock::now();
    Clock::time_point       quiet = start;
    FrameGrab               grab  = host_->CaptureSurface(settled);
    if (grab == FrameGrab::kClosed) return SettleResult::kAborted;
    bool             have = grab == FrameGrab::kCaptured;
    CalibrationFrame now;
    for (;;) {
        if (!Wait(kSettlePoll)) return SettleResult::kAborted;
        grab = host_->CaptureSurface(now);
        if (grab == FrameGrab::kClosed) return SettleResult::kAborted;
        if (grab == FrameGrab::kCaptured && (!have || now != settled)) {
            std::swap(settled, now);
            have  = true;
            quiet = Clock::now();
        }
        if (have && Since(quiet) >= kSettleQuiet) return SettleResult::kSettled;
        if (Since(start) >= kSettleLimit) {
            const long long limit_ms = static_cast<long long>(kSettleLimit.count());
            if (!have) {
                LOG(Cerf, "no guest frame within %lld ms\n", limit_ms);
                return SettleResult::kNoFrame;
            }
            LOG(Cerf, "the screen did not settle within %lld ms\n", limit_ms);
            return SettleResult::kSettled;
        }
    }
}

bool CalibrationRun::FindNext(const CalibrationFrame& before, const CalibrationFrame& after,
                              const CalibrationCross& old, CalibrationCross& next) {
    const CalibrationZone want = CalibrationCrossScan::NextZone(old.zone);
    const auto fits = [&](CrossScanStatus status) {
        return status == CrossScanStatus::kFound && next.zone == want && next.quarter == old.quarter;
    };
    const CrossScanStatus changed = scan_.FindChanged(before, after, old, next);
    if (fits(changed)) return true;
    const CrossScanStatus fresh = scan_.Find(after, next);
    if (fits(fresh)) return true;
    LOG(Cerf, "no %s target after the %s target: change scan %s, frame scan %s %s turn %d\n",
        CalibrationCrossScan::ZoneName(want), CalibrationCrossScan::ZoneName(old.zone),
        CalibrationCrossScan::StatusName(changed), CalibrationCrossScan::StatusName(fresh),
        CalibrationCrossScan::ZoneName(next.zone), next.quarter);
    return false;
}

bool CalibrationRun::Pen(PenStep step, int x, int y) {
    auto freeze  = freeze_->WorkerSection();
    bool applied = false;
    if (step == PenStep::kUp) {
        if (pen_down_) touch_->OnPenUp(*hold_, pen_x_, pen_y_);
        pen_down_ = false;
        applied   = true;
    } else if (!Aborted()) {
        if (step == PenStep::kDown) touch_->OnPenDown(*hold_, x, y);
        else                        touch_->OnPenMove(*hold_, x, y);
        pen_down_ = true;
        pen_x_    = x;
        pen_y_    = y;
        applied   = true;
    }
    if (pen_down_) cursor_->Show(pen_x_, pen_y_);
    else           cursor_->Hide();
    return applied;
}

void CalibrationRun::LiftPen() { Pen(PenStep::kUp, 0, 0); }

void CalibrationRun::SaveState(StateWriter& w) const {
    w.Write<uint8_t>("calibration_pen_down", pen_down_ ? 1u : 0u);
    w.Write<int32_t>("calibration_pen_x", pen_x_);
    w.Write<int32_t>("calibration_pen_y", pen_y_);
}

void CalibrationRun::RestoreState(StateReader& r) {
    uint8_t down = 0;
    int32_t x = 0, y = 0;
    r.Read("calibration_pen_down", down);
    r.Read("calibration_pen_x", x);
    r.Read("calibration_pen_y", y);
    pen_down_ = false;
    cursor_->Hide();
    if (down != 0) touch_->OnPenUp(x, y);
}

}
