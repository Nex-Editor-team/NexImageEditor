#include "core/FileIO.h"

#include "core/Pixels.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace fileio {
namespace {

std::string lowerExt(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    std::string e = path.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return e;
}

// Converts any 8-bit RGB(A) pixbuf into a premultiplied surface (no GTK dependency).
Surface fromPixbuf(GdkPixbuf* pb) {
    const int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    const int stride = gdk_pixbuf_get_rowstride(pb);
    const int nch = gdk_pixbuf_get_n_channels(pb);
    const guchar* data = gdk_pixbuf_get_pixels(pb);
    Pixels p;
    p.w = w;
    p.h = h;
    p.d.assign(size_t(w) * size_t(h) * 4, 255);
    for (int y = 0; y < h; ++y) {
        const guchar* row = data + size_t(y) * size_t(stride);
        for (int x = 0; x < w; ++x) {
            uint8_t* q = p.px(x, y);
            for (int c = 0; c < nch && c < 4; ++c) q[c] = row[x * nch + c];
        }
    }
    return fromPixels(p);
}

// Non-premultiplied GdkPixbuf from a surface, with or without an alpha channel.
GdkPixbuf* toPixbuf(const Surface& s, bool keepAlpha = true) {
    Pixels p = toPixels(s);
    GdkPixbuf* pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, keepAlpha, 8, p.w, p.h);
    const int stride = gdk_pixbuf_get_rowstride(pb);
    const int nch = keepAlpha ? 4 : 3;
    guchar* data = gdk_pixbuf_get_pixels(pb);
    for (int y = 0; y < p.h; ++y) {
        guchar* row = data + size_t(y) * size_t(stride);
        for (int x = 0; x < p.w; ++x)
            std::memcpy(row + x * nch, p.px(x, y), size_t(nch));
    }
    return pb;
}

