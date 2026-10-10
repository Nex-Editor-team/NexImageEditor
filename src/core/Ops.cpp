#include "core/Ops.h"

#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <deque>

namespace Ops {

void applyPixelFn(Layer& layer, const Selection* sel, const std::function<void(Pixels&)>& fn) {
    Pixels orig = toPixels(layer.image);
    Pixels work = orig;
    fn(work);
    if (!sel) {
        layer.image = fromPixels(work);
        return;
    }
    for (int y = 0; y < orig.h; ++y)
        for (int x = 0; x < orig.w; ++x) {
            int a = sel->at(x + layer.x, y + layer.y);
            if (a == 0) {
                std::copy(orig.px(x, y), orig.px(x, y) + 4, work.px(x, y));
            } else if (a < 255) {
                for (int c = 0; c < 4; ++c)
                    work.px(x, y)[c] = uint8_t((work.px(x, y)[c] * a + orig.px(x, y)[c] * (255 - a)) / 255);
            }
        }
    layer.image = fromPixels(work);
}

std::vector<uint8_t> rectMask(const QRect& r, int w, int h) {
    std::vector<uint8_t> m(size_t(w) * size_t(h), 0);
    QRect c = r.intersected(QRect(0, 0, w, h));
    for (int y = c.top(); y <= c.bottom(); ++y)
        for (int x = c.left(); x <= c.right(); ++x) m[size_t(y) * size_t(w) + size_t(x)] = 255;
    return m;
}

static std::vector<uint8_t> pathMask(const QPainterPath& path, int w, int h) {
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillPath(path, Qt::white);
    p.end();
    std::vector<uint8_t> m(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) m[size_t(y) * size_t(w) + size_t(x)] = uint8_t(qAlpha(img.pixel(x, y)));
    return m;
}

std::vector<uint8_t> ellipseMask(const QRect& r, int w, int h) {
    QPainterPath p;
    p.addEllipse(QRectF(r));
    return pathMask(p, w, h);
}

std::vector<uint8_t> polygonMask(const QPolygonF& poly, int w, int h) {
    QPainterPath p;
    p.addPolygon(poly);
    p.closeSubpath();
    return pathMask(p, w, h);
}

std::vector<uint8_t> floodMask(const QImage& srcImg, QPoint seed, int tolerance, bool contiguous) {
    Pixels p = toPixels(srcImg);
    const int w = p.w, h = p.h;
    std::vector<uint8_t> out(size_t(w) * size_t(h), 0);
    if (seed.x() < 0 || seed.y() < 0 || seed.x() >= w || seed.y() >= h) return out;
    const uint8_t* ref = p.px(seed.x(), seed.y());
    uint8_t r0 = ref[0], g0 = ref[1], b0 = ref[2], a0 = ref[3];
    auto matches = [&](int x, int y) {
        const uint8_t* q = p.px(x, y);
        int d = std::max({std::abs(q[0] - r0), std::abs(q[1] - g0), std::abs(q[2] - b0), std::abs(q[3] - a0)});
        return d <= tolerance;
    };
    if (contiguous) {
        std::deque<QPoint> q{seed};
        out[size_t(seed.y()) * size_t(w) + size_t(seed.x())] = 255;
        while (!q.empty()) {
            QPoint c = q.front();
            q.pop_front();
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k) {
                int nx = c.x() + dx[k], ny = c.y() + dy[k];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                size_t idx = size_t(ny) * size_t(w) + size_t(nx);
                if (out[idx] || !matches(nx, ny)) continue;
                out[idx] = 255;
                q.emplace_back(nx, ny);
            }
        }
    } else {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (matches(x, y)) out[size_t(y) * size_t(w) + size_t(x)] = 255;
    }
    return out;
}

