#ifndef SSTVIMAGEEDITORDIALOG_H
#define SSTVIMAGEEDITORDIALOG_H

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QImage>

class QCheckBox;
class QLabel;
class QListWidget;
class QTabWidget;
class QResizeEvent;
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

    void setTxMode(const QString &mode);
    QImage composedImage() const;
    QStringList qsoFields() const;
    QByteArray projectData() const;
    bool restoreProject(const QByteArray &data);
    void setQsoFields(const QString &own,const QString &call,const QString &name,const QString &qth,const QString &report);
    void reject() override;

    void setBackgroundImage(const QImage &image,
                            const QString &suggestedBaseName = QString());

signals:
    void imageReadyForTx(const QImage &image, const QString &suggestedFileName);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void openProject();
    void saveProject();
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
    void fitImage();
    void refreshElements();
    void updateTxPreview();
    QString m_txMode=QStringLiteral("Martin M1");
    QLabel *m_txPreview=nullptr;
    QListWidget *m_elements=nullptr;
    QTabWidget *m_toolTabs=nullptr;
    QPushButton *m_btnUndo=nullptr;
    QPushButton *m_btnRedo=nullptr;
    void syncDocumentControls();
    bool confirmDiscard();
    bool m_fitImage=true;
    QByteArray m_savedDocument;
    QLabel *m_imageInfo=nullptr;
    QLineEdit *m_editMyCall=nullptr;
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
