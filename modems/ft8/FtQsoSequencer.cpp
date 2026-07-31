#include "FtQsoSequencer.h"
#include "FtDecodedText.h"

#include <QtGlobal>

namespace {

FtQsoSequencer::Decision makeNoop(FtQsoSequencer::State state,
                                  const FtQsoSequencer::ParsedMessage &parsed,
                                  FtQsoSequencer::Action action = FtQsoSequencer::Action::None)
{
    FtQsoSequencer::Decision d;
    d.action = action;
    d.nextState = state;
    d.parsed = parsed;
    return d;
}

bool nearHz(int a, int b, int tolerance)
{
    if (a <= 0 || b <= 0) {
        return false;
    }
    return qAbs(a - b) <= qMax(0, tolerance);
}

} // namespace

bool FtQsoSequencer::isCallsignToken(const QString &token)
{
    return FtDecodedText::isCallsign(token);
}

bool FtQsoSequencer::isGridToken(const QString &token)
{
    return FtDecodedText::isGrid(token);
}

bool FtQsoSequencer::isReportToken(const QString &token)
{
    return FtDecodedText::isReport(token);
}

QString FtQsoSequencer::cleanReport(QString report)
{
    return FtDecodedText::cleanReport(report);
}

QString FtQsoSequencer::formatSignalReport(int snrDb, bool acknowledged)
{
    return FtDecodedText::formatSignalReport(snrDb, acknowledged);
}

FtQsoSequencer::ParsedMessage FtQsoSequencer::parseMessage(const QString &message, const QString &myCall)
{
    const FtDecodedText::ParsedMessage common = FtDecodedText::parse(message, myCall);

    ParsedMessage parsed;
    parsed.text = common.cleanText;
    parsed.parts = common.parts;
    parsed.firstCall = common.firstCall;
    parsed.secondCall = common.secondCall;
    parsed.senderCall = common.senderCall;
    parsed.receiverCall = common.receiverCall;
    parsed.grid = common.grid;
    parsed.report = common.report;
    parsed.rawReport = common.rawReport;
    parsed.contestExchange = common.contestExchange;
    parsed.cq = common.cq;
    parsed.directed = common.directed;
    parsed.addressedToMe = common.addressedToMe;
    parsed.containsMyCall = common.containsMyCall;
    parsed.final73 = common.final73;
    parsed.rr73 = common.rr73;
    parsed.rrr = common.rrr;
    parsed.rReport = common.rReport;
    parsed.contestExchangeLike = common.contestExchangeLike;
    parsed.contestAck = common.contestAck;
    return parsed;
}

