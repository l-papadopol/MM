#include "DdspPanelWidget.h"

#include "../utils/RuntimeI18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
QString T(const QString &s) { return MadModemI18n::text(s); }
}

class NeuralMatrixWidget : public QWidget
{
public:
    explicit NeuralMatrixWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(92);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(T(QStringLiteral("Live MIND neural activity. The displayed layer geometry follows the selected MIND profile.")));
    }

    void setProfile(const QString &profileName, const QVector<int> &layers)
    {
        m_profileName = profileName;
        m_layers = layers;
        update();
    }

    void setActivity(const QVector<float> &activity)
    {
        m_activity = activity;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(18, 22, 26));
        p.setPen(QColor(80, 90, 100));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        if (m_layers.isEmpty()) {
            p.setPen(QColor(190, 195, 200));
            p.drawText(rect(), Qt::AlignCenter, T(QStringLiteral("No MIND profile selected")));
            return;
        }

        const int cell = 3;
        const int gap = 1;
        const int stride = cell + gap;
        const int left = 6;
        const int right = 6;
        const int top = 18;
        const int bottom = 8;
        const int layerCount = m_layers.size();
        const int totalGap = 8 * (layerCount - 1);
        const int availW = qMax(1, width() - left - right - totalGap);
        const int layerW = qMax(18, availW / layerCount);
        const int availH = qMax(1, height() - top - bottom);

        int activityOffset = 0;
        for (int l = 0; l < layerCount; ++l) {
            const int neurons = qMax(1, m_layers[l]);
            const int x0 = left + l * (layerW + 8);
            const int cols = qMax(1, layerW / stride);
            const int rows = qMax(1, availH / stride);
            const int drawable = qMax(1, cols * rows);
            const int cellsToDraw = qMin(neurons, drawable);

            p.setPen(QColor(160, 166, 172));
            const QString layerText = QString::number(neurons);
            p.drawText(QRect(x0, 2, layerW, 14), Qt::AlignCenter, layerText);

            for (int i = 0; i < cellsToDraw; ++i) {
                const int srcIdx = activityOffset + static_cast<int>((static_cast<qint64>(i) * neurons) / cellsToDraw);
                float v = -1.0f;
                if (srcIdx >= 0 && srcIdx < m_activity.size()) {
                    v = qBound(0.0f, m_activity[srcIdx], 1.0f);
                }
                const int x = x0 + (i % cols) * stride;
                const int y = top + (i / cols) * stride;
                QColor c;
                if (v < 0.0f) {
                    c = QColor(42, 48, 54);
                } else if (v < 0.33f) {
                    c = QColor(20, 70 + static_cast<int>(v * 180.0f), 170);
                } else if (v < 0.66f) {
                    c = QColor(20 + static_cast<int>((v - 0.33f) * 300.0f), 180, 80);
                } else {
                    c = QColor(220, 130 + static_cast<int>((v - 0.66f) * 250.0f), 40);
                }
                p.fillRect(QRect(x, y, cell, cell), c);
            }
            activityOffset += neurons;
        }

        if (m_activity.isEmpty()) {
            p.setPen(QColor(190, 195, 200));
            p.drawText(rect().adjusted(0, 14, 0, 0), Qt::AlignCenter,
                       T(QStringLiteral("Waiting for profile activity")));
        }
    }

private:
    QString m_profileName;
    QVector<int> m_layers;
    QVector<float> m_activity;
};

