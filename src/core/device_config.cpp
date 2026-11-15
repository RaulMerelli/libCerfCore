#include <cerfcore/core/device_config.h>
#include <cerfcore/core/cerf_emulator.h>
#include "config_loader.h"

namespace cerfcore {

REGISTER_SERVICE(DeviceConfig);

std::string DefaultStorageFileName(std::string_view storage_id) {
    return std::string(storage_id) + ".img";
}

void DeviceConfig::OnReady() {
    emu_.Get<ConfigLoader>().LoadInto(*this);
}

std::shared_ptr<const LiveDeviceSettings> DeviceConfig::Live() const {
    std::lock_guard<std::mutex> lk(live_mtx_);
    return live_;
}

uint64_t DeviceConfig::LiveGeneration() const {
    std::lock_guard<std::mutex> lk(live_mtx_);
    return live_generation_;
}

void DeviceConfig::PublishLive(LiveDeviceSettings settings) {
    auto published = std::make_shared<const LiveDeviceSettings>(std::move(settings));
    std::lock_guard<std::mutex> lk(live_mtx_);
    live_ = std::move(published);
    ++live_generation_;
}

bool DeviceConfig::PublishLiveAt(uint64_t generation, LiveDeviceSettings settings) {
    auto published = std::make_shared<const LiveDeviceSettings>(std::move(settings));
    std::lock_guard<std::mutex> lk(live_mtx_);
    if (live_generation_ != generation) return false;
    live_ = std::move(published);
    ++live_generation_;
    return true;
}

void DeviceConfig::EditLive(const std::function<void(LiveDeviceSettings&)>& edit) {
    std::lock_guard<std::mutex> lk(live_mtx_);
    LiveDeviceSettings next = *live_;
    edit(next);
    live_ = std::make_shared<const LiveDeviceSettings>(std::move(next));
    ++live_generation_;
}

}
