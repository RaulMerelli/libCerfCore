#pragma once

#include <cstdint>
#include <string>

namespace cerfcore {

enum class PointerKind { Absolute, Relative, Stylus };

class PointerSource {
public:
    virtual ~PointerSource() = default;

    virtual std::string    SourceId()         const = 0;
    virtual std::string    SourceName()       const = 0;
    virtual int            SourcePriority()   const = 0;
    virtual PointerKind    Kind()             const = 0;
    virtual bool           SourceReady()      const { return true; }
};

}
