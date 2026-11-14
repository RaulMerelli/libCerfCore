#include "../../peripherals/open_bus_window.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh7729/sh7729_id.h"
#include "hp_600_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kPostCodePa = 0x02000400u;

class Hp600Sh7729Area0PostCodeOpenBus : public OpenBusWindow {
public:
    using OpenBusWindow::OpenBusWindow;

    uint32_t MmioBase() const override { return kPostCodePa; }
    uint32_t MmioSize() const override { return 2u; }

protected:
    std::string_view WindowBoardFamilyId() const override { return BoardFamilyId::Hp600; }
    bool             WindowOnBoard(const BoardContext& bd) const override { return bd.GetSocId() == SocId::Sh7729; }
    const char*      WindowTag() const override { return "Hp600Sh7729Area0PostCodeOpenBus"; }

    bool Floats(uint32_t addr, uint32_t bits, bool write) const override {
        return write && bits == 16u && addr == kPostCodePa;
    }
};

}

REGISTER_SERVICE(Hp600Sh7729Area0PostCodeOpenBus);

}
