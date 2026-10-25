#include "mainwindow.h"
#include "workspace.h"
#include "ui_design.h"
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QtWidgets>
#include <cmath>
#include <complex>
#include <functional>
#include <stdexcept>

int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(4, 0);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    if (app.arguments().contains("--smoke-test"))
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
            fprintf(stderr, "%s\n", message.toUtf8().constData());
        });
    applyStudioTheme(app);
    app.setApplicationName("Fractal Studio");
    const int captureIndex = app.arguments().indexOf("--capture-ui");
    Playground window;
    window.show();
    if (captureIndex >= 0 && captureIndex + 1 < app.arguments().size()) {
        const QString path = app.arguments().at(captureIndex + 1);
        QTimer::singleShot(1200, &app, [&app, &window, path] {
            app.exit(window.grab().save(path, "PNG") ? 0 : 3);
        });
    }
    if (app.arguments().contains("--smoke-test"))
        QTimer::singleShot(700, &app, [&] { app.exit(window.smoke() ? 0 : 2); });
    return app.exec();
}
