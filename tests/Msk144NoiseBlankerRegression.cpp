// IC8TEM: the noise blanker and the carrier notch must be transparent for an MSK144
// ping: the same round-trip as Msk144NativeRegression decodes with every setting
// (the recording path removes the 48 ms delay of the blanker).
#include "dsp/common/RxNoiseBlanker.h"
#include "modems/msk144/Msk144Decoder.h"
#include "modems/msk144/tx/Msk144Transmitter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>

#include <algorithm>
#include <iostream>

using namespace MadModemNoise;

namespace {
constexpr int kCentreHz = 1375;

QVector<float> ping()
{
    Msk144Transmitter transmitter(QStringLiteral("CQ IZ6NNH JN61"), 12000, 15, false, kCentreHz);
    QVector<float> generated(15 * 12000);
    generated.resize(qMax(0, transmitter.generate(generated.data(), generated.size())));
    QVector<float> samples(5 * 864 + 18, 0.0f);
    std::copy(generated.cbegin(), generated.cbegin() + 5 * 864, samples.begin() + 18);
    return samples;
}

bool decodes(const QVector<float> &samples)
{
    Msk144Decoder decoder;
    decoder.setDecodeDepth(2);
    decoder.setRxFrequencyHz(kCentreHz);
    decoder.setFrequencyToleranceHz(50);
    const auto results = decoder.decodeRecordedPeriod(samples, QDateTime::fromSecsSinceEpoch(0, Qt::UTC));
    return std::any_of(results.cbegin(), results.cend(), [](const Msk144Decode &r) {
        return r.message.trimmed().toUpper() == QStringLiteral("CQ IZ6NNH JN61") && !r.shortMessage;
    });
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QVector<float> samples = ping();
    int failures = 0;
    const struct { BlankerMode mode; bool zap; const char *name; } cases[] = {
        {BlankerMode::Off, false, "blanker off"},
        {BlankerMode::Nb2, false, "NB2"},
        {BlankerMode::Nb2, true, "NB2 and carrier notch"},
        {BlankerMode::Auto, true, "NB2 Auto and carrier notch"},
    };
    for (const auto &c : cases) {
        Settings s;
        s.mode = c.mode;
        s.mskCarrierNotch = c.zap;
        setSettings(s);
        const bool ok = decodes(samples);
        std::cout << (ok ? "PASS" : "FAIL") << " MSK144 ping decodes with " << c.name << "\n";
        failures += ok ? 0 : 1;
    }
    return failures ? 1 : 0;
}
