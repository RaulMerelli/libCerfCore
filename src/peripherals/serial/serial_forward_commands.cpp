#include "serial_forward_commands.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../platform/host_serial_port_list.h"

namespace cerfcore {

REGISTER_SERVICE(SerialForwardCommands);

std::vector<InsertCommand> SerialForwardCommands::Commands() const {
    const std::vector<std::string> ports = host_platform::EnumerateSerialPorts();
    if (ports.empty()) return {};
    InsertCommand insert{"insert", {}};
    for (const std::string& port : ports) insert.arguments.push_back(InsertArgument{port, port});
    return {std::move(insert)};
}

}
