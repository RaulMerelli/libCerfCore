#include "ohci_host_controller.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kHcRevision         = 0x00u;
constexpr uint32_t kHcControl          = 0x04u;
constexpr uint32_t kHcCommandStatus    = 0x08u;
constexpr uint32_t kHcInterruptStatus  = 0x0Cu;
constexpr uint32_t kHcInterruptEnable  = 0x10u;
constexpr uint32_t kHcInterruptDisable = 0x14u;
constexpr uint32_t kHcHcca             = 0x18u;
constexpr uint32_t kHcPeriodCurrentEd  = 0x1Cu;
constexpr uint32_t kHcControlHeadEd    = 0x20u;
constexpr uint32_t kHcControlCurrentEd = 0x24u;
constexpr uint32_t kHcBulkHeadEd       = 0x28u;
constexpr uint32_t kHcBulkCurrentEd    = 0x2Cu;
constexpr uint32_t kHcDoneHead         = 0x30u;
constexpr uint32_t kHcFmInterval       = 0x34u;
constexpr uint32_t kHcFmRemaining      = 0x38u;
constexpr uint32_t kHcFmNumber         = 0x3Cu;
constexpr uint32_t kHcPeriodicStart    = 0x40u;
constexpr uint32_t kHcLsThreshold      = 0x44u;
constexpr uint32_t kHcRhDescriptorA    = 0x48u;
constexpr uint32_t kHcRhDescriptorB    = 0x4Cu;
constexpr uint32_t kHcRhStatus         = 0x50u;
constexpr uint32_t kHcRhPortStatus1    = 0x54u;
constexpr uint32_t kHceControl         = 0x100u;

constexpr uint32_t kRevisionLegacy = 0x100u;
constexpr uint32_t kHceIrqActive   = 0x0C0u;

constexpr uint32_t kCtlPle       = 0x004u;
constexpr uint32_t kCtlIe        = 0x008u;
constexpr uint32_t kCtlCle       = 0x010u;
constexpr uint32_t kCtlBle       = 0x020u;
constexpr uint32_t kCtlHcfsShift = 6u;
constexpr uint32_t kCtlHcfsMask  = 0x0C0u;
constexpr uint32_t kCtlIr        = 0x100u;
constexpr uint32_t kCtlRwc       = 0x200u;

constexpr uint32_t kUsbReset       = 0u;
constexpr uint32_t kUsbOperational = 2u;
constexpr uint32_t kUsbSuspend     = 3u;

constexpr uint32_t kCmdHcr = 0x1u;
constexpr uint32_t kCmdClf = 0x2u;
constexpr uint32_t kCmdBlf = 0x4u;
constexpr uint32_t kCmdOcr = 0x8u;

constexpr uint32_t kIntSf     = 0x00000004u;
constexpr uint32_t kIntFno    = 0x00000020u;
constexpr uint32_t kIntRhsc   = 0x00000040u;
constexpr uint32_t kIntEvents = 0x0000007Fu;
constexpr uint32_t kIntOc     = 0x40000000u;
constexpr uint32_t kIntMie    = 0x80000000u;
constexpr uint32_t kIntBits   = kIntMie | kIntOc | kIntEvents;

constexpr uint32_t kFiMask            = 0x00003FFFu;
constexpr uint32_t kFmIntervalWritable = 0xFFFF3FFFu;
constexpr uint32_t kFit               = 0x80000000u;
constexpr uint32_t kFrt               = 0x80000000u;
constexpr uint32_t kFmIntervalReset   = 0x00002EDFu;
constexpr uint32_t kFmNumberMask      = 0x0000FFFFu;
constexpr uint32_t kFmNumberMsb       = 0x00008000u;
constexpr uint32_t kPeriodicStartMask = 0x00003FFFu;
constexpr uint32_t kLsThresholdMask   = 0x00000FFFu;

constexpr uint32_t kHccaAlignMask     = 0xFFFFFF00u;
constexpr uint32_t kHccaFrameNumber   = 0x80u;
constexpr uint32_t kHccaInterruptSlots = 32u;
constexpr uint32_t kEdPtrMask         = 0xFFFFFFF0u;
constexpr uint32_t kEdSkip            = 1u << 14;
constexpr uint32_t kEdIso             = 1u << 15;
constexpr uint32_t kEdHalted          = 0x1u;

