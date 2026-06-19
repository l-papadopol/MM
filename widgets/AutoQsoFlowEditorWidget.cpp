#include "AutoQsoFlowEditorWidget.h"
#include "../utils/RuntimeI18n.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGraphicsObject>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QShortcut>
#include <QStyle>
#include <QSet>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr qreal kRectNodeWidth = 250.0;
constexpr qreal kRectNodeHeight = 78.0;
constexpr qreal kDecisionNodeWidth = 250.0;
constexpr qreal kDecisionNodeHeight = 118.0;
constexpr qreal kTerminalNodeWidth = 250.0;
constexpr qreal kTerminalNodeHeight = 72.0;
constexpr qreal kTimerNodeWidth = 250.0;
constexpr qreal kTimerNodeHeight = 84.0;
constexpr qreal kWideNodeWidth = 270.0;
constexpr qreal kWideNodeHeight = 88.0;

QString normalizedPortLabel(const QString &port)
{
    const QString p = port.trimmed().toLower();
    if (p == QLatin1String("yes")) {
        return MadModemI18n::text(QStringLiteral("Yes"));
    }
    if (p == QLatin1String("no")) {
        return MadModemI18n::text(QStringLiteral("No"));
    }
    if (p == QLatin1String("ok")) {
        return MadModemI18n::text(QStringLiteral("OK"));
    }
    if (p == QLatin1String("retry")) {
        return MadModemI18n::text(QStringLiteral("Retry"));
    }
    return port;
}

QColor kindFillColour(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return QColor(225, 240, 255);
    }
    if (kind == QLatin1String("condition")) {
        return QColor(255, 243, 214);
    }
    if (kind == QLatin1String("action")) {
        return QColor(224, 244, 228);
    }
    if (kind == QLatin1String("timer")) {
        return QColor(237, 230, 252);
    }
    if (kind == QLatin1String("terminal")) {
        return QColor(252, 231, 231);
    }
    if (kind == QLatin1String("variable")) {
        return QColor(226, 246, 244);
    }
    if (kind == QLatin1String("compare")) {
        return QColor(255, 240, 226);
    }
    if (kind == QLatin1String("loop")) {
        return QColor(232, 238, 255);
    }
    if (kind == QLatin1String("io")) {
        return QColor(240, 245, 220);
    }
    if (kind == QLatin1String("math")) {
        return QColor(238, 234, 246);
    }
    return QColor(239, 239, 239);
}

QColor kindBorderColour(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return QColor(71, 132, 218);
    }
    if (kind == QLatin1String("condition")) {
        return QColor(205, 141, 30);
    }
    if (kind == QLatin1String("action")) {
        return QColor(66, 148, 78);
    }
    if (kind == QLatin1String("timer")) {
        return QColor(123, 94, 190);
    }
    if (kind == QLatin1String("terminal")) {
        return QColor(185, 84, 84);
    }
    if (kind == QLatin1String("variable")) {
        return QColor(38, 139, 130);
    }
    if (kind == QLatin1String("compare")) {
        return QColor(196, 103, 32);
    }
    if (kind == QLatin1String("loop")) {
        return QColor(73, 91, 182);
    }
    if (kind == QLatin1String("io")) {
        return QColor(116, 136, 34);
    }
    if (kind == QLatin1String("math")) {
        return QColor(107, 83, 158);
    }
    return QColor(120, 120, 120);
}

QString kindBadge(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return MadModemI18n::text(QStringLiteral("EVENT"));
    }
    if (kind == QLatin1String("condition")) {
        return MadModemI18n::text(QStringLiteral("DECISION"));
    }
    if (kind == QLatin1String("action")) {
        return MadModemI18n::text(QStringLiteral("ACTION"));
    }
    if (kind == QLatin1String("timer")) {
        return MadModemI18n::text(QStringLiteral("TIMER"));
    }
    if (kind == QLatin1String("terminal")) {
        return MadModemI18n::text(QStringLiteral("END"));
    }
    if (kind == QLatin1String("variable")) {
        return MadModemI18n::text(QStringLiteral("VAR"));
    }
    if (kind == QLatin1String("compare")) {
        return MadModemI18n::text(QStringLiteral("COMPARE"));
    }
    if (kind == QLatin1String("loop")) {
        return MadModemI18n::text(QStringLiteral("LOOP"));
    }
    if (kind == QLatin1String("io")) {
        return MadModemI18n::text(QStringLiteral("I/O"));
    }
    if (kind == QLatin1String("math")) {
        return MadModemI18n::text(QStringLiteral("MATH"));
    }
    return MadModemI18n::text(QStringLiteral("STEP"));
}

QString kindDisplayName(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return MadModemI18n::text(QStringLiteral("Event"));
    }
    if (kind == QLatin1String("condition")) {
        return MadModemI18n::text(QStringLiteral("Decision"));
    }
    if (kind == QLatin1String("action")) {
        return MadModemI18n::text(QStringLiteral("Action"));
    }
    if (kind == QLatin1String("timer")) {
        return MadModemI18n::text(QStringLiteral("Timer"));
    }
    if (kind == QLatin1String("terminal")) {
        return MadModemI18n::text(QStringLiteral("Terminal"));
    }
    if (kind == QLatin1String("variable")) {
        return MadModemI18n::text(QStringLiteral("Variable"));
    }
    if (kind == QLatin1String("compare")) {
        return MadModemI18n::text(QStringLiteral("Compare"));
    }
    if (kind == QLatin1String("loop")) {
        return MadModemI18n::text(QStringLiteral("Loop"));
    }
    if (kind == QLatin1String("io")) {
        return MadModemI18n::text(QStringLiteral("Input / Output"));
    }
    if (kind == QLatin1String("math")) {
        return MadModemI18n::text(QStringLiteral("Math"));
    }
    return MadModemI18n::text(QStringLiteral("Step"));
}

QString sanitizeId(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text.trimmed().toLower()) {
        if (ch.isLetterOrNumber()) {
            out.append(ch);
        } else if (ch == QLatin1Char('_') || ch == QLatin1Char('-') || ch.isSpace()) {
            if (!out.endsWith(QLatin1Char('_'))) {
                out.append(QLatin1Char('_'));
            }
        }
    }
    while (out.startsWith(QLatin1Char('_'))) {
        out.remove(0, 1);
    }
    while (out.endsWith(QLatin1Char('_'))) {
        out.chop(1);
    }
    if (out.isEmpty()) {
        out = QStringLiteral("node");
    }
    return out;
}

QString defaultTypeForKind(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return QStringLiteral("event.newDecode");
    }
    if (kind == QLatin1String("condition")) {
        return QStringLiteral("condition.custom");
    }
    if (kind == QLatin1String("timer")) {
        return QStringLiteral("timer.waitSlot");
    }
    if (kind == QLatin1String("terminal")) {
        return QStringLiteral("terminal.resumeOrStop");
    }
    if (kind == QLatin1String("variable")) {
        return QStringLiteral("variable.set");
    }
    if (kind == QLatin1String("compare")) {
        return QStringLiteral("compare.equal");
    }
    if (kind == QLatin1String("loop")) {
        return QStringLiteral("loop.forEach");
    }
    if (kind == QLatin1String("io")) {
        return QStringLiteral("io.popupMessage");
    }
    if (kind == QLatin1String("math")) {
        return QStringLiteral("math.add");
    }
    return QStringLiteral("action.custom");
}

QString defaultTitleForKind(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return MadModemI18n::text(QStringLiteral("New event"));
    }
    if (kind == QLatin1String("condition")) {
        return MadModemI18n::text(QStringLiteral("New decision"));
    }
    if (kind == QLatin1String("timer")) {
        return MadModemI18n::text(QStringLiteral("New timer"));
    }
    if (kind == QLatin1String("terminal")) {
        return MadModemI18n::text(QStringLiteral("New end state"));
    }
    if (kind == QLatin1String("variable")) {
        return MadModemI18n::text(QStringLiteral("Set variable"));
    }
    if (kind == QLatin1String("compare")) {
        return MadModemI18n::text(QStringLiteral("Compare values"));
    }
    if (kind == QLatin1String("loop")) {
        return MadModemI18n::text(QStringLiteral("Loop / iterate"));
    }
    if (kind == QLatin1String("io")) {
        return MadModemI18n::text(QStringLiteral("Show / ask user"));
    }
    if (kind == QLatin1String("math")) {
        return MadModemI18n::text(QStringLiteral("Calculate value"));
    }
    return MadModemI18n::text(QStringLiteral("New action"));
}

QString defaultNoteForKind(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return MadModemI18n::text(QStringLiteral("Trigger for the AutoQSO flow."));
    }
    if (kind == QLatin1String("condition")) {
        return MadModemI18n::text(QStringLiteral("Branch according to current QSO state."));
    }
    if (kind == QLatin1String("timer")) {
        return MadModemI18n::text(QStringLiteral("Wait for a valid FT slot or retry window."));
    }
    if (kind == QLatin1String("terminal")) {
        return MadModemI18n::text(QStringLiteral("Stop this branch and return control safely."));
    }
    if (kind == QLatin1String("variable")) {
        return MadModemI18n::text(QStringLiteral("Create, update or clear a named flow variable."));
    }
    if (kind == QLatin1String("compare")) {
        return MadModemI18n::text(QStringLiteral("Compare variables, constants or decoded-message fields."));
    }
    if (kind == QLatin1String("loop")) {
        return MadModemI18n::text(QStringLiteral("Repeat a branch over decodes, candidates, rows or a bounded counter."));
    }
    if (kind == QLatin1String("io")) {
        return MadModemI18n::text(QStringLiteral("Ask for keyboard input or show a popup/status message."));
    }
    if (kind == QLatin1String("math")) {
        return MadModemI18n::text(QStringLiteral("Compute arithmetic, distance, bearing or formatted values."));
    }
    return MadModemI18n::text(QStringLiteral("Emit an abstract AutoQSO or app action."));
}

