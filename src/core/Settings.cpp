#include "core/Settings.h"

#include <QSettings>

namespace {
QSettings settings() { return QSettings("Hyggshi", "NexImageEditor"); }
}

int Settings::recentLimit() { return 10; }

QStringList Settings::recentFiles() { return settings().value("recent").toStringList(); }

void Settings::addRecent(const QString& path) {
    QStringList list = recentFiles();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > recentLimit()) list.removeLast();
    settings().setValue("recent", list);
}

bool Settings::showGrid() { return settings().value("showGrid", false).toBool(); }
void Settings::setShowGrid(bool on) { settings().setValue("showGrid", on); }
