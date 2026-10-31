// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QTableWidget>
#include <QRadioButton>
#include <QCheckBox>
#include <QEventLoop>
#include <QTimer>
#include <QTextDocument>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
static void settle(){QEventLoop loop;QTimer::singleShot(150,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QApplication app(argc,argv);app.setApplicationName("MadModem");
 QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);settings.setValue("Updates/checkAtStartup",false);settings.setValue("UI/language","it");settings.sync();
 MainWindow w;w.show();w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);
 for(QString mode:{QString("FT8"),QString("FT4")}){
  w.ui->cmbMode->setCurrentText(mode);settle();
  auto*banner=w.m_lblFt8TxBanner;auto*controls=w.findChild<QGroupBox*>("ftOperatorControls");
  check(banner&&controls&&banner->parentWidget()==w.m_pageFt8Settings,"state outside Mode page");
  check(!controls->isAncestorOf(banner),"state still consumes waterfall controls");
  check(!w.m_lblFt8CqTimeout->isVisible(),"orphan CQ label visible");
  for(QSize size:{QSize(1280,800),QSize(1920,1080)}){
   w.resize(size);settle();w.m_ftSession.lastTxPlan.message="KJ6GHN IZ6NNH R-20";w.m_ftSession.lastTxPlan.tune=false;
   w.m_tableFt8TxMessages->item(3,2)->setText(w.m_ftSession.lastTxPlan.message);
   w.m_txRunning=true;w.updateFt8TxBannerUi();settle();
   QTextDocument doc;doc.setHtml(banner->text());
   check(doc.toPlainText().contains("KJ6GHN IZ6NNH R-20"),"exact TX message lost");
   check(doc.toPlainText().contains("Tx 4"),"TX row label missing");
   check(!doc.toPlainText().contains("SEQ"),"internal sequencer tag displayed");
   check(banner->property("ftBannerState")=="tx","on-air state wrong");
   check(banner->width()<=360&&banner->width()>150,"state width wrong");
   if(argc>1 && size.width()==1920)w.grab().save(QString(argv[1])+"-"+mode+".png");
   w.m_txRunning=false;
  }
  w.m_pendingFt8TxPlan.message="IZ6NNH KJ6GHN RR73";w.m_pendingFt8TxPlan.tune=false;w.m_pendingFt8TxPlan.slotBoundaryUtcMs=QDateTime::currentMSecsSinceEpoch()+15000;
  w.m_ft8PendingTxArmed=true;w.updateFt8TxBannerUi();check(banner->property("ftBannerState")=="armed","armed state wrong");check(banner->text().contains("15.0"),"countdown missing");
  w.m_ft8PendingTxArmed=false;w.updateFt8TxBannerUi();check(banner->property("ftBannerState")=="ready","waiting state wrong");
  w.m_pendingFt8TxPlan.message.clear();w.updateFt8TxBannerUi();check(banner->property("ftBannerState")=="monitor","last TX state wrong");
  w.m_ftSession.lastTxPlan.message.clear();w.updateFt8TxBannerUi();check(banner->property("ftBannerState")=="idle","idle state wrong");
  std::cout<<mode.toStdString()<<" compact state, layouts at1280/1920, armed/ready/last/idle PASS\n";
 }
 w.close();settle();
}
