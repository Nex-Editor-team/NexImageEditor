#include "HistogramWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(100);
    setMaximumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HistogramWidget::setHistogramData(const HistogramData& data) {
    m_data = data;
    m_hasData = true;
    update();
}

void HistogramWidget::clear() {
    m_data.clear();
    m_hasData = false;
    update();
}

void HistogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();
    const QRect rect(0, 0, w, h);

    // 1. Background dark container with rounded corners
    p.setPen(QPen(QColor(45, 45, 45), 1));
    p.setBrush(QColor(22, 22, 22));
    p.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);

    // 2. Grid lines for tonal zones: Blacks (0-25%), Shadows (25-50%), Highlights (50-75%), Whites (75-100%)
    p.setPen(QPen(QColor(40, 40, 40), 1, Qt::DashLine));
    for (int i = 1; i <= 3; ++i) {
        int x = (w * i) / 4;
        p.drawLine(x, 2, x, h - 2);
    }
    p.drawLine(2, h / 2, w - 2, h / 2);

    if (!m_hasData || m_data.maxCount <= 0) {
        p.setPen(QColor(90, 90, 90));
        p.drawText(rect, Qt::AlignCenter, "Biểu đồ Histogram");
        return;
    }

    double maxLog = std::log1p(static_cast<double>(m_data.maxCount));
    if (maxLog < 1e-4) maxLog = 1.0;

    auto makePath = [&](const std::array<int, 256>& channel, bool closed) -> QPainterPath {
        QPainterPath path;
        for (int i = 0; i < 256; ++i) {
            double x = (static_cast<double>(i) / 255.0) * (w - 8) + 4;
            double valLog = std::log1p(static_cast<double>(channel[i]));
            double norm = std::clamp(valLog / maxLog, 0.0, 1.0);
            double y = (h - 4) - norm * (h - 10);

            if (i == 0) {
                if (closed) path.moveTo(x, h - 4);
                path.lineTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        if (closed) {
            path.lineTo((w - 4), h - 4);
            path.closeSubpath();
        }
        return path;
    };

    // 3. Draw Luminance filled gradient
    QPainterPath lumPath = makePath(m_data.lum, true);
    QLinearGradient lumGrad(0, 0, 0, h);
    lumGrad.setColorAt(0.0, QColor(160, 160, 160, 120));
    lumGrad.setColorAt(1.0, QColor(60, 60, 60, 40));
    p.setPen(Qt::NoPen);
    p.setBrush(lumGrad);
    p.drawPath(lumPath);

    // 4. Draw Red, Green, Blue channel outlines
    p.setCompositionMode(QPainter::CompositionMode_Plus);

    // Red curve
    QPainterPath redPath = makePath(m_data.r, false);
    p.setPen(QPen(QColor(239, 68, 68, 190), 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawPath(redPath);

    // Green curve
    QPainterPath greenPath = makePath(m_data.g, false);
    p.setPen(QPen(QColor(34, 197, 94, 190), 1.3));
    p.drawPath(greenPath);

    // Blue curve
    QPainterPath bluePath = makePath(m_data.b, false);
    p.setPen(QPen(QColor(59, 130, 246, 210), 1.3));
    p.drawPath(bluePath);

    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // 5. Border overlay
    p.setPen(QPen(QColor(55, 55, 55), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 6, 6);
}
