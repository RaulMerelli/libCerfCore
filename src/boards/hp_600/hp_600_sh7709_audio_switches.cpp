#include "hp_600_audio_switches.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7709/sh7709_id.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "hp_600_family_id.h"

#include <utility>

namespace cerfcore {

namespace {

constexpr uint8_t kPortKMicrophoneOff = 0x01u;
constexpr uint8_t kPortKSpeakerOff    = 0x20u;

class Hp600Sh7709AudioSwitches : public Hp600AudioSwitches {
public:
    using Hp600AudioSwitches::Hp600AudioSwitches;

    bool ShouldRegister() override {
        const BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::Hp600 && board.GetSocId() == SocId::Sh7709;
    }

    void OnReady() override { wiring_ = &emu_.Get<Sh77xxPortWiring>(); }

    bool SpeakerOn() const override { return (wiring_->NetLevels(kSh77xxPortK) & kPortKSpeakerOff) == 0u; }

    bool MicrophoneOn() const override { return (wiring_->NetLevels(kSh77xxPortK) & kPortKMicrophoneOff) == 0u; }

    void AddListener(std::function<void()> listener) override {
        emu_.Get<Sh77xxPortData>().AddDriveListener(std::move(listener));
    }

private:
    const Sh77xxPortWiring* wiring_ = nullptr;
};

}

REGISTER_SERVICE_AS(Hp600Sh7709AudioSwitches, Hp600AudioSwitches);

}
