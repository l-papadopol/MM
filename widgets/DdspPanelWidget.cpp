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
    m_lblSamples->setToolTip(T(QStringLiteral("FT candidate ranker samples.")));
    m_lblModeMeaning = new QLabel(grpStatus);
    m_lblModeMeaning->setVisible(false);
    m_lblProfileMeaning = new QLabel(grpStatus);
    m_lblProfileMeaning->setVisible(false);
    m_lblBreakdown = new QLabel(QStringLiteral("FT8 0 · FT4 0"), grpStatus);
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
    m_cmbProfileView->setToolTip(T(QStringLiteral("FT ranker activity view.")));
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

    auto *assistLabel = new QLabel(T(QStringLiteral("MIND")), grpControl);
    m_cmbAssistMode = new QComboBox(grpControl);
    m_cmbAssistMode->addItem(T(QStringLiteral("Off")), QStringLiteral("off"));
    m_cmbAssistMode->addItem(T(QStringLiteral("Training")), QStringLiteral("shadow"));
    m_cmbAssistMode->addItem(T(QStringLiteral("Assist")), QStringLiteral("assisted"));
    m_cmbAssistMode->setToolTip(T(QStringLiteral("FT candidate priority.")));

    auto *autonomyLabel = new QLabel(grpControl);
    autonomyLabel->setVisible(false);

    controlLayout->addWidget(assistLabel, 0, 0);
    controlLayout->addWidget(m_cmbAssistMode, 0, 1, 1, 2);
    controlLayout->addWidget(autonomyLabel, 1, 0, 1, 3);

    // The production MIND panel is FT-only.  CW/RTTY/text-mode teaching and
    // active neural helpers are intentionally not exposed.


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
    if (profile != QStringLiteral("FT8") && profile != QStringLiteral("FT4")) {
        profile = QStringLiteral("FT8");
    }
    return profile;
}

void DdspPanelWidget::applyProfileToMatrix(const QString &profile, const DeepDspController::Status &status)
{
    if (m_matrixWidget == nullptr) return;

    QVector<int> layers = QVector<int>{464, 8, 24, 1};
    QVector<float> activity;
    if (status.ftActivity.size() == 464 + 8 + 24 + 1) {
        activity = status.ftActivity;
    } else if (status.neuralActivity.size() == 464 + 8 + 24 + 1) {
        activity = status.neuralActivity;
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
        m_lblModeMeaning->clear();
        m_lblModeMeaning->setVisible(false);
    }
    const QString profile = effectiveProfile(status);
    if (m_lblProfileMeaning != nullptr) {
        m_lblProfileMeaning->clear();
        m_lblProfileMeaning->setVisible(false);
    }
    if (m_lblSamples != nullptr) {
        int shownSamples = status.sampleCount;
        if (profile == QStringLiteral("FT8")) shownSamples = status.ft8Samples;
        else if (profile == QStringLiteral("FT4")) shownSamples = status.ft4Samples;
        m_lblSamples->setText(T(QStringLiteral("Training data")) + QStringLiteral(": %1 %2").arg(profile).arg(shownSamples));
    }
    if (m_lblBreakdown != nullptr) {
        m_lblBreakdown->setText(T(QStringLiteral("Data split")) + QStringLiteral(": FT8 %1 · FT4 %2")
                                    .arg(status.ft8Samples)
                                    .arg(status.ft4Samples));
    }
    if (m_lblLoss != nullptr) {
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
    if (m_lblArchitecture != nullptr) {
        QString arch = status.architectureText;
        if (profile == QStringLiteral("FT4")) arch = QStringLiteral("58×8 → Conv2D → sigmoid");
        else if (profile == QStringLiteral("FT8")) arch = QStringLiteral("58×8 → Conv2D → sigmoid");
        m_lblArchitecture->setText(T(QStringLiteral("Net")) + QStringLiteral(" ") + profile + QStringLiteral(": ") + arch);
    }
    if (m_lblBackend != nullptr) {
        m_lblBackend->setText(T(QStringLiteral("Backend")) + QStringLiteral(": ") + status.backendText);
        m_lblBackend->setToolTip(T(QStringLiteral("Low-priority autonomous FT ranker training.")));
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
        const QString valPart = warmup
            ? QStringLiteral("--")
            : QString::number(status.messageAccuracy, 'f', 2) + QStringLiteral(" %");
        const QString scorePart = status.bitAccuracy > 0.0
            ? QString::number(status.bitAccuracy, 'f', 1) + QStringLiteral(" %")
            : QStringLiteral("--");
        const QString bestPart = status.bestBitAccuracy > 0.0
            ? QString::number(status.bestBitAccuracy, 'f', 1) + QStringLiteral(" %")
            : QStringLiteral("--");
        m_lblTrainingCompletion->setText(T(QStringLiteral("Ranker")) + QStringLiteral(": ") + scorePart + QStringLiteral(" · ") +
                                         T(QStringLiteral("Best")) + QStringLiteral(": ") + bestPart + QStringLiteral(" · ") +
                                         T(QStringLiteral("Val")) + QStringLiteral(": ") + valPart +
                                         QStringLiteral(" · ") + T(QStringLiteral("Pos/Neg")) + QStringLiteral(": ") + QString::number(status.rankerPositiveSamples) +
                                         QStringLiteral("/") + QString::number(status.rankerNegativeSamples));
        m_lblTrainingCompletion->setToolTip(T(QStringLiteral("FT candidate priority.")));
    }
    if (m_progressAccuracy != nullptr) {
        m_progressAccuracy->setValue(qBound(0, static_cast<int>(status.trainingCompletionPercent + 0.5), 100));
        m_progressAccuracy->setFormat(T(QStringLiteral("MIND %p%")));
        m_progressAccuracy->setToolTip(T(QStringLiteral("FT candidate priority.")));
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
        QString tip = T(QStringLiteral("FT candidate priority."));
        if (status.assistRequested && !status.assistEnabled && !status.readinessReason.isEmpty()) {
            tip += QStringLiteral("\n") + status.readinessReason;
        }
        m_cmbAssistMode->setToolTip(tip);
    }
}