QStringList typeSuggestionsForKind(const QString &kind)
{
    if (kind == QLatin1String("event")) {
        return {
            QStringLiteral("event.newDecode"),
            QStringLiteral("event.cqPressed"),
            QStringLiteral("event.slotEnded"),
            QStringLiteral("event.noResponse"),
            QStringLiteral("event.qsoCompleted")
        };
    }
    if (kind == QLatin1String("condition")) {
        return {
            QStringLiteral("condition.blacklisted"),
            QStringLiteral("condition.activeTarget"),
            QStringLiteral("condition.cqCandidate"),
            QStringLiteral("condition.dupePolicy"),
            QStringLiteral("condition.qsoComplete"),
            QStringLiteral("condition.txAllowed"),
            QStringLiteral("condition.retryAvailable"),
            QStringLiteral("condition.custom")
        };
    }
    if (kind == QLatin1String("timer")) {
        return {
            QStringLiteral("timer.waitSlot"),
            QStringLiteral("timer.waitDecodeWindow"),
            QStringLiteral("timer.retryDelay"),
            QStringLiteral("timer.delayMs"),
            QStringLiteral("timer.delaySeconds"),
            QStringLiteral("timer.interval"),
            QStringLiteral("timer.onceAtUtc"),
            QStringLiteral("timer.everyFtSlot"),
            QStringLiteral("timer.watchdog"),
            QStringLiteral("timer.custom")
        };
    }
    if (kind == QLatin1String("variable")) {
        return {
            QStringLiteral("variable.set"),
            QStringLiteral("variable.copy"),
            QStringLiteral("variable.increment"),
            QStringLiteral("variable.clear"),
            QStringLiteral("variable.appendList"),
            QStringLiteral("variable.mapPut"),
            QStringLiteral("variable.mapGet"),
            QStringLiteral("variable.custom")
        };
    }
    if (kind == QLatin1String("compare")) {
        return {
            QStringLiteral("compare.equal"),
            QStringLiteral("compare.notEqual"),
            QStringLiteral("compare.less"),
            QStringLiteral("compare.greater"),
            QStringLiteral("compare.contains"),
            QStringLiteral("compare.regex"),
            QStringLiteral("compare.isEmpty"),
            QStringLiteral("compare.band"),
            QStringLiteral("compare.mode"),
            QStringLiteral("compare.custom")
        };
    }
    if (kind == QLatin1String("loop")) {
        return {
            QStringLiteral("loop.forEachDecode"),
            QStringLiteral("loop.forEachCandidate"),
            QStringLiteral("loop.forEachLogRow"),
            QStringLiteral("loop.repeatCount"),
            QStringLiteral("loop.whileCondition"),
            QStringLiteral("loop.break"),
            QStringLiteral("loop.continue"),
            QStringLiteral("loop.custom")
        };
    }
    if (kind == QLatin1String("io")) {
        return {
            QStringLiteral("io.keyboardInput"),
            QStringLiteral("io.popupMessage"),
            QStringLiteral("io.popupQuestion"),
            QStringLiteral("io.statusMessage"),
            QStringLiteral("io.logMessage"),
            QStringLiteral("io.tableOutput"),
            QStringLiteral("io.custom")
        };
    }
    if (kind == QLatin1String("math")) {
        return {
            QStringLiteral("math.add"),
            QStringLiteral("math.subtract"),
            QStringLiteral("math.multiply"),
            QStringLiteral("math.divide"),
            QStringLiteral("math.round"),
            QStringLiteral("math.distanceBearing"),
            QStringLiteral("math.gridDistance"),
            QStringLiteral("math.custom")
        };
    }
    if (kind == QLatin1String("terminal")) {
        return {
            QStringLiteral("terminal.resumeOrStop"),
            QStringLiteral("terminal.rxIdle"),
            QStringLiteral("terminal.abortBranch"),
            QStringLiteral("terminal.custom")
        };
    }
    return {
        QStringLiteral("action.chooseTxFrequency"),
        QStringLiteral("action.prepareReply"),
        QStringLiteral("action.armTx"),
        QStringLiteral("action.reclaimTarget"),
        QStringLiteral("action.logQso"),
        QStringLiteral("action.resumeCq"),
        QStringLiteral("action.changeMode"),
        QStringLiteral("action.changeBand"),
        QStringLiteral("action.setFrequency"),
        QStringLiteral("action.startRx"),
        QStringLiteral("action.stopRx"),
        QStringLiteral("action.saveFile"),
        QStringLiteral("action.custom")
    };
}

QJsonObject makeNode(const QString &id,
                     const QString &kind,
                     const QString &type,
                     const QString &title,
                     const QString &note,
                     int x,
                     int y)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("kind"), kind);
    obj.insert(QStringLiteral("type"), type);
    obj.insert(QStringLiteral("title"), title);
    obj.insert(QStringLiteral("note"), note);
    obj.insert(QStringLiteral("x"), x);
    obj.insert(QStringLiteral("y"), y);
    return obj;
}

QJsonObject makeEdge(const QString &from, const QString &to, const QString &port = QString())
{
    QJsonObject obj;
    obj.insert(QStringLiteral("from"), from);
    obj.insert(QStringLiteral("to"), to);
    if (!port.isEmpty()) {
        obj.insert(QStringLiteral("port"), port);
    }
    return obj;
}

QJsonObject migratedLegacyFlowObject(const QJsonObject &root)
{
    QJsonObject migrated = root;
    migrated.insert(QStringLiteral("schema"), 2);
    migrated.insert(QStringLiteral("visualStyle"), QStringLiteral("flowgorithm"));

    const QJsonArray legacyNodes = root.value(QStringLiteral("nodes")).toArray();
    QJsonArray nodes;
    for (const QJsonValue &value : legacyNodes) {
        QJsonObject node = value.toObject();
        const QString id = node.value(QStringLiteral("id")).toString();
        if (id == QLatin1String("decode")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 30; }
        else if (id == QLatin1String("is_blacklisted")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 160; }
        else if (id == QLatin1String("is_target")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 360; }
        else if (id == QLatin1String("is_cq")) { node[QStringLiteral("x")] = 760; node[QStringLiteral("y")] = 360; }
        else if (id == QLatin1String("dupe_policy")) { node[QStringLiteral("x")] = 760; node[QStringLiteral("y")] = 560; }
        else if (id == QLatin1String("choose_tx")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 560; }
        else if (id == QLatin1String("prepare_reply")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 700; }
        else if (id == QLatin1String("wait_slot")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 830; }
        else if (id == QLatin1String("transmit")) { node[QStringLiteral("x")] = 440; node[QStringLiteral("y")] = 960; }
        else if (id == QLatin1String("timeout")) { node[QStringLiteral("x")] = 760; node[QStringLiteral("y")] = 960; }
        else if (id == QLatin1String("reclaim")) { node[QStringLiteral("x")] = 760; node[QStringLiteral("y")] = 1090; }
        else if (id == QLatin1String("completed")) { node[QStringLiteral("x")] = 120; node[QStringLiteral("y")] = 360; }
        else if (id == QLatin1String("log_qso")) { node[QStringLiteral("x")] = 120; node[QStringLiteral("y")] = 560; }
        else if (id == QLatin1String("resume")) { node[QStringLiteral("x")] = 120; node[QStringLiteral("y")] = 700; }
        nodes.append(node);
    }
    migrated.insert(QStringLiteral("nodes"), nodes);
    return migrated;
}

QJsonObject compactFlowObject(const QJsonObject &root)
{
    const int schema = root.value(QStringLiteral("schema")).toInt(1);
    const QString visualStyle = root.value(QStringLiteral("visualStyle")).toString();
    if (schema >= 2 && visualStyle == QLatin1String("flowgorithm")) {
        return root;
    }
    return migratedLegacyFlowObject(root);
}

class FlowEdgeItem;

