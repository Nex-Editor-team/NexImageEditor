#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QPushButton>
#include <QHBoxLayout>

enum class CurveChannel {
    RGB = 0,
    Red = 1,
    Green = 2,
    Blue = 3
};

class CurveGraphArea : public QWidget {
    Q_OBJECT
public:
    explicit CurveGraphArea(QWidget* parent = nullptr);

    void setPoints(const QVector<QPointF>& points);
    const QVector<QPointF>& points() const { return m_points; }
    void setChannel(CurveChannel ch);

signals:
    void curvePointsChanged(const QVector<QPointF>& points);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QPointF widgetToGraph(const QPointF& p) const;
    QPointF graphToWidget(const QPointF& p) const;
    int hitTestPoint(const QPointF& widgetPos) const;

    QVector<QPointF> m_points{{0.0, 0.0}, {255.0, 255.0}};
    CurveChannel m_channel = CurveChannel::RGB;
    int m_dragIndex = -1;
    int m_hoverIndex = -1;
};

class CurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit CurveWidget(QWidget* parent = nullptr);

    void setCurves(const QVector<QPointF>& rgb,
                   const QVector<QPointF>& red,
                   const QVector<QPointF>& green,
                   const QVector<QPointF>& blue);

    const QVector<QPointF>& curveRgb() const { return m_rgb; }
    const QVector<QPointF>& curveRed() const { return m_red; }
    const QVector<QPointF>& curveGreen() const { return m_green; }
    const QVector<QPointF>& curveBlue() const { return m_blue; }

    void resetAllCurves();

signals:
    void curvesChanged();

private:
    void updateActiveChannelGraph();

    CurveGraphArea* m_graphArea;
    QPushButton* m_btnRgb;
    QPushButton* m_btnRed;
    QPushButton* m_btnGreen;
    QPushButton* m_btnBlue;
    QPushButton* m_btnReset;

    CurveChannel m_activeChannel = CurveChannel::RGB;
    QVector<QPointF> m_rgb{{0.0, 0.0}, {255.0, 255.0}};
    QVector<QPointF> m_red{{0.0, 0.0}, {255.0, 255.0}};
    QVector<QPointF> m_green{{0.0, 0.0}, {255.0, 255.0}};
    QVector<QPointF> m_blue{{0.0, 0.0}, {255.0, 255.0}};
};