FtQsoSequencer::Decision FtQsoSequencer::evaluateDecode(const Decode &decode, const Context &context)
{
    const QString myCall = context.myCall.trimmed().toUpper();
    const ParsedMessage parsed = parseMessage(decode.message, myCall);
    const QString activeDx = context.dxCall.trimmed().toUpper();

    if (myCall.isEmpty()) {
        return makeNoop(context.state, parsed, Action::Ignore);
    }

    // WSJT-X-style stop_tolerance: if the station we are working is replying
    // to somebody else near our TX offset, do not keep transmitting over that
    // QSO.  This must inspect messages not addressed to us; filtering solely on
    // addressedToMe misses the anti-QRM case.
    if (context.qsoActive && !activeDx.isEmpty() && parsed.directed &&
        FtDecodedText::callMatches(parsed.senderCall, activeDx) &&
        !FtDecodedText::callMatches(parsed.receiverCall, myCall) &&
        nearHz(decode.frequencyHz, context.txAudioFreqHz, context.stopToleranceHz)) {
        Decision d = makeNoop(context.state, parsed, Action::StopTx);
        d.dxCall = activeDx;
        d.logLine = QStringLiteral("FT sequencer: %1 is replying to %2 near our TX frequency; stopping pending/active TX to avoid QRM.")
                        .arg(parsed.senderCall, parsed.receiverCall.isEmpty() ? QStringLiteral("another caller") : parsed.receiverCall);
        return d;
    }

    if (!parsed.addressedToMe || parsed.senderCall.isEmpty() || FtDecodedText::callMatches(parsed.senderCall, myCall)) {
        return makeNoop(context.state, parsed, Action::Ignore);
    }

    if (context.qsoActive && !activeDx.isEmpty() && !FtDecodedText::callMatches(parsed.senderCall, activeDx)) {
        return makeNoop(context.state, parsed, Action::Ignore);
    }

    Decision d;
    d.nextState = context.state;
    d.parsed = parsed;
    d.dxCall = activeDx.isEmpty() ? parsed.senderCall : activeDx;
    d.dxGrid = !parsed.grid.isEmpty() ? parsed.grid.left(4) : context.dxGrid.left(4).toUpper();
    d.reportSent = context.reportSent.trimmed().toUpper();
    d.reportReceived = context.reportReceived.trimmed().toUpper();
    d.audioFreqHz = decode.frequencyHz;
    d.plan.dxCall = d.dxCall;
    d.plan.dxGrid = d.dxGrid;
    d.plan.audioFrequencyHz = decode.frequencyHz;
    d.plan.autoSequence = true;

    if (!context.qsoActive) {
        d.startsQso = true;
        d.stopsCqRepeat = context.cqRepeatActive;
        d.dxCall = parsed.senderCall;
        d.dxGrid = parsed.grid.left(4);
        d.plan.dxCall = d.dxCall;
        d.plan.dxGrid = d.dxGrid;
        d.logLine = QStringLiteral("FT auto-sequencer picked %1 %2 at %3 Hz.")
                        .arg(d.dxCall,
                             d.dxGrid.isEmpty() ? QStringLiteral("--") : d.dxGrid)
                        .arg(decode.frequencyHz);
    }

    if (parsed.rr73 || parsed.rrr || parsed.final73) {
        if (!parsed.report.isEmpty()) {
            d.reportReceived = cleanReport(parsed.report);
        }

        const bool waitingForFinal = (context.state == State::WaitingFinal73 ||
                                      context.state == State::SendingRReport ||
                                      context.state == State::WaitingRReport ||
                                      context.state == State::SendingReport ||
                                      context.state == State::SendingRr73);
        const QString lastTx = context.lastTxMessage.trimmed().toUpper();
        // WSJT-X processMessage() can still advance to the explicit final 73
        // after RRR/RR73 has been received.  Do not treat our previously sent
        // RR73 as equivalent to already having sent a final 73.  Repeated RR73
        // decodes in the same RX period must not complete early while Tx6 is
        // already armed but not transmitted yet.
        const bool alreadySentFinal73 = lastTx.endsWith(QStringLiteral(" 73"));

        if (context.state == State::SendingRr73 && !alreadySentFinal73) {
            d.action = Action::RetryCurrent;
            d.nextState = State::SendingRr73;
            d.plan.retry = true;
            d.logLine = QStringLiteral("FT sequencer: repeated final acknowledgement received; keeping final 73 armed.");
            return d;
        }

        if (waitingForFinal && !alreadySentFinal73) {
            d.action = Action::ArmRow;
            d.txRow = 5; // Tx6: DX MY 73
            d.tag = QStringLiteral("SEQ");
            d.plan.row = d.txRow;
            d.plan.tag = d.tag;
            d.plan.finalMessage = true;
            d.nextState = State::SendingRr73;
            d.logLine = parsed.rr73
                ? QStringLiteral("FT sequencer: RR73 received; sending final 73.")
                : QStringLiteral("FT sequencer: final acknowledgement received; sending final 73.");
            return d;
        }

        d.action = Action::Complete;
        d.nextState = State::Completed;
        d.completeReason = parsed.rr73 ? QStringLiteral("RR73 received")
                         : (parsed.rrr ? QStringLiteral("RRR received")
                                       : QStringLiteral("73 received"));
        return d;
    }

    if (parsed.contestAck) {
        if (context.state == State::WaitingFinal73 || context.state == State::SendingRReport || context.state == State::SendingRr73) {
            d.action = Action::RetryCurrent;
            d.nextState = (context.state == State::SendingRr73) ? State::SendingRr73 : State::WaitingFinal73;
            d.plan.retry = true;
            d.logLine = QStringLiteral("FT sequencer: duplicate contest ACK/exchange received; keeping current final exchange armed.");
            return d;
        }
        if (context.state == State::WaitingRReport || context.state == State::SendingReport ||
            context.state == State::CallingCq || context.state == State::WaitingDxReply) {
            d.action = Action::ArmRow;
            d.txRow = 4; // Tx5: DX MY RR73
            d.tag = QStringLiteral("SEQ");
            d.plan.row = d.txRow;
            d.plan.tag = d.tag;
            d.plan.finalMessage = true;
            d.nextState = State::SendingRr73;
            d.logLine = QStringLiteral("FT sequencer: contest R/exchange acknowledgement received (%1); sending RR73.")
                            .arg(parsed.contestExchange.isEmpty() ? QStringLiteral("R") : parsed.contestExchange);
            return d;
        }
    }

    if (parsed.contestExchangeLike && !parsed.contestExchange.isEmpty()) {
        // Conservative contest-mode bridge.  MM still generates ordinary six-row
        // FT messages, but it must not misclassify contest payloads as locators
        // or reports.  A first directed exchange starts the same response phase
        // as a locator reply; a later acknowledged exchange is handled above.
        if (context.state == State::CallingCq || context.state == State::WaitingDxReply || !context.qsoActive) {
            d.reportReceived = parsed.contestExchange;
            d.reportSent = formatSignalReport(decode.snrDb, false);
            d.refreshStandardMessages = true;
            d.action = Action::ArmRow;
            d.txRow = 2; // Tx3: DX MY -xx
            d.tag = QStringLiteral("SEQ");
            d.plan.row = d.txRow;
            d.plan.tag = d.tag;
            d.plan.reportSent = d.reportSent;
            d.plan.reportReceived = d.reportReceived;
            d.nextState = State::SendingReport;
            d.logLine = QStringLiteral("FT sequencer: contest exchange received (%1); sending report.")
                            .arg(parsed.contestExchange);
            return d;
        }

        if (context.state == State::WaitingReport || context.state == State::SendingLocator) {
            d.action = Action::RetryCurrent;
            d.plan.retry = true;
            d.logLine = QStringLiteral("FT sequencer: contest exchange seen while waiting for report; retrying locator/report phase.");
            return d;
        }
    }

    if (!parsed.report.isEmpty()) {
        d.reportReceived = cleanReport(parsed.report);

        if (context.state == State::WaitingFinal73 || context.state == State::SendingRReport || context.state == State::SendingRr73) {
            d.action = Action::RetryCurrent;
            d.nextState = (context.state == State::SendingRr73) ? State::SendingRr73 : State::WaitingFinal73;
            d.plan.retry = true;
            d.logLine = QStringLiteral("FT sequencer: waiting for final 73/RR73 from %1; retrying current R-report, no backtracking.")
                            .arg(d.dxCall.isEmpty() ? parsed.senderCall : d.dxCall);
            return d;
        }

        if (parsed.rReport || parsed.rawReport.startsWith(QLatin1Char('R'))) {
            if (context.state == State::WaitingRReport ||
                context.state == State::SendingReport ||
                context.state == State::CallingCq ||
                context.state == State::WaitingDxReply) {
                d.action = Action::ArmRow;
                d.txRow = 4; // Tx5: DX MY RR73
                d.tag = QStringLiteral("SEQ");
                d.plan.row = d.txRow;
                d.plan.tag = d.tag;
                d.plan.finalMessage = true;
                d.nextState = State::SendingRr73;
                d.logLine = QStringLiteral("FT sequencer: R-report received; sending RR73.");
                return d;
            }

            d.action = Action::RetryCurrent;
            d.plan.retry = true;
            d.logLine = QStringLiteral("FT sequencer: unexpected R-report for this phase; retrying current expected message.");
            return d;
        }

        d.reportSent = formatSignalReport(decode.snrDb, true);
        d.refreshStandardMessages = true;
        d.action = Action::ArmRow;
        d.txRow = 3; // Tx4: DX MY R-xx
        d.tag = QStringLiteral("SEQ");
        d.plan.row = d.txRow;
        d.plan.tag = d.tag;
        d.plan.reportSent = d.reportSent;
        d.plan.reportReceived = d.reportReceived;
        d.nextState = State::SendingRReport;
        d.logLine = QStringLiteral("FT sequencer: report received; sending acknowledged R-report.");
        return d;
    }

    if (!parsed.grid.isEmpty()) {
        if (context.state == State::CallingCq || context.state == State::WaitingDxReply || !context.qsoActive) {
            d.reportSent = formatSignalReport(decode.snrDb, false);
            d.refreshStandardMessages = true;
            d.action = Action::ArmRow;
            d.txRow = 2; // Tx3: DX MY -xx
            d.tag = QStringLiteral("SEQ");
            d.plan.row = d.txRow;
            d.plan.tag = d.tag;
            d.plan.reportSent = d.reportSent;
            d.nextState = State::SendingReport;
            d.logLine = QStringLiteral("FT sequencer: locator reply received; sending report.");
            return d;
        }

        if (context.state == State::WaitingReport || context.state == State::SendingLocator) {
            d.action = Action::RetryCurrent;
            d.plan.retry = true;
            d.logLine = QStringLiteral("FT sequencer: still waiting for report; retrying locator exchange.");
            return d;
        }
    }

    return d;
}