DdspPanelWidget::DdspPanelWidget(DeepDspController *controller, QWidget *parent)
    : QWidget(parent),
      m_controller(controller)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(6);

    auto *grpStatus = new QGroupBox(this);
    auto *statusLayout = new QGridLayout(grpStatus);
    statusLayout->setContentsMargins(8, 8, 8, 8);
    statusLayout->setHorizontalSpacing(8);
    statusLayout->setVerticalSpacing(5);

    m_lblState = new QLabel(T(QStringLiteral("Shadow training")), grpStatus);
    QFont stateFont = m_lblState->font();
    stateFont.setBold(true);
    m_lblState->setFont(stateFont);

    m_lblSamples = new QLabel(T(QStringLiteral("Samples: 0")), grpStatus);
    m_lblSamples->setToolTip(T(QStringLiteral("Gold samples collected for MIND. FT native samples come from CRC-valid FT candidate matrices.")));
    m_lblBreakdown = new QLabel(QStringLiteral("FT 0 · RTTY 0 · CW 0"), grpStatus);
    m_lblArchitecture = new QLabel(QStringLiteral("Net --"), grpStatus);
    m_lblBackend = new QLabel(T(QStringLiteral("Backend: CPU Eigen")), grpStatus);
    m_lblTrainingCompletion = new QLabel(T(QStringLiteral("Bit: -- · Best: --")), grpStatus);
    m_lblLoss = new QLabel(QStringLiteral("Replay 0 · Loss --"), grpStatus);

    m_progressAccuracy = new QProgressBar(grpStatus);
    m_progressAccuracy->setRange(0, 100);
    m_progressAccuracy->setValue(0);
    m_progressAccuracy->setFormat(T(QStringLiteral("MIND %p%")));
    m_progressAccuracy->setTextVisible(true);

    statusLayout->addWidget(m_lblState, 0, 0, 1, 2);
    statusLayout->addWidget(m_progressAccuracy, 1, 0, 1, 2);
    statusLayout->addWidget(m_lblSamples, 2, 0);
    statusLayout->addWidget(m_lblTrainingCompletion, 2, 1);
    statusLayout->addWidget(m_lblBreakdown, 3, 0, 1, 2);
    statusLayout->addWidget(m_lblArchitecture, 4, 0, 1, 2);
    statusLayout->addWidget(m_lblBackend, 5, 0, 1, 2);
    statusLayout->addWidget(m_lblLoss, 6, 0, 1, 2);

    auto *grpMatrix = new QGroupBox(this);
    auto *matrixLayout = new QVBoxLayout(grpMatrix);
    matrixLayout->setContentsMargins(8, 8, 8, 8);
    matrixLayout->setSpacing(4);
    auto *profileRow = new QHBoxLayout();
    profileRow->setSpacing(4);
    auto *profileLabel = new QLabel(T(QStringLiteral("Profile view")), grpMatrix);
    m_cmbProfileView = new QComboBox(grpMatrix);
    m_cmbProfileView->addItem(T(QStringLiteral("Auto")), QStringLiteral("AUTO"));
    m_cmbProfileView->addItem(QStringLiteral("FT8"), QStringLiteral("FT8"));
    m_cmbProfileView->addItem(QStringLiteral("FT4"), QStringLiteral("FT4"));
    m_cmbProfileView->addItem(QStringLiteral("CW"), QStringLiteral("CW"));
    m_cmbProfileView->addItem(QStringLiteral("RTTY"), QStringLiteral("RTTY"));
    m_cmbProfileView->setToolTip(T(QStringLiteral("Auto follows the current mode; manual view lets you inspect each dedicated MIND profile.")));
    profileRow->addWidget(profileLabel);
    profileRow->addWidget(m_cmbProfileView, 1);
    m_matrixWidget = new NeuralMatrixWidget(grpMatrix);
    matrixLayout->addLayout(profileRow);
    matrixLayout->addWidget(m_matrixWidget);

    auto *grpControl = new QGroupBox(this);
    auto *controlLayout = new QGridLayout(grpControl);
    controlLayout->setContentsMargins(8, 8, 8, 8);
    controlLayout->setHorizontalSpacing(6);
    controlLayout->setVerticalSpacing(4);

    m_chkTraining = new QCheckBox(T(QStringLiteral("Shadow training")), grpControl);
    m_chkTraining->setChecked(true);
    m_chkTraining->setToolTip(T(QStringLiteral("MIND learns from native FT candidate matrices after LDPC/CRC validation. It does not key TX or AutoQSO.")));
    m_chkAssist = new QCheckBox(T(QStringLiteral("Assist")), grpControl);
    m_chkAssist->setEnabled(false);
    m_chkAssist->setToolTip(T(QStringLiteral("Available only after native validation is perfect. Assisted decodes remain guarded by deterministic checks.")));

    auto *budgetLabel = new QLabel(T(QStringLiteral("Trainer budget")), grpControl);
    m_spinBudget = new QSpinBox(grpControl);
    m_spinBudget->setRange(0, 250);
    m_spinBudget->setValue(50);
    m_spinBudget->setSuffix(QStringLiteral(" ms"));
    m_spinBudget->setToolTip(T(QStringLiteral("Maximum dedicated trainer-thread work per cycle. Higher values use more CPU for offline learning; decoder, audio, CAT and UI threads are not used for MIND training.")));

    m_btnReset = new QPushButton(T(QStringLiteral("Reset")), grpControl);
    m_btnSave = new QPushButton(T(QStringLiteral("Save")), grpControl);
    m_btnLoad = new QPushButton(T(QStringLiteral("Load")), grpControl);

    controlLayout->addWidget(m_chkTraining, 0, 0, 1, 2);
    controlLayout->addWidget(m_chkAssist, 0, 2);
    controlLayout->addWidget(budgetLabel, 1, 0);
    controlLayout->addWidget(m_spinBudget, 1, 1, 1, 2);
    controlLayout->addWidget(m_btnSave, 2, 0);
    controlLayout->addWidget(m_btnLoad, 2, 1);
    controlLayout->addWidget(m_btnReset, 2, 2);

    auto *grpModel = new QGroupBox(this);
    auto *modelLayout = new QVBoxLayout(grpModel);
    modelLayout->setContentsMargins(8, 8, 8, 8);
    m_lblCheckpoint = new QLabel(T(QStringLiteral("Model: not loaded")), grpModel);
    m_lblCheckpoint->setWordWrap(false);
    m_lblLastCheckpoint = new QLabel(T(QStringLiteral("Checkpoint: never")), grpModel);
    m_lblLastCheckpoint->setWordWrap(false);
    modelLayout->addWidget(m_lblCheckpoint);
    modelLayout->addWidget(m_lblLastCheckpoint);

    // Manual CW/RTTY teaching and synthetic bootcamp controls are intentionally
    // hidden from the production MIND panel. FT labels are collected from
    // native CRC-valid candidate matrices; CW/RTTY profile plumbing remains
    // internal for later shadow-assist work.


    outer->addWidget(grpStatus);
    outer->addWidget(grpMatrix);
    outer->addWidget(grpControl);
    outer->addWidget(grpModel);
    outer->addStretch(1);

    if (m_controller != nullptr) {
        connect(m_chkTraining, &QCheckBox::toggled,
                m_controller, &DeepDspController::setEnabled);
        connect(m_chkAssist, &QCheckBox::toggled,
                m_controller, &DeepDspController::setAssistEnabled);
        connect(m_spinBudget, QOverload<int>::of(&QSpinBox::valueChanged),
                m_controller, &DeepDspController::setTrainingBudgetMs);
        connect(m_btnReset, &QPushButton::clicked,
                m_controller, &DeepDspController::resetModel);
        connect(m_btnSave, &QPushButton::clicked,
                m_controller, &DeepDspController::saveCheckpoint);
        connect(m_btnLoad, &QPushButton::clicked,
                m_controller, &DeepDspController::loadCheckpoint);
        connect(m_cmbProfileView, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DdspPanelWidget::profileSelectionChanged);
        connect(m_controller, &DeepDspController::statusChanged,
                this, &DdspPanelWidget::updateStatus);
        updateStatus(m_controller->status());
    }
}


