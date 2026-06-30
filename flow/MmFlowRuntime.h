#ifndef MMFLOWRUNTIME_H
#define MMFLOWRUNTIME_H

#include "MmFlowCapabilityManager.h"
#include "../modems/ft8/AutoQsoFlowExecutor.h"

#include <QString>
#include <QStringList>

namespace mm {

class MmFlowRuntime final
{
public:
    using ExecutionMode = MmFlowCapabilityManager::ExecutionMode;

    struct FtContext
    {
        AutoQsoFlowExecutor::Context autoQso;
        ExecutionMode mode = ExecutionMode::Shadow;
        bool hardwareArmed = false;
        bool schedulerArmed = false;

        bool rotatorConfigured = false;
        bool rotatorConnected = false;
        bool rotatorMoving = false;
        bool rotatorReady = false;
        double rotatorAzimuthDeg = -1.0;
        double rotatorElevationDeg = 0.0;
        double rotatorTargetBearingDeg = -1.0;
        int rotatorEtaMs = 0;
        bool ftTxGuardActive = false;
    };

    struct Result
    {
        QStringList lines;
        QString result;
        bool requestsReplyPreparation = false;
        bool requestsSchedulerArm = false;
        bool requestsRotatorPointing = false;
        bool requestsTxInhibit = false;
    };

    static Result runFtDecode(const QString &flowJson, const FtContext &ctx);
};

} // namespace mm

#endif // MMFLOWRUNTIME_H
