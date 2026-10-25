#include "mainwindow.h"
#include "ui_design.h"

QString Playground::gradientPath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/gradients.json";
}

void Playground::persistGradients() {
    if (QCoreApplication::arguments().contains("--smoke-test"))
        return;
    QDir().mkpath(QFileInfo(gradientPath()).absolutePath());
    QSaveFile file(gradientPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(gradientLibrary).toJson());
        file.commit();
    }
}

void Playground::refreshPalettes() {
    QSignalBlocker block(palette);
    palette->clear();
    palette->addItems({"Spectrum", "Ember", "Ocean", "Monochrome"});
    for (auto item : gradientLibrary)
        palette->addItem(item.toObject()["name"].toString());
    for (auto *view : views)
        if (view->settings.palette == 5)
            for (auto item : gradientLibrary) {
                auto entry = item.toObject();
                if (entry["name"].toString() == view->settings.gradientName) {
                    view->settings.gradient = entry["stops"].toArray();
                    view->update();
                    break;
                }
            }
    for (auto *view : views) {
        for (auto item : gradientLibrary) {
            auto entry = item.toObject();
            if (entry["name"].toString() == view->settings.colouring.interiorGradientName) {
                view->settings.colouring.interiorStops = entry["stops"].toArray();
                view->update();
                break;
            }
        }
    }
    load();
}

void Playground::initGradients() {
    QFile file(gradientPath());
    if (file.open(QIODevice::ReadOnly) && file.size() < 8 * 1024 * 1024) {
        auto values = QJsonDocument::fromJson(file.readAll()).array();
        for (auto item : values) {
            auto o = item.toObject();
            Settings s;
            s.gradient = o["stops"].toArray();
            try {
                readSettings(settingsJson(s));
                if (!o["name"].toString().isEmpty())
                    gradientLibrary.append(o);
            } catch (const std::exception &) {
            }
        }
    }
    if (gradientLibrary.isEmpty())
        gradientLibrary.append(QJsonObject{{"name", "Default"}, {"stops", Settings().gradient}});
    refreshPalettes();
}

View *Playground::byNumber(int number) const {
    for (auto *view : views)
        if (view->property("number").toInt() == number)
            return view;
    return nullptr;
}

QJsonObject Playground::settingsJson(const Settings &s) const {
    QJsonArray names;
    for (auto name : s.names)
        names.append(name);
    QJsonArray vars;
    for (const auto &v : s.variables)
        vars.append(QJsonObject{{"source", v.source},
                                {"real", v.real},
                                {"imag", v.imag},
                                {"equation", v.equation},
                                {"modifiers", v.modifiers}});
    QJsonArray negative;
    for (auto it = s.negativeHistory.begin(); it != s.negativeHistory.end(); ++it)
        negative.append(
            QJsonObject{{"index", it.key()}, {"real", it.value().x()}, {"imag", it.value().y()}});
    const auto &c = s.colouring;
    QJsonObject colouring{{"mode", c.mode}, {"escapeSolid", c.escapeSolid},
                          {"interiorGradient", c.interiorGradient}, {"convergence", c.convergence},
                          {"escapeColour", c.escapeColour.name()}, {"interiorColour", c.interiorColour.name()},
                          {"interiorGradientName", c.interiorGradientName}, {"interiorStops", c.interiorStops},
                          {"interiorPalette", c.interiorPalette}, {"trapViewScaled", c.trapViewScaled},
                          {"customReference", c.customReference},
                          {"referenceReal", c.referenceReal}, {"referenceImag", c.referenceImag},
                          {"referenceSpan", c.referenceSpan}, {"referenceAspect", c.referenceAspect},
                          {"tolerance", c.tolerance}, {"trap", c.trap}, {"trapReal", c.trapReal},
                          {"trapImag", c.trapImag}, {"trapRadius", c.trapRadius}};
    return {{"colouring", colouring}, {"negativeHistory", negative},
            {"equation", s.equation},
            {"iterations", s.iterations},
            {"historySize", s.historySize},
            {"palette", s.palette},
            {"bailout", s.bailout},
            {"frequency", s.frequency},
            {"phase", s.phase},
            {"smoothing", s.smoothing},
            {"names", names},
            {"variables", vars},
            {"gradient", s.gradient},
            {"gradientName", s.gradientName},
            {"orbit", s.orbit},
            {"orbitSteps", s.orbitSteps}};
}

