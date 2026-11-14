#include "engine_bridge.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/state/state_stream.h>
#include <cerfengine/sh/sh_cpu.h>
#include <cerfengine/sh/sh_interrupt_channel.h>
#include <cerfengine/sh/sh_mmu.h>
#include "../cpu/sh_processor_config.h"
#include "../tracing/trace_manager.h"
#include "guest_cycle_clock.h"

namespace cerfcore {

namespace {

template <typename F>
constexpr void VisitCpuState(cerfengine::ShCpuState& s, F& field) {
    field("gpr", s.gpr);
    field("bank", s.bank);
    field("sr", s.sr);
    field("gbr", s.gbr);
    field("vbr", s.vbr);
    field("ssr", s.ssr);
    field("spc", s.spc);
    field("mach", s.mach);
    field("macl", s.macl);
    field("pr", s.pr);
    field("pc", s.pc);
    field("tra", s.tra);
    field("expevt", s.expevt);
    field("intevt", s.intevt);
    field("rs", s.rs);
    field("re", s.re);
    field("mod", s.mod);
    field("dsr", s.dsr);
    field("a0", s.a0);
    field("a0g", s.a0g);
    field("a1", s.a1);
    field("a1g", s.a1g);
    field("m0", s.m0);
    field("m1", s.m1);
    field("x0", s.x0);
    field("x1", s.x1);
    field("y0", s.y0);
    field("y1", s.y1);
    field("guest_cycle_counter", s.guest_cycle_counter);
    field.Skip(s.guest_cycle_deadline);
    field("guest_cycle_hi", s.guest_cycle_hi);
    field("guest_cycle_folded", s.guest_cycle_folded);
    field.Skip(s.chain_exit_request);
    field("ldtlb_shadow", s.ldtlb_shadow);
    field("rte_slot", s.rte_slot);
    field("sleeping", s.sleeping);
    field("deep_sleep", s.deep_sleep);
    field.Skip(s.mmio_access);
}

template <typename F>
void VisitCpuSection(cerfengine::ShCpuState& s, uint32_t& reset_pending, cerfengine::ShResetKind& reset_kind,
                     F& field) {
    VisitCpuState(s, field);
    field("reset_pending", reset_pending);
    field("reset_kind", reset_kind);
}

template <typename F>
constexpr void VisitMmuSection(cerfengine::ShMmuState& s, F& field) {
    for (uint32_t set = 0; set < cerfengine::kShTlbSets; ++set) {
        for (uint32_t way = 0; way < cerfengine::kShTlbWays; ++way) {
            cerfengine::ShTlbEntry& e = s.tlb[set][way];
            field("tlb_vpn", e.vpn);
            field("tlb_data", e.data);
            field("tlb_asid", e.asid);
            field("tlb_valid", e.valid);
            field("tlb_defined", e.defined);
            field.Skip(e.pad);
        }
    }
    field("mmucr", s.mmucr);
    field("pteh", s.pteh);
    field("ptel", s.ptel);
    field("ttb", s.ttb);
    field("tea", s.tea);
    field("defined", s.defined);
}

class ShEngineBridge : public EngineBridge {
public:
    using EngineBridge::EngineBridge;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetCpuArch() == CpuArch::kSh;
    }

    void SaveCpuState(StateWriter& w) override {
        static_assert(StateVisitCoversAllBytes<cerfengine::ShCpuState>(
                          [](cerfengine::ShCpuState& s, StateFieldBytes& f) { VisitCpuState(s, f); }),
                      "the SH CPU visitor must name or skip every field of ShCpuState");
        cerfengine::ShCpu& cpu           = Engine().Get<cerfengine::ShCpu>();
        uint32_t           reset_pending = Engine().Get<cerfengine::ShInterruptChannel>().ResetRequested() ? 1u : 0u;
        cerfengine::ShResetKind reset_kind = cpu.PendingResetKind();
        StateWriteField         field(w);
        VisitCpuSection(*cpu.State(), reset_pending, reset_kind, field);
    }

    void RestoreCpuState(StateReader& r) override {
        cerfengine::ShCpu&      cpu           = Engine().Get<cerfengine::ShCpu>();
        uint32_t                reset_pending = 0u;
        cerfengine::ShResetKind reset_kind    = cerfengine::ShResetKind::kPowerOn;
        StateReadField          field(r);
        VisitCpuSection(*cpu.State(), reset_pending, reset_kind, field);
        cpu.SetPendingResetKind(reset_kind);
        Engine().Get<cerfengine::ShInterruptChannel>().OnCpuStateRestored(reset_pending != 0u);
        emu_.Get<GuestCycleClock>().OnCyclesRestored();
    }

    void SaveMmuState(StateWriter& w) override {
        static_assert(StateVisitCoversAllBytes<cerfengine::ShMmuState>(
                          [](cerfengine::ShMmuState& s, StateFieldBytes& f) { VisitMmuSection(s, f); }),
                      "the SH MMU visitor must name or skip every field of ShMmuState");
        StateWriteField field(w);
        VisitMmuSection(*Engine().Get<cerfengine::ShMmu>().State(), field);
    }

    void RestoreMmuState(StateReader& r) override {
        StateReadField field(r);
        VisitMmuSection(*Engine().Get<cerfengine::ShMmu>().State(), field);
    }

    void DispatchTraceIter() override {
#if CERF_DEV_MODE
        Trace().DispatchRunLoopIterSh(Engine().Get<cerfengine::ShCpu>().State());
#endif
    }

    void MapInjectionBand(uint32_t va, uint32_t pa, uint32_t size) override {
        emu_.Get<Fatal>().Die("ShEngineBridge: MapInjectionBand va=0x%08X pa=0x%08X size=0x%X is not implemented",
                              va, pa, size);
    }

protected:
    void FillSetup(cerfengine::Setup& setup) override {
        setup.isa    = cerfengine::Isa::kSh;
        setup.sh.dsp = emu_.Get<ShProcessorConfig>().HasDsp();
    }

    void ColdBoot() override {
        cerfengine::ShCpu& cpu = Engine().Get<cerfengine::ShCpu>();
        cpu.ResetState(cerfengine::ShResetKind::kPowerOn);
        LOG(Jit, "ShEngineBridge::ColdBoot: reset PC=0x%08X\n", cpu.State()->pc);
    }
};

}

REGISTER_SERVICE_AS(ShEngineBridge, EngineBridge);

}
