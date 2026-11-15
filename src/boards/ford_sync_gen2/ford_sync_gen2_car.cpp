#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_object.h>
#include <cerfcore/boards/board_context.h>
#include "ford_sync_gen2_ambient_temperature.h"
#include "ford_sync_gen2_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/machine_object_tree.h>

#include <string>

namespace cerfcore {

namespace {

class FordSyncGen2Car : public Service, public MachineObject {
public:
    using Service::Service;

    bool ShouldRegister() override {
        auto* board = emu_.TryGet<BoardContext>();
        return board && board->GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
    }

    void OnReady() override {
        auto& tree = emu_.Get<MachineObjectTree>();
        tree.Add(this);
        tree.Add(&emu_.Get<FordSyncGen2AmbientTemperature>(), this);
    }

    std::string       ObjectId() const override { return "car"; }
    static constexpr char kObjectName[] = "Car";
    std::string       ObjectName() const override { return kObjectName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kCar; }
};

REGISTER_SERVICE(FordSyncGen2Car);

}

}
