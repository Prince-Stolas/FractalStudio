#pragma once
#include <QtWidgets>
#include <cmath>
#include <stdexcept>
// Expressions compile to GLSL rather than being evaluated on the CPU per pixel.
class Equation {
    QString text;
    int pos = 0;
    QStringList names;
    void space();
    bool eat(QChar c);
    QString sum();
    QString product();
    QString unary();
    QString power();
    QString atom();

  public:
    static bool validName(QString name);
    static QString compile(QString source, QStringList names = {"c", "p"});
};
