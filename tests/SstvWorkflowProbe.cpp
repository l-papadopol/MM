// Full application integration; no audio device or RF transmission is started.
#include "mainwindow.h"
#include "utils/RuntimeI18n.h"
#include <QPushButton>
#include <QTabWidget>
#include <QTabBar>
#include "ui_mainwindow.h"
#include "dialogs/SstvImageEditorDialog.h"
#include "modems/sstv/tx/SstvTransmitter.h"
#include <QApplication>
#include <QSettings>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
int main(int argc,char **argv){QApplication app(argc,argv);app.setApplicationName("MadModem");try {
 QSettings config(AppSettings::settingsFilePath(),QSettings::IniFormat);config.setValue("Updates/checkAtStartup",false);config.setValue("UI/language","it");config.sync();
 MainWindow w;w.m_settings.textMyCallsign="IZ6NNH";w.ui->cmbMode->setCurrentText(SstvDecoder::modeName());
 w.resize(1280,800);w.show();w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);app.processEvents();
 QImage base(640,496,QImage::Format_RGB32);base.fill(QColor(120,160,200));SstvImageEditorDialog editor;
 editor.setBackgroundImage(base);editor.setQsoFields("IZ6NNH","F4ABC","Jean","JN18","595");
 w.m_sstvEditorProject=editor.projectData();w.m_sstvTxBaseImage=editor.composedImage();
 {QSignalBlocker a(w.m_editSstvTxCall),b(w.m_editSstvTxName),c(w.m_editSstvTxQth),d(w.m_editSstvTxReport);
 w.m_editSstvTxCall->setText("F4ABC");w.m_editSstvTxName->setText("Jean");w.m_editSstvTxQth->setText("JN18");w.m_editSstvTxReport->setText("595");}
 for(const QString &mode:{QString("Martin M1"),QString("Scottie S2"),QString("PD120")}){
  w.ui->cmbSstvMode->setCurrentText(mode);w.updateSstvTxPreparedImage();
  check(w.m_txSourceImage==editor.composedImage(),"main window changed editor composition");
  check(w.m_txPreparedImage==SstvTransmitter::prepareImage(editor.composedImage(),mode),"TX raster differs from editor output");
 }
 w.m_editSstvTxCall->setText("G0JEI");w.updateSstvTxPreparedImage();SstvImageEditorDialog reopened;
 check(reopened.restoreProject(w.m_sstvEditorProject),"main project no longer editable");check(reopened.qsoFields().value(1)=="G0JEI","quick field did not update shared project");
 check(w.m_txSourceImage==reopened.composedImage(),"quick-field composition duplicated");
 w.m_chkSstvAutoVis->setChecked(false);check(!w.m_settings.sstvAutoVis,"VIS setting not applied");
 w.m_chkSstvAutoVis->setChecked(true);check(w.m_settings.sstvAutoVis,"VIS setting not reenabled");
 for(const QString &lang:{QString("en"),QString("it"),QString("fr"),QString("de"),QString("no"),QString("cs")}){
  MadModemI18n::setLanguageCode(lang);SstvImageEditorDialog translated;translated.show();translated.resize(800,600);app.processEvents();app.processEvents();
  check(translated.width()==800 && translated.height()==600,"localized editor exceeds 800x600");
  auto *tools=translated.findChild<QTabWidget*>("sstvEditorTools");check(tools,"tools tabs missing");
  for(int tab=0;tab<tools->count();++tab)check(tools->tabBar()->rect().contains(tools->tabBar()->tabRect(tab)),"localized tab clipped");
  for(auto *button:translated.findChildren<QPushButton*>())if(button->isVisible()) {
   check(translated.rect().contains(button->mapTo(&translated,button->rect().center())),"localized action outside editor");
  }
  if(lang=="it"){translated.resize(1100,820);translated.setQsoFields("IZ6NNH","F4ABC","Jean","JN18","595");app.processEvents();translated.grab().save("sstv-editor-it.png");}
 }
 app.processEvents();w.grab().save("sstv-main-1280.png");
 check(!w.m_txRunning&&!w.m_rxRunning,"probe unexpectedly started hardware");w.close();app.processEvents();
 std::cout<<"PASS main editor composition exact across 3 modes; quick fields preserve project; VIS setting; no hardware started\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