// Paints with `painter` clipped to the selection, translated into layer coordinates.
template <typename Fn>
static void paintInLayer(Layer& layer, const Selection* sel, Fn fn) {
    QPainter p(&layer.image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(-layer.x, -layer.y);
    if (sel) {
        p.setClipPath(sel->clip);
    }
    fn(p);
    p.end();
}

void strokeBrush(Layer& layer, const Selection* sel, const QPointF& a, const QPointF& b,
                 double size, const QColor& color, double hardness, bool erase) {
    paintInLayer(layer, sel, [&](QPainter& p) {
        QPen pen(color, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (erase) p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        if (hardness >= 0.999) {
            p.setPen(pen);
            p.drawLine(a, b);
            if (a == b) p.drawPoint(a);
            return;
        }
        // Soft brush: stack concentric strokes; outer rings are fainter.
        const int steps = 6;
        for (int i = 0; i < steps; ++i) {
            double f = 1.0 - (1.0 - hardness) * double(i) / steps;
            QColor c = color;
            c.setAlphaF(color.alphaF() / steps * 1.6);
            p.setPen(QPen(c, size * f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(a, b);
        }
    });
}

void fillWithMask(Layer& layer, const std::vector<uint8_t>& layerMask, const QColor& color) {
    // layerMask has the layer image's size (layer coordinates); it already encodes the selection.
    Pixels p = toPixels(layer.image);
    const double srcA = color.alphaF();
    const double src[3] = {color.redF() * 255, color.greenF() * 255, color.blueF() * 255};
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) {
            double sa = srcA * layerMask[size_t(y) * size_t(p.w) + size_t(x)] / 255.0;
            if (sa <= 0) continue;
            uint8_t* q = p.px(x, y);
            double da = q[3] / 255.0;
            double oa = sa + da * (1 - sa);
            for (int c = 0; c < 3; ++c)
                q[c] = uint8_t(oa > 0 ? std::lround((src[c] * sa + q[c] * da * (1 - sa)) / oa) : 0);
            q[3] = uint8_t(std::lround(oa * 255));
        }
    layer.image = fromPixels(p);
}

void fillGradient(Layer& layer, const Selection* sel, const QPointF& from, const QPointF& to,
                  const QColor& c1, const QColor& c2) {
    paintInLayer(layer, sel, [&](QPainter& p) {
        QLinearGradient g(from, to);
        g.setColorAt(0, c1);
        g.setColorAt(1, c2);
        p.fillRect(QRectF(layer.x, layer.y, layer.w(), layer.h()), g);
    });
}

void drawShape(Layer& layer, const Selection* sel, const QRectF& rect, bool ellipse,
               const QColor& fill, const QColor& stroke, double strokeW) {
    paintInLayer(layer, sel, [&](QPainter& p) {
        p.setBrush(fill.alpha() > 0 ? QBrush(fill) : Qt::NoBrush);
        p.setPen(strokeW > 0 && stroke.alpha() > 0 ? QPen(stroke, strokeW) : Qt::NoPen);
        if (ellipse) p.drawEllipse(rect);
        else p.drawRect(rect);
    });
}

void drawText(Layer& layer, const Selection* sel, const QPointF& pos, const QString& text,
              const QFont& font, const QColor& color) {
    paintInLayer(layer, sel, [&](QPainter& p) {
        p.setFont(font);
        p.setPen(color);
        p.drawText(pos, text);
    });
}

void moveSelectedPixels(Layer& layer, const Selection& sel, int dx, int dy) {
    // Extract the selected pixels (layer coordinates) and erase them from the source.
    Pixels src = toPixels(layer.image);
    Pixels out = src;
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            uint8_t a = sel.at(x + layer.x, y + layer.y);
            if (a == 0) continue;
            for (int c = 0; c < 4; ++c)
                out.px(x, y)[c] = uint8_t(out.px(x, y)[c] * (255 - a) / 255);
        }
    QImage base = fromPixels(out);
    QPainter p(&base);
    for (int y = 0; y < src.h; ++y)
        for (int x = 0; x < src.w; ++x) {
            uint8_t a = sel.at(x + layer.x, y + layer.y);
            if (a == 0) continue;
            int tx = x + dx, ty = y + dy;
            if (tx < 0 || ty < 0 || tx >= src.w || ty >= src.h) continue;
            QColor c(src.px(x, y)[0], src.px(x, y)[1], src.px(x, y)[2], int(src.px(x, y)[3] * a / 255));
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.fillRect(QRect(tx, ty, 1, 1), c);
        }
    p.end();
    layer.image = base.convertToFormat(kPremult);
}

QRect transformedBounds(const QRect& r, const QTransform& t) {
    return t.mapRect(QRectF(r)).toAlignedRect();
}

void transformLayer(Layer& layer, const QTransform& t) {
    QRect dst = transformedBounds(QRect(0, 0, layer.w(), layer.h()), t);
    QImage out = newImage(dst.width(), dst.height(), Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setTransform(QTransform::fromTranslate(-dst.x(), -dst.y()) * t);
    p.drawImage(0, 0, layer.image);
    p.end();
    layer.image = out;
    layer.x += dst.x();
    layer.y += dst.y();
}

QImage cropImage(const QImage& img, const QRect& r) {
    return img.copy(r).convertToFormat(kPremult);
}

void cropDocument(Document& doc, const QRect& r) {
    for (Layer& l : doc.layers()) {
        l.x -= r.x();
        l.y -= r.y();
    }
    doc.setSize(r.width(), r.height());
    doc.selection.reset();
}

void resizeImage(Document& doc, int w, int h) {
    const double sx = double(w) / doc.width(), sy = double(h) / doc.height();
    for (Layer& l : doc.layers()) {
        l.image = l.image.scaled(std::max(1, int(std::lround(l.w() * sx))),
                                 std::max(1, int(std::lround(l.h() * sy))),
                                 Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                      .convertToFormat(kPremult);
        l.x = int(std::lround(l.x * sx));
        l.y = int(std::lround(l.y * sy));
    }
    doc.setSize(w, h);
    doc.selection.reset();
}

void resizeCanvas(Document& doc, int w, int h, int anchorX, int anchorY) {
    const int dx = (w - doc.width()) * anchorX / 2;
    const int dy = (h - doc.height()) * anchorY / 2;
    for (Layer& l : doc.layers()) {
        l.x += dx;
        l.y += dy;
    }
    doc.setSize(w, h);
    doc.selection.reset();
}

void rotateDocument(Document& doc, int degrees) {
    const int W = doc.width(), H = doc.height();
    QTransform t;
    int nw = W, nh = H;
    if (degrees == 90) {
        t.translate(H, 0).rotate(90);
        nw = H;
        nh = W;
    } else if (degrees == -90) {
        t.translate(0, W).rotate(-90);
        nw = H;
        nh = W;
    } else {
        t.translate(W, H).rotate(180);
    }
    for (Layer& l : doc.layers()) {
        QTransform lt = QTransform::fromTranslate(l.x, l.y) * t;
        QRect r = transformedBounds(QRect(0, 0, l.w(), l.h()), lt);
        QImage out = newImage(r.width(), r.height(), Qt::transparent);
        QPainter p(&out);
        p.setTransform(QTransform::fromTranslate(-r.x(), -r.y()) * lt);
        p.drawImage(0, 0, l.image);
        p.end();
        l.image = out;
        l.x = r.x();
        l.y = r.y();
    }
    doc.setSize(nw, nh);
    doc.selection.reset();
}

void flipDocument(Document& doc, bool horizontal) {
    const int W = doc.width(), H = doc.height();
    for (Layer& l : doc.layers()) {
        if (horizontal) {
            l.image = l.image.mirrored(true, false);
            l.x = W - l.x - l.w();
        } else {
            l.image = l.image.mirrored(false, true);
            l.y = H - l.y - l.h();
        }
    }
    doc.selection.reset();
}

}  // namespace Ops

namespace Ops {
std::vector<uint8_t> layerMaskFromSelection(const Layer& layer, const Selection* sel) {
    std::vector<uint8_t> m(size_t(layer.w()) * size_t(layer.h()), 255);
    if (!sel) return m;
    for (int y = 0; y < layer.h(); ++y)
        for (int x = 0; x < layer.w(); ++x)
            m[size_t(y) * size_t(layer.w()) + size_t(x)] = sel->at(x + layer.x, y + layer.y);
    return m;
}
}  // namespace Ops
