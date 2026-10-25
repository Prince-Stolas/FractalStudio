#include "mainwindow.h"
#include <QtWidgets>

bool Playground::captureReadme(const QString &directory) {
    if (!QDir().mkpath(directory))
        return false;
    resize(1440, 960);
    active()->settings = Settings();
    active()->reset();
    load();
    QApplication::processEvents();
    if (!active()->apply())
        return false;
    auto save = [&](const QString &name) {
        QApplication::processEvents();
        active()->repaint();
        QImage source = grab().toImage().convertToFormat(QImage::Format_ARGB32);
        source.setDevicePixelRatio(1);
        const int margin = 32;
        QImage result(source.size() + QSize(margin * 2, margin * 2), QImage::Format_ARGB32);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(margin, margin, source.width(), source.height()), 20, 20);
        painter.setClipPath(clip);
        painter.drawImage(margin, margin, source);
        painter.end();
        return result.save(QDir(directory).filePath(name + ".png"));
    };
    if (!save("workspace"))
        return false;
    auto &settings = active()->settings;
    settings.variables[2].source = Constant;
    settings.variables[2].real = -0.745;
    settings.variables[2].imag = 0.186;
    settings.variables[3].source = Pixel;
    settings.iterations = 256;
    settings.palette = 2;
    active()->reset();
    load();
    if (!active()->apply() || !save("julia"))
        return false;
    settings = Settings();
    settings.iterations = 256;
    settings.colouring.mode = 1;
    settings.colouring.trap = 1;
    settings.colouring.trapRadius = 0.5;
    settings.palette = 1;
    settings.frequency = 0.04;
    active()->reset();
    load();
    if (!active()->apply())
        return false;
    for (auto *tabs : findChildren<QTabWidget *>()) {
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabText(i) == "Appearance")
                tabs->setCurrentIndex(i);
    }
    return save("orbit-trap");
}
