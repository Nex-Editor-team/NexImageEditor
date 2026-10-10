#pragma once
#include "core/Color.h"

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

struct ParamSpec {
    std::string label;
    double min, max, value, step;
};

void showNewDocument(Gtk::Window& parent, std::function<void(int w, int h, Color bg, const std::string& name)> onOk);

// Canvas size (anchor offsets the existing content) or image size (scales content).
void showSize(Gtk::Window& parent, bool imageSize, int curW, int curH,
              std::function<void(int w, int h, int offsetX, int offsetY)> onOk);

// Filter / adjustment parameters with live preview. onChange runs on every change.
void showParams(Gtk::Window& parent, const std::string& title, const std::vector<ParamSpec>& specs,
                std::function<void(const std::vector<double>&)> onChange, std::function<void()> onOk,
                std::function<void()> onCancel);

void showMessage(Gtk::Window& parent, const std::string& text);
void confirmDiscard(Gtk::Window& parent, const std::string& text, std::function<void()> onDiscard);
