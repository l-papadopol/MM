#include "MmFlowCapabilityManager.h"

namespace mm {

QString MmFlowCapabilityManager::modeName(ExecutionMode mode)
{
    switch (mode) {
    case ExecutionMode::Shadow: return QStringLiteral("shadow");
    case ExecutionMode::LiveSafe: return QStringLiteral("live-safe");
    case ExecutionMode::LiveControlled: return QStringLiteral("live-controlled");
    }
    return QStringLiteral("unknown");
}

MmFlowCapabilityManager::Decision MmFlowCapabilityManager::evaluate(const Request &request,
                                                                     ExecutionMode mode,
                                                                     bool hardwareArmed,
                                                                     bool schedulerArmed)
{
    Decision d;
    const QString cap = request.capability.trimmed();
    const QString detail = request.detail.trimmed();

    if (mode == ExecutionMode::Shadow) {
        d.allowed = false;
        d.line = QStringLiteral("[Flow][capability] %1 -> observed only in shadow mode%2")
            .arg(cap, detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QStringLiteral(")"));
        return d;
    }

    if (request.needsHardware && !hardwareArmed) {
        d.allowed = false;
        d.line = QStringLiteral("[Flow][capability] %1 -> blocked: hardware capability is not armed%2")
            .arg(cap, detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QStringLiteral(")"));
        return d;
    }

    if (request.needsScheduler && !schedulerArmed) {
        d.allowed = false;
        d.line = QStringLiteral("[Flow][capability] %1 -> blocked: scheduler capability is not armed%2")
            .arg(cap, detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QStringLiteral(")"));
        return d;
    }

    if (request.needsHardware && mode != ExecutionMode::LiveControlled) {
        d.allowed = false;
        d.line = QStringLiteral("[Flow][capability] %1 -> blocked: live-safe mode cannot drive hardware%2")
            .arg(cap, detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QStringLiteral(")"));
        return d;
    }

    d.allowed = true;
    d.line = QStringLiteral("[Flow][capability] %1 -> allowed%2")
        .arg(cap, detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QStringLiteral(")"));
    return d;
}

} // namespace mm
