#pragma once
#include <QtWidgets>
#include <complex>
#include <functional>
#include <limits>

class ComplexExpression {
    using C = std::complex<double>;
    QString text;
    int pos = 0;
    std::function<C(QString)> variable;
    std::function<C(int)> history;
    void space();
    bool eat(QChar ch);
    C sum();
    C product();
    C unary();
    C atom();

  public:
    static C evaluate(QString text, std::function<C(QString)> variables,
                      std::function<C(int)> history = {});
};
