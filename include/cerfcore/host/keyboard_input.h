#pragma once

#include "../core/service.h"
#include "keyboard_map.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cerfcore {

class KeyboardInput : public Service {
public:
    using Service::Service;
    ~KeyboardInput() override = default;

    virtual void OnHostKey(uint8_t vk, bool key_up) = 0;

    virtual bool CanDeliverVk(uint8_t vk) const;
    virtual std::vector<SpecialKey> SpecialKeys() const;
    bool HasLetterKeys() const;

    static constexpr char kSourceName[] = "Stock keyboard";
    virtual std::string SourceId() const { return "keyboard"; }
    virtual std::string SourceName() const { return kSourceName; }
    virtual bool Stock() const { return true; }
    virtual int SourcePriority() const { return 0; }
    virtual bool SourceReady() const { return true; }
};

}
