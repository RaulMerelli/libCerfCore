#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>

namespace cerfcore {

class Intel21152Board : public Service {
public:
    using Service::Service;

    virtual uint32_t BridgeIdselLine() const = 0;
};

}