Settings Playground::readSettings(QJsonObject o) const {
    auto finite = [&](QString name, double lo, double hi) {
        auto v = o[name];
        double n = v.toDouble(NAN);
        if (!std::isfinite(n) || n < lo || n > hi)
            throw std::runtime_error(("Invalid " + name).toStdString());
        return n;
    };
    Settings s;
    if (o.contains("colouring")) {
        const auto object = o["colouring"].toObject();
        auto &c = s.colouring;
        auto numeric = [&](QString key, double minimum, double maximum) {
            double value = object[key].toDouble(NAN);
            if (!std::isfinite(value) || value < minimum || value > maximum)
                throw std::runtime_error("Invalid colouring setting");
            return value;
        };
        c.interiorPalette = object.contains("interiorPalette") ? int(numeric("interiorPalette", 0, 4)) : 4;
        c.trapViewScaled = object["trapViewScaled"].toBool(false);
        c.customReference = object["customReference"].toBool(false);
        if (object.contains("referenceSpan")) {
            c.referenceReal = numeric("referenceReal", -1e15, 1e15);
            c.referenceImag = numeric("referenceImag", -1e15, 1e15);
            c.referenceSpan = numeric("referenceSpan", 1e-15, 1e15);
            c.referenceAspect = numeric("referenceAspect", 0.1, 10);
        }
        c.mode = int(numeric("mode", 0, 1));
        c.trap = int(numeric("trap", 0, 2));
        c.tolerance = numeric("tolerance", 1e-12, 1);
        c.trapReal = numeric("trapReal", -1e9, 1e9);
        c.trapImag = numeric("trapImag", -1e9, 1e9);
        c.trapRadius = numeric("trapRadius", 0, 1e9);
        c.escapeSolid = object["escapeSolid"].toBool();
        c.interiorGradient = object["interiorGradient"].toBool();
        c.convergence = object["convergence"].toBool();
        c.escapeColour = QColor(object["escapeColour"].toString());
        c.interiorColour = QColor(object["interiorColour"].toString());
        if (!c.escapeColour.isValid() || !c.interiorColour.isValid())
            throw std::runtime_error("Invalid classification colour");
        c.interiorGradientName = object["interiorGradientName"].toString();
        c.interiorStops = object["interiorStops"].toArray();
        double previous = -1;
        for (auto stop : c.interiorStops) {
            auto entry = stop.toObject();
            double position = entry["position"].toDouble(NAN);
            if (!std::isfinite(position) || position < previous || position < 0 || position > 1 ||
                !QColor(entry["colour"].toString()).isValid())
                throw std::runtime_error("Invalid interior gradient");
            previous = position;
        }
        if (!c.interiorStops.isEmpty() && c.interiorStops.size() < 2)
            throw std::runtime_error("Interior gradient needs two stops");
    }
    if (o.contains("names")) {
        s.names.clear();
        for (auto name : o["names"].toArray())
            s.names.append(name.toString());
    }
    s.equation = o["equation"].toString();
    s.iterations = int(finite("iterations", 16, 10000));
    s.historySize = o["historySize"].toInt(1);
    if (s.historySize < 1 || s.historySize > 10000)
        throw std::runtime_error("History size must be between 1 and 10000");
    s.palette = int(finite("palette", 0, 5));
    s.bailout = finite("bailout", .001, 1e9);
    s.frequency = finite("frequency", .00001, 10);
    s.phase = finite("phase", -100, 100);
    s.smoothing = finite("smoothing", 0, 1);
    s.gradientName = o["gradientName"].toString("Default");
    s.orbit = o["orbit"].toBool();
    s.orbitSteps = int(finite("orbitSteps", 1, 1000));
    auto vars = o["variables"].toArray();
    if (vars.size() < 4 || vars.size() != s.names.size())
        throw std::runtime_error("Invalid named variables");
    s.variables.resize(vars.size());
    QSet<QString> unique;
    for (int i = 0; i < s.names.size(); ++i) {
        if (i == 3 || s.names[i].isEmpty())
            continue;
        if (!Equation::validName(s.names[i]) || unique.contains(s.names[i]))
            throw std::runtime_error("Invalid variable name");
        unique.insert(s.names[i]);
    }
    for (int i = 0; i < vars.size(); ++i) {
        auto v = vars[i].toObject();
        auto &out = s.variables[i];
        out.source = v["source"].toInt(-1);
        out.real = v["real"].toDouble(NAN);
        out.imag = v["imag"].toDouble(NAN);
        out.equation = v["equation"].toString();
        out.modifiers = v["modifiers"].toInt();
        if (out.source < 0 || out.source > (i == 3 ? 3 : 2) || !std::isfinite(out.real) ||
            !std::isfinite(out.imag) || std::abs(out.real) > 1e6 || std::abs(out.imag) > 1e6)
            throw std::runtime_error("Invalid variable");
        if (out.source == Expression) {
            QString compiled = Equation::compile(out.equation, s.names);
            if (QRegularExpression("\\b(z|n)\\b").match(compiled).hasMatch())
                throw std::runtime_error("Initial z cannot refer to z or n");
        }
    }
    for (auto item : o["negativeHistory"].toArray()) {
        auto v = item.toObject();
        double index = v["index"].toDouble(NAN), re = v["real"].toDouble(NAN),
               im = v["imag"].toDouble(NAN);
        if (!std::isfinite(index) || floor(index) != index || index >= 0 || index < -1000000 ||
            !std::isfinite(re) || !std::isfinite(im) || s.negativeHistory.contains(int(index)))
            throw std::runtime_error("Invalid negative z initial value");
        s.negativeHistory[int(index)] = QPointF(re, im);
    }
    s.gradient = o["gradient"].toArray();
    if (s.gradient.size() < 2)
        throw std::runtime_error("Gradient needs at least two stops");
    double previous = -1;
    for (auto item : s.gradient) {
        auto stop = item.toObject();
        double p = stop["position"].toDouble(-1);
        if (p < 0 || p > 1 || p <= previous || !QColor(stop["colour"].toString()).isValid())
            throw std::runtime_error("Invalid gradient stop");
        previous = p;
    }
    if (!s.names.contains("p") && o.contains("p")) {
        s.names.append("p");
        s.variables.append(Variable{Constant, o["p"].toDouble(2), 0, "0", 0});
    }
    Equation::compile(s.equation, s.names);
    return s;
}

QJsonObject Playground::layoutJson(QWidget *widget) const {
    if (auto *view = dynamic_cast<View *>(widget))
        return {{"view", view->property("number").toInt()}};
    auto *splitter = qobject_cast<QSplitter *>(widget);
    QJsonArray children, sizes;
    for (int i = 0; i < splitter->count(); ++i)
        children.append(layoutJson(splitter->widget(i)));
    for (int size : splitter->sizes())
        sizes.append(size);
    return {
        {"orientation", int(splitter->orientation())}, {"children", children}, {"sizes", sizes}};
}

QList<NumericSlider *> Playground::numericSliders() const {
    QList<NumericSlider *> result;
    for (auto *widget : findChildren<QWidget *>())
        if (auto *slider = dynamic_cast<NumericSlider *>(widget))
            result << slider;
    return result;
}

