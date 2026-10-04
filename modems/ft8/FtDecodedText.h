#ifndef FTDECODEDTEXT_H
#define FTDECODEDTEXT_H

#include <QString>
#include <QStringList>

/**
 * @brief Shared WSJT-X-style parser for decoded FT4/FT8 message text.
 *
 * This is the only FT text parser used by MadModem UI, double-click handling,
 * QSO map/logbook extraction and FtQsoSequencer.  It mirrors the practical
 * structure of WSJT-X 3.0.1 Decoder/decodedtext.*: normalize the formatted
 * decode line, split into message words using a tokens_re-compatible grammar,
 * then expose positional fields instead of scanning the whole message for any
 * token that merely looks like a grid/report.
 */
class FtDecodedText final
{
public:
    enum class Kind
    {
        Unknown,
        Cq,
        DirectedGrid,
        DirectedReport,
        DirectedRReport,
        DirectedFinal,
        DirectedContestExchange,
        DirectedOther,
        FreeText73,
        FreeText
    };

    struct ParsedMessage
    {
        QString text;        ///< Normalized bare FT message, max 37 chars.
        QString cleanText;   ///< Alias retained for older MainWindow callers.
        QString qsoText;     ///< Same as text unless a formatted decode line was supplied.
        QStringList parts;

        // WSJT-X tokens_re-compatible captured words.
        QString word1;
        QString word2;
        QString word3;
        QString word4;
        QString word5;
        QString dualReplyCall;

        // Positional standard-message fields.
        QString firstCall;       ///< Word1 for directed messages.
        QString secondCall;      ///< Word2 for directed messages.
        QString receiverCall;    ///< Standard FT direction: word1 is TO.
        QString senderCall;      ///< Standard FT direction: word2 is FROM.
        QString toCall;
        QString fromCall;
        QString grid;
        QString report;          ///< Cleaned report for operational use, no leading R.
        QString rawReport;       ///< Original payload report, e.g. R-15.
        QString payload;
        QString contestExchange; ///< Positional non-standard/contest payload, e.g. 1A WI or R FN42.

        Kind kind = Kind::Unknown;
        bool standardLike = false;
        bool cq = false;
        bool qrz = false;
        bool de = false;
        bool directed = false;
        bool addressedToMe = false;   ///< True only when the message is TO my call.
        bool containsMyCall = false;  ///< True if my call appears anywhere.
        bool final73 = false;
        bool rr73 = false;
        bool rrr = false;
        bool rReport = false;
        bool contestExchangeLike = false;
        bool contestAck = false;
    };

    static ParsedMessage parse(const QString &message, const QString &myCall = QString());
    static QStringList messageWords(const QString &message);
    static QString cqersCall(const QString &message);

    static bool isCallsign(const QString &token);
    static bool isGrid(const QString &token);
    static bool isReport(const QString &token);
    static bool isRReport(const QString &token);
    static bool isFinalToken(const QString &token);
    static bool isAckLikeGridTrap(const QString &token);

    static QString cleanReport(QString report);
    static QString formatSignalReport(int snrDb, bool acknowledged);
    static QString baseCall(QString callsign);
    static bool callMatches(const QString &a, const QString &b);
};

#endif // FTDECODEDTEXT_H
