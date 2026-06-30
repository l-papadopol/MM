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
#include <QLinearGradient>
#include <QProgressBar>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QPolygonF>
#include <QRadialGradient>
#include <QSpinBox>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>

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

class MindNixieGaugeWidget : public QWidget
{
public:
    explicit MindNixieGaugeWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(112);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(T(QStringLiteral("MIND extra decodes for the current FT session.")));
    }

    void setStats(int extraLastSlot,
                  int extraSession,
                  int ldpcSkippedLastSlot,
                  int ldpcSkippedSession,
                  int scoredLastSlot,
                  double avgSuccessPercent,
                  bool active)
    {
        Q_UNUSED(extraLastSlot)
        Q_UNUSED(ldpcSkippedLastSlot)
        Q_UNUSED(ldpcSkippedSession)
        Q_UNUSED(scoredLastSlot)
        Q_UNUSED(avgSuccessPercent)
        m_extraSession = qMax(0, extraSession);
        m_active = active;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF r = rect().adjusted(1, 1, -2, -2);
        QLinearGradient bg(r.topLeft(), r.bottomLeft());
        bg.setColorAt(0.0, QColor(18, 10, 6));
        bg.setColorAt(0.55, QColor(6, 4, 4));
        bg.setColorAt(1.0, QColor(2, 2, 3));
        p.setBrush(bg);
        p.setPen(QPen(QColor(112, 62, 30), 1.2));
        p.drawRoundedRect(r, 8, 8);
        p.setPen(QPen(QColor(236, 130, 48, 42), 3.0));
        p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), 8, 8);

        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(qMax(7.5, titleFont.pointSizeF() * 0.82));
        p.setFont(titleFont);
        p.setPen(QColor(224, 142, 72));
        p.drawText(r.adjusted(9, 6, -9, 0), Qt::AlignLeft | Qt::AlignTop,
                   T(QStringLiteral("MIND GAIN")));

        const QString digits = QStringLiteral("%1").arg(qMin(999, m_extraSession), 3, 10, QLatin1Char('0'));
        const QRectF digitArea = r.adjusted(16, 24, -16, -8);
        const qreal gap = qMax<qreal>(8.0, digitArea.width() * 0.035);
        const qreal tileW = qMin((digitArea.width() - 2.0 * gap) / 3.0, digitArea.height() * 0.76);
        const qreal tileH = digitArea.height();
        const qreal totalW = tileW * 3.0 + gap * 2.0;
        qreal x = digitArea.center().x() - totalW / 2.0;
        for (int i = 0; i < 3; ++i) {
            const QRectF tube(x, digitArea.top(), tileW, tileH);
            drawNixieTube(p, tube, digits.at(i), m_active);
            x += tileW + gap;
        }
    }