QJsonObject Playground::snapshot() const {
    QJsonArray list, links, custom;
    for (auto *view : views)
        list.append(QJsonObject{{"number", view->property("number").toInt()},
                                {"settings", settingsJson(view->settings)},
                                {"x", QString::number(double(view->cx), 'g', 17)},
                                {"y", QString::number(double(view->cy), 'g', 17)},
                                {"span", QString::number(double(view->span), 'g', 17)}});
    for (auto r : routes)
        custom.append(QJsonArray{
            r.source, r.target, r.variable, r.modifiers,
            r.variable == 3 ? QString() : byNumber(r.target)->settings.names.value(r.variable)});
    QJsonArray sliders;
    for (auto *slider : numericSliders())
        sliders.append(slider->state());
    return {{"variableDockVisible", variableDock->isVisible()},
            {"variableDockFloating", variableDock->isFloating()},
            {"variableDockArea", int(dockWidgetArea(variableDock))},
            {"variableDockGeometry", QString::fromLatin1(variableDock->saveGeometry().toBase64())},
            {"gradientLibrary", gradientLibrary},
            {"sliders", sliders},
            {"version", 1},
            {"views", list},
            {"layout", layoutJson(root)},
            {"links", links},
            {"routes", custom},
            {"tracks", tracks},
            {"fps", animationFps},
            {"duration", animationDuration},
            {"loop", animationLoop},
            {"selected", active()->property("number").toInt()},
            {"sidebar", dock->isVisible()}};
}

bool Playground::restore(QJsonObject state) {
    try {
        if (state["version"].toInt() != 1)
            throw std::runtime_error("Unsupported workspace version");
        auto list = state["views"].toArray();
        if (list.isEmpty() || list.size() > 256)
            throw std::runtime_error("Workspace must contain 1–256 views");
        QHash<int, Settings> settings;
        QHash<int, QVector<double>> cameras;
        for (auto item : list) {
            auto o = item.toObject();
            int id = o["number"].toInt();
            if (id < 1 || settings.contains(id))
                throw std::runtime_error("Duplicate or invalid view number");
            settings[id] = readSettings(o["settings"].toObject());
            QVector<double> camera;
            for (auto name : {"x", "y", "span"}) {
                bool ok;
                double n = o[name].toString().toDouble(&ok);
                if (!ok || !std::isfinite(n) || (QString(name) == "span" && n <= 0))
                    throw std::runtime_error("Invalid camera");
                camera << n;
            }
            cameras[id] = camera;
        }
        QSet<int> used;
        std::function<void(QJsonObject, int)> validate = [&](QJsonObject o, int depth) {
            if (depth > 64)
                throw std::runtime_error("Layout is too deeply nested");
            if (o.contains("view")) {
                int id = o["view"].toInt();
                if (!settings.contains(id) || used.contains(id))
                    throw std::runtime_error("Invalid layout view");
                used.insert(id);
                return;
            }
            int orientation = o["orientation"].toInt();
            auto children = o["children"].toArray();
            if ((orientation != 1 && orientation != 2) || children.isEmpty())
                throw std::runtime_error("Invalid split layout");
            for (auto child : children)
                validate(child.toObject(), depth + 1);
        };
        validate(state["layout"].toObject(), 0);
        if (used.size() != settings.size())
            throw std::runtime_error("Missing layout view");
        for (auto item : state["links"].toArray()) {
            auto pair = item.toArray();
            if (pair.size() != 2 || !settings.contains(pair[0].toInt()) ||
                !settings.contains(pair[1].toInt()))
                throw std::runtime_error("Invalid Julia link");
        }
        QList<PickRoute> newRoutes;
        for (auto item : state["routes"].toArray()) {
            auto r = item.toArray();
            if (r.size() == 5 && !r[4].toString().isEmpty() && settings.contains(r[1].toInt()))
                r[2] = settings[r[1].toInt()].names.indexOf(r[4].toString());
            if ((r.size() != 4 && r.size() != 5) || !settings.contains(r[0].toInt()) ||
                !settings.contains(r[1].toInt()) || r[2].toInt() < 0 ||
                r[2].toInt() >= settings[r[1].toInt()].variables.size())
                throw std::runtime_error("Invalid picking route");
            newRoutes << PickRoute{r[0].toInt(), r[1].toInt(), r[2].toInt(), r[3].toInt()};
        }
        validateTracks(state["tracks"].toArray(), settings.keys());
        double duration = state["duration"].toDouble();
        if (!std::isfinite(duration) || duration < .1 || duration > 3600)
            throw std::runtime_error("Invalid animation duration");
        for (auto item : state["gradientLibrary"].toArray()) {
            auto entry = item.toObject();
            Settings candidate;
            candidate.gradient = entry["stops"].toArray();
            readSettings(settingsJson(candidate));
        }
        animationTimer.stop();
        leaveFullscreen();
        restoring = true;
        for (auto item : state["gradientLibrary"].toArray()) {
            auto entry = item.toObject();
            bool exists = false;
            for (auto existing : gradientLibrary)
                if (existing.toObject()["name"] == entry["name"]) {
                    exists = true;
                    break;
                }
            if (!exists)
                gradientLibrary.append(entry);
        }
        for (auto *view : views) {
            view->activated = {};
            view->mousePicked = {};
            view->status = {};
        }
        views.clear();
        auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
        layout->removeWidget(root);
        delete root;
        std::function<QWidget *(QJsonObject)> build = [&](QJsonObject o) -> QWidget * {
            if (o.contains("view")) {
                int id = o["view"].toInt();
                auto *view = new View;
                view->setProperty("number", id);
                view->settings = settings[id];
                view->cx = cameras[id][0];
                view->cy = cameras[id][1];
                view->span = cameras[id][2];
                view->apply();
                views << view;
                bindView(view);
                nextNumber = qMax(nextNumber, id + 1);
                return view;
            }
            auto *splitter = new QSplitter(Qt::Orientation(o["orientation"].toInt()));
            splitter->setHandleWidth(1);
            splitter->setChildrenCollapsible(false);
            for (auto child : o["children"].toArray())
                splitter->addWidget(build(child.toObject()));
            QList<int> sizes;
            for (auto size : o["sizes"].toArray())
                sizes << qMax(1, size.toInt());
            splitter->setSizes(sizes);
            return splitter;
        };
        QWidget *tree = build(state["layout"].toObject());
        root = qobject_cast<QSplitter *>(tree);
        if (!root) {
            root = new QSplitter;
            root->setHandleWidth(1);
            root->addWidget(tree);
        }
        layout->addWidget(root);
        root->show();
        left = views.first();
        right = views.value(1, left);
        nextNumber = views.size() + 1;
        routes = newRoutes;
        tracks = state["tracks"].toArray();
        animationDuration = duration;
        animationFps = qBound(1, state["fps"].toInt(60), 240);
        animationLoop = state["loop"].toBool();
        dock->setVisible(state["sidebar"].toBool());
        refreshViews(byNumber(state["selected"].toInt()));
        auto sliders = numericSliders();
        auto savedSliders = state["sliders"].toArray();
        for (int i = 0; i < qMin(sliders.size(), savedSliders.size()); ++i)
            sliders[i]->restoreState(savedSliders[i].toObject());
        refreshPalettes();
        int area = state["variableDockArea"].toInt(Qt::LeftDockWidgetArea);
        if (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea ||
            area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea)
            addDockWidget(Qt::DockWidgetArea(area), variableDock);
        variableDock->setFloating(state["variableDockFloating"].toBool());
        variableDock->restoreGeometry(
            QByteArray::fromBase64(state["variableDockGeometry"].toString().toLatin1()));
        variableDock->setVisible(state["variableDockVisible"].toBool());
        restoring = false;
        return true;
    } catch (const std::exception &e) {
        restoring = false;
        error->setText(e.what());
        return false;
    }
}

