#pragma once

#include "../core/service.h"

#include <cstdint>

namespace cerfcore {

class GuestLiveResize : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    bool Supported() const;
    void Request(uint32_t surface_w, uint32_t surface_h);

private:
    uint32_t last_w_ = 0;
    uint32_t last_h_ = 0;
};

}
