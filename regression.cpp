#include "mainwindow.h"

bool Playground::smoke() {
    bool defaults = Equation::validName("p") && active()->settings.names.contains("p") &&
                    active()->settings.variables.last().real == 2 && views.size() == 1 &&
                    active()->settings.iterations == 128 &&
                    !active()->settings.names.contains("a") &&
                    !active()->settings.names.contains("b");
    auto *inspectorTabs = findChild<QTabWidget *>("inspectorTabs");
    defaults = defaults && inspectorTabs && inspectorTabs->count() == 3 &&
        inspectorTabs->tabText(0) == "Fractal" &&
        inspectorTabs->tabText(1) == "Parameters" &&
        inspectorTabs->tabText(2) == "Appearance" &&
        findChild<QPushButton *>("openPresetGallery") &&
        variableDock->allowedAreas() == Qt::AllDockWidgetAreas &&
        findChildren<QDockWidget *>("variableEditor").size() == 1;
    bool hasPresetAction = false, hasAnimationMenu = false;
    for (auto *action : menuBar()->actions()) {
        hasAnimationMenu |= action->text() == "&Animation";
        if (auto *menu = action->menu()) {
            for (auto *entry : menu->actions()) {
                if (entry->text() == "Start from preset…")
                    hasPresetAction = !entry->icon().isNull();
            }
        }
    }
    defaults = defaults && hasPresetAction && hasAnimationMenu &&
        !toolbar->actions().isEmpty() &&
        findChild<QComboBox *>("activeViewSelector") == target;
    defaults = defaults && findChild<QAction *>("splitHorizontal") &&
        findChild<QAction *>("splitVertical") && findChild<QAction *>("closeActiveView");
    QColor background = qApp->palette().color(QPalette::Window);
    defaults = defaults && background.red() == background.green() &&
        background.green() == background.blue() &&
        !QIcon(":/ui/icons/chevron-up.svg").isNull();
    splitView(Qt::Horizontal);
    right = views.last();
    left->settings.names = {"a", "b", "c", "", "p"};
    right->settings.names = left->settings.names;
    target->setCurrentIndex(0);
    load();
    auto *realTest = new QDoubleSpinBox;
    realTest->setRange(-10, 10);
    realTest->setDecimals(8);
    auto *integerTest = new QSpinBox;
    integerTest->setRange(16, 10000);
    NumericSlider realSlider(realTest), integerSlider(integerTest);
    bool ok =
        defaults && realSlider.test() && integerSlider.test() && left->check() && right->check();
    for (const QString &s :
         {QString("z[n]^p+c"), QString("abs(z[n])^p+c"), QString("conj(z[n])^p+c"),
          QString("sin(z[n])*a+c"), QString("z[n]-(z[n]^3-1)/(3*z[n]^2)+c"),
          QString("exp(z[n])+log(c)+cos(z[n])+norm(b)+i+pi+n")}) {
        left->settings.equation = s;
        ok = left->apply() && ok;
    }
    target->setCurrentIndex(0);
    left->settings = Settings();
    left->settings.names = {"a", "b", "c", "", "p"};
    left->settings.equation = "conj(z[n])^p+c+a";
    load();
    ok = makeJulia(left, right) && ok;
    ok = ok && right->settings.equation == left->settings.equation &&
         right->settings.variables[3].source == Pixel &&
         right->settings.variables[2].source == Mouse;
    picked(right, -.75, .1, Qt::NoModifier);
    ok = ok && right->settings.variables[2].real == -.75;
    for (int i = 0; i < 4; ++i) {
        right->settings.variables[i].source = Mouse;
    }
    picked(right, .125, -.25, Qt::AltModifier);
    ok = ok && right->settings.variables[3].real == .125 &&
         right->settings.variables[3].imag == -.25;
    picked(right, .2, .3, Qt::ShiftModifier);
    ok = ok && right->settings.variables[0].real == .2;
    picked(right, .4, .5, Qt::ControlModifier);
    ok = ok && right->settings.variables[1].imag == .5;
    right->settings.variables[0].modifiers = int(Qt::ShiftModifier | Qt::AltModifier);
    picked(right, .6, .7, Qt::ShiftModifier | Qt::AltModifier);
    ok = ok && right->settings.variables[0].real == .6;
    right->settings.variables[3].source = Expression;
    right->settings.variables[3].equation = "a+b*c";
    ok = right->apply() && ok;
    for (int i = 0; i < 3; ++i) {
        right->settings.variables[i].source = Pixel;
    }
    ok = right->apply() && ok;
    right->update();
    left->settings.variables[3].source = Pixel;
    ok = !makeJulia(left, right) && ok;
    splitView(Qt::Horizontal);
    splitView(Qt::Vertical);
    balance();
    ok = ok && views.size() == 4 && active() == views.last();
    enterFullscreen(true);
    ok = ok && isFullScreen() && toolbar->isHidden();
    leaveFullscreen();
    enterFullscreen(false);
    ok = ok && isFullScreen();
    leaveFullscreen();
    for (auto *view : views)
        ok = ok && !view->isHidden();
    View *third = views[2], *fourth = views[3];
    third->settings = Settings();
    fourth->settings.palette = 4;
    fourth->settings.frequency = .123;
    fourth->settings.phase = .4;
    fourth->settings.smoothing = .3;
    ok = makeJulia(third, fourth) && ok;
    ok = ok && fourth->settings.palette == 4 && fourth->settings.frequency == .123 &&
         fourth->settings.phase == .4 && fourth->settings.smoothing == .3;
    ok = copySettings(third, fourth) && ok;
    ok = ok && fourth->settings.variables[2].source == Pixel;
    removeView();
    ok = ok && views.size() == 3;
    removeView();
    removeView();
    ok = ok && views.size() == 1;
    removeView();
    ok = ok && views.size() == 1;
    active()->settings = Settings();
    active()->settings.palette = 5;
    active()->apply();
    QImage exported = active()->renderImage({320, 240});
    ok = ok && exported.size() == QSize(320, 240);
    QTemporaryDir temporary;
    QString imagePath = temporary.filePath("test.png");
    ok = ok && exported.save(imagePath, "PNG") && QImage(imagePath).size() == QSize(320, 240);
    animationFps = 120;
    auto saved = snapshot();
    ok = restore(saved) && ok;
    ok = ok && animationFps == 120;
    ok = ok && active()->settings.palette == 5 && views.size() == 1;
    auto bad = saved;
    bad["version"] = 99;
    ok = !restore(bad) && ok;
    int id = active()->property("number").toInt();
    tracks = {QJsonObject{{"view", id}, {"parameter", 12}, {"time", 0.0}, {"value", 0.0}},
              QJsonObject{{"view", id}, {"parameter", 12}, {"time", 2.0}, {"value", 1.0}}};
    animateAt(1);
    ok = ok && std::abs(active()->settings.phase - .5) < 1e-9;
    history.clear();
    historyIndex = -1;
    checkpoint();
    active()->settings.phase = .75;
    checkpoint();
    travel(-1);
    ok = ok && std::abs(active()->settings.phase - .5) < 1e-9;
    travel(1);
    ok = ok && std::abs(active()->settings.phase - .75) < 1e-9;
    auto orbit = ComplexExpression::evaluate(
        "z[0]^2+c",
        [](QString name) {
            return name == "n" ? std::complex<double>(0) : std::complex<double>(.5, 0);
        },
        [](int) { return std::complex<double>(1, 1); });
    ok = ok && std::abs(orbit - std::complex<double>(.5, 2)) < 1e-9;
    active()->settings.orbit = true;
    QMouseEvent orbitClick(QEvent::MouseButtonPress, QPointF(20, 20), QPointF(20, 20),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(active(), &orbitClick);
    active()->renderImage({96, 96});
    active()->update();
    splitView(Qt::Horizontal);
    View *routeSource = views.first(), *routeTarget = views.last();
    routes << PickRoute{routeSource->property("number").toInt(),
                        routeTarget->property("number").toInt(), 1, int(Qt::AltModifier)};
    picked(routeSource, .42, -.13, Qt::AltModifier);
    ok = ok && routeTarget->settings.variables[1].real == .42;
    for (int mode : {int(Constant), int(Mouse), int(Pixel)}) {
        routeTarget->settings.variables[1].source = mode;
        picked(routeSource, .31, .27, Qt::AltModifier);
        ok = ok && routeTarget->settings.variables[1].source == (mode == Pixel ? Constant : mode);
    }
    routeTarget->settings.variables[1].source = Constant;
    picked(routeSource, .42, -.13, Qt::AltModifier);
    picked(routeTarget, .99, .88, Qt::ControlModifier);
    ok = ok && routeTarget->settings.variables[1].real == .42;
    routes.last().variable = 3;
    routeTarget->settings.variables[3].source = Expression;
    routeTarget->settings.variables[3].equation = "c+1";
    picked(routeSource, .2, .1, Qt::AltModifier);
    ok = ok && routeTarget->settings.variables[3].source == Expression;
    routes.last().variable = 1;
    auto multi = snapshot();
    ok = restore(multi) && ok;
    ok = ok && views.size() == 2 && routes.size() == 1 && tracks.size() == 2;
    QSaveFile workspace(temporary.filePath("workspace.json"));
    ok = workspace.open(QIODevice::WriteOnly) && ok;
    workspace.write(QJsonDocument(multi).toJson());
    ok = workspace.commit() && ok;
    active()->renderImage({64, 64});
    for (int i = 0; i < views.size(); ++i)
        ok = ok && views[i]->property("number").toInt() == i + 1;
    target->setCurrentIndex(0);
    removeView();
    ok = ok && views.first()->property("number").toInt() == 1;
    splitView(Qt::Horizontal);
    ok = ok && active()->property("number").toInt() == 2;
    routes.clear();
    views[0]->settings.variables[2].source = Mouse;
    views[1]->settings.variables[2].source = Mouse;
    views[1]->settings.variables[2].real = .123;
    picked(views[0], .9, .8, Qt::NoModifier);
    ok = ok && views[0]->settings.variables[2].real == .9 &&
         views[1]->settings.variables[2].real == .123;
    GradientBar bar;
    bar.resize(400, 80);
    bar.stops = Settings().gradient;
    QMouseEvent addStop(QEvent::MouseButtonPress, QPointF(200, 30), QPointF(200, 30),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&bar, &addStop);
    ok = ok && bar.stops.size() == 5;
    QMouseEvent moveStop(QEvent::MouseMove, QPointF(260, 60), QPointF(260, 60), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&bar, &moveStop);
    ok = ok && bar.stops[3].toObject()["position"].toDouble() > .65;
    for (int i = 0; i < 25; ++i)
        gradientLibrary.append(
            QJsonObject{{"name", QString("Test %1").arg(i)}, {"stops", bar.stops}});
    refreshPalettes();
    ok = ok && palette->count() >= 29 && palette->findText("White / Colour / Black") < 0;
    QMouseEvent deleteStop(QEvent::MouseButtonPress, QPointF(260, 60), QPointF(260, 60),
                           Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(&bar, &deleteStop);
    ok = ok && bar.stops.size() == 4;
    bar.resetMonochrome();
    ok = ok && bar.stops.size() == 2 && gradientColour(bar.stops, 0) == QColor(Qt::black) &&
         gradientColour(bar.stops, 1) == QColor(Qt::white);
    QMouseEvent keepMinimum(QEvent::MouseButtonPress, QPointF(14, 60), QPointF(14, 60),
                            Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(&bar, &keepMinimum);
    ok = ok && bar.stops.size() == 2;
    ok = bar.setSelectedPosition(.5) && ok;
    ok = ok && bar.selectedPosition() == .5;
    ok = !bar.setSelectedPosition(1) && ok;
    ok = ok && bar.selectedPosition() == .5;
    bar.setStops(Settings().gradient);
    ok = ok && bar.selectedPosition() == 0;
    active()->settings.palette = 5;
    active()->settings.gradientName = gradientLibrary.last().toObject()["name"].toString();
    active()->settings.gradient = gradientLibrary.last().toObject()["stops"].toArray();
    active()->settings.frequency = .12;
    active()->settings.phase = .34;
    active()->settings.smoothing = .56;
    Settings appearance = active()->settings;
    for (int presetIndex = 0; presetIndex < 6; ++presetIndex) {
        choosePreset(presetIndex);
        const auto &s = active()->settings;
        ok = ok && s.palette == appearance.palette && s.gradientName == appearance.gradientName &&
             s.gradient == appearance.gradient && s.frequency == appearance.frequency &&
             s.phase == appearance.phase && s.smoothing == appearance.smoothing;
    }
    active()->settings.names.append("custom_parameter");
    active()->settings.variables.append(
        Variable{Constant, .25, .1, "0", int(Qt::ControlModifier | Qt::AltModifier)});
    active()->settings.equation = "z[n]^2+custom_parameter+floor(c)+ceil(c)+(c%2)";
    ok = active()->apply() && ok;
    ok = readSettings(settingsJson(active()->settings)).names.contains("custom_parameter") && ok;
    active()->settings.variables.last().source = Mouse;
    picked(active(), .61, .72, Qt::ControlModifier | Qt::AltModifier);
    ok = ok && active()->settings.variables.last().real == .61;
    for (QString invalid : {QString("z"), QString("z_n")}) {
        try {
            Equation::compile(invalid);
            ok = false;
        } catch (const std::exception &) {
        }
        try {
            ComplexExpression::evaluate(invalid, [](QString) { return std::complex<double>(0); });
            ok = false;
        } catch (const std::exception &) {
        }
    }
    auto rounded = ComplexExpression::evaluate("floor(-1.2+2.8*i)+ceil(1.2-2.8*i)+(-3%2)",
                                               [](QString) { return std::complex<double>(0); });
    ok = ok && std::abs(rounded - std::complex<double>(1, 0)) < 1e-9;
    active()->settings = Settings();
    active()->settings.variables[2].source = Constant;
    active()->settings.variables[2].real = 0;
    active()->settings.variables[2].imag = 0;
    active()->settings.historySize = 2;
    active()->settings.equation = "z[n-n%2]*0.1+0.1";
    ok = active()->apply() && ok;
    QImage rolling = active()->renderImage({32, 32});
    ok = ok && !rolling.isNull() && rolling.pixelColor(16, 16) != QColor(255, 0, 255);
    active()->settings.equation = "z[0]*0.1+0.1";
    ok = active()->apply() && ok;
    QImage expired = active()->renderImage({32, 32});
    ok = ok && expired.pixelColor(16, 16) == QColor(255, 0, 255);
    active()->settings.equation = "z[n+1]";
    ok = active()->apply() && ok;
    QImage future = active()->renderImage({32, 32});
    ok = ok && future.pixelColor(16, 16) == QColor(255, 0, 255);
    active()->settings.equation = "z[n]^2+z[n-1]*0.1";
    active()->settings.negativeHistory.clear();
    ok = active()->apply() && ok;
    QImage zeroPast = active()->renderImage({32, 32});
    ok = ok && zeroPast.pixelColor(16, 16) != QColor(255, 0, 255);
    active()->settings.equation = "z[-1]";
    active()->settings.negativeHistory[-1] = QPointF(100, 2);
    ok = active()->apply() && ok;
    QImage seeded = active()->renderImage({32, 32});
    ok = ok && seeded.pixelColor(16, 16) != zeroPast.pixelColor(16, 16) &&
         seeded.pixelColor(16, 16) != QColor(255, 0, 255);
    Settings seedRoundTrip = readSettings(settingsJson(active()->settings));
    ok = ok && seedRoundTrip.negativeHistory.value(-1) == QPointF(100, 2);
    auto defaultNegative =
        ComplexExpression::evaluate("z[-1]", [](QString) { return std::complex<double>(0); });
    ok = ok && defaultNegative == std::complex<double>(0);
    auto customNegative = ComplexExpression::evaluate(
        "z[-1]+z[0]", [](QString) { return std::complex<double>(0); },
        [](int index) {
            return index < 0 ? std::complex<double>(2, 3) : std::complex<double>(1, 0);
        });
    ok = ok && customNegative == std::complex<double>(3, 3);
    active()->settings = Settings();
    active()->settings.names.append("other");
    active()->settings.variables.append(Variable{});
    auto &matching = active()->settings.variables;
    matching[2].source = Mouse;
    matching.last().source = Mouse;
    matching[2].modifiers = 0;
    matching.last().modifiers = 0;
    load();
    picked(active(), .25, -.5, Qt::NoModifier);
    ok =
        ok && matching[2].real == .25 && matching.last().real == .25 && matching.last().imag == -.5;
    variableDock->show();
    variableDock->setFloating(true);
    ok = ok && variableDock->isFloating();
    variableDock->setFloating(false);
    ok = ok && !variableDock->isFloating();
    variableDock->hide();
    try {
        Equation::compile("z; invalid");
        ok = false;
    } catch (const std::exception &) {
    }
    active()->settings = Settings();
    auto &regular = active()->settings;
    int pIndex = regular.names.indexOf("p");
    regular.variables[pIndex].source = Mouse;
    regular.variables[pIndex].modifiers = int(Qt::ShiftModifier);
    load();
    picked(active(), 1.75, .25, Qt::ShiftModifier);
    ok = ok && regular.variables[pIndex].real == 1.75 && regular.variables[pIndex].imag == .25;
    regular.variables[pIndex].source = Pixel;
    ok = active()->apply() && ok;
    ok = ok && !active()->renderImage({32, 32}).isNull();
    auto ordinaryRoundTrip = readSettings(settingsJson(regular));
    ok = ok && ordinaryRoundTrip.variables[ordinaryRoundTrip.names.indexOf("p")].source == Pixel;
    regular.equation = "z[n]^2+c";
    regular.names.removeAt(pIndex);
    regular.variables.removeAt(pIndex);
    ok = active()->apply() && ok;
    regular.equation = "z[n]^p+c";
    ok = !active()->apply() && ok;
    ok = ok && Settings().historySize == 1 && variableDock->minimumWidth() >= 320 && variableDock->minimumWidth() <= 360;
    auto components = ComplexExpression::evaluate("real(3+2i)+imag(1-4i)+2i", [](QString){return std::complex<double>(0);});
    ok = ok && std::abs(components - std::complex<double>(-1, 2)) < 1e-9;
    active()->settings = Settings();
    active()->settings.equation = "real(z[n])+imag(c)*i+2i";
    ok = active()->apply() && ok;
    ok = ok && !active()->renderImage({16,16}).isNull();
    active()->settings.equation = "z[n-2]*0.1+0.1";
    active()->settings.historySize = 1;
    ok = active()->apply() && ok;
    ok = ok && active()->renderImage({16,16}).pixelColor(8,8) == QColor(255,0,255);
    active()->settings.historySize = 3;
    ok = active()->apply() && ok;
    ok = ok && active()->renderImage({16,16}).pixelColor(8,8) != QColor(255,0,255);
    ok = ok && readSettings(settingsJson(active()->settings)).historySize == 3;
    auto *colourView = active();
    colourView->settings = Settings();
    auto &colourSettings = colourView->settings;
    colourSettings.colouring.escapeSolid = true;
    colourSettings.colouring.escapeColour = QColor("#ff0000");
    colourSettings.colouring.interiorColour = QColor("#00ff00");
    colourSettings.equation = "n+1";
    colourSettings.iterations = 16;
    colourSettings.bailout = 15.5;
    ok = colourView->apply() && ok;
    ok = ok && colourView->renderImage({16, 16}).pixelColor(8, 8) == QColor("#ff0000");
    colourSettings.equation = "0.25";
    ok = colourView->apply() && ok;
    ok = ok && colourView->renderImage({16, 16}).pixelColor(8, 8) == QColor("#00ff00");
    colourSettings.colouring.convergence = true;
    colourSettings.colouring.interiorGradient = true;
    colourSettings.colouring.interiorStops = QJsonArray{
        QJsonObject{{"position", 0}, {"colour", "#0000ff"}},
        QJsonObject{{"position", 1}, {"colour", "#0000ff"}}};
    colourSettings.equation = "z[n]*0.1+0.1";
    ok = colourView->apply() && ok;
    ok = ok && colourView->renderImage({16, 16}).pixelColor(8, 8) == QColor("#0000ff");
    colourSettings.equation = "1-z[n]";
    ok = colourView->apply() && ok;
    ok = ok && colourView->renderImage({16, 16}).pixelColor(8, 8) == QColor("#00ff00");
    colourSettings.colouring.mode = 1;
    colourSettings.palette = 5;
    colourSettings.gradient = colourSettings.colouring.interiorStops;
    for (int shape = 0; shape < 3; ++shape) {
        colourSettings.colouring.trap = shape;
        ok = colourView->apply() && ok;
        ok = ok && colourView->renderImage({16, 16}).pixelColor(8, 8) == QColor("#0000ff");
    }
    auto colourRoundtrip = readSettings(settingsJson(colourSettings));
    ok = ok && settingsJson(colourRoundtrip) == settingsJson(colourSettings);
    auto invalidColour = settingsJson(colourSettings);
    auto invalidOptions = invalidColour["colouring"].toObject();
    invalidOptions["tolerance"] = -1;
    invalidColour["colouring"] = invalidOptions;
    try {
        readSettings(invalidColour);
        ok = false;
    } catch (const std::exception &) {
    }
    Settings anotherView;
    ok = ok && anotherView.negativeHistory.isEmpty();
    if (!ok)
        qWarning() << "Playground regression check failed" << error->text() << active()->lastError;
    return ok;
}
