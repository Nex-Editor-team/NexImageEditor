#include "ProjectIO.h"
#include "../project/TextLayer.h"
#include "../project/ShapeLayer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QBuffer>
#include <QFont>
#include <utility>

namespace {

QString layerTypeToString(LayerType t) {
    switch (t) {
        case LayerType::Raster: return "raster";
        case LayerType::Text:   return "text";
        case LayerType::Shape:  return "shape";
    }
    return "raster";
}

QJsonObject transformToJson(const Transform::FreeTransformState& t) {
    QJsonObject o;
    o["cx"] = t.center.x();
    o["cy"] = t.center.y();
    o["scaleX"] = t.scaleX;
    o["scaleY"] = t.scaleY;
    o["rotation"] = t.rotationDeg;
    o["shearX"] = t.shearXDeg;
    return o;
}

Transform::FreeTransformState transformFromJson(const QJsonObject& o) {
    Transform::FreeTransformState t;
    t.center = {o["cx"].toDouble(), o["cy"].toDouble()};
    t.scaleX = o["scaleX"].toDouble(1.0);
    t.scaleY = o["scaleY"].toDouble(1.0);
    t.rotationDeg = o["rotation"].toDouble(0.0);
    t.shearXDeg = o["shearX"].toDouble(0.0);
    return t;
}

QString imageToBase64Png(const QImage& img) {
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return QString::fromLatin1(bytes.toBase64());
}

QImage imageFromBase64Png(const QString& b64) {
    QByteArray bytes = QByteArray::fromBase64(b64.toLatin1());
    QImage img;
    img.loadFromData(bytes, "PNG");
    return img;
}

QJsonArray pointsToJson(const QVector<QPointF>& pts) {
    QJsonArray arr;
    for (const auto& pt : pts) {
        QJsonObject o;
        o["x"] = pt.x();
        o["y"] = pt.y();
        arr.append(o);
    }
    return arr;
}

QVector<QPointF> pointsFromJson(const QJsonArray& arr) {
    QVector<QPointF> pts;
    for (const auto& v : arr) {
        QJsonObject o = v.toObject();
        pts.push_back(QPointF(o["x"].toDouble(), o["y"].toDouble()));
    }
    if (pts.isEmpty()) {
        pts = {{0.0, 0.0}, {255.0, 255.0}};
    }
    return pts;
}

QJsonObject toneParametersToJson(const ToneParameters& p) {
    QJsonObject o;
    o["exposure"] = p.exposure;
    o["contrast"] = p.contrast;
    o["highlights"] = p.highlights;
    o["shadows"] = p.shadows;
    o["whites"] = p.whites;
    o["blacks"] = p.blacks;
    o["temp"] = p.temp;
    o["tint"] = p.tint;
    o["vibrance"] = p.vibrance;
    o["saturation"] = p.saturation;
    o["clarity"] = p.clarity;
    o["dehaze"] = p.dehaze;
    o["vignette"] = p.vignette;
    o["bwMode"] = p.bwMode;
    o["hdrMode"] = p.hdrMode;
    o["activePreset"] = p.activePreset;
    o["presetAmount"] = p.presetAmount;
    o["curveRgb"] = pointsToJson(p.curveRgb);
    o["curveRed"] = pointsToJson(p.curveRed);
    o["curveGreen"] = pointsToJson(p.curveGreen);
    o["curveBlue"] = pointsToJson(p.curveBlue);
    return o;
}

ToneParameters toneParametersFromJson(const QJsonObject& o) {
    ToneParameters p;
    if (o.isEmpty()) return p;
    p.exposure = o["exposure"].toDouble(0.0);
    p.contrast = o["contrast"].toDouble(0.0);
    p.highlights = o["highlights"].toDouble(0.0);
    p.shadows = o["shadows"].toDouble(0.0);
    p.whites = o["whites"].toDouble(0.0);
    p.blacks = o["blacks"].toDouble(0.0);
    p.temp = o["temp"].toDouble(0.0);
    p.tint = o["tint"].toDouble(0.0);
    p.vibrance = o["vibrance"].toDouble(0.0);
    p.saturation = o["saturation"].toDouble(0.0);
    p.clarity = o["clarity"].toDouble(0.0);
    p.dehaze = o["dehaze"].toDouble(0.0);
    p.vignette = o["vignette"].toDouble(0.0);
    p.bwMode = o["bwMode"].toBool(false);
    p.hdrMode = o["hdrMode"].toBool(false);
    p.activePreset = o["activePreset"].toString("Mặc định");
    p.presetAmount = o["presetAmount"].toDouble(100.0);
    if (o.contains("curveRgb")) p.curveRgb = pointsFromJson(o["curveRgb"].toArray());
    if (o.contains("curveRed")) p.curveRed = pointsFromJson(o["curveRed"].toArray());
    if (o.contains("curveGreen")) p.curveGreen = pointsFromJson(o["curveGreen"].toArray());
    if (o.contains("curveBlue")) p.curveBlue = pointsFromJson(o["curveBlue"].toArray());
    return p;
}

QJsonObject commonLayerFields(const Layer* l) {
    QJsonObject o;
    o["id"] = l->id().toString();
    o["name"] = l->name;
    o["type"] = layerTypeToString(l->type());
    o["visible"] = l->visible;
    o["locked"] = l->locked;
    o["opacity"] = l->opacity;
    o["blendMode"] = Blend::toString(l->blendMode);
    o["tintColor"] = l->tintColor.name(QColor::HexArgb);
    o["tintAmount"] = l->tintAmount;
    o["transform"] = transformToJson(l->transform);
    o["adjustments"] = toneParametersToJson(l->adjustments);
    return o;
}

} // namespace

