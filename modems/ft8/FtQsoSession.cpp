#include "FtQsoSession.h"

#include <QtGlobal>

void FtQsoSession::resetAll()
{
    state = State::Idle;
    deferredState = State::Idle;
    cqRepeatActive = false;
    qsoActive = false;
    resumeCqAfterQso = false;
    autoLogDone = false;
    cqRepeatDeadlineUtc = QDateTime();
    cqRepeatRemaining = 0;
    qsoStartUtc = QDateTime();
    dxCall.clear();
    dxGrid.clear();
    reportSent.clear();
    reportReceived.clear();
    audioFreqHz = 0;
    clearRetry();
    activeTxRow = -1;
    clearLastTx();
}

void FtQsoSession::clearQso()
{
    state = State::Idle;
    qsoActive = false;
    resumeCqAfterQso = false;
    autoLogDone = false;
    qsoStartUtc = QDateTime();
    dxCall.clear();
    dxGrid.clear();
    reportSent.clear();
    reportReceived.clear();
    audioFreqHz = 0;
    clearRetry();
    activeTxRow = -1;
}

void FtQsoSession::clearRetry()
{
    retryMessage.clear();
    retryTag.clear();
    retryRemaining = 0;
}

void FtQsoSession::clearLastTx()
{
    lastTxMessage.clear();
    lastTxTag.clear();
    lastTxWasTune = false;
}

void FtQsoSession::resetForCqRepeat(const QString &, const QString &, int timeoutMinutes)
{
    clearQso();
    cqRepeatActive = true;
    state = State::CallingCq;
    cqRepeatRemaining = 0;
    cqRepeatDeadlineUtc = timeoutMinutes > 0
        ? QDateTime::currentDateTimeUtc().addSecs(timeoutMinutes * 60)
        : QDateTime();
}

void FtQsoSession::resetForCqRepeatCount(const QString &, const QString &, int repeatCount)
{
    clearQso();
    cqRepeatActive = true;
    state = State::CallingCq;
    cqRepeatRemaining = qBound(1, repeatCount, 99);
    cqRepeatDeadlineUtc = QDateTime();
}

void FtQsoSession::startQso(const QString &call, const QString &grid, int audioFrequencyHz, const QDateTime &startUtc)
{
    const QString normalizedCall = call.trimmed().toUpper();
    if (!normalizedCall.isEmpty()) {
        if (!qsoActive || !FtDecodedText::callMatches(normalizedCall, dxCall)) {
            reportSent.clear();
            reportReceived.clear();
            autoLogDone = false;
            qsoStartUtc = startUtc.isValid() ? startUtc : QDateTime::currentDateTimeUtc();
        }
        dxCall = normalizedCall;
        qsoActive = true;
    }
    const QString normalizedGrid = grid.trimmed().left(4).toUpper();
    if (FtDecodedText::isGrid(normalizedGrid)) {
        dxGrid = normalizedGrid;
        rememberGrid(normalizedCall, normalizedGrid);
    }
    if (audioFrequencyHz > 0) {
        audioFreqHz = audioFrequencyHz;
    }
}

void FtQsoSession::applyDecision(const FtQsoSequencer::Decision &decision)
{
    if (decision.nextState != State::Idle || decision.action != FtQsoSequencer::Action::Ignore) {
        state = decision.nextState;
    }
    if (decision.startsQso || !decision.dxCall.trimmed().isEmpty()) {
        startQso(decision.dxCall, decision.dxGrid, decision.audioFreqHz);
    } else if (!decision.dxGrid.trimmed().isEmpty()) {
        const QString grid = decision.dxGrid.trimmed().left(4).toUpper();
        if (FtDecodedText::isGrid(grid)) {
            dxGrid = grid;
            rememberGrid(dxCall, grid);
        }
    }
    if (!decision.reportSent.trimmed().isEmpty()) {
        reportSent = decision.reportSent.trimmed().toUpper();
    }
    if (!decision.reportReceived.trimmed().isEmpty()) {
        reportReceived = decision.reportReceived.trimmed().toUpper();
    }
    if (decision.audioFreqHz > 0 && audioFreqHz <= 0) {
        audioFreqHz = decision.audioFreqHz;
    }
    if (decision.stopsCqRepeat) {
        cqRepeatActive = false;
    }
}

FtQsoSequencer::Context FtQsoSession::makeContext(const QString &myCall, int rxAudioFreqHz, int txAudioFreqHz) const
{
    FtQsoSequencer::Context context;
    context.myCall = myCall;
    context.dxCall = dxCall;
    context.dxGrid = dxGrid;
    context.reportSent = reportSent;
    context.reportReceived = reportReceived;
    context.lastTxMessage = lastTxMessage;
    context.state = state;
    context.qsoActive = qsoActive;
    context.cqRepeatActive = cqRepeatActive;
    context.autoTxEnabled = true;
    context.rxAudioFreqHz = rxAudioFreqHz;
    context.txAudioFreqHz = txAudioFreqHz;
    return context;
}

FtTxPlan FtQsoSession::makePlan(const QString &message, const QString &tag, int txAudioFreqHz) const
{
    FtTxPlan plan;
    plan.message = message.trimmed().toUpper();
    plan.tag = tag.trimmed().isEmpty() ? QStringLiteral("TX") : tag.trimmed().toUpper();
    plan.dxCall = dxCall;
    plan.dxGrid = dxGrid;
    plan.reportSent = reportSent;
    plan.reportReceived = reportReceived;
    plan.audioFrequencyHz = txAudioFreqHz;
    plan.autoSequence = (plan.tag == QStringLiteral("SEQ") || plan.tag == QStringLiteral("RETRY") || plan.tag == QStringLiteral("CQ"));
    plan.retry = (plan.tag == QStringLiteral("RETRY"));
    return plan;
}

void FtQsoSession::rememberGrid(const QString &call, const QString &grid)
{
    const QString key = FtDecodedText::baseCall(call.trimmed().toUpper());
    const QString locator = grid.trimmed().left(4).toUpper();
    if (key.isEmpty() || !FtDecodedText::isGrid(locator) || FtDecodedText::isAckLikeGridTrap(locator)) {
        return;
    }
    if (observedCallGrids.size() > 1024) {
        observedCallGrids.clear();
    }
    observedCallGrids.insert(key, locator);
}

QString FtQsoSession::knownGridFor(const QString &call) const
{
    return observedCallGrids.value(FtDecodedText::baseCall(call.trimmed().toUpper())).left(4).toUpper();
}
