#include "common/common.h"
#include <mutex>
#include <sys/stat.h>

namespace xld {

std::string errno_string() {
    // strerror is not thread-safe, so guard it with a lock.
    static std::mutex mu;
    std::scoped_lock lock(mu);
    return strerror(errno);
}

} // namespace xld
