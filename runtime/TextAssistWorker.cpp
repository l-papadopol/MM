#include "TextAssistWorker.h"

#include "../logbook/AdifLogbook.h"

#include <QRegularExpression>
#include <QSet>

TextAssistWorker::TextAssistWorker(QObject *parent)
    : QObject(parent)
{
}

QString TextAssistWorker::cleanToken(const QString &raw)
{
    QString token = raw.trimmed().toUpper();
    while (!token.isEmpty() && !token.front().isLetterOrNumber()) token.remove(0, 1);
    while (!token.isEmpty() && !token.back().isLetterOrNumber()) token.chop(1);
    return token;
}

bool TextAssistWorker::tokenMatchesField(const RttyContestFieldRule &field, const QString &token)
{
    if (token.isEmpty()) return false;
    if (!field.regex.isEmpty()) {
        return QRegularExpression(field.regex, QRegularExpression::CaseInsensitiveOption).match(token).hasMatch();
    }
    if (field.type == QStringLiteral("rst"))
        return QRegularExpression(QStringLiteral("^[1-5][1-9][1-9]$")).match(token).hasMatch();
    if (field.type == QStringLiteral("serial"))
        return QRegularExpression(QStringLiteral("^\\d{1,6}$")).match(token).hasMatch();
    if (field.type == QStringLiteral("locator"))
        return QRegularExpression(QStringLiteral("^[A-R]{2}\\d{2}(?:[A-X]{2})?$"), QRegularExpression::CaseInsensitiveOption).match(token).hasMatch();
    if (field.type == QStringLiteral("zone"))
        return QRegularExpression(QStringLiteral("^\\d{1,2}$")).match(token).hasMatch();
    if (field.type == QStringLiteral("year"))
        return QRegularExpression(QStringLiteral("^(?:19|20)?\\d{2}$")).match(token).hasMatch();
    if (field.type == QStringLiteral("time"))
        return QRegularExpression(QStringLiteral("^\\d{4}$")).match(token).hasMatch();
    if (field.type == QStringLiteral("age"))
        return QRegularExpression(QStringLiteral("^\\d{1,3}$")).match(token).hasMatch();
    if (field.type == QStringLiteral("name"))
        return QRegularExpression(QStringLiteral("^[A-Z]{2,12}$")).match(token).hasMatch();
    return false;
}

