#pragma once
#include <QDialog>
#include "../engine/ImageExport.h"

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QSlider;
class QPushButton;
class QLabel;
class QDialogButtonBox;

class ExportDialog : public QDialog {
public:
    explicit ExportDialog(QSize original, QWidget* parent = nullptr);
    QSize outputSize() const;
    QByteArray format() const;
    int quality() const;
    bool transparent() const;
    QColor background() const { return m_background; }
    ImageExport::Interpolation interpolation() const;
    void accept() override;
private:
    void dimensionsChanged(bool widthChanged);
    void updateOptions();
    QSize m_original;
    QColor m_background = Qt::white;
    QComboBox *m_format, *m_interpolation;
    QSpinBox *m_width, *m_height;
    QDoubleSpinBox* m_percent;
    QCheckBox *m_lockRatio, *m_transparent;
    QSlider* m_quality;
    QPushButton* m_backgroundButton;
    QLabel* m_hint;
    QDialogButtonBox* m_buttons;
    bool m_updating = false;
};
