// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/cw/CwDecoder.h"
#include "modems/cw/neural/CwNeuralDecoder.h"
#include "tests/CwTestSignal.h"
#include <QCoreApplication>
#include <algorithm>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* message) {
    if(!ok) throw std::runtime_error(message);
}

static std::string directDecode(const std::vector<float>& audio, double tone) {
    madmodem::cwskimmer::CwNeuralDecoder direct;
    direct.setToneHz(tone);
    std::string output;
    direct.setCallback([&](const std::string& s) { output+=s; });
    direct.process(audio.data(),audio.size(),48000);
    return output;
}

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    CwDecoder decoder;
    require(decoder.decodeEngine()=="native","native is not default");
    decoder.setDecodeEngine("unknown");
    require(decoder.decodeEngine()=="native","unknown engine did not select native");
    decoder.setToneHz(1000);
    decoder.setDecodeEngine("neural_search");
    require(decoder.decodeEngine()=="neural_search","neural engine not selected");
    std::string text, textB;
    QObject::connect(&decoder,&CwDecoder::priorityTextReceived,
                     [&](int rank,const QString& s) { if(rank==0)text+=s.toStdString(); else if(rank==1)textB+=s.toStdString(); });
    cwtest::SignalOptions options; options.frequencyHz=1000;options.wpm=20;
    options.amplitude=.4;options.riseTimeMs=2;
    const std::string reference="CQ CQ DE IZ6NNH TEST 73";
    auto audio=cwtest::synthesize(reference,options);
    audio.resize(audio.size()+48000,0.f);
    for(std::size_t i=0;i<audio.size();i+=997) {
        AudioBlock block;block.sampleRate=48000;block.firstSampleIndex=i;block.captureGeneration=7;
        const auto count=std::min<std::size_t>(997,audio.size()-i);
        for(std::size_t j=0;j<count;++j) block.samples.append(audio[i+j]);
        decoder.processAudioBlock(block);
    }
    std::cout << "Neural selected RX: " << text << '\n';
    // Verify full output forwarding, including spaces, independently from
    // acoustic accuracy. Accuracy/regressions are measured by compare.py;
    // the model card/report explicitly retain the known clean-fixture errors.
    const auto expected=directDecode(audio,1000);
    require(expected.find("CQ")!=std::string::npos,"neural fixture has no recognizable CW");
    require(text==expected,"RX A altered direct neural output");
    decoder.setSecondaryToneHz(1500);decoder.setSecondaryEnabled(true);
    options.frequencyHz=1500;
    auto audioB=cwtest::synthesize(reference,options);audioB.resize(audioB.size()+48000,0.f);
    for(std::size_t i=0;i<audioB.size();i+=997) {
        AudioBlock block;block.sampleRate=48000;block.firstSampleIndex=i;block.captureGeneration=8;
        const auto count=std::min<std::size_t>(997,audioB.size()-i);
        for(std::size_t j=0;j<count;++j)block.samples.append(audioB[i+j]);
        decoder.processAudioBlock(block);
    }
    require(textB==directDecode(audioB,1500),"RX B altered direct neural output");
    decoder.setSecondaryEnabled(false);
    decoder.clearReceiver(0); decoder.clearReceiver(1);
    std::string searchExpected;
    for (std::size_t chunk:{std::size_t(32),std::size_t(997),std::size_t(4096)}) {
      madmodem::cwskimmer::CwNeuralDecoder search;
      search.setToneHz(1000);std::string output;
      search.setCallback([&](const std::string& s){output+=s;});
      for(std::size_t i=0;i<audio.size();i+=chunk)
        search.process(audio.data()+i,std::min(chunk,audio.size()-i),48000);
      if(searchExpected.empty())searchExpected=output;
      require(output==searchExpected,"search output depends on audio block size");
    }
    require(searchExpected.find("IZ6NNH")!=std::string::npos,"search fixture lost callsign");
    text.clear();
    for(std::size_t i=0;i<audio.size();i+=997){
      AudioBlock b;b.sampleRate=48000;b.firstSampleIndex=i;b.captureGeneration=10;
      const auto n=std::min<std::size_t>(997,audio.size()-i);
      for(std::size_t j=0;j<n;++j)b.samples.append(audio[i+j]);
      decoder.processAudioBlock(b);
    }
    require(text==searchExpected,"search RX A changed direct output");
    decoder.setSecondaryEnabled(true);textB.clear();
    madmodem::cwskimmer::CwNeuralDecoder searchB;
    searchB.setToneHz(1500);std::string searchExpectedB;
    searchB.setCallback([&](const std::string& s){searchExpectedB+=s;});
    searchB.process(audioB.data(),audioB.size(),48000);
    for(std::size_t i=0;i<audioB.size();i+=997){
      AudioBlock b;b.sampleRate=48000;b.firstSampleIndex=i;b.captureGeneration=12;
      const auto n=std::min<std::size_t>(997,audioB.size()-i);
      for(std::size_t j=0;j<n;++j)b.samples.append(audioB[i+j]);
      decoder.processAudioBlock(b);
    }
    require(textB==searchExpectedB,"search RX B changed direct output");
    decoder.setSecondaryEnabled(false);decoder.clearReceiver(1);
    // A pending search hypothesis must not escape a capture discontinuity.
    const auto beforeGap=decoder.receivedText();
    AudioBlock searchGap;searchGap.sampleRate=48000;searchGap.firstSampleIndex=9000000;
    searchGap.captureGeneration=11;searchGap.samples.fill(0,48000);
    decoder.processAudioBlock(searchGap);
    require(decoder.receivedText()==beforeGap,"search capture gap released stale suffix");
    // Restore B's original output for the existing clear-receiver assertions.
    textB.clear();
    const auto received=decoder.receivedText();
    decoder.setDecodeEngine("native");
    require(decoder.receivedText()==received,"engine switch erased committed text");
    decoder.setDecodeEngine("neural_search");
    require(decoder.receivedText()==received,"neural switch erased committed text");
    decoder.resumeAfterLocalTransmit();
    require(decoder.receivedText()==received,"TX resume erased committed text");
    AudioBlock gap;gap.sampleRate=48000;gap.firstSampleIndex=9000000;
    gap.captureGeneration=8;gap.samples.fill(0,1024);decoder.processAudioBlock(gap);
    require(decoder.receivedText()==received,"capture discontinuity emitted stale text");
    decoder.clearReceiver(0);
    require(decoder.receivedText()==QString::fromStdString(textB),"clearing A changed B text");
    decoder.clearReceiver(1);
    require(decoder.receivedText().isEmpty(),"clear RX B did not clear selected text");
    std::cout << "PASS native default/fallback, neural routing, engine switch, TX resume, capture gap and clear RX\n";
}
