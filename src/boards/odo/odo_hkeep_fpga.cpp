#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "odo_family_id.h"
#include <cerfcore/host/machine_indicators.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t kHkeepFpgaPaBase   = 0x04040000u;
constexpr uint32_t kHkeepFpgaSize     = 0x00030000u;

constexpr uint32_t kLedDiscretePa     = 0x04040000u;
constexpr uint32_t kLedAlphaPa        = 0x04060000u;
constexpr uint32_t kLedAlphaMirror    = 0x0000FFFFu;
constexpr uint32_t kDiscreteLeds      = 8u;
constexpr ObjectRgb kLedColor{255, 180, 40};

class OdoHkeepFpga : public Peripheral, public MachineCharacterDisplay {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::Odo;
    }
    void OnReady() override {
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            std::lock_guard<std::mutex> lk(state_mutex_);
            led_discrete_value_ = 0;
            led_alpha_value_    = 0;
        });
        emu_.Get<PeripheralDispatcher>().Register(this);
        auto& tree = emu_.Get<MachineObjectTree>();
        tree.Add(this);
        for (auto& led : leds_) tree.Add(&led, this);
    }

    uint32_t MmioBase() const override { return kHkeepFpgaPaBase; }
    uint32_t MmioSize() const override { return kHkeepFpgaSize; }

    uint32_t ReadWord (uint32_t addr) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lk(state_mutex_);
        w.Write<uint32_t>("led_discrete_value", led_discrete_value_);
        w.Write<uint32_t>("led_alpha_value", led_alpha_value_);
    }
    void RestoreState(StateReader& r) override {
        std::lock_guard<std::mutex> lk(state_mutex_);
        r.Read("led_discrete_value", led_discrete_value_);
        r.Read("led_alpha_value", led_alpha_value_);
    }

    std::string ObjectId() const override { return "debug_display"; }
    static constexpr char kObjectName[] = "Debug LED";
    std::string ObjectName() const override { return kObjectName; }
    std::string Characters() const override;

private:
    class DiscreteLed final : public MachineLamp {
    public:
        DiscreteLed(OdoHkeepFpga& owner, uint32_t bit) : owner_(owner), bit_(bit) {}
        std::string ObjectId() const override { return "led" + std::to_string(bit_); }
        static constexpr char kObjectNameFormat[] = "LED %u";
        std::string ObjectName() const override {
            char name[sizeof(kObjectNameFormat) + 8];
            std::snprintf(name, sizeof(name), kObjectNameFormat, static_cast<unsigned>(bit_));
            return name;
        }
        LampState   Lamp() const override { return LampState{owner_.DiscreteLit(bit_), false, kLedColor}; }

        using MachineLamp::Refresh;

    private:
        OdoHkeepFpga& owner_;
        uint32_t      bit_;
    };

    bool ClassifyAddr(uint32_t addr, bool* out_is_alpha) const;
    bool DiscreteLit(uint32_t bit) const;

    void LogAlphaWrite(uint32_t addr, uint32_t value);

    mutable std::mutex state_mutex_;
    uint32_t           led_discrete_value_ = 0;
    uint32_t           led_alpha_value_    = 0;

    DiscreteLed leds_[kDiscreteLeds] = {{*this, 0}, {*this, 1}, {*this, 2}, {*this, 3},
                                        {*this, 4}, {*this, 5}, {*this, 6}, {*this, 7}};
};

bool OdoHkeepFpga::DiscreteLit(uint32_t bit) const {
    std::lock_guard<std::mutex> lk(state_mutex_);
    return (led_discrete_value_ >> bit) & 1u;
}

bool OdoHkeepFpga::ClassifyAddr(uint32_t addr, bool* out_is_alpha) const {
    if (addr == kLedDiscretePa) {
        *out_is_alpha = false;
        return true;
    }
    /* Alpha mirror: LED_ALPHA + (offset & LED_ALPHA_MIRROR).
       Any DWORD-aligned address in [LED_ALPHA, LED_ALPHA + 0xFFFF]
       hits the single alpha register. */
    if (addr >= kLedAlphaPa && addr <= kLedAlphaPa + kLedAlphaMirror) {
        *out_is_alpha = true;
        return true;
    }
    return false;
}

uint32_t OdoHkeepFpga::ReadWord(uint32_t addr) {
    bool is_alpha = false;
    if (!ClassifyAddr(addr, &is_alpha)) {
        HaltUnsupportedAccess("ReadWord", addr, 0);
    }

    uint32_t value;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        value = is_alpha ? led_alpha_value_ : led_discrete_value_;
    }

#if CERF_DEV_MODE
    LOG(Board, "Odo HKEEP %s read  [0x%08X] -> 0x%08X\n",
        is_alpha ? "LED_ALPHA" : "LED_DISCRETE", addr, value);
#endif
    return value;
}

void OdoHkeepFpga::WriteWord(uint32_t addr, uint32_t value) {
    bool is_alpha = false;
    if (!ClassifyAddr(addr, &is_alpha)) {
        HaltUnsupportedAccess("WriteWord", addr, value);
    }

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (is_alpha) led_alpha_value_    = value;
        else          led_discrete_value_ = value;
    }

    if (is_alpha) {
        Refresh();
        LogAlphaWrite(addr, value);
    } else {
        for (DiscreteLed& led : leds_) led.Refresh();
#if CERF_DEV_MODE
        /* Same 1 kHz consideration as the read path. */
        LOG(Board, "Odo HKEEP LED_DISCRETE = 0x%08X\n", value);
#endif
    }
}

void OdoHkeepFpga::LogAlphaWrite(uint32_t addr, uint32_t value) {
    /* LED_ALPHA writes are LOW-frequency on this build path
       (boot stub WhereAmI, OEMInit's 0xECEEECEE, and the very
       rare OEMWriteDebugLED-triggered prints). Permanent LOG is
       acceptable - these are major boot landmarks. */
    char as_ascii[5] = {
        static_cast<char>((value      ) & 0xFFu),
        static_cast<char>((value >>  8) & 0xFFu),
        static_cast<char>((value >> 16) & 0xFFu),
        static_cast<char>((value >> 24) & 0xFFu),
        0,
    };
    auto sanitize = [](char c) -> char {
        return (c >= 0x20 && c < 0x7F) ? c : '.';
    };
    const uint32_t offset = addr - kLedAlphaPa;
    LOG(Board, "Odo HKEEP LED_ALPHA +0x%04X = 0x%08X (\"%c%c%c%c\")\n",
        offset, value,
        sanitize(as_ascii[0]),
        sanitize(as_ascii[1]),
        sanitize(as_ascii[2]),
        sanitize(as_ascii[3]));
}

std::string OdoHkeepFpga::Characters() const {
    uint32_t alpha;
    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        alpha = led_alpha_value_;
    }
    std::string text;
    for (int shift = 0; shift < 32; shift += 8) {
        const char c = static_cast<char>((alpha >> shift) & 0xFFu);
        text += (c >= 0x20 && c < 0x7F) ? c : '.';
    }
    return text;
}

}  /* namespace */

REGISTER_SERVICE(OdoHkeepFpga);

}
