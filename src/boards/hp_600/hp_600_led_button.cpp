#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/host/host_widget.h>
#include <cerfcore/host/host_widget_registry.h>
#include <cerfcore/state/hibernation.h>
#include <cerfengine/host_request_channel.h>
#include "../../core/steady_time.h"
#include "../../execution/engine_bridge.h"
#include "../../socs/guest_sample_clock.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "hp_600_family_id.h"
#include "hp_600_battery.h"
#include "hp_600_key_matrix.h"
#include "hp_600_keyboard_layout.h"
#include "hp_600_led_colors.h"
#include "hp_600_notification_led_net.h"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint8_t kVkLedButton = 0x78u;
constexpr GuestCycleClock::Rate kPressHold{10u, 1u};
constexpr uint64_t  kFlashHalfPeriodMs = 500u;
constexpr WidgetRgb kClrOrange{255, 150, 40};
constexpr WidgetRgb kClrRed{235, 50, 40};
constexpr WidgetRgb kClrGreen{120, 255, 140};
constexpr WidgetRgb kClrDark{40, 40, 36};
constexpr WidgetRgb kClrRim{150, 160, 150};
constexpr WidgetRgb kClrRimBlink{120, 230, 130};

enum class Charge { kDark, kCharging, kCharged };

WidgetRgb LedRgb(Hp600LedColor color) {
    switch (color) {
        case Hp600LedColor::kRed:   return kClrRed;
        case Hp600LedColor::kGreen: return kClrGreen;
        default:                    return kClrOrange;
    }
}

const char* LedName(Hp600LedColor color) {
    switch (color) {
        case Hp600LedColor::kRed:   return "red";
        case Hp600LedColor::kGreen: return "green";
        default:                    return "orange";
    }
}

class Hp600LedButton : public Service, public HostWidget {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        wiring_  = &emu_.Get<Sh77xxPortWiring>();
        battery_ = &emu_.Get<Hp600Battery>();
        const Hp600LedColors& colors = emu_.Get<Hp600LedColors>();
        charging_     = colors.ChargingColor();
        notification_ = colors.NotificationColor();
        led_cell_     = emu_.Get<Hp600KeyboardLayout>().ScannedCellOf(kVkLedButton);
        if (led_cell_) {
            matrix_   = &emu_.Get<Hp600KeyMatrix>();
            requests_ = &emu_.Get<EngineBridge>().Engine().Get<cerfengine::HostRequestChannel>();
            requests_->RegisterListener([this] { ApplyPress(); });
            hold_.Attach([this] { Release(); });
            emu_.Get<GuestDeepSleep>().RegisterParkClock([this] { ApplyPress(); });
            emu_.Get<Hibernation>().RegisterPostRestoreListener([this] {
                press_pending_.store(false, std::memory_order_release);
                hold_.Stop();
            });
        }
        emu_.Get<HostWidgetRegistry>().Register(this);
    }

    std::string WidgetName() const override { return led_cell_ ? "LED button" : "LED"; }
    WidgetGroup Group() const override { return WidgetGroup::Indicator; }

    std::string Tooltip() const override {
        std::string text = WidgetName() + " - ";
        switch (CurrentCharge()) {
            case Charge::kCharging: text += LedName(charging_); break;
            case Charge::kCharged:  text += LedName(Hp600LedColor::kGreen); break;
            default:                text += "dark"; break;
        }
        if (Flashing()) text += " and flashing";
        if (led_cell_) text += " (click = press the LED button)";
        return text;
    }

    bool TryPrimaryAction() override {
        if (!led_cell_) return false;
        Press();
        return true;
    }

    std::vector<WidgetControl> Controls() override {
        if (!led_cell_) return {};
        ActionControl press;
        press.id    = "press_button";
        press.label = "Press the LED button";
        press.press = [this] { Press(); };
        return {WidgetControl{std::move(press)}};
    }

    WidgetIcon Icon() const override {
        constexpr int kR = 6;
        const WidgetRgb fill = drawn_flash_lit_ ? LedRgb(notification_) : ChargeColor(drawn_charge_);
        return LedIcon{fill, drawn_flashing_ ? kClrRimBlink : kClrRim, kR};
    }

    bool PollDirty() override {
        const Charge charge    = CurrentCharge();
        const bool   flashing  = Flashing();
        const bool   flash_lit = flashing && HostSteadySquareWaveHigh(kFlashHalfPeriodMs);
        if (charge == drawn_charge_ && flashing == drawn_flashing_ && flash_lit == drawn_flash_lit_) return false;
        drawn_charge_    = charge;
        drawn_flashing_  = flashing;
        drawn_flash_lit_ = flash_lit;
        return true;
    }

private:
    WidgetRgb ChargeColor(Charge charge) const {
        switch (charge) {
            case Charge::kCharging: return LedRgb(charging_);
            case Charge::kCharged:  return LedRgb(Hp600LedColor::kGreen);
            default:                return kClrDark;
        }
    }

    Charge CurrentCharge() const {
        if (battery_->Charging()) return Charge::kCharging;
        return battery_->OnExternalPower() ? Charge::kCharged : Charge::kDark;
    }

    bool Flashing() const { return (wiring_->NetLevels(kSh77xxPortJ) & kHp600PortJNotificationLed) == 0u; }

    void Press() {
        press_pending_.store(true, std::memory_order_release);
        requests_->Request();
    }

    void ApplyPress() {
        if (!press_pending_.exchange(false, std::memory_order_acq_rel)) return;
        matrix_->SetKey(*led_cell_, true);
        hold_.Start(kPressHold);
    }

    void Release() {
        hold_.Stop();
        matrix_->SetKey(*led_cell_, false);
    }

    Sh77xxPortWiring*                wiring_   = nullptr;
    Hp600Battery*                    battery_  = nullptr;
    Hp600KeyMatrix*                  matrix_   = nullptr;
    cerfengine::HostRequestChannel*  requests_ = nullptr;
    std::optional<uint32_t>          led_cell_;
    Hp600LedColor                    charging_     = Hp600LedColor::kOrange;
    Hp600LedColor                    notification_ = Hp600LedColor::kOrange;
    GuestSampleClock                 hold_{emu_};
    std::atomic<bool>                press_pending_{false};
    Charge                           drawn_charge_    = Charge::kDark;
    bool                             drawn_flashing_  = false;
    bool                             drawn_flash_lit_ = false;
};

}

REGISTER_SERVICE(Hp600LedButton);

}