namespace ProjectIO {

bool saveProject(const QString& path, const Project& project) {
    QJsonObject root;
    root["format"] = "NexImageEditor";
    root["version"] = 1;
    root["canvasWidth"] = project.canvasSize().width();
    root["canvasHeight"] = project.canvasSize().height();

    QJsonArray layersArr;
    for (const auto& layerPtr : project.layers()) {
        Layer* l = layerPtr.get();
        QJsonObject o = commonLayerFields(l);

        if (l->type() == LayerType::Text) {
            auto* tl = static_cast<TextLayer*>(l);
            o["text"] = tl->text;
            o["fontFamily"] = tl->font.family();
            o["fontSize"] = tl->font.pointSize();
            o["fontBold"] = tl->font.bold();
            o["fontItalic"] = tl->font.italic();
            o["color"] = tl->color.name(QColor::HexArgb);
        } else if (l->type() == LayerType::Shape) {
            auto* sl = static_cast<ShapeLayer*>(l);
            o["shapeKind"] = int(sl->kind);
            o["shapeW"] = sl->localSize.width();
            o["shapeH"] = sl->localSize.height();
            o["cornerRadius"] = sl->cornerRadius;
            o["fillMode"] = int(sl->fillMode);
            o["fillColor"] = sl->fillColor.name(QColor::HexArgb);
            o["fillColor2"] = sl->fillColor2.name(QColor::HexArgb);
            o["gradientAngle"] = sl->gradientAngleDeg;
            o["strokeColor"] = sl->strokeColor.name(QColor::HexArgb);
            o["strokeWidth"] = sl->strokeWidth;
        } else {
            // Raster (and, redundantly but harmlessly, text/shape too):
            // always embed the rasterized pixels so the file is
            // self-contained even if type-specific fields above are
            // ignored by a future reader.
            o["pixels"] = imageToBase64Png(l->content);
        }
        if (l->type() != LayerType::Raster) {
            o["pixels"] = imageToBase64Png(l->content); // keep a raster fallback for all types
        }

        layersArr.append(o);
    }
    root["layers"] = layersArr;

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (f.write(data) != data.size()) return false;
    return f.commit();
}

bool loadProject(const QString& path, Project& outProject) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    if (root["format"].toString() != "NexImageEditor" || root["version"].toInt(-1) != 1
        || !root["layers"].isArray()) return false;
    const int width = root["canvasWidth"].toInt(-1);
    const int height = root["canvasHeight"].toInt(-1);
    if (width < 1 || width > 20000 || height < 1 || height > 20000) return false;

