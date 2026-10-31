#include "runtime/SstvImageArchive.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *why){if(!b)throw std::runtime_error(why);}
static QImage picture(int value){QImage image(320,256,QImage::Format_RGB32);image.fill(QColor(value,80,170));return image;}
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{
 QTemporaryDir root;check(root.isValid(),"temporary archive root");const QString folder=root.filePath("Ricevute con spazi e accenti è");
 const QDateTime stamp=QDateTime::fromString("2026-10-26T12:34:56Z",Qt::ISODate);
 QDir().mkpath(folder);const QString existing=QDir(folder).filePath("2026-10-26_123456_Robot36.png");
 QFile sentinel(existing);check(sentinel.open(QIODevice::WriteOnly),"collision fixture");sentinel.write("existing user image");sentinel.close();
 QStringList saved,failed;
 SstvImageArchive archive;
 QObject::connect(&archive,&SstvImageArchive::imageSaved,&app,[&](const QString &p){saved<<p;});
 QObject::connect(&archive,&SstvImageArchive::saveFailed,&app,[&](const QString &e){failed<<e;});
 check(!archive.enqueue(picture(20),"Robot 36",stamp),"disabled archive accepted image");
 archive.configure(true,folder);
 QImage mutableImage=picture(20);check(archive.enqueue(mutableImage,"Robot 36",stamp),"first image enqueue");mutableImage.fill(Qt::green);
 check(archive.enqueue(picture(60),"Robot 36",stamp),"collision image enqueue");
 const QString second=root.filePath("Second folder");archive.configure(true,second);
 check(archive.enqueue(picture(100),"../../Robot /36",stamp),"sanitized name enqueue");
 archive.configure(false,second);check(!archive.enqueue(picture(120),"PD120",stamp),"disabled setting ignored");
 archive.finish();app.processEvents();for(const auto &error:failed)std::cerr<<error.toStdString()<<"\n";check(saved.size()==3&&failed.isEmpty(),"accepted jobs not drained successfully");
 check(QImage(saved[0])==picture(20),"queued image mutated after enqueue");check(QImage(saved[1])==picture(60),"second image changed");
 check(QFileInfo(saved[0]).fileName()=="2026-10-26_123456_Robot36_001.png"&&QFileInfo(saved[1]).fileName()=="2026-10-26_123456_Robot36_002.png","collision sequence");
 check(QFileInfo(saved[0]).absolutePath()==folder&&QFileInfo(saved[2]).absolutePath()==second,"folder not captured per image");
 check(QFileInfo(saved[2]).fileName()=="2026-10-26_123456_Robot36.png","unsafe filename not sanitized");
 check(sentinel.open(QIODevice::ReadOnly)&&sentinel.readAll()=="existing user image","existing image overwritten");sentinel.close();
 check(QDir(folder).entryList({"*.part"},QDir::Files|QDir::Hidden).isEmpty(),"temporary file leaked after success");
 // Two independent workers race for the same UTC+mode stem.
 const QString race=root.filePath("parallel");SstvImageArchive a,b;a.configure(true,race);b.configure(true,race);
 check(a.enqueue(picture(40),"PD120",stamp)&&b.enqueue(picture(80),"PD120",stamp),"parallel enqueue");a.finish();b.finish();
 const auto names=QDir(race).entryList({"*.png"},QDir::Files);check(names.size()==2,"parallel archives clobbered an image");
 const QImage first(QDir(race).filePath(names[0])),last(QDir(race).filePath(names[1]));
 check((first==picture(40)&&last==picture(80))||(first==picture(80)&&last==picture(40)),"parallel archive pixels");
 // A failed destination does not disable subsequent valid saves.
 const QString blocker=root.filePath("blocker");QFile file(blocker);file.open(QIODevice::WriteOnly);file.write("not a directory");file.close();
 SstvImageArchive errors;QStringList errorsSeen,valid;
 QObject::connect(&errors,&SstvImageArchive::saveFailed,&app,[&](const QString &e){errorsSeen<<e;});
 QObject::connect(&errors,&SstvImageArchive::imageSaved,&app,[&](const QString &p){valid<<p;});
 errors.configure(true,blocker+"/child");check(errors.enqueue(picture(10),"Robot 24",stamp),"bad destination should fail asynchronously");
 errors.configure(true,root.filePath("recovered"));check(errors.enqueue(picture(30),"Robot 24",stamp),"recovery enqueue");
 errors.finish();app.processEvents();check(errorsSeen.size()==1&&valid.size()==1,"write failure/recovery notification");
 check(QImage(valid.first())==picture(30),"valid image after failure missing");
 // Queue pressure is explicit, bounded and leaves accepted images readable.
 SstvImageArchive pressure;pressure.configure(true,root.filePath("pressure"));int accepted=0,rejected=0;
 QImage large(800,616,QImage::Format_RGB32);for(int y=0;y<616;++y)for(int x=0;x<800;++x)large.setPixel(x,y,qRgb(x%256,y%256,(x*y)%256));
 for(int i=0;i<100;++i){if(pressure.enqueue(large,"PD290",stamp))++accepted;else ++rejected;}
 pressure.finish();check(rejected>0,"bounded queue did not report pressure");
 check(QDir(root.filePath("pressure")).entryList({"*.png"},QDir::Files).size()==accepted,"accepted images lost at finish");
 for(const auto &name:QDir(root.filePath("pressure")).entryList({"*.png"},QDir::Files))check(QImage(QDir(root.filePath("pressure")).filePath(name))==large,"queued image corrupted");
 std::cout<<"PASS archive: disabled mode, immutable image/folder, UTC names, collisions/concurrent writers, Unicode paths, error recovery, bounded queue and shutdown drain\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
