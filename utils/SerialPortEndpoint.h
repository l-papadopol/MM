#pragma once
#include <QString>
#include <QFileInfo>
#include <QRegularExpression>
namespace MadModemSerial {
// Compare the same local endpoint across COM/\\.\COM names and POSIX aliases.
inline QString endpoint(QString value) {
    value = value.trimmed();
    if (value.startsWith(QStringLiteral("\\\\.\\"))) value.remove(0,4);
    if (QRegularExpression("^COM[0-9]+:?$",QRegularExpression::CaseInsensitiveOption).match(value).hasMatch()) {
        if(value.endsWith(':')) value.chop(1); return value.toUpper();
    }
    if (!value.startsWith('/') && QFileInfo::exists("/dev/"+value)) value="/dev/"+value;
    const auto canonical = QFileInfo(value).canonicalFilePath();
    return canonical.isEmpty() ? value : canonical;
}
inline bool sameEndpoint(const QString& a,const QString& b) {
    return !a.trimmed().isEmpty() && !b.trimmed().isEmpty() && endpoint(a)==endpoint(b);
}
}
