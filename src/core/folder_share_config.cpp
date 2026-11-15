#include <cerfcore/core/folder_share_config.h>
#include <cerfcore/core/board_database.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config_refresh.h>
#include <cerfcore/core/log.h>
#include <cerfcore/core/utf16.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/platform/host_file.h>

namespace cerfcore {

REGISTER_SERVICE(FolderShareConfig);

void FolderShareConfig::OnReady() {
    ApplyFromConfig();
    emu_.Get<DeviceConfigRefresh>().RegisterListener(
        [this] { ApplyFromConfig(); });
    emu_.Get<MachineObjectTree>().Add(this);
}

void FolderShareConfig::ApplyFromConfig() {
    const auto         live = emu_.Get<DeviceConfig>().Live();
    const std::string& name = live->share_folder_mount_point.empty()
        ? emu_.Get<BoardDatabase>().GaSharedFolderMountPoint()
        : live->share_folder_mount_point;
    std::u16string mount = Utf8ToUtf16(name);

    const std::string& p = live->share_folder;
    if (p.empty()) {
        Set(false, std::string(), std::move(mount));
        return;
    }

    std::string root = IsAbsoluteHostPath(p) ? p : GetCerfDir() + p;
    LOG(GuestAdditions, "[FolderShare] host root '%s' exists=%d mount '%s'\n",
        root.c_str(), (int)host_platform::HostPathExists(root), name.c_str());

    Set(true, std::move(root), std::move(mount));
}

void FolderShareConfig::Set(bool enabled, std::string host_root, std::u16string mount_point) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const bool on = enabled && !host_root.empty();
        changed       = on != enabled_.load(std::memory_order_relaxed) || host_root != host_root_ ||
                  mount_point != mount_point_;
        host_root_   = std::move(host_root);
        mount_point_ = std::move(mount_point);
        enabled_.store(on, std::memory_order_relaxed);
    }
    generation_.fetch_add(1, std::memory_order_relaxed);
    if (changed) Changed();
}

}