class FlowNodeItem final : public QGraphicsObject
{
public:
    FlowNodeItem(const QJsonObject &node, QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent),
          m_id(node.value(QStringLiteral("id")).toString()),
          m_kind(node.value(QStringLiteral("kind")).toString(QStringLiteral("action"))),
          m_type(node.value(QStringLiteral("type")).toString()),
          m_title(node.value(QStringLiteral("title")).toString(m_id)),
          m_note(node.value(QStringLiteral("note")).toString())
    {
        setFlags(QGraphicsItem::ItemIsMovable |
                 QGraphicsItem::ItemIsSelectable |
                 QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setZValue(10.0);
        setPos(node.value(QStringLiteral("x")).toDouble(),
               node.value(QStringLiteral("y")).toDouble());
    }

    QString id() const { return m_id; }
    QString kind() const { return m_kind; }
    QString nodeType() const { return m_type; }
    QString title() const { return m_title; }
    QString note() const { return m_note; }

    void setNodeData(const QString &id,
                     const QString &kind,
                     const QString &type,
                     const QString &title,
                     const QString &note)
    {
        const bool geometryMayChange = (kind != m_kind);
        if (geometryMayChange) {
            prepareGeometryChange();
        }
        m_id = id;
        m_kind = kind;
        m_type = type;
        m_title = title;
        m_note = note;
        update();
        updateEdges();
    }

    QRectF boundingRect() const override
    {
        const QSizeF s = nodeSize();
        return QRectF(0.0, 0.0, s.width(), s.height());
    }

    QPointF inputAnchor() const
    {
        const QRectF r = boundingRect();
        return mapToScene(QPointF(r.center().x(), r.top()));
    }

    QPointF outputAnchor(const QString &port = QString()) const
    {
        const QRectF r = boundingRect();
        if (m_kind == QLatin1String("condition") || m_kind == QLatin1String("compare") || m_kind == QLatin1String("loop")) {
            const QString p = port.trimmed().toLower();
            if (p == QLatin1String("yes") || p == QLatin1String("true") || p == QLatin1String("ok") ||
                p == QLatin1String("retry") || p == QLatin1String("loop") || p == QLatin1String("next")) {
                return mapToScene(QPointF(r.right(), r.center().y()));
            }
            if (p == QLatin1String("no") || p == QLatin1String("false") || p == QLatin1String("done") ||
                p == QLatin1String("exit")) {
                return mapToScene(QPointF(r.left(), r.center().y()));
            }
        }
        return mapToScene(QPointF(r.center().x(), r.bottom()));
    }

    void addEdge(FlowEdgeItem *edge)
    {
        if (edge != nullptr && !m_edges.contains(edge)) {
            m_edges.append(edge);
        }
    }

    void removeEdge(FlowEdgeItem *edge)
    {
        m_edges.removeAll(edge);
    }

    QList<FlowEdgeItem *> edges() const { return m_edges; }

    void updateEdges();

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), m_id);
        obj.insert(QStringLiteral("kind"), m_kind);
        obj.insert(QStringLiteral("type"), m_type);
        obj.insert(QStringLiteral("title"), m_title);
        obj.insert(QStringLiteral("note"), m_note);
        obj.insert(QStringLiteral("x"), qRound(pos().x()));
        obj.insert(QStringLiteral("y"), qRound(pos().y()));
        return obj;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = boundingRect().adjusted(1.0, 1.0, -1.0, -1.0);

        const QColor fill = isSelected() ? kindFillColour(m_kind).darker(104)
                                         : kindFillColour(m_kind);
        const QColor border = isSelected() ? kindBorderColour(m_kind).darker(110)
                                           : kindBorderColour(m_kind);

        painter->setPen(QPen(border, isSelected() ? 2.8 : 2.0));
        painter->setBrush(fill);
        painter->drawPath(shapePath().translated(1.0, 1.0));

        QRectF headerRect = r.adjusted(10.0, 10.0, -10.0, 0.0);
        headerRect.setHeight(20.0);
        painter->setPen(Qt::NoPen);
        painter->setBrush(border);
        painter->drawRoundedRect(QRectF(headerRect.left(), headerRect.top(), 72.0, 18.0), 9.0, 9.0);

        QFont badgeFont = painter->font();
        badgeFont.setBold(true);
        badgeFont.setPointSize(qMax(7, badgeFont.pointSize() - 2));
        painter->setFont(badgeFont);
        painter->setPen(Qt::white);
        painter->drawText(QRectF(headerRect.left(), headerRect.top(), 72.0, 18.0), Qt::AlignCenter, kindBadge(m_kind));

        QFont titleFont = painter->font();
        titleFont.setBold(true);
        titleFont.setPointSize(qMax(10, titleFont.pointSize()));
        painter->setFont(titleFont);
        painter->setPen(QColor(32, 32, 32));
        painter->drawText(QRectF(r.left() + 12.0, r.top() + 34.0, r.width() - 24.0, 22.0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          m_title);

        QFont noteFont = painter->font();
        noteFont.setBold(false);
        noteFont.setPointSize(qMax(8, noteFont.pointSize() - 1));
        painter->setFont(noteFont);
        painter->setPen(QColor(55, 55, 55));
        painter->drawText(QRectF(r.left() + 12.0, r.top() + 56.0, r.width() - 24.0, r.height() - 64.0),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          m_note);

        painter->setPen(QPen(border, 1.5));
        painter->setBrush(Qt::white);
        const QPointF in = mapFromScene(inputAnchor());
        const QPointF out = mapFromScene(outputAnchor());
        painter->drawEllipse(in, 4.0, 4.0);
        if (m_kind == QLatin1String("condition") || m_kind == QLatin1String("compare") || m_kind == QLatin1String("loop")) {
            painter->drawEllipse(mapFromScene(outputAnchor(QStringLiteral("yes"))), 4.0, 4.0);
            painter->drawEllipse(mapFromScene(outputAnchor(QStringLiteral("no"))), 4.0, 4.0);
        } else {
            painter->drawEllipse(out, 4.0, 4.0);
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QSizeF nodeSize() const
    {
        if (m_kind == QLatin1String("condition") || m_kind == QLatin1String("compare")) {
            return QSizeF(kDecisionNodeWidth, kDecisionNodeHeight);
        }
        if (m_kind == QLatin1String("terminal")) {
            return QSizeF(kTerminalNodeWidth, kTerminalNodeHeight);
        }
        if (m_kind == QLatin1String("timer") || m_kind == QLatin1String("loop")) {
            return QSizeF(kTimerNodeWidth, kTimerNodeHeight);
        }
        if (m_kind == QLatin1String("variable") || m_kind == QLatin1String("io") || m_kind == QLatin1String("math")) {
            return QSizeF(kWideNodeWidth, kWideNodeHeight);
        }
        return QSizeF(kRectNodeWidth, kRectNodeHeight);
    }

    QPainterPath shapePath() const
    {
        const QRectF r = boundingRect();
        QPainterPath p;
        if (m_kind == QLatin1String("condition") || m_kind == QLatin1String("compare")) {
            p.moveTo(r.center().x(), r.top());
            p.lineTo(r.right(), r.center().y());
            p.lineTo(r.center().x(), r.bottom());
            p.lineTo(r.left(), r.center().y());
            p.closeSubpath();
        } else if (m_kind == QLatin1String("terminal")) {
            p.addRoundedRect(r, r.height() / 2.4, r.height() / 2.4);
        } else if (m_kind == QLatin1String("timer") || m_kind == QLatin1String("loop")) {
            p.moveTo(r.left() + 18.0, r.top());
            p.lineTo(r.right() - 18.0, r.top());
            p.lineTo(r.right(), r.center().y());
            p.lineTo(r.right() - 18.0, r.bottom());
            p.lineTo(r.left() + 18.0, r.bottom());
            p.lineTo(r.left(), r.center().y());
            p.closeSubpath();
        } else if (m_kind == QLatin1String("io")) {
            p.moveTo(r.left() + 24.0, r.top());
            p.lineTo(r.right(), r.top());
            p.lineTo(r.right() - 24.0, r.bottom());
            p.lineTo(r.left(), r.bottom());
            p.closeSubpath();
        } else {
            p.addRoundedRect(r, 12.0, 12.0);
        }
        return p;
    }

    QString m_id;
    QString m_kind;
    QString m_type;
    QString m_title;
    QString m_note;
    QList<FlowEdgeItem *> m_edges;
};

class FlowEdgeItem final : public QGraphicsPathItem
{
public:
    FlowEdgeItem(FlowNodeItem *from, FlowNodeItem *to, const QString &port, QGraphicsItem *parent = nullptr)
        : QGraphicsPathItem(parent), m_from(from), m_to(to), m_port(port.trimmed())
    {
        setFlags(QGraphicsItem::ItemIsSelectable);
        setAcceptHoverEvents(true);
        setZValue(1.0);
        setPen(QPen(QColor(88, 95, 104), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (m_from != nullptr) {
            m_from->addEdge(this);
        }
        if (m_to != nullptr) {
            m_to->addEdge(this);
        }
        updatePath();
    }

    FlowNodeItem *fromNode() const { return m_from; }
    FlowNodeItem *toNode() const { return m_to; }
    QString fromId() const { return m_from != nullptr ? m_from->id() : QString(); }
    QString toId() const { return m_to != nullptr ? m_to->id() : QString(); }
    QString port() const { return m_port; }

    void detach()
    {
        if (m_from != nullptr) {
            m_from->removeEdge(this);
            m_from = nullptr;
        }
        if (m_to != nullptr) {
            m_to->removeEdge(this);
            m_to = nullptr;
        }
    }

    void updatePath()
    {
        if (m_from == nullptr || m_to == nullptr) {
            return;
        }
        const QPointF a = m_from->outputAnchor(m_port);
        const QPointF b = m_to->inputAnchor();

        QPainterPath p(a);
        const QString lp = m_port.trimmed().toLower();
        const bool sideExit = (m_from->kind() == QLatin1String("condition") ||
                               m_from->kind() == QLatin1String("compare") ||
                               m_from->kind() == QLatin1String("loop")) &&
                              (lp == QLatin1String("yes") || lp == QLatin1String("true") || lp == QLatin1String("ok") ||
                               lp == QLatin1String("retry") || lp == QLatin1String("loop") || lp == QLatin1String("next") ||
                               lp == QLatin1String("no") || lp == QLatin1String("false") || lp == QLatin1String("done") ||
                               lp == QLatin1String("exit"));

        if (sideExit) {
            const bool leftExit = (lp == QLatin1String("no") || lp == QLatin1String("false") ||
                                   lp == QLatin1String("done") || lp == QLatin1String("exit"));
            const qreal sideOffset = leftExit ? -38.0 : 38.0;
            const QPointF p1(a.x() + sideOffset, a.y());
            const qreal midY = (a.y() + b.y()) * 0.5;
            const QPointF p2(p1.x(), midY);
            const QPointF p3(b.x(), midY);
            p.lineTo(p1);
            p.lineTo(p2);
            p.lineTo(p3);
            p.lineTo(b);
        } else {
            const qreal midY = (a.y() + b.y()) * 0.5;
            p.lineTo(QPointF(a.x(), midY));
            p.lineTo(QPointF(b.x(), midY));
            p.lineTo(b);
        }
        setPath(p);
    }

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("from"), fromId());
        obj.insert(QStringLiteral("to"), toId());
        if (!m_port.isEmpty()) {
            obj.insert(QStringLiteral("port"), m_port);
        }
        return obj;
    }

    QPainterPath shape() const override
    {
        QPainterPathStroker stroker;
        stroker.setWidth(12.0);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        return stroker.createStroke(path());
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(option)
        Q_UNUSED(widget)
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(isSelected() ? QColor(25, 96, 180) : QColor(88, 95, 104),
                             isSelected() ? 3.0 : 2.0,
                             Qt::SolidLine,
                             Qt::RoundCap,
                             Qt::RoundJoin));
        painter->drawPath(path());

        if (m_from == nullptr || m_to == nullptr) {
            return;
        }

        const QPointF b = m_to->inputAnchor();
        const QPointF before = path().pointAtPercent(0.97);
        QLineF line(before, b);
        if (line.length() > 0.1) {
            const double angle = std::atan2(-line.dy(), line.dx());
            const QPointF p1 = b - QPointF(std::cos(angle + kPi / 7.0) * 10.0,
                                           -std::sin(angle + kPi / 7.0) * 10.0);
            const QPointF p2 = b - QPointF(std::cos(angle - kPi / 7.0) * 10.0,
                                           -std::sin(angle - kPi / 7.0) * 10.0);
            QPolygonF arrow;
            arrow << b << p1 << p2;
            painter->setPen(Qt::NoPen);
            painter->setBrush(isSelected() ? QColor(25, 96, 180) : QColor(88, 95, 104));
            painter->drawPolygon(arrow);
        }

        if (!m_port.isEmpty()) {
            const QPointF mid = path().pointAtPercent(0.5);
            const QString label = normalizedPortLabel(m_port);
            QRectF labelRect(mid.x() - 26.0, mid.y() - 11.0, 52.0, 22.0);
            painter->setBrush(Qt::white);
            painter->setPen(QPen(QColor(154, 158, 164), 1.0));
            painter->drawRoundedRect(labelRect, 8.0, 8.0);
            painter->setPen(QColor(45, 45, 45));
            painter->drawText(labelRect, Qt::AlignCenter, label);
        }
    }

private:
    FlowNodeItem *m_from = nullptr;
    FlowNodeItem *m_to = nullptr;
    QString m_port;
};

