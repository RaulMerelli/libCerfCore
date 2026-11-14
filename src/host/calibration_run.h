#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/calibration_cross_scan.h>
#include <cerfcore/host/host_pointer_input_lock.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace cerfcore {

class CalibrationHost;
class CalibrationPressCursor;
class EmulationFreeze;
class StateReader;
class StateWriter;
class TouchInput;

enum class CalibrationOutcome : uint8_t { kDone, kFailed, kAborted };

class CalibrationRun : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    uint64_t           AbortGeneration() const;
    CalibrationOutcome Perform(uint64_t generation);
    void               Abort();

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    enum class PressResult : uint8_t { kPassed, kIgnored, kAborted };
    enum class SettleResult : uint8_t { kSettled, kNoFrame, kAborted };
    enum class PenStep : uint8_t { kDown, kMove, kUp };

    CalibrationOutcome FindSettledTarget(CalibrationFrame& frame, CalibrationCross& cross);
    CalibrationOutcome Steps(CalibrationFrame& frame, CalibrationCross& cross);
    PressResult        Press(const CalibrationFrame& reference, const CalibrationCross& cross);
    PressResult        WatchAfterRelease(const CalibrationFrame& reference, const CalibrationCross& cross);
    PressResult        BoxReacted(const CalibrationFrame& reference, const CalibrationCross& cross, CalibrationFrame& now);
    SettleResult       Settle(CalibrationFrame& settled);
    bool               FindNext(const CalibrationFrame& before, const CalibrationFrame& after,
                                const CalibrationCross& old, CalibrationCross& next);
    bool               Aborted() const;
    bool               Wait(std::chrono::milliseconds span);
    bool               Pen(PenStep step, int x, int y);
    void               LiftPen();

    TouchInput*             touch_  = nullptr;
    EmulationFreeze*        freeze_ = nullptr;
    HostPointerInputLock*   lock_   = nullptr;
    CalibrationHost*        host_   = nullptr;
    CalibrationPressCursor* cursor_ = nullptr;

    mutable std::mutex      mutex_;
    std::condition_variable wake_;
    uint64_t                abort_generation_ = 0;
    uint64_t                run_generation_   = 0;

    CalibrationCrossScan              scan_;
    const HostPointerInputLock::Hold* hold_     = nullptr;
    bool                              pen_down_ = false;
    int                               pen_x_    = 0;
    int                               pen_y_    = 0;
};

}
