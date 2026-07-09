#include "AutoQsoFlowExecutor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QtGlobal>
#include <QVector>

namespace {

struct FlowNode
{
    QString id;
    QString kind;
    QString type;
    QString title;
    QString note;
};

struct FlowEdge
{
    QString from;
    QString to;
    QString port;
};

QString cleanTitle(const FlowNode &node)
{
    if (!node.title.trimmed().isEmpty()) {
        return node.title.trimmed();
    }
    if (!node.type.trimmed().isEmpty()) {
        return node.type.trimmed();
    }
    return node.id.trimmed();
}

QJsonObject nodeObject(const QString &id, const QString &kind, const QString &type, const QString &title)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("kind"), kind);
    obj.insert(QStringLiteral("type"), type);
    obj.insert(QStringLiteral("title"), title);
    return obj;
}

QJsonObject edgeObject(const QString &from, const QString &to, const QString &port = QString())
{
    QJsonObject obj;
    obj.insert(QStringLiteral("from"), from);
    obj.insert(QStringLiteral("to"), to);
    if (!port.isEmpty()) {
        obj.insert(QStringLiteral("port"), port);
    }
    return obj;
}

QJsonObject defaultFlowObject()
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 2);
    root.insert(QStringLiteral("visualStyle"), QStringLiteral("flowgorithm"));
    QJsonArray nodes;
    nodes.append(nodeObject(QStringLiteral("decode"), QStringLiteral("event"), QStringLiteral("event.newDecode"), QStringLiteral("New FT decode")));
    nodes.append(nodeObject(QStringLiteral("is_blacklisted"), QStringLiteral("condition"), QStringLiteral("condition.blacklisted"), QStringLiteral("Blacklisted?")));
    nodes.append(nodeObject(QStringLiteral("is_target"), QStringLiteral("condition"), QStringLiteral("condition.activeTarget"), QStringLiteral("Active QSO target?")));
    nodes.append(nodeObject(QStringLiteral("is_cq"), QStringLiteral("condition"), QStringLiteral("condition.cqCandidate"), QStringLiteral("CQ candidate?")));
    nodes.append(nodeObject(QStringLiteral("dupe_policy"), QStringLiteral("condition"), QStringLiteral("condition.dupePolicy"), QStringLiteral("Already worked?")));
    nodes.append(nodeObject(QStringLiteral("choose_tx"), QStringLiteral("action"), QStringLiteral("action.chooseTxFrequency"), QStringLiteral("Choose TX frequency")));
    nodes.append(nodeObject(QStringLiteral("prepare_reply"), QStringLiteral("action"), QStringLiteral("action.prepareReply"), QStringLiteral("Prepare reply")));
    nodes.append(nodeObject(QStringLiteral("wait_slot"), QStringLiteral("timer"), QStringLiteral("timer.waitSlot"), QStringLiteral("Wait valid slot")));
    nodes.append(nodeObject(QStringLiteral("transmit"), QStringLiteral("action"), QStringLiteral("action.armTx"), QStringLiteral("Arm TX")));
    nodes.append(nodeObject(QStringLiteral("timeout"), QStringLiteral("timer"), QStringLiteral("timer.noResponse"), QStringLiteral("No response?")));
    nodes.append(nodeObject(QStringLiteral("reclaim"), QStringLiteral("action"), QStringLiteral("action.reclaimTarget"), QStringLiteral("Reclaim target")));
    nodes.append(nodeObject(QStringLiteral("completed"), QStringLiteral("condition"), QStringLiteral("condition.qsoComplete"), QStringLiteral("QSO complete?")));
    nodes.append(nodeObject(QStringLiteral("log_qso"), QStringLiteral("action"), QStringLiteral("action.logQso"), QStringLiteral("Log QSO")));
    nodes.append(nodeObject(QStringLiteral("resume"), QStringLiteral("terminal"), QStringLiteral("terminal.resumeOrStop"), QStringLiteral("Resume / stop")));
    root.insert(QStringLiteral("nodes"), nodes);

    QJsonArray edges;
    edges.append(edgeObject(QStringLiteral("decode"), QStringLiteral("is_blacklisted")));
    edges.append(edgeObject(QStringLiteral("decode"), QStringLiteral("completed")));
    edges.append(edgeObject(QStringLiteral("is_blacklisted"), QStringLiteral("is_target"), QStringLiteral("no")));
    edges.append(edgeObject(QStringLiteral("is_target"), QStringLiteral("choose_tx"), QStringLiteral("yes")));
    edges.append(edgeObject(QStringLiteral("is_target"), QStringLiteral("is_cq"), QStringLiteral("no")));
    edges.append(edgeObject(QStringLiteral("is_cq"), QStringLiteral("dupe_policy"), QStringLiteral("yes")));
    edges.append(edgeObject(QStringLiteral("dupe_policy"), QStringLiteral("choose_tx"), QStringLiteral("ok")));
    edges.append(edgeObject(QStringLiteral("choose_tx"), QStringLiteral("prepare_reply")));
    edges.append(edgeObject(QStringLiteral("prepare_reply"), QStringLiteral("wait_slot")));
    edges.append(edgeObject(QStringLiteral("wait_slot"), QStringLiteral("transmit")));
    edges.append(edgeObject(QStringLiteral("timeout"), QStringLiteral("reclaim"), QStringLiteral("retry")));
    edges.append(edgeObject(QStringLiteral("reclaim"), QStringLiteral("choose_tx")));
    edges.append(edgeObject(QStringLiteral("completed"), QStringLiteral("log_qso"), QStringLiteral("yes")));
    edges.append(edgeObject(QStringLiteral("log_qso"), QStringLiteral("resume")));
    root.insert(QStringLiteral("edges"), edges);
    return root;
}

