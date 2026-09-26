#pragma once
#include "../utils/CockpitTheme.h"
#include "../rotator/CatRotatorPanel.h"
#include "../rotator/NavballWidget.h"
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QComboBox>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>

inline int runWindowRotatorRegression(QApplication &app)
{
    bool all=true;
    auto check=[&](bool ok,const char *name){all &=ok;QTextStream(stdout)<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
    auto click=[&](QWidget *widget,const QPoint &point){
        const QPointF global=widget->mapToGlobal(point);
        QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        app.sendEvent(widget,&press);app.sendEvent(widget,&release);app.processEvents();
    };
    for(const QString &theme:{QStringLiteral("avionica"),QStringLiteral("qt_default"),QStringLiteral("classic_dark")}){
        MadModemUi::applyUiTheme(app,theme);
        QMainWindow window;window.resize(800,600);
        auto *combo=new QComboBox;combo->addItems({"RTTY","CW","FT8"});window.setCentralWidget(combo);
        auto *bar=window.menuBar();bar->setNativeMenuBar(false);
        int activated=0;
        QList<QMenu*> menus;
        for(const QString &name:{QStringLiteral("File"),QStringLiteral("Mode"),QStringLiteral("Help")}){
            auto *menu=bar->addMenu(name);
            QObject::connect(menu->addAction("Select"),&QAction::triggered,&window,[&](){++activated;});menus<<menu;
        }
        QObject::connect(bar->addAction("Settings"),&QAction::triggered,&window,[&](){++activated;});
        MadModemUi::installCockpitMainWindowChrome(&window);
        MadModemUi::showMainWindowMaximized(&window);app.processEvents();
        check(window.isMaximized() && !window.isFullScreen(),"startup uses maximized window without fullscreen");
        auto *maximize=window.findChild<QPushButton*>("cockpitMaximizeButton");
        check(maximize!=nullptr,"custom maximize control exists");
        for(int state=0;state<3;++state){
            if(state && maximize){maximize->click();app.processEvents();}
            check(!window.isFullScreen(),"restore/maximize never enters fullscreen");
            for(auto *menu:menus){
                const QPoint point=bar->actionGeometry(menu->menuAction()).center();
                QWidget *hit=window.childAt(bar->mapTo(&window,point));
                check(hit==bar,"menu bar receives pointer through decorative chrome");
                click(bar,point);
                check(menu->isVisible() && QApplication::activePopupWidget()==menu,"menu opens and owns popup focus");
                menu->setActiveAction(menu->actions().first());
                const int before=activated;
                QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
                app.sendEvent(menu,&enter);app.processEvents();
                check(activated==before+1,"menu item can be selected");menu->close();
            }
        }
        combo->showPopup();app.processEvents();
        check(QApplication::activePopupWidget()!=nullptr,"combo popup remains available");combo->hidePopup();
        window.close();app.processEvents();
    }
    QTemporaryDir storage;check(storage.isValid(),"isolated preset settings available");
    mm::CatRotatorController controller;
    mm::CatRotatorPanel panel(&controller,nullptr,storage.filePath("presets.ini"));
    mm::CatRotatorController::Config config;config.useElevation=true;config.azimuthMaxDeg=450;config.elevationMaxDeg=180;
    panel.applyConfig(config);panel.resize(380,1000);panel.show();app.processEvents();
    auto *az=panel.findChild<QDoubleSpinBox*>("rotatorSetAz");auto *el=panel.findChild<QDoubleSpinBox*>("rotatorSetEl");
    auto *azSlider=panel.findChild<QSlider*>("rotatorAzSlider");auto *elSlider=panel.findChild<QSlider*>("rotatorElSlider");
    auto *navball=panel.findChild<mm::NavballWidget*>();
    QTextStream(stdout)<<"GEOMETRY navball bottom="<<(navball?navball->geometry().bottom():-1)<<" az top="<<az->mapTo(&panel,QPoint()).y()<<" panel height="<<panel.height()<<" minimum="<<panel.minimumHeight()<<"\n";
    QTextStream(stdout)<<"AXIS localY="<<az->y()<<" h="<<az->height()<<" parentY="<<az->parentWidget()->y()<<" parentH="<<az->parentWidget()->height()<<" navY="<<navball->y()<<" navH="<<navball->height()<<" navMin="<<navball->minimumHeight()<<"\n";
    check(navball && az->mapTo(&panel,QPoint()).y()>=navball->geometry().bottom(),"axis editor does not overlap navball");
    int commands=0;QObject::connect(&controller,&mm::CatRotatorController::targetChanged,&panel,[&](double,double,const QString &){++commands;});
    azSlider->setValue(3150);elSlider->setValue(125);
    check(az->value()==315 && el->value()==12.5,"sliders set azimuth and elevation");
    az->setValue(421.3);el->setValue(21.7);
    check(azSlider->value()==4213 && elSlider->value()==217,"numeric fields synchronize sliders including overlap");
    check(panel.storeManualPreset(0,"Japan") && panel.storeManualPreset(1,QString(100,'W')),"named positions persist");
    az->setValue(20);el->setValue(0);
    check(panel.recallManualPreset(0) && az->value()==421.3 && el->value()==21.7,"preset recalls both axes");
    check(commands==0,"editing and recalling never move the rotator");
    config.profileIndex=1;panel.applyConfig(config);
    check(!panel.recallManualPreset(0),"presets are isolated per rotator profile");
    config.profileIndex=0;config.azimuthMaxDeg=360;panel.applyConfig(config);az->setValue(100);
    check(!panel.recallManualPreset(0) && az->value()==100,"out-of-range saved position is refused without clamping");
    config.useElevation=false;panel.applyConfig(config);
    check(!el->isEnabled() && !elSlider->isEnabled() && el->value()==0,"azimuth-only rotators disable elevation");
    const QString captureDir=qEnvironmentVariable("MADMODEM_UI_CAPTURE_DIR");
    if(!captureDir.isEmpty()){QDir().mkpath(captureDir);panel.grab().save(QDir(captureDir).filePath("rotator_r9.png"));}
    panel.close();
    return all?0:1;
}
