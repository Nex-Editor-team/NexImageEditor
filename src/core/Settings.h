#pragma once
#include <QString>
#include <QStringList>

// Persistent user preferences (QSettings-backed).
struct Settings {
    static int recentLimit();
    static QStringList recentFiles();
    static void addRecent(const QString& path);
    static bool showGrid();
    static void setShowGrid(bool on);
};
