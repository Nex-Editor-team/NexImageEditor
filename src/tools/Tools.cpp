#include "tools/Tool.h"

namespace {
const ToolInfo kTools[] = {
    {ToolId::RectMarquee, "Rectangular Marquee", "m", "\u25AD"},
    {ToolId::EllipseMarquee, "Elliptical Marquee", "", "\u25CC"},
    {ToolId::Lasso, "Lasso", "l", "\u2AF7"},
    {ToolId::MagicWand, "Magic Wand", "w", "\u2729"},
    {ToolId::Move, "Move", "v", "\u2725"},
    {ToolId::Crop, "Crop", "c", "\u2702"},
    {ToolId::Transform, "Free Transform", "", "\u2922"},
    {ToolId::Brush, "Paintbrush", "b", "\u270E"},
    {ToolId::Eraser, "Eraser", "e", "\u232B"},
    {ToolId::Bucket, "Bucket Fill", "g", "\u25CD"},
    {ToolId::Gradient, "Gradient", "", "\u25E7"},
    {ToolId::Eyedropper, "Color Picker", "i", "\u2627"},
    {ToolId::Text, "Text", "t", "T"},
    {ToolId::Rectangle, "Rectangle", "u", "\u25A1"},
    {ToolId::Ellipse, "Ellipse", "", "\u25CB"},
    {ToolId::Hand, "Hand", "h", "\u270B"},
    {ToolId::Zoom, "Zoom", "z", "\u2315"},
};
static_assert(sizeof(kTools) / sizeof(kTools[0]) == size_t(ToolId::Count), "tool table out of sync");
}  // namespace

const ToolInfo& toolInfo(ToolId id) { return kTools[size_t(id)]; }

ToolId toolFromShortcut(const std::string& key, bool& found) {
    // Each key maps to one tool; tools reachable only from the toolbox have an empty shortcut.
    for (const ToolInfo& t : kTools)
        if (key == t.shortcut) {
            found = true;
            return t.id;
        }
    found = false;
    return ToolId::Move;
}

bool isSelectionTool(ToolId id) {
    return id == ToolId::RectMarquee || id == ToolId::EllipseMarquee || id == ToolId::Lasso ||
           id == ToolId::MagicWand;
}
