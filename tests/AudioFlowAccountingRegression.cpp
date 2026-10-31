// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio/BoundedAudioDispatcher.h"
#include "audio/RadioPowerWindow.h"
#include <QCoreApplication>
#include <cmath>
#include <stdexcept>
#include <iostream>
static void check(bool ok,const char *s){if(!ok)throw std::runtime_error(s);}
int main(int argc,char **argv){QCoreApplication app(argc,argv);
 BoundedAudioDispatcher q(4);AudioBlock b;b.sampleRate=48000;b.samples=QVector<float>(1024);
 for(int n=0;n<100;++n){b.captureSequence=n+1;q.enqueue(b);}auto before=q.statistics();check(before.dropped==96&&before.pending==4&&before.peakPending==4,"overflow accounting");
 int dropped=0;auto first=q.takePending(2,&dropped);check(dropped==96&&first[0].captureSequence==97&&first[1].captureSequence==98,"FIFO after overload");q.clear();auto after=q.statistics();check(after.enqueued==after.taken+after.dropped+after.cleared+after.pending&&after.cleared==2,"clear accounting");
 RadioPowerWindow window;const qint64 base=1792519200000000000LL;const qint64 duration=1024LL*1000000000LL/48000;
 window.begin(base,base+10*duration,300,3000);double sum=0;
 for(int n=0;n<=10;++n){b.firstSampleUtcNs=base+n*1024LL*1000000000LL/48000;for(int i=0;i<1024;++i)b.samples[i]=.01*std::sin((n*1024+i)*.71)+.02*std::sin((n*1024+i)*.13);
  if(n<10)sum+=MadModemAudio::radioTelescopeWelchBandPower(b.samples,48000,300,3000);window.process(b);}
 auto power=window.snapshot();check(power.ready&&!power.gap&&power.blocks==10&&power.sum==sum,"timestamped power window parity without GUI events");
 for(int n=11;n<30;++n){b.firstSampleUtcNs=base+n*duration;window.process(b);}window.markGap();
 power=window.snapshot();check(power.ready&&!power.gap&&power.blocks==10&&power.sum==sum,"finished window changed after later audio or gap");
 window.begin(base,base+10*duration,300,3000);b.firstSampleUtcNs=base+4*duration;window.process(b);b.firstSampleUtcNs=base+11*duration;window.process(b);power=window.snapshot();check(power.ready&&power.gap,"partial initial window accepted");
 window.begin(base+100*duration,base+110*duration,300,3000);b.firstSampleUtcNs=base;window.process(b);check(window.snapshot().blocks==0&&!window.snapshot().ready,"old window publication leaked");window.markGap();check(window.snapshot().gap,"queue loss did not invalidate measurement");
 std::cout<<"PASS bounded FIFO/overflow/clear/lifetime accounting; radio window exact power, GUI independence, partial/gap/epoch rejection\n";
}