void Playground::checkpoint() {
    if (restoring || animationTimer.isActive() || fullscreenMode)
        return;
    auto state = snapshot();
    if (historyIndex >= 0 && history[historyIndex] == state)
        return;
    while (history.size() > historyIndex + 1)
        history.removeLast();
    history << state;
    if (history.size() > 100)
        history.removeFirst();
    historyIndex = history.size() - 1;
}

void Playground::travel(int direction) {
    animationTimer.stop();
    checkpoint();
    int next = historyIndex + direction;
    if (next < 0 || next >= history.size())
        return;
    if (restore(history[next]))
        historyIndex = next;
}

void Playground::workspaceFile(bool save) {
    QString path = save ? QFileDialog::getSaveFileName(this, "Save workspace", {},
                                                       "Fractal workspace (*.json)")
                        : QFileDialog::getOpenFileName(this, "Load workspace", {},
                                                       "Fractal workspace (*.json)");
    if (path.isEmpty())
        return;
    if (save) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(QJsonDocument(snapshot()).toJson()) < 0 || !file.commit())
            QMessageBox::warning(this, "Save failed", file.errorString());
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
            QMessageBox::warning(this, "Load failed", "Cannot read workspace (maximum 8 MB).");
            return;
        }
        QJsonParseError parse;
        auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        checkpoint();
        if (parse.error != QJsonParseError::NoError || !restore(document.object()))
            QMessageBox::warning(this, "Load failed",
                                 parse.error != QJsonParseError::NoError ? parse.errorString()
                                                                         : error->text());
        else
            checkpoint();
    }
}

void Playground::exportDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Export active view");
    QFormLayout form(&dialog);
    QSpinBox width, height;
    for (auto *v : {&width, &height})
        v->setRange(16, 8192);
    width.setValue(1920);
    height.setValue(1080);
    form.addRow("Width", &width);
    form.addRow("Height", &height);
    QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString path = QFileDialog::getSaveFileName(this, "Export PNG", {}, "PNG image (*.png)");
    if (path.isEmpty())
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QImage image = active()->renderImage({width.value(), height.value()});
    QApplication::restoreOverrideCursor();
    if (image.isNull() || !image.save(path, "PNG"))
        QMessageBox::warning(this, "Export failed",
                             "Unable to render or save image at that resolution.");
}

void Playground::gradientDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Global gradient library");
    QVBoxLayout layout(&dialog);
    QComboBox library;
    layout.addWidget(&library);
    GradientBar bar;
    layout.addWidget(&bar);
    QLabel help("Click the bar to add a stop. Drag a triangle to move it. Double-click a stop to "
                "change its colour; right-click to delete it (at least two stops remain). A flat "
                "white region is made with two white stops.");
    help.setWordWrap(true);
    bar.setToolTip(help.text());
    QDoubleSpinBox position;
    position.setRange(0, 1);
    position.setDecimals(6);
    position.setSingleStep(.01);
    position.setKeyboardTracking(false);
    layout.addWidget(new QLabel("Selected stop position (0–1)"));
    layout.addWidget(&position);
    auto syncPosition = [&] {
        QSignalBlocker block(position);
        double value = bar.selectedPosition();
        position.setEnabled(value >= 0);
        if (value >= 0)
            position.setValue(value);
    };
    bar.changed = syncPosition;
    connect(&position, &QDoubleSpinBox::valueChanged, &dialog, [&](double value) {
        if (!bar.setSelectedPosition(value)) {
            syncPosition();
            position.setToolTip("Another stop already occupies that position.");
        } else
            position.setToolTip("");
    });
    QPushButton colour("Edit selected colour"), remove("Remove selected stop"),
        create("New gradient"), rename("Rename"), erase("Delete gradient"),
        reset("Reset to monochrome");
    for (auto *button : {&colour, &remove, &reset, &create, &rename, &erase})
        layout.addWidget(button);
    auto refresh = [&](int index) {
        QSignalBlocker block(library);
        library.clear();
        for (auto item : gradientLibrary)
            library.addItem(item.toObject()["name"].toString());
        library.setCurrentIndex(qBound(0, index, int(gradientLibrary.size()) - 1));
        bar.setStops(gradientLibrary[library.currentIndex()].toObject()["stops"].toArray());
    };
    connect(&library, &QComboBox::currentIndexChanged, &dialog, [&](int i) {
        if (i >= 0) {
            bar.setStops(gradientLibrary[i].toObject()["stops"].toArray());
        }
    });
    refresh(0);
    connect(&reset, &QPushButton::clicked, &bar, &GradientBar::resetMonochrome);
    connect(&colour, &QPushButton::clicked, &bar, &GradientBar::editColour);
    connect(&remove, &QPushButton::clicked, &bar, &GradientBar::removeSelected);
    connect(&create, &QPushButton::clicked, &dialog, [&] {
        bool ok;
        QString name = QInputDialog::getText(&dialog, "New gradient", "Name", QLineEdit::Normal,
                                             "Gradient", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            gradientLibrary.append(QJsonObject{{"name", name.trimmed()}, {"stops", bar.stops}});
            refresh(gradientLibrary.size() - 1);
        }
    });
    connect(&rename, &QPushButton::clicked, &dialog, [&] {
        bool ok;
        QString name = QInputDialog::getText(&dialog, "Rename gradient", "Name", QLineEdit::Normal,
                                             library.currentText(), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            int i = library.currentIndex();
            auto item = gradientLibrary[i].toObject();
            item["name"] = name.trimmed();
            gradientLibrary[i] = item;
            refresh(i);
        }
    });
    connect(&erase, &QPushButton::clicked, &dialog, [&] {
        if (gradientLibrary.size() > 1) {
            gradientLibrary.removeAt(library.currentIndex());
            refresh(0);
        }
    });
    QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Apply |
                             QDialogButtonBox::Close);
    layout.addWidget(&buttons);
    auto save = [&] {
        auto item = gradientLibrary[library.currentIndex()].toObject();
        item["stops"] = bar.stops;
        gradientLibrary[library.currentIndex()] = item;
        persistGradients();
        refreshPalettes();
    };
    connect(buttons.button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog, save);
    connect(buttons.button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&] {
        save();
        active()->settings.palette = 5;
        active()->settings.gradientName = library.currentText();
        active()->settings.gradient = bar.stops;
        active()->update();
        load();
        checkpoint();
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
    persistGradients();
    refreshPalettes();
}