void FlowNodeItem::updateEdges()
{
    for (FlowEdgeItem *edge : std::as_const(m_edges)) {
        if (edge != nullptr) {
            edge->updatePath();
        }
    }
}

QVariant FlowNodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionHasChanged) {
        updateEdges();
    }
    return QGraphicsObject::itemChange(change, value);
}

class FlowGraphicsView final : public QGraphicsView
{
public:
    explicit FlowGraphicsView(QWidget *parent = nullptr) : QGraphicsView(parent)
    {
        setRenderHint(QPainter::Antialiasing, true);
        setDragMode(QGraphicsView::RubberBandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
        setBackgroundBrush(QColor(248, 249, 251));
        setFrameShape(QFrame::StyledPanel);
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        const int delta = event->angleDelta().y();
#else
        const int delta = event->delta();
#endif
        const qreal factor = delta > 0 ? 1.10 : 1.0 / 1.10;
        scale(factor, factor);
        event->accept();
    }

    void drawBackground(QPainter *painter, const QRectF &rect) override
    {
        painter->fillRect(rect, QColor(248, 249, 251));
        const qreal grid = 24.0;
        QPen majorPen(QColor(220, 223, 228), 1.0);
        QPen minorPen(QColor(236, 238, 241), 1.0);
        const qreal left = std::floor(rect.left() / grid) * grid;
        const qreal top = std::floor(rect.top() / grid) * grid;
        for (qreal x = left; x < rect.right(); x += grid) {
            painter->setPen((static_cast<int>(std::round(x)) % 120 == 0) ? majorPen : minorPen);
            painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
        }
        for (qreal y = top; y < rect.bottom(); y += grid) {
            painter->setPen((static_cast<int>(std::round(y)) % 120 == 0) ? majorPen : minorPen);
            painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
        }
    }
};

class NodeEditorDialog final : public QDialog
{
public:
    explicit NodeEditorDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(MadModemI18n::text(QStringLiteral("AutoQSO flow block")));
        QVBoxLayout *outer = new QVBoxLayout(this);
        QFormLayout *form = new QFormLayout();

        idEdit = new QLineEdit(this);
        kindCombo = new QComboBox(this);
        kindCombo->addItem(kindDisplayName(QStringLiteral("event")), QStringLiteral("event"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("condition")), QStringLiteral("condition"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("action")), QStringLiteral("action"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("timer")), QStringLiteral("timer"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("variable")), QStringLiteral("variable"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("compare")), QStringLiteral("compare"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("loop")), QStringLiteral("loop"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("io")), QStringLiteral("io"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("math")), QStringLiteral("math"));
        kindCombo->addItem(kindDisplayName(QStringLiteral("terminal")), QStringLiteral("terminal"));
        typeCombo = new QComboBox(this);
        typeCombo->setEditable(true);
        titleEdit = new QLineEdit(this);
        noteEdit = new QTextEdit(this);
        noteEdit->setAcceptRichText(false);
        noteEdit->setMinimumHeight(90);

        form->addRow(MadModemI18n::text(QStringLiteral("ID")), idEdit);
        form->addRow(MadModemI18n::text(QStringLiteral("Kind")), kindCombo);
        form->addRow(MadModemI18n::text(QStringLiteral("Type")), typeCombo);
        form->addRow(MadModemI18n::text(QStringLiteral("Title")), titleEdit);
        form->addRow(MadModemI18n::text(QStringLiteral("Note")), noteEdit);
        outer->addLayout(form);

        QLabel *hint = new QLabel(MadModemI18n::text(QStringLiteral("The editor saves abstract AutoQSO actions only. PTT, audio and UTC slot safety remain controlled by the FT scheduler.")), this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
        outer->addWidget(hint);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        outer->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshTypeChoices(); });
        resize(520, 360);
    }

    void setValues(const QString &id, const QString &kind, const QString &type, const QString &title, const QString &note)
    {
        idEdit->setText(id);
        const int kindIndex = kindCombo->findData(kind);
        kindCombo->setCurrentIndex(kindIndex >= 0 ? kindIndex : kindCombo->findData(QStringLiteral("action")));
        refreshTypeChoices();
        const int typeIndex = typeCombo->findText(type);
        if (typeIndex >= 0) {
            typeCombo->setCurrentIndex(typeIndex);
        } else {
            typeCombo->setEditText(type.isEmpty() ? defaultTypeForKind(currentKind()) : type);
        }
        titleEdit->setText(title);
        noteEdit->setPlainText(note);
    }

    QString currentId() const { return sanitizeId(idEdit->text()); }
    QString currentKind() const { return kindCombo->currentData().toString(); }
    QString currentType() const { return typeCombo->currentText().trimmed(); }
    QString currentTitle() const { return titleEdit->text().trimmed(); }
    QString currentNote() const { return noteEdit->toPlainText().trimmed(); }

    void refreshTypeChoices()
    {
        const QString previous = typeCombo->currentText().trimmed();
        typeCombo->clear();
        typeCombo->addItems(typeSuggestionsForKind(currentKind()));
        const QString wanted = previous.isEmpty() ? defaultTypeForKind(currentKind()) : previous;
        const int idx = typeCombo->findText(wanted);
        if (idx >= 0) {
            typeCombo->setCurrentIndex(idx);
        } else {
            typeCombo->setEditText(wanted);
        }
    }

private:
    QLineEdit *idEdit = nullptr;
    QComboBox *kindCombo = nullptr;
    QComboBox *typeCombo = nullptr;
    QLineEdit *titleEdit = nullptr;
    QTextEdit *noteEdit = nullptr;
};

