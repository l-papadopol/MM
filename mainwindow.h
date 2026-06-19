#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "audio/AudioEngine.h"
#include "audio/TxAudioEngine.h"
#include "audio/FtTxWorker.h"
#include "dsp/DspEngine.h"
#include "dsp/common/DspConditioner.h"
#include "dialogs/BandSchedulerDialog.h"
#include "dialogs/AppSettingsDialog.h"
#include "modems/weatherfax/WeatherFaxDecoder.h"
#include "modems/sstv/SstvDecoder.h"
#include "modems/rtty/RttyDecoder.h"
#include "modems/rtty/RttyMultiDecoder.h"
#include "modems/bpsk31/Bpsk31Decoder.h"
#include "modems/mfsk/MfskDecoder.h"
#include "modems/cw/CwDecoder.h"
#include "modems/hell/HellschreiberDecoder.h"
#include "modems/ft8/Ft8Mode.h"
#include "modems/ft8/Ft8RxDecoder.h"
#include "modems/ft8/FtSlotScheduler.h"
#include "modems/ft8/FtQsoSequencer.h"
#include "modems/ft8/FtQsoSession.h"
#include "modems/ft8/FtTxPlan.h"
#include "modems/ft8/FtStandardMessageSet.h"
#include "settings/AppSettings.h"
#include "logbook/AdifLogbook.h"
#include "widgets/FaxImageWidget.h"
#include "widgets/WaterfallWidget.h"
#include "widgets/QsoMapWidget.h"
#include "rig/HamlibController.h"
#include "rotator/CatRotatorController.h"
#include "rotator/CatRotatorPanel.h"
#include "third_party/decodium_gpl/port/NtpClient.hpp"

#include <QComboBox>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QMainWindow>
#include <QStackedWidget>
#include <QSerialPort>
#include <QTimer>
#include <memory>
#include <QThread>

class QGroupBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QPlainTextEdit;
class QWidget;
class QLineEdit;
class QScrollArea;
class QTableWidget;
class QTableWidgetItem;
class QRadioButton;
class QLCDNumber;
class Ft8SlotClockWidget;
class QAction;
class QActionGroup;
class QMenu;
class RttyScopeWidget;
class QTabWidget;
class QCheckBox;
class DeepDspController;
class DdspPanelWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE


/**
 * @brief One decoded CQ waiting for Evil Auto QSO priority selection.
 *
 * Auto QSO must not answer the first CQ that happens to be emitted by the
 * decoder.  A full FT pass can contain many simultaneous CQs; MM buffers them
 * briefly, ranks them by worked-needed value and distance, then starts exactly
 * one QSO.
 */
struct Ft8FullAutoCqCandidate
{
    Ft8RxDecoder::Decode decode;
    QString call;
    QString grid;
    QString distanceGrid;
    QString country;
    QString dxcc;
    QString band;
    QString mode;
    bool valid = false;
    bool workedCall = false;
    bool newCountry = false;
    bool newGrid = false;
    bool newBand = false;
    bool newMode = false;
    double distanceKm = -1.0;
    QDateTime queuedUtc;
    QString priorityText;
};

/**
 * @brief Main application window for MadModem.
 *
 * Purpose:
 * - Own the main UI.
 * - Load and save persistent settings from settings.mad.
 * - Open the Audio / PTT settings dialog from the menu action.
 * - Start and stop the RX audio path using the saved audio input device.
 * - Show input level diagnostics.
 * - Display the live audio waterfall.
 * - Display the WeatherFax decoded image.
 * - Bind WeatherFax UI settings to the decoder.
 * - Enforce mutual exclusion between RX and TX/PTT operations.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Creates the main window and initializes UI/device/settings state.
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Releases UI resources and disables PTT if active.
     */
    ~MainWindow() override;

