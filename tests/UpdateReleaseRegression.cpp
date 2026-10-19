#include "updates/UpdateController.h"
#include "settings/AppSettings.h"
#include "MadModemVersion.h"
#include "MadModemBuildInfo.h"
#include <QApplication>
#include <QWidget>
#include <QMenu>
#include <QTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QSettings>
#include <QJsonArray>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char *what){if(!ok)throw std::runtime_error(what);}
static QJsonObject release(QString tag,bool pre=false,bool draft=false,QString body={}){
    return {{"tag_name",tag},{"name","An arbitrary edited title"},{"prerelease",pre},{"draft",draft},{"body",body}};
}
class SimulatedUpdate : public UpdateController {
public:
    QJsonArray pages[2]; QStringList requested; QJsonObject chosen;
    std::function<void()> finished;
    SimulatedUpdate(QWidget *w,QMenu *menu,std::function<bool()> idle):UpdateController(w,menu,idle){}
private:
    void fetch(const QUrl &url,qint64,std::function<void(QByteArray)> done) override {
        requested.append(url.toString());
        const int page = QUrlQuery(url).queryItemValue("page").toInt();
        ::check(page==1 || page==2,"unexpected pagination request");
        const auto bytes=QJsonDocument(pages[page-1]).toJson();
        QTimer::singleShot(0,this,[done,bytes]{done(bytes);});
    }
    void offer(const QJsonObject &release) override {chosen=release;if(finished)finished();}
};
int main(int argc,char **argv){
    QApplication app(argc,argv);
    auto newer=[](const QJsonObject&r,QString v="0.6.4",quint64 b=114){return UpdateController::newerPublishedVersion(r,v,b);};
    check(newer(release("build-0.6.4-115",true))=="0.6.4","newer prerelease build ignored");
    check(newer(release("build-0.6.4-115",false))=="0.6.4","unmarked build ignored");
    check(newer(release("build-0.6.4-114",true)).isEmpty(),"same build offered again");
    check(newer(release("build-0.6.4-113",false)).isEmpty(),"older build offered");
    check(newer(release("build-0.6.3-999",true)).isEmpty(),"public-version downgrade offered");
    check(newer(release("v0.6.5",true))=="0.6.5","higher prerelease version ignored");
    check(newer(release("v0.6.5",false,true)).isEmpty(),"draft offered");
    check(newer(release("v0.6.4",false,false,"Operator notes\n<!-- MadModem build: 116 -->"))=="0.6.4","version-tag rebuild ignored");
    check(newer(release("v0.6.4",false,false,"<!-- MadModem build: 114 -->")).isEmpty(),"same stamped build repeated");
    check(newer(release("build-0.6.4-115",true),"0.6.4",0)=="0.6.4","unidentified local build cannot acquire official identity");
    check(newer(release("v0.6.4"),"0.6.3",0)=="0.6.4","old client version bridge missing");
    check(newer(release("v0.6.4")).isEmpty(),"unknown same-version tag assumed newer");
    check(newer(release("main")).isEmpty(),"unversioned branch interpreted as release");
    check(newer(release("build-0.6.4-9999999999999999999999999")).isEmpty(),"overflow build accepted");
    check(newer(release("build-0.6.4-115",false,false,"<!-- MadModem build: 116 -->")).isEmpty(),"contradictory identity accepted");
    check(UpdateController::releaseBuildNumber(release("build-0.6.4-115",true))==115,"build not extracted");
    auto oldLatest=release("v0.6.3");oldLatest.insert("make_latest",true);
    auto newest=release("build-0.6.4-116",true);newest.insert("make_latest",false);
    auto best=UpdateController::newestPublishedRelease(QJsonArray{newest,release("v9.0.0",false,true),oldLatest,release("build-0.6.4-115",true)});
    check(best.value("tag_name")=="build-0.6.4-116","Latest badge or API order won over newer prerelease");
    auto promoted=release("v0.6.4",false,false,"<!-- MadModem build: 117 -->");
    best=UpdateController::newestPublishedRelease(QJsonArray{newest,promoted});
    check(best.value("tag_name")=="v0.6.4","stamped version release not ranked by build");
    QJsonArray firstPage;
    for(int i=0;i<100;++i)firstPage.append(release("v0.6.3"));
    auto page1=UpdateController::newestPublishedRelease(firstPage);
    auto page2=UpdateController::newestPublishedRelease(QJsonArray{newest});
    best=UpdateController::newestPublishedRelease(QJsonArray{page1,page2});
    check(best.value("tag_name")=="build-0.6.4-116","paged candidate merge lost the newest build");
    QSettings settings(AppSettings::settingsFilePath(),QSettings::IniFormat);
    settings.setValue("Updates/checkAtStartup",false);settings.sync();
    QWidget window;QMenu menu;bool idle=false;
    SimulatedUpdate updater(&window,&menu,[&]{return idle;});
    updater.pages[0]=firstPage;
    const auto number=QStringLiteral(MADMODEM_BUILD_NUMBER).toULongLong()+1;
    const QString newTag="build-"+QStringLiteral(MADMODEM_VERSION_STRING)+"-"+QString::number(number);
    updater.pages[1]=QJsonArray{release(newTag,true)};
    updater.check();check(updater.requested.isEmpty(),"automatic check did not defer active operation");
    idle=true;QEventLoop loop;updater.finished=[&]{loop.quit();};
    QTimer::singleShot(3000,&loop,&QEventLoop::quit);updater.check(true);loop.exec();
    check(updater.requested.size()==2,"release pages were not retrieved");
    check(updater.chosen.value("tag_name")==newTag,"check did not offer the next-page prerelease");
    check(updater.requested[0].contains("/releases?") && !updater.requested[0].contains("/latest"),"checker still uses Latest endpoint");
    std::cout<<"PASS real checker paginates simulated responses, defers busy operation, and reaches the offer for a newer prerelease\n";
    std::cout<<"PASS published stable/prerelease selection, Latest independence, version/build ordering, repeat/downgrade/draft/invalid rejection, local bridge and paged merge\n";
}