class ConnectionEditorDialog final : public QDialog
{
public:
    explicit ConnectionEditorDialog(const QList<FlowNodeItem *> &nodes, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(MadModemI18n::text(QStringLiteral("Connect AutoQSO blocks")));
        QVBoxLayout *outer = new QVBoxLayout(this);
        QFormLayout *form = new QFormLayout();
        fromCombo = new QComboBox(this);
        toCombo = new QComboBox(this);
        portCombo = new QComboBox(this);
        portCombo->setEditable(true);
        portCombo->addItems({QString(), QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("ok"), QStringLiteral("retry"), QStringLiteral("next"), QStringLiteral("done"), QStringLiteral("loop"), QStringLiteral("exit")});

        for (FlowNodeItem *node : nodes) {
            if (node == nullptr) {
                continue;
            }
            const QString label = QStringLiteral("%1  —  %2").arg(node->id(), node->title());
            fromCombo->addItem(label, node->id());
            toCombo->addItem(label, node->id());
        }

        form->addRow(MadModemI18n::text(QStringLiteral("From")), fromCombo);
        form->addRow(MadModemI18n::text(QStringLiteral("To")), toCombo);
        form->addRow(MadModemI18n::text(QStringLiteral("Port label")), portCombo);
        outer->addLayout(form);

        QLabel *hint = new QLabel(MadModemI18n::text(QStringLiteral("For decisions/comparisons/loops use ports such as yes/no, true/false, ok/retry, next/done or loop/exit. Leave empty for a normal sequential arrow.")), this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
        outer->addWidget(hint);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        outer->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        resize(520, 220);
    }

    void preselect(const QString &fromId, const QString &toId)
    {
        const int fromIdx = fromCombo->findData(fromId);
        const int toIdx = toCombo->findData(toId);
        if (fromIdx >= 0) {
            fromCombo->setCurrentIndex(fromIdx);
        }
        if (toIdx >= 0) {
            toCombo->setCurrentIndex(toIdx);
        }
    }

    QString fromId() const { return fromCombo->currentData().toString(); }
    QString toId() const { return toCombo->currentData().toString(); }
    QString port() const { return portCombo->currentText().trimmed().toLower(); }

private:
    QComboBox *fromCombo = nullptr;
    QComboBox *toCombo = nullptr;
    QComboBox *portCombo = nullptr;
};

QList<FlowNodeItem *> sceneNodes(QGraphicsScene *scene)
{
    QList<FlowNodeItem *> nodes;
    if (scene == nullptr) {
        return nodes;
    }
    for (QGraphicsItem *item : scene->items()) {
        if (FlowNodeItem *node = dynamic_cast<FlowNodeItem *>(item)) {
            nodes.append(node);
        }
    }
    std::sort(nodes.begin(), nodes.end(), [](const FlowNodeItem *a, const FlowNodeItem *b) {
        if (qFuzzyCompare(a->pos().y(), b->pos().y())) {
            return a->pos().x() < b->pos().x();
        }
        return a->pos().y() < b->pos().y();
    });
    return nodes;
}

FlowNodeItem *findNodeById(QGraphicsScene *scene, const QString &id)
{
    for (FlowNodeItem *node : sceneNodes(scene)) {
        if (node != nullptr && node->id() == id) {
            return node;
        }
    }
    return nullptr;
}

QList<FlowEdgeItem *> sceneEdges(QGraphicsScene *scene)
{
    QList<FlowEdgeItem *> edges;
    if (scene == nullptr) {
        return edges;
    }
    for (QGraphicsItem *item : scene->items()) {
        if (FlowEdgeItem *edge = dynamic_cast<FlowEdgeItem *>(item)) {
            edges.append(edge);
        }
    }
    return edges;
}

QString uniqueIdForScene(QGraphicsScene *scene, const QString &base)
{
    QSet<QString> ids;
    for (FlowNodeItem *node : sceneNodes(scene)) {
        if (node != nullptr) {
            ids.insert(node->id());
        }
    }
    QString candidate = sanitizeId(base);
    if (!ids.contains(candidate)) {
        return candidate;
    }
    int n = 2;
    while (ids.contains(QStringLiteral("%1_%2").arg(candidate).arg(n))) {
        ++n;
    }
    return QStringLiteral("%1_%2").arg(candidate).arg(n);
}

bool edgeAlreadyExists(QGraphicsScene *scene, const QString &fromId, const QString &toId, const QString &port)
{
    for (FlowEdgeItem *edge : sceneEdges(scene)) {
        if (edge != nullptr && edge->fromId() == fromId && edge->toId() == toId && edge->port() == port) {
            return true;
        }
    }
    return false;
}

} // namespace

AutoQsoFlowEditorWidget::AutoQsoFlowEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    QLabel *intro = new QLabel(MadModemI18n::text(QStringLiteral("Visual AutoQSO flow editor. Create, edit, connect and delete blocks here. The saved flow is still safe-by-design: it stores abstract AutoQSO actions; live PTT/audio/slot control remains in the FT scheduler.")), this);
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
    outer->addWidget(intro);

    QHBoxLayout *workspace = new QHBoxLayout();
    workspace->setContentsMargins(0, 0, 0, 0);
    workspace->setSpacing(8);
    QVBoxLayout *toolbar = new QVBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(6);
    m_btnAddEvent = new QPushButton(MadModemI18n::text(QStringLiteral("⚡ Event")), this);
    m_btnAddCondition = new QPushButton(MadModemI18n::text(QStringLiteral("◆ Decision")), this);
    m_btnAddAction = new QPushButton(MadModemI18n::text(QStringLiteral("✓ Action")), this);
    m_btnAddTimer = new QPushButton(MadModemI18n::text(QStringLiteral("⏱ Timer")), this);
    m_btnAddVariable = new QPushButton(MadModemI18n::text(QStringLiteral("𝑥 Variable")), this);
    m_btnAddCompare = new QPushButton(MadModemI18n::text(QStringLiteral("≟ Compare")), this);
    m_btnAddLoop = new QPushButton(MadModemI18n::text(QStringLiteral("↻ Loop")), this);
    m_btnAddIo = new QPushButton(MadModemI18n::text(QStringLiteral("⌨ I/O")), this);
    m_btnAddMath = new QPushButton(MadModemI18n::text(QStringLiteral("∑ Math")), this);
    m_btnAddTerminal = new QPushButton(MadModemI18n::text(QStringLiteral("■ End")), this);
    m_btnEdit = new QPushButton(MadModemI18n::text(QStringLiteral("✎ Edit")), this);
    m_btnConnect = new QPushButton(MadModemI18n::text(QStringLiteral("→ Connect")), this);
    m_btnDelete = new QPushButton(MadModemI18n::text(QStringLiteral("🗑 Delete")), this);
    m_btnDeleteArrows = new QPushButton(MadModemI18n::text(QStringLiteral("↛ Arrows")), this);
    m_btnValidate = new QPushButton(MadModemI18n::text(QStringLiteral("✓ Validate")), this);
    m_btnFit = new QPushButton(MadModemI18n::text(QStringLiteral("▣ Fit")), this);
    m_btnRestore = new QPushButton(MadModemI18n::text(QStringLiteral("↺ Default")), this);
    m_btnHelp = new QPushButton(MadModemI18n::text(QStringLiteral("? Help")), this);

    auto styleButton = [](QPushButton *button, QStyle::StandardPixmap, const QString &tooltip) {
        if (button == nullptr) return;
        button->setMinimumWidth(132);
        button->setMaximumWidth(170);
        button->setToolTip(tooltip);
    };
    styleButton(m_btnAddEvent, QStyle::SP_FileDialogNewFolder, MadModemI18n::text(QStringLiteral("Add an event/start block.")));
    styleButton(m_btnAddCondition, QStyle::SP_MessageBoxQuestion, MadModemI18n::text(QStringLiteral("Add a decision/comparison branch.")));
    styleButton(m_btnAddAction, QStyle::SP_DialogApplyButton, MadModemI18n::text(QStringLiteral("Add an action block.")));
    styleButton(m_btnAddTimer, QStyle::SP_BrowserReload, MadModemI18n::text(QStringLiteral("Add a timer/delay/watchdog block.")));
    styleButton(m_btnAddVariable, QStyle::SP_FileIcon, MadModemI18n::text(QStringLiteral("Add a variable assignment block.")));
    styleButton(m_btnAddCompare, QStyle::SP_FileDialogDetailedView, MadModemI18n::text(QStringLiteral("Add a variable comparison block.")));
    styleButton(m_btnAddLoop, QStyle::SP_ArrowRight, MadModemI18n::text(QStringLiteral("Add a loop/iteration block.")));
    styleButton(m_btnAddIo, QStyle::SP_DialogOpenButton, MadModemI18n::text(QStringLiteral("Add keyboard/popup/status I/O block.")));
    styleButton(m_btnAddMath, QStyle::SP_FileDialogListView, MadModemI18n::text(QStringLiteral("Add a calculation block.")));
    styleButton(m_btnAddTerminal, QStyle::SP_DialogCloseButton, MadModemI18n::text(QStringLiteral("Add an end/terminal block.")));
    styleButton(m_btnEdit, QStyle::SP_FileDialogContentsView, MadModemI18n::text(QStringLiteral("Edit the selected block.")));
    styleButton(m_btnConnect, QStyle::SP_ArrowRight, MadModemI18n::text(QStringLiteral("Connect two selected blocks with an arrow.")));
    styleButton(m_btnDelete, QStyle::SP_TrashIcon, MadModemI18n::text(QStringLiteral("Delete selected blocks or arrows.")));
    styleButton(m_btnDeleteArrows, QStyle::SP_DialogCancelButton, MadModemI18n::text(QStringLiteral("Delete only selected arrows.")));
    styleButton(m_btnValidate, QStyle::SP_DialogApplyButton, MadModemI18n::text(QStringLiteral("Validate the flow.")));
    styleButton(m_btnFit, QStyle::SP_TitleBarMaxButton, MadModemI18n::text(QStringLiteral("Fit the flow in the view.")));
    styleButton(m_btnRestore, QStyle::SP_BrowserReload, MadModemI18n::text(QStringLiteral("Restore the default flow.")));
    styleButton(m_btnHelp, QStyle::SP_DialogHelpButton, MadModemI18n::text(QStringLiteral("Open the flow editor help.")));

    m_btnConnect->setToolTip(MadModemI18n::text(QStringLiteral("Select two blocks then press Connect arrow, or press it and choose From/To in the dialog.")));
    m_btnDelete->setToolTip(MadModemI18n::text(QStringLiteral("Delete selected blocks or selected arrows. Deleting a block also deletes its connected arrows.")));
    m_btnDeleteArrows->setToolTip(MadModemI18n::text(QStringLiteral("Delete only the selected arrows, leaving blocks in place.")));
    m_btnHelp->setToolTip(MadModemI18n::text(QStringLiteral("Show quick instructions and the visual-programming block reference.")));

    toolbar->addWidget(m_btnAddEvent);
    toolbar->addWidget(m_btnAddCondition);
    toolbar->addWidget(m_btnAddAction);
    toolbar->addWidget(m_btnAddTimer);
    toolbar->addWidget(m_btnAddVariable);
    toolbar->addWidget(m_btnAddCompare);
    toolbar->addWidget(m_btnAddLoop);
    toolbar->addWidget(m_btnAddIo);
    toolbar->addWidget(m_btnAddMath);
    toolbar->addWidget(m_btnAddTerminal);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_btnEdit);
    toolbar->addWidget(m_btnConnect);
    toolbar->addWidget(m_btnDelete);
    toolbar->addWidget(m_btnDeleteArrows);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_btnValidate);
    toolbar->addWidget(m_btnFit);
    toolbar->addWidget(m_btnRestore);
    toolbar->addWidget(m_btnHelp);
    toolbar->addStretch(1);

    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 1180, 1400);
    m_view = new FlowGraphicsView(this);
    m_view->setScene(m_scene);
    m_view->setMinimumHeight(420);
    workspace->addLayout(toolbar);
    workspace->addWidget(m_view, 1);
    outer->addLayout(workspace, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    outer->addWidget(m_statusLabel);

    connect(m_btnAddEvent, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addEventNode);
    connect(m_btnAddCondition, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addConditionNode);
    connect(m_btnAddAction, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addActionNode);
    connect(m_btnAddTimer, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addTimerNode);
    connect(m_btnAddVariable, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addVariableNode);
    connect(m_btnAddCompare, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addCompareNode);
    connect(m_btnAddLoop, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addLoopNode);
    connect(m_btnAddIo, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addIoNode);
    connect(m_btnAddMath, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addMathNode);
    connect(m_btnAddTerminal, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::addTerminalNode);
    connect(m_btnEdit, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::editSelectedNode);
    connect(m_btnConnect, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::connectNodes);
    connect(m_btnDelete, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::deleteSelectedItems);
    connect(m_btnDeleteArrows, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::deleteSelectedArrows);
    connect(m_btnHelp, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::showHelp);
    connect(m_btnRestore, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::restoreDefaultFlow);
    connect(m_btnValidate, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::validateFlow);
    connect(m_btnFit, &QPushButton::clicked, this, &AutoQsoFlowEditorWidget::fitFlowInView);

    new QShortcut(QKeySequence::Delete, this, SLOT(deleteSelectedItems()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_E), this, SLOT(editSelectedNode()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this, SLOT(connectNodes()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), this, SLOT(showHelp()));

    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        int nodeCount = 0;
        int edgeCount = 0;
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            if (dynamic_cast<FlowNodeItem *>(item) != nullptr) {
                ++nodeCount;
            } else if (dynamic_cast<FlowEdgeItem *>(item) != nullptr) {
                ++edgeCount;
            }
        }
        if (nodeCount == 0 && edgeCount == 0) {
            setStatus(MadModemI18n::text(QStringLiteral("Select blocks or arrows. Mouse wheel zooms; drag blocks; Delete removes selected items.")));
        } else {
            setStatus(MadModemI18n::text(QStringLiteral("Selected: %1 block(s), %2 arrow(s). Delete removes selection; Delete arrows removes only arrows.")).arg(nodeCount).arg(edgeCount));
        }
    });

    setFlowJson(defaultFlowJson());
}

