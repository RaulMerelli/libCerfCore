#pragma once

#include "../core/service.h"
#include "machine_object.h"
#include "source_selection.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace cerfcore {

class KeyboardInput;

class KeyboardRouter : public Service, public MachineObject, public SourceSelection<KeyboardInput> {
public:
    using Service::Service;

    void OnReady() override;

    std::string       ObjectId() const override { return "keyboard"; }
    static constexpr char kObjectName[] = "Keyboard";
    std::string       ObjectName() const override { return kObjectName; }
    MachineObjectKind ObjectKind() const override { return MachineObjectKind::kKeyboard; }
    void              SaveObjectState(StateWriter& w) const override;
    void              RestoreObjectState(StateReader& r) override;

    void OnHostKey(uint8_t vk, bool key_up);
    void OnStockKey(uint8_t vk, bool key_up);
    void SendCtrlAltDel();
    bool Held(uint8_t vk) const;

private:
    void           SelectionChanged() override { Changed(); }
    KeyboardInput* Stock() const;
    void           Route(uint8_t vk, bool key_up, bool stock);

    mutable std::mutex              mtx_;
    std::array<KeyboardInput*, 256> key_owner_{};
};

}
