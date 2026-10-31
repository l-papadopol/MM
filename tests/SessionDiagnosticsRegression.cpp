// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/SessionDiagnostics.h"
#include "GzipLogRead.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif
#include <iostream>
#include <cstdlib>
#include <thread>
#include <vector>
static void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
static QVector<QJsonObject> read(const QString &p){QVector<QJsonObject> rows;for(const auto &line:readGzipLog(p).split('\n')){if(line.isEmpty())continue;QJsonParseError err;auto d=QJsonDocument::fromJson(line,&err);check(err.error==QJsonParseError::NoError,"valid JSONL");rows.append(d.object());}return rows;}
int main(int argc,char **argv){QCoreApplication app(argc,argv);app.setApplicationName("MadModemDiagnosticsRegression");
 if(argc==3&&QString(argv[1])=="--fatal"){
#ifdef Q_OS_UNIX
  const rlimit noCore{0,0};setrlimit(RLIMIT_CORE,&noCore);
#endif
  SessionDiagnostics::installQtCapture();check(SessionDiagnostics::instance().start(QString::fromLocal8Bit(argv[2])),"fatal start");qFatal("diagnostic fatal regression");}
 QTemporaryDir dir;check(dir.isValid(),"temporary folder");SessionDiagnostics log;
 check(!log.enabled(),"default disabled");log.record("off");check(QDir(dir.path()).entryList(QDir::Files).isEmpty(),"off creates no files");
 const auto folder=dir.path()+QString::fromUtf8("/log con spazi è");check(log.start(folder),"start Unicode folder");const auto first=log.path();
 log.record("periodic-flush");QThread::msleep(350);
 check(readGzipLog(first,true).contains("periodic-flush"),"idle periodic flush exposes recent events without explicit barrier");
 std::vector<std::thread> writers;for(int t=0;t<6;++t)writers.emplace_back([&,t]{for(int i=0;i<500;++i)log.record("concurrent",{{"producer",t},{"item",i}});});for(auto &t:writers)t.join();
 log.record("secrets",{{"password","SECRET_A"},{"nested",QJsonObject{{"token","SECRET_B"},{"message","Authorization: Bearer SECRET_C password=SECRET_D"}}}});
 check(log.flush(),"explicit flush barrier");
 check(first.endsWith(".jsonl.gz"),"standard gzip filename");
 const auto live=readGzipLog(first,true);check(live.contains("SECRET_")==false&&live.contains("[redacted]"),"flushed live gzip is readable and redacted before shutdown");
 check(!live.contains("session.end"),"live log is not falsely marked complete");
 log.stop();auto rows=read(first);int count=0;quint64 last=0;
 for(const auto &r:rows){if(r.contains("seq")){const auto seq=r["seq"].toString().toULongLong();check(seq==last+1,"strict admission order");last=seq;}if(r["event"]=="concurrent")++count;}
 check(count==3000&&log.lost()==0,"all concurrent records retained");check(rows.last()["event"]=="session.end"&&rows.last()["complete"].toBool(),"shutdown drains and writes complete footer");
 const auto plain=readGzipLog(first);check(!plain.contains("SECRET_"),"recursive redaction after decompression");
 check(QFileInfo(first).size()*2<plain.size(),"gzip materially reduces representative JSONL size");
 const auto exportPath=qEnvironmentVariable("MADMODEM_DIAGNOSTICS_TEST_EXPORT");if(!exportPath.isEmpty())check(QFile::copy(first,exportPath),"export compressed test fixture");
 check(log.start(folder),"restart");const auto second=log.path();check(first!=second,"unique path preserves earlier file");log.stop();check(read(first).size()==rows.size(),"no overwrite");
 QFile blocker(dir.path()+"/blocker");check(blocker.open(QIODevice::WriteOnly),"create blocker");blocker.write("x");blocker.close();check(!log.start(blocker.fileName()+"/sub"),"bad folder reports failure");check(!log.error().isEmpty(),"visible error");check(log.start(folder),"recover after error");log.stop();
 SessionDiagnostics pressure(64);check(pressure.start(folder),"pressure start");for(int i=0;i<100;++i)pressure.record("pressure",{{"payload",QString(500,'x')}});pressure.stop();auto losses=read(pressure.path());bool marker=false;for(const auto &r:losses)if(r["event"]=="diagnostic.loss")marker=true;check(marker&&pressure.lost()>0&&!losses.last()["complete"].toBool(),"overflow explicitly marked incomplete");
 const auto crashFolder=dir.path()+"/crash";QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--fatal",crashFolder});
 check(child.waitForFinished(10000)&&child.exitCode()!=0,"fatal child terminated");
 const auto crashes=QDir(crashFolder).entryList({"*.jsonl.gz"},QDir::Files);check(crashes.size()==1,"fatal log preserved");
 const auto recovered=readGzipLog(QDir(crashFolder).filePath(crashes.first()),true);
 check(recovered.contains("diagnostic fatal regression")&&!recovered.contains("session.end"),"fatal barrier preserves readable prefix without a false complete footer");
 std::cout<<"PASS gzip CRC/JSONL / live flush / fatal prefix / 3000 concurrent events / sequence / drain / Unicode / collision / error-recovery / decompressed redaction / overflow; plain="<<plain.size()<<" gzip="<<QFileInfo(first).size()<<'\n';
}