constexpr uint32_t kNdpMask = 0x000000FFu;
constexpr uint32_t kPsm     = 0x00000100u;
constexpr uint32_t kNps     = 0x00000200u;

constexpr uint32_t kRhClearGlobalPower = 0x00000001u;
constexpr uint32_t kRhDrwe             = 0x00008000u;
constexpr uint32_t kRhSetGlobalPower   = 0x00010000u;
constexpr uint32_t kRhOcic             = 0x00020000u;
constexpr uint32_t kRhCrwe             = 0x80000000u;
constexpr uint32_t kRhWritable = kRhClearGlobalPower | kRhDrwe | kRhSetGlobalPower | kRhOcic | kRhCrwe;

constexpr uint32_t kPortSetEnable     = 0x00000002u;
constexpr uint32_t kPortSetSuspend    = 0x00000004u;
constexpr uint32_t kPortSetReset      = 0x00000010u;
constexpr uint32_t kPortPps           = 0x00000100u;
constexpr uint32_t kPortClearPower    = 0x00000200u;
constexpr uint32_t kPortCsc           = 0x00010000u;
constexpr uint32_t kPortChangeMask    = 0x001F0000u;
constexpr uint32_t kPortWritable      = 0x001F031Fu;

constexpr int64_t kBitRateMhz = 12;
constexpr int64_t kNsPerUs    = 1000;

}

OhciHostController::OhciHostController(CerfEmulator& emu, const OhciProfile& profile, OhciHostBus& bus)
    : emu_(emu), profile_(profile), bus_(bus) {}

void OhciHostController::Attach() {
    frame_timer_ = &emu_.Get<ExternalDeviceClock>().AddTimer([this] { OnFrame(); });
    HardwareReset();
}

void OhciHostController::Reject(const char* what, uint32_t off, uint32_t value) {
    emu_.Get<Fatal>().Die("%s OHCI: %s off=0x%08X val=0x%08X control=0x%08X", profile_.name, what, off, value,
                          control_);
}

uint32_t OhciHostController::Ports() const { return profile_.rh_descriptor_a_reset & kNdpMask; }

uint32_t OhciHostController::Hcfs() const { return (control_ & kCtlHcfsMask) >> kCtlHcfsShift; }

bool OhciHostController::Counting() const { return Hcfs() == kUsbOperational && clock_enabled_; }

int64_t OhciHostController::NowBits() {
    return emu_.Get<ExternalDeviceClock>().NowNs() * kBitRateMhz / kNsPerUs;
}

uint32_t OhciHostController::CountingFmRemaining() {
    const int64_t fi   = fm_interval_ & kFiMask;
    const int64_t left = frame_end_bits_ - NowBits() - 1;
    const int64_t fr   = left < 0 ? 0 : (left > fi ? fi : left);
    return (fm_remaining_ & kFrt) | static_cast<uint32_t>(fr);
}

void OhciHostController::Recount(bool was_counting) {
    const bool now = Counting();
    if (now == was_counting) return;
    if (!now) {
        emu_.Get<ExternalDeviceClock>().Disarm(*frame_timer_);
        return;
    }
    frame_end_bits_ = NowBits() + static_cast<int64_t>(fm_remaining_ & kFiMask) + 1;
    ArmFrame();
}

void OhciHostController::SetClockEnabled(bool enabled) {
    const bool was = Counting();
    if (was) fm_remaining_ = CountingFmRemaining();
    clock_enabled_ = enabled;
    Recount(was);
}

void OhciHostController::SoftwareReset() {
    emu_.Get<ExternalDeviceClock>().Disarm(*frame_timer_);
    control_            = (control_ & (kCtlIr | kCtlRwc)) | (kUsbSuspend << kCtlHcfsShift);
    command_status_     = 0u;
    interrupt_status_   = 0u;
    interrupt_enable_   = 0u;
    hcca_               = 0u;
    period_current_ed_  = 0u;
    control_head_ed_    = 0u;
    control_current_ed_ = 0u;
    bulk_head_ed_       = 0u;
    bulk_current_ed_    = 0u;
    done_head_          = 0u;
    fm_interval_        = kFmIntervalReset;
    fm_remaining_       = 0u;
    fm_number_          = 0u;
    hcca_frame_number_  = 0u;
    periodic_start_     = 0u;
    ls_threshold_       = profile_.ls_threshold_reset;
    UpdateInterrupt();
}