void DdspPanelWidget::runCwBootcamp()
{
    if (m_controller != nullptr) {
        m_controller->runCwBootcamp();
        if (m_cmbProfileView != nullptr) {
            const int idx = m_cmbProfileView->findData(QStringLiteral("CW"));
            if (idx >= 0) m_cmbProfileView->setCurrentIndex(idx);
        }
    }
}


void DdspPanelWidget::teachManualLabel()
{
    if (m_controller == nullptr || m_cmbManualMode == nullptr || m_editManualLabel == nullptr) {
        return;
    }
    const QString mode = m_cmbManualMode->currentData().toString().trimmed();
    const QString label = m_editManualLabel->text().trimmed();
    if (mode.isEmpty() || label.isEmpty()) {
        return;
    }
    m_controller->submitConfirmedText(mode, label);
    m_editManualLabel->clear();
}


void DdspPanelWidget::profileSelectionChanged(int)
{
    if (m_controller != nullptr) {
        updateStatus(m_controller->status());
    }
}

QString DdspPanelWidget::effectiveProfile(const DeepDspController::Status &status) const
{
    QString profile = QStringLiteral("AUTO");
    if (m_cmbProfileView != nullptr) {
        profile = m_cmbProfileView->currentData().toString().trimmed().toUpper();
    }
    if (profile == QStringLiteral("AUTO") || profile.isEmpty()) {
        profile = status.activeProfile.trimmed().toUpper();
    }
    if (profile != QStringLiteral("FT8") && profile != QStringLiteral("FT4") &&
        profile != QStringLiteral("CW") && profile != QStringLiteral("RTTY")) {
        profile = QStringLiteral("FT8");
    }
    return profile;
}

