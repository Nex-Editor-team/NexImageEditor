#pragma once
#include <QString>
#include <QImage>
#include <QColor>
#include <QUuid>
#include <QRectF>
#include <memory>
#include "../engine/Blend.h"
#include "../engine/Transform.h"
#include "../engine/ToneParameters.h"

class QPainter;

enum class LayerType { Raster, Text, Shape };

class Layer {
public:
    explicit Layer(LayerType type = LayerType::Raster, QString name = "Layer");
    virtual ~Layer() = default;

    QUuid id() const { return m_id; }
    LayerType type() const { return m_type; }

    QString name;
    bool visible = true;
    bool locked = false;
    double opacity = 1.0; // 0..1
    BlendMode blendMode = BlendMode::Normal;

    // Pixel content in the layer's own local space, i.e. before the
    // free-transform placement below is applied. For raster layers this
    // is the imported bitmap; for text/shape layers it is regenerated
    // whenever their parameters change (see TextLayer/ShapeLayer).
    QImage content;

    // Non-destructive photo adjustments (Light, Color, Curves, Presets, Effects)
    ToneParameters adjustments;
    void applyAdjustments();
    void bakeAdjustments();

    // "Mix color": blends a solid color into the layer's pixels by
    // `tintAmount` (0 = untouched, 1 = fully replaced by tintColor),
    // preserving each pixel's original alpha. Non-destructive: `content`
    // stays the original pixels, and applyTint() rebuilds a cached
    // tinted copy that render() uses instead. Call applyTint() again
    // whenever tintColor/tintAmount/content changes.
    QColor tintColor = Qt::red;
    double tintAmount = 0.0; // 0..1
    void applyTint();
    const QImage& displayContent() const {
        if (tintAmount > 0.0 && !m_tintedContent.isNull()) return m_tintedContent;
        if (!adjustments.isIdentity() && !m_adjustedContent.isNull()) return m_adjustedContent;
        return content;
    }

    // Non-destructive placement on the canvas (position/scale/rotate/shear).
    Transform::FreeTransformState transform;

    QTransform matrix() const { return transform.toQTransform(content.size()); }

    // Bounding rect of the transformed content, in canvas space.
    QRectF boundingRect() const;

    // Draws this layer onto `painter` honoring opacity/blend/transform.
    // Virtual so Text/Shape layers can regenerate content lazily before
    // drawing if a subclass wants that (default just draws `content`).
    virtual void render(QPainter& painter) const;

    virtual std::unique_ptr<Layer> clone() const;

protected:
    QUuid m_id;
    LayerType m_type;
    QImage m_adjustedContent; // cache for photo adjustments
    QImage m_tintedContent;   // cache for tint; empty when tintAmount <= 0
};

