#include "host_path_open.h"

#include "host_local_path_open.h"

#include <unistd.h>

namespace cerfcore {

namespace host_platform {

bool HostDescriptor::Close() {
    const bool closed = fd_ < 0 || ::close(fd_) == 0;
    fd_               = -1;
    document_         = nullptr;
    return closed;
}

HostDescriptor OpenHostPath(const std::string& path, int flags, mode_t mode) {
    return HostDescriptor(OpenLocalPath(path, flags, mode), nullptr);
}

bool StatHostPath(const std::string& path, struct stat& st) { return StatLocalPath(path, st); }

}

}
