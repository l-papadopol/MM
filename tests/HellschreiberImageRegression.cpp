// SPDX-License-Identifier: GPL-3.0-or-later
// Independent keyed/FSK source; compare the public RX paper against known pixels.
#include "modems/hell/HellschreiberDecoder.h"
#include <QCoreApplication>
#include <cmath>
#include <random>
#include <iostream>
#include <vector>
static bool bit(int p){ unsigned v=unsigned(p/14)*2654435761u; int y=p%14; return ((v>>(y+3))&1)!=0; }
static void feed(HellschreiberDecoder &rx, int rate, int chunk) {
    for (int start = 0; start < rate; start += chunk) {
        AudioBlock b; b.sampleRate = rate; b.firstSampleIndex = start;
        b.samples.resize(std::min(chunk, rate - start));
        for (int i = 0; i < b.samples.size(); ++i) {
            const double time = double(start + i) / rate;
            b.samples[i] = ((int(time * 245) % 7) < 4 ? .3 : 0.) *
                           std::sin(6.283185307179586 * 1000 * time);
        }
        rx.processAudioBlock(b);
    }
}
static bool invariants() {
    for (bool fsk : {false, true}) {
        HellschreiberDecoder a, b;
        a.setVerticalScale(1); b.setVerticalScale(1);
        if (fsk) { a.setVariant(HellschreiberDecoder::Variant::Fsk105);
                   b.setVariant(HellschreiberDecoder::Variant::Fsk105); }
        for (int rate : {44100, 48000, 96000}) {
            feed(a, rate, 17); feed(b, rate, 1024);
            if (a.currentImage() != b.currentImage()) return false;
        }
        a.reset(); b.reset();
        feed(a, 48000, 513); feed(b, 48000, 1024);
        if (a.currentImage() != b.currentImage()) return false;
        const QImage before = a.currentImage();
        a.setVerticalScale(3); a.setVerticalScale(1);
        if (a.currentImage() != before) return false;
        a.reset(); AudioBlock silent; silent.sampleRate = 48000;
        silent.samples.fill(0, 48000); a.processAudioBlock(silent);
        const auto image = a.currentImage();
        for (int x = 0; x < 17; ++x) for (int y = 0; y < 14; ++y)
            if (qRed(image.pixel(x,y)) != 255) return false;
    }
    std::cout << "{\"chunk_reset_rate_zoom_silence\":true}\n";
    return true;
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);bool extended=argc>1;bool pass=true;for(int scenario=0;scenario<(extended?3:1);++scenario)for(bool fsk:{false,true})for(int rate:{44100,48000,96000})for(double snr:{40.,10.,0.}){
 HellschreiberDecoder rx;rx.setVerticalScale(1);if(fsk){rx.setVariant(HellschreiberDecoder::Variant::Fsk105);} double tone=scenario==1?500.:scenario==2?1800.:1000.; double offset=scenario==1?(fsk?8.:25.):scenario==2?(fsk?-8.:-25.):0.; rx.setToneHz(tone); double speed=17.5*14; int pixels=2800; int n=int(std::ceil(pixels*rate/speed)); double phase=0;std::mt19937 gen(63821+scenario*903);std::normal_distribution<double> noise(0,1);double sigma=0.3/std::sqrt(2.)*std::pow(10.,-snr/20.);
 for(int start=0;start<n;){AudioBlock b;b.sampleRate=rate;int count=std::min(1024,n-start);b.samples.resize(count);for(int i=0;i<count;++i){int p=int((start+i)*speed/rate);bool on=bit(p);double freq=tone+offset+(fsk?(on?-27.5:27.5):0.);phase+=6.283185307179586*freq/rate;double amp=fsk||on?.3:0.;double fade=extended?0.65+0.35*std::sin(6.283185307179586*0.3*(start+i)/rate):1.; b.samples[i]=fade*amp*std::sin(phase)+sigma*noise(gen)+(extended?.07*std::sin(6.283185307179586*(tone+650.)*(start+i)/rate):0.);}rx.processAudioBlock(b);start+=count;}
 QImage image=rx.currentImage();double best=1e9;int lag=0;for(int d=-3;d<=5;++d){double sum=0;int count=0;for(int p=280;p<pixels-20;++p){int k=p+d;if(k<0)continue;double ink=(255-qRed(image.pixel(k/14,13-k%14)))/245.;double target=bit(p)?1:0;sum+=(ink-target)*(ink-target);++count;}double rmse=std::sqrt(sum/count);if(rmse<best){best=rmse;lag=d;}}
 pass=pass && std::isfinite(best) && best<0.38; std::cout<<"{\"scenario\":"<<scenario<<",\"fsk\":"<<fsk<<",\"rate\":"<<rate<<",\"snr\":"<<snr<<",\"rmse\":"<<best<<",\"lag\":"<<lag<<"}\n";
}return pass && invariants()?0:2;}
