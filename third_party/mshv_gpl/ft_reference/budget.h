#pragma once
#include <functional>
namespace MshvReference {
inline thread_local std::function<bool()> stopRequested;
inline bool stopping(){return stopRequested && stopRequested();}
}
