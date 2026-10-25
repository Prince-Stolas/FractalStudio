#pragma once
#include <QtWidgets>
enum Source { Constant, Mouse, Pixel, Expression };
struct Variable {
    int source = Constant;
    double real = 0, imag = 0;
    QString equation = "0";
    int modifiers = 0;
};
struct Settings {
    QString equation = "z[n]^p+c";
    QStringList names = {"", "", "c", "", "p"};
    int iterations = 128, palette = 0;
    double bailout = 16, frequency = .025, phase = 0, smoothing = 1;
    QJsonArray gradient = {QJsonObject{{"position", 0.0}, {"colour", "#ffffff"}},
                           QJsonObject{{"position", .25}, {"colour", "#5599ff"}},
                           QJsonObject{{"position", .65}, {"colour", "#e060bb"}},
                           QJsonObject{{"position", 1.0}, {"colour", "#000000"}}};
    QMap<int, QPointF> negativeHistory;
    QString gradientName = "Default";
    bool orbit = false;
    int orbitSteps = 100;
    QVector<Variable> variables = {{Constant, 1, 0, "0", int(Qt::ShiftModifier)},
                                   {Constant, 0, 0, "0", int(Qt::ControlModifier)},
                                   {Pixel, -.745, .186, "0", 0},
                                   {Constant, 0, 0, "0", int(Qt::AltModifier)},
                                   {Constant, 2, 0, "0", 0}};
};
