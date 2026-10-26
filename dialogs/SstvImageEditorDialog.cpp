#include "SstvImageEditorDialog.h"
#include "../modems/sstv/tx/SstvTransmitter.h"
#include "../utils/UiScale.h"
#include "../utils/RuntimeI18n.h"

#include <QCheckBox>
#include <QDataStream>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QListWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QSplitter>
#include <QFormLayout>
#include <QShortcut>
#include <QTimer>
#include <QResizeEvent>
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

namespace {
QString L18n(const QString &source)
{
    return MadModemI18n::text(source);
}
QString P18n(const QString &source)
{
    return MadModemI18n::placeholder(source);
}
}


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

    QByteArray document() const
    {
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_12);
        out << quint32(0x53535456) << quint32(2) << m_background << m_frameEnabled << m_suppressedFields << quint32(m_overlays.size());
        for (const auto &o : m_overlays)
            out << int(o.type) << o.text << o.image << o.pos << o.font << o.color << o.qsoField << o.qsoKey;
        return data;
    }

    bool restoreDocument(const QByteArray &data)
    {
        QDataStream in(data); in.setVersion(QDataStream::Qt_5_12);
        quint32 magic=0, version=0, count=0; QImage background; bool frame=false;
        in >> magic >> version;
        if (magic!=0x53535456 || (version!=1 && version!=2)) return false;
        QStringList suppressed;
        in >> background >> frame;
        if(version==2)in >> suppressed;
        in >> count;
        if (background.isNull() || background.width()>8192 || background.height()>8192 || count>200) return false;
        QVector<Overlay> overlays;
        for (quint32 i=0;i<count;++i) {
            Overlay o; int type=0;
            in >> type >> o.text >> o.image >> o.pos >> o.font >> o.color >> o.qsoField >> o.qsoKey;
            if (type<0 || type>1) return false;
            o.type=OverlayType(type); overlays.append(o);
        }
        if (in.status()!=QDataStream::Ok || !in.atEnd()) return false;
        m_background=background; m_frameEnabled=frame; m_overlays=overlays;m_suppressedFields=suppressed;
        m_selectedIndex=-1; syncSize(); update(); notifySelection(); if(changed)changed();return true;
    }

    void restoreFields() {m_suppressedFields.clear();}
    bool hasField(const QString &key) const {for(const auto &o:m_overlays)if(o.qsoKey==key)return true;return false;}
    QString field(const QString &key) const { for (const auto &o:m_overlays) if(o.qsoKey==key) return o.text;return {}; }
    QStringList elementNames() const { QStringList names;for(const auto &o:m_overlays) names << (o.type==OverlayType::Image?L18n("Logo / image"):o.text.isEmpty()?L18n("Empty text"):o.text);return names; }
    int selectedIndex() const { return m_selectedIndex; }
    void selectElement(int index) { setSelectedIndex(index);update(); }
    void beginEdit() { m_groupEdit=true; }
    void endEdit() { m_groupEdit=false;checkpoint(); }
    std::function<void()> changed;
    bool canUndo() const {return m_historyIndex>0;}
    bool canRedo() const {return m_historyIndex+1<m_history.size();}
    bool frameEnabled() const { return m_frameEnabled; }
    void setZoom(double zoom) { m_zoom=qBound(0.05,zoom,4.0); syncSize(); update(); }
    void checkpoint() {
        if(m_groupEdit)return;
        const QByteArray data=document();
        if (m_historyIndex>=0 && m_history.value(m_historyIndex)==data) return;
        while (m_history.size()>m_historyIndex+1) m_history.removeLast();
        m_history.append(data);
        qint64 bytes=0;for(const auto &item:m_history)bytes+=item.size();
        while (m_history.size()>1 && (m_history.size()>25 || bytes>64*1024*1024)) {bytes-=m_history.first().size();m_history.removeFirst();}
        m_historyIndex=m_history.size()-1;
        if(changed)changed();
    }
    void undo() { if (m_historyIndex>0) restoreDocument(m_history.at(--m_historyIndex)); }
    void redo() { if (m_historyIndex+1<m_history.size()) restoreDocument(m_history.at(++m_historyIndex)); }
    void clearHistory() { m_history.clear(); m_historyIndex=-1; checkpoint(); }

    void makeWhiteCard()
    {
        m_background = QImage(960, 744, QImage::Format_RGB32);
        m_background.fill(Qt::white);
        m_overlays.clear();
        m_suppressedFields.clear();
        m_selectedIndex = -1;
        syncSize();
        update();
        notifySelection();
        checkpoint();
    }

    void setBackgroundImage(const QImage &image)
    {
        if (image.isNull()) {
            makeWhiteCard();
            return;
        }

        m_background = image.convertToFormat(QImage::Format_RGB32);
        m_overlays.clear();
        m_suppressedFields.clear();
        m_selectedIndex = -1;
        syncSize();
        update();
        notifySelection();
        checkpoint();
    }

    void setFrameEnabled(bool enabled)
    {
        m_frameEnabled = enabled;
        update();
        checkpoint();
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
        checkpoint();
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
        checkpoint();
    }

    void deleteSelectedOverlay()
    {
        if (!hasSelection()) {
            return;
        }

        if(m_overlays.at(m_selectedIndex).qsoField)m_suppressedFields.append(m_overlays.at(m_selectedIndex).qsoKey);
        m_overlays.removeAt(m_selectedIndex);
        if (m_overlays.isEmpty()) {
            m_selectedIndex = -1;
        } else if (m_selectedIndex >= m_overlays.size()) {
            m_selectedIndex = m_overlays.size() - 1;
        }
        update();
        notifySelection();
        checkpoint();
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
        checkpoint();
    }

    void updateOrCreateQsoField(const QString &key,
                                const QString &text,
                                const QPoint &pos,
                                const QFont &font,
                                const QColor &color)
    {
        if(m_suppressedFields.contains(key))return;
        for (Overlay &overlay : m_overlays) {
            if (overlay.qsoField && overlay.qsoKey == key) {
                overlay.text = text;
                update();
                checkpoint();
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
        checkpoint();
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
        checkpoint();
    }

    void setSelectedFont(const QFont &font)
    {
        if (!selectedIsText()) {
            return;
        }
        m_overlays[m_selectedIndex].font = font;
        update();
        checkpoint();
    }

    void setSelectedColor(const QColor &color)
    {
        if (!selectedIsText()) {
            return;
        }
        m_overlays[m_selectedIndex].color = color;
        update();
        checkpoint();
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
        painter.scale(m_zoom, m_zoom);
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
        setSelectedIndex(hitTest((QPointF(event->pos()) / m_zoom).toPoint()));
        if (hasSelection()) {
            m_dragging = true;
            m_dragOffset = (QPointF(event->pos()) / m_zoom).toPoint() - m_overlays.at(m_selectedIndex).pos;
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
        QPoint pos = (QPointF(event->pos()) / m_zoom).toPoint() - m_dragOffset;
        const QRect rect = overlayRect(m_selectedIndex);
        const int w = rect.width();
        const int h = rect.height();
        pos.setX(qBound(0, pos.x(), qMax(0, m_background.width() - w)));
        pos.setY(qBound(overlay.type==OverlayType::Image ? 0 : h, pos.y(), qMax(overlay.type==OverlayType::Image ? 0 : h, m_background.height() - (overlay.type==OverlayType::Image ? h : 0))));
        overlay.pos = pos;
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
            checkpoint();
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
        setFixedSize((QSizeF(m_background.size()) * m_zoom).toSize());
    }

    void renderCard(QPainter *painter) const
    {
        if (m_frameEnabled) {
            const int margin=qMax(3,m_background.width()/60);
            painter->setPen(QPen(Qt::black,qMax(1,m_background.width()/240)));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(m_background.rect().adjusted(margin,margin,-margin,-margin));
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

    QStringList m_suppressedFields;
    bool m_groupEdit=false;
    double m_zoom=1.0;
    QVector<QByteArray> m_history;
    int m_historyIndex=-1;
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
    setWindowTitle(L18n(QStringLiteral("SSTV QSO / QSL image editor")));
    resize(MadModemUi::size(1100, 820));
    setMinimumSize(640, 480);

    m_canvas = new SstvEditorCanvas(this);
    m_canvas->setSelectionChangedCallback([this](bool hasSelection) {
        handleCanvasSelectionChanged(hasSelection);
    });

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_canvas);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setMinimumHeight(180);

    m_btnLoadBackground = new QPushButton(L18n(QStringLiteral("Load background...")), this);
    m_btnWhiteCard = new QPushButton(L18n(QStringLiteral("White card")), this);
    m_btnLoadLogo = new QPushButton(L18n(QStringLiteral("Load logo...")), this);
    m_chkFrame = new QCheckBox(L18n(QStringLiteral("Frame preset")), this);
    m_chkFrame->setChecked(true);
    m_btnSavePng = new QPushButton(L18n(QStringLiteral("Save PNG...")), this);
    m_btnUseForTx = new QPushButton(L18n(QStringLiteral("Use for SSTV TX")), this);
    QPushButton *btnClose = new QPushButton(L18n(QStringLiteral("Close")), this);

    for (QPushButton *button : {m_btnLoadBackground, m_btnWhiteCard, m_btnLoadLogo,
                                m_btnSavePng, m_btnUseForTx, btnClose}) {
        button->setMinimumHeight(30);
        button->setMinimumWidth(0);
    }

    m_editCall = new QLineEdit(this);
    m_editName = new QLineEdit(this);
    m_editQth = new QLineEdit(this);
    m_editReport = new QLineEdit(this);
    m_editCall->setPlaceholderText(P18n(QStringLiteral("Other station callsign, e.g. W6HN")));
    m_editName->setPlaceholderText(P18n(QStringLiteral("Name / operator")));
    m_editQth->setPlaceholderText(P18n(QStringLiteral("QTH / locator / county")));
    m_editReport->setPlaceholderText(P18n(QStringLiteral("RST / note / equipment")));

    m_editOverlayText = new QLineEdit(this);
    m_editOverlayText->setPlaceholderText(P18n(QStringLiteral("Selected/new text overlay")));
    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setMinimumContentsLength(10);
    m_fontCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_spinFontSize = new QSpinBox(this);
    m_spinFontSize->setRange(8, 144);
    m_spinFontSize->setValue(28);
    m_btnColor = new QPushButton(L18n(QStringLiteral("Color")), this);
    m_btnColor->setMinimumHeight(42);
    m_btnAddText = new QPushButton(L18n(QStringLiteral("Add text overlay")), this);
    m_btnDeleteSelected = new QPushButton(L18n(QStringLiteral("Delete selected")), this);
    for (QPushButton *button : {m_btnAddText, m_btnDeleteSelected}) {
        button->setMinimumHeight(30);
        button->setMinimumWidth(0);
    }
    m_btnDeleteSelected->setEnabled(false);

    m_btnTemplateCq = new QPushButton(L18n(QStringLiteral("CQ SSTV")), this);
    m_btnTemplateQsl = new QPushButton(L18n(QStringLiteral("QSL card template")), this);
    m_btnTemplate73 = new QPushButton("73", this);
    for (QPushButton *button : {m_btnTemplateCq, m_btnTemplateQsl, m_btnTemplate73}) {
        button->setMinimumHeight(30);
        button->setMinimumWidth(0);
    }

    QWidget *toolArea = new QWidget(this);
    QVBoxLayout *toolLayout = new QVBoxLayout(toolArea);
    toolLayout->setContentsMargins(8, 8, 8, 8);
    toolLayout->setSpacing(12);
    m_toolTabs=new QTabWidget(toolArea);
    m_toolTabs->setObjectName("sstvEditorTools");
    m_toolTabs->tabBar()->setStyleSheet("QTabBar::tab { min-width: 0px; padding: 5px 7px; }");
    m_toolTabs->tabBar()->setUsesScrollButtons(false);
    m_toolTabs->tabBar()->setElideMode(Qt::ElideNone);
    auto *imagePage=new QWidget(m_toolTabs),*qsoPage=new QWidget(m_toolTabs),*elementPage=new QWidget(m_toolTabs),*previewPage=new QWidget(m_toolTabs);
    auto *imageTools=new QVBoxLayout(imagePage),*qsoTools=new QVBoxLayout(qsoPage),*elementTools=new QVBoxLayout(elementPage),*previewTools=new QVBoxLayout(previewPage);
    m_toolTabs->addTab(imagePage,L18n("Card"));m_toolTabs->addTab(qsoPage,L18n("QSO"));
    m_toolTabs->addTab(elementPage,L18n("Elements"));m_toolTabs->addTab(previewPage,L18n("TX"));
    toolLayout->addWidget(m_toolTabs);


    QGridLayout *fileRow = new QGridLayout;
    fileRow->addWidget(m_btnLoadBackground,0,0);
    fileRow->addWidget(m_btnWhiteCard,1,0);
    fileRow->addWidget(m_btnLoadLogo,2,0);
    fileRow->addWidget(m_chkFrame,3,0);
    imageTools->addLayout(fileRow);

    QLabel *qsoTitle = new QLabel(L18n(QStringLiteral("SSTV QSO / QSL fields")), this);
    QFont sectionFont = qsoTitle->font();
    sectionFont.setBold(true);
    qsoTitle->setFont(sectionFont);
    qsoTools->addWidget(qsoTitle);

    QFormLayout *qsoGrid = new QFormLayout;
    qsoGrid->setRowWrapPolicy(QFormLayout::WrapAllRows);
    qsoGrid->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_editMyCall=new QLineEdit(this);
    qsoGrid->addRow(L18n("My callsign"),m_editMyCall);
    qsoGrid->addRow(L18n("Correspondent"),m_editCall);
    qsoGrid->addRow(L18n("Name"),m_editName);
    qsoGrid->addRow(L18n("QTH"),m_editQth);
    qsoGrid->addRow(L18n("Report / note"),m_editReport);
    qsoTools->addLayout(qsoGrid);

    QVBoxLayout *templateRow = new QVBoxLayout;
    templateRow->addWidget(new QLabel(L18n(QStringLiteral("Templates"))));
    templateRow->addWidget(m_btnTemplateCq);
    templateRow->addWidget(m_btnTemplateQsl);
    templateRow->addWidget(m_btnTemplate73);
    templateRow->addStretch(1);
    imageTools->addLayout(templateRow);

    m_elements=new QListWidget(this);
    m_elements->setObjectName("sstvElements");m_elements->setMaximumHeight(125);
    elementTools->addWidget(new QLabel(L18n("Elements"),this));
    elementTools->addWidget(m_elements);
    connect(m_elements,&QListWidget::currentRowChanged,this,[this](int row){m_canvas->selectElement(row);});
    QLabel *textTitle = new QLabel(L18n(QStringLiteral("Text overlay")), this);
    textTitle->setFont(sectionFont);
    elementTools->addWidget(textTitle);

    QFormLayout *textGrid = new QFormLayout;
    textGrid->setRowWrapPolicy(QFormLayout::WrapLongRows);
    textGrid->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    textGrid->addRow(L18n("Text"),m_editOverlayText);
    textGrid->addRow(L18n("Font"),m_fontCombo);
    textGrid->addRow(L18n("Size"),m_spinFontSize);
    textGrid->addRow(L18n("Color"),m_btnColor);
    textGrid->addRow(m_btnAddText);
    textGrid->addRow(m_btnDeleteSelected);
    elementTools->addLayout(textGrid);

    QLabel *hint = new QLabel(L18n("Select an element in the list or on the image. Drag to move it; edit text, font and color below. Save a project to keep all elements editable."), this);
    hint->setWordWrap(true);
    elementTools->addWidget(hint);

    m_txPreview=new QLabel(this);m_txPreview->setAlignment(Qt::AlignCenter);
    m_txPreview->setMinimumHeight(140);
    previewTools->addWidget(new QLabel(L18n("TX preview"),this));previewTools->addWidget(m_txPreview);
    imageTools->addStretch();qsoTools->addStretch();elementTools->addStretch();previewTools->addStretch();
    auto *mainLayout = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    auto *openProject=new QPushButton(L18n("Open project..."),this);
    auto *saveProject=new QPushButton(L18n("Save project..."),this);
    auto *undoButton=new QPushButton(L18n("Undo"),this);m_btnUndo=undoButton;
    auto *redoButton=new QPushButton(L18n("Redo"),this);m_btnRedo=redoButton;
    auto *fitButton=new QPushButton(L18n("Fit image"),this);
    auto *actualButton=new QPushButton("100%",this);
    for (auto *button : {openProject,saveProject,undoButton,redoButton,fitButton,actualButton}) top->addWidget(button);
    mainLayout->addLayout(top);
    auto *split=new QSplitter(Qt::Horizontal,this);
    split->addWidget(m_scrollArea);
    auto *toolsScroll=new QScrollArea(split);
    toolsScroll->setWidgetResizable(true); toolsScroll->setWidget(toolArea);
    toolsScroll->setMinimumWidth(300);
    split->addWidget(toolsScroll); split->setStretchFactor(0,1);split->setStretchFactor(1,0);
    split->setSizes({740,320});
    mainLayout->addWidget(split,1);
    auto *bottom=new QHBoxLayout;
    m_imageInfo=new QLabel(this);bottom->addWidget(m_imageInfo,1);
    bottom->addWidget(m_btnSavePng);bottom->addWidget(m_btnUseForTx);bottom->addWidget(btnClose);
    mainLayout->addLayout(bottom);
    connect(openProject,&QPushButton::clicked,this,&SstvImageEditorDialog::openProject);
    connect(saveProject,&QPushButton::clicked,this,&SstvImageEditorDialog::saveProject);
    connect(undoButton,&QPushButton::clicked,this,[this]{m_canvas->undo();syncDocumentControls();});
    connect(redoButton,&QPushButton::clicked,this,[this]{m_canvas->redo();syncDocumentControls();});
    connect(fitButton,&QPushButton::clicked,this,[this]{m_fitImage=true;fitImage();});
    connect(actualButton,&QPushButton::clicked,this,[this]{m_fitImage=false;m_canvas->setZoom(1.0);});
    auto *undoKey=new QShortcut(QKeySequence::Undo,this);
    connect(undoKey,&QShortcut::activated,undoButton,&QPushButton::click);
    auto *redoKey=new QShortcut(QKeySequence::Redo,this);
    connect(redoKey,&QShortcut::activated,redoButton,&QPushButton::click);
    updateColorButton();

    connect(m_btnLoadBackground, &QPushButton::clicked, this, &SstvImageEditorDialog::loadBackground);
    connect(m_btnWhiteCard, &QPushButton::clicked, this, &SstvImageEditorDialog::makeWhiteCard);
    connect(m_btnLoadLogo, &QPushButton::clicked, this, &SstvImageEditorDialog::loadLogo);
    connect(m_btnSavePng, &QPushButton::clicked, this, &SstvImageEditorDialog::savePng);
    connect(m_btnUseForTx, &QPushButton::clicked, this, &SstvImageEditorDialog::useForTx);
    connect(btnClose, &QPushButton::clicked, this, &SstvImageEditorDialog::reject);
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

    connect(m_editMyCall,&QLineEdit::textChanged,this,&SstvImageEditorDialog::handleQsoFieldsChanged);
    applyQslTemplate();
    m_canvas->changed=[this]{refreshElements();updateTxPreview();};
    m_canvas->clearHistory();m_savedDocument=projectData();
    refreshElements();
    MadModemUi::scaleWidgetTree(this);
    QTimer::singleShot(0,this,[this]{fitImage();});
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
    m_canvas->clearHistory();m_savedDocument=projectData();fitImage();
}

void SstvImageEditorDialog::loadBackground()
{
    if(!confirmDiscard())return;
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          L18n(QStringLiteral("Load SSTV background")),
                                                          QDir::homePath(),
                                                          L18n(QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All files (*)")));
    if (fileName.isEmpty()) {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, L18n(QStringLiteral("SSTV editor")), L18n(QStringLiteral("Unable to load the selected background image.")));
        return;
    }

    m_suggestedBaseName = QFileInfo(fileName).completeBaseName() + "_sstv";
    m_canvas->setBackgroundImage(image);
    applyQslTemplate();
    fitImage();
}

void SstvImageEditorDialog::makeWhiteCard()
{
    if(!confirmDiscard())return;
    m_canvas->makeWhiteCard();
    applyQslTemplate();
}

void SstvImageEditorDialog::loadLogo()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          L18n(QStringLiteral("Load logo")),
                                                          QDir::homePath(),
                                                          L18n(QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All files (*)")));
    if (fileName.isEmpty()) {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, L18n(QStringLiteral("SSTV editor")), L18n(QStringLiteral("Unable to load the selected logo image.")));
        return;
    }

    const QImage scaled = image.scaled(230, 230, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_canvas->addImageOverlay(scaled.scaled(m_canvas->cardSize()/3,Qt::KeepAspectRatio,Qt::SmoothTransformation), QPoint(m_canvas->cardSize().width()*2/3, 0));
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
                                                          L18n(QStringLiteral("Save SSTV PNG")),
                                                          suggestedSavePath(),
                                                          L18n(QStringLiteral("PNG image (*.png)")));
    if (fileName.isEmpty()) {
        return;
    }

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly) || !m_canvas->composedImage().save(&file, "PNG") || !file.commit()) {
        QMessageBox::warning(this, L18n(QStringLiteral("SSTV editor")), L18n(QStringLiteral("Unable to save the PNG image.")));
    }
}