bool parseFlow(const QString &json, QMap<QString, FlowNode> *nodes, QVector<FlowEdge> *edges)
{
    if (nodes == nullptr || edges == nullptr) {
        return false;
    }
    nodes->clear();
    edges->clear();

    QJsonObject root;
    const QString trimmed = json.trimmed();
    if (!trimmed.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            root = doc.object();
        }
    }
    if (!root.value(QStringLiteral("nodes")).isArray() || !root.value(QStringLiteral("edges")).isArray()) {
        root = defaultFlowObject();
    }

    const QJsonArray nodeArray = root.value(QStringLiteral("nodes")).toArray();
    const QJsonArray edgeArray = root.value(QStringLiteral("edges")).toArray();

    for (const QJsonValue &value : nodeArray) {
        const QJsonObject obj = value.toObject();
        FlowNode node;
        node.id = obj.value(QStringLiteral("id")).toString().trimmed();
        node.kind = obj.value(QStringLiteral("kind")).toString().trimmed();
        node.type = obj.value(QStringLiteral("type")).toString().trimmed();
        node.title = obj.value(QStringLiteral("title")).toString().trimmed();
        node.note = obj.value(QStringLiteral("note")).toString().trimmed();
        if (!node.id.isEmpty() && !nodes->contains(node.id)) {
            nodes->insert(node.id, node);
        }
    }

    for (const QJsonValue &value : edgeArray) {
        const QJsonObject obj = value.toObject();
        FlowEdge edge;
        edge.from = obj.value(QStringLiteral("from")).toString().trimmed();
        edge.to = obj.value(QStringLiteral("to")).toString().trimmed();
        edge.port = obj.value(QStringLiteral("port")).toString().trimmed().toLower();
        if (!edge.from.isEmpty() && !edge.to.isEmpty() && nodes->contains(edge.from) && nodes->contains(edge.to)) {
            edges->append(edge);
        }
    }
    return !nodes->isEmpty();
}

QString firstNodeId(const QMap<QString, FlowNode> &nodes)
{
    for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
        if (it.value().type == QLatin1String("event.newDecode")) {
            return it.key();
        }
    }
    if (nodes.contains(QStringLiteral("decode"))) {
        return QStringLiteral("decode");
    }
    for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
        if (it.value().kind == QLatin1String("event")) {
            return it.key();
        }
    }
    return nodes.isEmpty() ? QString() : nodes.constBegin().key();
}

QString boolPort(bool value)
{
    return value ? QStringLiteral("yes") : QStringLiteral("no");
}

QString selectedPortForNode(const FlowNode &node, const AutoQsoFlowExecutor::Context &ctx, QString *decisionText)
{
    const QString type = node.type.trimmed();
    QString port;
    QString text;

    if (type == QLatin1String("condition.blacklisted")) {
        port = boolPort(ctx.blacklisted);
        text = ctx.blacklisted ? QStringLiteral("yes") : QStringLiteral("no");
    } else if (type == QLatin1String("condition.activeTarget")) {
        port = boolPort(ctx.activeTarget);
        text = ctx.activeTarget ? QStringLiteral("yes") : QStringLiteral("no");
    } else if (type == QLatin1String("condition.cqCandidate")) {
        port = boolPort(ctx.cqCandidate);
        text = ctx.cqCandidate ? QStringLiteral("yes") : QStringLiteral("no");
    } else if (type == QLatin1String("condition.dupePolicy")) {
        const bool ok = ctx.candidateValid && !ctx.duplicateRejected && !ctx.blacklisted;
        port = ok ? QStringLiteral("ok") : QStringLiteral("no");
        if (ok) {
            text = QStringLiteral("ok");
            if (!ctx.priorityText.isEmpty()) {
                text += QStringLiteral(" — ") + ctx.priorityText;
            }
        } else {
            text = QStringLiteral("blocked");
            if (!ctx.rejectReason.isEmpty()) {
                text += QStringLiteral(" — ") + ctx.rejectReason;
            }
        }
    } else if (type == QLatin1String("condition.qsoComplete")) {
        port = boolPort(ctx.qsoComplete);
        text = ctx.qsoComplete ? QStringLiteral("yes") : QStringLiteral("no");
    } else if (type == QLatin1String("condition.txAllowed")) {
        port = boolPort(ctx.txAllowedByRuntime);
        text = ctx.txAllowedByRuntime ? QStringLiteral("yes") : QStringLiteral("no — runtime gate closed");
    } else if (type == QLatin1String("condition.retryAvailable")) {
        port = boolPort(ctx.retryAvailable);
        text = ctx.retryAvailable ? QStringLiteral("yes") : QStringLiteral("no");
    } else if (node.kind == QLatin1String("condition")) {
        port = QStringLiteral("ok");
        text = QStringLiteral("not implemented in shadow evaluator; continuing if connected");
    }

    if (decisionText != nullptr) {
        *decisionText = text;
    }
    return port;
}

