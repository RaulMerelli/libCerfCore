#pragma once

#include <string>

namespace cerfcore {

class StateReader;
class StateWriter;

struct PcmciaCardIdentity {
    std::string id;
    std::string binding;

    void                      Save(StateWriter& w, const std::string& prefix) const;
    static PcmciaCardIdentity Restore(StateReader& r, const std::string& prefix);
};

}
