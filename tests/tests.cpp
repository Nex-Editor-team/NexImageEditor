// Plain C++ test runner (no QtTest). Exit code 0 = all checks passed.
#include "core/Adjustments.h"
#include "core/Document.h"
#include "core/FileIO.h"
#include "core/Ops.h"
#include "core/Pixels.h"
#include "core/Selection.h"

#include <QColor>
#include <QPainter>
#include <QTemporaryDir>
#include <cstdio>

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (cond) {                                                              \
            ++g_passed;                                                          \
        } else {                                                                 \
            ++g_failed;                                                          \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

static void testPixelRoundTrip() {
    QImage img = newImage(4, 3, QColor(10, 20, 30, 255));
    Pixels p = toPixels(img);
    CHECK(p.w == 4 && p.h == 3);
    CHECK(p.px(2, 1)[0] == 10 && p.px(2, 1)[1] == 20 && p.px(2, 1)[2] == 30 && p.px(2, 1)[3] == 255);
    QImage back = fromPixels(p);
    CHECK(back.width() == 4 && back.height() == 3);
    CHECK(QColor::fromRgba(back.pixel(0, 0)).red() == 10);
}

static void testSelectionAndCombine() {
    const int W = 10, H = 10;
    auto a = Selection::fromMask(Ops::rectMask(QRect(0, 0, 5, 10), W, H), W, H);
    auto b = Selection::fromMask(Ops::rectMask(QRect(3, 0, 5, 10), W, H), W, H);
    CHECK(a && b);
    CHECK(a->bounds() == QRect(0, 0, 5, 10));
    CHECK(!a->outline.isEmpty());
    auto add = combineSelections(a, b, SelOp::Add, W, H);
    CHECK(add && add->bounds() == QRect(0, 0, 8, 10));
    auto sub = combineSelections(a, b, SelOp::Subtract, W, H);
    CHECK(sub && sub->bounds() == QRect(0, 0, 3, 10));
    auto empty = Selection::fromMask(std::vector<uint8_t>(W * H, 0), W, H);
    CHECK(empty == nullptr);
    CHECK(combineSelections(a, b, SelOp::Replace, W, H) == b);
}

static void testFloodFill() {
    QImage img = newImage(6, 6, Qt::white);
    QPainter p(&img);
    p.fillRect(QRect(2, 0, 1, 6), Qt::black);  // vertical wall splits the image
    p.end();
    auto contiguous = Ops::floodMask(img, QPoint(0, 0), 10, true);
    int count = 0;
    for (uint8_t v : contiguous) count += v ? 1 : 0;
    CHECK(count == 2 * 6);  // only the left strip
    auto global = Ops::floodMask(img, QPoint(0, 0), 10, false);
    count = 0;
    for (uint8_t v : global) count += v ? 1 : 0;
    CHECK(count == 5 * 6);  // every white pixel
}

static void testAdjustments() {
    Pixels p = toPixels(newImage(1, 1, QColor(100, 150, 200, 255)));
    Adjust::invert(p);
    CHECK(p.px(0, 0)[0] == 155 && p.px(0, 0)[1] == 105 && p.px(0, 0)[2] == 55);
    Adjust::grayscale(p);
    CHECK(p.px(0, 0)[0] == p.px(0, 0)[1] && p.px(0, 0)[1] == p.px(0, 0)[2]);
    Pixels q = toPixels(newImage(1, 1, QColor(128, 128, 128, 255)));
    Adjust::brightnessContrast(q, 0, 0);
    CHECK(q.px(0, 0)[0] == 128);
}

static void testFillWithMask() {
    Document d(4, 4, Qt::white);
    Layer& l = d.layers()[0];
    std::vector<uint8_t> mask(16, 0);
    mask[5] = 255;
    Ops::fillWithMask(l, mask, QColor(255, 0, 0, 255));
    Pixels p = toPixels(l.image);
    CHECK(p.px(1, 1)[0] == 255 && p.px(1, 1)[1] == 0 && p.px(1, 1)[2] == 0);
    CHECK(p.px(0, 0)[0] == 255 && p.px(0, 0)[1] == 255);
}

static void testDocumentLayers() {
    Document d(20, 10, Qt::white);
    CHECK(d.layers().size() == 1);
    d.addLayer("A", newImage(20, 10));
    d.addLayer("B", newImage(20, 10));
    CHECK(d.layers().size() == 3);
    CHECK(d.activeIndex() == 2);
    d.moveLayer(2, 0);
    CHECK(d.layers()[0].name == "B" && d.activeIndex() == 0);
    d.removeLayer(0);
    CHECK(d.layers().size() == 2);
    d.flattenAll();
    CHECK(d.layers().size() == 1);
}

static void testCropRotateResize() {
    Document d(20, 10, Qt::white);
    Ops::cropDocument(d, QRect(5, 2, 8, 4));
    CHECK(d.width() == 8 && d.height() == 4);
    CHECK(d.layers()[0].x == -5 && d.layers()[0].y == -2);  // cropping moves layers, keeps pixels
    Ops::rotateDocument(d, 90);
    CHECK(d.width() == 4 && d.height() == 8);
    Ops::resizeImage(d, 16, 16);
    CHECK(d.width() == 16 && d.height() == 16);
    CHECK(!d.layers()[0].image.isNull());
    const int x0 = d.layers()[0].x;
    Ops::resizeCanvas(d, 20, 20, 2, 2);  // anchor right: content shifts by half the extra width
    CHECK(d.width() == 20 && d.height() == 20);
    CHECK(d.layers()[0].x == x0 + 4);
}

static void testSnapshotRestore() {
    Document d(8, 8, Qt::white);
    DocState before = d.snapshot();
    Ops::fillWithMask(d.layers()[0], std::vector<uint8_t>(64, 255), QColor(0, 0, 0, 255));
    CHECK(toPixels(d.layers()[0].image).px(0, 0)[0] == 0);
    d.restore(before);
    CHECK(toPixels(d.layers()[0].image).px(0, 0)[0] == 255);
}

static void testProjectRoundTrip() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.filePath("proj.nxe");
    Document d(12, 7, Qt::white, "Proj");
    d.addLayer("Top", newImage(12, 7, QColor(255, 0, 0, 255)), 1, 2);
    d.layers().back().opacity = 0.5;
    d.layers().back().blend = BlendMode::Multiply;
    QString err;
    CHECK(FileIO::saveProject(d, path, &err));
    Document loaded;
    CHECK(FileIO::loadProject(path, loaded, &err));
    CHECK(loaded.width() == 12 && loaded.height() == 7);
    CHECK(loaded.layers().size() == 2);
    CHECK(loaded.layers()[1].name == "Top");
    CHECK(loaded.layers()[1].x == 1 && loaded.layers()[1].y == 2);
    CHECK(loaded.layers()[1].opacity == 0.5);
    CHECK(loaded.layers()[1].blend == BlendMode::Multiply);
}

static void testExportImport() {
    QTemporaryDir dir;
    const QString png = dir.filePath("out.png");
    Document d(9, 5, QColor(0, 128, 255, 255), "Out");
    QString err;
    CHECK(FileIO::exportImage(d, png, 90, &err));
    Document back;
    CHECK(FileIO::importImage(png, back, &err));
    CHECK(back.width() == 9 && back.height() == 5);
}

int main() {
    testPixelRoundTrip();
    testSelectionAndCombine();
    testFloodFill();
    testAdjustments();
    testFillWithMask();
    testDocumentLayers();
    testCropRotateResize();
    testSnapshotRestore();
    testProjectRoundTrip();
    testExportImport();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
