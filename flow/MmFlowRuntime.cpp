#include "MmFlowRuntime.h"

#include <QtGlobal>

namespace mm {

namespace {

QString yesNo(bool v)
{
    return v ? QStringLiteral("yes") : QStringLiteral("no");
}

QString degreeText(double value)
{
    return value >= 0.0 ? QStringLiteral("%1°").arg(QString::number(value, 'f', 1)) : QStringLiteral("--");
}

bool appendCapability(MmFlowRuntime::Result *result,
                      const MmFlowCapabilityManager::Request &request,
                      MmFlowCapabilityManager::ExecutionMode mode,
                      bool hardwareArmed,
                      bool schedulerArmed)
{
    if (result == nullptr) {
        return false;
    }
    const MmFlowCapabilityManager::Decision d = MmFlowCapabilityManager::evaluate(request, mode, hardwareArmed, schedulerArmed);
    result->lines << d.line;
    return d.allowed;
}

} // namespace

MmFlowRuntime::Result MmFlowRuntime::runFtDecode(const QString &flowJson, const FtContext &ctx)
{
    Result result;
    result.lines << QStringLiteral("[Flow][runtime] Event bus: ft.decode -> MM Flow Studio runtime (%1 mode)")
        .arg(MmFlowCapabilityManager::modeName(ctx.mode));

    if (ctx.rotatorConfigured) {
        result.lines << QStringLiteral("[Flow][runtime] Rotator state: configured=yes, connected=%1, moving=%2, ready=%3, az=%4, target=%5, ETA=%6 s")
            .arg(yesNo(ctx.rotatorConnected),
                 yesNo(ctx.rotatorMoving),
                 yesNo(ctx.rotatorReady),
                 degreeText(ctx.rotatorAzimuthDeg),
                 degreeText(ctx.rotatorTargetBearingDeg),
                 QString::number(static_cast<double>(qMax(0, ctx.rotatorEtaMs)) / 1000.0, 'f', 1));
    } else {
        result.lines << QStringLiteral("[Flow][runtime] Rotator state: no rotator configured for this context.");
    }

    const AutoQsoFlowExecutor::Trace shadow = AutoQsoFlowExecutor::runShadow(flowJson, ctx.autoQso);
    for (const QString &line : shadow.lines) {
        result.lines << line;
    }
    result.result = shadow.result;

    if (ctx.rotatorConfigured && ctx.rotatorTargetBearingDeg >= 0.0 && !ctx.rotatorReady) {
        const bool pointAllowed = appendCapability(&result,
                         {QStringLiteral("rotator.point.to.qso"),
                          QStringLiteral("bearing %1, ETA %2 s").arg(degreeText(ctx.rotatorTargetBearingDeg), QString::number(static_cast<double>(ctx.rotatorEtaMs) / 1000.0, 'f', 1)),
                          true,
                          false},
                         ctx.mode,
                         ctx.hardwareArmed,
                         ctx.schedulerArmed);
        const bool inhibitAllowed = appendCapability(&result,
                         {QStringLiteral("scheduler.tx.inhibit.until.rotator.ready"),
                          QStringLiteral("FT TX guard waits for rotator ready"),
                          false,
                          true},
                         ctx.mode,
                         ctx.hardwareArmed,
                         ctx.schedulerArmed);
        result.requestsRotatorPointing = pointAllowed;
        result.requestsTxInhibit = inhibitAllowed;
    }

    if (shadow.wouldPrepareReply) {
        result.requestsReplyPreparation = appendCapability(&result,
                         {QStringLiteral("scheduler.prepare.ft.reply"), ctx.autoQso.suggestedTxMessage, false, true},
                         ctx.mode,
                         ctx.hardwareArmed,
                         ctx.schedulerArmed);
    }
    if (shadow.wouldArmTx) {
        result.requestsSchedulerArm = appendCapability(&result,
                         {QStringLiteral("scheduler.arm.ft.tx"), QStringLiteral("PTT/audio remain owned by FT scheduler"), false, true},
                         ctx.mode,
                         ctx.hardwareArmed,
                         ctx.schedulerArmed);
    }

    result.lines << QStringLiteral("[Flow][runtime] Result: %1.").arg(result.result.isEmpty() ? QStringLiteral("observed") : result.result);
    return result;
}

} // namespace mm
