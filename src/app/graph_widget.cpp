#include "graph_widget.h"
#include "core/note_repository.h"

#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QPen>
#include <QFont>
#include <QtMath>
#include <QSet>

static constexpr double NODE_RADIUS = 28.0;
static constexpr double REPULSION   = 8000.0;
static constexpr double ATTRACTION  = 0.005;
static constexpr double DAMPING     = 0.85;
static constexpr int    MAX_ITER    = 200;

GraphWidget::GraphWidget(NoteRepository *repo, QWidget *parent)
    : QGraphicsView(parent), m_repo(repo)
{
    setObjectName(QStringLiteral("graphWidget"));
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setStyleSheet(QStringLiteral(
        "QGraphicsView#graphWidget { background: #11111b; border: none; }"
    ));

    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-2000, -2000, 4000, 4000);
    setScene(m_scene);

    // Layout animation timer
    m_timer = new QTimer(this);
    m_timer->setInterval(16);  // ~60 FPS
    connect(m_timer, &QTimer::timeout, this, &GraphWidget::tick);
}

void GraphWidget::rebuild()
{
    if (!m_repo) return;
    m_scene->clear();
    m_nodes.clear();
    m_edges.clear();
    m_nodeIndex.clear();
    m_iterations = 0;

    QVector<NoteData> notes = m_repo->listNotes(500, 0);
    if (notes.isEmpty()) {
        auto *placeholder = m_scene->addText(
            QStringLiteral("(\u7a7a) \u6ca1\u6709\u7b14\u8bb0\uff0c\u5148\u521b\u5efa\u4e00\u7bc7\u5427"));
        placeholder->setDefaultTextColor(QColor(0x6c7086));
        placeholder->setPos(-80, -10);
        return;
    }

    // Create nodes in a circle
    int n = notes.size();
    double radius = 50.0 * qSqrt(n);
    for (int i = 0; i < n; i++) {
        double angle = 2.0 * M_PI * i / n;
        Node node;
        node.noteId = notes[i].id;
        node.title  = notes[i].title.isEmpty()
            ? QStringLiteral("\u672a\u547d\u540d") : notes[i].title;
        node.x = radius * qCos(angle);
        node.y = radius * qSin(angle);
        node.vx = 0;
        node.vy = 0;

        // Circle
        node.circle = m_scene->addEllipse(-NODE_RADIUS, -NODE_RADIUS,
                                          2 * NODE_RADIUS, 2 * NODE_RADIUS,
                                          QPen(QColor(0xcba6f7), 2),
                                          QBrush(QColor(0x313244)));
        node.circle->setPos(node.x, node.y);
        node.circle->setCursor(Qt::PointingHandCursor);
        node.circle->setZValue(1);

        // Label
        QString shortTitle = node.title.left(8);
        if (node.title.length() > 8) shortTitle += QStringLiteral("...");
        node.label = m_scene->addText(shortTitle);
        node.label->setDefaultTextColor(QColor(0xcdd6f4));
        QFont font = node.label->font();
        font.setPointSize(9);
        node.label->setFont(font);
        node.label->setPos(node.x - node.label->boundingRect().width() / 2,
                           node.y + NODE_RADIUS + 4);
        node.label->setZValue(2);

        m_nodes.append(node);
        m_nodeIndex[node.noteId] = i;
    }

    // Create edges from links
    QSet<QPair<qint64, qint64>> edgeSet;
    for (int i = 0; i < n; i++) {
        QVector<LinkData> links = m_repo->outgoingLinks(m_nodes[i].noteId);
        for (const LinkData &link : links) {
            if (link.targetNoteId <= 0) continue;
            if (!m_nodeIndex.contains(link.targetNoteId)) continue;

            auto pair = qMakePair(m_nodes[i].noteId, link.targetNoteId);
            auto rev  = qMakePair(link.targetNoteId, m_nodes[i].noteId);
            if (edgeSet.contains(pair) || edgeSet.contains(rev)) continue;
            edgeSet.insert(pair);

            Edge edge;
            edge.sourceId = m_nodes[i].noteId;
            edge.targetId = link.targetNoteId;
            edge.line = m_scene->addPath(QPainterPath(),
                                         QPen(QColor(0x45475a), 1));
            edge.line->setZValue(0);
            m_edges.append(edge);
        }
    }

    updateEdges();
    m_timer->start();
}

