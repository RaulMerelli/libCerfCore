#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_socket.h>
#include "pcmcia_card.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cerfcore {

class PcmciaCardCatalog : public Service {
public:
    using Service::Service;

    struct CardInserter {
        std::function<void(std::unique_ptr<PcmciaCard>)>      insert;
        std::function<void(const char* reason, std::string subject)> fail;
    };

    void OnReady() override;

    std::vector<InsertableType> Accepted() const;
    void Insert(MachineObject& socket, const std::string& type_id, const std::string& command_id,
                const std::string& argument, CardInserter inserter);

    std::unique_ptr<PcmciaCard> Create(const std::string& id, const std::string& binding = "");
    std::unique_ptr<PcmciaCard> TryCreate(const std::string& id, const std::string& binding = "");

private:
    using CommandList = std::function<std::vector<InsertCommand>()>;
    using CommandRun  = std::function<void(MachineObject& socket, const std::string& command_id,
                                           const std::string& argument, CardInserter inserter)>;
    using CardFactory = std::function<std::unique_ptr<PcmciaCard>(const std::string& binding)>;

    struct Entry {
        std::string id;
        std::string display_name;
        CommandList commands;
        CommandRun  run;
        CardFactory create;
    };

    void         AddInsertOnly(std::string id, std::string display_name, CommandList commands, CardFactory create);
    const Entry* Find(const std::string& id) const;

    std::vector<Entry> entries_;
};

}
