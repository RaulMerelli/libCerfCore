#pragma once

#include "host_path_open.h"
#include "host_shared_folder.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace host_platform {

class FdSharedFile : public SharedFsFile {
public:
    explicit FdSharedFile(HostDescriptor descriptor) : descriptor_(std::move(descriptor)) {}

    SharedFsStatus ReadAt(uint64_t position, void* buf, uint32_t size, uint32_t& done) override;
    SharedFsStatus WriteAt(uint64_t position, const void* buf, uint32_t size, uint32_t& done) override;
    SharedFsStatus TruncateAt(uint64_t position) override;

protected:
    int Fd() const { return descriptor_.Fd(); }

private:
    HostDescriptor descriptor_;
};

}

}
