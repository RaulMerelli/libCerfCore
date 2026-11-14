#pragma once

#include <cerfcore/core/service.h>

#include <array>
#include <cstddef>

namespace cerfcore {

inline constexpr size_t kHp600BezelZones = 4u;

struct Hp600BezelName {
    const char* id;
    const char* label;
};

class Hp600BezelButtons : public Service {
public:
    using Service::Service;

    virtual const std::array<Hp600BezelName, kHp600BezelZones>& Names() const = 0;
};

}
