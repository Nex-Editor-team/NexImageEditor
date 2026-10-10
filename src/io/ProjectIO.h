#pragma once
#include "../project/Project.h"
#include <QString>

// Saves/loads the native ".nxproj" format: a single JSON file with the
// canvas size and, per layer, its type/transform/opacity/blend plus its
// pixel content embedded as base64 PNG (simplest possible format that
// keeps the whole project in one file - no external asset references
// to go stale).
namespace ProjectIO {
    bool saveProject(const QString& path, const Project& project);
    // Replaces outProject only on success; leaves it unchanged on failure.
    bool loadProject(const QString& path, Project& outProject);
}
