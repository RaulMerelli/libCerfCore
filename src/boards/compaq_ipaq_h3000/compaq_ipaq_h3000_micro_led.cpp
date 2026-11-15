#include "compaq_ipaq_h3000_micro_led.h"

#include "compaq_ipaq_h3000_battery.h"
#include "compaq_ipaq_h3000_family_id.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint8_t   kColorYellow       = 0;
constexpr uint8_t   kColorGreen        = 1;
constexpr uint32_t  kMsPerTick         = 100;
constexpr uint32_t  kTicksForZero      = 256;
constexpr uint32_t  kChargeBlinkHalfMs = 1000;
constexpr ObjectRgb kClrGreen{120, 255, 140};
constexpr ObjectRgb kClrYellow{255, 200, 40};

uint32_t Ticks(uint8_t field) { return field == 0u ? kTicksForZero : field; }

}

REGISTER_SERVICE(CompaqIpaqH3000MicroLed);

bool CompaqIpaqH3000MicroLed::ShouldRegister() {
    auto* bd = emu_.TryGet<BoardContext>();
    return bd && bd->GetBoardFamilyId() == BoardFamilyId::CompaqIpaqH3000;
}

void CompaqIpaqH3000MicroLed::OnReady() {
    battery_ = &emu_.Get<CompaqIpaqH3000Battery>();
    battery_->AddChangeListener([this] { Refresh(); });
    emu_.Get<MachineObjectTree>().Add(this);
}

void CompaqIpaqH3000MicroLed::SetLed(const uint8_t* req, uint8_t len) {
    if (len != 4) {
        emu_.Get<Fatal>().Die("CompaqIpaqH3000MicroLed: LED request of %u bytes", len);
    }
    if (req[0] != kColorYellow && req[0] != kColorGreen) {
        emu_.Get<Fatal>().Die("CompaqIpaqH3000MicroLed: LED request %02X %02X %02X %02X",
                              req[0], req[1], req[2], req[3]);
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        led_ = Setting{req[0], req[1], req[2], req[3]};
    }
    Refresh();
}

void CompaqIpaqH3000MicroLed::PowerOn() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        led_ = Setting{};
    }
    Refresh();
}

void CompaqIpaqH3000MicroLed::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(mtx_);
    w.Write("micro_led_color", led_.color);
    w.Write("micro_led_flags", led_.flags);
    w.Write("micro_led_on_time", led_.on_time);
    w.Write("micro_led_off_time", led_.off_time);
}

void CompaqIpaqH3000MicroLed::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(mtx_);
    r.Read("micro_led_color", led_.color);
    r.Read("micro_led_flags", led_.flags);
    r.Read("micro_led_on_time", led_.on_time);
    r.Read("micro_led_off_time", led_.off_time);
}

CompaqIpaqH3000MicroLed::Setting CompaqIpaqH3000MicroLed::Current() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return led_;
}

LampState CompaqIpaqH3000MicroLed::Lamp() const {
    const Setting s = Current();
    if (s.color == kColorGreen)
        return LampState{true, true, kClrGreen, Ticks(s.on_time) * kMsPerTick, Ticks(s.off_time) * kMsPerTick};
    if (!battery_->OnAc()) return LampState{false, false, kClrYellow};
    if (!battery_->Charging()) return LampState{true, false, kClrYellow};
    return LampState{true, true, kClrYellow, kChargeBlinkHalfMs, kChargeBlinkHalfMs};
}

}
