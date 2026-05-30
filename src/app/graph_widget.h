#pragma once

#include <QGraphicsView>
#include <QVector>
#include <QMap>

class QGraphicsScene;
class QGraphicsEllipseItem;
class QGraphicsTextItem;
class QGraphicsPathItem;
class QTimer;
class NoteRepository;
struct NoteData;
struct LinkData;

/// Force-directed knowledge graph visualization.
class GraphWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit GraphWidget(NoteRepository *repo, QWidget *parent = nullptr);

    /// Rebuild the graph from all notes and links.
    void rebuild();

signals:
    /// Emitted when user clicks a note node.
    void noteClicked(qint64 noteId);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void tick();          // one step of force-directed layout
    void updateEdges();   // redraw all edge paths

    struct Node {
        qint64 noteId;
        QString title;
        QGraphicsEllipseItem *circle = nullptr;
        QGraphicsTextItem   *label  = nullptr;
        double x = 0, y = 0;
        double vx = 0, vy = 0;
    };

    struct Edge {
        qint64 sourceId;
        qint64 targetId;
        QGraphicsPathItem *line = nullptr;
    };

    QGraphicsScene *m_scene;
    NoteRepository *m_repo;
    QVector<Node> m_nodes;
    QVector<Edge> m_edges;
    QMap<qint64, int> m_nodeIndex;  // noteId -> index
    QTimer *m_timer;
    int m_iterations = 0;

    // Drag state
    bool m_dragging = false;
    int m_dragNode = -1;
    QPointF m_dragStart;
};
