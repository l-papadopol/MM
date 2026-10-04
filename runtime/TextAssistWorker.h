#ifndef TEXTASSISTWORKER_H
#define TEXTASSISTWORKER_H

#include "../modems/rtty/contest/RttyContestRules.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>

/**
 * @brief Background parser for all text-producing RX modes.
 *
 * This worker knows how to recognize callsigns, locator/callsign pairs and
 * generic contest exchange candidates. It never touches GUI objects or the logbook.
 * Logbook membership is resolved separately by
 * LogbookIndexWorker so the decoder and GUI threads remain independent.
 */
class TextAssistWorker final : public QObject
{
    Q_OBJECT
public:
    explicit TextAssistWorker(QObject *parent = nullptr);

    void analyze(quint64 requestId,
                 const QString &consumerId,
                 const QString &text,
                 int basePosition,
                 const QString &mode,
                 const QString &ownCall,
                 bool contestEnabled,
                 const RttyContestProfile &contestProfile);

signals:
    void analysisReady(quint64 requestId,
                       const QString &consumerId,
                       const QVariantList &annotations,
                       const QVariantMap &contestCandidate,
                       const QVariantList &heardStations);

private:
    static QString cleanToken(const QString &raw);
    static bool tokenMatchesField(const RttyContestFieldRule &field, const QString &token);

    QHash<QString, QString> m_lastHeardKey;
};

#endif // TEXTASSISTWORKER_H
