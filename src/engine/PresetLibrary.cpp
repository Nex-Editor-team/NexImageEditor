#include "PresetLibrary.h"
#include <algorithm>

namespace {

QVector<PresetItem> buildPresetsList() {
    QVector<PresetItem> list;

    // 1. Hiện đại 10 (Modern 10 - as in user's screenshot)
    {
        PresetItem p;
        p.name = "Hiện đại 10";
        p.category = "Hiện đại";
        p.baseParams.exposure = 0.29;
        p.baseParams.contrast = 25.0;
        p.baseParams.highlights = -45.0;
        p.baseParams.shadows = 35.0;
        p.baseParams.whites = -25.0;
        p.baseParams.blacks = -20.0;
        p.baseParams.temp = 5.0;
        p.baseParams.vibrance = 20.0;
        p.baseParams.saturation = -5.0;
        p.baseParams.vignette = -15.0;
        p.baseParams.curveRgb = {{0.0, 10.0}, {60.0, 50.0}, {190.0, 205.0}, {255.0, 250.0}};
        list.push_back(p);
    }

    // 2. Hiện đại 01 (Modern Clean)
    {
        PresetItem p;
        p.name = "Hiện đại 01";
        p.category = "Hiện đại";
        p.baseParams.exposure = 0.15;
        p.baseParams.contrast = 18.0;
        p.baseParams.highlights = -30.0;
        p.baseParams.shadows = 25.0;
        p.baseParams.whites = 10.0;
        p.baseParams.blacks = -15.0;
        p.baseParams.vibrance = 15.0;
        p.baseParams.saturation = 5.0;
        list.push_back(p);
    }

    // 3. Hiện đại 05 (Modern Moody)
    {
        PresetItem p;
        p.name = "Hiện đại 05";
        p.category = "Hiện đại";
        p.baseParams.exposure = -0.10;
        p.baseParams.contrast = 30.0;
        p.baseParams.highlights = -50.0;
        p.baseParams.shadows = -10.0;
        p.baseParams.whites = 5.0;
        p.baseParams.blacks = -30.0;
        p.baseParams.temp = -8.0;
        p.baseParams.vibrance = 10.0;
        p.baseParams.saturation = -15.0;
        list.push_back(p);
    }

    // 4. Sống động (Vivid Color)
    {
        PresetItem p;
        p.name = "Sống động";
        p.category = "Màu sắc";
        p.baseParams.exposure = 0.10;
        p.baseParams.contrast = 20.0;
        p.baseParams.highlights = -20.0;
        p.baseParams.shadows = 15.0;
        p.baseParams.whites = 15.0;
        p.baseParams.blacks = -10.0;
        p.baseParams.vibrance = 45.0;
        p.baseParams.saturation = 25.0;
        list.push_back(p);
    }

    // 5. Phim cổ điển (Vintage Film)
    {
        PresetItem p;
        p.name = "Phim cổ điển";
        p.category = "Cổ điển";
        p.baseParams.exposure = 0.05;
        p.baseParams.contrast = -15.0;
        p.baseParams.highlights = -35.0;
        p.baseParams.shadows = 40.0;
        p.baseParams.whites = -20.0;
        p.baseParams.blacks = 25.0; // lifted faded blacks
        p.baseParams.temp = 18.0;
        p.baseParams.tint = 8.0;
        p.baseParams.vibrance = -10.0;
        p.baseParams.saturation = -15.0;
        p.baseParams.vignette = -25.0;
        p.baseParams.curveRgb = {{0.0, 25.0}, {70.0, 70.0}, {180.0, 180.0}, {255.0, 235.0}};
        list.push_back(p);
    }

    // 6. Điện ảnh ấm áp (Cinematic Warm)
    {
        PresetItem p;
        p.name = "Điện ảnh ấm áp";
        p.category = "Điện ảnh";
        p.baseParams.exposure = 0.0;
        p.baseParams.contrast = 35.0;
        p.baseParams.highlights = -40.0;
        p.baseParams.shadows = 15.0;
        p.baseParams.whites = 10.0;
        p.baseParams.blacks = -35.0;
        p.baseParams.temp = 20.0;
        p.baseParams.tint = -5.0;
        p.baseParams.vibrance = 20.0;
        p.baseParams.saturation = 5.0;
        p.baseParams.vignette = -30.0;
        list.push_back(p);
    }

    // 7. Màu lạnh Bắc Âu (Nordic Cool)
    {
        PresetItem p;
        p.name = "Màu lạnh Bắc Âu";
        p.category = "Điện ảnh";
        p.baseParams.exposure = 0.15;
        p.baseParams.contrast = 15.0;
        p.baseParams.highlights = -30.0;
        p.baseParams.shadows = 20.0;
        p.baseParams.whites = 20.0;
        p.baseParams.blacks = -10.0;
        p.baseParams.temp = -30.0;
        p.baseParams.tint = -10.0;
        p.baseParams.vibrance = 15.0;
        p.baseParams.saturation = -10.0;
        list.push_back(p);
    }

    // 8. Chân dung mềm mại (Portrait Soft)
    {
        PresetItem p;
        p.name = "Chân dung mềm mại";
        p.category = "Chân dung";
        p.baseParams.exposure = 0.20;
        p.baseParams.contrast = -10.0;
        p.baseParams.highlights = -25.0;
        p.baseParams.shadows = 30.0;
        p.baseParams.whites = 5.0;
        p.baseParams.blacks = 5.0;
        p.baseParams.temp = 8.0;
        p.baseParams.tint = 5.0;
        p.baseParams.vibrance = 20.0;
        p.baseParams.saturation = -5.0;
        list.push_back(p);
    }

    // 9. Hoàng hôn rực rỡ (Golden Hour)
    {
        PresetItem p;
        p.name = "Hoàng hôn rực rỡ";
        p.category = "Phong cảnh";
        p.baseParams.exposure = 0.10;
        p.baseParams.contrast = 20.0;
        p.baseParams.highlights = -50.0;
        p.baseParams.shadows = 25.0;
        p.baseParams.whites = 10.0;
        p.baseParams.blacks = -20.0;
        p.baseParams.temp = 45.0;
        p.baseParams.tint = 15.0;
        p.baseParams.vibrance = 35.0;
        p.baseParams.saturation = 15.0;
        list.push_back(p);
    }

    // 10. Trắng đen tương phản (B&W High Contrast)
    {
        PresetItem p;
        p.name = "Trắng đen tương phản";
        p.category = "Đen trắng";
        p.baseParams.bwMode = true;
        p.baseParams.exposure = 0.10;
        p.baseParams.contrast = 45.0;
        p.baseParams.highlights = -30.0;
        p.baseParams.shadows = -15.0;
        p.baseParams.whites = 25.0;
        p.baseParams.blacks = -40.0;
        p.baseParams.vignette = -20.0;
        list.push_back(p);
    }

    // 11. Cyberpunk / Đêm thành phố (Moody Night)
    {
        PresetItem p;
        p.name = "Cyberpunk / Đêm";
        p.category = "Nghệ thuật";
        p.baseParams.exposure = -0.15;
        p.baseParams.contrast = 40.0;
        p.baseParams.highlights = 20.0;
        p.baseParams.shadows = -25.0;
        p.baseParams.whites = 15.0;
        p.baseParams.blacks = -45.0;
        p.baseParams.temp = -25.0;
        p.baseParams.tint = 35.0;
        p.baseParams.vibrance = 50.0;
        p.baseParams.saturation = 20.0;
        list.push_back(p);
    }

    // 12. Phong cảnh HDR (Landscape HDR)
    {
        PresetItem p;
        p.name = "Phong cảnh HDR";
        p.category = "Phong cảnh";
        p.baseParams.hdrMode = true;
        p.baseParams.exposure = 0.10;
        p.baseParams.contrast = 15.0;
        p.baseParams.highlights = -65.0;
        p.baseParams.shadows = 60.0;
        p.baseParams.whites = 15.0;
        p.baseParams.blacks = -20.0;
        p.baseParams.vibrance = 35.0;
        p.baseParams.saturation = 15.0;
        list.push_back(p);
    }

    // 13. Pastel Dream
    {
        PresetItem p;
        p.name = "Pastel Dream";
        p.category = "Nghệ thuật";
        p.baseParams.exposure = 0.30;
        p.baseParams.contrast = -20.0;
        p.baseParams.highlights = -40.0;
        p.baseParams.shadows = 45.0;
        p.baseParams.whites = -10.0;
        p.baseParams.blacks = 30.0;
        p.baseParams.temp = 5.0;
        p.baseParams.tint = 12.0;
        p.baseParams.vibrance = 25.0;
        p.baseParams.saturation = -10.0;
        list.push_back(p);
    }

    return list;
}

} // namespace

