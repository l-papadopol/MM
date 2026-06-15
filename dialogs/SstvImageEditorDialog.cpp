#include "SstvImageEditorDialog.h"
#include "../utils/UiScale.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <functional>

class SstvEditorCanvas : public QWidget
{
public:
    enum class OverlayType { Text, Image };

    struct Overlay {
        OverlayType type = OverlayType::Text;
        QString text;
        QImage image;
        QPoint pos;
        QFont font;
        QColor color = Qt::black;
        bool qsoField = false;
        QString qsoKey;
    };

    explicit SstvEditorCanvas(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        makeWhiteCard();
    }

    void setSelectionChangedCallback(const std::function<void(bool)> &callback)
    {
        m_selectionChanged = callback;
    }

    void makeWhiteCard()
    {
        m_background = QImage(960, 744, QImage::Format_RGB32);
        m_background.fill(Qt::white);
        m_overlays.clear();
        m_selectedIndex = -1;
        syncSize();
        update();
        notifySelection();
    }

    void setBackgroundImage(const QImage &image)
    {
        if (image.isNull()) {
            makeWhiteCard();
            return;
        }

        m_background = image.convertToFormat(QImage::Format_RGB32);
        m_overlays.clear();
        m_selectedIndex = -1;
        syncSize();
        update();
        notifySelection();
    }

    void setFrameEnabled(bool enabled)
    {
        m_frameEnabled = enabled;
        update();
    }

    void addTextOverlay(const QString &text,
                        const QFont &font,
                        const QColor &color,
                        const QPoint &pos = QPoint(28, 40),
                        bool qsoField = false,
                        const QString &qsoKey = QString())
    {
        Overlay overlay;
        overlay.type = OverlayType::Text;
        overlay.text = text;
        overlay.font = font;
        overlay.color = color;
        overlay.pos = pos;
        overlay.qsoField = qsoField;
        overlay.qsoKey = qsoKey;
        m_overlays.append(overlay);
        setSelectedIndex(m_overlays.size() - 1);
        update();
    }

    void addImageOverlay(const QImage &image, const QPoint &pos = QPoint(430, 18))
    {
        if (image.isNull()) {
            return;
        }

        Overlay overlay;
        overlay.type = OverlayType::Image;
        overlay.image = image;
        overlay.pos = pos;
        m_overlays.append(overlay);
        setSelectedIndex(m_overlays.size() - 1);
        update();
    }

    void deleteSelectedOverlay()
    {
        if (!hasSelection()) {
            return;
        }

        m_overlays.removeAt(m_selectedIndex);
        if (m_overlays.isEmpty()) {
            m_selectedIndex = -1;
        } else if (m_selectedIndex >= m_overlays.size()) {
            m_selectedIndex = m_overlays.size() - 1;
        }
        update();
        notifySelection();
    }

    void clearQsoFieldOverlays()
    {
        for (int i = m_overlays.size() - 1; i >= 0; --i) {
            if (m_overlays.at(i).qsoField) {
                m_overlays.removeAt(i);
            }
        }
        if (m_selectedIndex >= m_overlays.size()) {
            m_selectedIndex = m_overlays.size() - 1;
        }
        update();
        notifySelection();
    }

    void updateOrCreateQsoField(const QString &key,
                                const QString &text,
                                const QPoint &pos,
                                const QFont &font,
                                const QColor &color)
    {
        for (Overlay &overlay : m_overlays) {
            if (overlay.qsoField && overlay.qsoKey == key) {
                overlay.text = text;
                overlay.pos = pos;
                overlay.font = font;
                overlay.color = color;
                update();
                return;
            }
        }

        Overlay overlay;
        overlay.type = OverlayType::Text;
        overlay.text = text;
        overlay.font = font;
        overlay.color = color;
        overlay.pos = pos;
        overlay.qsoField = true;
        overlay.qsoKey = key;
        m_overlays.append(overlay);
        update();
    }

    bool hasSelection() const
    {
        return m_selectedIndex >= 0 && m_selectedIndex < m_overlays.size();
    }

    bool selectedIsText() const
    {
        return hasSelection() && m_overlays.at(m_selectedIndex).type == OverlayType::Text;
    }

    QString selectedText() const
    {
        if (!selectedIsText()) {
            return QString();
        }
        return m_overlays.at(m_selectedIndex).text;
    }

