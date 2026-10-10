#pragma once
#include <QWidget>
#include "../engine/PhotoEngine.h"

class HistogramWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget* parent = nullptr);

    void setHistogramData(const HistogramData& data);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    HistogramData m_data;
    bool m_hasData = false;
};
