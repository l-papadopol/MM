// SPDX-License-Identifier: GPL-3.0-or-later
// Independent FM chart -> public streaming RX -> final image, without TX reuse.
#include "modems/weatherfax/WeatherFaxDecoder.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <random>
#include <iostream>
static int chart(int x,int y) {
    if(y<4)return (x/80)%2?255:0;
    if(y<8)return qBound(0,x*255/799,255);
    return (x/4)%2?255:0;
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    bool pass=true;
    const bool extended=argc>1 && QString(argv[1])=="--extended";
    const QList<int> speeds=extended?QList<int>{60,90,240}:QList<int>{120};
    for(int rate:{44100,48000,96000})for(int lpm:speeds)for(double snr:(extended?QList<double>{35.,5.,-5.}:QList<double>{60.,10.,0.})) {
        WeatherFaxDecoder rx;rx.setAutoStartEnabled(false);rx.setAutoToneTrackingEnabled(false);rx.setLpm(lpm);
        const int length=int(std::ceil(rate*12.*60/lpm));double phase=0;
        std::mt19937 rng(extended?592783:87291);std::normal_distribution<double> noise(0,1);
        QElapsedTimer timer;timer.start();
        for(int start=0;start<length;) {
            // Deliberately varying, non line-aligned capture blocks.
            const int size=qMin(length-start,(start/257)%2?257:1024);
            AudioBlock block;block.sampleRate=rate;block.firstSampleIndex=start;block.samples.resize(size);
            for(int i=0;i<size;++i) {
                const int n=start+i;const int y=int(n*1LL*lpm/(rate*60LL)),x=int((n*1LL*lpm%(rate*60LL))*800/(rate*60LL));
                const double f=1500+800*chart(x,y)/255.;phase+=6.283185307179586*f/rate;
                // Held-out seed, slow fading and a nearby weak interfering tone.
                const double gain=.7+.3*std::cos(6.283185307179586*n/rate*.7);
                block.samples[i]=.25*gain*std::sin(phase)+.25/std::sqrt(2.)*std::pow(10.,-snr/20.)*noise(rng)
                    +.008*std::sin(6.283185307179586*3100*n/rate);
            }
            rx.processAudioBlock(block);start+=size;
        }
        const qint64 elapsed=timer.elapsed();const QImage image=rx.currentImage();
        double best=1e99,detail=0;int lag=0;
        // One global delay only; no per-row warping or contrast normalization.
        for(int delay=0;delay<=8;++delay) {
            double error=0,fine=0;int count=0,fineCount=0;
            for(int y=1;y<qMin(11,image.height());++y)for(int x=16;x<784;++x) {
                const double difference=qGray(image.pixel(x,y))-chart(x-delay,y);
                error+=difference*difference;++count;
                if(y>=8){fine+=difference*difference;++fineCount;}
            }
            const double rmse=count?std::sqrt(error/count):1e99;
            if(rmse<best){best=rmse;detail=fineCount?std::sqrt(fine/fineCount):1e99;lag=delay;}
        }
        // Quality floor on the known chart, in addition to streaming integrity.
        // The clean-detail bound catches a return to the R14 double smoothing.
        const double cleanBound=lpm==240?37.:lpm==120?34.:30.;
        const bool quality=snr<30 || (best<cleanBound && (lpm!=120 || detail<60));
        const bool ok=rx.receivedImageLines()==11&&std::isfinite(best)&&best<(snr<0?100:50)&&quality;
        pass &= ok;
        std::cout<<QJsonDocument(QJsonObject{{"rate",rate},{"lpm",lpm},{"snr_db",snr},{"rows",rx.receivedImageLines()},
            {"gray_rmse",best},{"detail_gray_rmse",detail},{"lag_pixels",lag},{"cpu_ms_including_source",elapsed},{"pass",ok}}).toJson(QJsonDocument::Compact).constData()<<std::endl;
    }
    // The public decoder must produce identical pixels regardless of capture
    // block boundaries; reset must also discard I/Q history after a rate change.
    auto feed=[](WeatherFaxDecoder &rx, int rate, int chunk) {
        const int length=rate*2;
        for(int start=0;start<length;start+=chunk) {
            AudioBlock b;b.sampleRate=rate;b.firstSampleIndex=start;
            b.samples.resize(qMin(chunk,length-start));
            for(int i=0;i<b.samples.size();++i) {
                const double t=(start+i)/double(rate);
                b.samples[i]=.25*std::sin(6.283185307179586*1900*t-2*std::cos(6.283185307179586*100*t));
            }
            rx.processAudioBlock(b);
        }
        return rx.currentImage();
    };
    WeatherFaxDecoder first,second;
    for(auto *rx:{&first,&second}){rx->setAutoStartEnabled(false);rx->setAutoToneTrackingEnabled(false);}
    bool boundaries=true;
    for(int rate:{44100,48000,96000}) {
        first.reset();second.reset();
        const QImage a=feed(first,rate,17),b=feed(second,rate,1024);
        boundaries &= a==b && first.receivedImageLines()==3;
        first.reset();
        boundaries &= feed(first,rate,513)==b;
    }
    std::cout<<"{\"chunk_reset_rate_change_pass\":"<<(boundaries?"true":"false")<<"}"<<std::endl;
    return pass&&boundaries?0:1;
}
