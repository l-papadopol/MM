// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "callsign/QrzLookupPanel.h"
#include <QApplication>
#include <QSettings>
#include <QTableWidget>
#include <QGroupBox>
#include <QEventLoop>
#include <QTimer>
#include <stdexcept>
#include <iostream>
static void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
static void settle(){QEventLoop l;QTimer::singleShot(100,&l,&QEventLoop::quit);l.exec();}
int main(int argc,char**argv){
 QApplication app(argc,argv);app.setApplicationName("MadModem");
 QSettings s(AppSettings::settingsFilePath(),QSettings::IniFormat);s.setValue("Updates/checkAtStartup",false);s.sync();
 MainWindow w;w.show();w.ui->sideTabWidget->setFixedWidth(426);
 for(QString mode:{QString("FT8"),QString("FT4")}) {
  w.ui->cmbMode->setCurrentText(mode);w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);w.resize(1280,900);settle();
  require(w.ui->sideTabWidget->indexOf(w.m_tabQrz)>w.ui->sideTabWidget->indexOf(w.ui->tabModeSettings),"QRZ must follow Mode");
  require(w.ui->sideTabWidget->indexOf(w.m_tabQrz)<w.ui->sideTabWidget->indexOf(w.m_tabCatRotator),"QRZ must precede Rotator");
  require(w.m_grpFt8UtcClock->isVisible(),"FT periods hidden in Mode");
  require(w.m_qrzLookupPanel->parentWidget()==w.m_tabQrz,"QRZ not in QRZ tab");
  require(w.m_grpFt8UtcClock->parentWidget()==w.ui->tabModeSettings,"period group not in Mode");
  require(w.ui->modeTabLayout->itemAt(w.ui->modeTabLayout->count()-1)->widget()==w.m_grpFt8UtcClock,"period group not last in Mode");
  auto *bars=w.findChild<QWidget*>("ftSlotClock");
  require(bars&&bars->height()<=48,"period bars are not compact");
  require(w.m_grpFt8UtcClock->height()<110,"period group still consumes dial height");
  w.m_ft8OperatorStopLatched=true;w.m_tableFt8Rx->setRowCount(0);
  auto publish=[&](QString text,int frequency){Ft8RxDecoder::Decode d;d.message=text;d.frequencyHz=frequency;d.utc="120000";d.slotStartUtcMs=1792309800000LL;d.slotPeriodMs=mode=="FT8"?15000:7500;w.handleFt8DecodeReady(d);};
  w.m_settings.decodeTableFontPointSize=14;w.m_settings.decodeTableRowHeightPx=36;w.applyDecodeTableVisualSettings();
  publish("CQ G0JEI IO93",1000);auto *cell=w.m_tableFt8Rx->item(0,4);require(cell&&cell->font().bold(),"CQ emphasis missing");
  QFont styled=cell->font();styled.setStrikeOut(true);cell->setFont(styled);
  w.m_settings.decodeTableFontPointSize=8;w.m_settings.decodeTableRowHeightPx=18;w.applyDecodeTableVisualSettings();
  require(cell->font().pointSize()==8&&cell->font().bold()&&cell->font().strikeOut(),"existing row font/style not updated");
  publish("CQ DL2LAU JO31",1500);require(w.m_tableFt8Rx->item(1,4)->font().pointSize()==8,"new CQ row ignored small font");
  w.appendFt8LocalTxRow("G0JEI IZ6NNH -10",1000,"TX");
  require(w.m_tableFt8Rx->item(w.m_tableFt8Rx->rowCount()-1,4)->font().pointSize()==8,"new TX row ignored font");
  require(w.m_tableFt8QsoHistory->item(w.m_tableFt8QsoHistory->rowCount()-1,5)->font().pointSize()==8,"TX history ignored font");
  settle();if(argc>1)w.grab().save(QString(argv[1])+"-"+mode+".png");
  std::cout<<mode.toStdString()<<" PASS bars="<<bars->width()<<"x"<<bars->height()<<" group="<<w.m_grpFt8UtcClock->height()<<" existing/new CQ/TX/history font and emphasis\n";
 }
 w.close();settle();
}
