#include "modems/q65/Q65NativeEngine.h"
#include "modems/q65/tx/Q65Transmitter.h"

#include <QCoreApplication>
#include <QVector>

#include <algorithm>
#include <iostream>

namespace {

QVector<double> generatedWaveform(Q65Transmitter &transmitter)
{
    QVector<double> samples;
    QVector<float> block(4096);
    while (!transmitter.isFinished()) {
        const int count = transmitter.generate(block.data(), block.size());
        if (count <= 0) break;
        const int oldSize = samples.size();
        samples.resize(oldSize + count);
        for (int i = 0; i < count; ++i) samples[oldSize + i] = block.at(i);
    }
    return samples;
}

bool testSubmode(Q65Mode::Submode submode)
{
    const QString sourceMessage = QStringLiteral("CQ IZ6NNH JN61");
    constexpr int txFrequencyHz = 1373;
    constexpr int startOffsetSamples = 3600;
    Q65Transmitter transmitter(sourceMessage, 12000, 15, submode, txFrequencyHz);
    if (!transmitter.generationSucceeded()) {
        std::cerr << "Q65 TX failed: " << transmitter.generationError().toStdString() << '\n';
        return false;
    }
    const QVector<double> generated = generatedWaveform(transmitter);
    if (generated.size() != 15 * 12000 - 3 * 12000 / 10) {
        std::cerr << "Q65 waveform length mismatch\n";
        return false;
    }
    QVector<double> waveform(15 * 12000, 0.0);
    const int copyCount = qMin(static_cast<int>(generated.size()),
                               static_cast<int>(waveform.size()) - startOffsetSamples);
    std::copy(generated.cbegin(), generated.cbegin() + copyCount,
              waveform.begin() + startOffsetSamples);

    Q65NativeEngine engine;
    Q65NativeEngine::Configuration configuration;
    configuration.periodSeconds = 15;
    configuration.decodeDepth = 2;
    configuration.submode = submode;
    configuration.rxFrequencyHz = 1375;
    configuration.dfToleranceHz = 50;
    configuration.averaging = false;
    configuration.singleDecode = true;
    configuration.apDecode = false;
    const QVector<Q65NativeEngine::Result> results = engine.decode(waveform, 0, configuration);
    const QString expected = transmitter.normalizedMessage().trimmed().toUpper();
    const bool found = std::any_of(results.cbegin(), results.cend(), [&expected](const Q65NativeEngine::Result &result) {
        return result.message.trimmed().toUpper() == expected;
    });
    std::cout << (found ? "PASS " : "FAIL ")
              << Q65Mode::modeName(submode).toStdString()
              << " offset/frequency round-trip: " << expected.toStdString() << '\n';
    return found;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    bool ok = true;
    ok = testSubmode(Q65Mode::Submode::A) && ok;
    ok = testSubmode(Q65Mode::Submode::B) && ok;
    ok = testSubmode(Q65Mode::Submode::C) && ok;
    ok = testSubmode(Q65Mode::Submode::D) && ok;
    ok = Q65NativeEngine::symbolSamples(15) == 1800 && ok;
    ok = Q65NativeEngine::symbolSamples(30) == 3600 && ok;
    ok = Q65NativeEngine::symbolSamples(60) == 7200 && ok;
    ok = Q65NativeEngine::symbolSamples(120) == 16000 && ok;
    ok = Q65Mode::minimumBaseToneHz(Q65Mode::Submode::A, 15) >= 600 && ok;
    ok = Q65Mode::maximumBaseToneHz(Q65Mode::Submode::D, 15) >= 1800 && ok;
    ok = Q65Mode::maximumBaseToneHz(Q65Mode::Submode::D, 15) < 1900 && ok;
    return ok ? 0 : 1;
}
