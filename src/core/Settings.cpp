#include "core/Settings.h"

#include <glib.h>

#include <algorithm>
#include <filesystem>

std::string Settings::configPath() {
    return std::string(g_get_user_config_dir()) + "/neximageeditor/settings.ini";
}

Settings Settings::load() {
    Settings s;
    GKeyFile* kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, configPath().c_str(), G_KEY_FILE_NONE, nullptr)) {
        gsize n = 0;
        gchar** recent = g_key_file_get_string_list(kf, "general", "recent", &n, nullptr);
        if (recent) {
            for (gsize i = 0; i < n; ++i) s.recentFiles.push_back(recent[i]);
            g_strfreev(recent);
        }
        if (gchar* d = g_key_file_get_string(kf, "general", "last_directory", nullptr)) {
            s.lastDirectory = d;
            g_free(d);
        }
        s.zoomDefaultPercent = g_key_file_get_integer(kf, "view", "default_zoom", nullptr);
        if (s.zoomDefaultPercent < 10 || s.zoomDefaultPercent > 3200) s.zoomDefaultPercent = 100;
    }
    g_key_file_free(kf);
    return s;
}

void Settings::save() const {
    GKeyFile* kf = g_key_file_new();
    std::vector<const char*> recent;
    for (const auto& r : recentFiles) recent.push_back(r.c_str());
    recent.push_back(nullptr);
    g_key_file_set_string_list(kf, "general", "recent", recent.data(), recent.size() - 1);
    g_key_file_set_string(kf, "general", "last_directory", lastDirectory.c_str());
    g_key_file_set_integer(kf, "view", "default_zoom", zoomDefaultPercent);
    std::string path = configPath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    g_key_file_save_to_file(kf, path.c_str(), nullptr);
    g_key_file_free(kf);
}

void Settings::addRecent(const std::string& path) {
    recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), path), recentFiles.end());
    recentFiles.insert(recentFiles.begin(), path);
    if (recentFiles.size() > 10) recentFiles.resize(10);
}