    QFont selectedFont() const
    {
        if (!selectedIsText()) {
            return QFont();
        }
        return m_overlays.at(m_selectedIndex).font;
    }

    QColor selectedColor() const
    {
        if (!selectedIsText()) {
            return Qt::black;
        }
        return m_overlays.at(m_selectedIndex).color;
    }

    QSize cardSize() const
    {
        return m_background.size();
    }

    void setSelectedText(const QString &text)
    {
        if (!selectedIsText()) {
            return;
        }
        m_overlays[m_selectedIndex].text = text;
        update();
    }

    void setSelectedFont(const QFont &font)
    {
        if (!selectedIsText()) {
            return;
        }
        m_overlays[m_selectedIndex].font = font;
        update();
    }

    void setSelectedColor(const QColor &color)
    {
        if (!selectedIsText()) {
            return;
        }
        m_overlays[m_selectedIndex].color = color;
        update();
    }

    QImage composedImage() const
    {
        QImage output = m_background;
        if (output.isNull()) {
            output = QImage(960, 744, QImage::Format_RGB32);
            output.fill(Qt::white);
        }

        QPainter painter(&output);
        renderCard(&painter);
        return output;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(180, 180, 180));
        painter.drawImage(0, 0, m_background);
        renderCard(&painter);

        if (hasSelection()) {
            const QRect rect = overlayRect(m_selectedIndex);
            if (!rect.isNull()) {
                painter.setPen(QPen(QColor(0, 120, 215), 2, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(rect.adjusted(-4, -4, 4, 4));
            }
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        setFocus(Qt::MouseFocusReason);
        setSelectedIndex(hitTest(event->pos()));
        if (hasSelection()) {
            m_dragging = true;
            m_dragOffset = event->pos() - m_overlays.at(m_selectedIndex).pos;
        } else {
            m_dragging = false;
        }
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_dragging || !hasSelection()) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        Overlay &overlay = m_overlays[m_selectedIndex];
        QPoint pos = event->pos() - m_dragOffset;
        const QRect rect = overlayRect(m_selectedIndex);
        const int w = rect.width();
        const int h = rect.height();
        pos.setX(qBound(0, pos.x(), qMax(0, width() - w)));
        pos.setY(qBound(h, pos.y(), height()));
        overlay.pos = pos;
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            deleteSelectedOverlay();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    void notifySelection()
    {
        if (m_selectionChanged) {
            m_selectionChanged(hasSelection());
        }
    }

    void setSelectedIndex(int index)
    {
        if (m_selectedIndex == index) {
            return;
        }
        m_selectedIndex = index;
        notifySelection();
    }

    void syncSize()
    {
        setMinimumSize(m_background.size());
        resize(m_background.size());
    }

    void renderCard(QPainter *painter) const
    {
        if (m_frameEnabled) {
            painter->setPen(QPen(Qt::black, 8));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(10, 10, m_background.width() - 20, m_background.height() - 20);

            painter->setPen(QPen(Qt::black, 2));
            painter->drawRect(24, 24, m_background.width() - 48, m_background.height() - 116);
            painter->drawRect(24, m_background.height() - 98, m_background.width() - 48, 54);
        }

        for (const Overlay &overlay : m_overlays) {
            if (overlay.type == OverlayType::Image) {
                painter->drawImage(overlay.pos, overlay.image);
            } else {
                painter->setPen(overlay.color);
                painter->setFont(overlay.font);
                painter->drawText(overlay.pos, overlay.text);
            }
        }
    }

    QRect overlayRect(int index) const
    {
        if (index < 0 || index >= m_overlays.size()) {
            return QRect();
        }

        const Overlay &overlay = m_overlays.at(index);
        if (overlay.type == OverlayType::Image) {
            return QRect(overlay.pos, overlay.image.size());
        }

        QFontMetrics metrics(overlay.font);
        QRect rect = metrics.boundingRect(overlay.text.isEmpty() ? QStringLiteral(" ") : overlay.text);
        rect.moveTopLeft(QPoint(overlay.pos.x(), overlay.pos.y() + rect.top()));
        return rect.normalized();
    }

    int hitTest(const QPoint &pos) const
    {
        for (int i = m_overlays.size() - 1; i >= 0; --i) {
            if (overlayRect(i).adjusted(-5, -5, 5, 5).contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    QImage m_background;
    QVector<Overlay> m_overlays;
    int m_selectedIndex = -1;
    bool m_frameEnabled = true;
    bool m_dragging = false;
    QPoint m_dragOffset;
    std::function<void(bool)> m_selectionChanged;
};

SstvImageEditorDialog::SstvImageEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("SSTV QSO / QSL image editor");
    resize(MadModemUi::size(2100, 1500));
    setMinimumSize(1650, 1180);

    m_canvas = new SstvEditorCanvas(this);
    m_canvas->setSelectionChangedCallback([this](bool hasSelection) {
        handleCanvasSelectionChanged(hasSelection);
    });

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_canvas);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setMinimumHeight(980);

    m_btnLoadBackground = new QPushButton("Load background...", this);
    m_btnWhiteCard = new QPushButton("White card", this);
    m_btnLoadLogo = new QPushButton("Load logo...", this);
    m_chkFrame = new QCheckBox("Frame preset", this);
    m_chkFrame->setChecked(true);
    m_btnSavePng = new QPushButton("Save PNG...", this);
    m_btnUseForTx = new QPushButton("Use for SSTV TX", this);
    QPushButton *btnClose = new QPushButton("Close", this);

    for (QPushButton *button : {m_btnLoadBackground, m_btnWhiteCard, m_btnLoadLogo,
                                m_btnSavePng, m_btnUseForTx, btnClose}) {
        button->setMinimumHeight(56);
        button->setMinimumWidth(210);
    }

    m_editCall = new QLineEdit(this);
    m_editName = new QLineEdit(this);
    m_editQth = new QLineEdit(this);
    m_editReport = new QLineEdit(this);
    m_editCall->setPlaceholderText("Other station callsign, e.g. W6HN");
    m_editName->setPlaceholderText("Name / operator");
    m_editQth->setPlaceholderText("QTH / locator / county");
    m_editReport->setPlaceholderText("RST / note / equipment");

    m_editOverlayText = new QLineEdit(this);
    m_editOverlayText->setPlaceholderText("Selected/new text overlay");
    m_fontCombo = new QFontComboBox(this);
    m_spinFontSize = new QSpinBox(this);
    m_spinFontSize->setRange(8, 144);
    m_spinFontSize->setValue(28);
    m_btnColor = new QPushButton("Color", this);
    m_btnColor->setMinimumHeight(42);
    m_btnAddText = new QPushButton("Add text overlay", this);
    m_btnDeleteSelected = new QPushButton("Delete selected", this);
    for (QPushButton *button : {m_btnAddText, m_btnDeleteSelected}) {
        button->setMinimumHeight(52);
        button->setMinimumWidth(230);
    }
    m_btnDeleteSelected->setEnabled(false);

    m_btnTemplateCq = new QPushButton("CQ SSTV", this);
    m_btnTemplateQsl = new QPushButton("QSL card template", this);
    m_btnTemplate73 = new QPushButton("73", this);
    for (QPushButton *button : {m_btnTemplateCq, m_btnTemplateQsl, m_btnTemplate73}) {
        button->setMinimumHeight(50);
        button->setMinimumWidth(210);
    }

    QWidget *toolArea = new QWidget(this);
    QVBoxLayout *toolLayout = new QVBoxLayout(toolArea);
    toolLayout->setContentsMargins(8, 8, 8, 8);
    toolLayout->setSpacing(12);

    QHBoxLayout *fileRow = new QHBoxLayout;
    fileRow->addWidget(m_btnLoadBackground);
    fileRow->addWidget(m_btnWhiteCard);
    fileRow->addWidget(m_btnLoadLogo);
    fileRow->addWidget(m_chkFrame);
    fileRow->addStretch(1);
    fileRow->addWidget(m_btnSavePng);
    fileRow->addWidget(m_btnUseForTx);
    fileRow->addWidget(btnClose);
    toolLayout->addLayout(fileRow);

    QLabel *qsoTitle = new QLabel("SSTV QSO / QSL fields", this);
    QFont sectionFont = qsoTitle->font();
    sectionFont.setBold(true);
    qsoTitle->setFont(sectionFont);
    toolLayout->addWidget(qsoTitle);

    QGridLayout *qsoGrid = new QGridLayout;
    qsoGrid->setHorizontalSpacing(12);
    qsoGrid->setVerticalSpacing(8);
    qsoGrid->addWidget(new QLabel("Callsign"), 0, 0);
    qsoGrid->addWidget(m_editCall, 0, 1);
    qsoGrid->addWidget(new QLabel("Name"), 0, 2);
    qsoGrid->addWidget(m_editName, 0, 3);
    qsoGrid->addWidget(new QLabel("QTH"), 1, 0);
    qsoGrid->addWidget(m_editQth, 1, 1);
    qsoGrid->addWidget(new QLabel("Report / note"), 1, 2);
    qsoGrid->addWidget(m_editReport, 1, 3);
    qsoGrid->setColumnStretch(1, 1);
    qsoGrid->setColumnStretch(3, 1);
    toolLayout->addLayout(qsoGrid);

    QHBoxLayout *templateRow = new QHBoxLayout;
    templateRow->addWidget(new QLabel("Templates"));
    templateRow->addWidget(m_btnTemplateCq);
    templateRow->addWidget(m_btnTemplateQsl);
    templateRow->addWidget(m_btnTemplate73);
    templateRow->addStretch(1);
    toolLayout->addLayout(templateRow);

    QLabel *textTitle = new QLabel("Text overlay", this);
    textTitle->setFont(sectionFont);
    toolLayout->addWidget(textTitle);

    QGridLayout *textGrid = new QGridLayout;
    textGrid->setHorizontalSpacing(12);
    textGrid->setVerticalSpacing(8);
    textGrid->addWidget(new QLabel("Text"), 0, 0);
    textGrid->addWidget(m_editOverlayText, 0, 1, 1, 6);
    textGrid->addWidget(new QLabel("Font"), 1, 0);
    textGrid->addWidget(m_fontCombo, 1, 1, 1, 2);
    textGrid->addWidget(new QLabel("Size"), 1, 3);
    textGrid->addWidget(m_spinFontSize, 1, 4);
    textGrid->addWidget(m_btnColor, 1, 5);
    textGrid->addWidget(m_btnAddText, 1, 6);
    textGrid->addWidget(m_btnDeleteSelected, 1, 7);
    textGrid->setColumnStretch(2, 1);
    toolLayout->addLayout(textGrid);

    QLabel *hint = new QLabel("Drag text/logo overlays directly on the card. Select an overlay and use Delete selected, Canc or Backspace to remove it. "
                               "The QSL template creates correctly positioned editable text elements inspired by classic SSTV/QSL card editors.", this);
    hint->setWordWrap(true);
    toolLayout->addWidget(hint);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(toolArea, 0);
    setLayout(mainLayout);

    updateColorButton();

    connect(m_btnLoadBackground, &QPushButton::clicked, this, &SstvImageEditorDialog::loadBackground);
    connect(m_btnWhiteCard, &QPushButton::clicked, this, &SstvImageEditorDialog::makeWhiteCard);
    connect(m_btnLoadLogo, &QPushButton::clicked, this, &SstvImageEditorDialog::loadLogo);
    connect(m_btnSavePng, &QPushButton::clicked, this, &SstvImageEditorDialog::savePng);
    connect(m_btnUseForTx, &QPushButton::clicked, this, &SstvImageEditorDialog::useForTx);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnAddText, &QPushButton::clicked, this, &SstvImageEditorDialog::addTextOverlay);
    connect(m_btnDeleteSelected, &QPushButton::clicked, this, &SstvImageEditorDialog::deleteSelectedOverlay);
    connect(m_btnTemplateCq, &QPushButton::clicked, this, &SstvImageEditorDialog::applyCqTemplate);
    connect(m_btnTemplateQsl, &QPushButton::clicked, this, &SstvImageEditorDialog::applyQslTemplate);
    connect(m_btnTemplate73, &QPushButton::clicked, this, &SstvImageEditorDialog::apply73Template);
    connect(m_btnColor, &QPushButton::clicked, this, &SstvImageEditorDialog::chooseTextColor);
    connect(m_editOverlayText, &QLineEdit::textEdited, this, &SstvImageEditorDialog::updateSelectedOverlayText);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &SstvImageEditorDialog::updateSelectedOverlayText);
    connect(m_spinFontSize, qOverload<int>(&QSpinBox::valueChanged), this, &SstvImageEditorDialog::updateSelectedOverlayText);
    connect(m_chkFrame, &QCheckBox::toggled, this, [this](bool checked) {
        m_canvas->setFrameEnabled(checked);
    });
    connect(m_editCall, &QLineEdit::textChanged, this, &SstvImageEditorDialog::handleQsoFieldsChanged);
    connect(m_editName, &QLineEdit::textChanged, this, &SstvImageEditorDialog::handleQsoFieldsChanged);
    connect(m_editQth, &QLineEdit::textChanged, this, &SstvImageEditorDialog::handleQsoFieldsChanged);
    connect(m_editReport, &QLineEdit::textChanged, this, &SstvImageEditorDialog::handleQsoFieldsChanged);

    applyQslTemplate();
    MadModemUi::scaleWidgetTree(this);
}

void SstvImageEditorDialog::setBackgroundImage(const QImage &image, const QString &suggestedBaseName)
{
    if (!suggestedBaseName.trimmed().isEmpty()) {
        m_suggestedBaseName = suggestedBaseName.trimmed();
    }
    if (!image.isNull()) {
        m_canvas->setBackgroundImage(image);
    }
    applyQslTemplate();
}

void SstvImageEditorDialog::loadBackground()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Load SSTV background",
                                                          QDir::homePath(),
                                                          "Images (*.png *.jpg *.jpeg *.bmp);;All files (*)");
    if (fileName.isEmpty()) {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, "SSTV editor", "Unable to load the selected background image.");
        return;
    }

    m_suggestedBaseName = QFileInfo(fileName).completeBaseName() + "_sstv";
    m_canvas->setBackgroundImage(image);
    applyQslTemplate();
}

