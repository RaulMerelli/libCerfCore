#pragma once

#include "../external_device_clock.h"

#include <array>
#include <cstdint>

namespace cerfcore {

class CerfEmulator;
class StateReader;
class StateWriter;

class OhciHostBus {
public:
    virtual ~OhciHostBus() = default;

    virtual uint32_t OhciRead32(uint32_t addr)                  = 0;
    virtual void     OhciWrite32(uint32_t addr, uint32_t value) = 0;
    virtual void     OhciInterrupt(bool asserted)               = 0;
};

struct OhciProfile {
    const char* name;
    uint32_t    revision;
    uint32_t    interrupt_enable_writable;
    uint32_t    control_writable;
    uint32_t    rh_descriptor_a_reset;
    uint32_t    rh_descriptor_a_writable;
    uint32_t    ls_threshold_reset;
};

class OhciHostController {
public:
    static constexpr uint32_t kMaxPorts = 15u;

    OhciHostController(CerfEmulator& emu, const OhciProfile& profile, OhciHostBus& bus);

    void Attach();
    void HardwareReset();
    void SetClockEnabled(bool enabled);

    uint32_t Read(uint32_t off);
    void     Write(uint32_t off, uint32_t value);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);
    void PostRestore();

private:
    using RootHubContent = std::array<uint32_t, kMaxPorts + 1u>;

    uint32_t Ports() const;
    bool     IsPortRegister(uint32_t off) const;
    RootHubContent CaptureRootHub();
    void     WriteRegister(uint32_t off, uint32_t value);
    uint32_t Hcfs() const;
    bool     Counting() const;
    int64_t  NowBits();
    uint32_t CountingFmRemaining();
    void     Recount(bool was_counting);
    void     WriteControl(uint32_t value);
    void     WriteCommandStatus(uint32_t value);
    void     WriteRhStatus(uint32_t value);
    void     WritePort(uint32_t port, uint32_t value);
    void     SetPortPower(uint32_t port, bool on);
    void     SetGlobalPower(bool on);
    void     RaiseRootHubChange();
    void     SoftwareReset();
    void     ResetRootHubStatus();
    void     ArmFrame();
    void     OnFrame();
    void     WalkEds(uint32_t ed, bool periodic, uint32_t* current);
    void     UpdateInterrupt();

    [[noreturn]] void Reject(const char* what, uint32_t off, uint32_t value);

    CerfEmulator&               emu_;
    const OhciProfile&          profile_;
    OhciHostBus&                bus_;
    ExternalDeviceClock::Timer* frame_timer_ = nullptr;

    uint32_t control_           = 0;
    uint32_t command_status_    = 0;
    uint32_t interrupt_status_  = 0;
    uint32_t interrupt_enable_  = 0;
    uint32_t hcca_              = 0;
    uint32_t period_current_ed_ = 0;
    uint32_t control_head_ed_   = 0;
    uint32_t control_current_ed_ = 0;
    uint32_t bulk_head_ed_      = 0;
    uint32_t bulk_current_ed_   = 0;
    uint32_t done_head_         = 0;
    uint32_t fm_interval_       = 0;
    uint32_t fm_remaining_      = 0;
    uint32_t fm_number_         = 0;
    uint32_t hcca_frame_number_ = 0;
    uint32_t periodic_start_    = 0;
    uint32_t ls_threshold_      = 0;
    uint32_t rh_descriptor_a_   = 0;
    uint32_t rh_descriptor_b_   = 0;
    uint32_t rh_status_         = 0;
    uint32_t port_status_[kMaxPorts] = {};
    int64_t  frame_end_bits_    = 0;
    bool     interrupt_out_     = false;
    bool     clock_enabled_     = true;
};

}
