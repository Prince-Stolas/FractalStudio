#include "gradient_editor.h"

double GradientBar::position(QPointF p) const {
    return qBound(0., (p.x() - 14) / qMax(1, width() - 28), 1.);
}

int GradientBar::nearest(QPointF p) const {
    int result = -1;
    double distance = 12;
    for (int i = 0; i < stops.size(); ++i) {
        double d =
            std::abs(14 + stops[i].toObject()["position"].toDouble() * (width() - 28) - p.x());
        if (d < distance) {
            distance = d;
            result = i;
        }
    }
    return result;
}

void GradientBar::sort() {
    QList<QJsonObject> values;
    QJsonValue chosen = stops[selected];
    for (auto v : stops)
        values << v.toObject();
    std::sort(values.begin(), values.end(),
              [](auto a, auto b) { return a["position"].toDouble() < b["position"].toDouble(); });
    stops = {};
    for (auto v : values)
        stops.append(v);
    for (int i = 0; i < stops.size(); ++i)
        if (stops[i] == chosen) {
            selected = i;
            break;
        }
}

void GradientBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    QRect bar(14, 10, width() - 28, 40);
    for (int x = 0; x < bar.width(); ++x) {
        p.setPen(gradientColour(stops, double(x) / qMax(1, bar.width() - 1)));
        p.drawLine(bar.left() + x, bar.top(), bar.left() + x, bar.bottom());
    }
    p.setPen(Qt::black);
    p.drawRect(bar);
    for (int i = 0; i < stops.size(); ++i) {
        int x = 14 + qRound(stops[i].toObject()["position"].toDouble() * (width() - 28));
        p.setPen(QPen(i == selected ? Qt::white : Qt::black, 2));
        p.setBrush(QColor(stops[i].toObject()["colour"].toString()));
        p.drawPolygon(QPolygon{QPoint(x, 53), QPoint(x - 7, 65), QPoint(x + 7, 65)});
    }
}

void GradientBar::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::RightButton) {
        dragging = false;
        selected = nearest(e->position());
        if (selected >= 0)
            removeSelected();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    selected = nearest(e->position());
    if (selected < 0) {
        stops.append(
            QJsonObject{{"position", position(e->position())},
                        {"colour", gradientColour(stops, position(e->position())).name()}});
        selected = stops.size() - 1;
        sort();
    }
    dragging = true;
    update();
    if (changed)
        changed();
}

void GradientBar::mouseMoveEvent(QMouseEvent *e) {
    if (!dragging || selected < 0)
        return;
    double value = position(e->position());
    for (int i = 0; i < stops.size(); ++i)
        if (i != selected && std::abs(stops[i].toObject()["position"].toDouble() - value) < .0001)
            return;
    auto stop = stops[selected].toObject();
    stop["position"] = value;
    stops[selected] = stop;
    sort();
    update();
    if (changed)
        changed();
}

void GradientBar::mouseReleaseEvent(QMouseEvent *) {
    dragging = false;
}

void GradientBar::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        editColour();
}

GradientBar::GradientBar(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 80);
}

double GradientBar::selectedPosition() const {
    return selected >= 0 && selected < stops.size()
               ? stops[selected].toObject()["position"].toDouble()
               : -1;
}

bool GradientBar::setSelectedPosition(double value) {
    if (selected < 0 || selected >= stops.size() || !std::isfinite(value) || value < 0 || value > 1)
        return false;
    for (int i = 0; i < stops.size(); ++i)
        if (i != selected && stops[i].toObject()["position"].toDouble() == value)
            return false;
    auto stop = stops[selected].toObject();
    stop["position"] = value;
    stops[selected] = stop;
    sort();
    update();
    if (changed)
        changed();
    return true;
}

void GradientBar::setStops(QJsonArray values) {
    stops = values;
    selected = stops.isEmpty() ? -1 : 0;
    dragging = false;
    update();
    if (changed)
        changed();
}

void GradientBar::editColour() {
    if (selected < 0 || selected >= stops.size())
        return;
    auto stop = stops[selected].toObject();
    QColor colour = QColorDialog::getColor(QColor(stop["colour"].toString()), this);
    if (colour.isValid()) {
        stop["colour"] = colour.name();
        stops[selected] = stop;
        update();
        if (changed)
            changed();
    }
}

void GradientBar::resetMonochrome() {
    stops = {QJsonObject{{"position", 0.0}, {"colour", "#000000"}},
             QJsonObject{{"position", 1.0}, {"colour", "#ffffff"}}};
    selected = 0;
    dragging = false;
    update();
    if (changed)
        changed();
}

void GradientBar::removeSelected() {
    if (stops.size() <= 2 || selected < 0)
        return;
    stops.removeAt(selected);
    selected = 0;
    update();
    if (changed)
        changed();
}