void OhciHostController::HardwareReset() {
    SoftwareReset();
    control_         = 0u;
    rh_descriptor_a_ = profile_.rh_descriptor_a_reset;
    rh_descriptor_b_ = 0u;
    ResetRootHubStatus();
}

void OhciHostController::ResetRootHubStatus() {
    rh_status_ = 0u;
    for (uint32_t& p : port_status_) p = 0u;
}

uint32_t OhciHostController::Read(uint32_t off) {
    switch (off) {
        case kHcRevision:         return profile_.revision;
        case kHcControl:          return control_;
        case kHcCommandStatus:    return command_status_;
        case kHcInterruptStatus:  return interrupt_status_;
        case kHcInterruptEnable:
        case kHcInterruptDisable: return interrupt_enable_;
        case kHcHcca:             return hcca_;
        case kHcPeriodCurrentEd:  return period_current_ed_;
        case kHcControlHeadEd:    return control_head_ed_;
        case kHcControlCurrentEd: return control_current_ed_;
        case kHcBulkHeadEd:       return bulk_head_ed_;
        case kHcBulkCurrentEd:    return bulk_current_ed_;
        case kHcDoneHead:         return done_head_;
        case kHcFmInterval:       return fm_interval_;
        case kHcFmNumber:         return fm_number_;
        case kHcPeriodicStart:    return periodic_start_;
        case kHcLsThreshold:      return ls_threshold_;
        case kHcRhDescriptorA:    return (rh_descriptor_a_ & ~kNdpMask) | Ports();
        case kHcRhDescriptorB:    return rh_descriptor_b_;
        case kHcRhStatus:         return rh_status_;
        case kHcFmRemaining:      return Counting() ? CountingFmRemaining() : fm_remaining_;
        default: break;
    }
    if (IsPortRegister(off)) {
        const uint32_t p = port_status_[(off - kHcRhPortStatus1) / 4u];
        return (rh_descriptor_a_ & kNps) != 0u ? (p | kPortPps) : p;
    }
    Reject("register read", off, 0u);
}

bool OhciHostController::IsPortRegister(uint32_t off) const {
    return off >= kHcRhPortStatus1 && (off - kHcRhPortStatus1) / 4u < Ports() && (off & 3u) == 0u;
}

OhciHostController::RootHubContent OhciHostController::CaptureRootHub() {
    RootHubContent content{};
    content[0] = Read(kHcRhStatus);
    for (uint32_t i = 0; i < Ports(); ++i) content[1u + i] = Read(kHcRhPortStatus1 + 4u * i);
    return content;
}

void OhciHostController::Write(uint32_t off, uint32_t value) {
    const bool root_hub = off == kHcControl || off == kHcRhStatus || off == kHcRhDescriptorA || IsPortRegister(off);
    if (!root_hub) {
        WriteRegister(off, value);
        return;
    }
    const RootHubContent before = CaptureRootHub();
    WriteRegister(off, value);
    if (CaptureRootHub() != before) RaiseRootHubChange();
}

