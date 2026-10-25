#pragma once
#include <QtWidgets>
#include <functional>

inline QColor gradientColour(const QJsonArray &stops, double position) {
    if (stops.isEmpty())
        return Qt::black;
    auto first = stops.first().toObject();
    if (position <= first["position"].toDouble())
        return QColor(first["colour"].toString());
    for (int i = 1; i < stops.size(); ++i) {
        auto a = stops[i - 1].toObject(), b = stops[i].toObject();
        double end = b["position"].toDouble();
        if (position <= end) {
            double t = qBound(
                0., (position - a["position"].toDouble()) / (end - a["position"].toDouble()), 1.);
            t = t * t * (3 - 2 * t);
            QColor x(a["colour"].toString()), y(b["colour"].toString());
            return QColor::fromRgbF(x.redF() * (1 - t) + y.redF() * t,
                                    x.greenF() * (1 - t) + y.greenF() * t,
                                    x.blueF() * (1 - t) + y.blueF() * t);
        }
    }
    return QColor(stops.last().toObject()["colour"].toString());
}
class GradientBar : public QWidget {
    int selected = 0;
    bool dragging = false;
    double position(QPointF p) const;
    int nearest(QPointF p) const;
    void sort();

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

  public:
    QJsonArray stops;
    std::function<void()> changed;
    explicit GradientBar(QWidget *parent = nullptr);
    double selectedPosition() const;
    bool setSelectedPosition(double value);
    void setStops(QJsonArray values);
    void editColour();
    void resetMonochrome();
    void removeSelected();
};
