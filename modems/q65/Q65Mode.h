#ifndef Q65MODE_H
#define Q65MODE_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cmath>

class Q65Mode
{
public:
    enum class Submode { A = 1, B = 2, C = 4, D = 8 };

    static QString familyName() { return QStringLiteral("Q65"); }
    static QString modeName(Submode s)
    {
        switch (s) {
        case Submode::A: return QStringLiteral("Q65A");
        case Submode::B: return QStringLiteral("Q65B");
        case Submode::C: return QStringLiteral("Q65C");
        case Submode::D: return QStringLiteral("Q65D");
        }
        return QStringLiteral("Q65A");
    }
    static QStringList allModeNames()
    {
        return { modeName(Submode::A), modeName(Submode::B), modeName(Submode::C), modeName(Submode::D) };
    }
    static bool isFamilyMode(const QString &mode)
    {
        const QString m = mode.trimmed().toUpper();
        return m == QStringLiteral("Q65") || m == QStringLiteral("Q65A") || m == QStringLiteral("Q65B") || m == QStringLiteral("Q65C") || m == QStringLiteral("Q65D");
    }
    static Submode submodeForMode(const QString &mode)
    {
        const QString m = mode.trimmed().toUpper();
        if (m == QStringLiteral("Q65B")) return Submode::B;
        if (m == QStringLiteral("Q65C")) return Submode::C;
        if (m == QStringLiteral("Q65D")) return Submode::D;
        return Submode::A;
    }
    static int mshvToneSpacingMultiplier(Submode s)
    {
        return static_cast<int>(s); // MSHV: A=1, B=2, C=4, D=8
    }
    static QString adifMode() { return QStringLiteral("Q65"); }
    static int defaultPeriodSeconds() { return 60; }
    static int defaultRxFrequencyHz() { return 1500; }
    static int defaultTxFrequencyHz() { return 1500; }
    static double baud(int periodSeconds)
    {
        switch (periodSeconds) {
        case 15: return 12000.0 / 1800.0;
        case 30: return 12000.0 / 3600.0;
        case 120: return 12000.0 / 16000.0;
        case 60:
        default: return 12000.0 / 7200.0;
        }
    }
    static int minimumBaseToneHz(Submode submode, int periodSeconds, int sampleRate = 12000)
    {
        const int multiplier = mshvToneSpacingMultiplier(submode);
        // q65_intrinsics_fastfading consumes 64 guard bins below data tone 0.
        // Preserve two percent of the sample rate above DC for the real-input
        // resampler/filter transition as well.
        const double lowestOffset = (64.0 - multiplier) * baud(periodSeconds);
        return qMax(100, static_cast<int>(std::ceil(0.02 * sampleRate + lowestOffset)));
    }
    static int maximumBaseToneHz(Submode submode, int periodSeconds, int sampleRate = 12000)
    {
        const int multiplier = mshvToneSpacingMultiplier(submode);
        // The soft QRA metric uses 64*(2+multiplier) bins per data symbol, not
        // only the 65 transmitted tones. Keep the complete upper fading window
        // below 0.48*Fs; accepting only the actual tone would silently clip the
        // metric in the widest/fastest submodes.
        const double highestMetricOffset = (63.0 + 65.0 * multiplier) *
                                           baud(periodSeconds);
        const int completeWindowLimit = static_cast<int>(
            std::floor(0.48 * sampleRate - highestMetricOffset));
        return qMax(minimumBaseToneHz(submode, periodSeconds, sampleRate),
                    qMin(2700, completeWindowLimit));
    }
};

#endif // Q65MODE_H
