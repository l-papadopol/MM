// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual UI native decoder, with default acquisition settings.
#include "modems/cw/CwDecoder.h"
#include <QCoreApplication>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <stdexcept>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        if (argc < 5 || std::string(argv[1]) != "audio")
            throw std::runtime_error("usage: CwNativeProbe audio RATE TONE INPUT.f32 [CHUNK]");
        const int rate=std::stoi(argv[2]);
        const std::size_t chunk=argc>5 ? std::stoul(argv[5]) : 997;
        if(rate<=0 || chunk==0) throw std::runtime_error("invalid rate/chunk");
        std::ifstream in(argv[4],std::ios::binary|std::ios::ate);
        if(!in) throw std::runtime_error("cannot open audio");
        const auto bytes=in.tellg();
        if(bytes<0 || bytes%4) throw std::runtime_error("invalid float audio");
        std::vector<float> audio(static_cast<std::size_t>(bytes)/4);
        in.seekg(0);in.read(reinterpret_cast<char*>(audio.data()),bytes);
        CwDecoder decoder;
        decoder.setDecodeEngine("native");decoder.setToneHz(std::stod(argv[3]));
        std::string text;
        QObject::connect(&decoder,&CwDecoder::priorityTextReceived,
            [&](int rank,const QString& s){if(rank==0)text+=s.toStdString();});
        for(std::size_t i=0;i<audio.size();i+=chunk){
            AudioBlock block;block.sampleRate=rate;block.firstSampleIndex=i;block.captureGeneration=1;
            const auto count=std::min(chunk,audio.size()-i);
            for(std::size_t j=0;j<count;++j)block.samples.append(audio[i+j]);
            decoder.processAudioBlock(block);
        }
        std::cout<<text;
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
