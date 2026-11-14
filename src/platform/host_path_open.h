#pragma once

#include <string>
#include <sys/stat.h>
#include <utility>

namespace cerfcore {

namespace host_platform {

class HostDescriptor {
public:
    HostDescriptor() = default;
    HostDescriptor(int fd, void* document) : fd_(fd), document_(document) {}
    ~HostDescriptor() { Close(); }
    HostDescriptor(HostDescriptor&& other) noexcept
        : fd_(std::exchange(other.fd_, -1)), document_(std::exchange(other.document_, nullptr)) {}
    HostDescriptor& operator=(HostDescriptor&& other) noexcept {
        if (this != &other) {
            Close();
            fd_       = std::exchange(other.fd_, -1);
            document_ = std::exchange(other.document_, nullptr);
        }
        return *this;
    }
    HostDescriptor(const HostDescriptor&)            = delete;
    HostDescriptor& operator=(const HostDescriptor&) = delete;

    bool Valid() const { return fd_ >= 0; }
    int  Fd() const { return fd_; }
    bool Close();

private:
    int   fd_       = -1;
    void* document_ = nullptr;
};

inline const HostDescriptor& DescriptorOf(void* handle) { return *static_cast<const HostDescriptor*>(handle); }

HostDescriptor OpenHostPath(const std::string& path, int flags, mode_t mode);
HostDescriptor OpenHostDocumentDescriptor(const std::string& uri, const char* mode);
bool           StatHostPath(const std::string& path, struct stat& st);

}

}
