#ifndef DDSPPANELWIDGET_H
#define DDSPPANELWIDGET_H

#include "../ai/DeepDspController.h"

#include <QWidget>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class NeuralMatrixWidget;

class DdspPanelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DdspPanelWidget(DeepDspController *controller, QWidget *parent = nullptr);

private slots:
    void updateStatus(const DeepDspController::Status &status);
    void teachManualLabel();
    void profileSelectionChanged(int index);
    void runCwBootcamp();

private:
    DeepDspController *m_controller = nullptr;
    QString effectiveProfile(const DeepDspController::Status &status) const;
    void applyProfileToMatrix(const QString &profile, const DeepDspController::Status &status);

    QLabel *m_lblState = nullptr;
    QLabel *m_lblSamples = nullptr;
    QLabel *m_lblModeMeaning = nullptr;
    QLabel *m_lblProfileMeaning = nullptr;
    QLabel *m_lblBreakdown = nullptr;
    QLabel *m_lblLoss = nullptr;
    QLabel *m_lblArchitecture = nullptr;
    QLabel *m_lblBackend = nullptr;
    QLabel *m_lblModelState = nullptr;
    QLabel *m_lblTrainingCompletion = nullptr;
    QComboBox *m_cmbProfileView = nullptr;
    QLabel *m_lblCheckpoint = nullptr;
    QLabel *m_lblLastCheckpoint = nullptr;
    QComboBox *m_cmbManualMode = nullptr;
    QLineEdit *m_editManualLabel = nullptr;
    QLabel *m_lblManualHint = nullptr;
    QProgressBar *m_progressAccuracy = nullptr;
    NeuralMatrixWidget *m_matrixWidget = nullptr;
    QComboBox *m_cmbAssistMode = nullptr;
};

#endif // DDSPPANELWIDGET_H
