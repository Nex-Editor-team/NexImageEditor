// Core tests. No GTK needed: they exercise the document model, pixel operations and file formats.
#include "core/Adjustments.h"
#include "core/FileIO.h"
#include "core/Filters.h"
#include "core/History.h"
#include "core/Ops.h"
#include "core/Pixels.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int g_failed = 0;
int g_passed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (cond) {                                                              \
            ++g_passed;                                                          \
        } else {                                                                 \
            ++g_failed;                                                          \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

uint32_t pixelAt(const Surface& s, int x, int y) { return surfRow(s, y)[x]; }

Color colorAt(const Surface& s, int x, int y) {
    Pixels p = toPixels(s);
    const uint8_t* q = p.px(x, y);
    return Color{q[0], q[1], q[2], q[3]};
}

void testSelectionAndWand() {
    Document d(20, 10, rgba(255, 255, 255));
    auto sel = ops::selRect(IRect{2, 3, 5, 4}, 20, 10);
    CHECK(sel != nullptr);
    CHECK(sel->bounds().x == 2 && sel->bounds().y == 3);
    CHECK(sel->bounds().w == 5 && sel->bounds().h == 4);
    CHECK(sel->at(2, 3) == 255 && sel->at(1, 3) == 0);

    // Magic wand on a uniform canvas selects everything.
    auto all = ops::selMagicWand(d.flattened(), IPoint{0, 0}, 0, true);
    CHECK(all && all->bounds().w == 20 && all->bounds().h == 10);

    // Subtract a rectangle from the full selection.
    auto both = combineSelections(all, sel, SelOp::Subtract, 20, 10);
    CHECK(both && both->at(3, 4) == 0 && both->at(0, 0) == 255);
}

void testHistory() {
    Document d(8, 8, rgba(255, 255, 255));
    History h;
    DocState before = d.snapshot();
    ops::fillSelection(*d.active(), nullptr, rgba(255, 0, 0));
    h.push("fill", before, d.snapshot());
    CHECK(colorAt(d.flattened(), 1, 1).r == 255);

    CHECK(h.canUndo());
    d.restore(*h.undo());
    CHECK(colorAt(d.flattened(), 1, 1).g == 255);

    CHECK(h.canRedo());
    d.restore(*h.redo());
    CHECK(colorAt(d.flattened(), 1, 1).g == 0);

    h.jumpTo(0, d);
    CHECK(colorAt(d.flattened(), 1, 1).g == 255);
}

void testCopyOnWrite() {
    Document d(4, 4, rgba(0, 0, 0));
    DocState snap = d.snapshot();  // shares the layer image
    ops::fillSelection(*d.active(), nullptr, rgba(0, 255, 0));
    // The snapshot must keep the old pixels.
    Pixels oldPixels = toPixels(snap.layers[0].img);
    CHECK(oldPixels.px(0, 0)[1] == 0);
    CHECK(toPixels(d.active()->img).px(0, 0)[1] == 255);
}

void testCanvasOps() {
    Document d(10, 6, rgba(255, 255, 255));
    ops::fillSelection(*d.active(), nullptr, rgba(255, 0, 0));
    ops::rotateCanvas90(d, true);
    CHECK(d.width() == 6 && d.height() == 10);
    CHECK(colorAt(d.flattened(), 0, 0).r == 255);

    ops::flipCanvas(d, true);
    CHECK(d.width() == 6);

    ops::cropDocument(d, IRect{1, 1, 3, 4});
    CHECK(d.width() == 3 && d.height() == 4);

    // Canvas 3x4 -> 6x8 scales the layer content by the same factor (2x): 6x10 -> 12x20.
    ops::resizeImage(d, 6, 8);
    CHECK(d.width() == 6 && d.height() == 8);
    CHECK(d.layers()[0].w() == 12 && d.layers()[0].h() == 20);
}

void testTransformLayer() {
    Document d(10, 10, rgba(0, 0, 0, 0));
    Layer& L = *d.active();
    L.img = newSurface(4, 4);
    fillSurface(L.img, rgba(0, 0, 255));
    L.x = 0;
    L.y = 0;
    cairo_matrix_t M;
    cairo_matrix_init_scale(&M, 2, 2);
    ops::transformLayer(L, M);
    CHECK(L.w() == 8 && L.h() == 8);
}

void testMoveSelectedPixels() {
    Document d(10, 10, rgba(0, 0, 0, 0));
    ops::fillSelection(*d.active(), nullptr, rgba(255, 255, 255));
    auto sel = ops::selRect(IRect{0, 0, 2, 2}, 10, 10);
    ops::moveSelectedPixels(*d.active(), *sel, 3, 0);
    CHECK(colorAt(d.flattened(), 0, 0).a == 0);
    CHECK(colorAt(d.flattened(), 3, 0).a == 255);
}

void testFilters() {
    Surface s = newSurface(16, 16);
    fillSurface(s, rgba(100, 100, 100));
    Surface b = filters::gaussianBlur(s, 2);
    CHECK(colorAt(b, 8, 8).r == 100);  // flat images stay flat
    Surface m = filters::median(s, 2);
    CHECK(colorAt(m, 4, 4).g == 100);
    Surface inv = adjust::invert(s);
    CHECK(colorAt(inv, 0, 0).r == 155);
    Surface gray = adjust::grayscale(s);
    CHECK(colorAt(gray, 0, 0).b == 100);
}

void testProjectRoundTrip() {
    Document d(12, 9, rgba(10, 20, 30));
    d.addLayer("Top", newSurface(12, 9), 0, 0);
    ops::fillSelection(*d.active(), nullptr, rgba(200, 0, 0, 128));
    d.active()->opacity = 0.5;
    d.active()->blend = BlendMode::Multiply;
    const std::string path = "/tmp/nex_test_roundtrip.nxe";
    std::string err;
    CHECK(fileio::saveProject(path, d, err));

    Document r;
    CHECK(fileio::loadProject(path, r, err));
    CHECK(r.width() == 12 && r.height() == 9);
    CHECK(r.layers().size() == 2);
    CHECK(r.layers()[1].name == "Top");
    CHECK(r.layers()[1].blend == BlendMode::Multiply);
    CHECK(r.layers()[1].opacity > 0.49 && r.layers()[1].opacity < 0.51);
    std::remove(path.c_str());
}

void testExportImport() {
    Surface s = newSurface(5, 5);
    fillSurface(s, rgba(12, 200, 90, 255));
    const std::string path = "/tmp/nex_test_export.png";
    std::string err;
    CHECK(fileio::exportImage(path, s, err));
    Surface back = fileio::importImage(path, err);
    CHECK(back != nullptr);
    if (back) CHECK(colorAt(back, 2, 2).g == 200);
    std::remove(path.c_str());
}

}  // namespace

int main() {
    testSelectionAndWand();
    testHistory();
    testCopyOnWrite();
    testCanvasOps();
    testTransformLayer();
    testMoveSelectedPixels();
    testFilters();
    testProjectRoundTrip();
    testExportImport();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
