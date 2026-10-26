#include "SstvImageArchive.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryFile>

SstvImageArchive::SstvImageArchive(QObject *parent) : QObject(parent), m_thread([this]{run();}) {}
SstvImageArchive::~SstvImageArchive() { finish(); }

void SstvImageArchive::configure(bool enabled, const QString &folder)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled=enabled;
    m_folder=folder;
}

bool SstvImageArchive::enqueue(const QImage &image, const QString &mode, const QDateTime &utc)
{
    QString error;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_enabled || m_closing) return false;
        if(image.isNull()) error=QStringLiteral("The completed SSTV image is empty.");
        else if(m_jobs.size()>=MaxPending) error=QStringLiteral("SSTV save queue is full; image was not queued.");
        else m_jobs.push_back(Job{image,m_folder,mode,utc.toUTC()});
    }
    if(!error.isEmpty()) { emit saveFailed(error);return false; }
    emit saveQueued();
    m_ready.notify_one();
    return true;
}

void SstvImageArchive::enqueueCompleted(const QImage &image, const QString &reason)
{
    // Completion mode travels with the image, independent of later GUI selection.
    const QString suffix=QStringLiteral(" frame complete");
    const QString mode=reason.endsWith(suffix)?reason.left(reason.size()-suffix.size()):QStringLiteral("SSTV");
    enqueue(image,mode);
}

void SstvImageArchive::finish()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closing=true;
    }
    m_ready.notify_one();
    if(m_thread.joinable()) m_thread.join();
}

void SstvImageArchive::run()
{
    for(;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_ready.wait(lock,[this]{return m_closing || !m_jobs.empty();});
            if(m_jobs.empty()) return;
            job=std::move(m_jobs.front());m_jobs.pop_front();
        }
        const Result result=write(job);
        if(result.error.isEmpty()) emit imageSaved(result.path);
        else emit saveFailed(result.error);
    }
}

SstvImageArchive::Result SstvImageArchive::write(const Job &job)
{
    if(job.folder.isEmpty() || !QDir::isAbsolutePath(job.folder))
        return {{},QStringLiteral("Choose an absolute destination folder for SSTV images.")};
    if(!QDir().mkpath(job.folder))
        return {{},QStringLiteral("Cannot create SSTV folder: %1").arg(job.folder)};
    const QDir folder(job.folder);
    QTemporaryFile temporary(folder.filePath(QStringLiteral(".sstv-XXXXXX.part")));
    if(!temporary.open()) return {{},QStringLiteral("Cannot create SSTV image file in %1: %2").arg(job.folder,temporary.errorString())};
    if(!job.image.save(&temporary,"PNG") || !temporary.flush())
        return {{},QStringLiteral("Cannot write SSTV PNG in %1: %2").arg(job.folder,temporary.errorString())};
    const QString temporaryPath=temporary.fileName();
    temporary.close();
    QString mode=job.mode;
    mode.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")));
    mode=mode.left(40);
    if(mode.isEmpty()) mode=QStringLiteral("SSTV");
    const QDateTime utc=job.utc.isValid()?job.utc:QDateTime::currentDateTimeUtc();
    const QString stem=utc.toUTC().toString(QStringLiteral("yyyy-MM-dd_HHmmss"))+QLatin1Char('_')+mode;
    for(int sequence=0;sequence<10000;++sequence) {
        const QString extra=sequence?QStringLiteral("_%1").arg(sequence,3,10,QLatin1Char('0')):QString();
        const QString path=folder.filePath(stem+extra+QStringLiteral(".png"));
        // QFile::rename refuses an existing destination. Publish only a fully
        // encoded file in the same directory; never overwrite an earlier image.
        QFile pending(temporaryPath);
        if(pending.rename(path)) {
            temporary.setAutoRemove(false);
            return {path,{}};
        }
        if(!QFileInfo::exists(path))
            return {{},QStringLiteral("Cannot finalize SSTV PNG %1: %2").arg(path,pending.errorString())};
    }
    return {{},QStringLiteral("No free filename for SSTV image in %1").arg(job.folder)};
}
