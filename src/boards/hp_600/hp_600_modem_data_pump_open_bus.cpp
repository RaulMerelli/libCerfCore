#include "../../peripherals/open_bus_window.h"

#include <cerfcore/core/cerf_emulator.h>
#include "hp_600_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kDataPumpPa        = 0x08000000u;
constexpr uint32_t kFirstSelfTestReg  = 0x1Au;
constexpr uint32_t kSelfTestRegs      = 4u;
constexpr uint32_t kRegisterStride    = 2u;
constexpr uint32_t kSelfTestWindowPa  = kDataPumpPa + kFirstSelfTestReg * kRegisterStride;

class Hp600ModemDataPumpOpenBus : public OpenBusWindow {
public:
    using OpenBusWindow::OpenBusWindow;

    uint32_t MmioBase() const override { return kSelfTestWindowPa; }
    uint32_t MmioSize() const override { return kSelfTestRegs * kRegisterStride; }

protected:
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::Hp600; }
    const char*      WindowTag() const override { return "Hp600ModemDataPumpOpenBus"; }

    bool Floats(uint32_t addr, uint32_t bits, bool write) const override {
        return !write && bits == 8u && (addr - kSelfTestWindowPa) % kRegisterStride == 0u;
    }
};

}

REGISTER_SERVICE(Hp600ModemDataPumpOpenBus);

}