void Playground::negativeHistoryDialog() {
    View *view = active();
    QDialog dialog(this);
    dialog.setWindowTitle(viewName(view) + " — Initial values before z[0]");
    QVBoxLayout layout(&dialog);
    QLabel help("z[0] uses the Initial z settings. Negative indices default to 0 + 0i; add "
                "overrides here for recurrences using z[n-1], z[n-2], etc.");
    dialog.setToolTip(help.text());
    QTableWidget table(0, 3);
    table.setHorizontalHeaderLabels({"Negative index", "Real", "Imaginary"});
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout.addWidget(&table);
    auto addRow = [&](int index, QPointF value) {
        int row = table.rowCount();
        table.insertRow(row);
        table.setItem(row, 0, new QTableWidgetItem(QString::number(index)));
        table.setItem(row, 1, new QTableWidgetItem(QString::number(value.x(), 'g', 17)));
        table.setItem(row, 2, new QTableWidgetItem(QString::number(value.y(), 'g', 17)));
    };
    for (auto it = view->settings.negativeHistory.begin();
         it != view->settings.negativeHistory.end(); ++it)
        addRow(it.key(), it.value());
    QPushButton add("Add value"), remove("Remove selected override");
    layout.addWidget(&add);
    layout.addWidget(&remove);
    connect(&add, &QPushButton::clicked, &dialog, [&] {
        int index = -1;
        for (int row = 0; row < table.rowCount(); ++row)
            if (table.item(row, 0))
                index = qMin(index, table.item(row, 0)->text().toInt() - 1);
        addRow(index, {});
    });
    connect(&remove, &QPushButton::clicked, &dialog, [&] {
        if (table.currentRow() >= 0)
            table.removeRow(table.currentRow());
    });
    QLabel message;
    layout.addWidget(&message);
    QDialogButtonBox buttons(QDialogButtonBox::Apply | QDialogButtonBox::Close);
    layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons.button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&] {
        QMap<int, QPointF> values;
        for (int row = 0; row < table.rowCount(); ++row) {
            bool valid[3] = {false, false, false};
            int index = table.item(row, 0) ? table.item(row, 0)->text().toInt(&valid[0]) : 0;
            double re = table.item(row, 1) ? table.item(row, 1)->text().toDouble(&valid[1]) : 0,
                   im = table.item(row, 2) ? table.item(row, 2)->text().toDouble(&valid[2]) : 0;
            if (!valid[0] || !valid[1] || !valid[2] || index >= 0 || index < -1000000 ||
                !std::isfinite(re) || !std::isfinite(im) || values.contains(index)) {
                message.setText("Use unique integer indices from -1000000 to -1 and finite complex "
                                "components.");
                return;
            }
            values[index] = QPointF(re, im);
        }
        auto previous = view->settings.negativeHistory;
        view->settings.negativeHistory = values;
        if (!view->apply()) {
            view->settings.negativeHistory = previous;
            message.setText(view->lastError);
            return;
        }
        checkpoint();
        message.setText("Applied. Unspecified negative values remain zero.");
    });
    dialog.exec();
}

void Playground::orbitDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Orbit visualization — active view");
    QFormLayout form(&dialog);
    QCheckBox enabled("Draw orbit of the clicked point");
    enabled.setChecked(active()->settings.orbit);
    QSpinBox steps;
    steps.setRange(1, 1000);
    steps.setValue(active()->settings.orbitSteps);
    form.addRow(&enabled);
    form.addRow("Maximum steps", &steps);
    QLabel note("Click a point in the view to trace successive z values. The yellow path uses the "
                "current equation and variable sources.");
    dialog.setToolTip(note.text());
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        active()->settings.orbit = enabled.isChecked();
        active()->settings.orbitSteps = steps.value();
        active()->update();
        checkpoint();
    }
}