void OhciHostController::WriteRegister(uint32_t off, uint32_t value) {
    switch (off) {
        case kHcRevision:
        case kHcPeriodCurrentEd:
        case kHcDoneHead:
        case kHcFmRemaining:
        case kHcFmNumber:         return;
        case kHcControl:          WriteControl(value); return;
        case kHcCommandStatus:    WriteCommandStatus(value); return;
        case kHcInterruptStatus:  interrupt_status_ &= ~(value & (kIntOc | kIntEvents)); UpdateInterrupt(); return;
        case kHcInterruptEnable:
            interrupt_enable_ |= value & kIntBits & profile_.interrupt_enable_writable;
            UpdateInterrupt();
            return;
        case kHcInterruptDisable:
            interrupt_enable_ &= ~(value & kIntBits & profile_.interrupt_enable_writable);
            UpdateInterrupt();
            return;
        case kHcHcca:             hcca_ = value & kHccaAlignMask; return;
        case kHcControlHeadEd:    control_head_ed_ = value & kEdPtrMask; return;
        case kHcControlCurrentEd: control_current_ed_ = value & kEdPtrMask; return;
        case kHcBulkHeadEd:       bulk_head_ed_ = value & kEdPtrMask; return;
        case kHcBulkCurrentEd:    bulk_current_ed_ = value & kEdPtrMask; return;
        case kHcFmInterval:       fm_interval_ = value & kFmIntervalWritable; return;
        case kHcPeriodicStart:    periodic_start_ = value & kPeriodicStartMask; return;
        case kHcLsThreshold:      ls_threshold_ = value & kLsThresholdMask; return;
        case kHcRhStatus:         WriteRhStatus(value); return;
        case kHcRhDescriptorA:
            if (((value ^ Read(kHcRhDescriptorA)) & ~profile_.rh_descriptor_a_writable) != 0u)
                Reject("root hub descriptor A setting that is not modeled", off, value);
            rh_descriptor_a_ = (rh_descriptor_a_ & ~profile_.rh_descriptor_a_writable) |
                               (value & profile_.rh_descriptor_a_writable);
            return;
        case kHcRhDescriptorB: {
            const uint32_t ports = ((1u << Ports()) - 1u) << 1;
            if ((value & ports) != 0u) Reject("non-removable device on a port", off, value);
            rh_descriptor_b_ = value & (ports << 16);
            return;
        }
        case kHceControl:
            if ((profile_.revision & kRevisionLegacy) == 0u || (value & ~kHceIrqActive) != 0u)
                Reject("legacy emulation setting that is not modeled", off, value);
            return;
        default: break;
    }
    if (IsPortRegister(off)) {
        WritePort((off - kHcRhPortStatus1) / 4u, value);
        return;
    }
    Reject("register write", off, value);
}

void OhciHostController::WriteControl(uint32_t value) {
    if ((value & kCtlIr) != 0u) Reject("interrupt routing to SMI", kHcControl, value);
    const bool     was = Counting();
    const uint32_t old = Hcfs();
    if (was) fm_remaining_ = CountingFmRemaining();
    control_           = value & profile_.control_writable;
    const uint32_t now = Hcfs();
    if (now != old && now == kUsbOperational) {
        fm_remaining_ = (fm_remaining_ & kFrt) | (fm_interval_ & kFiMask);
        fm_number_    = (fm_number_ + 1u) & kFmNumberMask;
    }
    if (now != old && now == kUsbReset) ResetRootHubStatus();
    Recount(was);
}

void OhciHostController::WriteCommandStatus(uint32_t value) {
    if ((value & kCmdOcr) != 0u) Reject("ownership change request", kHcCommandStatus, value);
    if ((value & kCmdHcr) != 0u) {
        SoftwareReset();
        return;
    }
    command_status_ |= value & (kCmdClf | kCmdBlf);
}

void OhciHostController::WriteRhStatus(uint32_t value) {
    if ((value & ~kRhWritable) != 0u) Reject("root hub status reserved bits", kHcRhStatus, value);
    if ((value & kRhClearGlobalPower) != 0u) SetGlobalPower(false);
    if ((value & kRhSetGlobalPower) != 0u) SetGlobalPower(true);
    if ((value & kRhDrwe) != 0u) rh_status_ |= kRhDrwe;
    if ((value & kRhCrwe) != 0u) rh_status_ &= ~kRhDrwe;
    rh_status_ &= ~(value & kRhOcic);
}

void OhciHostController::SetGlobalPower(bool on) {
    if ((rh_descriptor_a_ & kNps) != 0u) return;
    const bool per_port = (rh_descriptor_a_ & kPsm) != 0u;
    for (uint32_t i = 0; i < Ports(); ++i) {
        if (per_port && (rh_descriptor_b_ & (1u << (17u + i))) != 0u) continue;
        if (on) port_status_[i] |= kPortPps;
        else    port_status_[i] &= ~kPortPps;
    }
}