private slots:
    /**
     * @brief Refreshes hidden compatibility audio and serial device lists.
     */
    void refreshDevices();

    /**
     * @brief Starts the selected RX audio input.
     */
    void startRx();

    /**
     * @brief Stops the active RX audio input.
     */
    void stopRx();

    /**
     * @brief Toggles the main receiver state button between Ready and RX.
     */
    void toggleRxReady();

    /**
     * @brief Pulses RTS for a short PTT test.
     */
    void testPtt();

    /**
     * @brief Ends the active PTT test pulse.
     */
    void finishPttTest();

    /**
     * @brief Starts image transmission using the active modem mode.
     */
    void startImageTx();

    /**
     * @brief Stops the active image transmission.
     */
    void stopImageTx();

    /**
     * @brief Loads a still image for WEFAX/SSTV transmission.
     */
    void loadTxImage();

    /**
     * @brief Handles TX audio engine startup.
     */
    void handleTxStarted();

    /**
     * @brief Handles TX audio engine stop.
     */
    void handleTxStopped();

    /**
     * @brief Handles natural end of TX image modulation.
     */
    void handleTxFinished();

    /**
     * @brief Handles TX audio output errors.
     */
    void handleTxError(const QString &message);

    /**
     * @brief Updates the TX progress overlay and progress bar.
     */
    void handleTxProgress(double progress);

    /**
     * @brief Starts image TX from the top operation button.
     */
    void txToneTest();

    /**
     * @brief Handles input level updates from AudioEngine.
     */
    void handleAudioLevel(int percent, double db, double rms);

    /**
     * @brief Handles audio startup confirmation.
     */
    void handleAudioStarted();

    /**
     * @brief Handles audio stop confirmation.
     */
    void handleAudioStopped();

    /**
     * @brief Handles audio engine errors.
     */
    void handleAudioError(const QString &message);

    /**
     * @brief Handles dominant frequency updates from DspEngine.
     */
    void handleDominantFrequency(double frequencyHz, double levelDb);

    /**
     * @brief Handles WeatherFax decoder status text.
     */
    void handleWeatherFaxStatus(const QString &status);

    /**
     * @brief Handles automatic WeatherFax tone updates from the live decoder.
     */
    void handleWeatherFaxToneRangeUpdated(double blackHz, double whiteHz);

    /**
     * @brief Handles completed WEFAX images for optional unattended auto-save.
     */
    void handleWeatherFaxImageCompleted(const QImage &image, const QString &reason);

    /**
     * @brief Handles completed SSTV images.
     */
    void handleSstvImageCompleted(const QImage &image, const QString &reason);

    /**
     * @brief Applies a WEFAX line-count preset to the UI.
     */
    void handleWeatherFaxLinePresetChanged(int index);

    /**
     * @brief Opens a folder picker for automatic WEFAX image saving.
     */
    void browseWeatherFaxOutputFolder();

    /**
     * @brief Updates the image zoom status label.
     */
    void handleFaxImageZoomChanged(int percent, bool fitMode);

    /**
     * @brief Handles mode changes and updates mode-specific UI.
     */
    void handleModeChanged(const QString &modeName);

    /**
     * @brief Requests a user-visible modem mode change.
     *
     * Purpose:
     * - Stop any active RX or TX safely.
     * - Apply the requested mode after the audio path is idle.
     * - Restart RX automatically when the request interrupted an active session.
     */
    void requestModeChange(const QString &modeName);

    /**
     * @brief Routes one audio block to the active modem decoder.
     */
    void handleRxAudioBlock(const AudioBlock &block);

    /**
     * @brief Applies a narrow marker-relative AFC nudge for text modes.
     */
    void updateTextModeAfc(const AudioBlock &block);

    /**
     * @brief Retunes RTTY markers from AFC without clearing the text decoder.
     */
    void retuneRttyFromAfc(int markHz, int spaceHz);

    /**
     * @brief Updates the waterfall markers using the selected modem metadata.
     */
    void updateWaterfallMarkers();

    /**
     * @brief Applies a waterfall click as a direct marker/tone tuning request.
     */
    void handleWaterfallFrequencyClicked(double frequencyHz, Qt::MouseButton button);

    /**
     * @brief Applies MeteoFax UI parameters to the decoder and settings file.
     */
    void applyWeatherFaxSettings();

    /**
     * @brief Applies SSTV UI parameters to the decoder and settings file.
     */
    void applySstvSettings();

    /**
     * @brief Applies RTTY UI parameters to the decoder and settings file.
     */
    void applyRttySettings();

    /**
     * @brief Clears the received RTTY text buffer.
     */
    void clearRttyRxText();

    /**
     * @brief Clears the RTTY TX text editor.
     */
    void clearRttyTxText();

    /**
     * @brief Loads RTTY TX text from a plain-text file.
     */
    void loadRttyTxTextFile();

    /**
     * @brief Handles received RTTY decoded text.
     */
    void handleRttyTextUpdated(const QString &text);

    /**
     * @brief Applies an RTTY preset to baud/shift/tone controls.
     */
    void handleRttyPresetChanged(int index);

    /**
     * @brief Applies BPSK UI parameters to the decoder and settings file.
     */
    void applyBpsk31Settings();

    /**
     * @brief Applies MFSK UI parameters to the decoder and settings file.
     */
    void applyMfskSettings();

    /**
     * @brief Applies CW Morse UI parameters to the decoder and settings file.
     */
    void applyCwSettings();

    /**
     * @brief Applies Feld Hell UI parameters to the decoder and settings file.
     */
    void applyHellSettings();

    /**
     * @brief Clears the received BPSK text buffer.
     */
    void clearBpsk31RxText();

    /**
     * @brief Clears the received MFSK text buffer.
     */
    void clearMfskRxText();

    /**
     * @brief Clears the received CW Morse text buffer.
     */
    void clearCwRxText();

    /**
     * @brief Clears the CW Morse TX text editor.
     */
    void clearCwTxText();

    /**
     * @brief Loads CW Morse TX text from a plain-text file.
     */
    void loadCwTxTextFile();

    /**
     * @brief Clears the Feld Hell TX text editor.
     */
    void clearHellTxText();

    /**
     * @brief Loads Feld Hell TX text from a plain-text file.
     */
    void loadHellTxTextFile();

    /**
     * @brief Clears the BPSK TX text editor.
     */
    void clearBpsk31TxText();

    /**
     * @brief Loads BPSK TX text from a plain-text file.
     */
    void loadBpsk31TxTextFile();

    /**
     * @brief Handles received BPSK decoded text.
     */
    void handleBpsk31TextUpdated(const QString &text);

    /**
     * @brief Clears the MFSK TX text editor.
     */
    void clearMfskTxText();

    /**
     * @brief Loads MFSK TX text from a plain-text file.
     */
    void loadMfskTxTextFile();

    /**
     * @brief Handles received MFSK decoded text.
     */
    void handleMfskTextUpdated(const QString &text);

    /**
     * @brief Handles received CW Morse decoded text.
     */
    void handleCwTextUpdated(const QString &text);

    /**
     * @brief Sends the pending Feld Hell input line.
     */
    void sendHellTxText();

    /**
     * @brief Opens and analyzes a WEFAX WAV test file.
     */
    void openWeatherFaxWavFile();

    /**
     * @brief Opens and analyzes an FT4/FT8 WAV test file through the isolated FT decoder worker.
     */
    void openFtWavFile();

    /**
     * @brief Feeds a lightweight offline WAV fingerprint to MIND before FT WAV decode results arrive.
     */
    void primeMINDFromWav(const QString &modeName, const QString &fileName);

    /**
     * @brief Runs the bundled FT8 WAV benchmark in Fast and Deep modes.
     */
    void runFtAutoTest();

    /**
     * @brief Forces live MeteoFax reception to start without waiting for APT start/phasing.
     */
    void forceWeatherFaxManualRx();

    /**
     * @brief Clears the current MeteoFax image.
     */
    void resetWeatherFaxImage();

    /**
     * @brief Saves the current MeteoFax image as PNG.
     */
    void saveWeatherFaxImage();

    /**
     * @brief Opens and analyzes an SSTV WAV test file.
     */
    void openSstvWavFile();

    /**
     * @brief Forces live SSTV reception to start in manual/free-running mode.
     */
    void forceSstvManualRx();

    /**
     * @brief Clears the current SSTV image.
     */
    void resetSstvImage();

    /**
     * @brief Saves the current SSTV image as PNG.
     */
    void saveSstvImage();

    /**
     * @brief Opens the Audio/PTT settings dialog.
     */
    void showAudioPttSettings();

    /**
     * @brief Opens the QSSTV-style sound-card calibration dialog.
     */
    void showSoundCardCalibration();

    /**
     * @brief Opens the text macro settings dialog.
     */
    void showTextMacroSettings();

    /**
     * @brief Opens the ADIF logbook browser/editor window.
     */
    void showLogbookDialog();

    /**
     * @brief Opens the internal ADIF logbook file path settings dialog.
     */
    void showLogbookSettings();

    /**
     * @brief Opens Hamlib CAT/PTT settings.
     */
    void showRigControlSettings();

    /**
     * @brief Opens the unified WSJT-X-style application settings dialog.
     */
    void showAppSettingsDialog();

    /**
     * @brief Opens the unified settings dialog on a specific page.
     */
    void showAppSettingsDialogPage(AppSettingsDialog::InitialPage page);

    void pointCatRotatorToDirectTarget(const QString &query);

    /**
     * @brief Opens the SSTV image editor dialog.
     */
    void openSstvImageEditor();

    /**
     * @brief Rebuilds the prepared SSTV TX card preview from the base image and side-panel QSO fields.
     */
    void updateSstvTxPreparedImage();

    /**
     * @brief Adds the active mode QSO form contents to the ADIF logbook.
     */
    void addActiveQsoToLog();

    /**
     * @brief Updates read-only UTC fields in all QSO forms.
     */
    void updateQsoUtcFields();

    /**
     * @brief Sends the pending RTTY input line.
     */
    void sendRttyTxText();

    /**
     * @brief Sends the pending BPSK31 input line.
     */
    void sendBpsk31TxText();

    /**
     * @brief Sends the pending MFSK input line.
     */
    void sendMfskTxText();

    /**
     * @brief Sends the pending CW Morse input line.
     */
    void sendCwTxText();

    /**
     * @brief Applies FT8 shell UI values to settings and waterfall markers.
     */
    void applyFt8Settings();
    QString ft8TxFrequencyStrategyKey() const;
    int chooseFt8AutoFreeTxFrequency(int wantedHz = 0, QString *reason = nullptr) const;
    int resolveFt8TxFrequencyForStrategy(int correspondentHz, QString *reason = nullptr) const;
    void applyFt8RuntimeFrequencySelection(int rxHz, const QString &source, bool updateTx = true);
    void rememberFt8AudioActivity(const Ft8RxDecoder::Decode &decode, const QString &callHint = QString());
    void pruneFt8AudioActivity();
    bool reclaimFt8ActiveQsoAwayFromQrm(const Ft8RxDecoder::Decode &decode,
                                        const FtQsoSequencer::ParsedMessage &parsed,
                                        const QString &reason);

    /**
     * @brief Refreshes FT8 standard message preview from current calls/grid fields.
     */
    void refreshFt8StandardMessages();

    /**
     * @brief Updates the FT4/FT8 UTC slot status label from the scheduler clock.
     */
    void updateFt8SlotStatus();

    /**
     * @brief Shows the FT UTC clock only while an FT4/FT8 mode is selected.
     */
    void updateFtUtcClockVisibility(const QString &modeName = QString());

    void handleFtSlotUpdated(const QString &modeLabel,
                             int slotMs,
                             int cycleMs,
                             bool firstPeriodNow,
                             bool txWindow,
                             int cyclePosMs,
                             int slotElapsedMs,
                             int remainMs,
                             qint64 nowUtcMs);
    void handleFtScheduledPttPrearmDue(const QString &token,
                                        qint64 slotBoundaryUtcMs,
                                        int audioTargetDelayMs,
                                        int pttLeadMs,
                                        qint64 nowUtcMs);
    void handleFtScheduledTxDue(const QString &token,
                                qint64 slotBoundaryUtcMs,
                                int audioTargetDelayMs,
                                int pttLeadMs,
                                qint64 nowUtcMs);
    void handleFtSchedulerPendingChanged(bool pending, qint64 slotBoundaryUtcMs);

    /**
     * @brief Copies a decoded FT8 callsign/grid from the selected RX row into the QSO fields.
     */
    void handleFt8DecodeDoubleClicked(QTableWidgetItem *item);

    /**
     * @brief Clears the visible FT4/FT8 decode table and transient waterfall callouts.
     */
    void clearFt8DecodeList();

    /**
     * @brief Starts the FT8 live receiver/decoder.
     */
    void startFt8RxShell();

    /**
     * @brief Appends one decoded FT8 message to the RX table.
     */
    void handleFt8DecodeReady(const Ft8RxDecoder::Decode &decode);

    /**
     * @brief Shows profiling data for the latest background FT8 slot decode.
     */
    void handleFt8DecodePerformance(const Ft8RxDecoder::PerfStats &stats);
    void handleFtOfflineAnalysisFinished(const QString &filePath, bool ok, int decodeCount, const QString &message);
    void handleFt8NtpStatus(bool synced, const QString &statusText);
    void handleFt8NtpOffset(double offsetMs);

    /**
     * @brief Refreshes the FT8 decode performance label after language changes.
     */
    void updateFt8DecodePerformanceUi();

    /**
     * @brief Updates transient FT8 CQ/reply callouts drawn over the waterfall.
     */
    void updateFt8WaterfallOverlays();

    /**
     * @brief Adds a CQ or direct-reply callout from one decoded FT8 message.
     */
    void addFt8WaterfallOverlayForDecode(const Ft8RxDecoder::Decode &decode, bool blacklistedDecode = false);

    /**
     * @brief Feeds one received FT8 decode into the QSO auto-sequencer.
     */
    void processFt8SequencerDecode(const Ft8RxDecoder::Decode &decode);
    bool shouldParkFt8LateReply(const Ft8RxDecoder::Decode &decode, const FtQsoSequencer::ParsedMessage &parsed) const;
    void parkFt8LateReply(const Ft8RxDecoder::Decode &decode, const FtQsoSequencer::ParsedMessage &parsed);
    void processParkedFt8LateReplies();

    /**
     * @brief Starts a supervised WSJT-Z-style full-auto answer to a decoded CQ when enabled.
     */
    bool tryStartFt8FullAutoQso(const Ft8RxDecoder::Decode &decode);

    /**
     * @brief Runs the saved visual AutoQSO flow in read-only shadow mode and logs the chosen path.
     */
    void runFtAutoQsoFlowShadowForDecode(const Ft8RxDecoder::Decode &decode, bool blacklistedDecode, bool watchedDecode);

    /**
     * @brief Gates WSJT-Z-style unattended helpers behind the explicit Evil Mode unlock.
     */
    void handleFt8EvilModeToggled(bool enabled);

    /**
     * @brief Shows/hides Auto CQ and Auto QSO controls according to Evil Mode state.
     */
    void updateFt8EvilModeVisibility();

    /**
     * @brief Starts/stops WSJT-Z-style Auto CQ when the gated checkbox is toggled.
     */
    void handleFt8AutoCqToggled(bool enabled);

    /**
     * @brief Enables/disables supervised WSJT-Z-style Auto QSO CQ answering for FT4/FT8.
     */
    void handleFt8FullAutoQsoToggled(bool enabled);

    /**
     * @brief Selects the opposite transmit period from the decoded station slot.
     */
    void selectFt8OppositePeriodFromDecode(const Ft8RxDecoder::Decode &decode);

    /**
     * @brief Updates the visible FT8 sequencer status label.
     */
    void updateFt8SequencerUi();

    /**
     * @brief Moves the FT standard-message arrow to the row currently armed by the sequencer.
     */
    void setFt8ActiveTxRow(int row, bool clearWhenIdle = false);

    /**
     * @brief Updates the large FT4/FT8 TX banner with the exact armed/on-air message.
     */
    void updateFt8TxBannerUi();

    /**
     * @brief Updates the FT8 SNR/signal-report display from the latest or selected decode.
     */
    void updateFt8SignalReportUi();

    /**
     * @brief Starts or restarts repeated CQ calling with the configured timeout.
     */
    void startFt8CqRepeat();

    /**
     * @brief Stops the FT8 automatic sequencer and CQ repeat state.
     */
    void stopFt8Sequencer(const QString &reason = QString());

    /**
     * @brief Schedules one FT8 message for the next selected TX period.
     */
    void scheduleFt8SequencerMessage(const QString &message, const QString &tag);

    /**
     * @brief Handles state transitions after a locally generated FT8 slot finished.
     */
    void handleFt8TxCompleted();

    /**
     * @brief Completes an FT8 QSO, optionally logs it, and resumes CQ if requested.
     */
    void completeFt8Qso(const QString &reason);

    /**
     * @brief Saves the current FT8 QSO to the ADIF logbook without modal prompts.
     */
    void autoLogFt8Qso(const QString &reason);

    /**
     * @brief Selects one standard FT8 TX row and returns its generated text.
     */
    QString selectFt8TxRow(int row);

    /**
     * @brief Schedules/sends the selected FT8 standard message using the MSHV-derived TX path.
     */
    void startFt8TxShell();

    /**
     * @brief Stops live FT8 RX, pending FT8 TX, or active FT8 TX.
     */
    void stopFt8Shell();

    /**
     * @brief Starts a finite FT8 tune tone at the TX marker.
     */
    void tuneFt8Shell();

    /**
     * @brief Starts the pending FT8 transmission when the selected UTC slot opens.
     */
    void beginScheduledFt8Transmit();

    /**
     * @brief Minimal low-latency FT TX path used by the UTC scheduler.
     */
    void prearmFtPreparedSlotTransmit();

    /**
     * @brief Starts the already pre-armed FT audio worker at the scheduled waveform time.
     */
    void startFtPreparedSlotTransmit();

    /**
     * @brief Shows application information.
     */
    void showAboutMadModem();

    /**
     * @brief Opens the built-in MM help browser.
     */
    void showOnlineHelp();

    /**
     * @brief Enters Qt What's This context-help mode.
     */
    void enterWhatsThisMode();

    /**
     * @brief Opens the daily band/frequency scheduler editor.
     */
    void showBandSchedulerDialog();

    /**
     * @brief Periodically checks whether a scheduled QSY is due.
     */
    void handleBandSchedulerTick();