void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
}
void putStr(std::vector<uint8_t>& b, const std::string& s) {
    put32(b, uint32_t(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

struct Reader {
    const std::vector<uint8_t>& b;
    size_t pos = 0;
    bool ok = true;
    uint32_t u32() {
        if (pos + 4 > b.size()) { ok = false; return 0; }
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint32_t(b[pos + i]) << (8 * i);
        pos += 4;
        return v;
    }
    std::vector<uint8_t> bytes(size_t n) {
        if (pos + n > b.size()) { ok = false; return {}; }
        std::vector<uint8_t> r(b.begin() + long(pos), b.begin() + long(pos + n));
        pos += n;
        return r;
    }
    std::string str() {
        uint32_t n = u32();
        auto v = bytes(n);
        return std::string(v.begin(), v.end());
    }
};

}  // namespace

Surface importImage(const std::string& path, std::string& error) {
    GError* err = nullptr;
    GdkPixbuf* pb = gdk_pixbuf_new_from_file(path.c_str(), &err);
    if (!pb) {
        error = err ? err->message : "cannot open image";
        if (err) g_error_free(err);
        return nullptr;
    }
    Surface s = fromPixbuf(pb);
    g_object_unref(pb);
    return s;
}

bool exportImage(const std::string& path, const Surface& flat, std::string& error) {
    std::string ext = lowerExt(path);
    const char* type = nullptr;
    if (ext == "png") type = "png";
    else if (ext == "jpg" || ext == "jpeg") type = "jpeg";
    else if (ext == "webp") type = "webp";
    else if (ext == "bmp") type = "bmp";
    else if (ext == "tif" || ext == "tiff") type = "tiff";
    if (!type) {
        error = "unsupported export format: ." + ext;
        return false;
    }

    Surface src = flat;
    if (std::strcmp(type, "jpeg") == 0) {  // no alpha: composite on white
        src = newSurface(surfW(flat), surfH(flat));
        cairo_t* cr = cairo_create(src.get());
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        cairo_set_source_surface(cr, flat.get(), 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        surfMarkDirty(src);
    }
    const bool isJpeg = std::strcmp(type, "jpeg") == 0;
    GdkPixbuf* pb = toPixbuf(src, !isJpeg);
    GError* err = nullptr;
    gboolean ok = isJpeg ? gdk_pixbuf_save(pb, path.c_str(), type, &err, "quality", "92", nullptr)
                         : gdk_pixbuf_save(pb, path.c_str(), type, &err, nullptr);
    g_object_unref(pb);
    if (!ok) {
        error = err ? err->message : "export failed";
        if (err) g_error_free(err);
    }
    return ok;
}

bool isProjectPath(const std::string& path) { return lowerExt(path) == "nxe"; }

bool saveProject(const std::string& path, const Document& doc, std::string& error) {
    std::vector<uint8_t> out = {'N', 'X', 'E', '1'};
    put32(out, uint32_t(doc.width()));
    put32(out, uint32_t(doc.height()));
    put32(out, uint32_t(doc.layers().size()));
    put32(out, uint32_t(doc.activeIndex()));
    for (const Layer& l : doc.layers()) {
        putStr(out, l.name);
        put32(out, uint32_t(l.x));
        put32(out, uint32_t(l.y));
        uint32_t flags = (l.visible ? 1u : 0u) | (l.locked ? 2u : 0u) | (l.background ? 4u : 0u);
        put32(out, flags);
        put32(out, uint32_t(std::clamp(int(l.opacity * 1000 + 0.5), 0, 1000)));
        put32(out, uint32_t(l.blend));
        GdkPixbuf* pb = toPixbuf(l.img);
        gchar* buf = nullptr;
        gsize len = 0;
        GError* err = nullptr;
        gboolean ok = gdk_pixbuf_save_to_buffer(pb, &buf, &len, "png", &err, nullptr);
        g_object_unref(pb);
        if (!ok) {
            error = err ? err->message : "png encode failed";
            if (err) g_error_free(err);
            return false;
        }
        put32(out, uint32_t(len));
        out.insert(out.end(), buf, buf + len);
        g_free(buf);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        error = "cannot write " + path;
        return false;
    }
    f.write(reinterpret_cast<const char*>(out.data()), std::streamsize(out.size()));
    return bool(f);
}

bool loadProject(const std::string& path, Document& doc, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() < 4 || std::memcmp(data.data(), "NXE1", 4) != 0) {
        error = "not a NexImageEditor project";
        return false;
    }
    Reader r{data, 4};
    const int w = int(r.u32()), h = int(r.u32());
    const uint32_t count = r.u32();
    const int active = int(r.u32());
    std::vector<Layer> layers;
    for (uint32_t i = 0; i < count && r.ok; ++i) {
        Layer l;
        l.name = r.str();
        l.x = int(r.u32());
        l.y = int(r.u32());
        uint32_t flags = r.u32();
        l.visible = flags & 1u;
        l.locked = flags & 2u;
        l.background = flags & 4u;
        l.opacity = r.u32() / 1000.0;
        l.blend = BlendMode(r.u32());
        uint32_t len = r.u32();
        auto png = r.bytes(len);
        if (!r.ok) break;
        GdkPixbufLoader* loader = gdk_pixbuf_loader_new_with_type("png", nullptr);
        GError* err = nullptr;
        gdk_pixbuf_loader_write(loader, png.data(), png.size(), &err);
        gdk_pixbuf_loader_close(loader, nullptr);
        GdkPixbuf* pb = gdk_pixbuf_loader_get_pixbuf(loader);
        if (!pb) {
            error = err ? err->message : "corrupt layer";
            if (err) g_error_free(err);
            g_object_unref(loader);
            return false;
        }
        l.img = fromPixbuf(pb);
        g_object_unref(loader);
        layers.push_back(std::move(l));
    }
    if (!r.ok || layers.empty()) {
        error = "truncated project file";
        return false;
    }
    doc.setSize(w, h);
    doc.layers() = std::move(layers);
    doc.setActive(active);
    doc.selection = nullptr;
    doc.filePath = path;
    return true;
}

}  // namespace fileio
