// SPDX-License-Identifier: GPL-3.0-or-later
// Cross-language inference/audio parity and streaming lifecycle probe.
#include "modems/cw/neural/CwNeuralDecoder.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc<4) { std::cerr << "features input.f32 logits.f32 | audio rate tone input.f32 [chunk]\n"; return 2; }
    madmodem::cwskimmer::CwNeuralDecoder decoder;
    const std::string mode=argv[1];
    if (mode=="features") {

        std::ifstream input(argv[2],std::ios::binary);
        std::ofstream output(argv[3],std::ios::binary);
        if (!input || !output) return 3;
        std::array<float,9> features{};
        while (input.read(reinterpret_cast<char*>(features.data()),sizeof(features))) {
            const auto logits=decoder.inferFeatures(features);
            output.write(reinterpret_cast<const char*>(logits.data()),sizeof(logits));
        }
        return input.eof() && output.good() ? 0 : 4;
    }
    if (mode!="audio" || argc<5) return 2;

    const int rate=std::atoi(argv[2]);
    decoder.setToneHz(std::atof(argv[3]));
    std::ifstream input(argv[4],std::ios::binary|std::ios::ate);
    if (!input) return 3;
    const auto bytes=input.tellg(); input.seekg(0);
    if (bytes<0 || static_cast<std::size_t>(bytes)%sizeof(float)) return 4;
    std::vector<float> audio(static_cast<std::size_t>(bytes)/sizeof(float));
    input.read(reinterpret_cast<char*>(audio.data()),bytes);
    const std::size_t chunk=argc>5 ? std::max(1,std::atoi(argv[5])) : 1024;
    std::string text;
    decoder.setCallback([&](const std::string& s) { text+=s; });
    for (std::size_t i=0;i<audio.size();i+=chunk)
        decoder.process(audio.data()+i,std::min(chunk,audio.size()-i),rate);
    std::cout << text << '\n';
    return 0;
}