private:
    struct QsoFormWidgets
    {
        QWidget *container = nullptr;
        QLineEdit *callsign = nullptr;
        QLineEdit *rstSent = nullptr;
        QLineEdit *rstReceived = nullptr;
        QLineEdit *band = nullptr;
        QLineEdit *mode = nullptr;
        QLineEdit *grid = nullptr;
        QLineEdit *utc = nullptr;
        QPushButton *addButton = nullptr;
    };

    struct FtAutoTestItem
    {
        QString filePath;
        QString fileName;
        QString depthKey;
        QString depthLabel;
    };

    QString findFtAutoTestWavDirectory() const;
    void runNextFtAutoTestStep();
    void finishFtAutoTest();
    int ftAutoTestExpectedDecodes(const QString &fileName) const;
    QString ftAutoTestResultLabel(bool ok, int decodeCount, int expected) const;
    static int ftUnresolvedHashMarkerCount(const QString &message);
    static int ftVisibleAngleCallCount(const QString &message);

    /**
     * @brief Returns true when Evil Auto QSO is unlocked, idle and allowed to answer a CQ.
     */
    bool canStartFt8FullAutoQsoNow() const;

    /**
     * @brief Builds the worked-needed priority record for one decoded CQ.
     */
    Ft8FullAutoCqCandidate buildFt8FullAutoCqCandidate(const Ft8RxDecoder::Decode &decode) const;

    /**
     * @brief Adds a decoded CQ to the short Auto QSO priority buffer.
     */
    bool queueFt8FullAutoCqCandidate(const Ft8RxDecoder::Decode &decode);

    /**
     * @brief Selects and starts the best buffered Auto QSO CQ candidate.
     */
    void processFt8FullAutoCqCandidates();

    /**
     * @brief Computes the earliest safe retry-arm time after an FT TX slot.
     *
     * A retry must not stop the receiver before the just-started opposite RX
     * slot has reached the WSJT-X-style decode gate.
     */

    /**
     * @brief Selects the next sequencer message without arming the UTC TX scheduler yet.
     *
     * This is the important FT separation between the QSO state machine and
     * the radio/audio TX path: the sequencer may know the next retry/message
     * while the receiver must still keep collecting and decoding the current
     * opposite slot.  Actual PTT/audio arming remains deferred until the guard
     * time has expired.
     */

    /**
     * @brief Starts the actual FT QSO after priority selection has chosen a candidate.
     */
    bool startFt8FullAutoQsoFromCandidate(const Ft8FullAutoCqCandidate &candidate);

    void setupBandSchedulerTab();
    void updateBandSchedulerTabForMode(const QString &modeName);
    void setupDspTab();
    void updateDspTabForMode(const QString &modeName);
    void applyDspTabSettings();

    QWidget *createDspConditionerControls(const QString &modeKey, QWidget *parent);
    QString dspModeKeyForModeName(const QString &modeName) const;
    bool dspNoiseReductionEnabledForModeKey(const QString &modeKey) const;
    bool dspAgcEnabledForModeKey(const QString &modeKey) const;
    void setDspNoiseReductionEnabledForModeKey(const QString &modeKey, bool enabled);
    void setDspAgcEnabledForModeKey(const QString &modeKey, bool enabled);

    QString schedulerModeGroup(const QString &modeName) const;
    QString activeSchedulerModeGroup() const;
    bool isBandSchedulerEnabledForMode(const QString &modeName) const;
    void setBandSchedulerEnabledForMode(const QString &modeName, bool enabled);
    QList<ScheduledQsyEntry> schedulerEntries() const;
    bool shouldRunScheduledQsyForMode(const QString &modeName) const;
    bool canApplyScheduledQsyNow() const;
    void requestScheduledQsy(const ScheduledQsyEntry &entry, const QString &reason);
    void applyScheduledQsy(const ScheduledQsyEntry &entry);
    QString scheduledQsyDescription(const ScheduledQsyEntry &entry) const;

    /**
     * @brief Initializes static UI state.
     */
    void setupUiState();

    /**
     * @brief Creates custom runtime widgets inside Designer placeholders.
     */
    void setupCustomWidgets();

    /**
     * @brief Applies delayed tooltips and dense side-panel sizing.
     */
    void setupHelpTooltips();

    /**
     * @brief Connects audio, DSP, waterfall, and decoder signals.
     */
    void setupProcessingConnections();

    /**
     * @brief Connects UI actions and mode-specific controls.
     */
    void setupUiConnections();

    /**
     * @brief Populates standard WEFAX image line-count presets.
     */
    void populateWeatherFaxLinePresets();

    /**
     * @brief Loads current WeatherFax settings into UI controls.
     */
    void loadWeatherFaxSettingsToUi();

    /**
     * @brief Loads current SSTV settings into UI controls.
     */
    void loadSstvSettingsToUi();

    /**
     * @brief Loads current RTTY settings into runtime-created UI controls.
     */
    void loadRttySettingsToUi();

    /**
     * @brief Loads current BPSK settings into runtime-created UI controls.
     */
    void loadBpsk31SettingsToUi();

    /**
     * @brief Loads current MFSK settings into runtime-created UI controls.
     */
    void loadMfskSettingsToUi();

    /**
     * @brief Loads current CW Morse settings into runtime-created UI controls.
     */
    void loadCwSettingsToUi();

    /**
     * @brief Loads current Feld Hell settings into runtime-created UI controls.
     */
    void loadHellSettingsToUi();

    /**
     * @brief Creates runtime RTTY controls inside the mode stack.
     */
    void setupRttyPage();

    /**
     * @brief Creates runtime BPSK31 controls inside the mode stack.
     */
    void setupBpsk31Page();

    /**
     * @brief Creates runtime MFSK controls inside the mode stack.
     */
    void setupMfskPage();

    /**
     * @brief Creates runtime CW Morse controls inside the mode stack.
     */
    void setupCwPage();

    /**
     * @brief Creates runtime Feld Hell controls inside the mode stack.
     */
    void setupHellPage();

    /**
     * @brief Creates the FT8 UI shell and side-panel settings.
     */
    void setupFt8Page();

    /**
     * @brief Loads current FT8 shell settings into runtime-created controls.
     */
    void loadFt8SettingsToUi();

    /**
     * @brief Returns the selected FT8 TX table message, falling back to Tx 1.
     */
    QString selectedFt8TxMessage() const;

    /**
     * @brief Calculates milliseconds until the next selected FT8 TX period boundary.
     */
    int millisecondsToNextFt8TxPeriod() const;

    /**
     * @brief Returns the UTC epoch ms of the selected FT4/FT8 TX slot boundary.
     *
     * This is used by the FT TX scheduler to align the generated waveform to
     * the UTC slot in the same spirit as WSJT-X: if the audio engine starts
     * late, the beginning of the waveform is skipped rather than delaying the
     * whole over-the-air frame.
     */
    qint64 selectedFt8TxSlotBoundaryUtcMs() const;

    /**
     * @brief Updates FT8 table/status after a local transmission has been requested.
     */
    void appendFt8LocalTxRow(const QString &message, int frequencyHz, const QString &tag);
    void appendFt8QsoHistoryRow(const QString &utc,
                                const QString &direction,
                                const QString &dbOrTag,
                                const QString &dt,
                                int frequencyHz,
                                const QString &message,
                                const QColor &background,
                                const QColor &foreground = QColor(Qt::black));
    void scheduleFt8RetryIfNeeded();

    /**
     * @brief Creates the central text terminal pages used by RTTY/BPSK.
     */
    void setupTextTerminalPages();

    /**
     * @brief Builds one central text terminal page for a text modem.
     */
    QWidget *createTextTerminalPage(const QString &title,
                                    QPlainTextEdit **rxTerminal,
                                    QPlainTextEdit **txInput,
                                    QPushButton **clearRxButton,
                                    QPushButton **loadTextButton,
                                    QPushButton **clearInputButton,
                                    QPushButton **sendButton,
                                    QList<QPushButton *> *macroButtons,
                                    QsoFormWidgets **qsoForm);

    /**
     * @brief Wraps a text/FT display page with a second QSO map tab.
     */
    QWidget *wrapTextDisplayPageWithMap(QWidget *mainPage,
                                        const QString &mainTabTitle,
                                        const QString &modeFilter,
                                        QsoMapWidget **mapOut = nullptr,
                                        QTabWidget **tabsOut = nullptr,
                                        QWidget **mapPageOut = nullptr);
    void setSstvQsoMapVisible(bool visible);

    /**
     * @brief Refreshes all QSO map widgets from the current ADIF logbook.
     */
    void refreshQsoMaps();
    void recordHeardStationForMaps(const QString &callsign,
                                   const QString &grid,
                                   const QString &mode,
                                   const QString &band = QString(),
                                   const QString &comment = QString());
    void scanTextForHeardStations(QPlainTextEdit *terminal, const QString &newText);

    /**
     * @brief Updates the FT map filter when FT4/FT8 selection changes.
     */
    void updateFtQsoMapModeFilter();

    /**
     * @brief Creates the QSO log-entry strip shown under text-mode RX areas.
     */
    QWidget *createQsoFormPanel(QWidget *parent, const QString &modeLabel, QsoFormWidgets **qsoForm);

    /**
     * @brief Returns the QSO form belonging to the current active text mode.
     */
    QsoFormWidgets *activeQsoForm() const;

    /**
     * @brief Returns the QSO form associated with a specific RX terminal.
     */
    QsoFormWidgets *qsoFormForTerminal(QPlainTextEdit *terminal) const;

    /**
     * @brief Updates a QSO form with current mode/default values.
     */
    void populateQsoFormDefaults(QsoFormWidgets *form, const QString &modeName);

    /**
     * @brief Writes a QSO form to the ADIF logbook.
     */
    bool addQsoToLogFromForm(QsoFormWidgets *form);

    /**
     * @brief Installs right-click helpers on one RX text terminal.
     */
    void installRxTextContextMenu(QPlainTextEdit *terminal);

    /**
     * @brief Sends selected RX text to a selected QSO form field.
     */
    void sendSelectedRxTextToQsoField(QPlainTextEdit *terminal, const QString &fieldName);

    /**
     * @brief Highlights callsigns in one RX terminal using logbook duplicate state.
     */
    void highlightCallsignsInTerminal(QPlainTextEdit *terminal);
    void refreshFt8DecodeWorkedHighlights();
    bool isFtCallBlacklisted(const QString &call) const;
    bool isFtCallWatched(const QString &call) const;
    bool ftCountryAlreadyWorked(const QString &dxcc, const QString &countryName = QString()) const;
    bool ftCallWorkedWithinHours(const QString &call, int hours) const;

    /**
     * @brief Re-highlights all RX text terminals after logbook changes.
     */
    void refreshLogbookHighlights();

    /**
     * @brief Extracts a plausible ham-radio callsign from arbitrary selected text.
     */
    QString extractCallsignFromText(const QString &text) const;

    /**
     * @brief Returns the current mode name formatted for ADIF.
     */
    QString currentAdifMode() const;

    /**
     * @brief Switches the main central display between image and text-terminal pages.
     */
    void updateCentralDisplayForMode(const QString &modeName);

    /**
     * @brief Creates the modem-selection menu and keeps it synchronized with the hidden mode combo.
     */
    void setupModeMenu();

    /**
     * @brief Returns the short user-facing label for a modem mode.
     */
    QString shortModeLabel(const QString &modeName) const;

    /**
     * @brief Completes a pending automatic mode change after RX/TX has stopped.
     */
    void finishPendingModeChange();

    /**
     * @brief Populates standard RTTY presets.
     */
    void populateRttyPresets();

    /**
     * @brief Applies persistent settings to decoder/runtime state.
     */
    void applyPersistentSettingsToRuntime();

    void updateRigControlStatusUi();
    void setupCatRotatorModule();
    void setupCatRotatorSideTab();
    void setupRotatorStatusLamps();
    void updateRotatorStatusLamps();
    void applyCatRotatorSettings();
    void updateCatRotatorQsoTarget(const QString &reason = QString());
    mm::CatRotatorController::Config catRotatorConfigFromSettings() const;
    bool ftRotatorReadyForPendingTx(QString *reason = nullptr, int *etaMs = nullptr) const;
    void deferPendingFtTxForRotator(int etaMs, const QString &reason);
    void handleRigFrequencyChanged(double frequencyHz);
    void handleRigStatusChanged(const QString &status);
    QString bandFromFrequencyHz(double frequencyHz) const;
    double ftStandardFrequencyHz(const QString &modeName, const QString &band) const;
    QString ftBandFromFrequencyHz(double frequencyHz) const;
    bool isFtFrequencyOnStandardSlot(double frequencyHz, const QString &modeName, const QString &band) const;
    void updateFtBandFrequencyUi();
    void qsyRigToSelectedFtBand();
    QString stationCallsign() const;
    QString stationLocator() const;
    bool stationIdentityReady(QString *reason = nullptr) const;
    bool ensureStationIdentityForTx(const QString &modeLabel);
    QString formatRigFrequency(double frequencyHz) const;

    /**
     * @brief Saves persistent settings and logs failures.
     */
    void savePersistentSettings();

    /**
     * @brief Returns the default output directory for automatic WEFAX PNG files.
     */
    QString defaultWeatherFaxOutputFolder() const;

    /**
     * @brief Builds an automatic WeatherFAX PNG file name.
     */
    QString makeWeatherFaxAutoSaveFileName() const;

    /**
     * @brief Saves a WEFAX image to a specific PNG file.
     */
    bool saveWeatherFaxImageToFile(const QImage &image, const QString &fileName);

    /**
     * @brief Updates the line preset combo when manual values no longer match a preset.
     */
    void updateLinePresetSelectionFromCurrentValues();

    /**
     * @brief Builds the currently selected TX image modulator.
     */
    std::unique_ptr<TxModulator> buildCurrentTxModulator();

    /**
     * @brief Updates the displayed TX preview after image/mode changes.
     */
    void updateTxPreview();

    /**
     * @brief Applies enabled/disabled state to TX controls.
     */
    void updateTxControlState();

    /**
     * @brief Opens the configured RTS PTT line for image TX.
     */
    bool keyPttForTx();

    /**
     * @brief Releases the RTS PTT line after image TX.
     */
    void unkeyPttAfterTx();

    /**
     * @brief Returns the active audio output backend name.
     */
    QString selectedAudioOutputName() const;

    /**
     * @brief Returns the active audio output display label.
     */
    QString selectedAudioOutputLabel() const;

    /**
     * @brief Selects one item in a combo by its backend itemData().
     */
    void selectComboByBackendName(QComboBox *combo, const QString &backendName);

    /**
     * @brief Appends a timestamped line to the log panel.
     */
    void appendLog(const QString &message);
    void showRuntimeLogDialog();
    void appendRuntimeLogLine(const QString &line);
    void invokeRigConfigureFromSettings();
    bool invokeRigPttBlocking(bool enabled);
    void invokeRigSetFrequency(double frequencyHz);

    /**
     * @brief Applies receiver running/stopped UI state.
     */
    void setReceiverRunning(bool running);

    /**
     * @brief Updates the compact Ready/RX/TX main-state button.
     */
    void updateMainStateButton();

    /**
     * @brief Safely leaves live RX before an offline WAV analysis pass.
     */
    bool prepareForOfflineAnalysis(const QString &label);

    /**
     * @brief Populates available audio input devices.
     */
    void populateAudioInputs();

    /**
     * @brief Populates available audio output devices.
     */
    void populateAudioOutputs();

    /**
     * @brief Populates available serial ports.
     */
    void populateSerialPorts();

    /**
     * @brief Returns the active audio input backend name.
     */
    QString selectedAudioInputName() const;

    /**
     * @brief Returns the active audio input display label.
     */
    QString selectedAudioInputLabel() const;

    /**
     * @brief Returns the active PTT serial port name.
     */
    QString selectedPttPort() const;

    /**
     * @brief Decodes a PCM/float WAV file directly through the RX DSP chain.
     */
    bool analyzeWeatherFaxWavFile(const QString &fileName);

    /**
     * @brief Decodes a PCM/float WAV file directly through the active SSTV path.
     */
    bool analyzeSstvWavFile(const QString &fileName);

    /**
     * @brief Applies the shared DSP conditioning profile for the active modem.
     */
    AudioBlock conditionAudioForActiveMode(const AudioBlock &block);
    AudioBlock conditionAudioForWaterfall(const AudioBlock &block);

    /**
     * @brief Refreshes text macro button labels after settings changes.
     */
    void refreshTextMacroButtons();

    /**
     * @brief Expands QSO text variables inside a macro/template.
     */
    QString expandTextTemplate(const QString &source) const;

    /**
     * @brief Appends local TX echo text into the current text terminal.
     */
    void appendTextTerminal(QPlainTextEdit *terminal, const QString &prefix, const QString &text);

    /**
     * @brief Prepares the active text TX editor for progress highlighting.
     */
    void beginTextTxHighlight(QPlainTextEdit *editor);

    /**
     * @brief Resets all character formatting inside one text TX editor.
     */
    void resetTextTxHighlight(QPlainTextEdit *editor);

    /**
     * @brief Updates the green transmitted-character highlight from TX progress.
     */
    void updateTextTxHighlight(double progress);

    /**
     * @brief Clears the active text TX progress highlighter state.
     */
    void endTextTxHighlight();

    /**
     * @brief Appends decoded RX text as a continuous terminal stream.
     */
    void appendRxTextTerminal(QPlainTextEdit *terminal,
                              const QString &text,
                              bool requireLineBreakPair,
                              bool *pendingLineBreak);

    /**
     * @brief Starts text transmission in the active text mode.
     */
    bool startTextModeTx(const QString &text);

    /**
     * @brief Expands and transmits one stored macro.
     */
    void sendTextMacro(int index);

    /**
     * @brief Clears the worker-thread DSP engine safely.
     */
    void resetDspEngine();

    /**
     * @brief Sends selected WAV analysis blocks to the waterfall DSP engine.
     *
     * WAV decoding can run much faster than real time; the waterfall is only a
     * diagnostic display, so this path decimates display updates and never blocks
     * the decoder on FFT work.
     */
    void processDspAudioBlockForWav(const AudioBlock &block);

    /**
     * @brief Resets the local TX-side RTTY CRT trace state.
     */
    void resetRttyTxScopeState();

    /**
     * @brief Updates the RTTY crossed-ellipse scope from TX tone metadata.
     *
     * TX scope rendering must never demodulate the generated PCM.  The
     * transmitter already knows whether it is currently sending Mark or Space,
     * so the scope is driven from that metadata only.
     */
    void updateRttyScopeFromTxToneState(bool mark, double progress);