void DdspPanelWidget::applyProfileToMatrix(const QString &profile, const DeepDspController::Status &status)
{
    if (m_matrixWidget == nullptr) return;

    QVector<int> layers;
    QVector<float> activity;
    if (profile == QStringLiteral("CW")) {
        layers = QVector<int>{256, 96, 48, 6};
        if (status.cwActivity.size() == 256 + 96 + 48 + 6) {
            activity = status.cwActivity;
        }
    } else if (profile == QStringLiteral("RTTY")) {
        layers = QVector<int>{96, 64, 32, 8};
        if (status.rttyActivity.size() == 96 + 64 + 32 + 8) {
            activity = status.rttyActivity;
        }
    } else {
        layers = QVector<int>{464, 128, 64, 174};
        if (status.ftActivity.size() == 464 + 128 + 64 + 174) {
            activity = status.ftActivity;
        } else if (status.neuralActivity.size() == 464 + 128 + 64 + 174) {
            activity = status.neuralActivity;
        }
    }
    m_matrixWidget->setProfile(profile, layers);
    m_matrixWidget->setActivity(activity);
}

void DdspPanelWidget::updateStatus(const DeepDspController::Status &status)
{
    if (m_lblState != nullptr) {
        m_lblState->setText(T(status.stateText));
    }
    const QString profile = effectiveProfile(status);
    if (m_lblSamples != nullptr) {
        int shownSamples = status.sampleCount;
        if (profile == QStringLiteral("FT8")) shownSamples = status.ft8Samples;
        else if (profile == QStringLiteral("FT4")) shownSamples = status.ft4Samples;
        else if (profile == QStringLiteral("CW")) shownSamples = status.cwSamples;
        else if (profile == QStringLiteral("RTTY")) shownSamples = status.rttySamples;
        m_lblSamples->setText(T(QStringLiteral("Samples")) + QStringLiteral(" %1: %2").arg(profile).arg(shownSamples));
    }
    if (m_lblBreakdown != nullptr) {
        m_lblBreakdown->setText(QStringLiteral("FT8 %1 · FT4 %2 · RTTY %3 · CW %4")
                                    .arg(status.ft8Samples)
                                    .arg(status.ft4Samples)
                                    .arg(status.rttySamples)
                                    .arg(status.cwSamples));
    }
    if (m_lblLoss != nullptr) {
        if (profile == QStringLiteral("CW")) {
            m_lblLoss->setText(QStringLiteral("CW train %1 · Loss %2")
                                   .arg(status.trainingRuns)
                                   .arg(status.lastLoss, 0, 'f', 5));
        } else {
            m_lblLoss->setText(QStringLiteral("Replay %1 · Batch %2 · %3 samp/s · Loss %4")
                                   .arg(status.replayBufferSamples)
                                   .arg(status.ftBatchSize)
                                   .arg(status.trainSamplesPerSecond, 0, 'f', 0)
                                   .arg(status.lastLoss, 0, 'f', 5));
        }
    }
    if (m_lblArchitecture != nullptr) {
        QString arch = status.architectureText;
        if (profile == QStringLiteral("CW")) arch = QStringLiteral("256 → 96 → 48 → 6");
        else if (profile == QStringLiteral("RTTY")) arch = QStringLiteral("96 → 64 → 32 → 8");
        else if (profile == QStringLiteral("FT4")) arch = QStringLiteral("464 → 128 → 64 → 174");
        else if (profile == QStringLiteral("FT8")) arch = QStringLiteral("464 → 128 → 64 → 174");
        m_lblArchitecture->setText(QStringLiteral("Net ") + profile + QStringLiteral(" ") + arch);
    }
    if (m_lblBackend != nullptr) {
        m_lblBackend->setText(T(QStringLiteral("Backend")) + QStringLiteral(": ") + status.backendText);
        m_lblBackend->setToolTip(T(QStringLiteral("MIND uses Eigen/OpenMP batched matrix-matrix training on the dedicated trainer thread. Batch and samples/second show whether CPU parallel training is active.")));
    }
    constexpr int kUiMinValidation = 200;
    const bool warmup = status.validationCount < kUiMinValidation;
    if (m_lblTrainingCompletion != nullptr) {
        if (profile == QStringLiteral("CW")) {
            m_lblTrainingCompletion->setText(T(QStringLiteral("Symbol")) + QStringLiteral(": ") +
                                             QString::number(status.cwAccuracy, 'f', 1) + QStringLiteral(" %"));
        } else {
            const QString msgPart = warmup
                ? QStringLiteral("--")
                : QString::number(status.messageAccuracy, 'f', 2) + QStringLiteral(" %");
            m_lblTrainingCompletion->setText(T(QStringLiteral("Bit")) + QStringLiteral(": ") +
                                             QString::number(status.bitAccuracy, 'f', 1) + QStringLiteral(" % · ") +
                                             T(QStringLiteral("Best Bit")) + QStringLiteral(": ") +
                                             QString::number(status.bestBitAccuracy, 'f', 1) + QStringLiteral(" % · ") +
                                             T(QStringLiteral("Msg")) + QStringLiteral(": ") + msgPart);
        }
    }
    if (m_progressAccuracy != nullptr) {
        m_progressAccuracy->setValue(qBound(0, static_cast<int>(status.trainingCompletionPercent + 0.5), 100));
        if (profile == QStringLiteral("CW")) {
            m_progressAccuracy->setFormat(T(QStringLiteral("CW %p%")));
            m_progressAccuracy->setToolTip(T(QStringLiteral("CW training progress from the dedicated synthetic dit/dah/gap profile.")));
        } else {
            m_progressAccuracy->setFormat(T(QStringLiteral("MIND %p%")));
            m_progressAccuracy->setToolTip(T(QStringLiteral("MIND progress uses the guarded readiness score. Bit and Best Bit show actual learning quality.")));
        }
    }
    applyProfileToMatrix(profile, status);
    if (m_lblCheckpoint != nullptr) {
        const QString modelName = status.checkpointPath.isEmpty()
                                  ? QStringLiteral("--")
                                  : QFileInfo(status.checkpointPath).fileName();
        m_lblCheckpoint->setText(T(QStringLiteral("Model")) + QStringLiteral(": ") + modelName);
        m_lblCheckpoint->setToolTip(status.checkpointPath);
    }
    if (m_lblLastCheckpoint != nullptr) {
        m_lblLastCheckpoint->setText(T(QStringLiteral("Checkpoint")) + QStringLiteral(": ") + status.lastCheckpointText);
        m_lblLastCheckpoint->setToolTip(T(QStringLiteral("Stats")) + QStringLiteral(": ") + status.statsPath +
                                        QStringLiteral("\n") + T(QStringLiteral("Gold dataset")) + QStringLiteral(": ") + status.goldDatasetPath);
    }
    if (m_chkTraining != nullptr && m_chkTraining->isChecked() != status.enabled) {
        m_chkTraining->blockSignals(true);
        m_chkTraining->setChecked(status.enabled);
        m_chkTraining->blockSignals(false);
    }
    if (m_chkAssist != nullptr) {
        m_chkAssist->setEnabled(status.ready);
        if (m_chkAssist->isChecked() != status.assistEnabled) {
            m_chkAssist->blockSignals(true);
            m_chkAssist->setChecked(status.assistEnabled);
            m_chkAssist->blockSignals(false);
        }
    }
}
