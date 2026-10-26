#include "dialogs/SstvImageEditorDialog.h"
#include <QApplication>
#include <QDataStream>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <iostream>
#include <stdexcept>
static void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
struct Overlay {int type;QString text;QImage image;QPoint pos;QFont font;QColor color;bool qso;QString key;};
struct Document {quint32 magic,version;QImage bg;bool frame;QStringList suppressed;QVector<Overlay> objects;QStringList fields;};
static Document parse(const QByteArray &data){QDataStream outer(data);outer.setVersion(QDataStream::Qt_5_12);quint32 magic;QByteArray canvas;Document d;QString a,b,c,e,f;outer>>magic>>canvas>>a>>b>>c>>e>>f;check(magic==0x4d4d5331,"project magic");d.fields=QStringList{a,b,c,e,f};QDataStream in(canvas);in.setVersion(QDataStream::Qt_5_12);quint32 count;in>>d.magic>>d.version>>d.bg>>d.frame;if(d.version==2)in>>d.suppressed;in>>count;for(quint32 i=0;i<count;++i){Overlay o;in>>o.type>>o.text>>o.image>>o.pos>>o.font>>o.color>>o.qso>>o.key;d.objects<<o;}check(in.status()==QDataStream::Ok,"parse project");return d;}
static QByteArray pack(const Document &d){QByteArray canvas,data;QDataStream c(&canvas,QIODevice::WriteOnly);c.setVersion(QDataStream::Qt_5_12);c<<d.magic<<d.version<<d.bg<<d.frame;if(d.version==2)c<<d.suppressed;c<<quint32(d.objects.size());for(const auto &o:d.objects)c<<o.type<<o.text<<o.image<<o.pos<<o.font<<o.color<<o.qso<<o.key;QDataStream out(&data,QIODevice::WriteOnly);out.setVersion(QDataStream::Qt_5_12);out<<quint32(0x4d4d5331)<<canvas;for(const auto &f:d.fields)out<<f;return data;}
static QPushButton *button(SstvImageEditorDialog &d,const QString &text){for(auto *b:d.findChildren<QPushButton*>())if(b->text()==text)return b;throw std::runtime_error("button missing");}
int main(int argc,char **argv){QApplication app(argc,argv);try {
 SstvImageEditorDialog editor;editor.show();editor.resize(800,600);app.processEvents();app.processEvents();
 check(editor.width()==800 && editor.height()==600,"editor exceeds 800x600 desktop");
 for(const QString &text:{QString("Open project..."),QString("Save project..."),QString("Save PNG..."),QString("Use for SSTV TX"),QString("Close")}) {auto *b=button(editor,text);check(b->isVisible()&&editor.rect().contains(b->mapTo(&editor,b->rect().center())),"primary action outside dialog");}
 auto blank=parse(editor.projectData());for(const auto &o:blank.objects)check(!o.text.contains("IZ6NNH")&&!o.text.contains("W6HN")&&!o.text.contains("599")&&!o.text.contains("Italy"),"sample QSO data in blank image");
 QImage bg(640,496,QImage::Format_RGB32);bg.fill(QColor(190,210,230));editor.setBackgroundImage(bg,"test");
 editor.setQsoFields("IZ6NNH","F4ABC","Jean","JN18","595");
 auto d=parse(editor.projectData());int target=-1;for(int i=0;i<d.objects.size();++i)if(d.objects[i].key=="big_call")target=i;check(target>=0,"DX field missing");
 d.objects[target].pos=QPoint(45,110);d.objects[target].color=Qt::blue;d.objects[target].font.setPointSize(21);
 check(editor.restoreProject(pack(d)),"restore editable project");const QImage image=editor.composedImage();
 SstvImageEditorDialog second;check(second.restoreProject(editor.projectData()),"reopen project");check(second.composedImage()==image,"reopened composition changed");check(second.qsoFields()==editor.qsoFields(),"project fields not preserved");
 editor.setQsoFields("IZ6NNH","G0JEI","John","IO91","575");auto after=parse(editor.projectData());
 check(after.objects[target].pos==QPoint(45,110)&&after.objects[target].font.pointSize()==21&&after.objects[target].color==Qt::blue,"field edit reset placement/style");check(after.objects[target].text=="G0JEI","field edit did not update text");
 const QImage before=editor.composedImage();button(editor,"Add text overlay")->click();const QImage added=editor.composedImage();check(added!=before,"add text no change");
 button(editor,"Undo")->click();check(editor.composedImage()==before,"undo failed");button(editor,"Redo")->click();check(editor.composedImage()==added,"redo failed");
 auto *elements=editor.findChild<QListWidget*>("sstvElements");check(elements&&elements->count()==parse(editor.projectData()).objects.size(),"element list out of sync");elements->setCurrentRow(elements->count()-1);button(editor,"Delete selected")->click();check(editor.composedImage()==before,"remove selected failed");
 elements->setCurrentRow(target);button(editor,"Delete selected")->click();
 editor.setQsoFields("IZ6NNH","G0JEI","Other name","IO91","575");
 auto deleted=parse(editor.projectData());bool recreated=false;for(const auto &o:deleted.objects)if(o.key=="big_call")recreated=true;
 check(!recreated,"deleted QSO field reappeared after editing another field");
 SstvImageEditorDialog hiddenFieldProject;
 check(hiddenFieldProject.restoreProject(editor.projectData()),"suppressed-field project reopen");
 check(hiddenFieldProject.qsoFields().value(1)=="G0JEI","hidden field lost QSO metadata");
 const QImage finalImage=editor.composedImage();
 const QByteArray valid=editor.projectData();check(!editor.restoreProject(QByteArray("bad")),"invalid project accepted");check(editor.projectData()==valid,"invalid project changed document");
 QByteArray truncated=valid.left(valid.size()/2);check(!editor.restoreProject(truncated),"truncated project accepted");check(editor.projectData()==valid,"truncated project changed document");
 QImage handed;QObject::connect(&editor,&SstvImageEditorDialog::imageReadyForTx,[&](const QImage &im,const QString &){handed=im;});button(editor,"Use for SSTV TX")->click();check(handed==finalImage,"TX handoff differs from editor");
 second.setBackgroundImage(bg); // clean closing state
 editor.grab().save("sstv-editor-800.png");
 std::cout<<"PASS editor 800x600 actions, project roundtrip, field style/position, undo/redo, elements, invalid files, exact TX handoff\n";
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}return 0;}
