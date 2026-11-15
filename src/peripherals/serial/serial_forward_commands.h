#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_socket.h>

#include <vector>

namespace cerfcore {

class SerialForwardCommands : public Service {
public:
    using Service::Service;

    std::vector<InsertCommand> Commands() const;
};

}
