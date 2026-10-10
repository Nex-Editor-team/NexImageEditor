#pragma once
#include "tools/Tool.h"

#include <QIcon>

namespace Icons {
// Simple vector glyphs drawn at runtime, so the app needs no image resources.
QIcon tool(ToolId id);
QIcon visibility(bool visible);
QIcon lock(bool locked);
}  // namespace Icons
