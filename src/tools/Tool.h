#pragma once
#include "core/Color.h"
#include "core/Layer.h"
#include "core/Selection.h"

#include <string>

enum class ToolId {
    RectMarquee, EllipseMarquee, Lasso, MagicWand,
    Move, Crop, Transform,
    Brush, Eraser, Bucket, Gradient, Eyedropper,
    Text, Rectangle, Ellipse,
    Hand, Zoom,
    Count
};

struct ToolInfo {
    ToolId id;
    const char* name;      // user-visible name
    const char* shortcut;  // single key, lower case
    const char* glyph;     // toolbox icon (unicode symbol)
};

const ToolInfo& toolInfo(ToolId id);
ToolId toolFromShortcut(const std::string& key, bool& found);
bool isSelectionTool(ToolId id);

// Shared settings read by the canvas when a tool acts.
struct ToolOptions {
    Color fg{0, 0, 0, 255};
    Color bg{255, 255, 255, 255};
    double brushSize = 12;
    int tolerance = 32;
    bool contiguous = true;
    SelOp selectionMode = SelOp::Replace;
    std::string fontFamily = "Sans";
    int fontSize = 48;
    bool shapeFill = true;
    bool shapeStroke = false;
    double strokeWidth = 2;
    std::string text = "Text";
    double gradientStrength = 1.0;
};
