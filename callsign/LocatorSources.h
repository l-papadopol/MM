#pragma once
/*
 * Locator lookup (IC8TEM, port of MSHV-IC8TEM src/HvTxW/hvlocsrc.*).
 *
 * When the DX call has no locator, MadModem looks in these sources, in this order:
 *   RX    the locator the station itself sent in the decodes of the last 20 periods
 *   C3    CALL3.TXT of WSJT-X Improved (terrestrial)
 *   EME   CALL3.TXT EME of W7GJ
 *   JTDX  grid_data.bin of JTDX
 *   WEB   online, G7VRD public API (only when no other source knows the call)
 * The lists are downloaded and refreshed every 14 days; a download is checked
 * before it replaces the old list. A small label inside the DX grid field shows
 * where the locator comes from, its tooltip what every source says.
 *
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */
#include <QDialog>
#include <QHash>
#include <QPointer>
#include <QProcess>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <QVector>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

struct LocatorRecord
{
    char call[12]; // ASCII, zero padded
    quint32 grid;  // encoded locator
};

class LocatorTable
{
public:
    QVector<LocatorRecord> records;
    QString info; // e.g. "Revised 31 December, 2025" (W7GJ)
    bool find(const QString &call, QString &grid) const;
};
using LocatorTablePtr = QSharedPointer<LocatorTable>;

/** Reads and checks one list in the background. */
class LocatorLoader : public QThread
{
    Q_OBJECT
public:
    LocatorLoader(int source, const QString &file, QObject *parent = nullptr);
    int source;
    int generation = 0;
    QString file;
    LocatorTablePtr table;
    QString error;

protected:
    void run() override;
};

class LocatorSources : public QObject
{
    Q_OBJECT
public:
    enum Source { Rx = 0, Call3, Eme, Jtdx, Web, Count };

    explicit LocatorSources(QObject *parent = nullptr);
    ~LocatorSources() override;

    /** "CQ K1ABC FN42" -> K1ABC FN42; the locator belongs to the sender. Never RR73. */
    static bool parseMessage(const QString &message, QString &call, QString &grid);
    static bool isGrid(const QString &grid); // 4 or 6 characters
    static QString tag(int source);          // RX C3 EME JTDX WEB
    static QString name(int source);
    static int periods() { return 20; }

    bool isOn(int source) const { return source >= 0 && source < Count && m_on[source]; }
    void setOn(int source, bool on);
    bool autoUpdate() const { return m_autoUpdate; }
    void setAutoUpdate(bool on);

    /** A decoded message (live decodes only). */
    void addRx(const QString &message);
    QString findOne(int source, const QString &call, double periodSeconds) const;
    /** First source that knows the call; 4 characters become 6 when a later source has the same square. */
    QString findLocal(const QString &call, double periodSeconds, int &source) const;
    QString tooltip(const QString &call, double periodSeconds) const;
    /** Online lookup; answers with onlineResult() (empty grid when unknown or not possible). */
    void requestOnline(const QString &call);
    QString status(int source) const;
    bool busy() const { return m_downloading >= 0 || !m_downloadQueue.isEmpty(); }
    QString folder() const { return m_dir; }

public slots:
    void updateNow();

signals:
    void onlineResult(const QString &call, const QString &grid);
    void changed();
    void listLoaded();

private slots:
    void checkUpdates();
    void loaderFinished();
    void downloadFinished();
    void curlFinished(int code, QProcess::ExitStatus status);
    void downloadTimeout();
    void webFinished();
    void webCurlFinished(int code, QProcess::ExitStatus status);
    void webTimeout();

private:
    struct RxEntry { QString grid; qint64 ms = 0; };
    struct WebEntry { QString grid; qint64 t = 0; };

    QString filePath(int source) const;
    void readSettings();
    void saveSettings() const;
    void load(int source, const QString &file);
    void nextDownload();
    void downloadDone(bool ok, const QByteArray &data, const QString &error);
    void webStart(const QString &call);
    void webDone(const QByteArray &data, int http, const QString &error);
    void readWebCache();
    void saveWebCache();

    QString m_dir;
    bool m_on[Count];
    bool m_autoUpdate = true;
    bool m_loadingSettings = false;
    LocatorTablePtr m_tables[Count];
    int m_generation[Count];
    QString m_error[Count];
    QList<LocatorLoader *> m_loaders;
    QHash<QString, RxEntry> m_rx;
    int m_rxAdds = 0;
    mutable double m_rxPeriod = 15.0;
    QHash<QString, WebEntry> m_web;
    QString m_webError;
    QString m_webCurrent;
    QString m_webNext;
    qint64 m_webBackoff = 0;
    int m_webAsked = 0;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QProcess *m_downloadProcess = nullptr;
    QNetworkReply *m_webReply = nullptr;
    QProcess *m_webProcess = nullptr;
    QList<int> m_downloadQueue;
    int m_downloading = -1;
    bool m_force = false;
    QTimer *m_downloadTimer = nullptr;
    QTimer *m_webTimer = nullptr;
    QTimer *m_checkTimer = nullptr;
};

/**
 * Fills a DX grid field from the locator sources when the DX call changes and the
 * field is empty or still holds the locator of the previous call. A small label
 * inside the field shows the source; its tooltip shows every source.
 */
class LocatorLookupBinder : public QObject
{
    Q_OBJECT
public:
    LocatorLookupBinder(LocatorSources *sources, QLineEdit *callEdit, QLineEdit *gridEdit,
                        std::function<double()> periodSeconds, QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void poll();
    void lookup(bool callChanged);
    void setGrid(const QString &grid, const QString &tag);
    void placeBadge();
    void refreshTooltip();

    LocatorSources *m_sources;
    QPointer<QLineEdit> m_call;
    QPointer<QLineEdit> m_grid;
    std::function<double()> m_period;
    QLabel *m_badge = nullptr;
    QTimer m_pollTimer;
    QTimer m_webTimer;
    QString m_lastCall;
    QString m_lastGrid;
    QString m_autoGrid; // the grid this binder wrote for m_lastCall
};

/** Settings of the locator sources (Tools menu). */
class LocatorSourcesDialog : public QDialog
{
    Q_OBJECT
public:
    LocatorSourcesDialog(LocatorSources *sources, QWidget *parent = nullptr);

private:
    void refresh();
    LocatorSources *m_sources;
    QCheckBox *m_check[LocatorSources::Count] = {};
    QLabel *m_status[LocatorSources::Count] = {};
    QCheckBox *m_auto = nullptr;
    QPushButton *m_update = nullptr;
};
