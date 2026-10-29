// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SessionDiagnostics.h"
#include <QObject>
#include <QEvent>
#include <QWidget>
#include <QAbstractButton>
#include <QLineEdit>
class SessionDiagnosticsUiTap final : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(!SessionDiagnostics::instance().enabled())return false;
        switch(event->type()) {
        case QEvent::MouseButtonRelease: case QEvent::Wheel: case QEvent::KeyPress:
        case QEvent::Close: case QEvent::WindowActivate: case QEvent::WindowDeactivate: break;
        default:return false;
        }
        auto *widget=qobject_cast<QWidget*>(object);if(!widget)return false;
        QJsonObject data{{"type",int(event->type())},{"class",QString::fromLatin1(object->metaObject()->className())},{"object",object->objectName()}};
        // Describe the control, never key characters or password contents.
        if(auto *button=qobject_cast<QAbstractButton*>(widget)) {data.insert("button",button->text());data.insert("checked",button->isChecked());}
        SessionDiagnostics::instance().record("ui.event",data);
        return false;
    }
};