    // Build separately: neither a failed read nor a second successful load may
    // partially overwrite or append to the caller's existing document.
    Project loaded(QSize(width, height));
    loaded.filePath = path;

    for (const QJsonValue& v : root["layers"].toArray()) {
        if (!v.isObject()) return false;
        QJsonObject o = v.toObject();
        QString typeStr = o["type"].toString();
        if (typeStr != "raster" && typeStr != "text" && typeStr != "shape") return false;

        std::unique_ptr<Layer> layer;
        if (typeStr == "text") {
            auto tl = std::make_unique<TextLayer>(o["text"].toString("Text"), o["name"].toString("Text Layer"));
            QFont f(o["fontFamily"].toString("Sans Serif"), o["fontSize"].toInt(32));
            f.setBold(o["fontBold"].toBool(false));
            f.setItalic(o["fontItalic"].toBool(false));
            tl->font = f;
            tl->color = QColor(o["color"].toString("#ffffffff"));
            tl->regenerate();
            layer = std::move(tl);
        } else if (typeStr == "shape") {
            const int kind = o["shapeKind"].toInt(-1);
            const int fill = o["fillMode"].toInt(0);
            const int w = o["shapeW"].toInt(-1), h = o["shapeH"].toInt(-1);
            if (kind < 0 || kind > int(ShapeKind::Line)
                || fill < 0 || fill > int(ShapeFillMode::RadialGradient)
                || w < 1 || w > 20000 || h < 1 || h > 20000) return false;
            auto sl = std::make_unique<ShapeLayer>(ShapeKind(kind), o["name"].toString("Shape"));
            sl->localSize = QSize(w, h);
            sl->cornerRadius = o["cornerRadius"].toInt(20);
            sl->fillMode = ShapeFillMode(o["fillMode"].toInt(0));
            sl->fillColor = QColor(o["fillColor"].toString("#ffff0000"));
            sl->fillColor2 = QColor(o["fillColor2"].toString("#ffffff00"));
            sl->gradientAngleDeg = o["gradientAngle"].toDouble(0.0);
            sl->strokeColor = QColor(o["strokeColor"].toString("#ff000000"));
            sl->strokeWidth = o["strokeWidth"].toInt(0);
            sl->regenerate();
            layer = std::move(sl);
        } else {
            auto rl = std::make_unique<Layer>(LayerType::Raster, o["name"].toString("Layer"));
            if (!o["pixels"].isString()) return false;
            const QString pixels = o["pixels"].toString();
            rl->content = imageFromBase64Png(pixels);
            // Empty raster layers are valid; nonempty, undecodable data is not.
            if (!pixels.isEmpty() && rl->content.isNull()) return false;
            layer = std::move(rl);
        }

        layer->visible = o["visible"].toBool(true);
        layer->locked = o["locked"].toBool(false);
        layer->opacity = o["opacity"].toDouble(1.0);
        layer->blendMode = Blend::fromString(o["blendMode"].toString("Normal"));
        layer->tintColor = QColor(o["tintColor"].toString("#ffff0000"));
        layer->tintAmount = o["tintAmount"].toDouble(0.0);
        if (o.contains("adjustments")) {
            layer->adjustments = toneParametersFromJson(o["adjustments"].toObject());
        }
        layer->applyAdjustments();
        layer->transform = transformFromJson(o["transform"].toObject());

        loaded.insertLayer(std::move(layer));
    }
    outProject = std::move(loaded);
    return true;
}

} // namespace ProjectIO