void SstvImageEditorDialog::makeWhiteCard()
{
    m_canvas->makeWhiteCard();
    applyQslTemplate();
}

void SstvImageEditorDialog::loadLogo()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Load logo",
                                                          QDir::homePath(),
                                                          "Images (*.png *.jpg *.jpeg *.bmp);;All files (*)");
    if (fileName.isEmpty()) {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, "SSTV editor", "Unable to load the selected logo image.");
        return;
    }

    const QImage scaled = image.scaled(230, 230, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_canvas->addImageOverlay(scaled, QPoint(680, 55));
}

QString SstvImageEditorDialog::suggestedSavePath() const
{
    QString base = m_suggestedBaseName.trimmed();
    if (base.isEmpty()) {
        base = "SSTV_QSL_card";
    }
    if (!base.endsWith(".png", Qt::CaseInsensitive)) {
        base += ".png";
    }
    return QDir::home().filePath(QFileInfo(base).fileName());
}

void SstvImageEditorDialog::savePng()
{
    const QString fileName = QFileDialog::getSaveFileName(this,
                                                          "Save SSTV PNG",
                                                          suggestedSavePath(),
                                                          "PNG image (*.png)");
    if (fileName.isEmpty()) {
        return;
    }

    if (!m_canvas->composedImage().save(fileName, "PNG")) {
        QMessageBox::warning(this, "SSTV editor", "Unable to save the PNG image.");
    }
}

