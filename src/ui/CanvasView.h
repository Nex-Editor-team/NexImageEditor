#pragma once
#include "core/Document.h"
#include "tools/Tool.h"

#include <QPointer>
#include <QUndoStack>
#include <QWidget>
#include <optional>

class QTimer;

// One open document: owns the Document, its undo stack, and the active tool.
// Implements ToolHost so tools can edit through the undo system.
class CanvasView : public QWidget, public ToolHost {
    Q_OBJECT
public:
    CanvasView(Document doc, ToolOptions* options, QWidget* parent = nullptr);
    ~CanvasView() override;

    Document& document() { return m_doc; }
    const Document& document() const { return m_doc; }
    QUndoStack* undoStack() { return &m_undo; }

    double zoom() const { return m_zoom; }
    void setZoom(double z, const QPointF& anchorWidget);
    void fitToWindow();
    void setTool(ToolId id);
    ToolId toolId() const { return m_toolId; }
    Tool* tool() { return m_tool.get(); }

    // Undoable document-level edit (layers, size, pixels). fn runs between begin/commit.
    void editDocument(const QString& name, const std::function<void(Document&)>& fn);
    // Layer-level helpers.
    void selectLayer(int index);
    void addLayer();
    void duplicateLayer();
    void deleteLayer();
    void mergeDownLayer();
    void moveLayer(int from, int to);
    void editLayer(int index, const QString& name, const std::function<void(Layer&)>& fn);
    void flattenImage();

    // ToolHost
    Document& doc() override { return m_doc; }
    ToolOptions& options() override { return *m_opts; }
    void beginEdit(const QString& name) override;
    void commitEdit() override;
    void cancelEdit() override;
    void update() override { QWidget::update(); }
    void documentChanged() override;
    void setStatus(const QString& text) override;
    QString promptText() override;
    void zoomAt(const QPointF& canvasPos, bool zoomIn) override;

signals:
    void changed();              // layers/selection/size changed; refresh panels
    void statusMessage(const QString& text);
    void toolChanged(ToolId id);
    void zoomChanged(double zoom);
    void pointerMoved(const QPoint& canvasPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QPointF toCanvas(const QPointF& widgetPos) const;
    QPointF toWidget(const QPointF& canvasPos) const;
    void clampOrigin();
    void restoreState(const DocState& s);

    Document m_doc;
    QUndoStack m_undo;
    ToolOptions* m_opts;
    std::unique_ptr<Tool> m_tool;
    ToolId m_toolId = ToolId::Move;
    double m_zoom = 1.0;
    QPointF m_origin;  // widget position of canvas pixel (0,0)
    bool m_panning = false;
    bool m_spaceDown = false;
    QPoint m_panLast;
    std::optional<DocState> m_pending;
    QString m_pendingName;
    bool m_initialFit = false;
    double m_antsPhase = 0;
    QTimer* m_antsTimer;
    friend class EditCommand;
};
