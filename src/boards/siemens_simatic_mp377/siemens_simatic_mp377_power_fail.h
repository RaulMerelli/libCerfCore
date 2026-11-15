#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_power_input.h>

#include <atomic>
#include <string>

namespace cerfcore {

namespace siemens_simatic_mp377 {

/* siemens_mp377_v1040, PowerFail.dll: initialization 0x02951988-0x02951A24;
   IstPowerFail 0x02951670-0x02951900. */
class SiemensSimaticMp377PowerFail final : public Service, public MachinePowerInput {
public:
    using Service::Service;

    bool ShouldRegister() override;
    void OnReady() override;

    std::string ObjectId() const override { return "external_power"; }
    static constexpr char kObjectName[] = "External power";
    std::string ObjectName() const override { return kObjectName; }
    bool        PowerFailed() const override { return asserted_.load(std::memory_order_acquire); }
    void        SetPowerFailed(bool failed) override;

private:
    void Reset();
    void RefreshIrq();

    std::atomic<bool> asserted_{false};
};

} // namespace siemens_mp377

}