QString AutoQsoFlowEditorWidget::defaultFlowJson()
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 2);
    root.insert(QStringLiteral("visualStyle"), QStringLiteral("flowgorithm"));
    root.insert(QStringLiteral("name"), QStringLiteral("Default FT AutoQSO"));
    root.insert(QStringLiteral("description"), QStringLiteral("Built-in MadModem FT8/FT4 AutoQSO behaviour represented as a readable flow chart."));

    QJsonArray nodes;
    nodes.append(makeNode(QStringLiteral("decode"), QStringLiteral("event"), QStringLiteral("event.newDecode"),
                          QStringLiteral("New FT decode"), QStringLiteral("A valid FT4/FT8 message was decoded after the UTC period."), 440, 30));
    nodes.append(makeNode(QStringLiteral("is_blacklisted"), QStringLiteral("condition"), QStringLiteral("condition.blacklisted"),
                          QStringLiteral("Blacklisted?"), QStringLiteral("Ignore hidden or blocked calls before any reply is prepared."), 440, 160));
    nodes.append(makeNode(QStringLiteral("is_target"), QStringLiteral("condition"), QStringLiteral("condition.activeTarget"),
                          QStringLiteral("Active QSO target?"), QStringLiteral("Prefer the current QSO partner even if one slot was missed."), 440, 360));
    nodes.append(makeNode(QStringLiteral("is_cq"), QStringLiteral("condition"), QStringLiteral("condition.cqCandidate"),
                          QStringLiteral("CQ candidate?"), QStringLiteral("Accept CQ, directed CQ, and valid first-call opportunities."), 760, 360));
    nodes.append(makeNode(QStringLiteral("dupe_policy"), QStringLiteral("condition"), QStringLiteral("condition.dupePolicy"),
                          QStringLiteral("Already worked?"), QStringLiteral("Apply user policy for worked stations, DXCC and contest exclusions."), 760, 560));
    nodes.append(makeNode(QStringLiteral("choose_tx"), QStringLiteral("action"), QStringLiteral("action.chooseTxFrequency"),
                          QStringLiteral("Choose TX frequency"), QStringLiteral("Use fixed marker, follow-correspondent or automatic free-slot strategy."), 440, 560));
    nodes.append(makeNode(QStringLiteral("prepare_reply"), QStringLiteral("action"), QStringLiteral("action.prepareReply"),
                          QStringLiteral("Prepare reply"), QStringLiteral("Build the correct Tx1..Tx6 message from QSO state."), 440, 700));
    nodes.append(makeNode(QStringLiteral("wait_slot"), QStringLiteral("timer"), QStringLiteral("timer.waitSlot"),
                          QStringLiteral("Wait valid slot"), QStringLiteral("Pre-arm only for the correct opposite/even/odd UTC slot."), 440, 830));
    nodes.append(makeNode(QStringLiteral("transmit"), QStringLiteral("action"), QStringLiteral("action.armTx"),
                          QStringLiteral("Arm TX"), QStringLiteral("Emit abstract TX request; scheduler enforces PTT/audio safety."), 440, 960));
    nodes.append(makeNode(QStringLiteral("timeout"), QStringLiteral("timer"), QStringLiteral("timer.noResponse"),
                          QStringLiteral("No response?"), QStringLiteral("Detect missed reply without losing the active target."), 760, 960));
    nodes.append(makeNode(QStringLiteral("reclaim"), QStringLiteral("action"), QStringLiteral("action.reclaimTarget"),
                          QStringLiteral("Reclaim target"), QStringLiteral("Call the same target again on a clean TX frequency."), 760, 1090));
    nodes.append(makeNode(QStringLiteral("completed"), QStringLiteral("condition"), QStringLiteral("condition.qsoComplete"),
                          QStringLiteral("QSO complete?"), QStringLiteral("RR73/73 and report exchange completed."), 120, 360));
    nodes.append(makeNode(QStringLiteral("log_qso"), QStringLiteral("action"), QStringLiteral("action.logQso"),
                          QStringLiteral("Log QSO"), QStringLiteral("Store call, grid, reports, band and UTC."), 120, 560));
    nodes.append(makeNode(QStringLiteral("resume"), QStringLiteral("terminal"), QStringLiteral("terminal.resumeOrStop"),
                          QStringLiteral("Resume / stop"), QStringLiteral("Return to CQ repeat or RX standby."), 120, 700));

    QJsonArray edges;
    edges.append(makeEdge(QStringLiteral("decode"), QStringLiteral("is_blacklisted")));
    edges.append(makeEdge(QStringLiteral("decode"), QStringLiteral("completed")));
    edges.append(makeEdge(QStringLiteral("is_blacklisted"), QStringLiteral("is_target"), QStringLiteral("no")));
    edges.append(makeEdge(QStringLiteral("is_target"), QStringLiteral("choose_tx"), QStringLiteral("yes")));
    edges.append(makeEdge(QStringLiteral("is_target"), QStringLiteral("is_cq"), QStringLiteral("no")));
    edges.append(makeEdge(QStringLiteral("is_cq"), QStringLiteral("dupe_policy"), QStringLiteral("yes")));
    edges.append(makeEdge(QStringLiteral("dupe_policy"), QStringLiteral("choose_tx"), QStringLiteral("ok")));
    edges.append(makeEdge(QStringLiteral("choose_tx"), QStringLiteral("prepare_reply")));
    edges.append(makeEdge(QStringLiteral("prepare_reply"), QStringLiteral("wait_slot")));
    edges.append(makeEdge(QStringLiteral("wait_slot"), QStringLiteral("transmit")));
    edges.append(makeEdge(QStringLiteral("timeout"), QStringLiteral("reclaim"), QStringLiteral("retry")));
    edges.append(makeEdge(QStringLiteral("reclaim"), QStringLiteral("choose_tx")));
    edges.append(makeEdge(QStringLiteral("completed"), QStringLiteral("log_qso"), QStringLiteral("yes")));
    edges.append(makeEdge(QStringLiteral("log_qso"), QStringLiteral("resume")));

    root.insert(QStringLiteral("nodes"), nodes);
    root.insert(QStringLiteral("edges"), edges);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void AutoQsoFlowEditorWidget::setFlowJson(const QString &json)
{
    const QString normalized = normalizedOrDefaultJson(json);
    m_lastValidJson = normalized;
    rebuildSceneFromJson(normalized);
    setStatus(MadModemI18n::text(QStringLiteral("Flow loaded. Drag blocks, add/edit/connect/delete items, then press OK in Settings to save.")));
}

