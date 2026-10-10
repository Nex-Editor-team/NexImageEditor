#pragma once
#include "core/Pixels.h"

#include <QColor>
#include <QDialog>
#include <QImage>
#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

class NewDocumentDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewDocumentDialog(QWidget* parent = nullptr);
    int width() const;
    int height() const;
    QColor background() const;

private:
    QSpinBox* m_w;
    QSpinBox* m_h;
    QComboBox* m_bg;
};

// Image Size (anchor hidden) or Canvas Size (anchor shown).
class SizeDialog : public QDialog {
    Q_OBJECT
public:
    SizeDialog(QWidget* parent, const QString& title, int w, int h, bool canvasMode);
    int width() const;
    int height() const;
    int anchorX() const;  // 0..2
    int anchorY() const;  // 0..2

private:
    QSpinBox* m_w;
    QSpinBox* m_h;
    QCheckBox* m_keep = nullptr;
    QComboBox* m_anchor = nullptr;
    double m_ratio;
    bool m_updating = false;
};

// A pixel filter with integer sliders. `apply` receives the slider values.
struct FilterParam {
    QString label;
    int min;
    int max;
    int def;
};

struct FilterSpec {
    QString title;
    std::vector<FilterParam> params;
    std::function<void(Pixels&, const std::vector<int>&)> apply;
};

// Dialog with live preview. Preview runs on a downscaled copy for responsiveness.
class FilterDialog : public QDialog {
    Q_OBJECT
public:
    FilterDialog(QWidget* parent, const FilterSpec& spec, const QImage& source);
    std::vector<int> values() const;

private:
    void updatePreview();
    FilterSpec m_spec;
    Pixels m_src;
    std::vector<class QSlider*> m_sliders;
    QLabel* m_preview;
};