void Playground::linksDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Generalized picking links");
    QVBoxLayout layout(&dialog);
    QComboBox source, destination, variable, modifier;
    for (auto *view : views) {
        source.addItem(viewName(view), view->property("number"));
        destination.addItem(viewName(view), view->property("number"));
    }
    auto updateVariables = [&] {
        variable.clear();
        auto *v = byNumber(destination.currentData().toInt());
        if (!v)
            return;
        for (int i = 0; i < v->settings.names.size(); ++i)
            if (i == 3 || !v->settings.names[i].isEmpty())
                variable.addItem(i == 3 ? "Initial z" : v->settings.names[i], i);
    };
    updateVariables();
    connect(&destination, &QComboBox::currentIndexChanged, &dialog, updateVariables);
    for (int mask = 0; mask < 8; ++mask) {
        QStringList names;
        int flags = 0;
        if (mask & 1) {
            names << "Shift";
            flags |= Qt::ShiftModifier;
        }
        if (mask & 2) {
            names << "Ctrl";
            flags |= Qt::ControlModifier;
        }
        if (mask & 4) {
            names << "Alt";
            flags |= Qt::AltModifier;
        }
        modifier.addItem(names.isEmpty() ? "Right button" : names.join(" + ") + " + Right button",
                         flags);
    }
    QFormLayout form;
    form.addRow("Pick in", &source);
    form.addRow("Update view", &destination);
    form.addRow("Variable", &variable);
    form.addRow("Gesture", &modifier);
    layout.addLayout(&form);
    QListWidget list;
    layout.addWidget(&list);
    auto refresh = [&] {
        list.clear();
        for (auto r : routes)
            list.addItem(
                QString("View %1 → View %2, %3 (modifiers %4)")
                    .arg(r.source)
                    .arg(r.target)
                    .arg((r.variable == 3 ? QString("Initial z")
                                          : byNumber(r.target)->settings.names.value(r.variable)))
                    .arg(r.modifiers));
    };
    refresh();
    QPushButton add("Add link"), remove("Remove selected link");
    layout.addWidget(&add);
    layout.addWidget(&remove);
    connect(&add, &QPushButton::clicked, &dialog, [&] {
        routes << PickRoute{source.currentData().toInt(), destination.currentData().toInt(),
                            variable.currentData().toInt(), modifier.currentData().toInt()};
        refresh();
        checkpoint();
    });
    connect(&remove, &QPushButton::clicked, &dialog, [&] {
        if (list.currentRow() >= 0) {
            routes.removeAt(list.currentRow());
            refresh();
            checkpoint();
        }
    });
    QDialogButtonBox close(QDialogButtonBox::Close);
    layout.addWidget(&close);
    connect(&close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

QStringList Playground::parameterNames() const {
    QStringList result = {
        "a real",          "a imaginary",      "b real",         "b imaginary",
        "c real",          "c imaginary",      "Initial z real", "Initial z imaginary",
        "p real (legacy)", "Escape radius",    "Iterations",     "Colour frequency",
        "Colour phase",    "Colour smoothing", "Centre real",    "Centre imaginary",
        "Zoom span",       "Orbit steps"};
    for (auto *v : views)
        for (auto name : v->settings.names)
            if (!name.isEmpty()) {
                for (auto suffix : {QString(" real"), QString(" imaginary")})
                    if (!result.contains(name + suffix))
                        result.append(name + suffix);
            }
    return result;
}

void Playground::validateTracks(QJsonArray values, QList<int> ids) const {
    if (values.size() > 256)
        throw std::runtime_error("Too many keyframes");
    for (auto value : values) {
        auto o = value.toObject();
        double time = o["time"].toDouble(NAN), number = o["value"].toDouble(NAN);
        int parameter = o["parameter"].toInt(-1);
        if (!ids.contains(o["view"].toInt()) || parameter < 0 || parameter > 1024 ||
            !std::isfinite(time) || time < 0 || time > 3600 || !std::isfinite(number))
            throw std::runtime_error("Invalid animation keyframe");
    }
}

void Playground::setParameter(View *view, int parameter, double value) {
    auto &s = view->settings;
    if (parameter >= 18) {
        QString label = parameterNames().value(parameter);
        bool imaginary = label.endsWith(" imaginary");
        QString name = label.left(label.size() - (imaginary ? 10 : 5));
        int i = s.names.indexOf(name);
        if (i >= 0) {
            auto &v = s.variables[i];
            v.source = Constant;
            if (imaginary)
                v.imag = qBound(-1e6, value, 1e6);
            else
                v.real = qBound(-1e6, value, 1e6);
        }
        return;
    }
    if (parameter < 8) {
        auto &v = s.variables[parameter / 2];
        v.source = Constant;
        if (parameter % 2)
            v.imag = qBound(-1e6, value, 1e6);
        else
            v.real = qBound(-1e6, value, 1e6);
    } else
        switch (parameter) {
        case 8: {
            int i = s.names.indexOf("p");
            if (i >= 0)
                s.variables[i].real = qBound(-1e6, value, 1e6);
            break;
        }
        case 9:
            s.bailout = qBound(.001, value, 1e9);
            break;
        case 10:
            s.iterations = qRound(qBound(16., value, 10000.));
            break;
        case 11:
            s.frequency = qBound(.00001, value, 10.);
            break;
        case 12:
            s.phase = qBound(-100., value, 100.);
            break;
        case 13:
            s.smoothing = qBound(0., value, 1.);
            break;
        case 14:
            view->cx = value;
            break;
        case 15:
            view->cy = value;
            break;
        case 16:
            view->span = qMax(1e-300, value);
            break;
        case 17:
            s.orbitSteps = int(qBound(1., value, 1000.));
            break;
        }
}

void Playground::animateAt(double time) {
    QSet<View *> changed;
    QSet<QString> groups;
    for (auto item : tracks) {
        auto o = item.toObject();
        groups.insert(QString("%1:%2").arg(o["view"].toInt()).arg(o["parameter"].toInt()));
    }
    for (auto group : groups) {
        auto parts = group.split(':');
        View *view = byNumber(parts[0].toInt());
        if (!view)
            continue;
        int parameter = parts[1].toInt();
        QList<QPair<double, double>> keys;
        for (auto item : tracks) {
            auto o = item.toObject();
            if (o["view"].toInt() == parts[0].toInt() && o["parameter"].toInt() == parameter)
                keys << qMakePair(o["time"].toDouble(), o["value"].toDouble());
        }
        std::sort(keys.begin(), keys.end());
        double value = keys.first().second;
        if (time >= keys.last().first)
            value = keys.last().second;
        else
            for (int i = 1; i < keys.size(); ++i)
                if (time <= keys[i].first) {
                    double t = qBound(0.,
                                      (time - keys[i - 1].first) /
                                          qMax(1e-9, keys[i].first - keys[i - 1].first),
                                      1.);
                    value = keys[i - 1].second * (1 - t) + keys[i].second * t;
                    break;
                }
        QString named;
        for (auto item : tracks) {
            auto o = item.toObject();
            if (o["view"].toInt() == parts[0].toInt() && o["parameter"].toInt() == parameter) {
                named = o["parameterName"].toString();
                break;
            }
        }
        if (!named.isEmpty())
            parameter = parameterNames().indexOf(named);
        if (parameter >= 0)
            setParameter(view, parameter, value);
        changed.insert(view);
    }
    for (auto *view : changed)
        view->apply();
    if (!animationTimer.isActive() || animationClock.elapsed() - lastAnimationUi >= 200) {
        load();
        lastAnimationUi = animationClock.elapsed();
    }
}

void Playground::timelineDialog() {
    animationTimer.stop();
    QDialog dialog(this);
    dialog.setWindowTitle("Timeline animation");
    dialog.resize(650, 450);
    QVBoxLayout layout(&dialog);
    QTableWidget table(0, 4);
    table.setHorizontalHeaderLabels({"View number", "Parameter", "Time (seconds)", "Value"});
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto addRow = [&](QJsonObject o) {
        int row = table.rowCount();
        table.insertRow(row);
        auto *view = new QComboBox;
        for (auto *v : views)
            view->addItem(viewName(v), v->property("number"));
        view->setCurrentIndex(
            qMax(0, view->findData(o["view"].toInt(active()->property("number").toInt()))));
        table.setCellWidget(row, 0, view);
        auto *parameter = new QComboBox;
        parameter->addItems(parameterNames());
        parameter->setCurrentIndex(o.contains("parameterName")
                                       ? parameterNames().indexOf(o["parameterName"].toString())
                                       : o["parameter"].toInt(12));
        table.setCellWidget(row, 1, parameter);
        table.setItem(row, 2, new QTableWidgetItem(QString::number(o["time"].toDouble())));
        table.setItem(row, 3,
                      new QTableWidgetItem(QString::number(o["value"].toDouble(), 'g', 17)));
    };
    for (auto value : tracks)
        addRow(value.toObject());
    layout.addWidget(&table);
    QPushButton add("Add keyframe"), remove("Remove keyframe");
    layout.addWidget(&add);
    layout.addWidget(&remove);
    connect(&add, &QPushButton::clicked, &dialog, [&] { addRow({}); });
    connect(&remove, &QPushButton::clicked, &dialog, [&] {
        if (table.currentRow() >= 0)
            table.removeRow(table.currentRow());
    });
    QDoubleSpinBox duration;
    duration.setRange(.1, 3600);
    duration.setValue(animationDuration);
    QSpinBox fps;
    fps.setRange(1, 240);
    fps.setValue(animationFps);
    fps.setSuffix(" FPS");
    fps.setToolTip("Target playback rate; actual rate depends on GPU workload.");
    layout.addWidget(&fps);
    QCheckBox loop("Loop");
    loop.setChecked(animationLoop);
    layout.addWidget(new QLabel("Duration (seconds)"));
    layout.addWidget(&duration);
    layout.addWidget(&loop);
    QLabel message;
    layout.addWidget(&message);
    QDialogButtonBox buttons(QDialogButtonBox::Save | QDialogButtonBox::Close);
    auto *play = buttons.addButton("Play", QDialogButtonBox::ActionRole);
    layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto save = [&]() -> bool {
        QJsonArray result;
        for (int row = 0; row < table.rowCount(); ++row) {
            bool okTime = false, okValue = false;
            double time = table.item(row, 2) ? table.item(row, 2)->text().toDouble(&okTime) : 0,
                   value = table.item(row, 3) ? table.item(row, 3)->text().toDouble(&okValue) : 0;
            if (!okTime || !okValue) {
                message.setText("Enter valid keyframe times and values.");
                return false;
            }
            result.append(QJsonObject{
                {"view",
                 qobject_cast<QComboBox *>(table.cellWidget(row, 0))->currentData().toInt()},
                {"parameter", qobject_cast<QComboBox *>(table.cellWidget(row, 1))->currentIndex()},
                {"parameterName",
                 qobject_cast<QComboBox *>(table.cellWidget(row, 1))->currentText()},
                {"time", time},
                {"value", value}});
        }
        QList<int> ids;
        for (auto *v : views)
            ids << v->property("number").toInt();
        try {
            validateTracks(result, ids);
        } catch (const std::exception &e) {
            message.setText(e.what());
            return false;
        }
        tracks = result;
        animationDuration = duration.value();
        animationFps = fps.value();
        animationLoop = loop.isChecked();
        checkpoint();
        message.setText("Saved.");
        return true;
    };
    connect(buttons.button(QDialogButtonBox::Save), &QPushButton::clicked, &dialog,
            [&] { save(); });
    connect(play, &QPushButton::clicked, &dialog, [&] {
        if (save() && !tracks.isEmpty()) {
            lastAnimationUi = -200;
            animationClock.restart();
            animationTimer.setTimerType(Qt::PreciseTimer);
            animationTimer.start(qMax(1, qRound(1000.0 / animationFps)));
            dialog.accept();
        }
    });
    dialog.exec();
}

void Playground::setupTools() {
    auto *file = menuBar()->addMenu("&File");
    file->addAction(
        studioIcon("open"), "Open workspace…", this,
        [this] { workspaceFile(false); })->setShortcut(QKeySequence::Open);
    file->addAction(
        studioIcon("save"), "Save workspace…", this,
        [this] { workspaceFile(true); })->setShortcut(QKeySequence::Save);
    file->addSeparator();
    file->addAction("Export PNG…", this, [this] { exportDialog(); });
    file->addSeparator();
    file->addAction("Quit", this, &QWidget::close)->setShortcut(QKeySequence::Quit);
    auto *edit = menuBar()->addMenu("&Edit");
    edit->addAction("Undo", this, [this] { travel(-1); })->setShortcut(QKeySequence::Undo);
    edit->addAction("Redo", this, [this] { travel(1); })->setShortcut(QKeySequence::Redo);
    edit->addSeparator();
    edit->addAction("Named variables…", this, [this] { manageVariables(); });
    edit->addAction("Copy / Julia counterpart…", this, [this] { viewOperations(); });
    auto *view = menuBar()->addMenu("&View");
    view->addAction(dock->toggleViewAction());
    view->addAction(variableDock->toggleViewAction());
    view->addAction(toolbar->toggleViewAction());
    view->addSeparator();
    view->addAction("Reset camera", this, [this] { active()->reset(); });
    view->addAction("Balance dividers", this, [this] { balance(); });
    view->addSeparator();
    view->addAction("Fullscreen active view", this, [this] { enterFullscreen(true); });
    view->addAction("Fullscreen layout", this, [this] { enterFullscreen(false); });
    auto *create = menuBar()->addMenu("&Create");
    create->addAction(studioIcon("preset"), "Start from preset…", this, &Playground::presetGallery);
    create->addAction("Split view side by side", this, [this] { splitView(Qt::Horizontal); });
    create->addAction("Split view stacked", this, [this] { splitView(Qt::Vertical); });
    create->addSeparator();
    create->addAction("Close active view", this, [this] { removeView(); });
    auto *tools = menuBar()->addMenu("&Tools");
    auto *animation = menuBar()->addMenu("&Animation");
    animation->addAction("Timeline…", this, [this] { timelineDialog(); });
    animation->addAction("Stop animation", this, [this] {
        animationTimer.stop();
        checkpoint();
    });
    tools->addAction("Negative history seeds…", this, [this] { negativeHistoryDialog(); });
    tools->addAction("Orbit visualization…", this, [this] { orbitDialog(); });
    tools->addAction("Gradient library…", this, [this] { gradientDialog(); });
    tools->addAction("Picking links…", this, [this] { linksDialog(); });
    auto *help = menuBar()->addMenu("&Help");
    help->addAction(
        "Fractal Studio guide", this,
        [this] {
            QDialog dialog(this);
            dialog.setWindowTitle("Fractal Studio — Guide");
            dialog.resize(720, 580);
            QVBoxLayout layout(&dialog);
            QTextBrowser browser;
            browser.setHtml(
                R"(<h1>Fractal Studio</h1><h2>Explore</h2><p>Drag with the left button to pan. Scroll to zoom at the cursor. Home resets the camera; F11 focuses the active view, Ctrl+F11 the layout, and Escape exits fullscreen. Tab toggles the sidebar. View contains panel and layout controls.</p><h2>Equations and variables</h2><p>Presets are editable equations. Use z[n] for the current iterate; n starts at zero. History size is per view under Parameters, defaults to 1, and is never inferred from the equation. A size of 3 retains z[n], z[n-1] and z[n-2]. Larger sizes use more GPU resources. Negative indices default to zero; Tools → Negative history seeds provides overrides. Future, fractional and expired indices render magenta. Bare z and z_n are invalid.</p><p>Variables are ordinary named complex values, including p (initially 2) and c. Add or delete them in the variable editor. Each supports Constant, Mouse picked and Pixel coordinate sources. Initial z is separate and also supports Equation. Right-drag picks all variables matching the chosen modifiers. Picking is local unless an explicit link is configured; links preserve sources except Pixel becomes Constant.</p><p>Operators: + − * / % ^. Functions: sin, cos, exp, log, conj, abs, norm, floor, ceil, real, imag. real(w) returns its real component and imag(w) its imaginary component, each as a real-valued complex number. Imaginary constants may be written as 2i or -0.5i. abs is component-wise; norm is magnitude. Modulo requires a nonzero real divisor. Built-ins: pixel, i, pi, n. Example: z[n]^p+c.</p><h2>Colour and tools</h2><p>Presets and Julia conversion preserve target colours. The global gradient editor supports click to add, drag to move, double-click to recolour and right-click to delete stops. Two matching stops make a flat colour region. Use the ↔ button beside numbers to configure sliders.</p><p>File saves the workspace or exports PNG. Tools provides orbit tracing, picking links, animation and negative seeds. Workspaces preserve named variables and dock layout.</p><h2>Precision</h2><p>There is no artificial zoom cap. Double precision eventually limits coordinate detail; transcendental functions use float precision. Continuous colouring uses a quadratic reference, so unusual formulas may need reduced smoothing or different escape radii.</p>)");
            layout.addWidget(&browser);
            QDialogButtonBox buttons(QDialogButtonBox::Close);
            layout.addWidget(&buttons);
            connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            dialog.exec();
        })->setShortcut(QKeySequence::HelpContents);
    help->addAction("About Fractal Studio", this, [this] {
        QMessageBox::about(this, "Fractal Studio",
                           "A portable OpenGL equation playground built with Qt 6.");
    });
    connect(&animationTimer, &QTimer::timeout, this, [this] {
        double time = animationClock.elapsed() / 1000.;
        if (time > animationDuration) {
            if (animationLoop) {
                animationClock.restart();
                time = 0;
            } else {
                time = animationDuration;
                animationTimer.stop();
            }
        }
        animateAt(time);
    });
    connect(&historyTimer, &QTimer::timeout, this, [this] {
        if (!QApplication::mouseButtons() && !QApplication::activeModalWidget())
            checkpoint();
    });
    historyTimer.start(400);
    QTimer::singleShot(100, this, [this] { checkpoint(); });
}
