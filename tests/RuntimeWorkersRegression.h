#pragma once
#include "../runtime/RxDecoderWorker.h"
#include "../runtime/AsyncCatCommand.h"
#include "../modems/rtty/tx/RttyTransmitter.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <thread>

inline int runRuntimeWorkersRegression(QApplication &app)
{
    bool passed=true;
    auto check=[&](bool ok,const char *name){QTextStream(stdout)<<(ok?"PASS ":"FAIL ")<<name<<'\n';passed &=ok;};
    auto pump=[&](const std::function<bool()> &done,int limit=3000){
        QElapsedTimer timer;timer.start();
        while(!done() && timer.elapsed()<limit){app.processEvents();QThread::msleep(1);}
        return done();
    };
    QThread rxThread;
    auto *rtty=new RttyDecoder;
    auto *hell=new HellschreiberDecoder;
    auto *worker=new RxDecoderWorker({new WeatherFaxDecoder,new SstvDecoder,rtty,new RttyMultiDecoder,
        new Bpsk31Decoder,new MfskDecoder,new CwDecoder,hell,new Msk144Decoder});
    worker->moveToThread(&rxThread);
    QObject::connect(&rxThread,&QThread::finished,worker,&QObject::deleteLater);
    rxThread.start();
    RxDecoderWorker::Config config;config.mode=RttyDecoder::modeName();config.enabled=true;config.filter.enabled=false;
    QMetaObject::invokeMethod(worker,[&](){worker->configure(config);rtty->setAutoReverseEnabled(false);},Qt::BlockingQueuedConnection);
    QString decoded;QMutex textMutex;std::atomic_bool wrongThread{false};std::atomic_int drops{0};
    QObject::connect(rtty,&RttyDecoder::characterReceived,worker,[&](const QString &s){
        if(QThread::currentThread()!=&rxThread)wrongThread=true;
        QMutexLocker lock(&textMutex);decoded+=s;
    },Qt::DirectConnection);
    QObject::connect(worker,&RxDecoderWorker::overload,worker,[&](int count){drops+=count;},Qt::DirectConnection);
    auto decodedText=[&](){QMutexLocker lock(&textMutex);return decoded;};
    RttyTransmitter tx("TEST 599",48000,45.45,2125,2295,false);QVector<float> wave;
    while(!tx.isFinished()){QVector<float> b(4096);b.resize(tx.generate(b.data(),b.size()));wave+=b;}
    std::atomic_bool producerDone{false};
    std::thread producer([&](){
        for(int i=0;i<wave.size();i+=4096){AudioBlock b;b.sampleRate=48000;b.captureGeneration=1;b.firstSampleIndex=i;b.samples=wave.mid(i,4096);worker->queue()->enqueue(b);QThread::msleep(5);}
        producerDone=true;
    });
    // Deliberately do not process GUI events: audio must still reach the decoder.
    QElapsedTimer blocked;blocked.start();
    while((!producerDone || decodedText()!="TEST 599") && blocked.elapsed()<3000)QThread::msleep(5);
    producer.join();
    check(decodedText()=="TEST 599" && !wrongThread,"RTTY completes while GUI event loop is blocked");
    check(drops==0,"normal RX preserves every audio block");
    std::atomic_bool paused{false},release{false};
    QMetaObject::invokeMethod(worker,[&](){paused=true;while(!release)QThread::msleep(1);},Qt::QueuedConnection);
    while(!paused)QThread::msleep(1);
    for(int i=0;i<100;++i){AudioBlock b;b.sampleRate=48000;b.captureGeneration=2;b.firstSampleIndex=i*1024;b.samples=QVector<float>(1024);worker->queue()->enqueue(b);}
    check(worker->queue()->pendingBlockCount()==24,"RX overload queue remains bounded");
    release=true;
    check(pump([&](){return drops>=76;}),"RX overload detected in worker");
    // Reset after a missing region must clear the FIGS state from the first message.
    QMetaObject::invokeMethod(worker,[&](){worker->beginCapture();},Qt::BlockingQueuedConnection);
    {QMutexLocker lock(&textMutex);decoded.clear();}
    RttyTransmitter next("CQ",48000,45.45,2125,2295,false);qint64 offset=100000;
    while(!next.isFinished()){
        AudioBlock b;b.sampleRate=48000;b.captureGeneration=3;b.firstSampleIndex=offset;b.samples.resize(4096);b.samples.resize(next.generate(b.samples.data(),b.samples.size()));offset+=b.samples.size();
        worker->queue()->enqueue(b);QThread::msleep(5);
    }
    check(pump([&](){return decodedText()=="CQ";}),"RTTY decodes after overload and restart");
    {QMutexLocker lock(&textMutex);decoded.clear();}
    QMetaObject::invokeMethod(worker,[&](){worker->beginCapture();rtty->setAutoReverseEnabled(true);rtty->setCatModeHint("RTTYR");},Qt::BlockingQueuedConnection);
    QThread::msleep(20); // GUI stays blocked while worker applies automatic polarity.
    RttyTransmitter reverseTx("REVERSE",48000,45.45,2125,2295,true);offset=0;
    while(!reverseTx.isFinished()){
        AudioBlock b;b.sampleRate=48000;b.captureGeneration=4;b.firstSampleIndex=offset;b.samples.resize(4096);b.samples.resize(reverseTx.generate(b.samples.data(),b.samples.size()));offset+=b.samples.size();
        worker->queue()->enqueue(b);QThread::msleep(5);
    }
    blocked.restart();
    while(decodedText()!="REVERSE" && blocked.elapsed()<3000)QThread::msleep(5);
    check(decodedText()=="REVERSE","RTTY automatic polarity does not depend on GUI callbacks");
    std::atomic_int imageNotifications{0};
    worker->acknowledgeImages();
    QObject::connect(worker,&RxDecoderWorker::imagesAvailable,worker,[&](){++imageNotifications;},Qt::DirectConnection);
    QImage raster(16,16,QImage::Format_RGB32);raster.fill(Qt::black);
    for(int i=0;i<32;++i)worker->command(hell,&HellschreiberDecoder::appendTransmitRaster,true,raster);
    check(imageNotifications==1,"image updates coalesce while GUI is blocked");
    worker->acknowledgeImages();
    worker->command(hell,&HellschreiberDecoder::appendTransmitRaster,true,raster);
    check(imageNotifications==2 && !worker->image(hell).isNull(),"GUI consumes latest image and receives subsequent updates");
    rxThread.quit();rxThread.wait();

    QThread catThread;auto *target=new QObject;target->moveToThread(&catThread);
    QObject::connect(&catThread,&QThread::finished,target,&QObject::deleteLater);catThread.start();
    AsyncCatCommand gate;std::atomic_bool keyed{false};int callbacks=0;bool accepted=false;int heartbeats=0;
    QTimer heartbeat;QObject::connect(&heartbeat,&QTimer::timeout,&app,[&](){++heartbeats;});heartbeat.start(2);
    std::atomic_bool releaseBackend{false};
    gate.request(target,[&](){while(!releaseBackend)QThread::msleep(1);keyed=true;return true;},[&](){keyed=false;return true;},[&](bool ok){++callbacks;accepted=ok;},20);
    check(pump([&](){return callbacks==1;}),"slow CAT times out without blocking GUI");
    check(!accepted && gate.busy() && heartbeats>0,"timeout revokes TX while retaining CAT gate");
    releaseBackend=true;
    check(pump([&](){return !gate.busy();}) && !keyed && callbacks==1,"late PTT completion compensated exactly once");
    // Cancellation after worker success but before GUI receives its acknowledgement.
    std::atomic_bool completed{false};callbacks=0;
    gate.request(target,[&](){keyed=true;completed=true;return true;},[&](){keyed=false;return true;},[&](bool ok){++callbacks;accepted=ok;});
    while(!completed)QThread::msleep(1);
    gate.cancel();
    check(pump([&](){return !gate.busy();}) && callbacks==1 && !accepted && !keyed,"cancel races with successful CAT acknowledgement safely");
    callbacks=0;
    gate.request(target,[&](){keyed=true;return true;},[&](){keyed=false;return true;},[&](bool ok){++callbacks;accepted=ok;});
    check(pump([&](){return callbacks==1;}) && accepted && keyed,"successful CAT acknowledged before TX authorization");
    callbacks=0;
    gate.request(target,[](){return false;},[](){return false;},[&](bool ok){++callbacks;accepted=ok;});
    check(pump([&](){return callbacks==1;}) && !accepted && gate.faulted(),"failed CAT recovery blocks further TX");
    bool executed=false;
    gate.request(target,[&](){executed=true;return true;},[](){return true;},[&](bool ok){accepted=ok;});
    check(!executed && !accepted && !gate.busy(),"faulted CAT cannot authorize a new command");
    heartbeat.stop();catThread.quit();catThread.wait();
    return passed?0:1;
}
