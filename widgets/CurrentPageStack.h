#pragma once
#include <QStackedWidget>

// Hidden modes must not force the visible mode's scroll area to their size.
class CurrentPageStack : public QStackedWidget {
public:
    explicit CurrentPageStack(QWidget *parent = nullptr) : QStackedWidget(parent) {
        connect(this, &QStackedWidget::currentChanged, this, [this] { updateGeometry(); });
    }
    bool hasHeightForWidth() const override {
        return currentWidget() && currentWidget()->hasHeightForWidth();
    }
    int heightForWidth(int width) const override {
        return currentWidget() ? currentWidget()->heightForWidth(width) : -1;
    }
    QSize minimumSizeHint() const override {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize(0, 0);
    }
    QSize sizeHint() const override {
        return currentWidget() ? currentWidget()->sizeHint() : QSize(0, 0);
    }
};