QString AutoQsoFlowEditorWidget::flowJson() const
{
    return serializeSceneToJson();
}

QString AutoQsoFlowEditorWidget::normalizedOrDefaultJson(const QString &json) const
{
    const QString candidate = json.trimmed().isEmpty() ? defaultFlowJson() : json;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(candidate.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return defaultFlowJson();
    }
    QJsonObject obj = doc.object();
    if (!obj.value(QStringLiteral("nodes")).isArray() || !obj.value(QStringLiteral("edges")).isArray()) {
        return defaultFlowJson();
    }
    obj = compactFlowObject(obj);
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void AutoQsoFlowEditorWidget::rebuildSceneFromJson(const QString &json)
{
    if (m_scene == nullptr) {
        return;
    }
    m_scene->clear();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    const QJsonArray edges = root.value(QStringLiteral("edges")).toArray();

    QMap<QString, FlowNodeItem *> itemById;
    for (const QJsonValue &value : nodes) {
        const QJsonObject obj = value.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || itemById.contains(id)) {
            continue;
        }
        FlowNodeItem *item = new FlowNodeItem(obj);
        m_scene->addItem(item);
        itemById.insert(id, item);
    }

    for (const QJsonValue &value : edges) {
        const QJsonObject obj = value.toObject();
        FlowNodeItem *from = itemById.value(obj.value(QStringLiteral("from")).toString(), nullptr);
        FlowNodeItem *to = itemById.value(obj.value(QStringLiteral("to")).toString(), nullptr);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        m_scene->addItem(new FlowEdgeItem(from, to, obj.value(QStringLiteral("port")).toString()));
    }

    refreshSceneBounds();
    fitFlowInView();
}

