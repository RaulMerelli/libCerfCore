#pragma once

#include "machine_object.h"

#include <cstdint>
#include <string>

namespace cerfcore {

enum class StorageMedium : uint8_t { kNorFlash, kNandFlash, kHardDisk };

class MachineStorage : public MachineObject {
public:
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kStorage; }

    virtual StorageMedium Medium() const    = 0;
    virtual std::string   ImagePath() const = 0;
};

}
