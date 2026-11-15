#include "../../socs/sh77xx/sh77xx_serial_wiring.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "../../socs/sh77xx/sh77xx_pfc.h"
#include "../../socs/sh77xx/sh77xx_port_data.h"
#include "../../socs/sh77xx/sh77xx_port_wiring.h"
#include "../../socs/sh77xx/sh77xx_scif.h"
#include "hp_600_family_id.h"
#include "hp_600_input_nets.h"

#include <atomic>
#include <utility>

namespace cerfcore {

namespace {

constexpr char kSerialPortId[]    = "serial_port";
constexpr char kSerialPortLabel[] = "Serial port";

constexpr uint8_t kPortScDriverPowerOff = 0x08u;
constexpr uint8_t kPortScRtsOff         = 0x40u;
constexpr uint8_t kPortHDtrOff          = 0x80u;

class Hp600SerialWiring : public Sh77xxSerialWiring {
public:
    using Sh77xxSerialWiring::Sh77xxSerialWiring;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        ports_  = &emu_.Get<Sh77xxPortWiring>();
        inputs_ = &emu_.Get<Hp600InputNets>();
        emu_.Get<Sh77xxPortData>().AddDriveListener([this] { TrackNets(); });
        TrackNets();
    }

    std::optional<ObjectIdentity> Cradle(uint32_t channel_base) const override {
        if (channel_base != kSh77xxScif2Base) return std::nullopt;
        return ObjectIdentity{kSerialPortId, kSerialPortLabel};
    }

    bool DriverPowered(uint32_t channel_base) const override {
        return channel_base == kSh77xxScif2Base && powered_.load(std::memory_order_acquire);
    }

    SerialControlLines Control(uint32_t) const override {
        SerialControlLines lines;
        lines.dtr = dtr_.load(std::memory_order_acquire);
        lines.rts = rts_.load(std::memory_order_acquire);
        return lines;
    }

    void SetModemInputs(uint32_t, bool cts, bool, bool, bool dcd) override {
        inputs_->Drive(Hp600InputNet::kSerialCtsOff, !cts);
        emu_.Get<Sh77xxIntc>().SetPinLevel(Sh77xxIntcPin::kIrq2, !dcd);
    }

    void SetControlListener(uint32_t, std::function<void()> listener) override { listener_ = std::move(listener); }

private:
    void TrackNets() {
        const uint8_t port_sc = ports_->NetLevels(kSh77xxPortSc);
        const uint8_t port_h  = ports_->NetLevels(kSh77xxPortH);
        const bool    powered = (port_sc & kPortScDriverPowerOff) == 0u;
        const bool    dtr     = powered && (port_h & kPortHDtrOff) == 0u;
        const bool    rts     = powered && (port_sc & kPortScRtsOff) == 0u;
        bool changed = powered_.exchange(powered, std::memory_order_acq_rel) != powered;
        if (dtr_.exchange(dtr, std::memory_order_acq_rel) != dtr) changed = true;
        if (rts_.exchange(rts, std::memory_order_acq_rel) != rts) changed = true;
        if (changed && listener_) listener_();
    }

    Sh77xxPortWiring*     ports_  = nullptr;
    Hp600InputNets*       inputs_ = nullptr;
    std::atomic<bool>     powered_{false};
    std::atomic<bool>     dtr_{false};
    std::atomic<bool>     rts_{false};
    std::function<void()> listener_;
};

}

REGISTER_SERVICE_AS(Hp600SerialWiring, Sh77xxSerialWiring);

}
