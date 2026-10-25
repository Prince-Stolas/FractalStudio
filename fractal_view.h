#pragma once
#include "equation.h"
#include "fractal_settings.h"
#include "gradient_editor.h"
#include "workspace.h"
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
class View : public QOpenGLWidget, protected QOpenGLFunctions_4_0_Core {
  public:
    Settings settings;
    long double cx = -.5L, cy = 0, span = 3;
    std::function<void(double, double, Qt::KeyboardModifiers)> mousePicked;
    std::function<void(QString)> status;
    std::function<void()> activated;
    QString lastError;
    quint64 shaderBuildCount = 0, gradientUploadCount = 0, interiorUploadCount = 0;
    quint64 convergenceCalibrationCount = 0;
    QVector2D calibratedConvergenceRange() const { return convergenceRange; }

  private:
    QOpenGLShaderProgram *shader = nullptr;
    GLuint gradientTexture = 0, interiorTexture = 0;
    QJsonArray uploadedGradient, uploadedInterior;
    QString compiledFragment;
    QByteArray convergenceCalibrationKey;
    QVector2D convergenceRange = QVector2D(0, 1);
    void calibrateConvergence(float width, float height);
    GLuint vao = 0;
    QPoint last;
    bool dragging = false, picking = false, hasPoint = false;
    double px = 0, py = 0;
    QString expr = "(cpow(z,var4)+var2)", initial = "dvec2(0.0,0.0)";
    bool ready = false;
    QSize exportSize;
    void drawOrbit(QPainter &painter, QSize size);
    QString negativeDeclarations() const;
    QString declarations() const;
    QString bindings() const;
    QString fragment() const;
    bool rebuild();
    void uniform(const char *name, double x);
    void uniform2(const char *name, double x, double y);
    void inspect(QPointF pos, bool link);

  protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

  public:
    explicit View(QWidget *parent = nullptr);
    ~View();
    std::pair<long double, long double> coordinate(QPointF pos) const;
    void reset();
    bool apply();
    QImage renderImage(QSize size);
    bool check();
};
