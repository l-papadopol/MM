#pragma once
#include <QString>
#include <QRegularExpression>

// Per-station token parser shared by CW and RTTY. It sees committed text only.
class SkimmerCqTracker {
public:
    void reset() { *this = SkimmerCqTracker(); }
    QString call() const { return m_call; }
    void expire(double now) {
        if (now - m_cqTime > 12) m_call.clear();
        if (now - m_lastTextTime > 8) {
            m_tokens = 0; m_candidate.clear(); m_repetitions = 0;
            m_call.clear(); m_token.clear();
        }
    }
    void append(const QString& text, double now) {
        if (!text.trimmed().isEmpty()) m_lastTextTime = now;
        for (QChar c : text.toUpper()) {
            if (!c.isSpace()) { m_token += c; if (m_token.size() > 20) m_token = QStringLiteral("?"); continue; }
            if (m_token.isEmpty()) continue;
            const QString word = m_token; m_token.clear();
            if (word == QLatin1String("CQ")) {
                m_tokens = 16; continue;
            }
            if (word == QLatin1String("QRZ") || word == QLatin1String("73")) {
                m_tokens = 0; m_call.clear(); m_candidate.clear(); m_repetitions = 0;
            }
            if (m_tokens <= 0) continue;
            --m_tokens;
            static const QRegularExpression callPattern(QStringLiteral("^(?:[A-Z0-9]{1,4}/)?[A-Z0-9]{1,4}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?$"));
            static const QRegularExpression gridPattern(QStringLiteral("^[A-R]{2}[0-9]{2}(?:[A-X]{2})?$"));
            if (!callPattern.match(word).hasMatch() || gridPattern.match(word).hasMatch()) continue;
            if (m_candidate.isEmpty()) {
                if (m_call != word) m_call.clear();
                m_candidate = word; m_repetitions = 1;
            } else if (m_candidate == word) ++m_repetitions;
            else { m_tokens = 0; m_candidate.clear(); m_repetitions = 0; m_call.clear(); }
            if (m_repetitions >= 2) { m_call = m_candidate; m_cqTime = now; }
        }
    }
private:
    QString m_token, m_candidate, m_call;
    int m_tokens = 0, m_repetitions = 0;
    double m_lastTextTime = 0, m_cqTime = -100;
};
