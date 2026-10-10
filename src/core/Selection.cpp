#include "core/Selection.h"

#include <QImage>
#include <QPainter>
#include <algorithm>

QRect Selection::bounds() const {
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (mask[size_t(y) * size_t(w) + size_t(x)] > 127) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
    if (x1 < 0) return QRect();
    return QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

std::shared_ptr<Selection> Selection::fromMask(std::vector<uint8_t> mask, int w, int h) {
    auto s = std::make_shared<Selection>();
    s->w = w;
    s->h = h;
    s->mask = std::move(mask);
    bool any = false;
    for (int y = 0; y < h; ++y) {
        int x = 0;
        while (x < w) {
            if (s->at(x, y) > 127) {
                any = true;
                int start = x;
                while (x < w && s->at(x, y) > 127) ++x;
                s->clip.addRect(QRectF(start, y, x - start, 1));
            } else {
                ++x;
            }
        }
    }
    if (!any) return nullptr;
    // Boundary edges between covered and uncovered pixels.
    QPainterPath outline;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (s->at(x, y) <= 127) continue;
            if (s->at(x, y - 1) <= 127) { outline.moveTo(x, y); outline.lineTo(x + 1, y); }
            if (s->at(x, y + 1) <= 127) { outline.moveTo(x, y + 1); outline.lineTo(x + 1, y + 1); }
            if (s->at(x - 1, y) <= 127) { outline.moveTo(x, y); outline.lineTo(x, y + 1); }
            if (s->at(x + 1, y) <= 127) { outline.moveTo(x + 1, y); outline.lineTo(x + 1, y + 1); }
        }
    s->outline = outline;
    return s;
}

std::shared_ptr<Selection> Selection::fromPath(const QPainterPath& path, int w, int h) {
    QImage img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillPath(path, Qt::white);
    p.end();
    std::vector<uint8_t> mask(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mask[size_t(y) * size_t(w) + size_t(x)] = uint8_t(qAlpha(img.pixel(x, y)));
    return fromMask(std::move(mask), w, h);
}

SelectionPtr combineSelections(const SelectionPtr& base, const SelectionPtr& add, SelOp op,
                               int w, int h) {
    if (op == SelOp::Replace || !base) return op == SelOp::Subtract ? base : add;
    if (!add) return base;
    std::vector<uint8_t> mask(size_t(w) * size_t(h));
    for (size_t i = 0; i < mask.size(); ++i) {
        uint8_t a = base->mask[i];
        uint8_t b = add->mask[i];
        mask[i] = op == SelOp::Add ? std::max(a, b) : std::min(a, uint8_t(255 - b));
    }
    return Selection::fromMask(std::move(mask), w, h);
}