private:
    static void drawHoneycomb(QPainter &p, const QRectF &r)
    {
        p.save();
        p.setClipRect(r.adjusted(1, 1, -1, -1));
        p.setPen(QPen(QColor(168, 62, 20, 56), 0.7));
        const qreal cell = qMax<qreal>(5.0, r.width() / 12.0);
        const qreal h = cell * 0.8660254;
        for (qreal y = r.top() - h; y < r.bottom() + h; y += h) {
            const int row = static_cast<int>((y - r.top()) / h);
            for (qreal x = r.left() - cell; x < r.right() + cell; x += cell * 1.5) {
                const qreal ox = (row & 1) ? cell * 0.75 : 0.0;
                QPolygonF poly;
                for (int k = 0; k < 6; ++k) {
                    const double a = 3.14159265358979323846 / 6.0 + k * 3.14159265358979323846 / 3.0;
                    poly << QPointF(x + ox + cell * 0.48 * std::cos(a), y + cell * 0.48 * std::sin(a));
                }
                p.drawPolygon(poly);
            }
        }
        p.restore();
    }

    static QPainterPath sevenSegmentPath(const QRectF &r, int mask)
    {
        auto seg = [](const QRectF &sr, bool horizontal) {
            QPainterPath path;
            const qreal radius = qMin(sr.width(), sr.height()) * 0.45;
            if (horizontal) {
                QPolygonF poly;
                poly << QPointF(sr.left() + radius, sr.top())
                     << QPointF(sr.right() - radius, sr.top())
                     << QPointF(sr.right(), sr.center().y())
                     << QPointF(sr.right() - radius, sr.bottom())
                     << QPointF(sr.left() + radius, sr.bottom())
                     << QPointF(sr.left(), sr.center().y());
                path.addPolygon(poly);
                path.closeSubpath();
            } else {
                QPolygonF poly;
                poly << QPointF(sr.center().x(), sr.top())
                     << QPointF(sr.right(), sr.top() + radius)
                     << QPointF(sr.right(), sr.bottom() - radius)
                     << QPointF(sr.center().x(), sr.bottom())
                     << QPointF(sr.left(), sr.bottom() - radius)
                     << QPointF(sr.left(), sr.top() + radius);
                path.addPolygon(poly);
                path.closeSubpath();
            }
            return path;
        };

        const qreal t = qMax<qreal>(3.0, qMin(r.width(), r.height()) * 0.13);
        const qreal x0 = r.left();
        const qreal x1 = r.right();
        const qreal y0 = r.top();
        const qreal y1 = r.bottom();
        const qreal ym = r.center().y();
        const qreal inset = t * 0.58;
        QPainterPath out;
        if (mask & 0x01) out.addPath(seg(QRectF(x0 + inset, y0, r.width() - 2 * inset, t), true));
        if (mask & 0x02) out.addPath(seg(QRectF(x1 - t, y0 + inset, t, ym - y0 - inset), false));
        if (mask & 0x04) out.addPath(seg(QRectF(x1 - t, ym, t, y1 - ym - inset), false));
        if (mask & 0x08) out.addPath(seg(QRectF(x0 + inset, y1 - t, r.width() - 2 * inset, t), true));
        if (mask & 0x10) out.addPath(seg(QRectF(x0, ym, t, y1 - ym - inset), false));
        if (mask & 0x20) out.addPath(seg(QRectF(x0, y0 + inset, t, ym - y0 - inset), false));
        if (mask & 0x40) out.addPath(seg(QRectF(x0 + inset, ym - t / 2.0, r.width() - 2 * inset, t), true));
        return out;
    }

    static int digitMask(QChar ch)
    {
        switch (ch.toLatin1()) {
        case '0': return 0x3f; // a b c d e f
        case '1': return 0x06;
        case '2': return 0x5b;
        case '3': return 0x4f;
        case '4': return 0x66;
        case '5': return 0x6d;
        case '6': return 0x7d;
        case '7': return 0x07;
        case '8': return 0x7f;
        case '9': return 0x6f;
        default: return 0x00;
        }
    }

    static void drawNixieTube(QPainter &p, const QRectF &tubeRect, QChar digit, bool active)
    {
        p.save();
        const QColor rim(118, 46, 20, active ? 150 : 70);
        const QColor glass(28, 12, 5, active ? 190 : 130);
        const QRectF glassRect = tubeRect.adjusted(1.0, 1.0, -1.0, -1.0);
        QLinearGradient tubeBg(glassRect.topLeft(), glassRect.bottomRight());
        tubeBg.setColorAt(0.0, QColor(32, 14, 7));
        tubeBg.setColorAt(1.0, QColor(2, 2, 3));
        p.setBrush(tubeBg);
        p.setPen(QPen(rim, 1.1));
        p.drawRoundedRect(glassRect, 12, 12);
        p.setPen(QPen(QColor(235, 95, 22, active ? 42 : 22), 3.0));
        p.drawRoundedRect(glassRect.adjusted(1, 1, -1, -1), 12, 12);

        drawHoneycomb(p, glassRect.adjusted(4, 4, -4, -4));

        // Nixie internal supports/wires behind the active digit.
        p.setPen(QPen(QColor(145, 54, 22, active ? 75 : 36), 0.8));
        const qreal cx = glassRect.center().x();
        p.drawLine(QPointF(cx, glassRect.top() + 5), QPointF(cx, glassRect.bottom() - 5));
        p.drawLine(QPointF(cx - glassRect.width() * 0.20, glassRect.top() + 8),
                   QPointF(cx + glassRect.width() * 0.15, glassRect.bottom() - 8));
        p.drawLine(QPointF(cx + glassRect.width() * 0.20, glassRect.top() + 8),
                   QPointF(cx - glassRect.width() * 0.15, glassRect.bottom() - 8));

        const QRectF d = glassRect.adjusted(glassRect.width() * 0.18,
                                            glassRect.height() * 0.12,
                                            -glassRect.width() * 0.18,
                                            -glassRect.height() * 0.12);
        QPainterPath path = sevenSegmentPath(d, digitMask(digit));

        // Dark red inactive digit skeleton.
        p.setBrush(QColor(100, 22, 8, 80));
        p.setPen(Qt::NoPen);
        p.drawPath(sevenSegmentPath(d, 0x7f));

        if (active) {
            for (int glow = 10; glow >= 2; glow -= 2) {
                p.setPen(QPen(QColor(255, 58, 8, 12 + glow * 3), glow, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPath(path);
            }
            p.fillPath(path, QColor(255, 178, 34));
            p.setPen(QPen(QColor(255, 225, 64), 1.0));
            p.drawPath(path);
        } else {
            p.fillPath(path, QColor(105, 54, 25));
        }

        if (digit == QLatin1Char('0')) {
            const qreal dotR = qMax<qreal>(2.0, qMin(d.width(), d.height()) * 0.08);
            const QPointF dotCenter = glassRect.center();
            if (active) {
                QRadialGradient g(dotCenter, dotR * 4.5);
                g.setColorAt(0.0, QColor(255, 235, 70, 255));
                g.setColorAt(0.35, QColor(255, 130, 20, 190));
                g.setColorAt(1.0, QColor(255, 70, 5, 0));
                p.setBrush(g);
                p.setPen(Qt::NoPen);
                p.drawEllipse(dotCenter, dotR * 4.5, dotR * 4.5);
                p.setBrush(QColor(255, 212, 44));
                p.drawEllipse(dotCenter, dotR, dotR);
            } else {
                p.setBrush(QColor(100, 52, 22));
                p.setPen(Qt::NoPen);
                p.drawEllipse(dotCenter, dotR, dotR);
            }
        }
        p.restore();
    }

    int m_extraSession = 0;
    bool m_active = false;
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
    m_lblSamples->setToolTip(T(QStringLiteral("Weak-signal candidate ranker samples.")));
    m_lblModeMeaning = new QLabel(grpStatus);
    m_lblModeMeaning->setVisible(false);
    m_lblProfileMeaning = new QLabel(grpStatus);
    m_lblProfileMeaning->setVisible(false);
    m_lblBreakdown = new QLabel(QStringLiteral("FT8 0 · FT4 0 · MSK144 0"), grpStatus);
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

    m_nixieGauge = new MindNixieGaugeWidget(grpStatus);

    statusLayout->addWidget(m_lblState, 0, 0);
    statusLayout->addWidget(m_progressAccuracy, 1, 0);
    statusLayout->addWidget(m_nixieGauge, 2, 0);
    statusLayout->addWidget(m_lblModeMeaning, 3, 0);
    statusLayout->addWidget(m_lblSamples, 4, 0);
    statusLayout->addWidget(m_lblTrainingCompletion, 5, 0);
    statusLayout->addWidget(m_lblProfileMeaning, 6, 0);
    statusLayout->addWidget(m_lblBreakdown, 7, 0);
    statusLayout->addWidget(m_lblArchitecture, 8, 0);
    statusLayout->addWidget(m_lblBackend, 9, 0);
    statusLayout->addWidget(m_lblModelState, 10, 0);
    statusLayout->addWidget(m_lblLoss, 11, 0);
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
    m_cmbProfileView->addItem(QStringLiteral("MSK144"), QStringLiteral("MSK144"));
    m_cmbProfileView->setToolTip(T(QStringLiteral("FT/MSK144 ranker activity view.")));
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
    m_cmbAssistMode->setToolTip(T(QStringLiteral("FT/MSK144 candidate priority.")));

    auto *autonomyLabel = new QLabel(grpControl);
    autonomyLabel->setVisible(false);

    controlLayout->addWidget(assistLabel, 0, 0);
    controlLayout->addWidget(m_cmbAssistMode, 0, 1, 1, 2);
    controlLayout->addWidget(autonomyLabel, 1, 0, 1, 3);

    // The production MIND panel is exposed only for FT4/FT8 and MSK144.
    // CW/RTTY/BPSK/Q65/text-mode teaching and neural helpers remain hidden.


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
    if (profile != QStringLiteral("FT8") && profile != QStringLiteral("FT4") &&
        profile != QStringLiteral("MSK144")) {
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
    if (m_nixieGauge != nullptr) {
        const bool active = status.enabled && assist != QStringLiteral("off");
        m_nixieGauge->setStats(status.mindExtraLastSlot,
                               status.mindExtraSession,
                               status.mindLdpcSkippedLastSlot,
                               status.mindLdpcSkippedSession,
                               status.mindScoredLastSlot,
                               status.mindAvgSuccessLastSlot,
                               active);
        m_nixieGauge->setVisible(active);
    }
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
        else if (profile == QStringLiteral("MSK144")) shownSamples = status.msk144Samples;
        m_lblSamples->setText(T(QStringLiteral("Training data")) + QStringLiteral(": %1 %2").arg(profile).arg(shownSamples));
    }
    if (m_lblBreakdown != nullptr) {
        m_lblBreakdown->setText(T(QStringLiteral("Data split")) + QStringLiteral(": FT8 %1 · FT4 %2 · MSK144 %3")
                                    .arg(status.ft8Samples)
                                    .arg(status.ft4Samples)
                                    .arg(status.msk144Samples));
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
        else if (profile == QStringLiteral("MSK144")) arch = QStringLiteral("58×8 ping/chunk → Conv2D → sigmoid");
        m_lblArchitecture->setText(T(QStringLiteral("Net")) + QStringLiteral(" ") + profile + QStringLiteral(": ") + arch);
    }
    if (m_lblBackend != nullptr) {
        m_lblBackend->setText(T(QStringLiteral("Backend")) + QStringLiteral(": ") + status.backendText);
        m_lblBackend->setToolTip(T(QStringLiteral("Low-priority autonomous FT/MSK144 ranker training.")));
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
    const bool profileIsMsk = (profile == QStringLiteral("MSK144"));
    int profileSamples = status.sampleCount;
    if (profile == QStringLiteral("FT8")) profileSamples = status.ft8Samples;
    else if (profile == QStringLiteral("FT4")) profileSamples = status.ft4Samples;
    else if (profile == QStringLiteral("MSK144")) profileSamples = status.msk144Samples;
    const QString activeProfile = status.activeProfile.trimmed().toUpper();
    const bool showingActiveProfile = (profile == activeProfile);
    const int profileValidation = showingActiveProfile ? (profileIsMsk ? status.activeProfileValidationCount : status.validationCount) : 0;
    const double profileScore = showingActiveProfile ? (profileIsMsk ? status.activeProfileRankerAccuracy : status.bitAccuracy) : 0.0;
    const double profileBest = showingActiveProfile ? (profileIsMsk ? status.activeProfileBestRankerAccuracy : status.bestBitAccuracy) : 0.0;
    const bool warmup = !showingActiveProfile || profileValidation < kUiMinValidation || profileSamples <= 0;
    if (m_lblTrainingCompletion != nullptr) {
        const QString valPart = warmup
            ? QStringLiteral("--")
            : QString::number(profileValidation);
        const QString scorePart = (!warmup && profileScore > 0.0)
            ? QString::number(profileScore, 'f', 1) + QStringLiteral(" %")
            : QStringLiteral("--");
        const QString bestPart = (!warmup && profileBest > 0.0)
            ? QString::number(profileBest, 'f', 1) + QStringLiteral(" %")
            : QStringLiteral("--");
        m_lblTrainingCompletion->setText(T(QStringLiteral("Ranker")) + QStringLiteral(": ") + scorePart + QStringLiteral(" · ") +
                                         T(QStringLiteral("Best")) + QStringLiteral(": ") + bestPart + QStringLiteral(" · ") +
                                         T(QStringLiteral("Val")) + QStringLiteral(": ") + valPart +
                                         QStringLiteral(" · ") + T(QStringLiteral("Pos/Neg")) + QStringLiteral(": ") + QString::number(status.rankerPositiveSamples) +
                                         QStringLiteral("/") + QString::number(status.rankerNegativeSamples));
        m_lblTrainingCompletion->setToolTip(profileIsMsk
            ? T(QStringLiteral("MSK144 ranker stats are shown only after real MSK144 candidate samples exist."))
            : T(QStringLiteral("FT candidate priority.")));
    }
    if (m_progressAccuracy != nullptr) {
        const int progress = warmup ? 0 : qBound(0, static_cast<int>(status.trainingCompletionPercent + 0.5), 100);
        m_progressAccuracy->setValue(progress);
        m_progressAccuracy->setFormat(profileIsMsk && status.msk144Samples <= 0
            ? T(QStringLiteral("MIND MSK144 0%"))
            : T(QStringLiteral("MIND %p%")));
        m_progressAccuracy->setToolTip(profileIsMsk
            ? T(QStringLiteral("MSK144 training starts only from real MSK144 ping/chunk candidates."))
            : T(QStringLiteral("FT candidate priority.")));
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
        QString tip = T(QStringLiteral("FT/MSK144 candidate priority."));
        if (status.assistRequested && !status.assistEnabled && !status.readinessReason.isEmpty()) {
            tip += QStringLiteral("\n") + status.readinessReason;
        }
        m_cmbAssistMode->setToolTip(tip);
    }
}
