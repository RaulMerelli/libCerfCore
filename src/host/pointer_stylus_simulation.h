#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_pointer_feature.h>

#include <atomic>
#include <string>

namespace cerfcore {

class PointerStylusSimulation : public Service, public MachinePointerFeature {
public:
    using Service::Service;

    void OnReady() override;

    std::string ObjectId() const override { return "stylus_simulation"; }
    static constexpr char kObjectName[] = "Stylus simulation";
    std::string ObjectName() const override { return kObjectName; }
    bool        Available() const override;
    bool        Enabled() const override { return enabled_.load(std::memory_order_acquire); }
    void        SetEnabled(bool on) override {
        enabled_.store(on, std::memory_order_release);
        Refresh();
    }
    void        SaveObjectState(StateWriter& w) const override;
    void        RestoreObjectState(StateReader& r) override;

private:
    std::atomic<bool> enabled_{false};
};

}