void OhciHostController::SetPortPower(uint32_t port, bool on) {
    if ((rh_descriptor_a_ & kNps) != 0u || (rh_descriptor_a_ & kPsm) == 0u) return;
    if ((rh_descriptor_b_ & (1u << (17u + port))) == 0u) return;
    if (on) port_status_[port] |= kPortPps;
    else    port_status_[port] &= ~kPortPps;
}

void OhciHostController::WritePort(uint32_t port, uint32_t value) {
    if ((value & ~kPortWritable) != 0u) Reject("port status reserved bits", kHcRhPortStatus1 + 4u * port, value);
    uint32_t& p = port_status_[port];
    if ((value & (kPortSetEnable | kPortSetSuspend | kPortSetReset)) != 0u) p |= kPortCsc;
    if ((value & kPortPps) != 0u) SetPortPower(port, true);
    if ((value & kPortClearPower) != 0u) SetPortPower(port, false);
    p &= ~(value & kPortChangeMask);
}

void OhciHostController::RaiseRootHubChange() {
    interrupt_status_ |= kIntRhsc;
    UpdateInterrupt();
}

void OhciHostController::ArmFrame() {
    const int64_t due_ns = (frame_end_bits_ * kNsPerUs + kBitRateMhz - 1) / kBitRateMhz;
    auto&         clock  = emu_.Get<ExternalDeviceClock>();
    const int64_t delay  = due_ns - clock.NowNs();
    clock.Arm(*frame_timer_, delay < 0 ? 0 : delay);
}

void OhciHostController::OnFrame() {
    const uint32_t fi = fm_interval_ & kFiMask;
    frame_end_bits_ += fi + 1;
    fm_remaining_          = (fm_interval_ & kFit) != 0u ? kFrt : 0u;
    fm_number_             = (fm_number_ + 1u) & kFmNumberMask;
    bus_.OhciWrite32(hcca_ + kHccaFrameNumber, fm_number_);
    interrupt_status_ |= kIntSf;
    if (((hcca_frame_number_ ^ fm_number_) & kFmNumberMsb) != 0u) interrupt_status_ |= kIntFno;
    hcca_frame_number_ = fm_number_;
    if ((control_ & kCtlPle) != 0u) {
        const uint32_t slot = hcca_ + 4u * (fm_number_ % kHccaInterruptSlots);
        WalkEds(bus_.OhciRead32(slot) & kEdPtrMask, true, &period_current_ed_);
    }
    if ((control_ & kCtlCle) != 0u && (command_status_ & kCmdClf) != 0u) {
        command_status_ &= ~kCmdClf;
        WalkEds(control_current_ed_ != 0u ? control_current_ed_ : control_head_ed_, false, &control_current_ed_);
    }
    if ((control_ & kCtlBle) != 0u && (command_status_ & kCmdBlf) != 0u) {
        command_status_ &= ~kCmdBlf;
        WalkEds(bulk_current_ed_ != 0u ? bulk_current_ed_ : bulk_head_ed_, false, &bulk_current_ed_);
    }
    UpdateInterrupt();
    ArmFrame();
}

void OhciHostController::WalkEds(uint32_t ed, bool periodic, uint32_t* current) {
    const uint32_t limit = (fm_interval_ & kFiMask) + 1u;
    for (uint32_t n = 0; ed != 0u; ++n) {
        if (n > limit) Reject("endpoint descriptor list longer than one frame", ed, n);
        *current             = ed;
        const uint32_t dword0 = bus_.OhciRead32(ed);
        const uint32_t tail   = bus_.OhciRead32(ed + 4u);
        const uint32_t head   = bus_.OhciRead32(ed + 8u);
        const uint32_t next   = bus_.OhciRead32(ed + 12u);
        if (periodic && (dword0 & kEdIso) != 0u && (control_ & kCtlIe) == 0u) break;
        if ((dword0 & kEdSkip) == 0u && (head & kEdHalted) == 0u && (head & kEdPtrMask) != (tail & kEdPtrMask))
            Reject("transfer descriptor queued with no USB device model", ed, head);
        ed = next & kEdPtrMask;
    }
    *current = 0u;
}

