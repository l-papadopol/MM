#include "modems/ft8/FtDecodedText.h"
#include "modems/ft8/FtQsoSequencer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QString>

namespace {

using Seq = FtQsoSequencer;

struct Expect
{
    QString name;
    QString message;
    Seq::State state = Seq::State::Idle;
    bool qsoActive = true;
    QString dxCall = QStringLiteral("PT2ND");
    Seq::Action action = Seq::Action::None;
    Seq::State nextState = Seq::State::Idle;
    int txRow = -1;
};

QString actionName(Seq::Action a)
{
    switch (a) {
    case Seq::Action::None: return QStringLiteral("None");
    case Seq::Action::Ignore: return QStringLiteral("Ignore");
    case Seq::Action::ArmRow: return QStringLiteral("ArmRow");
    case Seq::Action::RetryCurrent: return QStringLiteral("RetryCurrent");
    case Seq::Action::Complete: return QStringLiteral("Complete");
    case Seq::Action::StopTx: return QStringLiteral("StopTx");
    }
    return QStringLiteral("?");
}

bool runCase(const Expect &e)
{
    Seq::Context c;
    c.myCall = QStringLiteral("IZ6NNH");
    c.dxCall = e.dxCall;
    c.qsoActive = e.qsoActive;
    c.cqRepeatActive = false;
    c.state = e.state;
    c.rxAudioFreqHz = 1500;
    c.txAudioFreqHz = 1500;
    c.lastTxMessage = (e.state == Seq::State::SendingRr73) ? QStringLiteral("PT2ND IZ6NNH RR73") : QString();

    Seq::Decode d;
    d.message = e.message;
    d.snrDb = -10;
    d.frequencyHz = 1500;

    const Seq::Decision out = Seq::evaluateDecode(d, c);
    const bool ok = out.action == e.action && out.nextState == e.nextState && out.txRow == e.txRow;
    if (!ok) {
        qWarning().noquote() << "FAIL" << e.name
                             << "msg=" << e.message
                             << "got action" << actionName(out.action)
                             << "state" << static_cast<int>(out.nextState)
                             << "row" << out.txRow
                             << "expected" << actionName(e.action)
                             << static_cast<int>(e.nextState)
                             << e.txRow;
    }
    return ok;
}

bool runParserTrapChecks()
{
    bool ok = true;
    const auto rr79 = FtDecodedText::parse(QStringLiteral("IZ6NNH PT2ND RR79"), QStringLiteral("IZ6NNH"));
    if (!rr79.grid.isEmpty() || rr79.rr73 || rr79.final73) {
        qWarning().noquote() << "FAIL parser: RR79 must not become grid/final" << rr79.grid << rr79.text;
        ok = false;
    }

    const auto compound = FtDecodedText::parse(QStringLiteral("EA/IZ6NNH PT2ND -10"), QStringLiteral("IZ6NNH"));
    if (!compound.addressedToMe || compound.senderCall != QStringLiteral("PT2ND") || compound.report != QStringLiteral("-10")) {
        qWarning().noquote() << "FAIL parser: compound addressed call" << compound.text << compound.addressedToMe << compound.senderCall << compound.report;
        ok = false;
    }

    const auto bracketed = FtDecodedText::parse(QStringLiteral("<IZ6NNH> PT2ND -10"), QStringLiteral("IZ6NNH"));
    if (!bracketed.addressedToMe || bracketed.senderCall != QStringLiteral("PT2ND") || bracketed.report != QStringLiteral("-10")) {
        qWarning().noquote() << "FAIL parser: bracketed hash call" << bracketed.text << bracketed.addressedToMe << bracketed.senderCall << bracketed.report;
        ok = false;
    }

    const auto directedCq = FtDecodedText::parse(QStringLiteral("CQ DX K1ABC FN42"), QStringLiteral("IZ6NNH"));
    if (!directedCq.cq || directedCq.senderCall != QStringLiteral("K1ABC") || directedCq.grid != QStringLiteral("FN42")) {
        qWarning().noquote() << "FAIL parser: directed CQ" << directedCq.text << directedCq.senderCall << directedCq.grid;
        ok = false;
    }
    return ok;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    bool ok = runParserTrapChecks();
    const QList<Expect> cases = {
        {QStringLiteral("locator reply -> Tx3 report"), QStringLiteral("IZ6NNH PT2ND JN63"), Seq::State::WaitingDxReply, true, QStringLiteral("PT2ND"), Seq::Action::ArmRow, Seq::State::SendingReport, 2},
        {QStringLiteral("report reply -> Tx4 R-report"), QStringLiteral("IZ6NNH PT2ND -10"), Seq::State::WaitingReport, true, QStringLiteral("PT2ND"), Seq::Action::ArmRow, Seq::State::SendingRReport, 3},
        {QStringLiteral("R-report -> Tx5 RR73"), QStringLiteral("IZ6NNH PT2ND R-10"), Seq::State::WaitingRReport, true, QStringLiteral("PT2ND"), Seq::Action::ArmRow, Seq::State::SendingRr73, 4},
        {QStringLiteral("RR73 -> Tx6 final 73"), QStringLiteral("IZ6NNH PT2ND RR73"), Seq::State::WaitingFinal73, true, QStringLiteral("PT2ND"), Seq::Action::ArmRow, Seq::State::SendingRr73, 5},
        {QStringLiteral("repeated RR73 while Tx6 armed -> no premature complete"), QStringLiteral("IZ6NNH PT2ND RR73"), Seq::State::SendingRr73, true, QStringLiteral("PT2ND"), Seq::Action::RetryCurrent, Seq::State::SendingRr73, -1},
        {QStringLiteral("own transmitted echo ignored"), QStringLiteral("PT2ND IZ6NNH -10"), Seq::State::WaitingReport, true, QStringLiteral("PT2ND"), Seq::Action::Ignore, Seq::State::WaitingReport, -1},
        {QStringLiteral("contest exchange starts report phase"), QStringLiteral("IZ6NNH PT2ND 1A WI"), Seq::State::WaitingDxReply, true, QStringLiteral("PT2ND"), Seq::Action::ArmRow, Seq::State::SendingReport, 2},
        {QStringLiteral("contest R grid ack -> RR73"), QStringLiteral("IZ6NNH PT2ND R JN63"), Seq::State::WaitingRReport, true, QStringLiteral("PT2ND"), Seq::Action::ArmRow, Seq::State::SendingRr73, 4},
    };

    for (const Expect &e : cases) {
        ok = runCase(e) && ok;
    }

    if (ok) {
        qInfo().noquote() << "FtSequencerSelfTest: all regression cases passed";
        return 0;
    }
    return 1;
}
