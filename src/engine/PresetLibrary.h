#pragma once
#include "ToneParameters.h"
#include <QString>
#include <QVector>
#include <QStringList>

struct PresetItem {
    QString name;
    QString category;
    ToneParameters baseParams;
};

namespace PresetLibrary {
    // Returns the list of all available presets
    const QVector<PresetItem>& allPresets();

    // Returns the list of preset names
    QStringList presetNames();

    // Finds a preset by name (returns default if not found)
    ToneParameters getPreset(const QString& name);

    // Applies preset with amount percentage (0% = identity, 100% = 1.0x, 200% = 2.0x)
    ToneParameters applyPresetWithAmount(const QString& presetName, double amountPercent);
}
