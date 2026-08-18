#include "WeakSignalCodecLock.h"

namespace WeakSignalCodecLock {

std::mutex &mutex()
{
    static std::mutex codecMutex;
    return codecMutex;
}

} // namespace WeakSignalCodecLock
