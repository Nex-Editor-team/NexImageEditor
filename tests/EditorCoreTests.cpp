#include "project/Commands.h"
#include "io/ProjectIO.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "project/TextLayer.h"
#include "project/ShapeLayer.h"
#include <QUndoStack>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Project project(QSize(100, 80));
    QUndoStack history;
    auto* first = project.insertLayer(std::make_unique<Layer>(LayerType::Raster, "First"));
    auto* second = project.insertLayer(std::make_unique<Layer>(LayerType::Raster, "Second"));
    first->content = QImage(8, 8, QImage::Format_ARGB32);
    first->content.fill(Qt::red);
    first->transform.center = QPointF(10, 20);
    second->transform.center = QPointF(30, 40);
    history.push(new MoveLayerCommand(&project, 0, 1));
    CHECK(project.layerAt(1) == first);
    CHECK(!history.isClean());
    history.undo();
    CHECK(project.layerAt(0) == first && history.isClean());
    history.redo();
    CHECK(project.layerAt(0) == second);
    history.push(new SetLayerPropertyCommand<bool>(&project, first->id(), true, false,
        [](Layer* l) { return l->visible; }, [](Layer* l, bool v) { l->visible = v; }, "Visibility"));
    CHECK(!first->visible);
    history.undo();
    CHECK(first->visible);
    history.redo();
    CHECK(!first->visible);
    history.push(new ResizeCanvasCommand(&project, QSize(201, 41), true));
    CHECK(project.canvasSize() == QSize(201, 41));
    CHECK(first->transform.center == QPointF(60.5, 0.5));
    CHECK(second->transform.center == QPointF(80.5, 20.5));
    history.undo();
    CHECK(project.canvasSize() == QSize(100, 80));
    CHECK(first->transform.center == QPointF(10, 20));
    history.redo();
    history.setClean();
    CHECK(history.isClean());
    history.push(new ResizeCanvasCommand(&project, QSize(50, 50), false));
    CHECK(first->transform.center == QPointF(60.5, 0.5));
    history.undo();
    CHECK(history.isClean());

    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.filePath("test.nxproj");
    CHECK(ProjectIO::saveProject(path, project));
    Project loaded;
    CHECK(ProjectIO::loadProject(path, loaded));
    CHECK(loaded.canvasSize() == project.canvasSize());
    CHECK(loaded.layerCount() == 2);
    CHECK(loaded.layerAt(1)->name == "First");
    CHECK(!loaded.layerAt(1)->visible);
    CHECK(loaded.layerAt(1)->content.pixelColor(0, 0) == QColor(Qt::red));
    CHECK(loaded.layerAt(1)->transform.center == first->transform.center);
    CHECK(!ProjectIO::saveProject(dir.filePath("missing/test.nxproj"), project));
    CHECK(ProjectIO::loadProject(path, loaded));
    CHECK(loaded.layerCount() == 2); // Reload must replace, never append.

    QFile saved(path);
    CHECK(saved.open(QIODevice::ReadOnly));
    const QJsonObject validRoot = QJsonDocument::fromJson(saved.readAll()).object();
    const QString badPath = dir.filePath("bad.nxproj");
    Layer* existing = loaded.layerAt(0);
    auto rejectsWithoutMutation = [&](const QByteArray& bytes) {
        QFile bad(badPath);
        if (!bad.open(QIODevice::WriteOnly) || bad.write(bytes) != bytes.size()) return false;
        bad.close();
        return !ProjectIO::loadProject(badPath, loaded)
            && loaded.layerCount() == 2 && loaded.layerAt(0) == existing
            && loaded.canvasSize() == project.canvasSize() && loaded.filePath == path;
    };
    CHECK(rejectsWithoutMutation("{"));
    CHECK(rejectsWithoutMutation("{}"));
    for (const QString field : {QString("format"), QString("version"), QString("canvasWidth"),
                                QString("canvasHeight"), QString("layers")}) {
        QJsonObject bad = validRoot;
        bad.remove(field);
        CHECK(rejectsWithoutMutation(QJsonDocument(bad).toJson()));
    }
    for (const int dimension : {-1, 0, 20001}) {
        QJsonObject bad = validRoot;
        bad["canvasWidth"] = dimension;
        CHECK(rejectsWithoutMutation(QJsonDocument(bad).toJson()));
    }
    QJsonObject bad = validRoot;
    bad["version"] = 2;
    CHECK(rejectsWithoutMutation(QJsonDocument(bad).toJson()));
    QJsonArray layers = validRoot["layers"].toArray();
    QJsonObject corruptLayer = layers[1].toObject();
    corruptLayer["pixels"] = "not a PNG";
    layers[1] = corruptLayer;
    bad = validRoot;
    bad["layers"] = layers; // Failure after an earlier layer was already parsed.
    CHECK(rejectsWithoutMutation(QJsonDocument(bad).toJson()));
    corruptLayer["type"] = "unknown-layer-type";
    layers[1] = corruptLayer;
    bad["layers"] = layers;
    CHECK(rejectsWithoutMutation(QJsonDocument(bad).toJson()));

    Project typed(QSize(64, 64));
    typed.insertLayer(std::make_unique<TextLayer>("Hello"));
    typed.insertLayer(std::make_unique<ShapeLayer>(ShapeKind::Ellipse));
    CHECK(ProjectIO::saveProject(path, typed));
    CHECK(ProjectIO::loadProject(path, loaded));
    CHECK(loaded.layerCount() == 2);
    CHECK(loaded.layerAt(0)->type() == LayerType::Text);
    CHECK(loaded.layerAt(1)->type() == LayerType::Shape);
    CHECK(loaded.flatten().image().size() == QSize(64, 64));

    // Independent transform gestures must not merge into one undo step.
    const auto original = first->transform;
    auto shifted = original;
    shifted.center += QPointF(5, 0);
    auto rotated = shifted;
    rotated.rotationDeg += 90;
    history.push(new TransformLayerCommand(&project, first->id(), original, shifted));
    history.push(new TransformLayerCommand(&project, first->id(), shifted, rotated));
    history.undo();
    CHECK(first->transform.center == shifted.center);
    CHECK(first->transform.rotationDeg == original.rotationDeg);
    history.undo();
    CHECK(first->transform.center == original.center);
    history.push(new RemoveLayerCommand(&project, 1));
    CHECK(project.indexOf(first) == -1);
    history.undo();
    CHECK(project.layerAt(1) == first);
    return 0;
}