QString AutoQsoFlowEditorWidget::serializeSceneToJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 2);
    root.insert(QStringLiteral("visualStyle"), QStringLiteral("flowgorithm"));
    root.insert(QStringLiteral("name"), QStringLiteral("User FT AutoQSO Flow"));
    root.insert(QStringLiteral("savedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonArray nodes;
    QJsonArray edges;
    QList<FlowNodeItem *> nodeItems = sceneNodes(m_scene);
    for (const FlowNodeItem *node : std::as_const(nodeItems)) {
        if (node != nullptr) {
            nodes.append(node->toJson());
        }
    }
    for (FlowEdgeItem *edge : sceneEdges(m_scene)) {
        if (edge != nullptr && !edge->fromId().isEmpty() && !edge->toId().isEmpty()) {
            edges.append(edge->toJson());
        }
    }
    root.insert(QStringLiteral("nodes"), nodes);
    root.insert(QStringLiteral("edges"), edges);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void AutoQsoFlowEditorWidget::restoreDefaultFlow()
{
    setFlowJson(defaultFlowJson());
    emit flowChanged();
    setStatus(MadModemI18n::text(QStringLiteral("Default AutoQSO flow restored. Press OK to save it permanently.")));
}

void AutoQsoFlowEditorWidget::validateFlow()
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(flowJson().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        setStatus(MadModemI18n::text(QStringLiteral("Flow JSON is invalid.")), true);
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
    const QJsonArray edges = root.value(QStringLiteral("edges")).toArray();
    QSet<QString> ids;
    for (const QJsonValue &value : nodes) {
        const QJsonObject obj = value.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || ids.contains(id)) {
            setStatus(MadModemI18n::text(QStringLiteral("Flow validation failed: duplicated or empty node id.")), true);
            return;
        }
        ids.insert(id);
    }
    if (ids.isEmpty()) {
        setStatus(MadModemI18n::text(QStringLiteral("Flow validation failed: no blocks in the flow.")), true);
        return;
    }
    for (const QJsonValue &value : edges) {
        const QJsonObject obj = value.toObject();
        const QString from = obj.value(QStringLiteral("from")).toString();
        const QString to = obj.value(QStringLiteral("to")).toString();
        if (!ids.contains(from) || !ids.contains(to)) {
            setStatus(MadModemI18n::text(QStringLiteral("Flow validation failed: a connection points to a missing node.")), true);
            return;
        }
        // Self-loops are legal in the new programming editor: they model bounded
        // retry/while loops.  Runtime execution will enforce iteration limits.
    }
    setStatus(MadModemI18n::text(QStringLiteral("Flow validation OK. The graph is structurally valid; runtime permissions and safety checks are enforced by MadModem.")));
}

void AutoQsoFlowEditorWidget::fitFlowInView()
{
    if (m_view == nullptr || m_scene == nullptr) {
        return;
    }

    const QRectF rect = m_scene->itemsBoundingRect().adjusted(-60, -60, 60, 60);
    if (!rect.isValid() || rect.isEmpty()) {
        return;
    }

    m_view->resetTransform();

    const QSize viewportSize = m_view->viewport() != nullptr ? m_view->viewport()->size() : QSize();
    if (viewportSize.isValid() && viewportSize.width() > 0 && viewportSize.height() > 0) {
        const qreal sx = static_cast<qreal>(viewportSize.width()) / rect.width();
        const qreal sy = static_cast<qreal>(viewportSize.height()) / rect.height();
        const qreal fullFitScale = qMin(sx, sy);
        const qreal readableScale = qBound<qreal>(0.90, fullFitScale, 1.15);
        m_view->scale(readableScale, readableScale);
    }

    m_view->centerOn(QPointF(rect.center().x(), rect.top() + 260.0));
}

void AutoQsoFlowEditorWidget::addEventNode()
{
    addNodeWithDialog(QStringLiteral("event"));
}

void AutoQsoFlowEditorWidget::addConditionNode()
{
    addNodeWithDialog(QStringLiteral("condition"));
}

void AutoQsoFlowEditorWidget::addActionNode()
{
    addNodeWithDialog(QStringLiteral("action"));
}

void AutoQsoFlowEditorWidget::addTimerNode()
{
    addNodeWithDialog(QStringLiteral("timer"));
}

void AutoQsoFlowEditorWidget::addVariableNode()
{
    addNodeWithDialog(QStringLiteral("variable"));
}

void AutoQsoFlowEditorWidget::addCompareNode()
{
    addNodeWithDialog(QStringLiteral("compare"));
}

void AutoQsoFlowEditorWidget::addLoopNode()
{
    addNodeWithDialog(QStringLiteral("loop"));
}

void AutoQsoFlowEditorWidget::addIoNode()
{
    addNodeWithDialog(QStringLiteral("io"));
}

void AutoQsoFlowEditorWidget::addMathNode()
{
    addNodeWithDialog(QStringLiteral("math"));
}

void AutoQsoFlowEditorWidget::addTerminalNode()
{
    addNodeWithDialog(QStringLiteral("terminal"));
}

void AutoQsoFlowEditorWidget::addNodeWithDialog(const QString &kind)
{
    if (m_scene == nullptr || m_view == nullptr) {
        return;
    }
    NodeEditorDialog dlg(this);
    const QString baseId = QStringLiteral("%1_%2").arg(kind).arg(sceneNodes(m_scene).size() + 1);
    dlg.setValues(uniqueIdForScene(m_scene, baseId), kind, defaultTypeForKind(kind), defaultTitleForKind(kind), defaultNoteForKind(kind));
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString id = uniqueIdForScene(m_scene, dlg.currentId());
    const QString title = dlg.currentTitle().isEmpty() ? id : dlg.currentTitle();
    const QString type = dlg.currentType().isEmpty() ? defaultTypeForKind(dlg.currentKind()) : dlg.currentType();
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    QJsonObject node = makeNode(id,
                                dlg.currentKind(),
                                type,
                                title,
                                dlg.currentNote(),
                                qRound(center.x()),
                                qRound(center.y()));
    FlowNodeItem *item = new FlowNodeItem(node);
    m_scene->addItem(item);
    m_scene->clearSelection();
    item->setSelected(true);
    refreshSceneBounds();
    emit flowChanged();
    setStatus(MadModemI18n::text(QStringLiteral("Block added. Press OK in Settings to save the flow.")));
}

void AutoQsoFlowEditorWidget::editSelectedNode()
{
    if (m_scene == nullptr) {
        return;
    }
    FlowNodeItem *target = nullptr;
    for (QGraphicsItem *item : m_scene->selectedItems()) {
        if (FlowNodeItem *node = dynamic_cast<FlowNodeItem *>(item)) {
            if (target != nullptr) {
                setStatus(MadModemI18n::text(QStringLiteral("Select exactly one block to edit.")), true);
                return;
            }
            target = node;
        }
    }
    if (target == nullptr) {
        setStatus(MadModemI18n::text(QStringLiteral("Select one block, then press Edit selected.")), true);
        return;
    }

    NodeEditorDialog dlg(this);
    dlg.setValues(target->id(), target->kind(), target->nodeType(), target->title(), target->note());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QString newId = dlg.currentId();
    if (newId.isEmpty()) {
        newId = target->id();
    }
    for (FlowNodeItem *node : sceneNodes(m_scene)) {
        if (node != target && node->id() == newId) {
            setStatus(MadModemI18n::text(QStringLiteral("Cannot edit block: another block already uses this ID.")), true);
            return;
        }
    }

    const QString title = dlg.currentTitle().isEmpty() ? newId : dlg.currentTitle();
    const QString type = dlg.currentType().isEmpty() ? defaultTypeForKind(dlg.currentKind()) : dlg.currentType();
    target->setNodeData(newId, dlg.currentKind(), type, title, dlg.currentNote());
    refreshSceneBounds();
    emit flowChanged();
    setStatus(MadModemI18n::text(QStringLiteral("Block updated. Press OK in Settings to save the flow.")));
}

void AutoQsoFlowEditorWidget::connectNodes()
{
    if (m_scene == nullptr) {
        return;
    }
    QList<FlowNodeItem *> nodes = sceneNodes(m_scene);
    if (nodes.size() < 2) {
        setStatus(MadModemI18n::text(QStringLiteral("At least two blocks are required to create a connection.")), true);
        return;
    }

    QList<FlowNodeItem *> selected;
    for (QGraphicsItem *item : m_scene->selectedItems()) {
        if (FlowNodeItem *node = dynamic_cast<FlowNodeItem *>(item)) {
            selected.append(node);
        }
    }
    std::sort(selected.begin(), selected.end(), [](const FlowNodeItem *a, const FlowNodeItem *b) {
        if (qFuzzyCompare(a->pos().y(), b->pos().y())) {
            return a->pos().x() < b->pos().x();
        }
        return a->pos().y() < b->pos().y();
    });

    ConnectionEditorDialog dlg(nodes, this);
    if (selected.size() >= 2) {
        dlg.preselect(selected.at(0)->id(), selected.at(1)->id());
    } else if (selected.size() == 1) {
        dlg.preselect(selected.first()->id(), QString());
    }
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString fromId = dlg.fromId();
    const QString toId = dlg.toId();
    const QString port = dlg.port();
    if (fromId.isEmpty() || toId.isEmpty() || fromId == toId) {
        setStatus(MadModemI18n::text(QStringLiteral("Invalid connection: choose two different blocks.")), true);
        return;
    }
    if (edgeAlreadyExists(m_scene, fromId, toId, port)) {
        setStatus(MadModemI18n::text(QStringLiteral("Connection already exists.")), true);
        return;
    }
    FlowNodeItem *from = findNodeById(m_scene, fromId);
    FlowNodeItem *to = findNodeById(m_scene, toId);
    if (from == nullptr || to == nullptr) {
        setStatus(MadModemI18n::text(QStringLiteral("Cannot create connection: missing block.")), true);
        return;
    }

    FlowEdgeItem *edge = new FlowEdgeItem(from, to, port);
    m_scene->addItem(edge);
    m_scene->clearSelection();
    edge->setSelected(true);
    refreshSceneBounds();
    emit flowChanged();
    setStatus(MadModemI18n::text(QStringLiteral("Connection added. Press OK in Settings to save the flow.")));
}

void AutoQsoFlowEditorWidget::deleteSelectedArrows()
{
    if (m_scene == nullptr) {
        return;
    }
    QSet<FlowEdgeItem *> edgesToDelete;
    for (QGraphicsItem *item : m_scene->selectedItems()) {
        if (FlowEdgeItem *edge = dynamic_cast<FlowEdgeItem *>(item)) {
            edgesToDelete.insert(edge);
        }
    }
    if (edgesToDelete.isEmpty()) {
        setStatus(MadModemI18n::text(QStringLiteral("Select one or more arrows, then press Delete arrows.")), true);
        return;
    }
    for (FlowEdgeItem *edge : std::as_const(edgesToDelete)) {
        if (edge != nullptr) {
            edge->detach();
            m_scene->removeItem(edge);
            delete edge;
        }
    }
    refreshSceneBounds();
    emit flowChanged();
    setStatus(MadModemI18n::text(QStringLiteral("Selected arrow(s) deleted. Press OK in Settings to save the flow.")));
}

void AutoQsoFlowEditorWidget::showHelp()
{
    QDialog dlg(this);
    dlg.setWindowTitle(MadModemI18n::text(QStringLiteral("AutoQSO / MM Flow help")));
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTextEdit *text = new QTextEdit(&dlg);
    text->setReadOnly(true);
    text->setAcceptRichText(true);
    text->setHtml(MadModemI18n::translate(QStringLiteral("help.autoqso_flow_html"), QStringLiteral(
        "<h2>MM Flow editor</h2>"
        "<p>This editor is becoming the visual programming layer for MadModem. "
        "Today it stores and validates the flow graph; live PTT/audio/CAT safety still remains in the scheduler.</p>"
        "<h3>Basic editing</h3>"
        "<ul>"
        "<li><b>Create a block:</b> press one of the + buttons, choose the type, title and note.</li>"
        "<li><b>Create an arrow:</b> select two blocks, press <b>Connect arrow</b>, choose the output port if needed.</li>"
        "<li><b>Delete an arrow:</b> click the arrow line and press <b>Delete arrows</b> or the Delete key.</li>"
        "<li><b>Delete a block:</b> select the block and press Delete. Connected arrows are removed too.</li>"
        "<li><b>Edit:</b> select exactly one block and press <b>Edit selected</b>. Shortcuts: Delete, Ctrl+E, Ctrl+L, Ctrl+H.</li>"
        "</ul>"
        "<h3>Programming blocks</h3>"
        "<ul>"
        "<li><b>Variable:</b> set/copy/increment/clear/list/map values.</li>"
        "<li><b>Compare:</b> compare variables, constants, decoded calls, bands, modes and regex matches.</li>"
        "<li><b>Loop:</b> iterate over decodes, candidates, log rows or bounded counters. Use next/done or loop/exit ports.</li>"
        "<li><b>I/O:</b> keyboard input, popup message/question, status message, log message, table output.</li>"
        "<li><b>Timer:</b> FT slot wait, decode-window wait, retry delay, interval, watchdog, UTC time trigger.</li>"
        "<li><b>Math:</b> arithmetic plus radio helpers such as locator distance/bearing.</li>"
        "</ul>"
        "<h3>Safety model</h3>"
        "<p>Flows may request abstract app actions. Dangerous operations such as TX, PTT, audio start, CAT QSY, file writes and scheduler operations must pass through MadModem's runtime guards.</p>")));
    layout->addWidget(text, 1);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    dlg.resize(780, 560);
    dlg.exec();
}

void AutoQsoFlowEditorWidget::deleteSelectedItems()
{
    if (m_scene == nullptr) {
        return;
    }
    const QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        setStatus(MadModemI18n::text(QStringLiteral("Select blocks or arrows to delete.")), true);
        return;
    }

    QSet<FlowEdgeItem *> edgesToDelete;
    QSet<FlowNodeItem *> nodesToDelete;
    for (QGraphicsItem *item : selected) {
        if (FlowEdgeItem *edge = dynamic_cast<FlowEdgeItem *>(item)) {
            edgesToDelete.insert(edge);
        } else if (FlowNodeItem *node = dynamic_cast<FlowNodeItem *>(item)) {
            nodesToDelete.insert(node);
            for (FlowEdgeItem *edge : node->edges()) {
                if (edge != nullptr) {
                    edgesToDelete.insert(edge);
                }
            }
        }
    }

    for (FlowEdgeItem *edge : std::as_const(edgesToDelete)) {
        if (edge != nullptr) {
            edge->detach();
            m_scene->removeItem(edge);
            delete edge;
        }
    }
    for (FlowNodeItem *node : std::as_const(nodesToDelete)) {
        if (node != nullptr) {
            m_scene->removeItem(node);
            delete node;
        }
    }

    refreshSceneBounds();
    emit flowChanged();
    setStatus(MadModemI18n::text(QStringLiteral("Selected item(s) deleted. Press OK in Settings to save the flow.")));
}

void AutoQsoFlowEditorWidget::refreshSceneBounds()
{
    if (m_scene == nullptr) {
        return;
    }
    QRectF rect = m_scene->itemsBoundingRect().adjusted(-120, -120, 160, 160);
    if (!rect.isValid() || rect.isEmpty()) {
        rect = QRectF(0, 0, 1180, 1400);
    }
    m_scene->setSceneRect(rect);
}

void AutoQsoFlowEditorWidget::setStatus(const QString &message, bool warning)
{
    if (m_statusLabel == nullptr) {
        return;
    }
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(warning
        ? QStringLiteral("QLabel { color: #B06000; }")
        : QStringLiteral("QLabel { color: palette(mid); }"));
}
