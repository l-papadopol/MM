#pragma once
/*
 * LoTW users (IC8TEM, from MSHV-IC8TEM): the ARRL list lotw-user-activity.csv
 * ("CALL,YYYY-MM-DD,HH:MM:SS" = last upload to LoTW) marks the decodes of LoTW
 * users with a dot and can hide the other stations in the FT decode list.
 *
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */
#include <QDate>
#include <QDialog>
#include <QHash>
#include <QObject>
#include <QProcess>

class QCheckBox;
class QDateEdit;
class QLabel;
class QNetworkAccessManager;
class QPushButton;

class LotwUsers : public QObject
{
    Q_OBJECT
public:
    explicit LotwUsers(QObject *parent = nullptr);
    ~LotwUsers() override;

    /** Marker on: the call (or its base call) uploaded to LoTW on or after the minimum date. */
    static bool isUser(const QString &call);
    /** Like isUser() but ignores the marker setting. */
    static bool isValid(const QString &call);
    static bool hasList();
    static QDate lastUpload(const QString &call);
    /** The marker appended to the decoded message of a LoTW user. */
    static QString marker() { return QStringLiteral("  ●"); }

    bool showMarker() const { return m_showMarker; }
    void setShowMarker(bool on);
    bool filterFt() const { return m_filterFt; }
    void setFilterFt(bool on);
    QDate minDate() const { return m_minDate; }
    void setMinDate(const QDate &date);
    bool autoUpdate() const { return m_autoUpdate; }
    void setAutoUpdate(bool on);

    void download();
    bool loadFile(const QString &path, QString *error = nullptr);
    bool downloading() const { return m_downloading; }
    QString status() const;
    QString csvPath() const { return m_csvPath; }
    static QString url() { return QStringLiteral("https://lotw.arrl.org/lotw-user-activity.csv"); }

signals:
    void changed();
    /** The list, the minimum date or the filter changed: decode lists may refresh. */
    void listChanged();

private:
    void readSettings();
    void saveSettings() const;
    void loadInBackground(const QString &path, bool replaceSaved);
    void downloadDone(bool ok, const QByteArray &data, const QString &error);
    void checkAge();

    QString m_csvPath;
    bool m_showMarker = true;
    bool m_filterFt = false;
    bool m_autoUpdate = true;
    QDate m_minDate;
    bool m_downloading = false;
    qint64 m_progress = 0;
    QString m_error;
    QNetworkAccessManager *m_network = nullptr;
    QProcess *m_curl = nullptr;
};

class LotwUsersDialog : public QDialog
{
    Q_OBJECT
public:
    LotwUsersDialog(LotwUsers *users, QWidget *parent = nullptr);

private:
    void refresh();
    LotwUsers *m_users;
    QLabel *m_status = nullptr;
    QPushButton *m_download = nullptr;
};
