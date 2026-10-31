// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settings/AppSettings.h"
#include "utils/RuntimeI18n.h"
#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QLabel>
#include <QLCDNumber>
#include <QSettings>
#include <QTimer>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
static void settle() {
    QEventLoop loop; QTimer::singleShot(200,&loop,&QEventLoop::quit); loop.exec();
}
static void checkClock(MainWindow& window) {
    auto* group=window.findChild<QGroupBox*>("ftUtcClockGroup");
    auto* dial=window.findChild<QWidget*>("ftSlotClock");
    auto* lcd=window.findChild<QLCDNumber*>("ftUtcClock");
    require(group && dial && lcd && group->isVisible(), "FT clock is not visible");
    require(!qobject_cast<QGroupBox*>(group->parentWidget()), "clock still nested in sequencer box");
    require(dial->parentWidget()==group && lcd->parentWidget()==group,"clock children reparented");
    require(dial->y()<40,"duplicate content still reserves space above dial");
    require(!dial->geometry().intersects(lcd->geometry()),"dial overlaps digital clock");
    require(group->rect().contains(lcd->geometry()),"digital clock is clipped");
    require(lcd->visibleRegion().boundingRect()==lcd->rect(),"digital clock clipped by settings viewport");
    require(dial->visibleRegion().boundingRect()==dial->rect(),"dial clipped by settings viewport");
    require(!group->toolTip().isEmpty(),"sequencer details lost");
    for (auto* label:group->findChildren<QLabel*>())
        require(!label->isVisible(),"duplicate sequencer label returned above clock");
    std::cout << "Clock " << window.width() << 'x' << window.height()
              << " clock-top=" << group->mapTo(&window,QPoint()).y() << ": group=" << group->width() << 'x' << group->height()
              << " dial-top=" << dial->y() << " lcd-bottom=" << lcd->geometry().bottom() << '\n';
}

int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setApplicationName("MadModem");
    QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);
    settings.setValue("Updates/checkAtStartup",false);
    settings.setValue("UI/language","it");
    for (const auto& id:{"neural_ctc","neural_envelope","neural_search","unknown","native"}) {
        settings.setValue("CW/decodeEngine",id);settings.sync();
        AppSettings loaded;loaded.load();
        const QString expected=QString::fromLatin1(id).startsWith("neural_") ? "neural_search" : "native";
        require(loaded.cwDecodeEngine==expected,"saved CW engine migration failed");
    }
    settings.setValue("CW/decodeEngine","native");settings.sync();
    MadModemI18n::setLanguageCode("it");
    {
        MainWindow window;window.resize(1280,900);window.show();settle();
        auto* mode=window.findChild<QComboBox*>("cmbMode");require(mode,"mode selector missing");
        mode->setCurrentText("CW Morse");settle();
        auto* engines=window.findChild<QComboBox*>("cwDecodeEngine");
        require(engines && engines->isVisible() && engines->count()==2,"CW selector must expose exactly two engines");
        require(engines->currentData().toString()=="native","Native R12 not default");
        require(engines->itemText(0).contains("R12") && engines->itemText(1).contains("R13"),"engine revision labels missing");
        engines->setCurrentIndex(1);settle();settings.sync();
        require(settings.value("CW/decodeEngine").toString()=="neural_search","Neural R13 not saved");
        if (argc>1) window.grab().save(QString::fromLocal8Bit(argv[1])+"-cw.png");
        for (const auto& ftMode:{"FT8","FT4"}) {
            mode->setCurrentText(ftMode);window.ui->sideTabWidget->setCurrentWidget(window.ui->tabModeSettings);settle();
            for (const auto& size:{QSize(1280,900),QSize(1600,1000),QSize(1024,768)}) {
                window.resize(size);settle();checkClock(window);
                if (argc>1 && size==QSize(1280,900)) window.grab().save(QString::fromLocal8Bit(argv[1])+"-"+ftMode+"-window.png");
            }
            if (argc>1) window.findChild<QGroupBox*>("ftUtcClockGroup")->grab().save(
                QString::fromLocal8Bit(argv[1])+"-"+ftMode+"-clock.png");
        }
        mode->setCurrentText("CW Morse");settle();
        require(engines->currentData().toString()=="neural_search","mode switch changed engine");
        require(!window.findChild<QWidget*>("ftUtcClockGroup")->isVisible(),"FT clock leaked into CW");
        window.close();
    }
    settle();
    {
        MainWindow window;settle();
        auto* engines=window.findChild<QComboBox*>("cwDecodeEngine");
        require(engines && engines->currentData().toString()=="neural_search","engine not restored after reopen");
        engines->setCurrentIndex(0);settle();settings.sync();
        require(settings.value("CW/decodeEngine").toString()=="native","return to Native R12 not saved");
        window.close();
    }
    std::cout << "PASS two-engine UI, settings migration, mode switch, save/reload, Native return and FT4/FT8 clock layout\n";
}
