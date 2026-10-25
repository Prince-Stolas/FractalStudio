#pragma once
#include "fractal_view.h"
#include "numeric_slider.h"
class Playground : public QMainWindow {
    QJsonArray gradientLibrary;
    QString gradientPath() const;
    void persistGradients();
    void refreshPalettes();
    void initGradients();
    struct PickRoute {
        int source, target, variable, modifiers;
    };
    QList<PickRoute> routes;
    QJsonArray tracks;
    QTimer animationTimer, historyTimer;
    QElapsedTimer animationClock;
    double animationDuration = 10;
    int animationFps = 60;
    qint64 lastAnimationUi = -200;
    bool animationLoop = false, restoring = false;
    QList<QJsonObject> history;
    int historyIndex = -1;
    View *byNumber(int number) const;
    QJsonObject settingsJson(const Settings &s) const;
    Settings readSettings(QJsonObject o) const;
    QJsonObject layoutJson(QWidget *widget) const;
    QList<NumericSlider *> numericSliders() const;
    QJsonObject snapshot() const;
    bool restore(QJsonObject state);
    void checkpoint();
    void travel(int direction);
    void workspaceFile(bool save);
    void exportDialog();
    void gradientDialog();
    void negativeHistoryDialog();
    void orbitDialog();
    void linksDialog();
    QStringList parameterNames() const;
    void validateTracks(QJsonArray values, QList<int> ids) const;
    void setParameter(View *view, int parameter, double value);
    void animateAt(double time);
    void timelineDialog();
    void setupTools();

    View *left, *right;
    QDockWidget *dock;
    QComboBox *target, *palette;
    void presetGallery();
    void initializeWorkspace();
    QLineEdit *equation;
    QLabel *error, *coordinates;
    QDoubleSpinBox *bailout, *frequency, *phase, *smoothing;
    QSpinBox *iterations, *historySize;
    QList<View *> views;
    int nextNumber = 1;
    void viewOperations();
    QString viewName(View *view) const;
    void refreshViews(View *selected);
    void removeView();
    bool copySettings(View *source, View *destination);
    QSplitter *root;
    QToolBar *toolbar;
    bool fullscreenMode = false, wasMaximized = false, dockWasVisible = false;
    QList<QWidget *> fullscreenHidden;
    void bindView(View *view);
    void balance();
    void splitView(Qt::Orientation orientation);
    void leaveFullscreen();
    void enterFullscreen(bool single);
    struct VariableControls {
        QComboBox *source, *modifier;
        QWidget *values, *picking;
        QDoubleSpinBox *real, *imag;
        QLineEdit *expression;
    };
    QVector<VariableControls> controls;
    QFormLayout *variableForm = nullptr;
    QDockWidget *variableDock = nullptr;
    QStringList controlNames;
    void rebuildVariables();
    void manageVariables();
    void visibility(int i);
    void addVariable(QFormLayout *form, int i);
    bool loading = false;
    View *active() const;
    QDoubleSpinBox *number(double min, double max, int decimals = 8);
    void load();
    void apply();
    void choosePreset(int index);
    bool makeJulia(View *source, View *destination);
    void picked(View *source, double x, double y, Qt::KeyboardModifiers modifiers);
    void pickInto(View *dest, double x, double y, Qt::KeyboardModifiers modifiers);

  public:
    Playground();
    bool smoke();
};
