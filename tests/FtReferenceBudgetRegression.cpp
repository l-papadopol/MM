// SPDX-License-Identifier: GPL-3.0-or-later
#include "third_party/mshv_gpl/ft_reference/budget.h"
#include <iostream>
#include <thread>

bool referenceBudgetPeerStopping();

int main()
{
    using MshvReference::stopRequested;
    if (referenceBudgetPeerStopping()) return 1;
    stopRequested = [] { return true; };
    if (!referenceBudgetPeerStopping()) return 2;

    bool isolated = false;
    std::thread worker([&] {
        if (referenceBudgetPeerStopping()) return;
        stopRequested = [] { return true; };
        if (!referenceBudgetPeerStopping()) return;
        stopRequested = {};
        isolated = !referenceBudgetPeerStopping();
    });
    worker.join();
    if (!isolated || !referenceBudgetPeerStopping()) return 3;
    stopRequested = {};
    if (referenceBudgetPeerStopping()) return 4;
    std::cout << "Cross-translation-unit cancellation and thread isolation PASS\n";
}
