// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/SessionDiagnostics.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <vector>
static void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
static QVector<QJsonObject> read(const QString &p){QFile f(p);check(f.open(QIODevice::ReadOnly),"open log");QVector<QJsonObject> rows;while(!f.atEnd()){QJsonParseError err;auto d=QJsonDocument::fromJson(f.readLine(),&err);check(err.error==QJsonParseError::NoError,"valid JSONL");rows.append(d.object());}return rows;}
int main(int argc,char **argv){QCoreApplication app(argc,argv);app.setApplicationName("MadModemDiagnosticsRegression");
 if(argc==3&&QString(argv[1])=="--fatal"){SessionDiagnostics::installQtCapture();check(SessionDiagnostics::instance().start(QString::fromLocal8Bit(argv[2])),"fatal start");qFatal("diagnostic fatal regression");}
 QTemporaryDir dir;check(dir.isValid(),"temporary folder");SessionDiagnostics log;
 check(!log.enabled(),"default disabled");log.record("off");check(QDir(dir.path()).entryList(QDir::Files).isEmpty(),"off creates no files");
 const auto folder=dir.path()+QString::fromUtf8("/log con spazi è");check(log.start(folder),"start Unicode folder");const auto first=log.path();
 std::vector<std::thread> writers;for(int t=0;t<6;++t)writers.emplace_back([&,t]{for(int i=0;i<500;++i)log.record("concurrent",{{"producer",t},{"item",i}});});for(auto &t:writers)t.join();
 log.record("secrets",{{"password","SECRET_A"},{"nested",QJsonObject{{"token","SECRET_B"},{"message","Authorization: Bearer SECRET_C password=SECRET_D"}}}});
 check(log.flush(),"explicit flush barrier");log.stop();auto rows=read(first);int count=0;quint64 last=0;
 for(const auto &r:rows){if(r.contains("seq")){const auto seq=r["seq"].toString().toULongLong();check(seq==last+1,"strict admission order");last=seq;}if(r["event"]=="concurrent")++count;}
 check(count==3000&&log.lost()==0,"all concurrent records retained");check(rows.last()["event"]=="session.end"&&rows.last()["complete"].toBool(),"shutdown drains and writes complete footer");
 QFile secretFile(first);check(secretFile.open(QIODevice::ReadOnly),"secret check");check(!secretFile.readAll().contains("SECRET_"),"recursive redaction");
 check(log.start(folder),"restart");const auto second=log.path();check(first!=second,"unique path preserves earlier file");log.stop();check(read(first).size()==rows.size(),"no overwrite");
 QFile blocker(dir.path()+"/blocker");check(blocker.open(QIODevice::WriteOnly),"create blocker");blocker.write("x");blocker.close();check(!log.start(blocker.fileName()+"/sub"),"bad folder reports failure");check(!log.error().isEmpty(),"visible error");check(log.start(folder),"recover after error");log.stop();
 SessionDiagnostics pressure(64);check(pressure.start(folder),"pressure start");for(int i=0;i<100;++i)pressure.record("pressure",{{"payload",QString(500,'x')}});pressure.stop();auto losses=read(pressure.path());bool marker=false;for(const auto &r:losses)if(r["event"]=="diagnostic.loss")marker=true;check(marker&&pressure.lost()>0&&!losses.last()["complete"].toBool(),"overflow explicitly marked incomplete");
 std::cout<<"PASS default-off / 3000 concurrent events / sequence / flush / drain / Unicode / collision / error-recovery / secret redaction / explicit overflow\n";
}
