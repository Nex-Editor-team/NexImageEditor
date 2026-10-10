#pragma once
#include "Layer.h"
#include <QColor>
#include <QPainterPath>
#include <QBrush>

// Extended shape catalogue — simple geometric primitives plus a few
// common decorative shapes, all built as a QPainterPath and filled/
// stroked in regenerate().
enum class ShapeKind {
    Rectangle,
    RoundedRectangle,
    Ellipse,
    Triangle,
    Pentagon,
    Hexagon,
    Star,
    Arrow,
    Line
};

// How the shape's interior is painted. Gradient modes "mix" fillColor
// (start) into fillColor2 (end) across the shape.
enum class ShapeFillMode { Solid, LinearGradient, RadialGradient };

class ShapeLayer : public Layer {
public:
    explicit ShapeLayer(ShapeKind kind = ShapeKind::Rectangle, QString name = "Shape");

    ShapeKind kind;
    QSize localSize{200, 200};

    ShapeFillMode fillMode = ShapeFillMode::Solid;
    QColor fillColor{Qt::red};
    QColor fillColor2{Qt::yellow};       // gradient end color ("mix" target)
    double gradientAngleDeg = 0.0;       // direction for LinearGradient (0 = left->right)

    QColor strokeColor{Qt::black};
    int strokeWidth = 0;                 // 0 = no stroke
    int cornerRadius = 20;               // used only by RoundedRectangle

    void regenerate();

    std::unique_ptr<Layer> clone() const override;

private:
    QPainterPath buildPath(const QRectF& r) const;
    QBrush buildBrush(const QRectF& r) const;
};
