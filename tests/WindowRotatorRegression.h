#pragma once
#include "../utils/CockpitTheme.h"
#include "../rotator/CatRotatorPanel.h"
#include "../rotator/NavballWidget.h"
#include "../dialogs/SstvImageEditorDialog.h"
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
#include <QDialog>
#include <QMessageBox>
#include <QVBoxLayout>

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
    auto dragEdge=[&](QWidget &window,Qt::Edges edge,const QPoint &delta){
        auto *handle=window.findChild<QWidget*>(QStringLiteral("mmResizeHandle_%1").arg(int(edge)));
        check(handle && handle->isVisible(),"resize handle visible on restored window");
        if(!handle || !handle->isVisible()) return;
        const QPoint local=handle->rect().center(), origin=handle->mapToGlobal(local);
        const QRect before=window.geometry();
        QMouseEvent press(QEvent::MouseButtonPress,local,origin,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        app.sendEvent(handle,&press);
        QMouseEvent move(QEvent::MouseMove,handle->mapFromGlobal(origin+delta),origin+delta,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        app.sendEvent(handle,&move);
        QMouseEvent release(QEvent::MouseButtonRelease,handle->mapFromGlobal(origin+delta),origin+delta,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        app.sendEvent(handle,&release);app.processEvents();
        check(window.size()!=before.size(),"edge drag actually resizes window");
        if(edge & Qt::LeftEdge) check(window.geometry().right()==before.right(),"left drag preserves right edge");
        if(edge & Qt::TopEdge) check(window.geometry().bottom()==before.bottom(),"top drag preserves bottom edge");
        check(QWidget::mouseGrabber()!=handle,"resize releases mouse capture");
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
        window.showNormal();window.resize(800,600);app.processEvents();
        for(const Qt::Edges edge:{Qt::Edges(Qt::LeftEdge),Qt::Edges(Qt::RightEdge),Qt::Edges(Qt::TopEdge),Qt::Edges(Qt::BottomEdge),
              Qt::LeftEdge|Qt::TopEdge,Qt::RightEdge|Qt::TopEdge,Qt::LeftEdge|Qt::BottomEdge,Qt::RightEdge|Qt::BottomEdge}) {
            dragEdge(window,edge,QPoint(edge & Qt::LeftEdge ? -35 : 35,edge & Qt::TopEdge ? -25 : 25));
        }
        window.showMaximized();app.processEvents();
        for(auto *child:window.findChildren<QWidget*>()) if(child->objectName().startsWith("mmResizeHandle_"))
            check(!child->isVisible(),"maximized window does not intercept border controls");
        QDialog dialog;auto *layout=new QVBoxLayout(&dialog);layout->addWidget(new QPushButton("Control"));
        dialog.resize(400,250);dialog.show();app.processEvents();
        dragEdge(dialog,Qt::RightEdge|Qt::BottomEdge,QPoint(55,45));dialog.close();
        QMessageBox message(QMessageBox::Information,"Message","Resizable message",QMessageBox::Ok);
        message.show();app.processEvents();app.processEvents();
        check(message.maximumWidth()==QWIDGETSIZE_MAX && message.maximumHeight()==QWIDGETSIZE_MAX,"message box releases Qt fixed-size constraints");
        dragEdge(message,Qt::RightEdge|Qt::BottomEdge,QPoint(55,45));message.close();
        QDialog embedded(&window);embedded.setWindowFlags(Qt::Widget);
        embedded.setProperty("cockpitEmbeddedDialog",true);embedded.show();app.processEvents();
        check(!embedded.property("mmWindowResizeInstalled").toBool(),"embedded settings pages have no window resize handles");
        window.close();app.processEvents();
    }
    SstvImageEditorDialog editor;editor.show();app.processEvents();
    editor.resize(800,600);app.processEvents();
    check(editor.width()<=800 && editor.height()<=600,"SSTV editor fits a small desktop with scrollable tools");
    dragEdge(editor,Qt::RightEdge|Qt::BottomEdge,QPoint(40,40));editor.close();
    QTemporaryDir storage;check(storage.isValid(),"isolated preset settings available");
    mm::CatRotatorController controller;
    check(controller.trackingMode()==mm::CatRotatorController::TrackingMode::Manual,"rotator starts in manual mode");
    controller.setTrackingMode(mm::CatRotatorController::TrackingMode::Qso);
    check(controller.trackingMode()==mm::CatRotatorController::TrackingMode::Qso,"operator can explicitly select QSO tracking");
    controller.setTrackingQsoTarget(false);
    check(controller.trackingMode()==mm::CatRotatorController::TrackingMode::Manual,"disabling QSO tracking also updates the visible manual mode");
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
