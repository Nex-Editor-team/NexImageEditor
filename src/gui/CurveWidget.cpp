#include "CurveWidget.h"
#include "../engine/CurveSpline.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <cmath>

// ---------------- CurveGraphArea ----------------

CurveGraphArea::CurveGraphArea(QWidget* parent) : QWidget(parent) {
    setMinimumSize(180, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
}

void CurveGraphArea::setPoints(const QVector<QPointF>& points) {
    m_points = points;
    if (m_points.size() < 2) {
        m_points = {{0.0, 0.0}, {255.0, 255.0}};
    }
    update();
}

void CurveGraphArea::setChannel(CurveChannel ch) {
    m_channel = ch;
    update();
}

QPointF CurveGraphArea::widgetToGraph(const QPointF& p) const {
    const double pad = 8.0;
    double w = width() - 2.0 * pad;
    double h = height() - 2.0 * pad;
    if (w <= 0.0 || h <= 0.0) return {0.0, 0.0};

    double gx = std::clamp(((p.x() - pad) / w) * 255.0, 0.0, 255.0);
    double gy = std::clamp((1.0 - (p.y() - pad) / h) * 255.0, 0.0, 255.0);
    return {gx, gy};
}

QPointF CurveGraphArea::graphToWidget(const QPointF& p) const {
    const double pad = 8.0;
    double w = width() - 2.0 * pad;
    double h = height() - 2.0 * pad;

    double wx = pad + (p.x() / 255.0) * w;
    double wy = pad + (1.0 - (p.y() / 255.0)) * h;
    return {wx, wy};
}

int CurveGraphArea::hitTestPoint(const QPointF& widgetPos) const {
    const double hitRadiusSq = 9.0 * 9.0;
    for (int i = 0; i < m_points.size(); ++i) {
        QPointF ptW = graphToWidget(m_points[i]);
        double dx = ptW.x() - widgetPos.x();
        double dy = ptW.y() - widgetPos.y();
        if (dx * dx + dy * dy <= hitRadiusSq) {
            return i;
        }
    }
    return -1;
}

void CurveGraphArea::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const double pad = 8.0;
    const QRectF area(pad, pad, w - 2.0 * pad, h - 2.0 * pad);

    // 1. Background dark container
    p.setPen(QPen(QColor(45, 45, 45), 1));
    p.setBrush(QColor(24, 24, 24));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    // 2. Coordinate Grid 4x4
    p.setPen(QPen(QColor(40, 40, 40), 1, Qt::DashLine));
    for (int i = 1; i <= 3; ++i) {
        double gx = area.left() + (area.width() * i) / 4.0;
        double gy = area.top() + (area.height() * i) / 4.0;
        p.drawLine(QPointF(gx, area.top()), QPointF(gx, area.bottom()));
        p.drawLine(QPointF(area.left(), gy), QPointF(area.right(), gy));
    }

    // 3. Diagonal reference line (identity)
    p.setPen(QPen(QColor(55, 55, 55), 1, Qt::DotLine));
    p.drawLine(area.bottomLeft(), area.topRight());

    // 4. Draw interpolated spline curve
    std::array<int, 256> lut = CurveSpline::buildLut(m_points);
    QPainterPath curvePath;
    for (int i = 0; i < 256; ++i) {
        QPointF gPt(i, lut[i]);
        QPointF wPt = graphToWidget(gPt);
        if (i == 0) curvePath.moveTo(wPt);
        else curvePath.lineTo(wPt);
    }

    QColor channelColor = Qt::white;
    switch (m_channel) {
        case CurveChannel::RGB:   channelColor = QColor(240, 240, 240); break;
        case CurveChannel::Red:   channelColor = QColor(239, 68, 68); break;
        case CurveChannel::Green: channelColor = QColor(34, 197, 94); break;
        case CurveChannel::Blue:  channelColor = QColor(59, 130, 246); break;
    }

    p.setPen(QPen(channelColor, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curvePath);

    // 5. Draw control point handles
    for (int i = 0; i < m_points.size(); ++i) {
        QPointF ptW = graphToWidget(m_points[i]);
        bool active = (i == m_dragIndex || i == m_hoverIndex);
        double r = active ? 5.5 : 4.0;

        p.setPen(QPen(channelColor, 2.0));
        p.setBrush(active ? channelColor : QColor(30, 30, 30));
        p.drawEllipse(ptW, r, r);
    }
}

void CurveGraphArea::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int idx = hitTestPoint(event->position());
        if (idx >= 0) {
            m_dragIndex = idx;
        } else {
            // Add a new control point at click location
            QPointF gPt = widgetToGraph(event->position());
            m_points.push_back(gPt);
            std::sort(m_points.begin(), m_points.end(), [](const QPointF& a, const QPointF& b) {
                return a.x() < b.x();
            });
            m_dragIndex = hitTestPoint(event->position());
            emit curvePointsChanged(m_points);
        }
        update();
    } else if (event->button() == Qt::RightButton) {
        // Right click removes point
        int idx = hitTestPoint(event->position());
        if (idx > 0 && idx < m_points.size() - 1 && m_points.size() > 2) {
            m_points.removeAt(idx);
            emit curvePointsChanged(m_points);
            update();
        }
    }
}

