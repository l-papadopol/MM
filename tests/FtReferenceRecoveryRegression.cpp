// SPDX-License-Identifier: GPL-3.0-or-later
#include "modems/ft8/FtReferenceRecovery.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <atomic>
#include <cmath>
#include <cstring>
#include <future>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
// Keep the imported GNU complex macros after the standard-library headers.
#include "third_party/mshv_gpl/ft_reference/decoderpom.h"

static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
static QSet<QString> messages(const FtReferenceRecovery::Result &result) {
    QSet<QString> out;
    for (const auto &row : result.rows) out.insert(row.message);
    return out;
}
static QVector<double> signal(const QString &mode, const QString &message, int hz) {
    const int count = mode == "FT4" ? 90000 : 180000;
    Ft8Transmitter tx(mode, message, 48000, hz);
    QVector<float> pcm(count * 4);
    int n = tx.generate(pcm.data(), pcm.size());
    QVector<double> samples(count);
    const int offset = mode == "FT4" ? 4032 : 6000;
    std::mt19937 rng(94327);
    std::normal_distribution<double> noise(0.0, .001);
    for (auto &v : samples) v = noise(rng);
    for (int i = 0; i < n / 4 && offset + i < count; ++i)
        samples[offset + i] += .2 * pcm[4 * i];
    return samples;
}
static FtReferenceRecovery::Request request(const QString &mode) {
    FtReferenceRecovery::Request r;
    r.mode = mode; r.myCall = "IZ6NNH"; r.dxCall = "LZ2KSQ";
    r.lowHz = 200; r.highHz = 3000; r.markerHz = 1500;
    r.radioHz = 14074000; r.offline = true; r.workers = 4;
    r.slotUtcMs = 1793288340000LL; r.phase = "offline";
    return r;
}
static void fftRegression() {
    MshvReference::F2a fft;
    for (int n : {64, 96, 4032}) {
        std::vector<double> input(n), output(n);
        std::vector<double complex> spectrum(n), wave(n);
        for (int i = 0; i < n; ++i) input[i] = .3 + std::sin(2 * std::acos(-1.) * 5 * i / n);
        fft.four2a_d2c(spectrum.data(), input.data(), n, -1, 0);
        require(std::abs(creal(spectrum[0]) - .3 * n) < 1.e-8, "FFT DC");
        require(std::abs(cimag(spectrum[5]) + n / 2.) < 1.e-8, "FFT sine");
        fft.four2a_d2c(spectrum.data(), output.data(), n, 1, -1);
        for (int i = 0; i < n; ++i) require(std::abs(output[i] / n - input[i]) < 1.e-10, "FFT real round trip");
        for (int i = 0; i < n; ++i) wave[i] = mshv_make_complex(input[i], .5 * input[i]);
        fft.four2a_c2c(wave.data(), n, -1, 1);
        fft.four2a_c2c(wave.data(), n, 1, 1);
        for (int i = 0; i < n; ++i) require(std::abs(creal(wave[i]) / n - input[i]) < 1.e-10, "FFT complex round trip");
    }
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            QFile f(argv[i]); require(f.open(QIODevice::ReadOnly), "fixture open");
            QByteArray bytes = f.readAll();
            QVector<double> samples(bytes.size() / sizeof(double));
            std::memcpy(samples.data(), bytes.constData(), samples.size() * sizeof(double));
            auto r = request(samples.size() < 100000 ? "FT4" : "FT8");
            FtReferenceRecovery decoder;
            auto result = decoder.decode(samples, r);
            QJsonArray rows;
            for (const auto &d : result.rows) rows.append(QJsonObject{{"message",d.message},{"frequency",d.frequency},{"method",d.method},{"quality",d.quality}});
            std::cout << QJsonDocument(QJsonObject{{"file",QString(argv[i])},{"ms",result.elapsedMs},{"cutoff",result.timedOut},{"rejected",result.rejected},{"rows",rows}}).toJson(QJsonDocument::Compact).constData() << std::endl;
        }
        return 0;
    }
    fftRegression();
    for (const QString mode : {QString("FT8"), QString("FT4")}) {
        auto r = request(mode);
        const QString expected = "CQ IZ6NNH JN63";
        const auto samples = signal(mode, expected, 1500);
        FtReferenceRecovery decoder;
        QSet<QString> published;
        auto result = decoder.decode(samples, r, [&](const auto &row) { published.insert(row.message); });
        require(messages(result).contains(expected), "native blind decode lost generated message");
        require(messages(result) == QSet<QString>{expected}, "unexpected message on generated signal");
        require(published == messages(result), "published results lost");
        if (!qEnvironmentVariableIsSet("ASAN_OPTIONS")) require(!result.timedOut, "clean offline decode timed out");
        auto duplicate = decoder.decode(samples, r);
        require((result.timedOut || duplicate.cached) && messages(duplicate) == published, "identical input cache");
        r.offline = false; r.phase = "boundary";
        r.deadlineUtcMs = QDateTime::currentMSecsSinceEpoch() - 1;
        auto expired = decoder.decode(samples, r);
        require(expired.cached && expired.timedOut && messages(expired) == published, "gate results lost on expired boundary");
        r.epoch++;
        require(decoder.decode(samples, r).rows.isEmpty(), "stale generation reused");
        r.offline = true; r.phase = "offline";
        r.cancelled = [] { return true; };
        require(decoder.decode(samples, r).rows.isEmpty(), "cancelled job published");
        r.cancelled = {};
        r.slotUtcMs += 30000;
        QVector<double> silence(samples.size());
        require(decoder.decode(silence, r).rows.isEmpty(), "silence decoded");
        std::mt19937 rng(426);
        std::normal_distribution<double> noise(0., .05);
        for (int seed = 0; seed < 3; ++seed) {
            QVector<double> random(samples.size());
            for (auto &v : random) v = noise(rng);
            r.slotUtcMs += 30000;
            require(decoder.decode(random, r).rows.isEmpty(), "noise decoded");
        }
        // Real TX construction/generation must acquire the shared message codec
        // while the new native recovery is active, without waiting for its job.
        r.slotUtcMs += 30000;
        QElapsedTimer overlap; overlap.start();
        auto receive = std::async(std::launch::async, [&] { return decoder.decode(samples, r); });
        while (overlap.elapsed() < 15) std::this_thread::yield();
        QElapsedTimer txTime; txTime.start();
        Ft8Transmitter concurrentTx(mode, "CQ IZ6NNH JN63", 48000, 1500);
        QVector<float> txSamples(4800);
        require(concurrentTx.generate(txSamples.data(), txSamples.size()) > 0, "concurrent TX generated no audio");
        require(txTime.elapsed() < 750, "native recovery blocked the shared TX codec");
        receive.get();
        // Cancellation must interrupt an active native job, not just its entry.
        std::atomic<bool> cancel{false};
        r.cancelled = [&] { return cancel.load(); };
        r.slotUtcMs += 30000;
        QElapsedTimer wall; wall.start();
        auto future = std::async(std::launch::async, [&] { return decoder.decode(samples, r); });
        while (wall.elapsed() < 15) std::this_thread::yield();
        cancel = true; future.get();
        require(wall.elapsed() < (qEnvironmentVariableIsSet("ASAN_OPTIONS") ? 7500 : 750), "active cancellation stalled");
        std::cout << mode.toStdString() << " blind signal, cache, expired boundary, generation, cancel, silence/noise PASS\n";
    }
    // Distinct FT4 instances run simultaneously with different AP/marker state.
    auto a = request("FT4"), b = request("FT4");
    a.markerHz = 700; b.markerHz = 2400; b.myCall = "DL1ABC"; b.dxCall = "W1AW";
    auto sa = signal("FT4", "CQ IZ6NNH JN63", a.markerHz);
    auto sb = signal("FT4", "CQ DL1ABC JO31", b.markerHz);
    FtReferenceRecovery da, db;
    auto fa = std::async(std::launch::async, [&] { return da.decode(sa,a); });
    auto fb = std::async(std::launch::async, [&] { return db.decode(sb,b); });
    require(messages(fa.get()).contains("CQ IZ6NNH JN63"), "parallel FT4 A");
    require(messages(fb.get()).contains("CQ DL1ABC JO31"), "parallel FT4 B");
    std::cout << "FFT and concurrent FT4 instance isolation PASS\n";
}
