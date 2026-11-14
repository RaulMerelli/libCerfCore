#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/memory_kind.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../page_table_builder.h"
#include "hp_600_family_id.h"

#include <cstdint>
#include <iterator>

namespace cerfcore {

namespace {

struct RomWriteCycle {
    uint32_t pa;
    uint16_t value;
};

constexpr RomWriteCycle kStartCodeSequence[] = {
    { 0x00000004u, 0x5656u },
    { 0x00000000u, 0x5651u },
    { 0x00000004u, 0x0000u },
};

constexpr uint32_t kSequenceLength = static_cast<uint32_t>(std::size(kStartCodeSequence));

class Hp600Rom : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::Hp600;
    }

    void OnReady() override {
        for (const BackedRegion& r : emu_.Get<PageTableBuilder>().BackedMemoryRegions()) {
            if (r.kind == MemoryKind::kRom) rom_ = r;
        }
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return rom_.pa_base; }
    uint32_t MmioSize() const override { return rom_.size; }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        if (IsCycle(0u, addr, value)) {
            next_cycle_ = 1u;
            return;
        }
        if (next_cycle_ != 0u && IsCycle(next_cycle_, addr, value)) {
            next_cycle_ = (next_cycle_ + 1u) % kSequenceLength;
            return;
        }
        HaltUnsupportedAccess("WriteHalf", addr, value);
    }

    void SaveState(StateWriter& w) override { w.Write("next_cycle", next_cycle_); }
    void RestoreState(StateReader& r) override { r.Read("next_cycle", next_cycle_); }

private:
    bool IsCycle(uint32_t index, uint32_t addr, uint16_t value) const {
        return kStartCodeSequence[index].pa == addr && kStartCodeSequence[index].value == value;
    }

    BackedRegion rom_{};
    uint32_t     next_cycle_ = 0u;
};

}

REGISTER_SERVICE(Hp600Rom);

}
