#include "audio/TxOutputDevice.h"
#include <QCoreApplication>
#include <algorithm>
#include <iostream>

class Tone final : public TxModulator {
public:
    explicit Tone(int tail) : tail(tail) {}
    int sampleRate() const override { return 48000; }
    int generate(float *out, int count) override {
        const int n = std::min(remaining, count);
        std::fill_n(out, n, 0.5f); remaining -= n; return n;
    }
    bool isFinished() const override { return remaining == 0; }
    double progress() const override { return isFinished() ? 1.0 : 0.0; }
    QImage previewImage() const override { return {}; }
    QString description() const override { return "test"; }
    int trailingSilenceSamples() const override { return tail; }
    int remaining = 4096, tail;
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    for (int tail : {0, 17, 16000}) {
        Tone tone(tail);
        TxOutputDevice source(&tone, 48000, 100);
        source.start();
        bool availableBeforeRead = source.bytesAvailable() > 0 && !source.atEnd();
        QByteArray pcm;
        char buffer[8192];
        for (int i = 0; i < 20; ++i) {
            // Emulate a backend that will not call read() on an empty source.
            if (source.bytesAvailable() <= 0) break;
            const auto n = source.read(buffer, sizeof buffer);
            if (n <= 0) break;
            pcm.append(buffer, static_cast<int>(n));
        }
        bool pass = availableBeforeRead && source.exhausted() && source.atEnd() && source.bytesAvailable()==0 && pcm.size() == (4096 + tail) * 2;
        for (int i = 4096 * 2; i < pcm.size(); ++i) pass &= pcm.at(i) == '\0';
        pass &= source.read(buffer, sizeof buffer) == 0;
        std::cout << (pass ? "PASS " : "FAIL ") << "finite TX source, tail=" << tail << ", bytes=" << pcm.size() << '\n';
        ok &= pass;
    }
    {
        Tone tone(0); TxOutputDevice source(&tone,48000,100);
        source.setStartGuard(1, {}); source.start(); char pcm[512];
        const bool pass = source.read(pcm,sizeof pcm)==0 && source.aborted() && tone.remaining==4096;
        std::cout << (pass?"PASS ":"FAIL ") << "expired first pull produces no PCM\n"; ok &= pass;
    }
    {
        Tone tone(0); TxOutputDevice source(&tone,48000,100); bool authorized=true;
        source.setStartGuard(0,[&](){return authorized;}); source.start(); char pcm[512];
        const bool began=source.read(pcm,sizeof pcm)>0; const int remaining=tone.remaining;
        authorized=false;
        const bool pass=began && source.read(pcm,sizeof pcm)==0 && source.aborted() && tone.remaining==remaining;
        std::cout << (pass?"PASS ":"FAIL ") << "STOP revokes subsequent PCM pulls\n"; ok &= pass;
    }
    return ok ? 0 : 1;
}
