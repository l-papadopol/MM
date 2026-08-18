#include "modems/msk144/Msk144Decoder.h"
#include "modems/msk144/tx/Msk144Transmitter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>

#include <algorithm>
#include <iostream>

namespace {

QVector<float> generatedPrefix(Msk144Transmitter &transmitter, int wanted)
{
    QVector<float> samples(wanted);
    const int produced = transmitter.generate(samples.data(), wanted);
    samples.resize(qMax(0, produced));
    return samples;
}

QVector<float> withStartOffset(const QVector<float> &source, int offset)
{
    QVector<float> shifted(source.size() + offset, 0.0f);
    std::copy(source.cbegin(), source.cend(), shifted.begin() + offset);
    return shifted;
}

bool containsMessage(const QVector<Msk144Decode> &results, const QString &message, bool shortMessage)
{
    const QString expected = message.trimmed().toUpper();
    return std::any_of(results.cbegin(), results.cend(), [&](const Msk144Decode &result) {
        return result.message.trimmed().toUpper() == expected && result.shortMessage == shortMessage;
    });
}

bool testFullFrame()
{
    constexpr int centreHz = 1375;
    Msk144Transmitter transmitter(QStringLiteral("CQ IZ6NNH JN61"), 12000, 15, false, centreHz);
    if (!transmitter.generationSucceeded()) return false;
    const QVector<float> generated = generatedPrefix(transmitter, 15 * 12000);
    if (generated.isEmpty() || generated.size() % 864 != 0 ||
        generated.size() > 15 * 12000 - 3 * 12000 / 10) {
        std::cerr << "FAIL MSK144 complete-frame TX geometry\n";
        return false;
    }
    const QVector<float> samples = withStartOffset(generated.mid(0, 5 * 864), 18);
    Msk144Decoder decoder;
    decoder.setDecodeDepth(2);
    decoder.setRxFrequencyHz(centreHz);
    decoder.setFrequencyToleranceHz(50);
    const QVector<Msk144Decode> results =
        decoder.decodeRecordedPeriod(samples, QDateTime::fromSecsSinceEpoch(0, Qt::UTC));
    const bool ok = containsMessage(results, transmitter.normalizedMessage(), false);
    std::cout << (ok ? "PASS" : "FAIL") << " MSK144 full-frame round-trip\n";
    return ok;
}

bool testShortFrame()
{
    const QString source = QStringLiteral("<K1ABC IZ6NNH> R+00");
    constexpr int centreHz = 1375;
    Msk144Transmitter transmitter(source, 12000, 15, true, centreHz);
    if (!transmitter.generationSucceeded() || !transmitter.generatedShortMessage()) return false;
    const QVector<float> generated = generatedPrefix(transmitter, 15 * 12000);
    if (generated.isEmpty() || generated.size() % 240 != 0 ||
        generated.size() > 15 * 12000 - 3 * 12000 / 10) {
        std::cerr << "FAIL MSK40 complete-frame TX geometry\n";
        return false;
    }
    const QVector<float> samples = withStartOffset(generated.mid(0, 6 * 240), 18);
    Msk144Decoder decoder;
    decoder.setDecodeDepth(2);
    decoder.setRxFrequencyHz(centreHz);
    decoder.setFrequencyToleranceHz(50);
    decoder.setShortMessagesEnabled(true);
    decoder.setMyCall(QStringLiteral("IZ6NNH"));
    decoder.setDxCall(QStringLiteral("K1ABC"));
    const QVector<Msk144Decode> results =
        decoder.decodeRecordedPeriod(samples, QDateTime::fromSecsSinceEpoch(0, Qt::UTC));
    const bool ok = containsMessage(results, transmitter.normalizedMessage(), true);
    std::cout << (ok ? "PASS" : "FAIL") << " MSK40 short-frame round-trip\n";
    return ok;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    return (testFullFrame() && testShortFrame()) ? 0 : 1;
}
