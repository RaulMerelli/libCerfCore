#pragma once

#include <cerfcore/core/service.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cerfcore {

class GuestAdditionsBinaries : public Service {
public:
    using Service::Service;

    std::string BodyPath();
    std::string StubPath();

    void StampWindowBase(std::vector<uint8_t>& image);

private:
    std::string ArchDir();
};

}
