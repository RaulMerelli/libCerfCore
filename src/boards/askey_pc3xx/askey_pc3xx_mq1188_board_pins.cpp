#include "../../peripherals/mediaq_mq1188/mediaq_mq1188_board_pins.h"

#include <cerfcore/boards/board_context.h>
#include "askey_pc3xx_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../socs/pxa255/pxa255_gpio.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kGpioMq1188Irl      = 21u;
constexpr uint32_t kMqGpioSdCardDetect = 2u;

class AskeyPc3xxMq1188BoardPins : public MediaQMq1188BoardPins {
public:
    using MediaQMq1188BoardPins::MediaQMq1188BoardPins;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::AskeyPc3xx;
    }

    void SetInterruptLevel(bool high) override { emu_.Get<Pxa255Gpio>().SetInputLevel(kGpioMq1188Irl, high); }

    bool GpioInputHigh(uint32_t gpio) override {
        if (gpio != kMqGpioSdCardDetect)
            emu_.Get<Fatal>().Die("Askey PC3xx: MQ1188 GPIO %u input level is not modeled", gpio);
        return true;
    }
};

}

REGISTER_SERVICE_AS(AskeyPc3xxMq1188BoardPins, MediaQMq1188BoardPins);

}
