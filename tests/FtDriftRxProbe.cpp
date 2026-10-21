// Optional DSP probe: tools/run_ft_drift_probes.py builds this against the app.
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <QFile>
#include <QElapsedTimer>
#include <QSet>
#include <QTextStream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 4) return 2;
    const QString action = argv[1], mode = argv[2], path = argv[3];
    const bool ft4 = mode == "FT4";
    if (action == "generate") {
        Ft8Transmitter tx(mode, "IZ6NNH LZ2KSQ RR73", 48000, 1033);
        QVector<float> wave((ft4 ? 7500 : 15000) * 48);
        const int n = tx.generate(wave.data(), wave.size());
        QVector<double> out((ft4 ? 7500 : 15000) * 12);
        const int offset = ft4 ? 3600 : 6000;
        for (int i = 0; i < n / 4 && i + offset < out.size(); ++i) out[i + offset] = wave[i * 4];
        QFile file(path); if (!file.open(QIODevice::WriteOnly)) return 2;
        file.write(reinterpret_cast<const char *>(out.constData()), out.size() * sizeof(double));
        return 0;
    }
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return 2;
    const auto bytes = file.readAll();
    QVector<double> samples(bytes.size() / sizeof(double));
    std::memcpy(samples.data(), bytes.constData(), samples.size() * sizeof(double));
    Ft8RxDecoder rx;
    rx.setRadioFrequencyHz(144000000.0);
    rx.setModeName(mode); rx.setMyCall("IZ6NNH"); rx.setDxCall("LZ2KSQ"); rx.setRxMarkerHz(1033);
    const auto utc = QDateTime::fromMSecsSinceEpoch(1792309800000LL, Qt::UTC);
    const double truth = argc > 4 ? QString(argv[4]).toDouble() : 0;
    const bool expected = argc > 5 ? QString(argv[5]) == "yes" : true;
    QElapsedTimer timer; timer.start();
    QVector<Ft8RxDecoder::Decode> base;
    Ft8RxDecoder::PerfStats stats; stats.modeName = mode; stats.phase = "boundary";
    if (action == "pipeline") base = rx.decodeSlot(samples, utc, nullptr, &stats);
    else if (action == "off" || action == "recover") rx.recoverDriftingSignal(samples, utc, base, &stats, {});
    const double baselineMs = timer.nsecsElapsed() / 1e6;
    const int baselineSize = base.size();
    if (action == "off") {
        if (!base.isEmpty() || stats.driftAttempts || stats.driftRecoveryMs) return 1;
        std::cout << "OFF PASS\n"; return 0;
    }
    const double maxRate = argc > 6 ? QString(argv[6]).toDouble() : 5.0;
    rx.setDriftRxOptions(true, maxRate);
    if (action == "rfblock") {
        rx.setRadioFrequencyHz(argc > 7 ? QString(argv[7]).toDouble() : 143999999.0);
        rx.recoverDriftingSignal(samples, utc, base, &stats, {});
        if (!base.isEmpty() || stats.driftAttempts || stats.driftRecoveryMs) return 1;
        std::cout << mode.toStdString() << " RF gate PASS\n";
        return 0;
    }

    if (action == "track") {
        QVector<Ft8RxDecoder::Decode> first;
        rx.recoverDriftingSignal(samples, utc, first, &stats, {});
        if (first.isEmpty() || !rx.m_driftTrackSlotMs || argc < 8) { std::cerr << "track: initial recovery absent\n"; return 1; }
        std::cerr << "track first " << first.first().frequencyHz << " drift " << rx.m_driftTrackRate << "\n";
        QFile secondFile(argv[7]); if (!secondFile.open(QIODevice::ReadOnly)) return 2;
        const auto secondBytes = secondFile.readAll();
        QVector<double> second(secondBytes.size() / sizeof(double));
        std::memcpy(second.data(), secondBytes.constData(), secondBytes.size());
        QVector<Ft8RxDecoder::Decode> next;
        rx.recoverDriftingSignal(second, utc.addMSecs(ft4 ? 15000 : 30000), next, &stats, {});
        if (next.isEmpty() || rx.m_rxMarkerHz != 1033) { std::cerr << "track: next recovery absent or RX marker moved\n"; return 1; }
        std::cerr << "track next " << next.first().frequencyHz << " drift " << next.first().driftHzPerSec << "\n";
        if (std::abs(next.first().frequencyHz - (1033 + truth * (ft4 ? 15 : 30))) > 4) return 1;
        // A different selected DX must not inherit this station's drift.
        rx.setDxCall("G0JEI");
        QVector<Ft8RxDecoder::Decode> other;
        Ft8RxDecoder::PerfStats otherStats;
        rx.recoverDriftingSignal(second, utc.addMSecs(ft4 ? 15000 : 30000), other, &otherStats, {});
        // A valid station may still be inside the ordinary local search after
        // changing DX. Assert that the OLD predicted center was not inherited,
        // rather than requiring the decoder to suppress a CRC-valid message.
        if (std::abs(otherStats.driftSearchCenterHz - 1033) > 0.01) return 1;
        std::cout << mode.toStdString() << " next-slot prediction/reset PASS\n";
        return 0;
    }
    if (action == "pipeline") {
        Ft8RxDecoder::PerfStats enabledStats; enabledStats.modeName=mode; enabledStats.phase="boundary";
        const auto enhanced = rx.decodeSlot(samples, utc, nullptr, &enabledStats);
        for (const auto &d : base) {
            const auto it = std::find_if(enhanced.cbegin(), enhanced.cend(), [&](const Ft8RxDecoder::Decode &e) {
                return d.message == e.message && d.frequencyHz == e.frequencyHz && d.dt == e.dt && d.snrDb == e.snrDb;
            });
            if (it == enhanced.cend()) throw std::runtime_error("baseline result changed/lost");
        }
        base = enhanced; stats = enabledStats;
    } else rx.recoverDriftingSignal(samples, utc, base, &stats, {});
    bool found = false;
    double estimate = 0;
    for (const auto &d : base) if (d.message == "IZ6NNH LZ2KSQ RR73") { found = true; estimate = d.driftHzPerSec; }
    std::cout << mode.toStdString() << " truth=" << truth << " baseline=" << baselineSize
              << " found=" << found << " drift=" << estimate << " attempts=" << stats.driftAttempts
              << " workers=" << stats.driftWorkers << " recovery_ms=" << stats.driftRecoveryMs << " baseline_ms=" << baselineMs << '\n';
    if (found != expected) return 1;
    if (expected && estimate != 0 && std::abs(estimate - truth) > (ft4 ? 1.1 : .55)) return 1;
    if (!expected && !base.isEmpty()) return 1;
    return 0;
}
