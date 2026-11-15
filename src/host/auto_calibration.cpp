#include <cerfcore/host/auto_calibration.h>

#include "calibration_frame_source.h"
#include "calibration_run.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/core/log.h>
#include <cerfcore/core/user_config_writer.h>
#include <cerfcore/host/host_presentation.h>
#include <cerfcore/peripherals/cerf_virt/cerf_virt_config_apply.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_boot_gate.h>

#include <utility>

namespace cerfcore {

namespace {

using Ms = std::chrono::milliseconds;

constexpr std::chrono::seconds kWindowLength{60};
constexpr Ms                   kTickInterval{500};
constexpr uint32_t             kResolutionDrift = 50;

}

REGISTER_SERVICE(AutoCalibration);

void AutoCalibration::OnReady() {
    host_          = &emu_.Get<CalibrationHost>();
    frames_        = &emu_.Get<CalibrationFrameSource>();
    presentation_  = &emu_.Get<HostPresentation>();
    config_        = &emu_.Get<DeviceConfig>();
    threads_       = &emu_.Get<InstanceThreads>();
    run_           = emu_.TryGet<CalibrationRun>();
    panel_         = emu_.Get<BoardDatabase>().FindBoard(config_->board_id)->lcd_panel_size;
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        if (emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) return;
        BeginWindow();
    });
    emu_.Get<StateBootGate>().RegisterColdStartListener([this] { BeginWindow(); });
}

void AutoCalibration::OnShutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_          = true;
        window_active_ = false;
        request_.reset();
        wake_.notify_all();
    }
    if (run_) run_->Abort();
    if (worker_.joinable()) worker_.join();
}

bool AutoCalibration::CanPress() const { return run_ != nullptr; }

void AutoCalibration::RegisterFailureListener(std::function<void(CalibrationTrigger)> listener) {
    failure_listeners_.Add(std::move(listener));
}

void AutoCalibration::Request(CalibrationTrigger trigger) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) return;
    if (busy_ || request_) {
        LOG(Cerf, "%s: a calibration cycle is already running\n", TriggerName(trigger));
        return;
    }
    LOG(Cerf, "%s: calibration cycle requested\n", TriggerName(trigger));
    request_ = trigger;
    EnsureWorkerLocked();
    wake_.notify_all();
}

void AutoCalibration::BeginWindow() {
    if (!run_) return;
    if (!config_->automatic_calibration) {
        LOG(Cerf, "automatic calibration is off, no boot scan\n");
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_) return;
    window_active_ = true;
    window_end_    = Clock::now() + kWindowLength;
    request_.reset();
    LOG(Cerf, "scanning for the calibration screen for %lld s\n", static_cast<long long>(kWindowLength.count()));
    EnsureWorkerLocked();
    wake_.notify_all();
}

void AutoCalibration::StopWindow() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (window_active_) LOG(Cerf, "boot scan stopped\n");
    window_active_ = false;
}

void AutoCalibration::SetBusy(bool busy) {
    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = busy;
}

void AutoCalibration::ClaimBootFind() {
    std::lock_guard<std::mutex> lock(mutex_);
    busy_ = true;
    if (!request_) return;
    LOG(Cerf, "%s: the boot scan found the target first, the request is dropped\n", TriggerName(*request_));
    request_.reset();
}

void AutoCalibration::EnsureWorkerLocked() {
    if (worker_running_) return;
    if (worker_.joinable()) worker_.join();
    worker_running_ = true;
    worker_         = threads_->Start([this] { Loop(); });
}

void AutoCalibration::Loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        if (request_) {
            const CalibrationTrigger trigger = *request_;
            request_.reset();
            busy_ = true;
            lock.unlock();
            Explicit(trigger);
            lock.lock();
            busy_ = false;
            continue;
        }
        if (!window_active_) break;
        if (Clock::now() >= window_end_) {
            window_active_ = false;
            LOG(Cerf, "no calibration screen within %lld s, the boot scan ends\n",
                static_cast<long long>(kWindowLength.count()));
            break;
        }
        lock.unlock();
        Tick();
        lock.lock();
        wake_.wait_for(lock, kTickInterval, [this] { return stop_ || request_.has_value(); });
    }
    worker_running_ = false;
}

void AutoCalibration::Tick() {
    CalibrationFrame frame;
    if (!frames_->Grab(frame)) return;
    if (frame == last_tick_frame_) return;
    CalibrationCross      cross;
    const CrossScanStatus status = scan_.Find(frame, cross);
    if (status != last_tick_status_)
        LOG(Cerf, "boot scan of the %ux%u screen: %s\n", frame.width, frame.height,
            CalibrationCrossScan::StatusName(status));
    last_tick_status_ = status;
    if (status != CrossScanStatus::kFound) {
        last_tick_frame_ = std::move(frame);
        return;
    }
    last_tick_frame_ = CalibrationFrame{};
    ClaimBootFind();
    StopWindow();
    const uint64_t generation = run_->AbortGeneration();
    if (ResolutionAllows())
        Attempt(CalibrationTrigger::kBootWindow, frame, cross, generation);
    SetBusy(false);
}

