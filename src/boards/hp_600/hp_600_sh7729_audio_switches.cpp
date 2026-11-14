#include "hp_600_audio_switches.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/hitachi_hd64461/hd64461_gpio.h"
#include "../../peripherals/hitachi_hd64461/hd64461_gpio_wiring.h"
#include "../../socs/sh7729/sh7729_id.h"
#include "hp_600_family_id.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr uint8_t kGpioASpeakerOff = 0x01u;

class Hp600Sh7729AudioSwitches : public Hp600AudioSwitches {
public:
    using Hp600AudioSwitches::Hp600AudioSwitches;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7729;
    }

    void OnReady() override { wiring_ = &emu_.Get<Hd64461GpioWiring>(); }

    bool SpeakerOn() const override { return (wiring_->NetLevels(Hd64461Gpio::kPortA) & kGpioASpeakerOff) == 0u; }

    bool MicrophoneOn() const override { return true; }

    void AddListener(std::function<void()> listener) override {
        emu_.Get<Hd64461Gpio>().AddPinListener(std::move(listener));
    }

private:
    const Hd64461GpioWiring* wiring_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600Sh7729AudioSwitches, Hp600AudioSwitches);

}
