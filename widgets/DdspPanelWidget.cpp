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

    void setIdleText(const QString &text)
    {
        m_idleText = text;
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
                       m_idleText.isEmpty() ? T(QStringLiteral("Waiting for profile activity")) : m_idleText);
        }
    }

private:
    QString m_profileName;
    QString m_idleText;
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

    m_lblState = new QLabel(T(QStringLiteral("Autonomous training")), grpStatus);
    QFont stateFont = m_lblState->font();
    stateFont.setBold(true);
    m_lblState->setFont(stateFont);

    m_lblSamples = new QLabel(T(QStringLiteral("Samples: 0")), grpStatus);
    m_lblSamples->setToolTip(T(QStringLiteral("MIND ranker samples. FT native samples include CRC-valid positives and LDPC/CRC failed negatives for candidate pruning.")));
    m_lblModeMeaning = new QLabel(T(QStringLiteral("Off: MIND is bypassed. Training: it learns but does not touch decoding. Active: it may assist low-level decisions; final text remains classical.")), grpStatus);
    m_lblModeMeaning->setWordWrap(true);
    m_lblModeMeaning->setStyleSheet(QStringLiteral("color: palette(mid);"));
    m_lblProfileMeaning = new QLabel(T(QStringLiteral("Current profile: FT ranks candidates, CW heavily assists human-fist timing, RTTY assists mark/space slicing.")), grpStatus);
    m_lblProfileMeaning->setWordWrap(true);
    m_lblBreakdown = new QLabel(QStringLiteral("FT 0 · RTTY 0 · CW 0"), grpStatus);
    m_lblArchitecture = new QLabel(QStringLiteral("Net --"), grpStatus);
    m_lblBackend = new QLabel(T(QStringLiteral("Backend: CPU Eigen")), grpStatus);
    m_lblModelState = new QLabel(T(QStringLiteral("Model")) + QStringLiteral(": --"), grpStatus);
    m_lblTrainingCompletion = new QLabel(T(QStringLiteral("Ranker: -- · Best: --")), grpStatus);
    m_lblLoss = new QLabel(QStringLiteral("Replay 0 · Loss --"), grpStatus);

    for (QLabel *lbl : {m_lblState, m_lblSamples, m_lblModeMeaning, m_lblProfileMeaning,
                        m_lblBreakdown, m_lblArchitecture,
                        m_lblBackend, m_lblModelState, m_lblTrainingCompletion, m_lblLoss}) {
        lbl->setWordWrap(true);
        lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    m_progressAccuracy = new QProgressBar(grpStatus);
    m_progressAccuracy->setRange(0, 100);
    m_progressAccuracy->setValue(0);
    m_progressAccuracy->setFormat(T(QStringLiteral("MIND %p%")));
    m_progressAccuracy->setTextVisible(true);

    statusLayout->addWidget(m_lblState, 0, 0);
    statusLayout->addWidget(m_progressAccuracy, 1, 0);
    statusLayout->addWidget(m_lblModeMeaning, 2, 0);
    statusLayout->addWidget(m_lblSamples, 3, 0);
    statusLayout->addWidget(m_lblTrainingCompletion, 4, 0);
    statusLayout->addWidget(m_lblProfileMeaning, 5, 0);
    statusLayout->addWidget(m_lblBreakdown, 6, 0);
    statusLayout->addWidget(m_lblArchitecture, 7, 0);
    statusLayout->addWidget(m_lblBackend, 8, 0);
    statusLayout->addWidget(m_lblModelState, 9, 0);
    statusLayout->addWidget(m_lblLoss, 10, 0);
    m_lblArchitecture->setVisible(false);
    m_lblBackend->setVisible(false);
    m_lblModelState->setVisible(false);
    m_lblLoss->setVisible(false);

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

    auto *assistLabel = new QLabel(T(QStringLiteral("MIND Assist")), grpControl);
    m_cmbAssistMode = new QComboBox(grpControl);
    m_cmbAssistMode->addItem(T(QStringLiteral("Off")), QStringLiteral("off"));
    m_cmbAssistMode->addItem(T(QStringLiteral("Training")), QStringLiteral("shadow"));
    m_cmbAssistMode->addItem(T(QStringLiteral("Active")), QStringLiteral("assisted"));
    m_cmbAssistMode->setToolTip(T(QStringLiteral("Off: native decoders only. Training: MIND learns and scores without changing decoding. Active: MIND may rank/prioritize FT candidates and run mode-specific helpers, but final text remains produced by the classical decoders.")));

    auto *autonomyLabel = new QLabel(T(QStringLiteral("Training is automatic. Off = no overhead. Training = safe learning. Active = assisted decoding without neural text generation.")), grpControl);
    autonomyLabel->setWordWrap(true);
    autonomyLabel->setToolTip(T(QStringLiteral("No manual training budget, save, load or reset controls are exposed. MIND checkpoints and replay data are managed automatically in the background.")));

    controlLayout->addWidget(assistLabel, 0, 0);
    controlLayout->addWidget(m_cmbAssistMode, 0, 1, 1, 2);
    controlLayout->addWidget(autonomyLabel, 1, 0, 1, 3);

    // Manual CW/RTTY teaching and synthetic bootcamp controls are intentionally
    // hidden from the production MIND panel. FT labels are collected from
    // native CRC-valid candidate matrices; CW/RTTY profile plumbing remains
    // internal for later training/active assist work.


    outer->addWidget(grpStatus);
    outer->addWidget(grpMatrix);
    outer->addWidget(grpControl);
    outer->addStretch(1);

    if (m_controller != nullptr) {
        connect(m_cmbAssistMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    if (m_controller != nullptr && m_cmbAssistMode != nullptr) {
                        m_controller->setAssistMode(m_cmbAssistMode->currentData().toString());
                    }
                });
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
        layers = QVector<int>{464, 8, 24, 1};
        if (status.ftActivity.size() == 464 + 8 + 24 + 1) {
            activity = status.ftActivity;
        } else if (status.neuralActivity.size() == 464 + 8 + 24 + 1) {
            activity = status.neuralActivity;
        }
    }
    m_matrixWidget->setProfile(profile, layers);
    if (activity.isEmpty()) {
        const QString idle = status.modelStateText.contains(QStringLiteral("loaded"), Qt::CaseInsensitive)
            ? T(QStringLiteral("Model loaded")) + QStringLiteral(" · ") + T(QStringLiteral("waiting for %1 activity")).arg(profile)
            : T(QStringLiteral("Model missing")) + QStringLiteral(" · ") + T(QStringLiteral("collecting %1 samples")).arg(profile);
        m_matrixWidget->setIdleText(idle);
    } else {
        m_matrixWidget->setIdleText(QString());
    }
    m_matrixWidget->setActivity(activity);
}

