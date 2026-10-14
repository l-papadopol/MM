#pragma once

#include "AdifLogbook.h"
#include "../runtime/BackgroundQueue.h"
#include <QCoreApplication>
#include <QEvent>
#include <QHash>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

// The store is touched exclusively by the serial queue. The GUI receives a
// committed, implicitly shared snapshot, never speculative records. All write
// paths (including the editor dialog) must use this service.
class AsyncLogbook final : public QObject
{
public:
    using Completion = std::function<void(int, const QString &)>;
    explicit AsyncLogbook(AdifLogbook *mirror, QObject *parent = nullptr)
        : QObject(parent), m_mirror(mirror), m_store(*mirror) {}
    ~AsyncLogbook() override { m_queue.stop(); }

    void setResetHandler(std::function<void()> handler) { m_resetHandler = std::move(handler); }

    void append(const LogbookEntry &entry, Completion done, bool recentFtDuplicate = false) {
        const quint64 request = ++m_sequence;
        Operation operation = [this, entry, recentFtDuplicate](AdifLogbook &store, QString &error) {
            ensureIndexes(store);
            const auto id = entry.adifFields.value(QStringLiteral("APP_MADMODEM_QSO_ID"));
            if (!id.isEmpty() && m_recordIds.contains(id)) return 0;
            if (recentFtDuplicate) {
                const auto dates = m_ftDates.value(ftKey(entry));
                for (const auto &date : dates)
                    if (date.isValid() && qAbs(date.secsTo(entry.utc)) < 600) return 0;
            }
            if (!store.append(entry, &error)) return -1;
            indexEntry(entry);
            return 1;
        };
        submit(request, operation, std::move(done), true);
    }
    void importFile(const QString &path, Completion done) {
        submit(++m_sequence, [this, path](AdifLogbook &store, QString &error) {
            m_indexesValid = false;
            int count = 0;
            return store.importAdif(path, &count, &error) ? count : -1;
        }, std::move(done), false, true);
    }
    void remove(const QVector<LogbookEntry> &records, Completion done) {
        submit(++m_sequence, [this, records](AdifLogbook &store, QString &error) {
            m_indexesValid = false;
            return store.removeEntries(records, &error);
        }, std::move(done), false, true);
    }
    void reload(const QString &path, Completion done) {
        submit(++m_sequence, [this, path](AdifLogbook &store, QString &error) {
            AdifLogbook next(path);
            if (!next.load(&error)) return -1;
            store = std::move(next);
            m_indexesValid = false;
            return store.count();
        }, std::move(done), false, true);
    }
    bool busy() const { return m_pending != 0; }
    bool hasUnsaved() const { return !m_failed.isEmpty(); }
    void retryFailed() {
        const auto failed = m_failed;
        for (auto it = failed.cbegin(); it != failed.cend(); ++it)
            submit(it.key(), it->operation, it->done, true);
    }
    void whenIdle(std::function<void()> done) {
        m_idle.push_back(std::move(done));
        notifyIdle();
    }
    // Destruction fallback. Normal Close uses whenIdle and keeps the event loop
    // alive, allowing completion/error handling before deciding whether to exit.
    void drain() {
        while (m_pending) {
            m_queue.drain();
            QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
        }
    }
private:
    using Operation = std::function<int(AdifLogbook &, QString &)>;
    struct Failed { Operation operation; Completion done; };
    void submit(quint64 request, Operation operation, Completion done, bool retainFailure = false, bool reset = false) {
        ++m_pending;
        m_queue.post([this, request, operation, done, retainFailure, reset] {
            QString error;
            const int result = operation(m_store, error);
            const auto snapshot = std::make_shared<AdifLogbook>(m_store);
            QMetaObject::invokeMethod(this, [this, request, operation, done, retainFailure, reset, result, error, snapshot] {
                *m_mirror = *snapshot;
                if (retainFailure && result < 0) m_failed.insert(request, {operation, done});
                else m_failed.remove(request);
                // Keep pending true through the callback: Close can be requested
                // from a modal error box nested inside that callback.
                if (reset && result >= 0 && m_resetHandler) m_resetHandler();
                if (done) done(result, error);
                --m_pending;
                notifyIdle();
            }, Qt::QueuedConnection);
        });
    }
    void notifyIdle() {
        if (m_pending || m_idle.empty()) return;
        auto callbacks = std::move(m_idle);
        m_idle.clear();
        QTimer::singleShot(0, this, [callbacks] { for (const auto &done : callbacks) done(); });
    }
    static QString ftKey(const LogbookEntry &entry) {
        return entry.callsign.trimmed().toUpper() + QLatin1Char('|') +
            entry.band.trimmed().toUpper() + QLatin1Char('|') + entry.mode.trimmed().toUpper();
    }
    void indexEntry(const LogbookEntry &entry) {
        const auto id = entry.adifFields.value(QStringLiteral("APP_MADMODEM_QSO_ID"));
        if (!id.isEmpty()) m_recordIds.insert(id);
        m_ftDates[ftKey(entry)].append(entry.utc);
    }
    void ensureIndexes(const AdifLogbook &store) {
        if (m_indexesValid) return;
        m_recordIds.clear(); m_ftDates.clear();
        for (const auto &entry : store.records()) indexEntry(entry);
        m_indexesValid = true;
    }
    bool m_indexesValid = false; // worker-only, including indexes below
    QSet<QString> m_recordIds;
    QHash<QString, QVector<QDateTime>> m_ftDates;
    std::function<void()> m_resetHandler; // GUI-only
    AdifLogbook *m_mirror;
    AdifLogbook m_store;
    quint64 m_sequence = 0;
    int m_pending = 0;
    QHash<quint64, Failed> m_failed;
    std::vector<std::function<void()>> m_idle;
    BackgroundQueue m_queue;
};
