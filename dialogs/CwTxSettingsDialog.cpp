#include "CwTxSettingsDialog.h"
#include "../utils/RuntimeI18n.h"
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QSignalBlocker>
namespace {
QString uiText(const char* key, const char* fallback) { return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback)); }
QString selectedPort(QComboBox* box) {
    return box->currentIndex() >= 0 && box->currentText() == box->itemText(box->currentIndex())
        ? box->currentData().toString() : box->currentText().trimmed();
}
}
CwTxSettingsDialog::CwTxSettingsDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent), m_result(settings) {
    setWindowTitle(uiText("cw_tx_settings", "CW transmission"));
    resize(610, 480);
    auto *outer = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *method = new QComboBox(this); method->setObjectName("cwTxMethod");
    method->addItem(uiText("cw_tx_audio_option", "Audio tone over SSB/Data"), "audio");
    method->addItem(uiText("cw_tx_serial_option", "Serial KEY (RTS/DTR)"), "serial_key");
    method->setCurrentIndex(settings.cwTxMethod == "serial_key" ? 1 : 0);
    form->addRow(uiText("cw_tx_method", "Transmit CW using"), method);
    auto *portRow = new QWidget(this);auto *portLayout = new QHBoxLayout(portRow);portLayout->setContentsMargins(0,0,0,0);
    auto *port = new QComboBox(portRow);port->setObjectName("cwKeyPort");port->setEditable(true);port->setInsertPolicy(QComboBox::NoInsert);
    port->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);port->setMinimumContentsLength(14);
    auto *refresh = new QPushButton(uiText("button.refresh", "Refresh"), portRow);portLayout->addWidget(port,1);portLayout->addWidget(refresh);
    auto refreshPorts = [port] {
        const QString previous = selectedPort(port); QSignalBlocker blocker(port);port->clear();
        for (const auto& info : QSerialPortInfo::availablePorts()) {
            port->addItem(info.portName(), info.systemLocation());
            port->setItemData(port->count()-1,info.description(),Qt::ToolTipRole);
        }
        const int found = port->findData(previous);
        if (found>=0) port->setCurrentIndex(found);else port->setEditText(previous);
    };
    refreshPorts();
    const int initialPort = port->findData(settings.cwKeyPort);
    if (initialPort>=0) port->setCurrentIndex(initialPort); else port->setEditText(settings.cwKeyPort);
    connect(refresh,&QPushButton::clicked,this,refreshPorts);
    form->addRow(uiText("cw_key_port", "CW KEY serial port"),portRow);
    auto *key = new QComboBox(this);key->setObjectName("cwKeyLine");key->addItem("RTS","rts");key->addItem("DTR","dtr");key->setCurrentIndex(settings.cwKeyLine == "dtr" ? 1 : 0);
    form->addRow(uiText("cw_key_line", "KEY line (dots/dashes)"),key);
    auto *ptt = new QComboBox(this);ptt->setObjectName("cwKeyPttLine");
    ptt->addItem(uiText("cw_key_bkin", "None — radio BK-IN"),"none");ptt->addItem("RTS","rts");ptt->addItem("DTR","dtr");ptt->setCurrentIndex(qMax(0,ptt->findData(settings.cwKeyPttLine)));
    form->addRow(uiText("cw_key_ptt", "PTT on this port"),ptt);
    auto *lead = new QSpinBox(this);lead->setRange(0,1000);lead->setSuffix(" ms");lead->setValue(settings.cwKeyLeadMs);
    auto *tail = new QSpinBox(this);tail->setRange(0,2000);tail->setSuffix(" ms");tail->setValue(settings.cwKeyTailMs);
    form->addRow(uiText("cw_key_lead", "PTT lead"),lead);form->addRow(uiText("cw_key_tail", "PTT tail"),tail);outer->addLayout(form);
    auto *help = new QLabel(uiText("cw_key_help", "Serial KEY requires the radio in CW and an interface wired to its KEY input. Select the actual virtual COM port and its assigned line; the COM number does not define its function. With no PTT line, enable BK-IN on the radio. Audio CW uses SSB/Data and the usual Audio/PTT settings."),this);help->setWordWrap(true);outer->addWidget(help);
    auto *sharing = new QLabel(uiText("cw_key_cat_share", "If KEY shares the CAT serial port, MM temporarily closes CAT, keys the message, then reconnects CAT after releasing the lines. CAT must use no handshaking and force both controlled lines OFF. A separate KEY port keeps CAT available during TX. The USB/OS path is software timed; this is not a hardware WinKeyer or a guarantee of full QSK."),this);sharing->setWordWrap(true);outer->addWidget(sharing);
    outer->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);outer->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText(uiText("cw_save_tx_settings", "Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText("text.cancel", "Cancel"));
    auto update = [=] {
        const bool serial = method->currentData().toString()=="serial_key";
        portRow->setEnabled(serial);key->setEnabled(serial);ptt->setEnabled(serial);sharing->setVisible(serial);
        const bool hasPtt = serial && ptt->currentData().toString()!="none";lead->setEnabled(hasPtt);tail->setEnabled(hasPtt);
    };
    connect(method,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[update](int){update();});
    connect(ptt,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[update](int){update();});
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,this,[=] {
        if (method->currentData().toString()=="serial_key" && (selectedPort(port).isEmpty() || key->currentData()==ptt->currentData())) {
            QMessageBox::warning(this,windowTitle(),uiText("cw_key_invalid", "Select a serial KEY port and use different lines for KEY and PTT."));return;
        }
        m_result.cwTxMethod=method->currentData().toString();m_result.cwKeyPort=selectedPort(port);
        m_result.cwKeyLine=key->currentData().toString();m_result.cwKeyPttLine=ptt->currentData().toString();
        m_result.cwKeyLeadMs=lead->value();m_result.cwKeyTailMs=tail->value();accept();
    });
    update();
}
