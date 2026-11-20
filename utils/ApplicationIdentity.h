#pragma once
#include "MadModemVersion.h"
#include "MadModemBuildInfo.h"
#include <QString>

namespace MadModemIdentity {
inline QString windowTitle()
{
    const QString edition = QStringLiteral(MADMODEM_EDITION);
    return QStringLiteral(MADMODEM_VERSION_DISPLAY) + (edition.isEmpty() ? QString() : QStringLiteral(" ") + edition)
           + QStringLiteral(" — build " MADMODEM_BUILD_NUMBER);
}
}
