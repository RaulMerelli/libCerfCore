#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include <cerfcore/platform/host_path_name.h>
#include "ford_sync_gen2_family_id.h"
#include "../../socs/imx51/imx51_usboh3.h"
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/host/machine_requests.h>
#include <cerfcore/host/machine_socket.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/emulation_freeze.h>
#include "ford_sync_gen2_media_hub.h"
#include "ford_sync_gen2_media_hub_sd_reader.h"

#include <algorithm>
#include <atomic>
#include <optional>

namespace cerfcore {

namespace {

constexpr char kInsertBundled[] = "insert_bundled";
constexpr char kChooseImage[]   = "choose_image";
constexpr char kSdName[]        = "Media Hub SD";
constexpr char kUsbName[]       = "Media Hub USB";
constexpr char kSdCardName[]    = "SD card";
constexpr char kUsbDiskName[]   = "USB disk";

class MediaSocket final : public MachineSocket {
public:
    MediaSocket(CerfEmulator& emu, int slot) : emu_(emu), slot_(slot) {}

    std::string ObjectId() const override { return Sd() ? "media_sd" : "media_usb"; }
    std::string ObjectName() const override { return Sd() ? kSdName : kUsbName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kMediaSocket; }

    std::optional<InsertedItem> Inserted() const override {
        auto locked = Controller().LockHostPort();
        auto* device = Current();
        if (!device) return std::nullopt;
        return InsertedItem{TypeId(), device->ImageName(),
                            StorageCardFacts{device->ImagePath(), device->CapacityBytes()}};
    }

    std::vector<InsertableType> Accepted() const override {
        InsertableType type{TypeId(), Sd() ? kSdCardName : kUsbDiskName, {}};
        InsertCommand  bundled{kInsertBundled, {}};
        for (const auto& media : Entries()) bundled.arguments.push_back(InsertArgument{media.file, media.name});
        if (!bundled.arguments.empty()) type.commands.push_back(std::move(bundled));
        type.commands.push_back(InsertCommand{kChooseImage, {}});
        return {std::move(type)};
    }

protected:
    void InsertAccepted(const std::string&, const std::string& command_id, const std::string& argument) override {
        if (command_id == kInsertBundled) {
            const auto& entries = Entries();
            Apply(&*std::find_if(entries.begin(), entries.end(),
                                 [&](const BundledUsbMedia& m) { return m.file == argument; }));
            return;
        }
        MachineRequest request;
        request.kind       = MachineRequestKind::kPath;
        request.id         = "media_image";
        request.object     = this;
        request.extensions = {"img", "bin", "ima"};
        emu_.Get<MachineRequests>().Ask(std::move(request), [this](const MachineReply& reply) {
            if (reply.cancelled) return;
            BundledUsbMedia media;
            media.file = reply.path;
            media.name = host_platform::HostPathDisplayName(reply.path);
            Apply(&media);
        });
    }

public:

    void Eject() override { Apply(nullptr); }
    void RestoreObjectState(StateReader&) override { ++generation_; }

