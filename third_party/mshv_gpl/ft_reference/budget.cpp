// SPDX-License-Identifier: GPL-3.0-or-later
#include "budget.h"

namespace MshvReference {
thread_local std::function<bool()> stopRequested;
}
