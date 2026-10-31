// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "runtime/SessionDiagnostics.h"
#include "GzipLogRead.h"
#include "runtime/SessionDiagnosticsUiTap.h"
#include "modems/ft8/tx/Ft8Transmitter.h"
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QTimer>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QKeyEvent>
#include <iostream>
#include <cstdlib>
static void check(bool v,const char *s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
int main(int argc,char **argv){QApplication app(argc,argv);app.setApplicationName("MadModem");check(argc==2,"pass aligned FT8 WAV");QTemporaryDir temporary;check(temporary.isValid(),"temporary folder");
 QFile managed(QCoreApplication::applicationDirPath()+"/managed-install.flag");bool owned=!managed.exists();if(owned){check(managed.open(QIODevice::WriteOnly),"managed test marker");managed.close();}
 QSettings initial(AppSettings::settingsFilePath(),QSettings::IniFormat);initial.clear();initial.setValue("Updates/checkAtStartup",false);initial.setValue("Diagnostics/sessionEnabled",false);initial.setValue("QRZ/password","SECRET_PROFILE_PROBE");initial.sync();
 SessionDiagnostics::installQtCapture();SessionDiagnosticsUiTap tap;app.installEventFilter(&tap);QString path,closePath;
 {
 MainWindow w;w.resize(1280,900);w.show();app.processEvents();const QString folder=temporary.path()+QString::fromUtf8("/Sessione è");
 auto settings=[&](bool enable,bool accept){QTimer::singleShot(0,&w,[&w,folder,enable,accept]{auto *d=qobject_cast<AppSettingsDialog*>(QApplication::activeModalWidget());check(d,"actual settings dialog");auto *flag=d->findChild<QCheckBox*>("sessionDiagnosticsEnabled");auto *edit=d->findChild<QLineEdit*>("sessionDiagnosticsFolder");check(flag&&edit,"diagnostics controls");flag->setChecked(enable);edit->setText(folder);if(accept)d->accept();else d->reject();});w.showAppSettingsDialogPage(AppSettingsDialog::InitialPage::Diagnostics);app.processEvents();};
 settings(true,false);check(!SessionDiagnostics::instance().enabled(),"Cancel does not start logging");settings(true,true);check(SessionDiagnostics::instance().enabled(),"OK starts logging immediately");path=SessionDiagnostics::instance().path();AppSettings loaded;loaded.load();check(loaded.sessionDiagnosticsEnabled&&loaded.sessionDiagnosticsFolder==folder,"preferences reload");
 w.appendLog("diagnostic native runtime marker");qWarning("diagnostic native Qt warning");
 w.m_rttyDecoder->characterReceived("RTTY trace probe");w.m_bpsk31Decoder->characterReceived("BPSK trace probe");w.m_mfskDecoder->characterReceived("MFSK trace probe");w.m_cwDecoder->priorityTextReceived(0,"CW trace probe");
 w.m_q65Decoder->statusChanged("Q65 trace probe");QImage preview(16,16,QImage::Format_RGB32);preview.fill(Qt::black);w.m_hellDecoder->imageUpdated(preview);

 QLineEdit password;password.setObjectName("nativePasswordProbe");password.setEchoMode(QLineEdit::Password);password.show();QKeyEvent key(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"SECRET_KEY_PROBE");QApplication::sendEvent(&password,&key);password.close();
 AudioBlock block;block.sampleRate=12000;block.samples.fill(.02f,240);block.captureSequence=1;block.captureGeneration=1;block.firstSampleUtcNs=QDateTime::currentMSecsSinceEpoch()*1000000;block.captureSpanNs=20000000;SessionDiagnostics::instance().audio("capture",block);BoundedAudioDispatcher dispatcher(4);dispatcher.enqueue(block);check(dispatcher.takePending(1).size()==1,"actual dispatcher route");
 auto *decoder=w.m_ft8RxDecoder;const QString wav=QString::fromLocal8Bit(argv[1]);QMetaObject::invokeMethod(decoder,[decoder,wav]{decoder->setModeName("FT8");decoder->analyzeAudioFile(wav);},Qt::BlockingQueuedConnection);app.processEvents();
 Ft8Transmitter tx("FT4","CQ IZ6NNH JN63",48000,1500);QVector<float> pcm(360000);int n=tx.generate(pcm.data(),pcm.size());QVector<double> ft4(90000);for(int i=0;i<n/4&&4032+i<ft4.size();++i)ft4[4032+i]=pcm[i*4]*.15;
 Ft8RxDecoder rx;rx.setModeName("FT4");Ft8RxDecoder::PerfStats stats;stats.phase="boundary";auto rows=rx.decodeSlot(ft4,QDateTime::currentDateTimeUtc().addMSecs(-7500),nullptr,&stats);check(!rows.isEmpty(),"actual FT4 synthesis/decoder trace");
 for(const QString &lang:{"en","it","fr","de","no","cs"}){w.m_uiLanguageCode=lang;w.loadUiTranslationFile(lang);w.applyUiLanguage();AppSettingsDialog d(w.m_settings,QString(),QString(),{},[&w](const QString &s){return w.uiTextFromSource("text",s);},{},AppSettingsDialog::InitialPage::Diagnostics,&w);d.resize(1100,780);d.show();app.processEvents();auto *flag=d.findChild<QCheckBox*>("sessionDiagnosticsEnabled");check(flag&&flag->isVisible()&&flag->width()>100,"six-language checkbox visibility");check(d.rect().contains(QRect(flag->mapTo(&d,QPoint()),flag->size())),"checkbox within dialog");if(lang=="it")d.grab().save("r27-settings-it.png");d.reject();}
 settings(false,true);check(!SessionDiagnostics::instance().enabled(),"disable closes file immediately");loaded.load();check(!loaded.sessionDiagnosticsEnabled,"disable persists");
 settings(true,true);closePath=SessionDiagnostics::instance().path();
 w.close();app.processEvents();
 }
 SessionDiagnostics::instance().stop();SessionDiagnostics::restoreQtCapture();auto bytes=readGzipLog(path);check(!bytes.contains("SECRET_"),"no profile/password key text in log");
 QSet<QString> categories,modes,textModes;int candidates=0;bool rejected=false,accepted=false,decoderPerf=false,guiPerf=false;
 for(const auto &line:bytes.split('\n')){if(line.isEmpty())continue;QJsonParseError err;auto o=QJsonDocument::fromJson(line,&err).object();check(err.error==QJsonParseError::NoError,"native JSONL integrity");auto event=o["event"].toString();categories.insert(event);if(event=="rx.text")textModes.insert(o["mode"].toString());if(event=="ft.candidate"){++candidates;modes.insert(o["mode"].toString());if(o["accepted"].toBool())accepted=true;else rejected=true;}if(event=="ft.performance"){if(o["stage"]=="gui.delivery")guiPerf=true;else decoderPerf=true;}}
 for(const auto &c:{"runtime.log","qt.message","ui.event","audio.block","settings.snapshot","settings.persisted","ft.candidate","ft.job.begin","ft.job.end","ft.search","ft.pass","ft.performance","session.end"})check(categories.contains(c),c);
 check(textModes.contains("RTTY")&&textModes.contains("BPSK")&&textModes.contains("MFSK")&&textModes.contains("CW")&&categories.contains("rx.status")&&categories.contains("rx.image.progress"),"other-mode text/status/image hooks");
 check(candidates>100&&accepted&&rejected&&modes.contains("FT8")&&modes.contains("FT4")&&decoderPerf&&guiPerf,"FT8/FT4 attempts and complete producer/GUI telemetry");
 auto shutdownBytes=readGzipLog(closePath);check(shutdownBytes.contains("MainWindow destructor done")&&shutdownBytes.contains("session.end"),"enabled close records complete shutdown");
 AppSettings cleanup;cleanup.load();cleanup.sessionDiagnosticsEnabled=false;check(cleanup.save(),"test preference cleanup");
 if(owned)managed.remove();std::cout<<"PASS actual Settings Cancel/OK/live-enable/disable/prefs, six languages, runtime/Qt/UI/audio/dispatcher, FT8 WAV and FT4 synthesis, candidate rejects/success, producer/GUI stats, redaction and drain; candidates="<<candidates<<'\n';
}
