#ifndef HAMLIBCONTROLLER_H
#define HAMLIBCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QTimer>

class AppSettings;
class QTcpSocket;

/**
 * @brief Optional Hamlib CAT/PTT controller.
 *
 * The class is compiled as a safe stub when Hamlib headers/libraries are not
 * available.  When MadModem is built with MADMODEM_WITH_HAMLIB it owns one
 * Hamlib RIG handle, polls the transceiver frequency, and can key CAT PTT for
 * transmit.  Serial RTS PTT remains available as the legacy fallback.
 */
class HamlibController final : public QObject
{
    Q_OBJECT

public:
    struct Config
    {
        bool catEnabled = false;
        bool pttEnabled = false;
        bool updateFt8BandFromCat = true;
        // True for safe read-only tests: open CAT and read frequency only,
        // with Hamlib PTT explicitly disabled even if the dialog PTT checkbox is enabled.
        bool readOnlyTest = false;
        int rigModel = 1;
        QString rigPath;
        int baudRate = 38400;
        int dataBits = 0; // 0 = Hamlib backend default, otherwise 7 or 8
        int stopBits = 0; // 0 = Hamlib backend default, otherwise 1 or 2
        // default, none, xonxoff, hardware
        QString handshake = "default";
        // unchanged, on, off. Never changed unless explicitly selected.
        QString forceDtr = "unchanged";
        QString forceRts = "unchanged";
        // none, cat_hamlib, serial_rts, serial_dtr.
        QString pttMethod = "none";
        QString pttPort;
        int pollIntervalMs = 1000;
        // WSJT-X-like TX mode policy: default = do not change rig mode,
        // usb = force normal USB on TX, data_pkt = force Data/Pkt/USB-D mode on TX.
        // PTT itself remains CAT/RTS/DTR/VOX and is not a vendor route selector.
        QString txAudioRoute = "default";
        // WSJT-X-like transmit audio source hint: front_mic or rear_data.
        // This is not a vendor route selector; it only guides Hamlib MIC/DATA PTT when available.
        QString transmitAudioSource = "rear_data";
    };

    explicit HamlibController(QObject *parent = nullptr);
    ~HamlibController() override;

    static bool isCompiledWithHamlib();
    static int hamRadioDeluxeModelId();
    static bool isHamRadioDeluxeModel(int rigModel);

    void configure(const Config &config);
    void configureFromSettings(const AppSettings &settings);
    Config config() const { return m_config; }

    bool isConnected() const;
    double lastFrequencyHz() const { return m_lastFrequencyHz; }
    QString lastStatus() const { return m_lastStatus; }

public slots:
    bool connectRig();
    void disconnectRig();
    void pollNow();
    bool setPtt(bool enabled);
    bool setFrequencyHz(double frequencyHz);

signals:
    void statusChanged(const QString &status);
    void errorOccurred(const QString &message);
    void frequencyChanged(double frequencyHz);
    void pttChanged(bool enabled);

private:
    void setStatus(const QString &status);
    void emitError(const QString &message);
    bool prepareDigitalTxAudioRoute();
    bool forceUsbMode(bool required);
    bool forceDataUsbMode(bool required);
#ifdef MADMODEM_WITH_HAMLIB
    void capturePreTxMode();
    void restorePreTxMode();
#endif
    bool sendRawCatCommand(const QByteArray &command);
    bool setHamlibPttMode(bool enabled, int onMode, const QString &label, bool reportErrors = true);
    bool setWsjtLikeCatPtt(bool enabled);

    bool connectHrd();
    void disconnectHrd();
    bool pollHrd();
    bool setHrdPtt(bool enabled);
    QString hrdEndpoint() const;
    bool hrdSendCommand(const QString &command,
                        QString *reply,
                        bool prependContext = true,
                        QString *errorMessage = nullptr);
    bool hrdSendSimpleCommand(const QString &command, QString *errorMessage = nullptr);
    bool hrdWriteAll(const QByteArray &data);
    bool hrdReadResponse(const QString &command, QString *reply, QString *errorMessage);
    int hrdFindButton(const QStringList &patterns) const;

    Config m_config;
    QTimer *m_pollTimer = nullptr;
    double m_lastFrequencyHz = 0.0;
    bool m_lastPtt = false;
    QString m_lastStatus;

    QTcpSocket *m_hrdSocket = nullptr;
    int m_hrdProtocol = 0;
    unsigned m_hrdCurrentRadio = 0;
    QStringList m_hrdButtons;
    int m_hrdPttButton = -1;
    int m_hrdAltPttButton = -1;
    bool m_vendorDataPttActive = false;

#ifdef MADMODEM_WITH_HAMLIB
    struct RigDeleter;
    void *m_rig = nullptr;
    bool m_havePreTxMode = false;
    qint64 m_preTxMode = 0;
    qint64 m_preTxPassband = 0;
#endif
};

#endif // HAMLIBCONTROLLER_H