void CurveGraphArea::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        QPointF gPt = widgetToGraph(event->position());

        // Constrain X coordinate between adjacent points for non-endpoints
        if (m_dragIndex == 0) {
            gPt.setX(0.0);
        } else if (m_dragIndex == m_points.size() - 1) {
            gPt.setX(255.0);
        } else {
            double minX = m_points[m_dragIndex - 1].x() + 1.0;
            double maxX = m_points[m_dragIndex + 1].x() - 1.0;
            gPt.setX(std::clamp(gPt.x(), minX, maxX));
        }
        m_points[m_dragIndex] = gPt;
        emit curvePointsChanged(m_points);
        update();
    } else {
        int oldHover = m_hoverIndex;
        m_hoverIndex = hitTestPoint(event->position());
        if (m_hoverIndex != oldHover) {
            setCursor((m_hoverIndex >= 0) ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }
}

void CurveGraphArea::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragIndex = -1;
        update();
    }
}

void CurveGraphArea::mouseDoubleClickEvent(QMouseEvent* event) {
    int idx = hitTestPoint(event->position());
    if (idx > 0 && idx < m_points.size() - 1 && m_points.size() > 2) {
        m_points.removeAt(idx);
        emit curvePointsChanged(m_points);
        update();
    }
}

// ---------------- CurveWidget ----------------