void TextAssistWorker::analyze(quint64 requestId,
                               const QString &consumerId,
                               const QString &text,
                               int basePosition,
                               const QString &mode,
                               const QString &ownCall,
                               bool contestEnabled,
                               const RttyContestProfile &contestProfile)
{
    QVariantList annotations;
    QVariantList heardStations;
    QVariantMap contestCandidate;
    if (text.isEmpty()) {
        emit analysisReady(requestId, consumerId, annotations, contestCandidate, heardStations);
        return;
    }

    const QString upper = text.toUpper();
    const QRegularExpression callRe(
        QStringLiteral("(?<![A-Z0-9/])(?:[A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?(?![A-Z0-9/])"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator calls = callRe.globalMatch(upper);
    while (calls.hasNext()) {
        const QRegularExpressionMatch match = calls.next();
        const QString call = AdifLogbook::normalizeCallsign(match.captured(0));
        if (call.size() < 3) continue;
        QVariantMap annotation;
        annotation.insert(QStringLiteral("kind"), QStringLiteral("call"));
        annotation.insert(QStringLiteral("start"), basePosition + match.capturedStart());
        annotation.insert(QStringLiteral("length"), match.capturedLength());
        annotation.insert(QStringLiteral("value"), call);
        annotations.push_back(annotation);
    }

    // Generic heard-station extraction for textual modes.  Only the most
    // recent call/grid pair is emitted, and only when it changes, so a 160 ms
    // assist refresh cannot repeatedly repaint the QSO map for old text.
    const QRegularExpression tokenRe(QStringLiteral("\\b([A-Z0-9/]{3,16})\\b"));
    QRegularExpressionMatchIterator tokenIt = tokenRe.globalMatch(upper);
    QStringList tokens;
    while (tokenIt.hasNext()) tokens.push_back(tokenIt.next().captured(1));
    const QRegularExpression locatorRe(QStringLiteral("^[A-R]{2}\\d{2}(?:[A-X]{2})?$"), QRegularExpression::CaseInsensitiveOption);
    QString heardCall;
    QString heardGrid;
    for (int i = tokens.size() - 1; i >= 0 && heardCall.isEmpty(); --i) {
        const QString call = AdifLogbook::normalizeCallsign(tokens.at(i));
        if (!callRe.match(call).hasMatch()) continue;
        for (int j = i + 1; j < tokens.size() && j <= i + 6; ++j) {
            const QString locator = cleanToken(tokens.at(j));
            if (!locatorRe.match(locator).hasMatch()) continue;
            heardCall = call;
            heardGrid = locator;
            break;
        }
    }
    if (!heardCall.isEmpty()) {
        const QString heardKey = heardCall + QStringLiteral("|") + heardGrid + QStringLiteral("|") + mode;
        if (m_lastHeardKey.value(consumerId) != heardKey) {
            m_lastHeardKey.insert(consumerId, heardKey);
            QVariantMap heard;
            heard.insert(QStringLiteral("call"), heardCall);
            heard.insert(QStringLiteral("grid"), heardGrid);
            heard.insert(QStringLiteral("mode"), mode);
            heardStations.push_back(heard);
        }
    }

    if (contestEnabled) {
        const QRegularExpression locatorGlobal(QStringLiteral("\\b[A-R]{2}\\d{2}(?:[A-X]{2})?\\b"),
                                               QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator locIt = locatorGlobal.globalMatch(upper);
        while (locIt.hasNext()) {
            const QRegularExpressionMatch match = locIt.next();
            QVariantMap annotation;
            annotation.insert(QStringLiteral("kind"), QStringLiteral("locator"));
            annotation.insert(QStringLiteral("start"), basePosition + match.capturedStart());
            annotation.insert(QStringLiteral("length"), match.capturedLength());
            annotation.insert(QStringLiteral("value"), match.captured(0).toUpper());
            annotations.push_back(annotation);
        }

        const QString myCall = AdifLogbook::normalizeCallsign(ownCall);
        if (!myCall.isEmpty()) {
            const QRegularExpression ownRe(QStringLiteral("(?<![A-Z0-9/])%1(?![A-Z0-9/])")
                                               .arg(QRegularExpression::escape(myCall)),
                                           QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator ownIt = ownRe.globalMatch(upper);
            int ownPos = -1;
            while (ownIt.hasNext()) ownPos = ownIt.next().capturedStart();
            if (ownPos >= 0) {
                int segmentStart = upper.lastIndexOf(QLatin1Char('\n'), ownPos);
                segmentStart = segmentStart < 0 ? qMax(0, ownPos - 192) : segmentStart + 1;
                int segmentEnd = upper.indexOf(QLatin1Char('\n'), ownPos);
                segmentEnd = segmentEnd < 0 ? qMin(upper.size(), ownPos + 320) : segmentEnd;
                if (segmentEnd - segmentStart > 512) segmentStart = qMax(segmentStart, segmentEnd - 512);
                const QString segment = upper.mid(segmentStart, segmentEnd - segmentStart);

                QStringList segmentCalls;
                QRegularExpressionMatchIterator segmentCallIt = callRe.globalMatch(segment);
                while (segmentCallIt.hasNext()) {
                    const QString call = AdifLogbook::normalizeCallsign(segmentCallIt.next().captured(0));
                    if (!call.isEmpty()) segmentCalls.push_back(call);
                }
                QString dxCall;
                for (const QString &call : segmentCalls) {
                    if (call.compare(myCall, Qt::CaseInsensitive) != 0) {
                        dxCall = call;
                        break;
                    }
                }
                contestCandidate.insert(QStringLiteral("dxCall"), dxCall);

                const QStringList words = segment.split(QRegularExpression(QStringLiteral("[^A-Z0-9/]+")), Qt::SkipEmptyParts);
                QStringList cleanWords;
                cleanWords.reserve(words.size());
                for (const QString &raw : words) {
                    const QString token = cleanToken(raw);
                    if (!token.isEmpty()) cleanWords.push_back(token);
                }
                QSet<int> used;
                QVariantMap fields;
                for (const RttyContestFieldRule &field : contestProfile.receivedFields) {
                    if (field.type == QStringLiteral("call")) continue;
                    QString value;
                    for (int i = 0; i < cleanWords.size(); ++i) {
                        if (used.contains(i)) continue;
                        const QString token = cleanWords.at(i);
                        if (token == myCall || token == dxCall) continue;
                        if (field.id == QStringLiteral("QTH") && used.isEmpty()) continue;
                        if (tokenMatchesField(field, token)) {
                            used.insert(i);
                            value = token;
                            break;
                        }
                    }
                    if (!value.isEmpty()) fields.insert(field.id, value);
                }
                contestCandidate.insert(QStringLiteral("fields"), fields);
            }
        }
    }

    emit analysisReady(requestId, consumerId, annotations, contestCandidate, heardStations);
}
