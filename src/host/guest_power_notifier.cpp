#include <cerfcore/host/guest_power_notifier.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/hw_screen.h>
#include <cerfcore/host/machine_object_tree.h>

namespace cerfcore {

REGISTER_SERVICE(GuestPowerNotifier);

void GuestPowerNotifier::Banner(const char* line) {
    LOG(Caution, "%s\n", line);
    auto& uart = emu_.Get<HwScreen>();
    uart.AddLine("");
    uart.AddLine(line);
}

void GuestPowerNotifier::RegisterRebootListener(std::function<void()> fn) {
    reboot_listeners_.Add(std::move(fn));
}

void GuestPowerNotifier::NotifyPowerDown() {
    Banner("!! CERF: CPU Sleep !!");
}

void GuestPowerNotifier::NotifyReboot() {
    Banner("!! CERF: CPU reset !!");
    reboot_listeners_.Call();
}

void GuestPowerNotifier::RegisterResetReleasedListener(std::function<void()> fn) {
    reset_released_listeners_.Add(std::move(fn));
}

void GuestPowerNotifier::NotifyResetReleased() {
    emu_.Get<MachineObjectTree>().RefreshAll();
    reset_released_listeners_.Call();
}

void GuestPowerNotifier::NotifyResume(ResumeSource src) {
    Banner(src == ResumeSource::Hardware ? "!! CERF: CPU Resume by hardware !!"
                                         : "!! CERF: CPU Resume by user !!");
}

void GuestPowerNotifier::NotifyHardReset() {
    Banner("!! CERF: CPU+RAM hard reset !!");
}

}