CurveWidget::CurveWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(6);

    // Channel Selector Header
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(4);

    m_btnRgb = new QPushButton("RGB", this);
    m_btnRed = new QPushButton("Đỏ", this);
    m_btnGreen = new QPushButton("Lục", this);
    m_btnBlue = new QPushButton("Lam", this);
    m_btnReset = new QPushButton("↺", this);
    m_btnReset->setToolTip("Đặt lại đường cong kênh này");
    m_btnReset->setFixedWidth(28);

    for (auto* btn : {m_btnRgb, m_btnRed, m_btnGreen, m_btnBlue}) {
        btn->setCheckable(true);
        topRow->addWidget(btn);
    }
    m_btnRgb->setChecked(true);
    topRow->addWidget(m_btnReset);
    layout->addLayout(topRow);

    m_graphArea = new CurveGraphArea(this);
    layout->addWidget(m_graphArea);

    auto updateBtnStyles = [this]() {
        m_btnRgb->setStyleSheet(m_btnRgb->isChecked()
            ? "background-color: #444444; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 4px;"
            : "background-color: #282828; color: #bbbbbb; border-radius: 4px; padding: 4px;");
        m_btnRed->setStyleSheet(m_btnRed->isChecked()
            ? "background-color: #991b1b; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 4px;"
            : "background-color: #282828; color: #f87171; border-radius: 4px; padding: 4px;");
        m_btnGreen->setStyleSheet(m_btnGreen->isChecked()
            ? "background-color: #166534; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 4px;"
            : "background-color: #282828; color: #4ade80; border-radius: 4px; padding: 4px;");
        m_btnBlue->setStyleSheet(m_btnBlue->isChecked()
            ? "background-color: #1e40af; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 4px;"
            : "background-color: #282828; color: #60a5fa; border-radius: 4px; padding: 4px;");
    };

    updateBtnStyles();

    auto selectChannel = [this, updateBtnStyles](CurveChannel ch, QPushButton* activeBtn) {
        m_btnRgb->setChecked(false);
        m_btnRed->setChecked(false);
        m_btnGreen->setChecked(false);
        m_btnBlue->setChecked(false);
        activeBtn->setChecked(true);
        m_activeChannel = ch;
        m_graphArea->setChannel(ch);
        updateActiveChannelGraph();
        updateBtnStyles();
    };

    connect(m_btnRgb, &QPushButton::clicked, this, [=]() { selectChannel(CurveChannel::RGB, m_btnRgb); });
    connect(m_btnRed, &QPushButton::clicked, this, [=]() { selectChannel(CurveChannel::Red, m_btnRed); });
    connect(m_btnGreen, &QPushButton::clicked, this, [=]() { selectChannel(CurveChannel::Green, m_btnGreen); });
    connect(m_btnBlue, &QPushButton::clicked, this, [=]() { selectChannel(CurveChannel::Blue, m_btnBlue); });

    connect(m_btnReset, &QPushButton::clicked, this, [this]() {
        QVector<QPointF> defaultCurve{{0.0, 0.0}, {255.0, 255.0}};
        switch (m_activeChannel) {
            case CurveChannel::RGB:   m_rgb = defaultCurve; break;
            case CurveChannel::Red:   m_red = defaultCurve; break;
            case CurveChannel::Green: m_green = defaultCurve; break;
            case CurveChannel::Blue:  m_blue = defaultCurve; break;
        }
        m_graphArea->setPoints(defaultCurve);
        emit curvesChanged();
    });

    connect(m_graphArea, &CurveGraphArea::curvePointsChanged, this, [this](const QVector<QPointF>& pts) {
        switch (m_activeChannel) {
            case CurveChannel::RGB:   m_rgb = pts; break;
            case CurveChannel::Red:   m_red = pts; break;
            case CurveChannel::Green: m_green = pts; break;
            case CurveChannel::Blue:  m_blue = pts; break;
        }
        emit curvesChanged();
    });
}

void CurveWidget::updateActiveChannelGraph() {
    switch (m_activeChannel) {
        case CurveChannel::RGB:   m_graphArea->setPoints(m_rgb); break;
        case CurveChannel::Red:   m_graphArea->setPoints(m_red); break;
        case CurveChannel::Green: m_graphArea->setPoints(m_green); break;
        case CurveChannel::Blue:  m_graphArea->setPoints(m_blue); break;
    }
}

void CurveWidget::setCurves(const QVector<QPointF>& rgb,
                           const QVector<QPointF>& red,
                           const QVector<QPointF>& green,
                           const QVector<QPointF>& blue) {
    m_rgb = rgb;
    m_red = red;
    m_green = green;
    m_blue = blue;
    updateActiveChannelGraph();
}

void CurveWidget::resetAllCurves() {
    QVector<QPointF> defaultCurve{{0.0, 0.0}, {255.0, 255.0}};
    m_rgb = defaultCurve;
    m_red = defaultCurve;
    m_green = defaultCurve;
    m_blue = defaultCurve;
    updateActiveChannelGraph();
    emit curvesChanged();
}
