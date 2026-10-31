// SPDX-License-Identifier: GPL-3.0-or-later
#include "../modems/ft8/FtTimingPolicy.h"
using namespace MadModemFtTiming;
static_assert(latestFullFrameArmMs(15000,12640)==1340,"FT8 complete-frame admission");
static_assert(latestFullFrameArmMs(7500,5040)==1440,"FT4 complete-frame admission");
static_assert(recoveryDeadlineUtcMs(0,15000,12640)==16150,"FT8 RX + late-reply recovery deadline");
static_assert(recoveryDeadlineUtcMs(0,7500,5040)==8750,"FT4 RX + late-reply recovery deadline");
static_assert(replyArmDeadlineUtcMs(100000,15000,12640)-recoveryDeadlineUtcMs(100000,15000,12640)==190,"independent GUI/finish reserve");
static_assert(recoveryDeadlineUtcMs(1000,15000,12640)-recoveryDeadlineUtcMs(0,15000,12640)==1000,"deadline must follow slot UTC, not launch time");
static_assert(latestFullFrameArmMs(500,1000)==0,"short profiles cannot admit an intact late frame");
static_assert(latestFullFrameStartMs(15000,12640)==2040,"FT8 hard first-PCM cutoff differs from tone target");
static_assert(latestFullFrameStartMs(7500,5040)==2140,"FT4 full-frame backend allowance");
int main(){return 0;}
