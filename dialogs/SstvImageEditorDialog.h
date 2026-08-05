#ifndef SSTVIMAGEEDITORDIALOG_H
#define SSTVIMAGEEDITORDIALOG_H

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QImage>

class QCheckBox;
class QFontComboBox;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QWidget;

class SstvEditorCanvas;

/**
 * @brief Basic QSL/SSTV card editor.
 *
 * The dialog is intentionally simple and radio-workflow oriented:
 * a large canvas at the top, editing tools below, draggable text/logo overlays,
 * quick QSL templates, PNG export and direct handoff to SSTV TX.
 */
class SstvImageEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SstvImageEditorDialog(QWidget *parent = nullptr);

    void setBackgroundImage(const QImage &image,
                            const QString &suggestedBaseName = QString());

signals:
    void imageReadyForTx(const QImage &image, const QString &suggestedFileName);

private slots:
    void loadBackground();
    void makeWhiteCard();
    void loadLogo();
    void savePng();
    void useForTx();
    void addTextOverlay();
    void deleteSelectedOverlay();
    void applyCqTemplate();
    void applyQslTemplate();
    void apply73Template();
    void updateSelectedOverlayText();
    void chooseTextColor();
    void handleCanvasSelectionChanged(bool hasSelection);
    void handleQsoFieldsChanged();

private:
    void rebuildPreviewFromFields();
    QString suggestedSavePath() const;
    QFont overlayFont(int pointSize, bool bold = false) const;
    void updateColorButton();

    SstvEditorCanvas *m_canvas = nullptr;
    QScrollArea *m_scrollArea = nullptr;

    QLineEdit *m_editCall = nullptr;
    QLineEdit *m_editName = nullptr;
    QLineEdit *m_editQth = nullptr;
    QLineEdit *m_editReport = nullptr;

    QLineEdit *m_editOverlayText = nullptr;
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_spinFontSize = nullptr;
    QPushButton *m_btnColor = nullptr;
    QCheckBox *m_chkFrame = nullptr;

    QPushButton *m_btnLoadBackground = nullptr;
    QPushButton *m_btnWhiteCard = nullptr;
    QPushButton *m_btnLoadLogo = nullptr;
    QPushButton *m_btnSavePng = nullptr;
    QPushButton *m_btnUseForTx = nullptr;
    QPushButton *m_btnAddText = nullptr;
    QPushButton *m_btnDeleteSelected = nullptr;
    QPushButton *m_btnTemplateCq = nullptr;
    QPushButton *m_btnTemplateQsl = nullptr;
    QPushButton *m_btnTemplate73 = nullptr;

    QString m_suggestedBaseName;
    QColor m_currentColor = Qt::black;
    bool m_updatingUi = false;
};

#endif // SSTVIMAGEEDITORDIALOG_H
