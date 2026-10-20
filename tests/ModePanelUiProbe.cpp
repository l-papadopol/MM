// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callsign/QrzLookupPanel.h"
#include "widgets/RttyScopeWidget.h"
#include <QApplication>
#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTableWidget>
#include <QEventLoop>
#include <QTimer>
#include <QGroupBox>
#include <iostream>
static void settle(){QEventLoop l;QTimer::singleShot(150,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){QApplication app(argc,argv);app.setApplicationName("MadModem");QSettings s(AppSettings::settingsFilePath(),QSettings::IniFormat);s.setValue("Updates/checkAtStartup",false);s.setValue("UI/language","it");s.sync();MainWindow w;w.show();w.ui->sideTabWidget->setFixedWidth(426);
for(QString mode:{QString("FT8"),QString("RTTY Text")}){w.ui->cmbMode->setCurrentText(mode);w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);w.resize(1440,1080);settle();auto* scroll=w.findChild<QScrollArea*>("modeSettingsScroll");scroll->verticalScrollBar()->setValue(0);settle();if(argc>1)w.ui->sideTabWidget->grab().save(QString(argv[1])+"-"+mode+".png");std::cout<<mode.toStdString()<<" side="<<w.ui->sideTabWidget->height()<<" viewport="<<scroll->viewport()->height()<<" content="<<scroll->widget()->height()<<" qrz="<<w.m_qrzLookupPanel->height()<<" overflow="<<scroll->verticalScrollBar()->maximum();if(mode=="FT8")std::cout<<" clock="<<w.m_grpFt8UtcClock->height()<<" tx="<<w.m_tableFt8TxMessages->height();else{auto*scope=w.findChild<RttyScopeWidget*>();std::cout<<" scope="<<scope->height()<<" visible="<<scope->visibleRegion().boundingRect().height();}std::cout<<std::endl;}w.close();settle();}