QString nextNodeForPort(const QString &from,
                        const QString &wantedPort,
                        const QVector<FlowEdge> &edges,
                        const QMap<QString, FlowNode> &nodes,
                        const AutoQsoFlowExecutor::Context &ctx)
{
    QVector<FlowEdge> candidates;
    for (const FlowEdge &edge : edges) {
        if (edge.from == from) {
            candidates.append(edge);
        }
    }
    if (candidates.isEmpty()) {
        return QString();
    }

    // The default graph has two outgoing links from the event node.  The
    // completion branch is only meaningful when the current decode closes a QSO;
    // otherwise the shadow walk follows the ordinary new-decode path.
    const FlowNode fromNode = nodes.value(from);
    if (fromNode.type == QLatin1String("event.newDecode")) {
        if (ctx.qsoComplete) {
            for (const FlowEdge &edge : candidates) {
                if (nodes.value(edge.to).type == QLatin1String("condition.qsoComplete")) {
                    return edge.to;
                }
            }
        }
        for (const FlowEdge &edge : candidates) {
            if (nodes.value(edge.to).type == QLatin1String("condition.blacklisted")) {
                return edge.to;
            }
        }
    }

    if (!wantedPort.isEmpty()) {
        for (const FlowEdge &edge : candidates) {
            if (edge.port.compare(wantedPort, Qt::CaseInsensitive) == 0) {
                return edge.to;
            }
        }
    }
    for (const FlowEdge &edge : candidates) {
        if (edge.port.isEmpty()) {
            return edge.to;
        }
    }
    if (wantedPort == QLatin1String("ok")) {
        for (const FlowEdge &edge : candidates) {
            if (edge.port.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0) {
                return edge.to;
            }
        }
    }
    return QString();
}

QString decodeSummary(const AutoQsoFlowExecutor::Context &ctx)
{
    QStringList bits;
    bits << ctx.decodedMessage.trimmed();
    if (ctx.snrDb != 0) {
        bits << QStringLiteral("%1 dB").arg(ctx.snrDb);
    }
    if (ctx.rxHz > 0) {
        bits << QStringLiteral("RX %1 Hz").arg(ctx.rxHz);
    }
    if (!ctx.targetCall.isEmpty()) {
        bits << QStringLiteral("target %1").arg(ctx.targetCall);
    }
    return bits.join(QStringLiteral(" | "));
}

} // namespace

