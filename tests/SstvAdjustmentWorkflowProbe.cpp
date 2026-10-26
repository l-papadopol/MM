// Native GUI + RX worker integration, without opening an audio device or PTT.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QPushButton>
#include <QToolButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QMetaObject>
#include <QSignalBlocker>
#include "modems/sstv/tx/SstvTransmitter.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(int argc,char **argv){QApplication app(argc,argv);app.setApplicationName("MadModem");try{
 QSettings config(AppSettings::settingsFilePath(),QSettings::IniFormat);config.setValue("Updates/checkAtStartup",false);config.setValue("UI/language","it");config.sync();
 MainWindow w;w.resize(1280,900);w.show();w.ui->cmbMode->setCurrentText(SstvDecoder::modeName());
 w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);w.ui->cmbSstvMode->setCurrentText("Robot 24");
 w.ui->spinSstvHorizontalShift->setValue(0);w.ui->spinSstvRedShift->setValue(0);w.ui->spinSstvBlueShift->setValue(0);
 w.ui->chkSstvAutoSync->setChecked(true);w.m_chkSstvAutoVis->setChecked(true);
 w.applySstvSettings();app.processEvents();w.syncLiveRxConfig();
 auto barrier=[&]{QMetaObject::invokeMethod(w.m_rxDecoderWorker,[]{},Qt::BlockingQueuedConnection);app.processEvents();};barrier();
 check(!w.m_grpSstvAdjust->isVisible(),"adjustment panel should start collapsed");
 w.m_btnSstvAdjust->click();app.processEvents();check(w.m_grpSstvAdjust->isVisible(),"image adjustment panel did not expand");
 check(w.ui->spinSstvHorizontalShift->isVisible()&&w.ui->spinSstvRedShift->isVisible()&&w.ui->spinSstvBlueShift->isVisible(),"image controls remain hidden");
 check(w.m_chkSstvAutoVis->isChecked()&&w.ui->chkSstvAutoSync->isChecked(),"opening adjustments changed receive synchronization");
 QImage image(160,120,QImage::Format_RGB32);for(int y=0;y<120;++y)for(int x=0;x<160;++x)image.setPixel(x,y,qRgb(x,y*2,(x+y)%256));
 SstvTransmitter tx(image,"Robot 24",12000);
 while(!tx.isFinished()){AudioBlock b;b.sampleRate=12000;b.samples.resize(4096);b.samples.resize(tx.generate(b.samples.data(),b.samples.size()));
  w.m_rxDecoderWorker->command(w.m_sstvDecoder,&SstvDecoder::processAudioBlock,true,b);}
 barrier();const QImage original=w.m_rxDecoderWorker->image(w.m_sstvDecoder);
 check(original.size()==QSize(160,120),"received image missing in worker snapshot");
 w.ui->spinSstvHorizontalShift->setValue(7);w.ui->spinSstvRedShift->setValue(3);w.ui->spinSstvBlueShift->setValue(-2);barrier();
 const QImage changed=w.m_rxDecoderWorker->image(w.m_sstvDecoder);check(changed!=original,"idle worker did not redraw corrections");
 check(w.m_faxImageWidget->m_image==changed,"GUI preview not updated from idle worker snapshot");
 w.m_btnSstvClearAdjust->click();barrier();check(w.m_rxDecoderWorker->image(w.m_sstvDecoder)==original,"reset adjustments lost original pixels");
 check(w.m_faxImageWidget->m_image==original,"GUI did not restore original pixels");
 check(w.m_chkSstvAutoVis->isChecked()&&w.ui->chkSstvAutoSync->isChecked(),"reset adjustments changed RX synchronization");
 check(w.m_settings.sstvHorizontalShiftPixels==0&&w.m_settings.sstvRedShiftPixels==0&&w.m_settings.sstvBlueShiftPixels==0,"reset adjustments not persisted");
 w.grab().save("sstv-r24-adjustments.png");
 // Exercise the explicitly named free-run action with a simulated running flag
 // so its hardware-start branch is bypassed; this probe opens no audio device.
 w.m_rxRunning=true;w.m_btnSstvForceRx->click();w.m_rxRunning=false;barrier();
 check(!w.m_chkSstvAutoVis->isChecked()&&!w.ui->chkSstvAutoSync->isChecked(),"explicit manual RX did not disable both automatic settings");
 QImage blank(original.size(),QImage::Format_RGB32);blank.fill(Qt::black);
 check(w.m_rxDecoderWorker->image(w.m_sstvDecoder)==blank,"manual restart did not clear received pixels");
 check(!w.m_audioEngine->isRunning()&&!w.m_txRunning,"probe unexpectedly opened hardware");
 w.ui->chkSstvAutoSync->setChecked(true);w.m_chkSstvAutoVis->setChecked(true);barrier();
 for(const QString &lang:{QString("en"),QString("it"),QString("fr"),QString("de"),QString("no"),QString("cs")}){
  w.m_uiLanguageCode=lang;w.loadUiTranslationFile(lang);w.applyUiLanguage();w.resize(1280,900);app.processEvents();
  check(w.width()==1280,"localized adjustment controls widened the main window");
  std::cout<<"UI "<<lang.toStdString()<<" mode="<<w.ui->cmbMode->currentText().toStdString()<<" TXvisible="<<w.m_btnStartImageTx->isVisible()<<"\n";
  check(w.ui->cmbMode->currentText()==SstvDecoder::modeName(),"language change changed active SSTV mode");
  check(w.m_btnStartImageTx->isVisible(),"SSTV TX control hidden after language change");
  for(QWidget *control:QList<QWidget*>{w.ui->spinSstvHorizontalShift,w.ui->spinSstvRedShift,w.ui->spinSstvBlueShift,w.m_btnSstvClearAdjust,w.m_btnSstvForceRx}){
   check(control->isVisible(),"localized correction control hidden");
   const QRect bounds(control->mapTo(w.m_grpSstvAdjust,QPoint()),control->size());
   check(w.m_grpSstvAdjust->rect().contains(bounds),"localized correction control outside group");
  }
  check(w.m_btnSstvClearAdjust->geometry().bottom()<w.m_btnSstvForceRx->geometry().top(),"manual and reset buttons overlap");
  check(w.m_btnSstvForceRx->geometry().bottom()<w.m_lblSstvManualHelp->geometry().top(),"manual RX button overlaps explanatory text");
  auto *scroll=w.findChild<QScrollArea*>("modeSettingsScroll");check(scroll,"mode settings scroll missing");
  scroll->ensureWidgetVisible(w.m_lblSstvManualHelp);app.processEvents();app.processEvents();
  check(w.m_lblSstvManualHelp->visibleRegion().boundingRect().contains(w.m_lblSstvManualHelp->rect()),"manual RX explanation clipped by parent or viewport");
  const QRect txAction(w.m_btnStartImageTx->mapTo(&w,QPoint()),w.m_btnStartImageTx->size());
  check(w.rect().contains(txAction),"image TX action pushed outside main window");
  if(lang=="it")w.grab().save("sstv-r24-adjustments-it.png");
 }
 w.m_uiLanguageCode="it";w.loadUiTranslationFile("it");w.applyUiLanguage();w.close();app.processEvents();
 std::cout<<"PASS native GUI+worker: expand, stopped-RX redraw, preview/snapshot equality, reversible reset, explicit unsynchronized restart; no hardware\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