private:
    void setUiLanguageFromAction(QAction *action);
    void setupLanguageMenu();
    void loadUiLanguageSetting();
    void saveUiLanguageSetting() const;
    void loadUiTranslationFile(const QString &languageCode);
    void applyUiLanguage();
    void applyUiLanguageToObjectTree(QObject *root);
    void translateObjectTree(QObject *object);
    QString uiText(const QString &key, const QString &fallback) const;
    QString uiTextFromSource(const QString &prefix, const QString &source) const;
    QString localizedRuntimeText(const QString &prefix, const QString &message) const;

    Ui::MainWindow *ui = nullptr;

    AppSettings m_settings;
    AdifLogbook m_logbook;
    QTimer m_qsoUtcTimer;
    QString m_uiLanguageCode = QStringLiteral("en");
    QHash<QString, QString> m_uiTranslations;
    QActionGroup *m_languageActionGroup = nullptr;
    QMenu *m_menuLanguage = nullptr;
    QAction *m_actionLogbook = nullptr;
    QAction *m_actionSstvEditor = nullptr;
    QAction *m_actionSoundCardCalibration = nullptr;
    QAction *m_actionLogbookSettings = nullptr;
    QAction *m_actionRigControlSettings = nullptr;
    QAction *m_actionAppSettings = nullptr;
    QAction *m_actionFtAnalyzeWav = nullptr;
    QAction *m_actionFtAutoTest = nullptr;
    QAction *m_actionHelpContents = nullptr;
    QAction *m_actionWhatsThisMode = nullptr;

    AudioEngine *m_audioEngine = nullptr;
    TxAudioEngine *m_txAudioEngine = nullptr;
    FtTxWorker *m_ftTxWorker = nullptr;
    QThread *m_ftTxThread = nullptr;
    bool m_ftTxWorkerRunning = false;
    DspEngine *m_dspEngine = nullptr;
    QThread *m_dspThread = nullptr;
    WeatherFaxDecoder *m_weatherFaxDecoder = nullptr;
    SstvDecoder *m_sstvDecoder = nullptr;
    RttyDecoder *m_rttyDecoder = nullptr;
    RttyMultiDecoder *m_rttyMultiDecoder = nullptr;
    Bpsk31Decoder *m_bpsk31Decoder = nullptr;
    MfskDecoder *m_mfskDecoder = nullptr;
    CwDecoder *m_cwDecoder = nullptr;
    HellschreiberDecoder *m_hellDecoder = nullptr;
    Ft8RxDecoder *m_ft8RxDecoder = nullptr;
    QThread *m_ft8RxThread = nullptr;
    NtpClient *m_ntpClient = nullptr;
    HamlibController *m_rigController = nullptr;
    QThread *m_rigThread = nullptr;
    DeepDspController *m_ddspController = nullptr;
    DdspPanelWidget *m_ddspPanelWidget = nullptr;
    bool m_rigCatConnected = false;
    QString m_rigStatusMirror;
    QDialog *m_runtimeLogDialog = nullptr;
    QPlainTextEdit *m_runtimeLogText = nullptr;
    QPushButton *m_btnShowRuntimeLog = nullptr;
    DspConditioner m_decoderConditioner;
    DspConditioner m_waterfallConditioner;
    int m_wavWaterfallDecimationCounter = 0;

    WaterfallWidget *m_waterfallWidget = nullptr;
    FaxImageWidget *m_faxImageWidget = nullptr;
    QStackedWidget *m_mainDisplayStack = nullptr;
    QWidget *m_imageDisplayPage = nullptr;
    QTabWidget *m_imageDisplayTabs = nullptr;
    QWidget *m_sstvMapPage = nullptr;
    QsoMapWidget *m_sstvQsoMapWidget = nullptr;
    QWidget *m_rttyDisplayPage = nullptr;
    QWidget *m_bpsk31DisplayPage = nullptr;
    QWidget *m_mfskDisplayPage = nullptr;
    QWidget *m_cwDisplayPage = nullptr;
    QWidget *m_hellDisplayPage = nullptr;
    QWidget *m_ft8DisplayPage = nullptr;
    QLabel *m_lblHellRaster = nullptr;
    QScrollArea *m_scrollHellRaster = nullptr;

    QGroupBox *m_grpTxImage = nullptr;
    QLabel *m_lblTxImageName = nullptr;
    QLabel *m_lblTxMode = nullptr;
    QProgressBar *m_progressTx = nullptr;
    QPushButton *m_btnLoadTxImage = nullptr;
    QPushButton *m_btnStartImageTx = nullptr;
    QPushButton *m_btnStopImageTx = nullptr;
    QLabel *m_lblSstvTxPreview = nullptr;
    QLabel *m_lblSstvTxCall = nullptr;
    QLabel *m_lblSstvTxName = nullptr;
    QLabel *m_lblSstvTxQth = nullptr;
    QLabel *m_lblSstvTxInfo = nullptr;
    QLineEdit *m_editSstvTxCall = nullptr;
    QLineEdit *m_editSstvTxName = nullptr;
    QLineEdit *m_editSstvTxQth = nullptr;
    QLineEdit *m_editSstvTxReport = nullptr;
    QPushButton *m_btnSstvEditor = nullptr;
    QPushButton *m_btnFaxForceRx = nullptr;
    QPushButton *m_btnSstvForceRx = nullptr;
    QImage m_sstvTxBaseImage;

    QWidget *m_pageRttySettings = nullptr;
    QComboBox *m_cmbRttyPreset = nullptr;
    QDoubleSpinBox *m_spinRttyBaud = nullptr;
    QSpinBox *m_spinRttyShiftHz = nullptr;
    QSpinBox *m_spinRttyMarkHz = nullptr;
    QCheckBox *m_chkRttyReverse = nullptr;
    QCheckBox *m_chkRttyAfc = nullptr;
    QSpinBox *m_spinRttyAfcRangeHz = nullptr;
    QCheckBox *m_chkRttyMultiDecode = nullptr;
    QCheckBox *m_chkRttyOverlayCallsigns = nullptr;
    QCheckBox *m_chkRttyContestEnhanced = nullptr;
    QCheckBox *m_chkRttySecondPass = nullptr;
    QSpinBox *m_spinRttyMaxDecoders = nullptr;
    RttyScopeWidget *m_rttyScopeWidget = nullptr;
    QPlainTextEdit *m_txtRttyRx = nullptr;
    QPlainTextEdit *m_txtRttyTx = nullptr;
    QPushButton *m_btnRttyClearRx = nullptr;
    QPushButton *m_btnRttyLoadTxText = nullptr;
    QPushButton *m_btnRttyClearTx = nullptr;
    QPushButton *m_btnRttySend = nullptr;
    QList<QPushButton *> m_rttyMacroButtons;
    QsoFormWidgets *m_rttyQsoForm = nullptr;

    QWidget *m_pageBpsk31Settings = nullptr;
    QComboBox *m_cmbBpsk31Variant = nullptr;
    QSpinBox *m_spinBpsk31ToneHz = nullptr;
    QCheckBox *m_chkBpsk31Afc = nullptr;
    QSpinBox *m_spinBpsk31AfcRangeHz = nullptr;
    QCheckBox *m_chkBpsk31Invert = nullptr;
    QPlainTextEdit *m_txtBpsk31Rx = nullptr;
    QPlainTextEdit *m_txtBpsk31Tx = nullptr;
    QPushButton *m_btnBpsk31ClearRx = nullptr;
    QPushButton *m_btnBpsk31LoadTxText = nullptr;
    QPushButton *m_btnBpsk31ClearTx = nullptr;
    QPushButton *m_btnBpsk31Send = nullptr;
    QList<QPushButton *> m_bpsk31MacroButtons;
    QsoFormWidgets *m_bpsk31QsoForm = nullptr;

    QWidget *m_pageMfskSettings = nullptr;
    QComboBox *m_cmbMfskVariant = nullptr;
    QSpinBox *m_spinMfskCenterHz = nullptr;
    QCheckBox *m_chkMfskAfc = nullptr;
    QSpinBox *m_spinMfskAfcRangeHz = nullptr;
    QPlainTextEdit *m_txtMfskRx = nullptr;
    QPlainTextEdit *m_txtMfskTx = nullptr;
    QPushButton *m_btnMfskClearRx = nullptr;
    QPushButton *m_btnMfskLoadTxText = nullptr;
    QPushButton *m_btnMfskClearTx = nullptr;
    QPushButton *m_btnMfskSend = nullptr;
    QList<QPushButton *> m_mfskMacroButtons;
    QsoFormWidgets *m_mfskQsoForm = nullptr;

    QWidget *m_pageCwSettings = nullptr;
    QSpinBox *m_spinCwToneHz = nullptr;
    QSpinBox *m_spinCwWpm = nullptr;
    QCheckBox *m_chkCwAutoWpm = nullptr;
    QLabel *m_lblCwTrackedWpm = nullptr;
    QSpinBox *m_spinCwBandwidthHz = nullptr;
    QCheckBox *m_chkCwAfc = nullptr;
    QSpinBox *m_spinCwAfcRangeHz = nullptr;
    QPlainTextEdit *m_txtCwRx = nullptr;
    QPlainTextEdit *m_txtCwTx = nullptr;
    QPushButton *m_btnCwClearRx = nullptr;
    QPushButton *m_btnCwLoadTxText = nullptr;
    QPushButton *m_btnCwClearTx = nullptr;
    QPushButton *m_btnCwSend = nullptr;
    QList<QPushButton *> m_cwMacroButtons;
    QsoFormWidgets *m_cwQsoForm = nullptr;

    QWidget *m_pageHellSettings = nullptr;
    QComboBox *m_cmbHellVariant = nullptr;
    QSpinBox *m_spinHellToneHz = nullptr;
    QDoubleSpinBox *m_spinHellColumnRate = nullptr;
    QSpinBox *m_spinHellBandwidthHz = nullptr;
    QCheckBox *m_chkHellAfc = nullptr;
    QSpinBox *m_spinHellAfcRangeHz = nullptr;
    QPlainTextEdit *m_txtHellTx = nullptr;
    QPushButton *m_btnHellLoadTxText = nullptr;
    QPushButton *m_btnHellClearTx = nullptr;
    QPushButton *m_btnHellSend = nullptr;
    QPushButton *m_btnHellResetImage = nullptr;
    QList<QPushButton *> m_hellMacroButtons;
    QsoFormWidgets *m_hellQsoForm = nullptr;

    using Ft8SequencerState = FtQsoSequencer::State;

    QWidget *m_pageFt8Settings = nullptr;
    QWidget *m_pageFt8Time = nullptr;
    QTableWidget *m_tableFt8Rx = nullptr;
    QTableWidget *m_tableFt8QsoHistory = nullptr;
    QPushButton *m_btnFt8ClearRx = nullptr;
    QCheckBox *m_chkFt8AutoScroll = nullptr;
    QTableWidget *m_tableFt8TxMessages = nullptr;
    QLineEdit *m_editFt8MyCall = nullptr;
    QLineEdit *m_editFt8MyGrid = nullptr;
    QLineEdit *m_editFt8DxCall = nullptr;
    QLineEdit *m_editFt8DxGrid = nullptr;
    QComboBox *m_cmbFt8Band = nullptr;
    QLabel *m_lblFt8BandStatus = nullptr;
    QSpinBox *m_spinFt8RxFreq = nullptr;
    QSpinBox *m_spinFt8TxFreq = nullptr;
    QLabel *m_lblFt8DecodeEngine = nullptr;
    QRadioButton *m_radioFt8TxFirst = nullptr;
    QRadioButton *m_radioFt8TxSecond = nullptr;
    QLabel *m_lblFt8SlotStatus = nullptr;
    QWidget *m_grpFt8UtcClock = nullptr;
    Ft8SlotClockWidget *m_ft8SlotClock = nullptr;
    QLabel *m_lblFt8PeriodStatus = nullptr;
    QLCDNumber *m_lcdFt8UtcClock = nullptr;
    QLabel *m_lblFt8WindowStatus = nullptr;
    QPushButton *m_btnFt8GenerateStd = nullptr;
    QPushButton *m_btnFt8Rx = nullptr;
    QPushButton *m_btnFt8Tx = nullptr;
    QPushButton *m_btnFt8Stop = nullptr;
    QPushButton *m_btnFt8Tune = nullptr;
    QCheckBox *m_chkFt8AutoSeq = nullptr;
    QCheckBox *m_chkFt8CqRepeat = nullptr;
    QLabel *m_lblFt8CqTimeout = nullptr;
    QCheckBox *m_chkFt8AutoLog = nullptr;
    QCheckBox *m_chkFt8FullAutoQso = nullptr;
    QCheckBox *m_chkFt8EvilMode = nullptr;
    bool m_ft8EvilModeUnlocked = false;
    QVector<Ft8FullAutoCqCandidate> m_ft8FullAutoCqCandidates;
    QTimer m_ft8FullAutoCqSelectionTimer;
    QHash<QString, QDateTime> m_ft8AutoNoResponseCooldown;
    struct Ft8ParkedLateReply
    {
        Ft8RxDecoder::Decode decode;
        QString senderCall;
        QDateTime heardUtc;
    };
    QVector<Ft8ParkedLateReply> m_ft8ParkedLateReplies;
    QWidget *m_tabBandScheduler = nullptr;
    QWidget *m_tabDspSettings = nullptr;
    QLabel *m_lblDspMode = nullptr;
    QGroupBox *m_grpDspCore = nullptr;
    QGroupBox *m_grpDspRtty = nullptr;
    QGroupBox *m_grpDspBpsk = nullptr;
    QGroupBox *m_grpDspImage = nullptr;
    QCheckBox *m_chkDspSoftwareAgc = nullptr;
    QCheckBox *m_chkDspNoiseReduction = nullptr;
    QCheckBox *m_chkDspAdaptiveLineEnhancer = nullptr;
    QCheckBox *m_chkDspRttyMatchedFilter = nullptr;
    QCheckBox *m_chkDspRttyMarkSpaceEnhancer = nullptr;
    QCheckBox *m_chkDspBpskCoherentTracking = nullptr;
    QCheckBox *m_chkDspImageWaveletDenoise = nullptr;
    QWidget *m_tabFtDecodeDiagnostics = nullptr;
    QLabel *m_lblBandSchedulerMode = nullptr;
    QCheckBox *m_chkBandSchedulerEnabled = nullptr;
    QPushButton *m_btnBandSchedulerSettings = nullptr;
    QLabel *m_lblBandSchedulerHint = nullptr;
    QTimer m_bandSchedulerTimer;
    QHash<QString, QList<QCheckBox *>> m_dspNoiseReductionChecks;
    QHash<QString, QList<QCheckBox *>> m_dspAgcChecks;
    QSet<QString> m_bandSchedulerTriggeredKeys;
    bool m_hasPendingScheduledQsy = false;
    ScheduledQsyEntry m_pendingScheduledQsy;
    QString m_pendingScheduledQsyReason;
    QCheckBox *m_chkFt8HoldTxFreq = nullptr;
    QComboBox *m_cmbFt8TxStrategy = nullptr;
    QCheckBox *m_chkFt8DeepDecode = nullptr;
    QCheckBox *m_chkFt8DspPlusDecode = nullptr;
    QComboBox *m_cmbFtLiveDecodeDepth = nullptr;
    QPushButton *m_btnFt8AnalyzeWav = nullptr;
    QPushButton *m_btnFtDecodeAnalyzeWav = nullptr;
    QPushButton *m_btnFtDecodeAutoTest = nullptr;
    QSpinBox *m_spinFt8CqTimeoutMin = nullptr;
    QSpinBox *m_spinFt8NoResponseLimit = nullptr;
    QLabel *m_lblFt8NoResponseLimit = nullptr;
    QLabel *m_lblFt8SequencerStatus = nullptr;
    QLabel *m_lblFt8TxBanner = nullptr;
    QLabel *m_lblFt8LastSnr = nullptr;
    QLabel *m_lblFt8TxReport = nullptr;
    QGroupBox *m_grpFt8DecodePerformance = nullptr;
    QLabel *m_lblFt8DecodePerformance = nullptr;
    QsoMapWidget *m_ftQsoMapWidget = nullptr;
    QVector<QsoMapWidget *> m_qsoMapWidgets;
    Ft8RxDecoder::PerfStats m_lastFt8PerfStats;
    bool m_haveFt8PerfStats = false;
    QVector<double> m_ft8RecentLiveDecodeMs;
    QVector<QString> m_ft8RecentLiveDecodeSlots;
    QVector<double> m_ft8RecentDtSeconds;
    QVector<FtAutoTestItem> m_ftAutoTestQueue;
    QVector<QString> m_ftAutoTestReportLines;
    int m_ftAutoTestIndex = 0;
    bool m_ftAutoTestRunning = false;
    QString m_ftAutoTestPreviousDepth;
    QString m_ftAutoTestPreviousMode;
    QString m_ftAutoTestStartedUtc;
    int m_ftAutoTestStepUnresolvedHashCount = 0;
    int m_ftAutoTestStepVisibleAngleCallCount = 0;
    QString m_lastFt8RxTableUtc;
    bool m_ft8NtpSynced = false;
    double m_ft8NtpOffsetMs = 0.0;
    double m_ft8NtpRttMs = 0.0;
    int m_ft8NtpServers = 0;
    QString m_ft8NtpStatusText = QStringLiteral("NTP: starting");
    QDateTime m_ft8NtpLastSyncUtc;
    QGroupBox *m_grpWaterfallDisplay = nullptr;
    QSlider *m_sliderWaterfallScale = nullptr;
    QLabel *m_lblWaterfallScale = nullptr;
    QComboBox *m_cmbWaterfallPalette = nullptr;
    QGroupBox *m_grpRigStatus = nullptr;
    QLabel *m_lblRigCatStatus = nullptr;
    QLabel *m_lblRigFrequency = nullptr;
    QLabel *m_lblRigPttStatus = nullptr;
    FtSlotScheduler *m_ftSlotScheduler = nullptr;
    QThread *m_ftSlotThread = nullptr;
    bool m_ft8PendingTxArmed = false;
    QString m_ft8PendingTxToken;
    QString m_pendingFt8TxMessage;
    QString m_pendingFt8TxTag;
    bool m_pendingFt8Tune = false;
    int m_pendingFt8PreSilenceMs = 0;
    qint64 m_pendingFt8SlotBoundaryUtcMs = 0;
    int m_pendingFt8AudioTargetDelayMs = 0;
    int m_pendingFt8PttLeadMs = 0;
    bool m_pendingFt8PttPrearmed = false;
    bool m_pendingFt8PttKeyed = false;
    std::unique_ptr<TxModulator> m_pendingFt8PreparedModulator;
    FtTxPlan m_pendingFt8TxPlan;
    FtTxPlan m_lastFt8TxPlan;
    bool m_hasDeferredFt8TxPlan = false;
    QString m_deferredFt8TxMessage;
    QString m_deferredFt8TxTag;
    FtTxPlan m_deferredFt8TxPlan;
    FtQsoSession m_ftSession;
    QString m_lastCompletedFt8Call;
    QDateTime m_lastCompletedFt8Utc;
    QString m_lastCompletedFt8Reason;
    struct Ft8AudioActivity
    {
        int frequencyHz = 0;
        qint64 slotStartUtcMs = 0;
        QDateTime heardUtc;
        QString call;
    };
    QVector<Ft8AudioActivity> m_ft8RecentAudioActivity;
    FtStandardMessageSet m_ftStandardMessages;
    double m_lastRigFrequencyHz = 0.0;
    bool m_lastRigPttOn = false;
    mm::CatRotatorController *m_catRotatorController = nullptr;
    QWidget *m_tabCatRotator = nullptr;
    mm::CatRotatorPanel *m_catRotatorSidePanel = nullptr;
    QLabel *m_lblRotatorMovingLamp = nullptr;
    QLabel *m_lblRotatorReadyLamp = nullptr;

    struct Ft8WaterfallCallout
    {
        int frequencyHz = 0;
        QString label;
        QColor color;
        QDateTime expiresUtc;
        bool directReplyToMe = false;
        bool blacklisted = false;
    };
    QVector<Ft8WaterfallCallout> m_ft8WaterfallCallouts;
    QVector<RttyMultiDecoder::Callout> m_rttyWaterfallCallouts;

    QImage m_txSourceImage;
    QImage m_txPreparedImage;
    QString m_txImageFileName;
    QString m_txImageOwnerMode;
    bool m_returnToRxAfterTx = false;
    bool m_txFinishedNaturally = false;
    bool m_currentTxIsTextMode = false;
    bool m_preserveTextTerminalOnNextRx = false;
    QPlainTextEdit *m_activeTextTxEditor = nullptr;
    int m_activeTextTxLength = 0;
    int m_textTxHighlightedChars = -1;

    bool m_rxRunning = false;
    bool m_txRunning = false;
    bool m_offlineAnalysisActive = false;
    int m_textAfcSamplesSinceUpdate = 0;

    QString m_pendingModeName;
    bool m_pendingModeRestartRx = false;

    QString m_lastRttyDecodedText;
    QString m_lastBpsk31DecodedText;
    bool m_rttyPendingRxLineBreak = false;
    bool m_bpsk31PendingRxLineBreak = false;
    bool m_mfskPendingRxLineBreak = false;

    double m_rttyTxScopePhase = 0.0;
    QVector<QPointF> m_rttyTxScopeTrace;

    QSerialPort m_pttSerial;
    QTimer m_pttTestTimer;
};

#endif // MAINWINDOW_H
