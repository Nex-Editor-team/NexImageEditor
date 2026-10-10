#pragma once
#include "core/Color.h"

#include <cairo.h>

#include <cstdint>
#include <memory>

// Shared, reference-counted cairo ARGB32 surface (premultiplied, native endian).
using Surface = std::shared_ptr<cairo_surface_t>;

Surface wrapSurface(cairo_surface_t* s);     // takes ownership
Surface newSurface(int w, int h);            // transparent
Surface cloneSurface(const Surface& s);
int surfW(const Surface& s);
int surfH(const Surface& s);
void surfFlush(const Surface& s);            // call before reading pixels
void surfMarkDirty(const Surface& s);        // call after writing pixels
uint32_t* surfRow(const Surface& s, int y);  // premultiplied pixels of row y
void fillSurface(const Surface& s, Color c); // premultiplied solid colour
