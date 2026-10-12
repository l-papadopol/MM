#pragma once
#include <QApplication>
#include <QDirIterator>
#include <QPluginLoader>
#include <QTextStream>
#include <QSslSocket>
#ifdef MADMODEM_WITH_QT_LOCATION
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#endif

inline int runLinuxPortableRuntimeRegression()
{
    const QDir root(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    QTextStream out(stdout);
    bool ok = true;
    for (const QString &group : {QStringLiteral("plugins"), QStringLiteral("qml")}) {
        const QString directory = root.filePath(group);
        if (!QDir(directory).exists()) { out << "FAIL missing " << directory << '\n'; ok = false; }
        QDirIterator files(directory, {QStringLiteral("*.so")}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            QPluginLoader plugin(files.next());
            if (!plugin.load()) { out << "FAIL plugin " << plugin.fileName() << ": " << plugin.errorString() << '\n'; ok = false; }
        }
    }
    if (!QSslSocket::supportsSsl()) { out << "FAIL OpenSSL runtime\n"; ok = false; }
#ifdef MADMODEM_WITH_QT_LOCATION
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.12\nimport QtLocation 5.12\nimport QtPositioning 5.12\n"
                      "Item { property var point: QtPositioning.coordinate(0, 0); Plugin { name: \"osm\" } }", QUrl());
    QScopedPointer<QObject> object(component.create());
    if (!object) { out << "FAIL QML map imports: " << component.errorString() << '\n'; ok = false; }
#else
    out << "FAIL portable release lacks Qt Location support\n"; ok = false;
#endif
    out << (ok ? "PASS" : "FAIL") << " portable Qt plugins, QML and TLS runtime\n";
    return ok ? 0 : 1;
}