void OhciHostController::UpdateInterrupt() {
    const bool level =
        (interrupt_enable_ & kIntMie) != 0u && (interrupt_status_ & interrupt_enable_ & kIntEvents) != 0u;
    if (level == interrupt_out_) return;
    interrupt_out_ = level;
    bus_.OhciInterrupt(level);
}

void OhciHostController::SaveState(StateWriter& w) {
    const int64_t left = Counting() ? frame_end_bits_ - NowBits() : 0;
    w.Write("ohci_control", control_);
    w.Write("ohci_command_status", command_status_);
    w.Write("ohci_interrupt_status", interrupt_status_);
    w.Write("ohci_interrupt_enable", interrupt_enable_);
    w.Write("ohci_hcca", hcca_);
    w.Write("ohci_period_current_ed", period_current_ed_);
    w.Write("ohci_control_head_ed", control_head_ed_);
    w.Write("ohci_control_current_ed", control_current_ed_);
    w.Write("ohci_bulk_head_ed", bulk_head_ed_);
    w.Write("ohci_bulk_current_ed", bulk_current_ed_);
    w.Write("ohci_done_head", done_head_);
    w.Write("ohci_fm_interval", fm_interval_);
    w.Write("ohci_fm_remaining", fm_remaining_);
    w.Write("ohci_fm_number", fm_number_);
    w.Write("ohci_hcca_frame_number", hcca_frame_number_);
    w.Write("ohci_periodic_start", periodic_start_);
    w.Write("ohci_ls_threshold", ls_threshold_);
    w.Write("ohci_rh_descriptor_a", rh_descriptor_a_);
    w.Write("ohci_rh_descriptor_b", rh_descriptor_b_);
    w.Write("ohci_rh_status", rh_status_);
    w.WriteBytes("ohci_port_status", port_status_, sizeof(port_status_));
    w.Write("ohci_frame_bits_left", left);
    w.Write("ohci_interrupt_out", interrupt_out_);
    w.Write("ohci_clock_enabled", clock_enabled_);
    emu_.Get<ExternalDeviceClock>().SaveTimer(*frame_timer_, w, "ohci_frame_");
}

void OhciHostController::RestoreState(StateReader& r) {
    int64_t left = 0;
    r.Read("ohci_control", control_);
    r.Read("ohci_command_status", command_status_);
    r.Read("ohci_interrupt_status", interrupt_status_);
    r.Read("ohci_interrupt_enable", interrupt_enable_);
    r.Read("ohci_hcca", hcca_);
    r.Read("ohci_period_current_ed", period_current_ed_);
    r.Read("ohci_control_head_ed", control_head_ed_);
    r.Read("ohci_control_current_ed", control_current_ed_);
    r.Read("ohci_bulk_head_ed", bulk_head_ed_);
    r.Read("ohci_bulk_current_ed", bulk_current_ed_);
    r.Read("ohci_done_head", done_head_);
    r.Read("ohci_fm_interval", fm_interval_);
    r.Read("ohci_fm_remaining", fm_remaining_);
    r.Read("ohci_fm_number", fm_number_);
    r.Read("ohci_hcca_frame_number", hcca_frame_number_);
    r.Read("ohci_periodic_start", periodic_start_);
    r.Read("ohci_ls_threshold", ls_threshold_);
    r.Read("ohci_rh_descriptor_a", rh_descriptor_a_);
    r.Read("ohci_rh_descriptor_b", rh_descriptor_b_);
    r.Read("ohci_rh_status", rh_status_);
    r.ReadBytes("ohci_port_status", port_status_, sizeof(port_status_));
    r.Read("ohci_frame_bits_left", left);
    r.Read("ohci_interrupt_out", interrupt_out_);
    r.Read("ohci_clock_enabled", clock_enabled_);
    emu_.Get<ExternalDeviceClock>().RestoreTimer(*frame_timer_, r, "ohci_frame_");
    frame_end_bits_ = NowBits() + left;
}

void OhciHostController::PostRestore() {
    emu_.Get<ExternalDeviceClock>().ResumeTimer(*frame_timer_);
    bus_.OhciInterrupt(interrupt_out_);
}

}
