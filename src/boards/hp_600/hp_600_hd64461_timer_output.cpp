#include "../../peripherals/hitachi_hd64461/hd64461_timer_output.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../socs/sh77xx/sh77xx_adc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "hp_600_family_id.h"
#include "hp_600_microphone.h"

namespace cerfcore {

namespace {

constexpr uint32_t kTimer0       = 0u;
constexpr uint32_t kAdTriggerPin = 5u;

class Hp600Hd64461TimerOutput : public Hd64461TimerOutput {
public:
    using Hd64461TimerOutput::Hd64461TimerOutput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        pfc_        = &emu_.Get<Sh77xxPfc>();
        adc_        = &emu_.Get<Sh77xxAdc>();
        microphone_ = &emu_.Get<Hp600Microphone>();
    }

    void Pulse(uint32_t channel, uint32_t rate_hz) override {
        if (channel != kTimer0) {
            emu_.Get<Fatal>().Die("Hp600Hd64461TimerOutput: TMO%u# pulse at %u Hz", channel, rate_hz);
        }
        if (pfc_->PinMode(kSh77xxPortH, kAdTriggerPin) != kSh77xxPinModeOther) return;
        microphone_->SetRate(rate_hz);
        adc_->ExternalTrigger();
    }

private:
    Sh77xxPfc*       pfc_        = nullptr;
    Sh77xxAdc*       adc_        = nullptr;
    Hp600Microphone* microphone_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600Hd64461TimerOutput, Hd64461TimerOutput);

}
