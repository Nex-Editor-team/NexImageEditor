#pragma once
#include "core/Document.h"

#include <QString>
#include <QStringList>

namespace FileIO {
// Filter string for QFileDialog open dialogs.
QString imageOpenFilter();
QString exportFilter();

// Opens an image file as a new document (single background layer).
bool importImage(const QString& path, Document& out, QString* error = nullptr);

// Saves the flattened visible canvas in the format given by the file suffix.
bool exportImage(const Document& doc, const QString& path, int quality, QString* error = nullptr);

// Native project format (.nxe): QDataStream with magic "NXE1", layers stored as PNG.
bool saveProject(const Document& doc, const QString& path, QString* error = nullptr);
bool loadProject(const QString& path, Document& out, QString* error = nullptr);
}  // namespace FileIO
