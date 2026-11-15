#pragma once

#include "service.h"
#include "device_config.h"
#include "cerf_emulator.h"
#include "../host/machine_object.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace cerfcore {

class FolderShareConfig : public Service, public MachineObject {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<DeviceConfig>().guest_additions;
    }

    void OnReady() override;

    std::string       ObjectId() const override { return "shared_folder"; }
    static constexpr char kObjectName[] = "Shared folder";
    std::string       ObjectName() const override { return kObjectName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kSharedFolder; }

    void ApplyFromConfig();

    bool           Enabled()    const { return enabled_.load(std::memory_order_relaxed); }
    uint32_t       Generation() const { return generation_.load(std::memory_order_relaxed); }
    std::string    HostRoot()   const { std::lock_guard<std::mutex> lk(mtx_); return host_root_; }
    std::u16string MountPoint() const { std::lock_guard<std::mutex> lk(mtx_); return mount_point_; }

private:
    void Set(bool enabled, std::string host_root, std::u16string mount_point);

    mutable std::mutex    mtx_;
    std::atomic<bool>     enabled_{false};
    std::atomic<uint32_t> generation_{0};
    std::string           host_root_;
    std::u16string        mount_point_;
};

}