void SstvImageEditorDialog::useForTx()
{
    const QImage image = m_canvas->composedImage();
    if (image.isNull()) {
        QMessageBox::warning(this, "SSTV editor", "There is no image to send.");
        return;
    }

    emit imageReadyForTx(image, suggestedSavePath());
    accept();
}

void SstvImageEditorDialog::addTextOverlay()
{
    QString text = m_editOverlayText->text().trimmed();
    if (text.isEmpty()) {
        text = "CQ SSTV de IZ6NNH";
        m_editOverlayText->setText(text);
    }

    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_spinFontSize->value());
    m_canvas->addTextOverlay(text, font, m_currentColor, QPoint(70, 130));
}

void SstvImageEditorDialog::deleteSelectedOverlay()
{
    m_canvas->deleteSelectedOverlay();
}

void SstvImageEditorDialog::applyCqTemplate()
{
    QFont big = overlayFont(66, true);
    m_canvas->addTextOverlay(QString("CQ SSTV de %1").arg(m_editCall->text().trimmed().isEmpty() ? QString("IZ6NNH") : m_editCall->text().trimmed()),
                             big,
                             QColor(220, 0, 0),
                             QPoint(80, 135));
}

void SstvImageEditorDialog::applyQslTemplate()
{
    rebuildPreviewFromFields();
}

