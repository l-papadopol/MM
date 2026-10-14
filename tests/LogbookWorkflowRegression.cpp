#include "dialogs/LogbookDialog.h"
#include "widgets/QsoMapWidget.h"
#include "settings/AppSettings.h"
#include "audio/TxPlaybackWatchdog.h"
#include "utils/CockpitTheme.h"
#include "utils/RuntimeI18n.h"
#include <QApplication>
#include <QTableWidget>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDateEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QAction>
#include <QTemporaryDir>
#include <QFile>
#include <QFileDialog>
#include <QTimer>
#include <QSettings>
#include <QVBoxLayout>
#include <QTextStream>
#include <stdexcept>
void check(bool result,const char* message) {if(!result) throw std::runtime_error(message);QTextStream(stdout)<<"PASS "<<message<<'\n';}
struct RestoreSettings {
 QString path=AppSettings::settingsFilePath(); QByteArray bytes; bool existed=false;
 RestoreSettings() {QFile f(path);existed=f.exists();if(f.open(QIODevice::ReadOnly)) bytes=f.readAll();}
 ~RestoreSettings() {if(existed){QFile f(path);if(f.open(QIODevice::WriteOnly))f.write(bytes);}else QFile::remove(path);}
};
int main(int argc,char** argv) {
 QApplication app(argc,argv);app.setAttribute(Qt::AA_DontUseNativeDialogs);
 RestoreSettings restore;
 try {
  QTemporaryDir temp;const auto path=temp.filePath("log.adi");
  QVector<LogbookEntry> contacts;
  const auto date=QDateTime(QDate(2026,10,7),QTime(17,0),Qt::UTC);
  for(int i=0;i<5;++i) {LogbookEntry e;e.callsign=QString("F%1ABC").arg(i+1);e.stationCallsign="IZ6NNH";e.band=i==3?"12m":"2m";e.mode=i==4?"FT4":"FT8";e.freq=e.band=="2m"?"144.174":"24.915";e.utc=date.addSecs(i*60);e.grid="JN18";e.rstSent="-10";e.rstReceived="-12";contacts.append(e);}
  QFile file(path);check(file.open(QIODevice::WriteOnly),"fixture opens");file.write(AdifLogbook::recordsToAdif(contacts).toUtf8());file.close();
  AdifLogbook log(path);QString error;check(log.load(&error),"fixture loads");
  for(const auto& theme:{QStringLiteral("classic_dark"),QStringLiteral("qt_default"),QStringLiteral("avionica")}) {
   MadModemUi::applyUiTheme(app,theme);MadModemI18n::setLanguageCode("it");
   LogbookDialog dialog(&log);dialog.setTextTranslator([](const QString& text){return MadModemI18n::text(text);});dialog.resize(960,640);dialog.show();app.processEvents();
   auto* table=dialog.findChild<QTableWidget*>("logbookTable");
   auto* band=dialog.findChild<QComboBox*>("logbookBand");auto* mode=dialog.findChild<QComboBox*>("logbookMode");
   auto* period=dialog.findChild<QComboBox*>("logbookPeriod");auto* scope=dialog.findChild<QComboBox*>("logbookOutputScope");
   auto refresh=[&]{QMetaObject::invokeMethod(&dialog,"refreshTable");app.processEvents();};
   check(table && table->rowCount()==5,"all records visible initially");
   band->setCurrentText("2m");mode->setCurrentText("FT8");refresh();
   check(table->rowCount()==3,"2m and FT8 filters exclude 12m and FT4");
   period->setCurrentIndex(4);
   dialog.findChild<QDateTimeEdit*>("logbookFromUtc")->setDateTime(date.addSecs(60));
   dialog.findChild<QDateTimeEdit*>("logbookUntilUtc")->setDateTime(date.addSecs(120));refresh();
   check(table->rowCount()==1 && table->item(0,1)->text()=="F2ABC","UTC interval includes start and excludes end");
   period->setCurrentIndex(0);refresh();table->sortItems(1,Qt::DescendingOrder);table->selectRow(0);
   check(scope->currentIndex()==1 && scope->currentText().contains("(1)"),"selection switches output scope with count");
   if(theme=="classic_dark") {
    const auto exported=temp.filePath("selected.adi");
    bool sawFileDialog=false;
    QTimer::singleShot(0,&dialog,[&]{
      auto* save=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
      if(save) {sawFileDialog=true;save->selectFile(exported);QMetaObject::invokeMethod(save,"accept");}
      else if(auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget())) modal->reject();
    });
    dialog.findChild<QAction*>("logbookExportAdif")->trigger();
    QFile result(exported);check(sawFileDialog && result.open(QIODevice::ReadOnly),"ADIF export opens file picker directly, without options wizard");
    const auto records=AdifLogbook::parseAdif(QString::fromUtf8(result.readAll()));
    check(records.size()==1 && records.first().callsign=="F3ABC","sorted selection exports the correct contact only");
   }
   dialog.findChild<QPushButton*>("logbookClearSelection")->click();check(scope->currentIndex()==0,"clearing selection returns to visible scope");
   dialog.findChild<QPushButton*>("logbookSelectVisible")->click();check(scope->currentText().contains("(3)"),"select visible uses the filtered set");
   check(dialog.size().width()<=960 && table->height()>200,"compact screen retains usable table area");
   if(qEnvironmentVariableIsSet("MM_WORKFLOW_SCREENSHOTS")) dialog.grab().save("logbook-"+theme+".png");
  }
  // Exercise independent clocks, including a sink clock ticking without pulls.
  TxPlaybackWatchdog watchdog;watchdog.reset(85);
  check(watchdog.observe(0,4096,0,false)==TxPlaybackWatchdog::Failure::None,"buffer priming is allowed");
  check(watchdog.observe(1600,8192,0,false)==TxPlaybackWatchdog::Failure::PlaybackStalled,"new PCM cannot mask stopped playback");
  watchdog.reset(85);watchdog.observe(0,4096,1,false);
  check(watchdog.observe(1600,4096,1600000,false)==TxPlaybackWatchdog::Failure::SourceStalled,"backend clock cannot mask stopped source");
  watchdog.reset(1400);watchdog.observe(0,100000,1,true);
  check(watchdog.observe(1800,100000,1800000,true)==TxPlaybackWatchdog::Failure::None,"EOF may drain a large backend buffer without further pulls");
  check(!AppSettings{}.rotatorTrackSelectedQso,"new settings default to manual pointing");
  {QSettings legacy(AppSettings::settingsFilePath(),QSettings::IniFormat);legacy.setValue("Rotator/trackSelectedQso",true);legacy.remove("Rotator/explicitQsoTracking");legacy.sync();}
  AppSettings migrated;migrated.load();check(!migrated.rotatorTrackSelectedQso,"legacy automatic default migrates to manual pointing");
  migrated.rotatorTrackSelectedQso=true;check(migrated.save(),"explicit tracking preference saves");
  AppSettings enabled;enabled.load();check(enabled.rotatorTrackSelectedQso,"explicit opt-in survives restart");
  // Test map controls without contacting a tile server.
  qputenv("MADMODEM_OSM_TILE_BASE","http://127.0.0.1:9/{z}/{x}/{y}.png");
  QWidget page;QVBoxLayout layout(&page);QsoMapWidget map;map.setTextTranslator([](const QString& text){return MadModemI18n::text(text);});map.setRecords(contacts);map.setModeFilter("FT8");map.setHomeGrid("JN63");
  auto* controls=map.createControls(&page);layout.addWidget(controls);layout.addWidget(&map);page.resize(960,640);page.show();app.processEvents();
  auto* source=controls->findChild<QComboBox*>("mapSource");auto* period=controls->findChild<QComboBox*>("mapPeriod");
  source->setCurrentIndex(1);check(map.displayBehavior()==QsoMapWidget::DisplayBehavior::HeardToday && !period->isEnabled(),"heard-today source exposes fixed date scope");
  source->setCurrentIndex(0);period->setCurrentIndex(period->findData("custom"));
  auto* from=controls->findChild<QDateEdit*>("mapFromDate");auto* until=controls->findChild<QDateEdit*>("mapUntilDate");
  from->setDate(QDate(2026,10,7));until->setDate(QDate(2026,10,7));
  check(from->isVisible() && until->date()>=from->date(),"custom map dates accessible without modal dialog");
  controls->findChild<QComboBox*>("mapBand")->setCurrentIndex(controls->findChild<QComboBox*>("mapBand")->findData("2m"));
  check(controls->geometry().bottom()<map.geometry().top(),"controls do not cover map canvas");
  if(qEnvironmentVariableIsSet("MM_WORKFLOW_SCREENSHOTS")) page.grab().save("qso-map-workflow.png");
  layout.removeWidget(&map);map.setParent(nullptr);
 } catch(const std::exception& e) {QTextStream(stderr)<<"FAIL "<<e.what()<<'\n';return 1;}
 return 0;
}
