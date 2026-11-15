#pragma once

#include "machine_object.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cerfcore {

struct InsertArgument {
    std::string id;
    std::string name;
};

struct InsertCommand {
    std::string                 id;
    std::vector<InsertArgument> arguments;
};

struct InsertableType {
    std::string                id;
    std::string                name;
    std::vector<InsertCommand> commands;
};

struct NoCardFacts {
    bool operator==(const NoCardFacts&) const = default;
};

struct StorageCardFacts {
    std::string image_path;
    uint64_t    capacity_bytes = 0;

    bool operator==(const StorageCardFacts&) const = default;
};

struct NetworkCardFacts {
    uint8_t mac[6] = {};

    bool operator==(const NetworkCardFacts&) const = default;
};

struct SerialLinkFacts {
    std::string host_port;

    bool operator==(const SerialLinkFacts&) const = default;
};

using CardFacts = std::variant<NoCardFacts, StorageCardFacts, NetworkCardFacts, SerialLinkFacts>;

struct InsertedItem {
    std::string type_id;
    std::string name;
    CardFacts   facts;

    bool operator==(const InsertedItem&) const = default;
};

enum class InsertStatus : uint8_t { kAccepted, kUnknownType, kUnknownCommand, kInvalidArgument };

class MachineSocket : public MachineObject {
public:
    virtual std::optional<InsertedItem>  Inserted() const = 0;
    virtual std::vector<InsertableType> Accepted() const = 0;
    InsertStatus Insert(const std::string& type_id, const std::string& command_id, const std::string& argument);
    virtual void Eject() = 0;

protected:
    virtual void InsertAccepted(const std::string& type_id, const std::string& command_id,
                                const std::string& argument) = 0;

    void ReportInserted();
    void ReportEjected();
};

}