void SstvImageEditorDialog::apply73Template()
{
    QFont font = overlayFont(72, true);
    m_canvas->addTextOverlay("73!", font, Qt::black, QPoint(720, 650));
}

void SstvImageEditorDialog::updateSelectedOverlayText()
{
    if (m_updatingUi || !m_canvas->selectedIsText()) {
        return;
    }

    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_spinFontSize->value());
    m_canvas->setSelectedText(m_editOverlayText->text());
    m_canvas->setSelectedFont(font);
    m_canvas->setSelectedColor(m_currentColor);
}

void SstvImageEditorDialog::chooseTextColor()
{
    const QColor color = QColorDialog::getColor(m_currentColor, this, "Choose text color");
    if (!color.isValid()) {
        return;
    }

    m_currentColor = color;
    updateColorButton();
    updateSelectedOverlayText();
}

void SstvImageEditorDialog::handleCanvasSelectionChanged(bool hasSelection)
{
    m_btnDeleteSelected->setEnabled(hasSelection);

    if (!hasSelection || !m_canvas->selectedIsText()) {
        return;
    }

    m_updatingUi = true;
    m_editOverlayText->setText(m_canvas->selectedText());
    const QFont font = m_canvas->selectedFont();
    m_fontCombo->setCurrentFont(font);
    if (font.pointSize() > 0) {
        m_spinFontSize->setValue(font.pointSize());
    }
    m_currentColor = m_canvas->selectedColor();
    updateColorButton();
    m_updatingUi = false;
}