void SstvImageEditorDialog::useForTx()
{
    const QImage image = m_canvas->composedImage();
    if (image.isNull()) {
        QMessageBox::warning(this, L18n(QStringLiteral("SSTV editor")), L18n(QStringLiteral("There is no image to send.")));
        return;
    }

    emit imageReadyForTx(image, suggestedSavePath());
    accept();
}

void SstvImageEditorDialog::addTextOverlay()
{
    QString text = m_editOverlayText->text().trimmed();
    if (text.isEmpty()) {
        text = "CQ SSTV";
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
    const QSize size=m_canvas->cardSize();
    const QString own=m_editMyCall->text().trimmed().toUpper();
    m_canvas->updateOrCreateQsoField("cq",own.isEmpty()?QString("CQ SSTV"):QString("CQ SSTV de %1").arg(own),
        QPoint(size.width()/12,size.height()/3),overlayFont(qMax(16,size.height()/14),true),QColor(220,0,0));
}

void SstvImageEditorDialog::applyQslTemplate()
{
    m_canvas->restoreFields();
    rebuildPreviewFromFields();
}

void SstvImageEditorDialog::apply73Template()
{
    const QSize size=m_canvas->cardSize();
    m_canvas->updateOrCreateQsoField("73","73!",QPoint(size.width()*2/3,size.height()*4/5),overlayFont(qMax(16,size.height()/9),true),Qt::black);
}

void SstvImageEditorDialog::updateSelectedOverlayText()
{
    if (m_updatingUi || !m_canvas->selectedIsText()) {
        return;
    }

    QFont font = m_fontCombo->currentFont();
    font.setPointSize(m_spinFontSize->value());
    m_canvas->beginEdit();
    m_canvas->setSelectedText(m_editOverlayText->text());
    m_canvas->setSelectedFont(font);
    m_canvas->setSelectedColor(m_currentColor);
    m_canvas->endEdit();
    syncDocumentControls();
}

void SstvImageEditorDialog::chooseTextColor()
{
    const QColor color = QColorDialog::getColor(
        m_currentColor, this, L18n(QStringLiteral("Choose text color")));
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
    if(hasSelection && m_toolTabs)m_toolTabs->setCurrentIndex(2);
    if(m_elements){QSignalBlocker b(m_elements);m_elements->setCurrentRow(m_canvas->selectedIndex());}

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
    if (!m_updatingUi) rebuildPreviewFromFields();
}

void SstvImageEditorDialog::rebuildPreviewFromFields()
{
    m_canvas->beginEdit();
    const QString call = m_editCall->text().trimmed().isEmpty() ? QString() : m_editCall->text().trimmed();
    const QString name = m_editName->text().trimmed().isEmpty() ? QString() : m_editName->text().trimmed();
    const QString qth = m_editQth->text().trimmed().isEmpty() ? QString() : m_editQth->text().trimmed();
    const QString report = m_editReport->text().trimmed().isEmpty() ? QString() : m_editReport->text().trimmed();

    const QSize s = m_canvas->cardSize();
    const int w = s.width();
    const int h = s.height();
    const int left = qMax(8, w / 16);
    const int top = qMax(20, h / 8);
    const int rightTextX = w / 2 + w / 18;

    m_canvas->updateOrCreateQsoField("big_call", call,
                                     QPoint(left, top),
                                     overlayFont(qMax(12, h / 15), true),
                                     QColor(220, 0, 0));

    m_canvas->updateOrCreateQsoField("name", name,
                                     QPoint(left + w/40, top + h / 9),
                                     overlayFont(qMax(9, h / 34)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("qth", qth,
                                     QPoint(left + w/40, top + h / 6),
                                     overlayFont(qMax(9, h / 34)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("report", report,
                                     QPoint(left + w/40, top + h / 4),
                                     overlayFont(qMax(9, h / 34)),
                                     Qt::black);

    m_canvas->updateOrCreateQsoField("my_info",
                                     m_editMyCall->text().trimmed().toUpper(),
                                     QPoint(rightTextX, top + qMax(30, h / 16)),
                                     overlayFont(qMax(10, h / 34), true),
                                     Qt::black);

    if(!m_canvas->field("cq").isEmpty()) {
        const QString own=m_editMyCall->text().trimmed().toUpper();
        m_canvas->updateOrCreateQsoField("cq",own.isEmpty()?QString("CQ SSTV"):QString("CQ SSTV de %1").arg(own),
            QPoint(w/12,h/3),overlayFont(qMax(16,h/14),true),QColor(220,0,0));
    }
    m_canvas->endEdit();
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

    m_btnColor->setStyleSheet(QString("background-color:%1;color:%2;").arg(m_currentColor.name(),m_currentColor.lightness()<128?QString("white"):QString("black")));
    m_btnColor->setText(m_currentColor.name().toUpper());
}

QByteArray SstvImageEditorDialog::projectData() const
{
    QByteArray data;QDataStream out(&data,QIODevice::WriteOnly);out.setVersion(QDataStream::Qt_5_12);
    out << quint32(0x4d4d5331) << m_canvas->document() << m_editMyCall->text() << m_editCall->text()
        << m_editName->text() << m_editQth->text() << m_editReport->text();
    return data;
}

bool SstvImageEditorDialog::restoreProject(const QByteArray &data)
{
    if (data.size()>64*1024*1024) return false;
    QDataStream in(data);in.setVersion(QDataStream::Qt_5_12);
    quint32 magic;QByteArray canvas;QString own,call,name,qth,report;
    in >> magic >> canvas >> own >> call >> name >> qth >> report;
    if (magic!=0x4d4d5331 || in.status()!=QDataStream::Ok || !in.atEnd()) return false;
    if (!m_canvas->restoreDocument(canvas)) return false;
    m_updatingUi=true;
    m_editMyCall->setText(own);m_editCall->setText(call);m_editName->setText(name);
    m_editQth->setText(qth);m_editReport->setText(report);
    m_updatingUi=false;m_canvas->clearHistory();syncDocumentControls();
    m_savedDocument=projectData();fitImage();return true;
}

void SstvImageEditorDialog::setQsoFields(const QString &own,const QString &call,const QString &name,const QString &qth,const QString &report)
{
    m_updatingUi=true;m_editMyCall->setText(own);m_editCall->setText(call);m_editName->setText(name);
    m_editQth->setText(qth);m_editReport->setText(report);m_updatingUi=false;
    rebuildPreviewFromFields();m_canvas->clearHistory();m_savedDocument=projectData();
}

void SstvImageEditorDialog::fitImage()
{
    const QSize image=m_canvas->cardSize();
    updateTxPreview();
    if (!m_fitImage || image.isEmpty()) return;
    const QSize view=m_scrollArea->viewport()->size()-QSize(8,8);
    m_canvas->setZoom(qMin(double(view.width())/image.width(),double(view.height())/image.height()));
}
void SstvImageEditorDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);QTimer::singleShot(0,this,[this]{fitImage();});
}
void SstvImageEditorDialog::syncDocumentControls()
{
    QSignalBlocker blocker(m_chkFrame);m_chkFrame->setChecked(m_canvas->frameEnabled());
    m_updatingUi=true;
    if(m_canvas->hasField("my_info"))m_editMyCall->setText(m_canvas->field("my_info"));
    if(m_canvas->hasField("big_call"))m_editCall->setText(m_canvas->field("big_call"));
    if(m_canvas->hasField("name"))m_editName->setText(m_canvas->field("name"));
    if(m_canvas->hasField("qth"))m_editQth->setText(m_canvas->field("qth"));
    if(m_canvas->hasField("report"))m_editReport->setText(m_canvas->field("report"));
    m_updatingUi=false;
    refreshElements();fitImage();
}
void SstvImageEditorDialog::openProject()
{
    if (!confirmDiscard()) return;
    const QString path=QFileDialog::getOpenFileName(this,L18n("Open project..."),QString(),"MadModem SSTV (*.mmsstv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size()>64*1024*1024 || !restoreProject(file.readAll()))
        QMessageBox::warning(this,L18n("SSTV editor"),L18n("Unable to open SSTV project."));
}
void SstvImageEditorDialog::saveProject()
{
    const QString path=QFileDialog::getSaveFileName(this,L18n("Save project..."),"SSTV.mmsstv","MadModem SSTV (*.mmsstv)");
    if (path.isEmpty()) return;
    QSaveFile file(path);const QByteArray data=projectData();
    if (!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) {
        QMessageBox::warning(this,L18n("SSTV editor"),L18n("Unable to save SSTV project."));return;
    }
    m_savedDocument=projectData();
}
bool SstvImageEditorDialog::confirmDiscard()
{
    return projectData()==m_savedDocument || QMessageBox::question(this,L18n("SSTV editor"),
        L18n("Discard unsaved image changes?"),QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Discard;
}
void SstvImageEditorDialog::reject() { if (confirmDiscard()) QDialog::reject(); }

void SstvImageEditorDialog::refreshElements()
{
    if(!m_elements)return;
    if(m_btnUndo)m_btnUndo->setEnabled(m_canvas->canUndo());
    if(m_btnRedo)m_btnRedo->setEnabled(m_canvas->canRedo());
    QSignalBlocker blocker(m_elements);m_elements->clear();m_elements->addItems(m_canvas->elementNames());
    m_elements->setCurrentRow(m_canvas->selectedIndex());
}
QImage SstvImageEditorDialog::composedImage() const {return m_canvas->composedImage();}
QStringList SstvImageEditorDialog::qsoFields() const {
    return {m_editMyCall->text(),m_editCall->text(),m_editName->text(),m_editQth->text(),m_editReport->text()};
}

void SstvImageEditorDialog::setTxMode(const QString &mode) {m_txMode=mode;updateTxPreview();}
void SstvImageEditorDialog::updateTxPreview()
{
    if(!m_txPreview || !m_imageInfo)return;
    const QImage prepared=SstvTransmitter::prepareImage(m_canvas->composedImage(),m_txMode);
    m_txPreview->setPixmap(QPixmap::fromImage(prepared).scaled(240,160,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    const QSize source=m_canvas->cardSize();
    m_imageInfo->setText(QString("%1 × %2 px → %3 · %4 × %5 px").arg(source.width()).arg(source.height()).arg(m_txMode).arg(prepared.width()).arg(prepared.height()));
}
