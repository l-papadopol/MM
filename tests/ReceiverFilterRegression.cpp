#include "dsp/WaterfallLeveler.h"
#include "rig/Ts890FilterQuery.h"
#include "dsp/DspEngine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <random>

int main()
{
    bool ok = true;
    auto check = [&](bool condition, const char *name) {
        std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
        ok = condition && ok;
    };
    Ts890FilterQuery query;
    bool complete = false;
    check(query.command("RTTY") == "SL0;" && query.accept("RTTY", "SL000;", complete) && complete && query.widthHz() == 250,
          "FSK actual 250 Hz, not backend nominal 500 Hz");
    query.reset();
    check(query.accept("CW", "SL002;", complete) && complete && query.widthHz() == 100, "CW width table");
    query.reset();
    check(query.command("PKTUSB") == "EX00612;" && query.accept("PKTUSB", "EX00612 001;", complete) && !complete,
          "DATA shift/width configuration");
    check(query.accept("PKTUSB", "SL005;", complete) && complete && query.widthHz() == 250, "DATA narrow width");
    query.reset();
    check(query.accept("PKTLSB", "EX00612 000;", complete) && !complete &&
          query.accept("PKTLSB", "SL012;", complete) && !complete && query.command("PKTLSB") == "SH0;" &&
          query.accept("PKTLSB", "SH0010;", complete) && complete && query.widthHz() == 500,
          "DATA high/low 1100..1600 Hz gives 500 Hz");
    query.reset();
    check(query.command("USB") == "EX00611;", "SSB uses separate menu from DATA");
    check(!query.accept("USB", "EX00612 001;", complete) && query.widthHz() == 0, "reject unrelated/stale reply");
    check(!query.accept("RTTY", "SL099;", complete) && query.widthHz() == 0, "reject preset sentinel");
    check(!query.accept("RTTY", "SL0+1;", complete) && !query.accept("CW", "SL002", complete), "reject malformed or truncated frame");
    check(query.command("FM").isEmpty(), "unsupported mode has no extra CAT traffic");
    for (int width : {100,250,500}) for (int center : {700,1500,2210}) for (bool cat : {false,true}) {
        constexpr std::size_t bins = 1024;
        const double binHz = 2900.0 / (bins - 1);
        std::vector<double> line(bins), inside;
        for (std::size_t i = 0; i < bins; ++i) {
            const double hz = 100.0 + i * binHz;
            const double skirt = std::max(0.0, std::abs(hz - center) - width / 2.0);
            line[i] = -85.0 - std::min(55.0, skirt * 0.8) +
                      1.35 * std::sin(.173*i) + .45 * std::cos(.071*i);
        }
        // One CW tone or the two RTTY tones: never make those peaks part of
        // the floor. +/-85 Hz matches the 170-Hz RTTY shift.
        const int tone = static_cast<int>((center - 100) / binHz);
        auto withSignal = line;
        withSignal[tone] += 32;
        if (width >= 250) {
            withSignal[tone - 30] += 24;
            withSignal[tone + 30] += 28;
        }
        WaterfallLeveler leveler;
        const auto result = leveler.update(withSignal, .05, binHz, cat ? width : 0);
        for (std::size_t i = 0; i < bins; ++i)
            if (std::abs(100.0 + i*binHz - center) < width*.3 && withSignal[i] == line[i])
                inside.push_back(withSignal[i] - result.baselineDb[i]);
        std::sort(inside.begin(), inside.end());
        const double median = inside[inside.size()/2];
        std::cout << "width=" << width << " center=" << center << " CAT=" << cat << " noise=" << median << '\n';
        check(result.narrowBandCorrected && median < 5 && median > -2 &&
              withSignal[tone] - result.baselineDb[tone] > 28 && !result.partialBand,
              "narrow filter: dark noise, preserved tone and frequency axis");
        for (double &x : withSignal) x += 17;
        const auto step = leveler.update(withSignal, .05, binHz, cat ? width : 0);
        double delta = 0;
        for (std::size_t i = 0; i < bins; ++i)
            delta = std::max(delta, std::abs(step.baselineDb[i] - result.baselineDb[i] - 17));
        check(delta < .01, "AGC step does not change colours");
    }
    // A single strong FFT-shaped tone on an unfiltered noise floor is not a
    // receiver passband, even when a stale/nominal narrow CAT hint is supplied.
    std::vector<double> carrier(1024, -90);
    for (std::size_t i = 0; i < carrier.size(); ++i)
        carrier[i] += 55 * std::exp(-std::pow((static_cast<double>(i)-400)/4, 2));
    WaterfallLeveler leveler;
    const auto result = leveler.update(carrier,.05,2.83,100);
    check(!result.narrowBandCorrected && carrier[400] - result.baselineDb[400] > 50, "strong carrier is not flattened");
    // Exercise the production FFT/interpolation/colour mapping with PCM, not
    // only idealized dB lines. Random independent quadratures model band noise.
    for (int width : {100,250,500}) for (bool cat : {false,true}) {
        DspEngine dsp;
        dsp.setNarrowBandDisplay(true);
        dsp.configureReceiverFilter("CW", cat ? width : 0, cat ? 2 : 0);
        std::vector<int> noiseColours;
        int signalColour = 0;
        QObject::connect(&dsp, &DspEngine::waterfallLineReady, [&](const QVector<quint8> &colours, double lo, double hi) {
            for (int i = 0; i < colours.size(); ++i) {
                const double hz = lo + (hi-lo)*i/(colours.size()-1);
                if (std::abs(hz-1500) < width*.35 && std::abs(hz-1500) > 22) noiseColours.push_back(colours[i]);
                if (std::abs(hz-1500) < 3) signalColour = std::max(signalColour, int(colours[i]));
            }
        });
        std::mt19937 random(890);
        std::normal_distribution<double> gaussian(0,1);
        AudioBlock block;
        block.samples.resize(8192);
        for (int i = 0; i < block.samples.size(); ++i) block.samples[i] = static_cast<float>(gaussian(random)*1e-6);
        for (int k = 1; k < 4096; ++k) {
            const double hz = k*48000.0/8192;
            if (std::abs(hz-1500) > width/2.0) continue;
            const double re = gaussian(random)*.0002, im = gaussian(random)*.0002;
            for (int i = 0; i < block.samples.size(); ++i) {
                const double phase = 6.283185307179586*k*i/8192;
                block.samples[i] += static_cast<float>(re*std::cos(phase)+im*std::sin(phase));
            }
        }
        for (int i = 0; i < block.samples.size(); ++i)
            block.samples[i] += static_cast<float>(.01*std::sin(6.283185307179586*1500*i/48000));
        dsp.processAudioBlock(block);
        std::sort(noiseColours.begin(),noiseColours.end());
        const int medianColour = noiseColours.empty() ? 255 : noiseColours[noiseColours.size()/2];
        std::cout << "PCM width=" << width << " CAT=" << cat << " noise colour=" << medianColour << " tone=" << signalColour << '\n';
        check(medianColour < 140 && signalColour > 200, "production FFT: narrow noise not saturated, carrier visible");
    }
    return ok ? 0 : 1;
}
