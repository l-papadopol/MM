#include "MadModemVersion.h"
#include "MadModemBuildInfo.h"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionDiagnostics.h"
#include "../audio/AudioBlock.h"
#include "../settings/AppSettings.h"
#include "../utils/InstallationPaths.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QUuid>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace {
QByteArray line(QJsonObject fields) { return QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n'; }
QString redact(QString text) {
    static const QRegularExpression re(QStringLiteral("((?:password|passwd|token|api[_-]?key|authorization)[\\\"'\\s]*[:=][\\\"'\\s]*)([^\\s&\\\"',}]+)"), QRegularExpression::CaseInsensitiveOption);
    text.replace(re, QStringLiteral("\\1[redacted]"));
    static const QRegularExpression bearer(QStringLiteral("Bearer\\s+[^\\s\\\"']+"), QRegularExpression::CaseInsensitiveOption);
    text.replace(bearer,QStringLiteral("Bearer [redacted]"));
    return text;
}
QJsonValue safeValue(const QJsonValue &v) {
    if(v.isString()) {
        QString t=v.toString();
        static const QRegularExpression bearer(QStringLiteral("Bearer\\s+[^\\s\"']+"),QRegularExpression::CaseInsensitiveOption);
        t.replace(bearer,QStringLiteral("Bearer [redacted]"));return redact(t);
    }
    if(v.isArray()){QJsonArray a;for(const auto &x:v.toArray())a.append(safeValue(x));return a;}
    if(v.isObject()) {
        QJsonObject out;const auto o=v.toObject();
        static const QRegularExpression secret(QStringLiteral("password|passwd|secret|token|credential|authorization|api.?key"),QRegularExpression::CaseInsensitiveOption);
        for(auto it=o.begin();it!=o.end();++it)out.insert(it.key(),secret.match(it.key()).hasMatch()?QJsonValue("[redacted]"):safeValue(it.value()));return out;
    }
    return v;
}
// Conservative logical budget: includes container overhead and worst-case
// JSON escaping. Formatting/redaction belongs exclusively to the writer.
size_t eventBudget(const QJsonValue &v) {
    size_t bytes=64;
    if(v.isString())return bytes+size_t(v.toString().size())*6;
    if(v.isArray())for(const auto &x:v.toArray())bytes+=eventBudget(x);
    if(v.isObject()){const auto o=v.toObject();for(auto it=o.begin();it!=o.end();++it)bytes+=64+size_t(it.key().size())*6+eventBudget(it.value());}
    return bytes;
}
QtMessageHandler previousHandler = nullptr;
void qtCapture(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    thread_local bool inside = false;
    if (!inside) {
        inside = true;
        auto &log = SessionDiagnostics::instance();
        if (log.enabled()) log.record(QStringLiteral("qt.message"), {{"severity",int(type)}, {"message",message}, {"file",QString::fromUtf8(context.file ? context.file : "")}, {"line",context.line}, {"function",QString::fromUtf8(context.function ? context.function : "")}, {"qt_category",QString::fromUtf8(context.category ? context.category : "")}});
        if (type == QtFatalMsg) log.flush();
        inside = false;
    }
    if (previousHandler) previousHandler(type,context,message);
    else {const QByteArray text=qFormatLogMessage(type,context,message).toLocal8Bit();std::fprintf(stderr,"%s\n",text.constData());}
    if (type == QtFatalMsg) std::abort();
}
}
SessionDiagnostics::SessionDiagnostics(size_t limit) : m_limit(limit) {}
SessionDiagnostics::~SessionDiagnostics() { stop(); }
SessionDiagnostics &SessionDiagnostics::instance() { static SessionDiagnostics log; return log; }
QString SessionDiagnostics::defaultFolder() { return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("diagnostics"); }
void SessionDiagnostics::installQtCapture() { static std::once_flag once; std::call_once(once,[]{previousHandler=qInstallMessageHandler(qtCapture);}); }
void SessionDiagnostics::restoreQtCapture() {qInstallMessageHandler(previousHandler);}
QString SessionDiagnostics::path() const {std::lock_guard<std::mutex> l(m_mutex);return m_path;}
QString SessionDiagnostics::error() const {std::lock_guard<std::mutex> l(m_mutex);return m_error;}
quint64 SessionDiagnostics::lost() const {std::lock_guard<std::mutex> l(m_mutex);return m_lost;}
bool SessionDiagnostics::start(const QString &folder) {
    std::lock_guard<std::mutex> control(m_control);
    stopLocked();
    {
        std::lock_guard<std::mutex> l(m_mutex);
        m_queue.clear();m_bytes=0;m_sequence=0;m_lost=0;m_flushRequest=0;m_flushDone=0;m_stopping=false;m_ready=false;m_error.clear();
        const QString destination=folder.trimmed().isEmpty()?defaultFolder():QDir(folder).absolutePath();
        m_path=QDir(destination).filePath("MadModem_"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz")+"_"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".jsonl");
    }
    m_thread=std::thread([this]{run();});
    {std::unique_lock<std::mutex> l(m_mutex);m_cv.wait(l,[this]{return m_ready;});if(!m_error.isEmpty())return false;m_active.store(true);}

    return true;
}
void SessionDiagnostics::stopLocked() {
    m_active.store(false);
    {std::lock_guard<std::mutex> l(m_mutex);m_stopping=true;}
    m_cv.notify_all();if(m_thread.joinable())m_thread.join();
}
void SessionDiagnostics::stop() {std::lock_guard<std::mutex> control(m_control);stopLocked();}
bool SessionDiagnostics::flush(int timeoutMs) {
    std::unique_lock<std::mutex> l(m_mutex);
    if (!m_active.load()) return m_error.isEmpty();
    const auto request=++m_flushRequest;m_cv.notify_all();
    return m_cv.wait_for(l,std::chrono::milliseconds(timeoutMs),[&]{return m_flushDone>=request||!m_error.isEmpty();}) && m_error.isEmpty();
}
void SessionDiagnostics::record(const QString &category,QJsonObject fields) {
    if (!enabled()) return;
    fields.insert("event",category);fields.insert("utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    fields.insert("steady_ns",QString::number(AudioBlock::steadyNowNs()));
    fields.insert("thread",QString::number(quintptr(QThread::currentThreadId()),16));
    fields.insert("pid",double(QCoreApplication::applicationPid()));
    const size_t budget=eventBudget(fields)+128;
    std::lock_guard<std::mutex> l(m_mutex);
    if (!m_active.load()) return;
    if (m_bytes+budget>m_limit) {++m_lost;m_cv.notify_one();return;}
    fields.insert("seq",QString::number(++m_sequence));
    m_bytes+=budget;m_queue.push_back(std::move(fields));m_cv.notify_one();
}
void SessionDiagnostics::run() {
    const QString filePath=path();QFile file(filePath);
    auto write=[&](const QByteArray &bytes){qint64 pos=0;while(pos<bytes.size()){const auto n=file.write(bytes.constData()+pos,bytes.size()-pos);if(n<=0)return false;pos+=n;}return true;};
    bool opened=QDir().mkpath(QFileInfo(filePath).absolutePath())&&file.open(QIODevice::WriteOnly|QIODevice::NewOnly);
    if(opened)opened=write(line({{"event","session.start"},{"seq","1"},{"schema",1},
        {"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"steady_ns",QString::number(AudioBlock::steadyNowNs())},
        {"pid",double(QCoreApplication::applicationPid())},{"application",QCoreApplication::applicationName()},
        {"version",QStringLiteral(MADMODEM_VERSION_STRING)},{"source_revision",QStringLiteral(MADMODEM_SOURCE_REVISION)},
        {"build",QStringLiteral(MADMODEM_BUILD_NUMBER)},{"commit",QStringLiteral(MADMODEM_BUILD_COMMIT)},
        {"os",QSysInfo::prettyProductName()},{"architecture",QSysInfo::currentCpuArchitecture()},{"qt",QString::fromLatin1(qVersion())},
        {"logical_processors",QThread::idealThreadCount()},{"queue_limit_bytes",double(m_limit)}}))&&file.flush();
    {std::lock_guard<std::mutex> l(m_mutex);if(!opened)m_error="Cannot create diagnostic log: "+filePath+" ("+file.errorString()+")";else m_sequence=1;m_ready=true;}
    m_cv.notify_all();if(!opened)return;
    quint64 written=1,reportedLoss=0;
    auto fail=[&]{std::lock_guard<std::mutex> l(m_mutex);m_error="Diagnostic log write/flush failed: "+file.errorString();m_lost+=m_queue.size();m_queue.clear();m_bytes=0;m_active.store(false);m_cv.notify_all();};
    for (;;) {
        std::deque<QJsonObject> batch;quint64 losses,request;bool stopping;
        {std::unique_lock<std::mutex> l(m_mutex);m_cv.wait_for(l,std::chrono::milliseconds(250),[this,&reportedLoss]{return m_stopping||!m_queue.empty()||m_flushRequest>m_flushDone||m_lost>reportedLoss;});batch.swap(m_queue);m_bytes=0;losses=m_lost;request=m_flushRequest;stopping=m_stopping;}
        bool ok=true;
        for(const auto &event:batch){if(!write(line(safeValue(event).toObject()))){ok=false;break;}++written;}
        if(ok&&losses>reportedLoss){ok=write(line({{"event","diagnostic.loss"},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"lost_total",QString::number(losses)}}));reportedLoss=losses;}
        if(ok&&stopping)ok=write(line({{"event","session.end"},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"written",QString::number(written)},{"lost_total",QString::number(losses)},{"complete",losses==0}}));
        if(!ok||!file.flush()){fail();return;}
        {std::lock_guard<std::mutex> l(m_mutex);m_flushDone=request;}
        m_cv.notify_all();if(stopping)break;
    }
}
void SessionDiagnostics::audio(const QString &stage,const AudioBlock &block) {
    if(!enabled())return;
    double power=0,peak=0;int clipped=0;
    for(float x:block.samples){power+=double(x)*x;peak=std::max(peak,std::abs(double(x)));if(std::abs(x)>=.995f)++clipped;}
    record("audio.block",{{"stage",stage},{"sequence",QString::number(block.captureSequence)},{"generation",QString::number(block.captureGeneration)},{"first_index",QString::number(block.firstSampleIndex)},{"utc_ns",QString::number(block.firstSampleUtcNs)},{"capture_mono_ns",QString::number(block.firstSampleMonotonicNs)},{"span_ns",QString::number(block.captureSpanNs)},{"queued_ns",QString::number(block.enqueuedSteadyNs)},{"rate",block.sampleRate},{"samples",block.samples.size()},{"rms",block.samples.isEmpty()?0:std::sqrt(power/block.samples.size())},{"peak",peak},{"clipped",clipped},{"sha256",QString::fromLatin1(QCryptographicHash::hash(QByteArray::fromRawData(reinterpret_cast<const char*>(block.samples.constData()),block.samples.size()*int(sizeof(float))),QCryptographicHash::Sha256).toHex())}});
}
void SessionDiagnostics::settings(const AppSettings &s) {
    if(!enabled())return;
    QSettings stored(MadModemInstallation::settingsFilePath(),QSettings::IniFormat);QJsonObject config;
    for(const auto &key:stored.allKeys())config.insert(key,QJsonValue::fromVariant(stored.value(key)));
    record("settings.persisted",config);
    record("settings.snapshot",{{"input",s.audioInputName},{"output",s.audioOutputName},{"rate",s.audioSampleRate},{"ptt_method",s.pttMethod},{"ft_band",s.ft8Band},{"ft_deep",s.ft8DeepDecode},{"ft_dsp_plus",s.ft8DspPlusDecode},{"drift",s.ftDriftRxEnabled},{"drift_max",s.ftDriftMaxHzPerSec},{"gpu_sync",s.ftGpuSyncEnabled},{"gpu_decode",s.ftGpuDecodeEnabled}});
}
