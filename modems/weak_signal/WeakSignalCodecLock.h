#ifndef WEAKSIGNALCODECLOCK_H
#define WEAKSIGNALCODECLOCK_H

#include <mutex>

namespace WeakSignalCodecLock {

// The assimilated FT4/FT8, Q65 and MSK144 coding primitives share process-wide
// message/hash state; Q65 also owns a process-wide QRA workspace. A single lock
// therefore owns every codec entry, including the short interval in which a
// previous-mode decode may still be completing after an operator mode change.
std::mutex &mutex();

} // namespace WeakSignalCodecLock

#endif // WEAKSIGNALCODECLOCK_H
