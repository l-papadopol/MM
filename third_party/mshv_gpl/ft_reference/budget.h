#pragma once
#include <functional>
namespace MshvReference {
// One definition in budget.cpp: a non-trivial inline TLS object emitted
// duplicate initialization functions with the Windows MinGW toolchain.
extern thread_local std::function<bool()> stopRequested;
inline bool stopping(){return stopRequested && stopRequested();}
}
