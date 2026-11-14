#include <cerfcore/state/state_boot_gate.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/execution/jit_runner.h>
#include <cerfcore/state/hibernation.h>

#include <utility>

namespace cerfcore {

REGISTER_SERVICE(StateBootGate);

void StateBootGate::OnReady() {
    emu_.Get<JitRunner>().RegisterPreStartHook([this] { Run(); });
}

void StateBootGate::RegisterColdStartListener(std::function<void()> fn) { cold_start_listeners_.Add(std::move(fn)); }

void StateBootGate::Run() {
    auto& hib  = emu_.Get<Hibernation>();
    bool  cold = true;
    if (hib.DefaultStateExists()) {
        switch (emu_.Get<DeviceConfig>().boot_mode) {
            case StateBootMode::Cold:   break;
            case StateBootMode::Warm:   cold = !hib.Restore("", true); break;
            case StateBootMode::Resume: cold = !hib.Restore("", false); break;
        }
    }
    if (cold) cold_start_listeners_.Call();
}

}
