#include "equation.h"

bool Equation::validName(QString name) {
    return QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$").match(name).hasMatch() &&
           !QStringList{"z",   "z_n", "n",    "pixel", "i",    "pi",    "sin", "cos",
                        "exp", "log", "conj", "abs",   "norm", "floor", "ceil"}
                .contains(name);
}

void Equation::space() {
    while (pos < text.size() && text[pos].isSpace())
        ++pos;
}

bool Equation::eat(QChar c) {
    space();
    if (pos < text.size() && text[pos] == c) {
        ++pos;
        return true;
    }
    return false;
}

QString Equation::sum() {
    QString a = product();
    for (;;) {
        if (eat('+'))
            a = "(" + a + "+" + product() + ")";
        else if (eat('-'))
            a = "(" + a + "-" + product() + ")";
        else
            return a;
    }
}

QString Equation::product() {
    QString a = unary();
    for (;;) {
        if (eat('*'))
            a = "mul(" + a + "," + unary() + ")";
        else if (eat('/'))
            a = "divide(" + a + "," + unary() + ")";
        else if (eat('%'))
            a = "cmod(" + a + "," + unary() + ")";
        else
            return a;
    }
}

QString Equation::unary() {
    if (eat('-'))
        return "(-" + unary() + ")";
    if (eat('+'))
        return unary();
    return power();
}

QString Equation::power() {
    QString a = atom();
    if (eat('^'))
        a = "cpow(" + a + "," + unary() + ")";
    return a;
}

QString Equation::atom() {
    space();
    if (eat('(')) {
        QString a = sum();
        if (!eat(')'))
            throw std::runtime_error("Expected closing parenthesis");
        return a;
    }
    int start = pos;
    if (pos < text.size() && (text[pos].isDigit() || text[pos] == '.')) {
        bool digit = false;
        while (pos < text.size() && text[pos].isDigit()) {
            ++pos;
            digit = true;
        }
        if (pos < text.size() && text[pos] == '.') {
            ++pos;
            while (pos < text.size() && text[pos].isDigit()) {
                ++pos;
                digit = true;
            }
        }
        if (!digit)
            throw std::runtime_error("Invalid number");
        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
            ++pos;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
                ++pos;
            int s = pos;
            while (pos < text.size() && text[pos].isDigit())
                ++pos;
            if (s == pos)
                throw std::runtime_error("Invalid exponent");
        }
        bool ok;
        double n = text.mid(start, pos - start).toDouble(&ok);
        if (!ok || !std::isfinite(n))
            throw std::runtime_error("Invalid number");
        return "dvec2(" + QString::number(n, 'g', 17) +
               (std::floor(n) == n && std::abs(n) < 1e15 ? ".0" : "") + ",0.0)";
    }
    while (pos < text.size() && (text[pos].isLetterOrNumber() || text[pos] == '_'))
        ++pos;
    QString id = text.mid(start, pos - start);
    if (id == "z") {
        if (!eat('['))
            throw std::runtime_error("Use z[index], not bare z");
        QString index = sum();
        if (!eat(']'))
            throw std::runtime_error("Expected ]");
        return index == "dvec2(n,0.0)" ? QString("z") : "historyAt(" + index + ")";
    }
    if (id == "z_n")
        throw std::runtime_error("Use z[n], not z_n");
    if (id == "pixel")
        return id;
    if (names.contains(id))
        return "var" + QString::number(names.indexOf(id));
    if (id == "n")
        return "dvec2(" + id + ",0.0)";
    if (id == "i")
        return "dvec2(0.0,1.0)";
    if (id == "pi")
        return "dvec2(3.141592653589793,0.0)";
    QString fn;
    if (id == "sin")
        fn = "csin";
    if (id == "cos")
        fn = "ccos";
    if (id == "exp")
        fn = "cexp";
    if (id == "log")
        fn = "clog";
    if (id == "conj")
        fn = "conjugate";
    if (id == "abs")
        fn = "componentAbs";
    if (id == "norm")
        fn = "magnitude";
    if (id == "floor" || id == "ceil")
        fn = id;
    if (fn.isEmpty())
        throw std::runtime_error(("Unknown name: " + id).toStdString());
    if (!eat('('))
        throw std::runtime_error("Expected function argument");
    QString a = sum();
    if (!eat(')'))
        throw std::runtime_error("Expected closing parenthesis");
    return fn + "(" + a + ")";
}

QString Equation::compile(QString source, QStringList names) {
    if (source.size() > 512)
        throw std::runtime_error("Equation is too long (maximum 512 characters)");
    Equation e;
    e.text = source;
    e.names = names;
    QString result = e.sum();
    e.space();
    if (e.pos != source.size())
        throw std::runtime_error("Unexpected character in equation");
    return result;
}
