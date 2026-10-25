#pragma once
#include <QtWidgets>
#include <cmath>
class NumericSlider : public QWidget {
    QAbstractSpinBox *editor;
    QDoubleSpinBox *realEditor;
    QSpinBox *intEditor;
    QSlider *slider;
    double lower, upper;
    double value() const;
    void sync();

  public:
    explicit NumericSlider(QAbstractSpinBox *input);
    QJsonObject state() const;
    void restoreState(QJsonObject state);
    bool test();
};
