// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/Ft8RxDecoder.h"
#include "dsp/gpu/FtSyncCompute.h"
#include <QCoreApplication>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);auto &engine=FtSyncCompute::Engine::instance();QElapsedTimer warm;warm.start();
 while(!engine.status().ready&&!engine.status().unavailable&&warm.elapsed()<20000)QThread::msleep(5);
 check(!engine.status().ready,"This probe requires no GPU: tests fallback, not hardware acceleration");
 int attempts=0,decoded=0;
 for(int file=1;file<argc;++file){QFile input(argv[file]);check(input.open(QIODevice::ReadOnly),"fixture missing");auto bytes=input.readAll();
  QVector<double> samples(bytes.size()/sizeof(double));std::memcpy(samples.data(),bytes.constData(),bytes.size());
  const bool ft4=samples.size()<100000;Ft8RxDecoder rx;rx.setModeName(ft4?"FT4":"FT8");
  rx.setRadioFrequencyHz(7074000);rx.setDriftRxOptions(false,20);
  auto candidates=ft4?rx.findFt4Candidates(samples):rx.findCandidates(samples);
  const auto utc=QDateTime::fromMSecsSinceEpoch(1792309800000LL,Qt::UTC);
  int accepted=0;
  for(int k=0;k<qMin(24,candidates.size());++k){Ft8RxDecoder::Decode a,b;
   rx.setGpuDecodeEnabled(false);
   const bool ac=ft4?rx.decodeFt4Candidate(samples,utc,candidates[k],a):rx.decodeCandidate(samples,utc,candidates[k],a,nullptr,nullptr,nullptr,false,false);
   rx.setGpuDecodeEnabled(true);
   const bool bc=ft4?rx.decodeFt4Candidate(samples,utc,candidates[k],b):rx.decodeCandidate(samples,utc,candidates[k],b,nullptr,nullptr,nullptr,false,false);
   check(ac==bc,"candidate acceptance changed in GPU fallback");
   if(ac){check(a.message==b.message&&a.frequencyHz==b.frequencyHz&&a.dt==b.dt&&a.snrDb==b.snrDb&&a.syncScore==b.syncScore,"decoded candidate changed in GPU fallback");++decoded;++accepted;}
   ++attempts;
  }
  std::cout<<"file="<<argv[file]<<" mode="<<(ft4?"FT4":"FT8")<<" pairs="<<qMin(24,candidates.size())<<" accepted="<<accepted<<" RF=7074000 drift=off exact=PASS\n";
 }
 std::cout<<"candidate_pairs="<<attempts<<" accepted_pairs="<<decoded<<" CPU/GPU-requested fallback parity PASS\n";
}
