#include "mainwindow.h"
#include "workspace.h"
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
    app.setStyle("Fusion");
    app.setApplicationName("Fractal Studio");
    app.setStyleSheet(
        "QGroupBox { font-weight: 600; margin-top: 12px; padding-top: 12px; } QGroupBox::title { "
        "subcontrol-origin: margin; left: 8px; } QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { "
        "min-height: 24px; } QPushButton { padding: 6px 12px; } QToolBar { spacing: 6px; padding: "
        "4px; } QDockWidget::title { padding: 8px; } QMenu { padding: 4px; } QMenu::item { "
        "padding: 7px 24px; }");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#20232b"));
    palette.setColor(QPalette::WindowText, QColor("#e5e9f0"));
    palette.setColor(QPalette::Base, QColor("#151820"));
    palette.setColor(QPalette::AlternateBase, QColor("#252a35"));
    palette.setColor(QPalette::Text, QColor("#e5e9f0"));
    palette.setColor(QPalette::Button, QColor("#303643"));
    palette.setColor(QPalette::ButtonText, QColor("#e5e9f0"));
    palette.setColor(QPalette::Highlight, QColor("#527ec4"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);
    Playground window;
    window.show();
    if (app.arguments().contains("--smoke-test"))
        QTimer::singleShot(700, &app, [&] { app.exit(window.smoke() ? 0 : 2); });
    return app.exec();
}
