// Real SSTV completion -> queued archive facade -> PNG, without RF/audio hardware.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "runtime/SstvImageArchive.h"
#include "modems/sstv/tx/SstvTransmitter.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QElapsedTimer>
#include <QThread>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QScrollArea>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSignalBlocker>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
static QByteArray bytes(const QString &path){QFile f(path);check(f.open(QIODevice::ReadOnly),"read saved PNG");return f.readAll();}
int main(int argc,char **argv){QApplication app(argc,argv);app.setApplicationName("MadModem");try{
 QTemporaryDir root;check(root.isValid(),"archive fixture directory");
 QSettings config(AppSettings::settingsFilePath(),QSettings::IniFormat);config.setValue("Updates/checkAtStartup",false);config.setValue("UI/language","it");config.setValue("SSTV/autoSave",false);config.sync();
 MainWindow w;w.resize(1280,900);w.show();w.ui->cmbMode->setCurrentText(SstvDecoder::modeName());w.ui->cmbSstvMode->setCurrentText("Robot 24");
 w.ui->sideTabWidget->setCurrentWidget(w.ui->tabModeSettings);w.ui->spinSstvHorizontalShift->setValue(0);w.ui->spinSstvRedShift->setValue(0);w.ui->spinSstvBlueShift->setValue(0);
 w.ui->chkSstvAutoSync->setChecked(true);w.m_chkSstvAutoVis->setChecked(true);w.applySstvSettings();app.processEvents();w.syncLiveRxConfig();
 auto barrier=[&]{QMetaObject::invokeMethod(w.m_rxDecoderWorker,[]{},Qt::BlockingQueuedConnection);app.processEvents();};barrier();
 check(!w.m_chkSstvAutoSave->isChecked(),"automatic saving should be opt-in");
 const QString folder=root.filePath("Immagini ricevute è");
 w.m_settings.sstvOutputFolder=folder;w.m_editSstvOutputFolder->setText(folder);w.m_chkSstvAutoSave->setChecked(true);
 AppSettings reload;reload.load();check(reload.sstvAutoSave&&reload.sstvOutputFolder==folder,"auto-save preferences not persisted");
 // R24's wide-raster trims must survive the same settings load/save path.
 AppSettings wide=reload;wide.sstvMode="PD290";wide.sstvHorizontalShiftPixels=600;wide.sstvRedShiftPixels=200;wide.sstvBlueShiftPixels=-200;check(wide.save(),"save wide raster settings");
 AppSettings loadedWide;loadedWide.load();check(loadedWide.sstvHorizontalShiftPixels==600&&loadedWide.sstvRedShiftPixels==200&&loadedWide.sstvBlueShiftPixels==-200,"valid R24 wide-raster trims lost on reload");w.savePersistentSettings();
 QStringList saved,failures;
 QObject::connect(w.m_sstvImageArchive,&SstvImageArchive::imageSaved,&app,[&](const QString &path){saved<<path;});
 QObject::connect(w.m_sstvImageArchive,&SstvImageArchive::saveFailed,&app,[&](const QString &error){failures<<error;});
 QImage own(320,256,QImage::Format_RGB32);own.fill(Qt::blue);w.m_sstvTxBaseImage=own;w.m_sstvEditorProject.clear();
 {QSignalBlocker a(w.m_editSstvTxCall),b(w.m_editSstvTxName),c(w.m_editSstvTxQth),d(w.m_editSstvTxReport);w.m_editSstvTxCall->clear();w.m_editSstvTxName->clear();w.m_editSstvTxQth->clear();w.m_editSstvTxReport->clear();}
 w.updateSstvTxPreparedImage();const QImage ownBefore=w.m_txSourceImage;
 QImage received(160,120,QImage::Format_RGB32);for(int y=0;y<120;++y)for(int x=0;x<160;++x)received.setPixel(x,y,qRgb(x,y*2,(x+y)%256));
 auto receive=[&](bool events){SstvTransmitter tx(received,"Robot 24",12000);int count=0;
  while(!tx.isFinished()){AudioBlock b;b.sampleRate=12000;b.samples.resize(4096);b.samples.resize(tx.generate(b.samples.data(),b.samples.size()));w.m_rxDecoderWorker->command(w.m_sstvDecoder,&SstvDecoder::processAudioBlock,true,b);if(events&&++count%16==0)app.processEvents();}};
 receive(true);barrier();const QImage completion=w.m_rxDecoderWorker->image(w.m_sstvDecoder);
 QElapsedTimer timer;timer.start();while(saved.size()<1&&timer.elapsed()<5000){app.processEvents();QThread::msleep(2);}app.processEvents();
 check(saved.size()==1&&failures.isEmpty(),"real completed frame was not saved exactly once");
 check(QImage(saved.first())==completion,"saved PNG differs from completion image");check(w.m_sstvArchiveStatus==2,"successful save not reflected in RX status");
 check(QFileInfo(saved.first()).fileName().contains("_Robot24"),"completion mode missing from filename");
 check(w.m_sstvTxBaseImage==own&&w.m_txSourceImage==ownBefore&&w.m_sstvEditorProject.isEmpty(),"received image changed own TX/QSL composition");
 const QByteArray archived=bytes(saved.first());w.ui->spinSstvHorizontalShift->setValue(7);w.ui->spinSstvRedShift->setValue(3);barrier();
 check(bytes(saved.first())==archived&&QDir(folder).entryList({"*.png"},QDir::Files).size()==1,"post-RX adjustment resaved or changed archived PNG");
 w.m_chkSstvAutoSave->setChecked(false);receive(true);barrier();check(saved.size()==1&&QDir(folder).entryList({"*.png"},QDir::Files).size()==1,"disabled saving wrote another image");
 w.m_chkSstvAutoSave->setChecked(true);
 for(const QString &lang:{QString("en"),QString("it"),QString("fr"),QString("de"),QString("no"),QString("cs")}){
  w.m_uiLanguageCode=lang;w.loadUiTranslationFile(lang);w.applyUiLanguage();w.resize(1280,900);app.processEvents();
  check(w.width()==1280&&w.ui->cmbMode->currentText()==SstvDecoder::modeName(),"archive UI changed window width or active mode");
  auto *scroll=w.findChild<QScrollArea*>("modeSettingsScroll");check(scroll,"scroll missing");scroll->ensureWidgetVisible(w.m_lblSstvArchiveStatus);app.processEvents();app.processEvents();
  check(w.m_lblSstvArchiveStatus->visibleRegion().boundingRect().contains(w.m_lblSstvArchiveStatus->rect()),"archive status clipped");
  check(w.m_editSstvOutputFolder->isReadOnly()&&w.m_btnSstvBrowseFolder->isVisible()&&w.m_btnSstvOpenFolder->isVisible(),"archive folder controls missing");
  check(w.m_btnSstvOpenFolder->geometry().bottom()<w.m_lblSstvArchiveStatus->geometry().top(),"folder action overlaps archive status");
  if(lang=="it")w.grab().save("sstv-r25-auto-save-it.png");
 }
 w.m_uiLanguageCode="it";w.loadUiTranslationFile("it");w.applyUiLanguage();
 // A completed image still queued for the GUI at Close must be drained to disk.
 receive(false);const QImage closing=w.m_rxDecoderWorker->image(w.m_sstvDecoder);w.close();app.processEvents();
 const auto finalNames=QDir(folder).entryList({"*.png"},QDir::Files);check(finalNames.size()==2,"queued completion lost at shutdown");
 bool closingFound=false;for(const auto &name:finalNames)if(QImage(QDir(folder).filePath(name))==closing)closingFound=true;
 check(closingFound,"shutdown PNG differs from queued completed image");
 check(!w.m_txRunning,"unexpected TX");
 AppSettings cleanup;cleanup.load();cleanup.sstvAutoSave=false;cleanup.sstvOutputFolder.clear();cleanup.save();
 std::cout<<"PASS actual SSTV completion, exact PNG, own QSL isolation, disabled/adjustment behaviour, settings including wide trims, six-language UI and queued completion at shutdown\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
