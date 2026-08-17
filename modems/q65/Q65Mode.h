#ifndef Q65MODE_H
#define Q65MODE_H

#include <QString>
#include <QStringList>
#include <Qt>

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
};

#endif // Q65MODE_H