    void InsertOnLaunch() {
        for (const auto& media : Entries()) {
            if (!media.insert_on_launch) continue;
            auto device = Open(media);
            if (!device) {
                LOG(Caution, "Media Hub: cannot open launch image '%s'\n", media.file.c_str());
                continue;
            }
            Hub().SetMedia(slot_, std::move(device));
        }
    }

private:
    bool Sd() const { return slot_ == FordSyncGen2MediaHub::kSdPort; }
    Imx51Usboh3& Controller() const { return emu_.Get<Imx51Usboh3>(); }
    const char* TypeId() const { return Sd() ? "sd_card" : "usb_disk"; }
    const std::vector<BundledUsbMedia>& Entries() const {
        const auto& cfg = emu_.Get<DeviceConfig>();
        return Sd() ? cfg.bundled_sd_cards : cfg.bundled_usb_disks;
    }
    FordSyncGen2MediaHub& Hub() const {
        auto* device = Controller().OtgHostRootPort().Device();
        if (!device || device->StateKind() != FordSyncGen2MediaHub::kStateKind)
            emu_.Get<Fatal>().Die("Media Hub: the USB root port does not hold the Media Hub");
        return *static_cast<FordSyncGen2MediaHub*>(device);
    }
    UsbMassStorageDevice* Current() const {
        return static_cast<UsbMassStorageDevice*>(Hub().Port(slot_).Device());
    }
    std::unique_ptr<UsbMassStorageDevice> Open(const BundledUsbMedia& media) {
        std::unique_ptr<UsbMassStorageDevice> device;
        if (Sd()) device = std::make_unique<FordSyncGen2MediaHubSdReader>(media.cid);
        else device = std::make_unique<UsbMassStorageDevice>();
        const auto& cfg = emu_.Get<DeviceConfig>();
        if (!device->OpenImage(ResolveDeviceFile(cfg.device_name, media.file), media.name)) return {};
        return device;
    }
    void Apply(const BundledUsbMedia* media) {
        const std::string path = media
            ? ResolveDeviceFile(emu_.Get<DeviceConfig>().device_name, media->file) : std::string();
        int                                   removed = 0;
        std::unique_ptr<UsbMassStorageDevice> device;
        for (;;) {
            uint64_t generation = 0;
            {
                auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
                auto locked = Controller().LockHostPort();
                generation    = generation_;
                auto* current = Current();
                const bool same_file = media && current && current->ImagePath() == path;
                if (same_file && (!Sd() || static_cast<FordSyncGen2MediaHubSdReader*>(current)->Cid() == media->cid))
                    break;
                if (!media || same_file) {
                    if (current) ++removed;
                    Hub().SetMedia(slot_, {});
                    generation = ++generation_;
                }
            }
            if (!media) break;
            if (!device) device = Open(*media);
            if (!device) {
                LOG(Caution, "Media Hub: cannot open media image '%s'\n", media->file.c_str());
                for (; removed > 0; --removed) ReportEjected();
                Failed("image_open_failed", 0, media->file);
                return;
            }
            {
                auto frozen = emu_.Get<EmulationFreeze>().WorkerSection();
                auto locked = Controller().LockHostPort();
                if (generation_ != generation) continue;
                if (Current()) ++removed;
                Hub().SetMedia(slot_, std::move(device));
                ++generation_;
            }
            for (; removed > 0; --removed) ReportEjected();
            ReportInserted();
            return;
        }
        for (; removed > 0; --removed) ReportEjected();
    }

    CerfEmulator&         emu_;
    int                   slot_;
    std::atomic<uint64_t> generation_{0};
};

class FordSyncGen2MediaSlots : public Service {
public:
    using Service::Service;
    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
    }

    void OnReady() override {
        auto& controller = emu_.Get<Imx51Usboh3>();
        auto  locked     = controller.LockHostPort();
        auto& root       = controller.OtgHostRootPort();
        root.SetRestoreFactory([](uint32_t kind) -> std::unique_ptr<UsbDevice> {
            return kind == FordSyncGen2MediaHub::kStateKind ? std::make_unique<FordSyncGen2MediaHub>() : nullptr;
        });
        root.SetRequiresDevice();
        root.Attach(std::make_unique<FordSyncGen2MediaHub>());
        for (int i = 0; i < FordSyncGen2MediaHub::kPortCount; ++i) {
            sockets_[i] = std::make_unique<MediaSocket>(emu_, i);
            sockets_[i]->InsertOnLaunch();
            emu_.Get<MachineObjectTree>().Add(sockets_[i].get());
        }
    }

private:
    std::unique_ptr<MediaSocket> sockets_[FordSyncGen2MediaHub::kPortCount];
};

}
REGISTER_SERVICE(FordSyncGen2MediaSlots);

}