FtQsoSequencer::TxCompleteDecision FtQsoSequencer::onTxCompleted(State state)
{
    TxCompleteDecision d;
    d.nextState = state;

    switch (state) {
    case State::SendingLocator:
        d.action = Action::RetryCurrent;
        d.nextState = State::WaitingReport;
        d.logLine = QStringLiteral("FT sequencer: locator sent; waiting for DX report.");
        break;
    case State::SendingReport:
        d.action = Action::RetryCurrent;
        d.nextState = State::WaitingRReport;
        d.logLine = QStringLiteral("FT sequencer: report sent; waiting for R-report.");
        break;
    case State::SendingRReport:
        d.action = Action::RetryCurrent;
        d.nextState = State::WaitingFinal73;
        d.logLine = QStringLiteral("FT sequencer: R-report sent; waiting for final 73/RR73.");
        break;
    case State::WaitingDxReply:
    case State::WaitingReport:
    case State::WaitingRReport:
    case State::WaitingFinal73:
        d.action = Action::RetryCurrent;
        d.nextState = state;
        break;
    case State::SendingRr73:
        d.action = Action::Complete;
        d.nextState = State::Completed;
        d.completeReason = QStringLiteral("RR73/73 sent");
        break;
    default:
        d.action = Action::None;
        break;
    }

    return d;
}
