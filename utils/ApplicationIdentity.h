#pragma once
#include "MadModemVersion.h"
#include "MadModemBuildInfo.h"
#include <QString>

namespace MadModemIdentity {
inline QString windowTitle()
{
    return QStringLiteral(MADMODEM_VERSION_DISPLAY " — build " MADMODEM_BUILD_NUMBER);
}
}
