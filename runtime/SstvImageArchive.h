#ifndef MADMODEM_SSTV_IMAGE_ARCHIVE_H
#define MADMODEM_SSTV_IMAGE_ARCHIVE_H

#include <QObject>
#include <QDateTime>
#include <QImage>
#include <QString>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

// GUI-side facade. PNG encoding and disk I/O run on a dedicated serial worker.
// Each accepted job owns the completion image, destination, mode and UTC time.
class SstvImageArchive final : public QObject
{
    Q_OBJECT
public:
    explicit SstvImageArchive(QObject *parent=nullptr);
    ~SstvImageArchive() override;
    void configure(bool enabled, const QString &folder);
    bool enqueue(const QImage &image, const QString &mode,
                 const QDateTime &utc=QDateTime::currentDateTimeUtc());
    void enqueueCompleted(const QImage &image, const QString &reason);
    void finish(); // Stop accepting new jobs and drain all accepted images.

signals:
    void saveQueued();
    void imageSaved(const QString &path);
    void saveFailed(const QString &error);

private:
    struct Job { QImage image; QString folder; QString mode; QDateTime utc; };
    struct Result { QString path; QString error; };
    static Result write(const Job &job);
    void run();
    static constexpr size_t MaxPending=8;
    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<Job> m_jobs;
    bool m_enabled=false;
    bool m_closing=false;
    QString m_folder;
    std::thread m_thread;
};
#endif