void AutoCalibration::Explicit(CalibrationTrigger trigger) {
    const uint64_t generation = run_->AbortGeneration();
    if (!ResolutionAllows()) return;
    CalibrationFrame frame;
    CalibrationCross cross;
    CrossScanStatus  status = CrossScanStatus::kNone;
    if (frames_->Grab(frame)) status = scan_.Find(frame, cross);
    if (run_->AbortGeneration() != generation) {
        LOG(Cerf, "%s: a reset, a state restore or shutdown stopped the scan\n", TriggerName(trigger));
        return;
    }
    if (status != CrossScanStatus::kFound) {
        LOG(Cerf, "%s scan of the %ux%u screen: %s\n", TriggerName(trigger), frame.width, frame.height,
            CalibrationCrossScan::StatusName(status));
        Fail(trigger, "no calibration target on the screen");
        return;
    }
    if (trigger == CalibrationTrigger::kGuestAdditions) StopWindow();
    Attempt(trigger, frame, cross, generation);
}

void AutoCalibration::Attempt(CalibrationTrigger trigger, const CalibrationFrame& frame, const CalibrationCross& cross,
                              uint64_t generation) {
    LOG(Cerf, "%s found the %s target at (%d,%d), guest turn %d, on the %ux%u screen\n", TriggerName(trigger),
        CalibrationCrossScan::ZoneName(cross.zone), cross.press_x, cross.press_y, cross.quarter, frame.width,
        frame.height);
    if (!CalibrationApproved()) {
        LOG(Cerf, "automatic calibration not approved\n");
        return;
    }
    switch (run_->Perform(generation)) {
        case CalibrationOutcome::kDone:
            LOG(Cerf, "automatic calibration finished\n");
            host_->CalibrationFinished();
            return;
        case CalibrationOutcome::kAborted:
            LOG(Cerf, "automatic calibration stopped by a reset, a state restore or shutdown\n");
            return;
        case CalibrationOutcome::kFailed: Fail(trigger, "the calibration run stopped before the last target"); return;
    }
}

bool AutoCalibration::ResolutionAllows() {
    if (!config_->guest_additions || !panel_) return true;
    const uint32_t w = presentation_->SurfaceWidth();
    const uint32_t h = presentation_->SurfaceHeight();
    if (w <= panel_->width + kResolutionDrift && h <= panel_->height + kResolutionDrift) return true;
    LOG(Cerf, "the Guest Additions screen is %ux%u, the board panel is %ux%u\n", w, h, panel_->width,
        panel_->height);
    if (PanelResolutionApproved(panel_->width, panel_->height))
        return SwitchToPanelResolution(panel_->width, panel_->height);
    LOG(Cerf, "the custom resolution stays, calibrating at it\n");
    return true;
}

bool AutoCalibration::SwitchToPanelResolution(uint32_t width, uint32_t height) {
    LOG(Cerf, "switching the Guest Additions resolution to %ux%u and soft resetting\n", width, height);
    const auto previous = config_->Live();
    if (emu_.Get<UserConfigWriter>().WriteConfigurableScreenSize(width, height)) {
        emu_.Get<CerfVirtConfigApply>().Apply(GuestReset::kSoft, previous->board_configurable_screen_width,
                                              previous->board_configurable_screen_height);
        return false;
    }
    LOG(Cerf, "the panel resolution could not be written to the device configuration, calibrating at the custom "
              "resolution\n");
    return true;
}

bool AutoCalibration::CalibrationApproved() {
    if (config_->calibration_auto_answer) {
        LOG(Cerf, "--auto-calibration approves the calibration\n");
        return true;
    }
    return host_->ApprovesCalibration();
}

bool AutoCalibration::PanelResolutionApproved(uint32_t width, uint32_t height) {
    if (config_->calibration_auto_answer) {
        LOG(Cerf, "--auto-calibration approves the panel resolution\n");
        return true;
    }
    return host_->ApprovesPanelResolution(width, height);
}

void AutoCalibration::Fail(CalibrationTrigger trigger, const char* reason) {
    LOG(Cerf, "%s: automatic calibration failed, %s\n", TriggerName(trigger), reason);
    if (!config_->calibration_auto_answer) host_->CalibrationFailed();
    failure_listeners_.Call(trigger);
}

const char* AutoCalibration::TriggerName(CalibrationTrigger trigger) {
    switch (trigger) {
        case CalibrationTrigger::kBootWindow:     return "boot scan";
        case CalibrationTrigger::kMenu:           return "menu";
        case CalibrationTrigger::kGuestAdditions: return "Guest Additions";
    }
    return "boot scan";
}

}