namespace PresetLibrary {

const QVector<PresetItem>& allPresets() {
    static const QVector<PresetItem> presets = buildPresetsList();
    return presets;
}

QStringList presetNames() {
    QStringList names;
    names.append("Mặc định (Default)");
    for (const auto& p : allPresets()) {
        names.append(p.name);
    }
    return names;
}

ToneParameters getPreset(const QString& name) {
    if (name == "Mặc định" || name == "Mặc định (Default)") {
        ToneParameters p;
        p.activePreset = name;
        return p;
    }
    for (const auto& item : allPresets()) {
        if (item.name == name) {
            ToneParameters p = item.baseParams;
            p.activePreset = name;
            return p;
        }
    }
    return ToneParameters{};
}

ToneParameters applyPresetWithAmount(const QString& presetName, double amountPercent) {
    ToneParameters base = getPreset(presetName);
    if (base.isIdentity()) return base;

    double factor = std::clamp(amountPercent / 100.0, 0.0, 2.0);
    ToneParameters res = base;
    res.activePreset = presetName;
    res.presetAmount = amountPercent;

    res.exposure = base.exposure * factor;
    res.contrast = base.contrast * factor;
    res.highlights = base.highlights * factor;
    res.shadows = base.shadows * factor;
    res.whites = base.whites * factor;
    res.blacks = base.blacks * factor;

    res.temp = base.temp * factor;
    res.tint = base.tint * factor;
    res.vibrance = base.vibrance * factor;
    res.saturation = base.saturation * factor;

    res.clarity = base.clarity * factor;
    res.dehaze = base.dehaze * factor;
    res.vignette = base.vignette * factor;

    return res;
}

} // namespace PresetLibrary
