#include "../mainwindow.h"
#include "ui_mainwindow.h"
#include "../modems/cw/tx/CwSerialTransmitter.h"
#include "../dialogs/CwTxSettingsDialog.h"
#include "../utils/RuntimeI18n.h"
#include "../utils/SerialPortEndpoint.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSpinBox>
namespace {
QString uiText(const char* key, const char* fallback) { return MadModemI18n::translate(QString::fromUtf8(key),QString::fromUtf8(fallback)); }

}
void MainWindow::configureCwTransmission()
{
    if (m_txRunning || m_txPreparationPending || m_cwKeyReleaseFailed) return;
    CwTxSettingsDialog dialog(m_settings,this);
    if (dialog.exec()!=QDialog::Accepted) return;
    const auto& s=dialog.resultSettings();
    m_settings.cwTxMethod=s.cwTxMethod;m_settings.cwKeyPort=s.cwKeyPort;
    m_settings.cwKeyLine=s.cwKeyLine;m_settings.cwKeyPttLine=s.cwKeyPttLine;
    m_settings.cwKeyLeadMs=s.cwKeyLeadMs;m_settings.cwKeyTailMs=s.cwKeyTailMs;
    savePersistentSettings();updateCwTxMethodNotice();
}
void MainWindow::updateCwTxMethodNotice()
{
    if (!m_lblCwTxMethodNotice) return;
    const bool serial=m_settings.cwTxMethod=="serial_key";
    m_lblCwTxMethodNotice->setText(serial
        ? uiText("cw_serial_tx_method", "TX: serial KEY — radio in CW/BK-IN")
        : uiText("cw_audio_tx_method", "TX: audio CW over SSB/Data"));
    m_lblCwTxMethodNotice->setToolTip(serial
        ? uiText("cw_serial_tx_explanation", "Dots and dashes use the selected RTS/DTR KEY line. Put the radio in CW. The radio uses BK-IN, or the optional other serial line provides PTT. Audio/PTT CAT mode forcing is bypassed. Sharing the CAT port pauses CAT during the message.")
        : uiText("cw_audio_tx_explanation", "MM generates Morse as an audio tone through the radio's SSB/Data input. CAT uses the configured audio TX mode. Select Serial KEY in CW transmission settings to key the radio's CW input."));
}
void MainWindow::startCwSerialTransmission(const QString& text)
{
    const QString port=m_settings.cwKeyPort.trimmed();
    QString error;
    if (port.isEmpty() || port.contains("://") || (port.contains(':') && !QRegularExpression("^COM[0-9]+:?$",QRegularExpression::CaseInsensitiveOption).match(port).hasMatch()))
        error=uiText("cw_key_port_required", "Choose a local serial CW KEY port. A CAT TCP address is not a KEY port.");
    const QString catPort=m_settings.hamlibRigPath.trimmed().isEmpty() ? m_settings.hamlibSerialPath : m_settings.hamlibRigPath;
    const bool shared=(m_settings.hamlibCatEnabled || m_settings.hamlibPttEnabled) && m_settings.hamlibTcpAddress.trimmed().isEmpty() && MadModemSerial::sameEndpoint(port,catPort);
    if (shared && (m_settings.hamlibHandshake!="none" || m_settings.hamlibForceDtr!="off" || m_settings.hamlibForceRts!="off"))
        error=uiText("cw_key_shared_config", "Sharing the CAT port for CW KEY requires CAT handshaking None and both DTR and RTS forced OFF in Audio/PTT/CAT settings. Alternatively choose the radio's separate KEY port.");
    if (m_settings.rotatorEnabled && MadModemSerial::sameEndpoint(port,m_settings.rotatorPath))
        error=uiText("cw_key_rotator_conflict", "The CW KEY port is assigned to the rotator. Choose a different serial port.");
    if (!error.isEmpty()) { appendLog(error);QMessageBox::warning(this,QStringLiteral("CW KEY"),error);return; }
    if (!m_cwSerialTx || m_cwSerialTx->isRunning()) return;
    const bool hadRx=m_rxStartPending || m_rxRunning || (m_audioEngine && m_audioEngine->isRunning());
    m_cwSerialActive=true;m_cwSerialFinishing=false;m_cwCatPortSuspended=false;
    m_cwSerialGeneration=m_txRequestGeneration;
    m_txRunning=true;m_currentTxIsTextMode=true;m_returnToRxAfterTx=true;m_txFinishedNaturally=false;
    if(hadRx){m_fastResumeCwRttyRxPending=true;m_fastResumeCwRttyMode=CwDecoder::modeName();}
    requestAudioInputStop();setReceiverRunning(false);pauseQsoSignalPeakForTransmit();
    m_progressTx->setValue(0);updateTxControlState();
    CwSerialConfig config;
    config.port=MadModemSerial::endpoint(port);config.keyLine=m_settings.cwKeyLine;config.pttLine=m_settings.cwKeyPttLine;
    config.leadMs=m_settings.cwKeyLeadMs;config.tailMs=m_settings.cwKeyTailMs;
    const double wpm=m_spinCwTxWpm ? m_spinCwTxWpm->value() : m_settings.cwTxWpm;
    const auto generation=m_txRequestGeneration;
    auto begin=[this,config,text,wpm,generation](bool ok) {
        if(m_shutdownInProgress || m_runtimeShutdownComplete) return;
        if(!ok || generation!=m_txRequestGeneration) {
            finishCwSerialTransmission(false,generation==m_txRequestGeneration ? uiText("cw_key_handoff_failed", "CAT could not release the CW KEY port; transmission cancelled.") : QString());return;
        }
        appendLog(QStringLiteral("CW KEY: %1 on %2, %3 WPM; radio must be in CW.").arg(config.keyLine.toUpper(),config.port).arg(wpm));
        if(!m_cwSerialTx->startMessage(text,wpm,config)) finishCwSerialTransmission(false,QStringLiteral("CW KEY worker is busy."));
    };
    if(shared) {
        auto *rig=m_rigController;
        // Reserve reconnect authorization before the asynchronous handoff. Its
        // rollback restores CAT if Stop/timeout revokes this TX request.
        m_cwCatPortSuspended=true;
        m_catCommand->request(rig,[rig,port]{return rig->suspendSerialPortForCw(port);},
            [rig]{return rig->resumeSerialPortAfterCw();},begin);
    } else begin(true);
}
void MainWindow::finishCwSerialTransmission(bool natural,const QString& error)
{
    if(!m_cwSerialActive || m_cwSerialFinishing || m_shutdownInProgress) return;
    m_cwSerialFinishing=true;
    if(!error.isEmpty()) appendLog(QStringLiteral("CW KEY: ")+error);
    auto done=[this,natural](bool restored) {
        if(m_shutdownInProgress || m_runtimeShutdownComplete) return;
        if(!restored) appendLog(uiText("cw_key_cat_restore_failed", "CW KEY has stopped, but CAT reconnect failed. Check the CAT connection."));
        if (!m_cwKeyReleaseFailed) m_cwCatPortSuspended=false;
        else {
            m_returnToRxAfterTx=false;
            appendLog(uiText("cw_key_release_failed", "CW KEY/PTT release was not confirmed. TX is blocked. Reconnect the interface and press Stop to retry releasing the lines."));
        }
        m_txFinishedNaturally=natural && m_cwSerialGeneration==m_txRequestGeneration;
        // Keep m_cwSerialActive true through this common stop handler so it
        // cannot send the audio-CW CAT/PTT route or change the radio to USB-D.
        handleTxStopped();
        m_cwSerialActive=false;m_cwSerialFinishing=false;
        if (!m_cwKeyReleaseFailed) resumeQsoSignalPeakAfterTransmit();
        updateTxControlState();
    };
    if(m_cwCatPortSuspended && !m_cwKeyReleaseFailed) {
        auto *rig=m_rigController;
        // A timed-out handoff is still compensating on the CAT thread. Its
        // completion owns port recovery; never launch an overlapping request.
        if(m_catCommand->busy()) {
            auto connection=std::make_shared<QMetaObject::Connection>();
            *connection=connect(m_catCommand,&AsyncCatCommand::settled,this,[this,connection,natural,error]{
                disconnect(*connection);m_cwSerialFinishing=false;finishCwSerialTransmission(natural,error);
            });return;
        }
        m_catCommand->request(rig,[rig]{return rig->resumeSerialPortAfterCw();},
            [rig]{return rig->disconnectRig();},done,3000,true);
    } else done(true);
}
