#pragma once
#include "Layer.h"
#include <QFont>
#include <QColor>

// A layer whose `content` bitmap is regenerated from text parameters
// rather than imported/painted directly.
class TextLayer : public Layer {
public:
    explicit TextLayer(QString text = "Text", QString name = "Text Layer");

    QString text;
    QFont font{"Sans Serif", 32};
    QColor color{Qt::white};

    // Rebuilds `content` from text/font/color. Call after editing any of
    // the fields above; kept explicit (not auto-triggered) so batch edits
    // from the Properties panel only rasterize once.
    void regenerate();

    std::unique_ptr<Layer> clone() const override;
};
