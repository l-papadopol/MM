#include "FtDecodedText.h"

#include <QRegularExpression>
#include <QtGlobal>

namespace {

QString normalizeMessage(QString text)
{
    const int nbsp = text.indexOf(QChar::Nbsp);
    if (nbsp >= 0) {
        text = text.left(nbsp);
    }

    text = text.trimmed().toUpper();

    // Accept either bare FT message text or a WSJT-X formatted decode line.
    // WSJT-X decodedtext.cpp uses column 22 (+ optional seconds padding).  A
    // regex fallback is safer for MadModem tests/log imports where spacing may
    // not be byte-identical to WSJT-X display output.
    thread_local const QRegularExpression formattedLine(
        QStringLiteral("^\\s*\\d{4}(?:\\d{2})?\\s+[-+ ]?\\d+\\s+[-+]?\\d+(?:\\.\\d+)?\\s+\\d+\\s+[#@~&+ ]\\s+(.*)$"));
    const QRegularExpressionMatch fm = formattedLine.match(text);
    if (fm.hasMatch()) {
        text = fm.captured(1).trimmed();
    }

    // WSJT-X may append AP/confidence tags to formatted display lines.  Strip
    // only a trailing designator token; never scan the whole message because
    // valid callsigns can contain A/Q followed by a digit.
    text.replace(QRegularExpression(QStringLiteral("\\s+(?:\\?\\s*)?[AQ][0-9]\\s*$")), QString());
    text.remove(QLatin1Char('<'));
    text.remove(QLatin1Char('>'));
    const int cr = text.indexOf(QLatin1Char('\r'));
    if (cr > 0) {
        text = text.left(cr);
    }
    text = text.left(37).trimmed();
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text;
}

const QRegularExpression &tokensRe()
{
    // Direct port of the grammar used by WSJT-X 3.0.1 DecodedText.  We do not
    // blindly trust a match as a decodable standard message without pack/unpack,
    // but the positional captures are the right basis for UI/sequencer logic.
    thread_local const QRegularExpression re(QStringLiteral(R"(
^
  (?:(?<dual>[A-Z0-9/]+)\sRR73;\s)?
  (?:
    (?<word1>
      (?:CQ|DE|QRZ)
      (?:\s?DX|\s(?:[A-Z]{1,4}|\d{3}))?
      | [A-Z0-9/]+
      | \.{3}
    )\s
  )
  (?:
    (?<word2>[A-Z0-9/]+)
    (?:\s
      (?<word3>[-+A-Z0-9]+)
      (?:\s
        (?<word4>
          (?:
            OOO
            | (?!RR73)[A-R]{2}[0-9]{2}
            | 5[0-9]{5}
          )
        )
        (?:\s
          (?<word5>[A-R]{2}[0-9]{2}[A-X]{2})
        )?
      )?
    )?
  )?
)"), QRegularExpression::ExtendedPatternSyntaxOption);
    return re;
}

QStringList splitWords(const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#else
    return text.split(QLatin1Char(' '), QString::SkipEmptyParts);
#endif
}

bool hasDigit(const QString &s)
{
    for (const QChar ch : s) {
        if (ch.isDigit()) {
            return true;
        }
    }
    return false;
}

bool containsLetter(const QString &s)
{
    for (const QChar ch : s) {
        if (ch.isLetter()) {
            return true;
        }
    }
    return false;
}


bool looksLikeContestExchange(const QStringList &words, int fromIndex)
{
    if (fromIndex < 0 || fromIndex >= words.size()) {
        return false;
    }
    QStringList payload;
    for (int i = fromIndex; i < words.size(); ++i) {
        const QString w = words.at(i).trimmed().toUpper();
        if (w.isEmpty()) {
            continue;
        }
        if (w == QStringLiteral("RR73") || w == QStringLiteral("RRR") || w == QStringLiteral("73") ||
            w == QStringLiteral("CQ") || w == QStringLiteral("DE") || w == QStringLiteral("QRZ")) {
            return false;
        }
        payload << w;
    }
    if (payload.isEmpty() || payload.size() > 3) {
        return false;
    }
    const QString joined = payload.join(QStringLiteral(" "));
    if (joined.size() > 16) {
        return false;
    }
    // Contest/field-day/portable exchanges are not free text: they are short,
    // positional, and normally contain at least one digit or an R+locator ACK.
    if (payload.first() == QStringLiteral("R") && payload.size() >= 2) {
        return hasDigit(payload.at(1)) || payload.at(1).size() <= 4;
    }
    if (!hasDigit(joined)) {
        return false;
    }
    thread_local const QRegularExpression allowed(QStringLiteral("^[A-Z0-9/+ -]+$"));
    return allowed.match(joined).hasMatch();
}

} // namespace

FtDecodedText::ParsedMessage FtDecodedText::parse(const QString &message, const QString &myCall)
{
    ParsedMessage parsed;
    parsed.text = normalizeMessage(message);
    parsed.cleanText = parsed.text;
    parsed.qsoText = parsed.text;
    parsed.parts = splitWords(parsed.text);

    if (parsed.parts.isEmpty()) {
        return parsed;
    }

    const QRegularExpressionMatch match = tokensRe().match(parsed.text);
    parsed.standardLike = match.hasMatch();
    if (parsed.standardLike) {
        parsed.dualReplyCall = match.captured(QStringLiteral("dual")).trimmed();
        parsed.word1 = match.captured(QStringLiteral("word1")).trimmed();
        parsed.word2 = match.captured(QStringLiteral("word2")).trimmed();
        parsed.word3 = match.captured(QStringLiteral("word3")).trimmed();
        parsed.word4 = match.captured(QStringLiteral("word4")).trimmed();
        parsed.word5 = match.captured(QStringLiteral("word5")).trimmed();
    } else {
        parsed.word1 = parsed.parts.value(0);
        parsed.word2 = parsed.parts.value(1);
        parsed.word3 = parsed.parts.value(2);
        parsed.word4 = parsed.parts.value(3);
        parsed.word5 = parsed.parts.value(4);
    }

    parsed.qrz = (parsed.word1 == QStringLiteral("QRZ"));
    parsed.de = (parsed.word1 == QStringLiteral("DE"));
    parsed.cq = parsed.word1.startsWith(QStringLiteral("CQ")) || parsed.qrz;
    parsed.rr73 = parsed.parts.contains(QStringLiteral("RR73"));
    parsed.rrr = parsed.parts.contains(QStringLiteral("RRR"));
    parsed.final73 = parsed.rr73 || parsed.rrr || parsed.parts.contains(QStringLiteral("73"));

    const QString my = myCall.trimmed().toUpper();
    if (!my.isEmpty()) {
        for (const QString &part : parsed.parts) {
            if (callMatches(part, my)) {
                parsed.containsMyCall = true;
                break;
            }
        }
    }

    if (parsed.cq) {
        parsed.kind = Kind::Cq;
        parsed.firstCall = parsed.word2;
        if (isCallsign(parsed.firstCall)) {
            parsed.senderCall = parsed.firstCall;
            parsed.fromCall = parsed.senderCall;
            parsed.payload = parsed.word3;
            if (isGrid(parsed.payload)) {
                parsed.grid = parsed.payload.left(4);
            }
        } else {
            // Directed CQ forms are captured in word1 (CQ DX, CQ EU, CQ 001).
            // The first real callsign following the CQ token is the caller.
            for (int i = 1; i < parsed.parts.size(); ++i) {
                if (isCallsign(parsed.parts.at(i))) {
                    parsed.firstCall = parsed.parts.at(i);
                    parsed.senderCall = parsed.firstCall;
                    parsed.fromCall = parsed.senderCall;
                    if (i + 1 < parsed.parts.size() && isGrid(parsed.parts.at(i + 1))) {
                        parsed.grid = parsed.parts.at(i + 1).left(4);
                    }
                    break;
                }
            }
        }
        return parsed;
    }

    parsed.firstCall = parsed.word1;
    parsed.secondCall = parsed.word2;

    if (isCallsign(parsed.firstCall) && isCallsign(parsed.secondCall)) {
        parsed.directed = true;
        parsed.receiverCall = parsed.firstCall;
        parsed.toCall = parsed.receiverCall;
        parsed.senderCall = parsed.secondCall;
        parsed.fromCall = parsed.senderCall;
        parsed.payload = parsed.word3;

        if (!my.isEmpty()) {
            // Standard FT messages are ordered TO FROM PAYLOAD.  Only word1 is
            // addressed to us.  If our call is word2, it is most likely our own
            // transmitted message/echo and must not drive auto-sequence.
            parsed.addressedToMe = callMatches(parsed.receiverCall, my);
        }

        if (isFinalToken(parsed.payload)) {
            parsed.kind = Kind::DirectedFinal;
        } else if (isReport(parsed.payload)) {
            parsed.rawReport = parsed.payload;
            parsed.report = cleanReport(parsed.payload);
            parsed.rReport = isRReport(parsed.payload);
            parsed.kind = parsed.rReport ? Kind::DirectedRReport : Kind::DirectedReport;
        } else if (parsed.payload == QStringLiteral("R") && isReport(parsed.word4)) {
            parsed.rawReport = QStringLiteral("R") + cleanReport(parsed.word4);
            parsed.report = cleanReport(parsed.word4);
            parsed.rReport = true;
            parsed.kind = Kind::DirectedRReport;
        } else if (parsed.payload == QStringLiteral("R") && !parsed.word4.isEmpty()) {
            // Contest-style acknowledgement, e.g. "MYCALL DXCALL R FN42"
            // or "MYCALL DXCALL R 1A".  Treat it as an ACK/exchange only in
            // positional form; never let tokens like RR79 become a locator.
            parsed.contestAck = true;
            parsed.contestExchangeLike = true;
            QStringList exchangeWords;
            if (!parsed.word3.isEmpty()) exchangeWords << parsed.word3;
            if (!parsed.word4.isEmpty()) exchangeWords << parsed.word4;
            if (!parsed.word5.isEmpty()) exchangeWords << parsed.word5;
            parsed.contestExchange = exchangeWords.join(QStringLiteral(" ")).trimmed();
            if (isGrid(parsed.word4)) {
                parsed.grid = parsed.word4.left(4);
            }
            parsed.kind = Kind::DirectedContestExchange;
        } else if (isGrid(parsed.payload)) {
            parsed.grid = parsed.payload.left(4);
            parsed.kind = Kind::DirectedGrid;
        } else if (looksLikeContestExchange(parsed.parts, 2)) {
            // Field Day/contest exchanges such as "MYCALL DXCALL 1A WI" are
            // valid directed payloads but are not ordinary FT reports/locators.
            parsed.contestExchangeLike = true;
            parsed.contestExchange = parsed.parts.mid(2).join(QStringLiteral(" ")).trimmed();
            if (isGrid(parsed.word4)) {
                parsed.grid = parsed.word4.left(4);
            } else if (isGrid(parsed.word5)) {
                parsed.grid = parsed.word5.left(4);
            }
            parsed.kind = Kind::DirectedContestExchange;
        } else {
            parsed.kind = Kind::DirectedOther;
        }

        if (parsed.report.isEmpty()) {
            // Contest/EME variants keep extra report/grid fields.  Accept them
            // only from positional captures, never via free scanning.
            if (isReport(parsed.word4)) {
                parsed.rawReport = parsed.word4;
                parsed.report = cleanReport(parsed.word4);
                parsed.rReport = isRReport(parsed.word4);
                parsed.kind = parsed.rReport ? Kind::DirectedRReport : Kind::DirectedReport;
            } else if (parsed.grid.isEmpty() && isGrid(parsed.word4)) {
                parsed.grid = parsed.word4.left(4);
            } else if (parsed.grid.isEmpty() && isGrid(parsed.word5)) {
                parsed.grid = parsed.word5.left(4);
            }
        }

        return parsed;
    }

    parsed.kind = parsed.final73 ? Kind::FreeText73 : Kind::FreeText;
    if (!my.isEmpty()) {
        parsed.addressedToMe = parsed.containsMyCall;
    }
    return parsed;
}

QStringList FtDecodedText::messageWords(const QString &message)
{
    const ParsedMessage p = parse(message);
    QStringList result;
    result << p.cleanText << p.dualReplyCall << p.word1 << p.word2 << p.word3 << p.word4 << p.word5;
    return result;
}

QString FtDecodedText::cqersCall(const QString &message)
{
    return parse(message).firstCall;
}

bool FtDecodedText::isFinalToken(const QString &token)
{
    const QString t = token.trimmed().toUpper();
    return t == QStringLiteral("RR73") || t == QStringLiteral("RRR") || t == QStringLiteral("73");
}

bool FtDecodedText::isAckLikeGridTrap(const QString &token)
{
    const QString t = token.trimmed().toUpper();
    thread_local const QRegularExpression rrDigits(QStringLiteral("^RR[0-9]{2}$"));
    return isFinalToken(t) || rrDigits.match(t).hasMatch();
}

bool FtDecodedText::isGrid(const QString &token)
{
    const QString t = token.trimmed().toUpper();
    if (isAckLikeGridTrap(t) || isReport(t) || t == QStringLiteral("R")) {
        return false;
    }
    thread_local const QRegularExpression grid4(QStringLiteral("^[A-R]{2}[0-9]{2}$"));
    thread_local const QRegularExpression grid6(QStringLiteral("^[A-R]{2}[0-9]{2}[A-X]{2}$"));
    return grid4.match(t).hasMatch() || grid6.match(t).hasMatch();
}

bool FtDecodedText::isReport(const QString &token)
{
    const QString t = token.trimmed().toUpper();
    thread_local const QRegularExpression re(QStringLiteral("^R?[+-][0-9]{2}$"));
    if (!re.match(t).hasMatch()) {
        return false;
    }
    bool ok = false;
    const int v = cleanReport(t).toInt(&ok);
    return ok && v >= -50 && v <= 49;
}

bool FtDecodedText::isRReport(const QString &token)
{
    const QString t = token.trimmed().toUpper();
    return t.startsWith(QLatin1Char('R')) && isReport(t);
}

bool FtDecodedText::isCallsign(const QString &token)
{
    const QString t = token.trimmed().toUpper();
    if (t.isEmpty() || t == QStringLiteral("CQ") || t == QStringLiteral("DE") || t == QStringLiteral("QRZ") ||
        isReport(t) || isGrid(t) || isFinalToken(t) || isAckLikeGridTrap(t) || t == QStringLiteral("OOO")) {
        return false;
    }
    if (!hasDigit(t) || !containsLetter(t)) {
        return false;
    }

    thread_local const QRegularExpression normal(QStringLiteral("^(?:[A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?$"));
    if (normal.match(t).hasMatch()) {
        return true;
    }

    thread_local const QRegularExpression compound(QStringLiteral("^[A-Z0-9]+(?:/[A-Z0-9]+)+$"));
    return compound.match(t).hasMatch();
}

QString FtDecodedText::cleanReport(QString report)
{
    report = report.trimmed().toUpper();
    if (report.startsWith(QLatin1Char('R')) && report.size() >= 4 &&
        (report.at(1) == QLatin1Char('+') || report.at(1) == QLatin1Char('-'))) {
        report.remove(0, 1);
    }
    return report;
}

QString FtDecodedText::formatSignalReport(int snrDb, bool acknowledged)
{
    const int bounded = qBound(-50, snrDb, 49);
    const QChar sign = bounded >= 0 ? QChar('+') : QChar('-');
    const QString value = QString("%1%2").arg(sign).arg(qAbs(bounded), 2, 10, QChar('0'));
    return acknowledged ? QStringLiteral("R") + value : value;
}

QString FtDecodedText::baseCall(QString callsign)
{
    callsign = callsign.trimmed().toUpper();
    callsign.remove(QLatin1Char('<'));
    callsign.remove(QLatin1Char('>'));

    if (!callsign.contains(QLatin1Char('/'))) {
        return callsign;
    }

    const QStringList pieces = callsign.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString best;
    for (const QString &piece : pieces) {
        if (isCallsign(piece) && piece.size() > best.size()) {
            best = piece;
        }
    }
    return best.isEmpty() ? callsign : best;
}

bool FtDecodedText::callMatches(const QString &a, const QString &b)
{
    const QString aa = a.trimmed().toUpper();
    const QString bb = b.trimmed().toUpper();
    if (aa.isEmpty() || bb.isEmpty()) {
        return false;
    }
    if (aa == bb || aa.endsWith(QStringLiteral("/") + bb) || aa.startsWith(bb + QStringLiteral("/")) ||
        bb.endsWith(QStringLiteral("/") + aa) || bb.startsWith(aa + QStringLiteral("/"))) {
        return true;
    }
    return baseCall(aa) == baseCall(bb);
}
