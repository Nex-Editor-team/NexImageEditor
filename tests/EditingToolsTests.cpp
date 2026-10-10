#include "engine/CropGeometry.h"
#include "engine/Filters.h"
#include "engine/ImageExport.h"
#include "gui/Canvas.h"
#include "gui/ExportDialog.h"
#include "io/ImageIO.h"
#include "project/Commands.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSpinBox>
#include <QTemporaryDir>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

static void mouse(Canvas& canvas, QEvent::Type type, QPointF p, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, p, p, canvas.mapToGlobal(p.toPoint()), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &event);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QSize bounds(320, 240);
    CHECK(CropGeometry::fromDrag({20, 20}, {100, 80}, bounds) == QRect(20, 20, 80, 60));
    CHECK(CropGeometry::fromDrag({100, 80}, {20, 20}, bounds) == QRect(20, 20, 80, 60));
    CHECK(CropGeometry::fromDrag({-10, -20}, {999, 999}, bounds) == QRect(QPoint(), bounds));
    CHECK(CropGeometry::fromDrag({0, 0}, {3, 3}, bounds, {16, 9}).isEmpty());
    for (QSize ratio : {QSize(1, 1), QSize(16, 9), QSize(4, 3), QSize(9, 16)}) {
        const QRect r = CropGeometry::fromDrag({300, 200}, {0, 0}, bounds, ratio);
        CHECK(!r.isEmpty() && QRect(QPoint(), bounds).contains(r));
        CHECK(r.width() * ratio.height() == r.height() * ratio.width());
    }

    Project project(bounds);
    auto raster = std::make_unique<Layer>(LayerType::Raster, "Raster");
    raster->content = QImage(320, 240, QImage::Format_ARGB32);
    raster->content.fill(QColor(220, 40, 20, 128));
    raster->transform.center = QPointF(160, 120);
    auto* layer = project.insertLayer(std::move(raster));
    auto hidden = std::make_unique<Layer>(LayerType::Raster, "Hidden");
    hidden->visible = false;
    hidden->locked = true;
    hidden->transform.center = QPointF(10, 15);
    auto* hiddenLayer = project.insertLayer(std::move(hidden));
    const QImage original = layer->content;
    QUndoStack history;
    history.push(new CropCanvasCommand(&project, QRect(20, 30, 160, 90)));
    CHECK(project.canvasSize() == QSize(160, 90));
    CHECK(layer->transform.center == QPointF(140, 90));
    CHECK(hiddenLayer->transform.center == QPointF(-10, -15));
    CHECK(layer->content == original);
    CHECK(project.flatten().size() == QSize(160, 90));
    history.undo();
    CHECK(project.canvasSize() == bounds && layer->transform.center == QPointF(160, 120));
    CHECK(hiddenLayer->transform.center == QPointF(10, 15));
    CHECK(history.isClean());
    history.redo();
    CHECK(project.canvasSize() == QSize(160, 90));
    history.undo();
    history.clear();

    Canvas canvas;
    canvas.resize(640, 480);
    canvas.setProject(&project);
    canvas.setUndoStack(&history);
    canvas.setZoom(1.0); // Canvas origin = (160,120).
    canvas.setTool(ToolMode::Crop);
    canvas.setCropAspect(QSize(4, 3));
    QObject::connect(&canvas, &Canvas::cropRequested, [&](QRect r) { history.push(new CropCanvasCommand(&project, r)); });
    mouse(canvas, QEvent::MouseButtonPress, {180, 140}, Qt::LeftButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseMove, {280, 215}, Qt::NoButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseButtonRelease, {280, 215}, Qt::LeftButton, Qt::NoButton);
    CHECK(canvas.cropRect() == QRect(20, 20, 100, 75));
    // Move the frame without changing size.
    mouse(canvas, QEvent::MouseButtonPress, {220, 175}, Qt::LeftButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseMove, {230, 185}, Qt::NoButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseButtonRelease, {230, 185}, Qt::LeftButton, Qt::NoButton);
    CHECK(canvas.cropRect() == QRect(30, 30, 100, 75));
    // Resize the bottom-right corner while retaining the 4:3 aspect.
    mouse(canvas, QEvent::MouseButtonPress, {290, 225}, Qt::LeftButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseMove, {310, 240}, Qt::NoButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseButtonRelease, {310, 240}, Qt::LeftButton, Qt::NoButton);
    CHECK(canvas.cropRect() == QRect(30, 30, 120, 90));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &enter);
    CHECK(project.canvasSize() == QSize(120, 90) && history.count() == 1 && canvas.cropRect().isEmpty());
    history.undo();
    canvas.setZoom(2.0);
    mouse(canvas, QEvent::MouseButtonPress, {40, 40}, Qt::LeftButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseMove, {240, 190}, Qt::NoButton, Qt::LeftButton);
    mouse(canvas, QEvent::MouseButtonRelease, {240, 190}, Qt::LeftButton, Qt::NoButton);
    CHECK(canvas.cropRect() == QRect(20, 20, 100, 75));
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &escape);
    CHECK(canvas.cropRect().isEmpty() && project.canvasSize() == bounds && history.isClean());

    QImage transparent(9, 9, QImage::Format_ARGB32);
    transparent.fill(Qt::transparent);
    transparent.setPixelColor(4, 4, QColor(255, 0, 0, 255));
    ImageBuffer source(transparent);
    const auto blur = Filters::gaussianBlur(source, 1.0);
    CHECK(!blur.isNull() && blur.size() == source.size());
    CHECK(blur.image().pixelColor(4, 4).alpha() < 255);
    CHECK(blur.image().pixelColor(4, 3).alpha() > 0);
    CHECK(blur.image().pixelColor(4, 3).red() >= 250); // No black halo.
    CHECK(Filters::gaussianBlur(source, 0).image() == source.image());
    CHECK(Filters::gaussianBlur({}, 1).isNull());
    const ImageBuffer color(original);
    CHECK(Filters::grayscale(color).image().pixelColor(0, 0).alpha() == 128);
    CHECK(Filters::sepia(color).size() == color.size());
    CHECK(Filters::invert(color).image().pixelColor(0, 0).alpha() == 128);
    CHECK(original == layer->content);

    CHECK(!ImageExport::validSize({0, 2}));
    CHECK(!ImageExport::validSize({20001, 2}));
    CHECK(!ImageExport::validSize({20000, 20000}));
    for (auto mode : {ImageExport::Interpolation::Nearest, ImageExport::Interpolation::Bilinear,
                     ImageExport::Interpolation::Bicubic, ImageExport::Interpolation::Lanczos, ImageExport::Interpolation::Area}) {
        const auto resized = ImageExport::prepare(source, {18, 18}, mode);
        CHECK(resized.size() == QSize(18, 18));
        for (int y = 0; y < 18; ++y) for (int x = 0; x < 18; ++x) {
            const QRgb p = resized.image().pixel(x, y);
            CHECK(qRed(p) <= qAlpha(p) && qGreen(p) <= qAlpha(p) && qBlue(p) <= qAlpha(p));
        }
        const auto opaque = ImageExport::prepare(source, {18, 18}, mode, false, Qt::blue);
        CHECK(opaque.image().pixelColor(0, 0) == QColor(Qt::blue));
        CHECK(opaque.image().pixelColor(9, 9).alpha() == 255);
    }
    CHECK(source.image().pixelColor(4, 4) == QColor(Qt::red));
    CHECK(ImageExport::prepare(source, {0, 1}, ImageExport::Interpolation::Nearest).isNull());
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const auto exported = ImageExport::prepare(source, {36, 36}, ImageExport::Interpolation::Nearest);
    const QString path = dir.filePath("explicit-format.dat");
    CHECK(ImageIO::saveImage(path, exported, -1, "png"));
    CHECK(QImageReader::imageFormat(path) == "png");
    CHECK(ImageIO::loadImage(path).size() == QSize(36, 36));
    const QImage saved = ImageIO::loadImage(path).image();
    CHECK(!ImageIO::saveImage(path, exported, -1, "invalid-format"));
    CHECK(ImageIO::loadImage(path).image() == saved); // Failed write preserves existing export.

    ExportDialog dialog({1920, 1080});
    dialog.findChild<QDoubleSpinBox*>("exportPercent")->setValue(50);
    CHECK(dialog.outputSize() == QSize(960, 540));
    dialog.findChild<QSpinBox*>("exportWidth")->setValue(1280);
    CHECK(dialog.outputSize() == QSize(1280, 720));
    dialog.findChild<QCheckBox*>("exportKeepRatio")->setChecked(false);
    dialog.findChild<QSpinBox*>("exportHeight")->setValue(400);
    CHECK(dialog.outputSize() == QSize(1280, 400));
    dialog.findChild<QComboBox*>("exportFormat")->setCurrentIndex(1);
    CHECK(dialog.format() == "jpg" && !dialog.transparent() && dialog.quality() == 90);
    dialog.findChild<QComboBox*>("exportFormat")->setCurrentIndex(0);
    CHECK(dialog.transparent() && dialog.quality() == -1);
    CHECK(project.canvasSize() == bounds && layer->content == original && history.isClean());
    return 0;
}