void SstvImageEditorDialog::handleQsoFieldsChanged()
{
    rebuildPreviewFromFields();
}

void SstvImageEditorDialog::rebuildPreviewFromFields()
{
    const QString call = m_editCall->text().trimmed().isEmpty() ? QStringLiteral("W6HN") : m_editCall->text().trimmed();
    const QString name = m_editName->text().trimmed().isEmpty() ? QStringLiteral("Operator name") : m_editName->text().trimmed();
    const QString qth = m_editQth->text().trimmed().isEmpty() ? QStringLiteral("QTH / locator") : m_editQth->text().trimmed();
    const QString report = m_editReport->text().trimmed().isEmpty() ? QStringLiteral("RST 599") : m_editReport->text().trimmed();

    const QSize s = m_canvas->cardSize();
    const int w = s.width();
    const int h = s.height();
    const int left = qMax(55, w / 16);
    const int top = qMax(70, h / 10);
    const int bottom = h - qMax(65, h / 11);
    const int tableTop = h - qMax(215, h / 3);
    const int rightTextX = w / 2 + w / 18;

    m_canvas->updateOrCreateQsoField("big_call", call,
                                     QPoint(left, top),
                                     overlayFont(qMax(54, h / 9), true),
                                     QColor(220, 0, 0));

    m_canvas->updateOrCreateQsoField("name", name,
                                     QPoint(left + 20, top + qMax(65, h / 10)),
                                     overlayFont(qMax(18, h / 32)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("qth", qth,
                                     QPoint(left + 20, top + qMax(100, h / 7)),
                                     overlayFont(qMax(18, h / 32)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("report", report,
                                     QPoint(left + 20, top + qMax(135, h / 5)),
                                     overlayFont(qMax(18, h / 32)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("table_header",
                                     "DATE          UTC          BAND        2-WAY        RST",
                                     QPoint(left + 20, tableTop),
                                     overlayFont(qMax(13, h / 54), true),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("table_values",
                                     "____/____     ______       ____        ____         ____",
                                     QPoint(left + 20, tableTop + qMax(40, h / 18)),
                                     overlayFont(qMax(15, h / 48)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("my_info",
                                     "IZ6NNH  SSTV QSO",
                                     QPoint(rightTextX, top + qMax(30, h / 16)),
                                     overlayFont(qMax(24, h / 28), true),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("location_left",
                                     qth,
                                     QPoint(left, bottom),
                                     overlayFont(qMax(24, h / 28)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("location_right",
                                     "Italy",
                                     QPoint(rightTextX, bottom),
                                     overlayFont(qMax(24, h / 28)),
                                     Qt::black);
}

QFont SstvImageEditorDialog::overlayFont(int pointSize, bool bold) const
{
    QFont font = m_fontCombo != nullptr ? m_fontCombo->currentFont() : QFont();
    font.setPointSize(pointSize);
    font.setBold(bold);
    return font;
}

void SstvImageEditorDialog::updateColorButton()
{
    if (m_btnColor == nullptr) {
        return;
    }

    m_btnColor->setStyleSheet(QString("background-color:%1;").arg(m_currentColor.name()));
    m_btnColor->setText(m_currentColor.name().toUpper());
}
