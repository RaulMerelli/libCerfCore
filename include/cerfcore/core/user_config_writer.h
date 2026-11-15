#pragma once

#include "service.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace cerfcore {

class UserConfigWriter : public Service {
public:
    using Service::Service;

    bool WriteConfigurableScreenSize(uint32_t width, uint32_t height);
    void WriteHud(const std::string& hud);

private:
    std::mutex mtx_;
};

}
