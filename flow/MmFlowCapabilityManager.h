#ifndef MMFLOWCAPABILITYMANAGER_H
#define MMFLOWCAPABILITYMANAGER_H

#include <QString>
#include <QStringList>

namespace mm {

class MmFlowCapabilityManager final
{
public:
    enum class ExecutionMode
    {
        Shadow,
        LiveSafe,
        LiveControlled
    };

    struct Request
    {
        QString capability;
        QString detail;
        bool needsHardware = false;
        bool needsScheduler = false;
    };

    struct Decision
    {
        bool allowed = false;
        QString line;
    };

    static QString modeName(ExecutionMode mode);
    static Decision evaluate(const Request &request, ExecutionMode mode, bool hardwareArmed, bool schedulerArmed);
};

} // namespace mm

#endif // MMFLOWCAPABILITYMANAGER_H