void GraphWidget::tick()
{
    if (m_iterations >= MAX_ITER) {
        m_timer->stop();
        return;
    }
    m_iterations++;

    int n = m_nodes.size();

    // Compute forces
    QVector<double> fx(n, 0), fy(n, 0);

    // Repulsion between all pairs
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double dx = m_nodes[i].x - m_nodes[j].x;
            double dy = m_nodes[i].y - m_nodes[j].y;
            double dist = qSqrt(dx * dx + dy * dy);
            if (dist < 1.0) dist = 1.0;

            double force = REPULSION / (dist * dist);
            double fxij = force * dx / dist;
            double fyij = force * dy / dist;

            fx[i] += fxij;
            fy[i] += fyij;
            fx[j] -= fxij;
            fy[j] -= fyij;
        }
    }

    // Attraction along edges
    for (const Edge &edge : m_edges) {
        int si = m_nodeIndex.value(edge.sourceId, -1);
        int ti = m_nodeIndex.value(edge.targetId, -1);
        if (si < 0 || ti < 0) continue;

        double dx = m_nodes[ti].x - m_nodes[si].x;
        double dy = m_nodes[ti].y - m_nodes[si].y;
        double dist = qSqrt(dx * dx + dy * dy);
        if (dist < 1.0) dist = 1.0;

        double force = ATTRACTION * dist;
        fx[si] += force * dx / dist;
        fy[si] += force * dy / dist;
        fx[ti] -= force * dx / dist;
        fy[ti] -= force * dy / dist;
    }

    // Center gravity (pull slightly toward origin)
    for (int i = 0; i < n; i++) {
        fx[i] -= m_nodes[i].x * 0.001;
        fy[i] -= m_nodes[i].y * 0.001;
    }

    // Apply forces with damping
    for (int i = 0; i < n; i++) {
        if (m_dragging && i == m_dragNode) continue;

        m_nodes[i].vx = (m_nodes[i].vx + fx[i]) * DAMPING;
        m_nodes[i].vy = (m_nodes[i].vy + fy[i]) * DAMPING;
        m_nodes[i].x += m_nodes[i].vx;
        m_nodes[i].y += m_nodes[i].vy;

        m_nodes[i].circle->setPos(m_nodes[i].x, m_nodes[i].y);
        m_nodes[i].label->setPos(
            m_nodes[i].x - m_nodes[i].label->boundingRect().width() / 2,
            m_nodes[i].y + NODE_RADIUS + 4);
    }

    updateEdges();
}

void GraphWidget::updateEdges()
{
    for (Edge &edge : m_edges) {
        int si = m_nodeIndex.value(edge.sourceId, -1);
        int ti = m_nodeIndex.value(edge.targetId, -1);
        if (si < 0 || ti < 0) continue;

        QPainterPath path;
        path.moveTo(m_nodes[si].x, m_nodes[si].y);
        path.lineTo(m_nodes[ti].x, m_nodes[ti].y);
        edge.line->setPath(path);
    }
}

void GraphWidget::wheelEvent(QWheelEvent *event)
{
    double factor = 1.15;
    if (event->angleDelta().y() < 0)
        factor = 1.0 / factor;
    scale(factor, factor);
}

void GraphWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QGraphicsItem *item = itemAt(event->pos());
        // Check if it's a node circle
        for (int i = 0; i < m_nodes.size(); i++) {
            if (item == m_nodes[i].circle || item == m_nodes[i].label) {
                emit noteClicked(m_nodes[i].noteId);
                // Also start drag
                m_dragging = true;
                m_dragNode = i;
                m_dragStart = mapToScene(event->pos());
                return;
            }
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void GraphWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_dragNode = -1;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && m_dragNode >= 0) {
        QPointF scenePos = mapToScene(event->pos());
        QPointF delta = scenePos - m_dragStart;
        m_dragStart = scenePos;

        Node &node = m_nodes[m_dragNode];
        node.x += delta.x();
        node.y += delta.y();
        node.circle->setPos(node.x, node.y);
        node.label->setPos(
            node.x - node.label->boundingRect().width() / 2,
            node.y + NODE_RADIUS + 4);
        updateEdges();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}