void DdspPanelWidget::updateStatus(const DeepDspController::Status &status)
{
    if (m_lblState != nullptr) {
        m_lblState->setText(T(status.stateText));
    }
    const QString assist = status.assistMode.trimmed().toLower();
    if (m_lblModeMeaning != nullptr) {
        if (!status.enabled || assist == QStringLiteral("off")) {
            m_lblModeMeaning->setText(T(QStringLiteral("Off: MIND is completely bypassed. Native decoders only, zero training/scoring overhead.")));
        } else if (assist == QStringLiteral("shadow") || assist == QStringLiteral("training")) {
            m_lblModeMeaning->setText(T(QStringLiteral("Training: MIND is learning from RX. It does not change the decoder output.")));
        } else {
            m_lblModeMeaning->setText(T(QStringLiteral("Active: MIND may help weak FT candidates, CW human-fist event timing and RTTY mark/space slicing. It never invents decoded text.")));
        }
    }
    const QString profile = effectiveProfile(status);
    if (m_lblProfileMeaning != nullptr) {
        if (profile == QStringLiteral("CW")) {
            if (assist == QStringLiteral("assisted") || assist == QStringLiteral("active")) {
                const QString state = status.cwAssistReady
                    ? T(QStringLiteral("CW Active: heavy human-fist assist ready. MIND steers dit/dah/gap timing and the native Morse event decoder; it does not invent callsigns."))
                    : T(QStringLiteral("CW Active: not ready yet. Classic CW/ggmorse is used while MIND trains.")) + QStringLiteral(" ") + status.cwAssistReason;
                m_lblProfileMeaning->setText(state);
            } else {
                m_lblProfileMeaning->setText(T(QStringLiteral("CW Training: MIND learns dit/dah/gap timing only. Active mode uses it heavily for human fist recovery.")));
            }
        } else if (profile == QStringLiteral("RTTY")) {
            if (assist == QStringLiteral("assisted") || assist == QStringLiteral("active")) {
                const QString state = status.rttyAssistReady
                    ? T(QStringLiteral("RTTY Active: ready. MIND may correct only weak/borderline Mark/Space bits; text remains Baudot."))
                    : T(QStringLiteral("RTTY Active: not ready yet. Classic RTTY is used while MIND trains.")) + QStringLiteral(" ") + status.rttyAssistReason;
                m_lblProfileMeaning->setText(state);
            } else {
                m_lblProfileMeaning->setText(T(QStringLiteral("RTTY Training: MIND collects Mark/Space bit samples. It does not change decoding.")));
            }
        } else {
            m_lblProfileMeaning->setText(T(QStringLiteral("FT MIND: ranks candidates and chooses where to spend ultra-deep recovery. CRC/parser still validate final messages.")));
        }
    }
    if (m_lblSamples != nullptr) {
        int shownSamples = status.sampleCount;
        if (profile == QStringLiteral("FT8")) shownSamples = status.ft8Samples;
        else if (profile == QStringLiteral("FT4")) shownSamples = status.ft4Samples;
        else if (profile == QStringLiteral("CW")) shownSamples = status.cwSamples;
        else if (profile == QStringLiteral("RTTY")) shownSamples = status.rttySamples;
        m_lblSamples->setText(T(QStringLiteral("Training data")) + QStringLiteral(": %1 %2").arg(profile).arg(shownSamples));
    }
    if (m_lblBreakdown != nullptr) {
        m_lblBreakdown->setText(T(QStringLiteral("Data split")) + QStringLiteral(": FT8 %1 · FT4 %2 · RTTY %3 · CW %4")
                                    .arg(status.ft8Samples)
                                    .arg(status.ft4Samples)
                                    .arg(status.rttySamples)
                                    .arg(status.cwSamples));
    }
    if (m_lblLoss != nullptr) {
        if (profile == QStringLiteral("CW")) {
            m_lblLoss->setText(T(QStringLiteral("CW event profile")) + QStringLiteral(" · ") + T(QStringLiteral("Loss")) + QStringLiteral(" %1")
                                   .arg(status.lastLoss, 0, 'f', 5));
        } else if (profile == QStringLiteral("RTTY")) {
            m_lblLoss->setText(T(QStringLiteral("RTTY soft-slicer")) + QStringLiteral(" · ") + T(QStringLiteral("Loss")) + QStringLiteral(" %1")
                                   .arg(status.lastLoss, 0, 'f', 5));
        } else {
            const QString lossText = T(QStringLiteral("Replay")) + QStringLiteral(" %1 · ") +
                                     T(QStringLiteral("Auto")) + QStringLiteral(" %2 ms/%3 · %4 ") +
                                     T(QStringLiteral("samp/s")) + QStringLiteral(" · ") +
                                     T(QStringLiteral("Loss")) + QStringLiteral(" %5");
            m_lblLoss->setText(lossText.arg(status.replayBufferSamples)
                                       .arg(status.adaptiveTrainingBudgetMs)
                                       .arg(status.adaptiveBatchSize)
                                       .arg(status.trainSamplesPerSecond, 0, 'f', 0)
                                       .arg(status.lastLoss, 0, 'f', 5));
        }
    }
    if (m_lblArchitecture != nullptr) {
        QString arch = status.architectureText;
        if (profile == QStringLiteral("CW")) arch = QStringLiteral("256 → 96 → 48 → 6");
        else if (profile == QStringLiteral("RTTY")) arch = QStringLiteral("96 → 64 → 32 → 8");
        else if (profile == QStringLiteral("FT4")) arch = QStringLiteral("58×8 → Conv2D → sigmoid");
        else if (profile == QStringLiteral("FT8")) arch = QStringLiteral("58×8 → Conv2D → sigmoid");
        m_lblArchitecture->setText(T(QStringLiteral("Net")) + QStringLiteral(" ") + profile + QStringLiteral(": ") + arch);
    }
    if (m_lblBackend != nullptr) {
        m_lblBackend->setText(T(QStringLiteral("Backend")) + QStringLiteral(": ") + status.backendText);
        m_lblBackend->setToolTip(T(QStringLiteral("MIND uses Eigen/OpenMP batched matrix-matrix training on a low-priority autonomous trainer thread. The user cannot set a fixed trainer budget; MadModem adapts it from RX/TX, CW, FT timing and idle/configuration state.")));
    }
    if (m_lblModelState != nullptr) {
        const QString modelText = T(status.modelStateText);
        m_lblModelState->setText(T(QStringLiteral("Model")) + QStringLiteral(": ") + modelText +
                                 QStringLiteral(" · ") + T(QStringLiteral("Dataset")) + QStringLiteral(" ") + QString::number(status.loadedGoldSamples) +
                                 QStringLiteral(" · ") + status.lastCheckpointText);
        m_lblModelState->setToolTip(T(QStringLiteral("Checkpoint")) + QStringLiteral(": ") + status.checkpointPath +
                                    QStringLiteral("\n") + T(QStringLiteral("Stats")) + QStringLiteral(": ") + status.statsPath +
                                    QStringLiteral("\n") + T(QStringLiteral("Gold dataset")) + QStringLiteral(": ") + status.goldDatasetPath);
    }
    constexpr int kUiMinValidation = 200;
    const bool warmup = status.validationCount < kUiMinValidation;
    if (m_lblTrainingCompletion != nullptr) {
        if (profile == QStringLiteral("CW")) {
            m_lblTrainingCompletion->setText(T(QStringLiteral("CW assist")) + QStringLiteral(": ") +
                                             (status.cwAssistReady ? T(QStringLiteral("ready")) : T(QStringLiteral("training"))) +
                                             QStringLiteral(" · ") + T(QStringLiteral("batches")) + QStringLiteral(" ") + QString::number(status.cwTrainingRuns) +
                                             QStringLiteral(" · ") + T(QStringLiteral("accuracy")) + QStringLiteral(" ") + QString::number(status.cwAccuracy, 'f', 1) + QStringLiteral(" %"));
            m_lblTrainingCompletion->setToolTip(T(QStringLiteral("CW MIND is a heavy human-fist event/timing helper. In Active it can steer dit/dah/gap decisions and the native Morse stream, but it never invents callsigns or free text.")));
        } else if (profile == QStringLiteral("RTTY")) {
            m_lblTrainingCompletion->setText(T(QStringLiteral("RTTY assist")) + QStringLiteral(": ") +
                                             (status.rttyAssistReady ? T(QStringLiteral("ready")) : T(QStringLiteral("training"))) +
                                             QStringLiteral(" · ") + T(QStringLiteral("batches")) + QStringLiteral(" ") + QString::number(status.rttyTrainingRuns) +
                                             QStringLiteral(" · ") + T(QStringLiteral("accuracy")) + QStringLiteral(" ") + QString::number(status.rttyAccuracy, 'f', 1) + QStringLiteral(" % · ") +
                                             T(QStringLiteral("samples")) + QStringLiteral(" ") + QString::number(status.rttySamples));
            m_lblTrainingCompletion->setToolTip(T(QStringLiteral("RTTY MIND classifies bit-level Mark/Space confidence. Active mode may override only low-confidence slicer decisions; it never generates text.")));
        } else {
            const QString msgPart = warmup
                ? QStringLiteral("--")
                : QString::number(status.messageAccuracy, 'f', 2) + QStringLiteral(" %");
            const QString estPart = status.estimatedExactFrameAccuracy > 0.0
                ? QString::number(status.estimatedExactFrameAccuracy, 'f', 1) + QStringLiteral(" %")
                : QStringLiteral("--");
            const QString bitPart = status.bitAccuracy > 0.0
                ? QString::number(status.bitAccuracy, 'f', 1) + QStringLiteral(" %")
                : QStringLiteral("--");
            const QString bestPart = status.bestBitAccuracy > 0.0
                ? QString::number(status.bestBitAccuracy, 'f', 1) + QStringLiteral(" %")
                : QStringLiteral("--");
            m_lblTrainingCompletion->setText(T(QStringLiteral("Ranker")) + QStringLiteral(": ") + bitPart + QStringLiteral(" · ") +
                                             T(QStringLiteral("Best")) + QStringLiteral(": ") + bestPart + QStringLiteral(" · ") +
                                             T(QStringLiteral("Val")) + QStringLiteral(": ") + msgPart +
                                             QStringLiteral(" · ") + T(QStringLiteral("Pos/Neg")) + QStringLiteral(": ") + QString::number(status.rankerPositiveSamples) +
                                             QStringLiteral("/") + QString::number(status.rankerNegativeSamples));
            m_lblTrainingCompletion->setToolTip(T(QStringLiteral("MIND Ranker predicts candidate_success_probability for FT candidate ranking/pruning. Final FT text still requires classical LDPC, CRC, unpack and parser validation.")));
        }
    }
    if (m_progressAccuracy != nullptr) {
        m_progressAccuracy->setValue(qBound(0, static_cast<int>(status.trainingCompletionPercent + 0.5), 100));
        if (profile == QStringLiteral("CW")) {
            m_progressAccuracy->setFormat(T(QStringLiteral("CW %p%")));
            m_progressAccuracy->setToolTip(T(QStringLiteral("CW training progress from the dedicated keying-event profile.")));
        } else if (profile == QStringLiteral("RTTY")) {
            m_progressAccuracy->setFormat(T(QStringLiteral("RTTY %p%")));
            m_progressAccuracy->setToolTip(T(QStringLiteral("RTTY training progress from the dedicated soft-slicer profile.")));
        } else {
            m_progressAccuracy->setFormat(T(QStringLiteral("MIND %p%")));
            m_progressAccuracy->setToolTip(T(QStringLiteral("MIND progress measures candidate-ranker assist readiness, not direct message generation.")));
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
    if (m_cmbAssistMode != nullptr) {
        const QString mode = status.assistMode.trimmed().toLower().isEmpty()
                             ? QStringLiteral("shadow")
                             : status.assistMode.trimmed().toLower();
        const int idx = m_cmbAssistMode->findData(mode);
        if (idx >= 0 && m_cmbAssistMode->currentIndex() != idx) {
            m_cmbAssistMode->blockSignals(true);
            m_cmbAssistMode->setCurrentIndex(idx);
            m_cmbAssistMode->blockSignals(false);
        }
        const QString tip = status.assistRequested && !status.assistEnabled
            ? T(QStringLiteral("Active is selected but will remain training-only until the MIND ranker assist-ready gate is reached. Final FT messages still require CRC/unpack/parser validation."))
                  + QStringLiteral("\n") + status.readinessReason
            : T(QStringLiteral("MIND Assist controls DNN helpers: FT ranker before LDPC, CW human-fist event timing, and RTTY mark/space slicing. It never accepts unvalidated FT text or invents callsigns."));
        m_cmbAssistMode->setToolTip(tip);
    }
}