AutoQsoFlowExecutor::Trace AutoQsoFlowExecutor::runShadow(const QString &flowJson, const Context &ctx)
{
    Trace trace;
    QMap<QString, FlowNode> nodes;
    QVector<FlowEdge> edges;
    if (!parseFlow(flowJson, &nodes, &edges)) {
        trace.lines << QStringLiteral("[Flow][shadow] ERROR: saved flow is empty or invalid; no runtime action taken.");
        trace.result = QStringLiteral("flow invalid");
        trace.stoppedByDecision = true;
        return trace;
    }

    trace.lines << QStringLiteral("[Flow][shadow] Event: New FT decode — %1").arg(decodeSummary(ctx));
    if (!ctx.evilModeUnlocked || !ctx.autoQsoArmed) {
        trace.lines << QStringLiteral("[Flow][shadow] Runtime gate: Evil/AutoQSO not armed; observing only, no TX possible.");
    } else if (!ctx.txAllowedByRuntime) {
        trace.lines << QStringLiteral("[Flow][shadow] Runtime gate: TX start currently blocked by session/slot/audio state; observing only.");
    }

    QString current = firstNodeId(nodes);
    QSet<QString> visited;
    int steps = 0;
    while (!current.isEmpty() && nodes.contains(current) && steps++ < 48) {
        if (visited.contains(current)) {
            trace.lines << QStringLiteral("[Flow][shadow] Stop: loop detected at block '%1'; no runtime action taken.").arg(current);
            trace.stoppedByDecision = true;
            break;
        }
        visited.insert(current);

        const FlowNode node = nodes.value(current);
        const QString title = cleanTitle(node);
        QString wantedPort;

        if (node.kind == QLatin1String("condition")) {
            QString decision;
            wantedPort = selectedPortForNode(node, ctx, &decision);
            trace.lines << QStringLiteral("[Flow][shadow] Decision: %1 -> %2").arg(title, decision.isEmpty() ? QStringLiteral("continue") : decision);
        } else if (node.kind == QLatin1String("action")) {
            if (node.type == QLatin1String("action.chooseTxFrequency")) {
                QString line = QStringLiteral("[Flow][shadow] Action: %1 -> would choose TX %2 Hz").arg(title).arg(ctx.suggestedTxHz > 0 ? ctx.suggestedTxHz : ctx.rxHz);
                if (!ctx.txStrategy.isEmpty()) {
                    line += QStringLiteral(" using %1").arg(ctx.txStrategy);
                }
                if (!ctx.txReason.isEmpty()) {
                    line += QStringLiteral(" (%1)").arg(ctx.txReason);
                }
                trace.lines << line;
            } else if (node.type == QLatin1String("action.prepareReply")) {
                trace.wouldPrepareReply = true;
                trace.lines << QStringLiteral("[Flow][shadow] Action: %1 -> would prepare '%2'")
                                   .arg(title, ctx.suggestedTxMessage.isEmpty() ? QStringLiteral("<no message available>") : ctx.suggestedTxMessage);
            } else if (node.type == QLatin1String("action.armTx")) {
                trace.wouldArmTx = true;
                trace.lines << QStringLiteral("[Flow][shadow] Action: %1 -> would request scheduler arm only; PTT/audio remain blocked in shadow mode.").arg(title);
            } else if (node.type == QLatin1String("action.reclaimTarget")) {
                trace.lines << QStringLiteral("[Flow][shadow] Action: %1 -> would reclaim active target %2.")
                                   .arg(title, ctx.targetCall.isEmpty() ? QStringLiteral("<unknown>") : ctx.targetCall);
            } else if (node.type == QLatin1String("action.logQso")) {
                trace.lines << QStringLiteral("[Flow][shadow] Action: %1 -> would log completed QSO context, not from shadow mode.").arg(title);
            } else {
                trace.lines << QStringLiteral("[Flow][shadow] Action: %1 -> abstract action '%2' observed only.").arg(title, node.type);
            }
        } else if (node.kind == QLatin1String("timer")) {
            if (node.type == QLatin1String("timer.waitSlot")) {
                trace.lines << QStringLiteral("[Flow][shadow] Timer: %1 -> would wait for valid UTC slot boundary.").arg(title);
            } else if (node.type == QLatin1String("timer.noResponse")) {
                trace.lines << QStringLiteral("[Flow][shadow] Timer: %1 -> would check retry/reclaim window.").arg(title);
            } else {
                trace.lines << QStringLiteral("[Flow][shadow] Timer: %1 -> timer '%2' observed only.").arg(title, node.type);
            }
        } else if (node.kind == QLatin1String("terminal")) {
            trace.lines << QStringLiteral("[Flow][shadow] Terminal: %1 -> branch ended, no runtime action taken.").arg(title);
            trace.result = QStringLiteral("terminal");
            break;
        }

        const QString next = nextNodeForPort(current, wantedPort, edges, nodes, ctx);
        if (next.isEmpty()) {
            if (node.kind == QLatin1String("condition")) {
                trace.lines << QStringLiteral("[Flow][shadow] Stop: no '%1' branch from '%2'.").arg(wantedPort.isEmpty() ? QStringLiteral("default") : wantedPort, title);
                trace.stoppedByDecision = true;
            }
            break;
        }
        current = next;
    }

    if (trace.result.isEmpty()) {
        if (trace.wouldArmTx) {
            trace.result = QStringLiteral("would arm TX in real executor, but shadow mode did not transmit");
        } else if (trace.wouldPrepareReply) {
            trace.result = QStringLiteral("would prepare reply, but no TX arm reached");
        } else if (ctx.blacklisted) {
            trace.result = QStringLiteral("blacklisted / ignored");
        } else if (ctx.duplicateRejected) {
            trace.result = QStringLiteral("candidate rejected by policy");
        } else if (!ctx.cqCandidate && !ctx.activeTarget && !ctx.qsoComplete) {
            trace.result = QStringLiteral("not an AutoQSO start candidate");
        } else {
            trace.result = QStringLiteral("observed only");
        }
    }
    trace.lines << QStringLiteral("[Flow][shadow] Result: %1.").arg(trace.result);
    return trace;
}
