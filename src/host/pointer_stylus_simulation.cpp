#include "pointer_stylus_simulation.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/host/pointer_input.h>
#include <cerfcore/host/pointer_router.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

REGISTER_SERVICE(PointerStylusSimulation);

void PointerStylusSimulation::OnReady() {
    if (emu_.TryGet<PointerInput>() == nullptr) return;
    auto&                tree   = emu_.Get<MachineObjectTree>();
    PointerRouter&       owner  = emu_.Get<PointerRouter>();
    const MachineObject* router = &owner;
    tree.Add(this, &owner);
    tree.Subscribe([this, router](const MachineEvent& event) {
        if (event.kind == MachineEventKind::kChanged && event.object == router) Refresh();
    });
}

bool PointerStylusSimulation::Available() const {
    auto* ga = emu_.TryGet<PointerInput>();
    return ga && emu_.Get<PointerRouter>().Active() == static_cast<PointerSource*>(ga);
}

void PointerStylusSimulation::SaveObjectState(StateWriter& w) const {
    w.Write<uint8_t>("stylus_sim_enabled", Enabled() ? 1u : 0u);
}

void PointerStylusSimulation::RestoreObjectState(StateReader& r) {
    uint8_t on = 0;
    r.Read("stylus_sim_enabled", on);
    SetEnabled(on != 0);
}

}
