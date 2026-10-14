#pragma once
#include "AdifLogbook.h"
#include <QJsonObject>

namespace ContestExport {
struct Identity {
    QString id, name;
    QString mode;
    QJsonObject definition;
};
using Profiles = QMap<QString, Identity>;
QString automaticExchange(const LogbookEntry& entry, const Identity& profile, bool sent, QString* error = nullptr);
}
