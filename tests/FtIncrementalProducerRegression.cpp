// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtIncrementalAnalysis.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);
 std::mutex mutex;std::condition_variable cv;bool release=false;std::atomic<bool> entered{false};
 MadModemFt::IncrementalAnalysis producer([&](const QVector<double>&,int){
  entered=true;std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return release;});return std::vector<double>(2048,1.0);
 },[](const QVector<double>&,int,double){return 1.0;});
 MadModemFt::AnalysisConfig c;c.epoch=1;c.slotUtcMs=15000;QVector<double> audio(6000,.25);
 need(producer.submit(c,0,audio),"first submit rejected");QElapsedTimer wait;wait.start();while(!entered&&wait.elapsed()<2000)QThread::msleep(1);need(entered,"worker did not start");
 QElapsedTimer elapsed;elapsed.start();
 need(producer.submit(c,6000,audio),"contiguous tail rejected");
 need(!producer.snapshot(audio,c.slotUtcMs),"unfinished work published");producer.invalidate();
 const auto submitMs=elapsed.elapsed();
 {std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_all();producer.stop();
 need(submitMs<100,"capture waited for the blocked preprocessing worker");
 need(!producer.snapshot(audio,c.slotUtcMs),"cancelled generation published");
 MadModemFt::IncrementalAnalysis failed([](const QVector<double>&,int)->std::vector<double>{throw std::bad_alloc();},{});
 need(failed.submit(c,0,audio),"failure fixture rejected");wait.restart();bool refused=false;
 while(wait.elapsed()<2000){if(!failed.submit(c,6000,audio)){refused=true;break;}QThread::msleep(1);}
 need(refused&&!failed.snapshot(audio,c.slotUtcMs),"worker failure did not fall back to uncached decoding");
 std::cout<<"PASS capture submit/snapshot/invalidate do not wait for blocked producer; cancelled publication rejected; worker exception falls back\n";
}
