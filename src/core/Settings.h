#pragma once
#include <string>
#include <vector>

// Persistent preferences in $XDG_CONFIG_HOME/neximageeditor/settings.ini (GKeyFile format).
struct Settings {
    std::vector<std::string> recentFiles;  // most recent first, max 10
    std::string lastDirectory;
    int zoomDefaultPercent = 100;

    static Settings load();
    void save() const;
    void addRecent(const std::string& path);
    static std::string configPath();
};
