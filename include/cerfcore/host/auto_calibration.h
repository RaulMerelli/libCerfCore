#pragma once

#include "../core/board_database.h"
#include "../core/service.h"
#include "calibration_cross_scan.h"
#include "calibration_host.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

namespace cerfcore {

struct DeviceConfig;
class CalibrationRun;
class HostPresentation;
class InstanceThreads;
class NotificationStack;
class UserAlert;
class StateReader;
class StateWriter;

enum class CalibrationTrigger : uint8_t { kBootWindow, kMenu, kGuestAdditions };

class AutoCalibration : public Service {
public:
    using Service::Service;

    void OnReady() override;
    void OnShutdown() override;

    bool CanPress() const;
    void Request(CalibrationTrigger trigger);
    void OnGuestAdditionsCalibration(bool shown);
    bool GuestAdditionsCalibrationShown() const;
    void SwitchToStockPointer();

    void SaveGuestAdditionsState(StateWriter& w) const;
    void RestoreGuestAdditionsState(StateReader& r);

private:
    using Clock = std::chrono::steady_clock;

    void BeginWindow();
    void StopWindow();
    void SetBusy(bool busy);
    void ClaimBootFind();
    void EnsureWorkerLocked();
    void Loop();
    void Tick();
    void Explicit(CalibrationTrigger trigger);
    void Attempt(CalibrationTrigger trigger, const CalibrationFrame& frame, const CalibrationCross& cross,
                 uint64_t generation);
    bool ResolutionAllows();
    bool SwitchToPanelResolution(uint32_t width, uint32_t height);
    PromptAnswer Confirm(const char* message);
    void Fail(CalibrationTrigger trigger, const char* reason);
    void ShowFallback();
    bool SwitchBackLocked();

    static const char* TriggerName(CalibrationTrigger trigger);

    CalibrationHost*              host_          = nullptr;
    HostPresentation*             presentation_  = nullptr;
    DeviceConfig*                 config_        = nullptr;
    InstanceThreads*              threads_       = nullptr;
    UserAlert*                    alert_         = nullptr;
    NotificationStack*            notifications_ = nullptr;
    CalibrationRun*               run_           = nullptr;
    std::optional<DbLcdPanelSize> panel_;

    mutable std::mutex ga_mutex_;
    bool               ga_shown_       = false;
    bool               stock_switched_ = false;

    std::mutex                        mutex_;
    std::condition_variable           wake_;
    std::thread                       worker_;
    bool                              worker_running_ = false;
    bool                              stop_           = false;
    bool                              busy_           = false;
    bool                              window_active_  = false;
    Clock::time_point                 window_end_;
    std::optional<CalibrationTrigger> request_;

    CalibrationCrossScan scan_;
    CalibrationFrame     last_tick_frame_;
    CrossScanStatus      last_tick_status_ = CrossScanStatus::kNone;
};

}
