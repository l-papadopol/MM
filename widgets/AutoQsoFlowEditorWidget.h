#ifndef AUTOQSOFLOWEDITORWIDGET_H
#define AUTOQSOFLOWEDITORWIDGET_H

#include <QWidget>

class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPushButton;

/**
 * @brief Visual FT AutoQSO flow editor.
 *
 * The editor stores a JSON flow description in Settings.  This step makes the
 * visual editor actually editable: users can add, edit, connect, delete,
 * validate, restore and save AutoQSO flow blocks.  The live radio executor is
 * still intentionally separated from the editor; the scheduler keeps final
 * authority over UTC slots, PTT and audio safety.
 */
class AutoQsoFlowEditorWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit AutoQsoFlowEditorWidget(QWidget *parent = nullptr);

    void setFlowJson(const QString &json);
    QString flowJson() const;

    static QString defaultFlowJson();

signals:
    void flowChanged();

private slots:
    void restoreDefaultFlow();
    void validateFlow();
    void fitFlowInView();
    void addEventNode();
    void addConditionNode();
    void addActionNode();
    void addTimerNode();
    void addVariableNode();
    void addCompareNode();
    void addLoopNode();
    void addIoNode();
    void addMathNode();
    void addTerminalNode();
    void deleteSelectedArrows();
    void showHelp();
    void editSelectedNode();
    void connectNodes();
    void deleteSelectedItems();

private:
    void rebuildSceneFromJson(const QString &json);
    QString serializeSceneToJson() const;
    QString normalizedOrDefaultJson(const QString &json) const;
    void setStatus(const QString &message, bool warning = false);
    void addNodeWithDialog(const QString &kind);
    void refreshSceneBounds();

    QGraphicsScene *m_scene = nullptr;
    QGraphicsView *m_view = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_btnRestore = nullptr;
    QPushButton *m_btnValidate = nullptr;
    QPushButton *m_btnFit = nullptr;
    QPushButton *m_btnAddEvent = nullptr;
    QPushButton *m_btnAddCondition = nullptr;
    QPushButton *m_btnAddAction = nullptr;
    QPushButton *m_btnAddTimer = nullptr;
    QPushButton *m_btnAddVariable = nullptr;
    QPushButton *m_btnAddCompare = nullptr;
    QPushButton *m_btnAddLoop = nullptr;
    QPushButton *m_btnAddIo = nullptr;
    QPushButton *m_btnAddMath = nullptr;
    QPushButton *m_btnAddTerminal = nullptr;
    QPushButton *m_btnEdit = nullptr;
    QPushButton *m_btnConnect = nullptr;
    QPushButton *m_btnDelete = nullptr;
    QPushButton *m_btnDeleteArrows = nullptr;
    QPushButton *m_btnHelp = nullptr;
    QString m_lastValidJson;
};

#endif // AUTOQSOFLOWEDITORWIDGET_H
