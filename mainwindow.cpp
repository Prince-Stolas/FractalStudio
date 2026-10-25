#include "mainwindow.h"
#include "ui_design.h"

void Playground::viewOperations() {
    QDialog dialog(this);
    dialog.setWindowTitle("Copy settings / Julia counterpart");
    dialog.setMinimumWidth(390);
    QFormLayout form(&dialog);
    QComboBox source, destination, operation;
    for (auto *view : views) {
        source.addItem(viewName(view));
        destination.addItem(viewName(view));
    }
    destination.setCurrentIndex(views.indexOf(active()));
    source.setCurrentIndex(active() == views.first() && views.size() > 1 ? 1 : 0);
    operation.addItems({"Copy all settings", "Create Julia counterpart"});
    form.addRow("Source view", &source);
    form.addRow("Target view", &destination);
    form.addRow("Operation", &operation);
    operation.setToolTip("Julia conversion preserves target colours and local picking.");
    QLabel message;
    message.setWordWrap(true);
    form.addRow(&message);
    QDialogButtonBox buttons(QDialogButtonBox::Apply | QDialogButtonBox::Close);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons.button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&] {
        View *from = views[source.currentIndex()], *to = views[destination.currentIndex()];
        if (from == to) {
            message.setText("Choose different source and target views.");
            return;
        }
        bool ok = operation.currentIndex() == 0 ? copySettings(from, to) : makeJulia(from, to);
        message.setText(ok ? "Applied successfully." : error->text());
    });
    dialog.exec();
}

QString Playground::viewName(View *view) const {
    return QString("View %1").arg(view->property("number").toInt());
}

void Playground::refreshViews(View *selected) {
    {
        QSignalBlocker block(target);
        target->clear();
        for (auto *view : views)
            target->addItem(viewName(view));
        target->setCurrentIndex(qMax(0, int(views.indexOf(selected))));
    }
    load();
}

void Playground::removeView() {
    if (views.size() == 1)
        return;
    leaveFullscreen();
    View *view = active();
    auto *parent = qobject_cast<QSplitter *>(view->parentWidget());
    int removedNumber = view->property("number").toInt();
    routes.erase(std::remove_if(routes.begin(), routes.end(),
                                [&](PickRoute r) {
                                    return r.source == removedNumber || r.target == removedNumber;
                                }),
                 routes.end());
    QJsonArray remainingTracks;
    for (auto track : tracks)
        if (track.toObject()["view"].toInt() != removedNumber)
            remainingTracks.append(track);
    tracks = remainingTracks;
    views.removeOne(view);
    view->setParent(nullptr);
    delete view;
    if (parent != root && parent->count() == 1) {
        QWidget *remaining = parent->widget(0);
        auto *grand = qobject_cast<QSplitter *>(parent->parentWidget());
        int index = grand->indexOf(parent);
        remaining->setParent(nullptr);
        grand->replaceWidget(index, remaining);
        delete parent;
        remaining->show();
    }
    QHash<int, int> renumber;
    for (int i = 0; i < views.size(); ++i) {
        renumber[views[i]->property("number").toInt()] = i + 1;
        views[i]->setProperty("number", i + 1);
    }
    for (auto &route : routes) {
        route.source = renumber.value(route.source);
        route.target = renumber.value(route.target);
    }
    for (int i = 0; i < tracks.size(); ++i) {
        auto track = tracks[i].toObject();
        track["view"] = renumber.value(track["view"].toInt());
        tracks[i] = track;
    }
    nextNumber = views.size() + 1;
    refreshViews(views.first());
}

bool Playground::copySettings(View *source, View *destination) {
    Settings previous = destination->settings;
    destination->settings = source->settings;
    if (!destination->apply()) {
        destination->settings = previous;
        error->setText(destination->lastError);
        return false;
    }
    if (active() == destination)
        load();
    return true;
}

void Playground::bindView(View *view) {
    view->mousePicked = [this, view](double x, double y, Qt::KeyboardModifiers m) {
        picked(view, x, y, m);
    };
    view->status = [this](QString text) { coordinates->setText(text); };
    view->activated = [this, view] { target->setCurrentIndex(int(views.indexOf(view))); };
}

void Playground::balance() {
    for (auto *s : centralWidget()->findChildren<QSplitter *>()) {
        QList<int> sizes;
        for (int i = 0; i < s->count(); ++i)
            sizes << 1000;
        s->setSizes(sizes);
    }
}

void Playground::splitView(Qt::Orientation orientation) {
    if (fullscreenMode)
        leaveFullscreen();
    View *old = active();
    auto *parent = qobject_cast<QSplitter *>(old->parentWidget());
    int index = parent->indexOf(old);
    auto *branch = new QSplitter(orientation);
    branch->setHandleWidth(1);
    branch->setChildrenCollapsible(false);
    parent->replaceWidget(index, branch);
    branch->addWidget(old);
    auto *next = new View;
    next->settings = old->settings;
    next->cx = old->cx;
    next->cy = old->cy;
    next->span = old->span;
    next->apply();
    branch->addWidget(next);
    next->setProperty("number", nextNumber++);
    views << next;
    bindView(next);
    refreshViews(next);
    branch->setSizes({1000, 1000});
}

void Playground::leaveFullscreen() {
    if (!fullscreenMode)
        return;
    for (auto *widget : fullscreenHidden)
        widget->show();
    fullscreenHidden.clear();
    toolbar->show();
    statusBar()->show();
    dock->setVisible(dockWasVisible);
    fullscreenMode = false;
    if (wasMaximized)
        showMaximized();
    else
        showNormal();
}

void Playground::enterFullscreen(bool single) {
    if (fullscreenMode)
        leaveFullscreen();
    wasMaximized = isMaximized();
    dockWasVisible = dock->isVisible();
    if (single) {
        QWidget *node = active();
        while (node != centralWidget()) {
            QWidget *parent = node->parentWidget();
            if (auto *s = qobject_cast<QSplitter *>(parent)) {
                for (int i = 0; i < s->count(); ++i) {
                    QWidget *other = s->widget(i);
                    if (other != node && !other->isHidden()) {
                        fullscreenHidden << other;
                        other->hide();
                    }
                }
            }
            node = parent;
        }
    }
    if (variableDock && variableDock->isVisible()) {
        fullscreenHidden << variableDock;
        variableDock->hide();
    }
    if (menuBar()->isVisible()) {
        fullscreenHidden << menuBar();
        menuBar()->hide();
    }
    toolbar->hide();
    dock->hide();
    statusBar()->hide();
    fullscreenMode = true;
    showFullScreen();
    active()->setFocus();
}

void Playground::rebuildVariables() {
    while (variableForm->rowCount())
        variableForm->removeRow(0);
    controls.clear();
    controls.resize(active()->settings.variables.size());
    controlNames = active()->settings.names;
    for (int i = 0; i < controls.size(); ++i)
        if (i == 3 || !active()->settings.names[i].isEmpty())
            addVariable(variableForm, i);
}

void Playground::manageVariables() {
    QDialog dialog(this);
    dialog.setWindowTitle("Named complex variables");
    QVBoxLayout layout(&dialog);
    QListWidget list;
    layout.addWidget(&list);
    auto refresh = [&] {
        list.clear();
        for (auto name : active()->settings.names)
            if (!name.isEmpty())
                list.addItem(name);
    };
    refresh();
    QLineEdit name;
    name.setPlaceholderText("Variable name");
    layout.addWidget(&name);
    QPushButton add("Add"), remove("Delete selected");
    layout.addWidget(&add);
    layout.addWidget(&remove);
    QLabel message;
    layout.addWidget(&message);
    connect(&add, &QPushButton::clicked, &dialog, [&] {
        auto &s = active()->settings;
        QString id = name.text().trimmed();
        if (!Equation::validName(id) || s.names.contains(id)) {
            message.setText("Choose a unique identifier; built-in names are reserved.");
            return;
        }
        s.names.append(id);
        s.variables.append(Variable{});
        rebuildVariables();
        load();
        apply();
        refresh();
    });
    connect(&remove, &QPushButton::clicked, &dialog, [&] {
        auto *item = list.currentItem();
        if (!item)
            return;
        auto &s = active()->settings;
        int i = s.names.indexOf(item->text());
        if (i < 0)
            return;
        if (i < 4) {
            s.names[i] = "";
            s.variables[i] = Variable{};
        } else {
            s.names.removeAt(i);
            s.variables.removeAt(i);
        }
        routes.erase(std::remove_if(routes.begin(), routes.end(),
                                    [&](PickRoute r) {
                                        return r.target == active()->property("number").toInt() &&
                                               r.variable == i;
                                    }),
                     routes.end());
        for (auto &r : routes)
            if (r.target == active()->property("number").toInt() && r.variable > i && i >= 4)
                --r.variable;
        rebuildVariables();
        load();
        apply();
        refresh();
    });
    dialog.exec();
}

void Playground::visibility(int i) {
    auto &u = controls[i];
    int mode = u.source->currentIndex();
    u.values->setVisible(mode == Constant || mode == Mouse);
    u.picking->setVisible(mode == Mouse);
    u.expression->setVisible(mode == Expression);
}

void Playground::addVariable(QFormLayout *form, int i) {
    auto &u = controls[i];
    auto *box = new QGroupBox(i == 3 ? QString("Initial z") : active()->settings.names[i]);
    auto *rows = new QFormLayout(box);
    u.source = new QComboBox;
    u.source->addItems({"Constant", "Mouse picked", "Pixel coordinate"});
    if (i == 3)
        u.source->addItem("Equation");
    rows->addRow("Source", u.source);
    u.values = new QWidget;
    auto *values = new QFormLayout(u.values);
    values->setContentsMargins(0, 0, 0, 0);
    u.real = number(-1e6, 1e6, 12);
    u.imag = number(-1e6, 1e6, 12);
    values->addRow("Real", new NumericSlider(u.real));
    values->addRow("Imaginary", new NumericSlider(u.imag));
    rows->addRow(u.values);
    u.picking = new QWidget;
    auto *picks = new QFormLayout(u.picking);
    picks->setContentsMargins(0, 0, 0, 0);
    u.modifier = new QComboBox;
    for (int mask = 0; mask < 8; ++mask) {
        QStringList names;
        if (mask & 1)
            names << "Shift";
        if (mask & 2)
            names << "Ctrl";
        if (mask & 4)
            names << "Alt";
        int flags = ((mask & 1) ? int(Qt::ShiftModifier) : 0) |
                    ((mask & 2) ? int(Qt::ControlModifier) : 0) |
                    ((mask & 4) ? int(Qt::AltModifier) : 0);
        u.modifier->addItem(
            names.isEmpty() ? "Right button" : names.join(" + ") + " + Right button", flags);
    }
    picks->addRow("Pick with", u.modifier);
    rows->addRow(u.picking);
    u.expression = new QLineEdit;
    u.expression->setMaxLength(512);
    u.expression->setPlaceholderText("e.g. a+b*i");
    rows->addRow(u.expression);
    form->addRow(box);
    connect(u.source, &QComboBox::currentIndexChanged, this, [this, i] {
        visibility(i);
        apply();
    });
    connect(u.modifier, &QComboBox::currentIndexChanged, this, [this] { apply(); });
    for (auto *value : {u.real, u.imag})
        connect(value, &QDoubleSpinBox::valueChanged, this, [this] { apply(); });
    connect(u.expression, &QLineEdit::returnPressed, this, [this] { apply(); });
}

View *Playground::active() const {
    return views.value(qMax(0, target->currentIndex()), views.first());
}

QDoubleSpinBox *Playground::number(double min, double max, int decimals) {
    auto *v = new QDoubleSpinBox;
    v->setRange(min, max);
    v->setDecimals(decimals);
    v->setSingleStep(.005);
    v->setKeyboardTracking(false);
    return v;
}

void Playground::load() {
    loading = true;
    const auto &s = active()->settings;
    equation->setText(s.equation);
    bailout->setValue(s.bailout);
    iterations->setValue(s.iterations);
    historySize->setValue(s.historySize);
    palette->setCurrentIndex(s.palette >= 4 ? qMax(4, palette->findText(s.gradientName))
                                            : s.palette);
    frequency->setValue(s.frequency);
    phase->setValue(s.phase);
    smoothing->setValue(s.smoothing);
    if (controlNames != s.names)
        rebuildVariables();
    if (variableDock) {
        variableDock->setWindowTitle("Variables — " + viewName(active()));
        variableDock->toggleViewAction()->setText("Variable editor");
    }
    for (int i = 0; i < s.variables.size(); ++i) {
        if (i != 3 && s.names[i].isEmpty())
            continue;
        const auto &v = s.variables[i];
        auto &u = controls[i];
        u.source->setCurrentIndex(v.source);
        u.real->setValue(v.real);
        u.imag->setValue(v.imag);
        u.expression->setText(v.equation);
        u.modifier->setCurrentIndex(u.modifier->findData(v.modifiers));
        visibility(i);
    }
    loading = false;
}

void Playground::apply() {
    if (loading)
        return;
    auto &s = active()->settings;
    s.equation = equation->text();
    s.bailout = bailout->value();
    s.iterations = iterations->value();
    s.historySize = historySize->value();
    s.palette = palette->currentIndex() >= 4 ? 5 : palette->currentIndex();
    if (palette->currentIndex() >= 4 && palette->currentIndex() - 4 < gradientLibrary.size()) {
        auto entry = gradientLibrary[palette->currentIndex() - 4].toObject();
        s.gradientName = entry["name"].toString();
        s.gradient = entry["stops"].toArray();
    }
    s.frequency = frequency->value();
    s.phase = phase->value();
    s.smoothing = smoothing->value();
    for (int i = 0; i < s.variables.size(); ++i) {
        if (i != 3 && s.names[i].isEmpty())
            continue;
        auto &v = s.variables[i];
        auto &u = controls[i];
        v.source = u.source->currentIndex();
        v.real = u.real->value();
        v.imag = u.imag->value();
        v.equation = u.expression->text();
        v.modifiers = u.modifier->currentData().toInt();
    }
    error->setText(active()->apply() ? "" : active()->lastError);
}

void Playground::choosePreset(int index) {
    if (loading)
        return;
    auto &s = active()->settings;
    Settings previous = s;
    s = Settings();
    s.palette = previous.palette;
    s.frequency = previous.frequency;
    s.phase = previous.phase;
    s.smoothing = previous.smoothing;
    s.gradient = previous.gradient;
    s.gradientName = previous.gradientName;
    switch (index) {
    case 1:
        s.variables[3].source = Pixel;
        s.variables[2].source = Mouse;
        break;
    case 2:
        s.equation = "abs(z[n])^p+c";
        break;
    case 3:
        s.equation = "conj(z[n])^p+c";
        break;
    case 4:
        s.variables[s.names.indexOf("p")].real = 3;
        break;
    case 5:
        s.names[0] = "a";
        s.equation = "sin(z[n])*a+c";
        s.variables[3].source = Pixel;
        s.variables[2].source = Mouse;
        break;
    }
    load();
    apply();
    active()->reset();
}

bool Playground::makeJulia(View *source, View *destination) {
    if (source == destination) {
        error->setText("Choose different source and target views.");
        return false;
    }
    if (active() == source) {
        apply();
        if (!error->text().isEmpty())
            return false;
    }
    try {
        if (source->settings.variables[2].source != Pixel)
            throw std::runtime_error("The source view must use pixel coordinates as c.");
        if (source->settings.variables[3].source == Pixel ||
            source->settings.variables[3].source == Expression)
            throw std::runtime_error(
                "The source view must have a constant or mouse-picked initial z.");
        if (QRegularExpression("\\bpixel\\b")
                .match(Equation::compile(source->settings.equation, source->settings.names))
                .hasMatch())
            throw std::runtime_error("Use c rather than pixel in the source recurrence so it can "
                                     "become a constant Julia parameter.");
        Settings previous = destination->settings;
        destination->settings = source->settings;
        auto &s = destination->settings;
        s.palette = previous.palette;
        s.frequency = previous.frequency;
        s.phase = previous.phase;
        s.smoothing = previous.smoothing;
        s.gradient = previous.gradient;
        s.gradientName = previous.gradientName;
        s.variables[3].source = Pixel;
        s.variables[2] = previous.variables[2];
        s.variables[2].source = Mouse;
        if (!destination->apply()) {
            destination->settings = previous;
            throw std::runtime_error(destination->lastError.toStdString());
        }
        destination->reset();
        target->setCurrentIndex(views.indexOf(destination));
        load();
        error->clear();
        return true;
    } catch (const std::exception &e) {
        error->setText(e.what());
        return false;
    }
}

void Playground::picked(View *source, double x, double y, Qt::KeyboardModifiers modifiers) {
    int flags = int(modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier));
    bool routed = false;
    for (auto route : routes)
        if (route.source == source->property("number").toInt() && route.modifiers == flags) {
            if (auto *dest = byNumber(route.target)) {
                auto &v = dest->settings.variables[route.variable];
                if (v.source == Pixel)
                    v.source = Constant;
                v.real = x;
                v.imag = y;
                dest->apply();
                if (active() == dest)
                    load();
                routed = true;
            }
        }
    if (routed)
        return;
    pickInto(source, x, y, modifiers);
}

void Playground::pickInto(View *dest, double x, double y, Qt::KeyboardModifiers modifiers) {
    int flags = int(modifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier));
    bool changed = false;
    for (int i = 0; i < dest->settings.variables.size(); ++i) {
        auto &v = dest->settings.variables[i];
        if ((i == 3 || !dest->settings.names[i].isEmpty()) && v.source == Mouse &&
            v.modifiers == flags) {
            v.real = x;
            v.imag = y;
            changed = true;
        }
    }
    if (!changed)
        return;
    if (!dest->apply())
        error->setText(dest->lastError);
    if (active() == dest)
        load();
}

Playground::Playground() {
    initializeWorkspace();
}

void Playground::initializeWorkspace() {
    setWindowTitle("Fractal Studio");
    resize(1420, 900);
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    container->setStyleSheet("QSplitter::handle { background: black; }");
    root = new QSplitter;
    root->setHandleWidth(1);
    root->setChildrenCollapsible(false);
    left = new View;
    right = nullptr;
    root->addWidget(left);
    views = {left};
    for (auto *view : views)
        view->setProperty("number", nextNumber++);
    layout->addWidget(root);
    setCentralWidget(container);
    dock = new QDockWidget("ACTIVE VIEW  /  INSPECTOR", this);
    dock->setObjectName("activeViewInspector");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(320);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *body = new QWidget;
    auto *inspectorLayout = new QVBoxLayout(body);
    inspectorLayout->setContentsMargins(16, 18, 16, 16);
    auto *inspectorTitle = new QLabel("View properties");
    inspectorTitle->setProperty("role", "title");
    inspectorLayout->addWidget(inspectorTitle);
    auto *tabs = new QTabWidget;
    tabs->setObjectName("inspectorTabs");
    inspectorLayout->addWidget(tabs);
    auto makePage = [tabs](const QString &name) {
        auto *page = new QWidget;
        auto *form = new QFormLayout(page);
        form->setContentsMargins(0, 20, 0, 0);
        form->setVerticalSpacing(14);
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        tabs->addTab(page, name);
        return form;
    };
    auto *fractalForm = makePage("Fractal");
    auto *parameterForm = makePage("Parameters");
    auto *appearanceForm = makePage("Appearance");
    auto *form = fractalForm;
    inspectorLayout->addStretch();
    scroll->setWidget(body);
    dock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    toolbar = addToolBar("Workspace controls");
    toolbar->setObjectName("workspaceControls");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(18, 18));
    auto *workspaceTitle = new QLabel("Untitled workspace");
    toolbar->addWidget(workspaceTitle);
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    auto *splitHorizontal = toolbar->addAction(studioIcon("split"), "Split H");
    splitHorizontal->setObjectName("splitHorizontal");
    splitHorizontal->setToolTip("Split active view horizontally (side by side)");
    connect(splitHorizontal, &QAction::triggered, this, [this] { splitView(Qt::Horizontal); });
    auto *splitVertical = toolbar->addAction(studioIcon("splitVertical"), "Split V");
    splitVertical->setObjectName("splitVertical");
    splitVertical->setToolTip("Split active view vertically (stacked)");
    connect(splitVertical, &QAction::triggered, this, [this] { splitView(Qt::Vertical); });
    auto *closeView = toolbar->addAction(studioIcon("close"), "Close view");
    closeView->setObjectName("closeActiveView");
    connect(closeView, &QAction::triggered, this, [this] { removeView(); });
    toolbar->addSeparator();
    auto *reset = toolbar->addAction(studioIcon("reset"), "Reset");
    auto *focus = toolbar->addAction(studioIcon("full"), "Focus");
    connect(focus, &QAction::triggered, this, [this] { enterFullscreen(true); });
    toolbar->addSeparator();
    dock->toggleViewAction()->setText("Inspector");
    dock->toggleViewAction()->setIcon(studioIcon("sliders"));
    toolbar->addAction(dock->toggleViewAction());
    connect(reset, &QAction::triggered, this, [this] { active()->reset(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] { leaveFullscreen(); });
    auto *full = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(full, &QShortcut::activated, this, [this] {
        if (fullscreenMode)
            leaveFullscreen();
        else
            enterFullscreen(true);
    });
    auto *all = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F11), this);
    connect(all, &QShortcut::activated, this, [this] {
        if (fullscreenMode)
            leaveFullscreen();
        else
            enterFullscreen(false);
    });

    auto *shortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    connect(shortcut, &QShortcut::activated, this,
            [this] { dock->setVisible(!dock->isVisible()); });
    target = new QComboBox;
    target->addItem("View 1");
    inspectorLayout->insertWidget(1, target);
    target->setObjectName("activeViewSelector");
    connect(target, &QComboBox::currentIndexChanged, this, [this] { load(); });

    form->addRow(new QLabel("<b>Equation</b>"));
    auto *presetButton = new QPushButton(studioIcon("preset"), "Start from preset…");
    presetButton->setObjectName("openPresetGallery");
    form->addRow(presetButton);
    connect(presetButton, &QPushButton::clicked, this, &Playground::presetGallery);
    equation = new QLineEdit;
    equation->setMaxLength(512);
    equation->setFont(QFont("Consolas", 11));
    form->addRow("z[n+1] =", equation);
    auto *applyButton = new QPushButton("Compile equation");
    form->addRow(applyButton);
    connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
    connect(equation, &QLineEdit::returnPressed, this, [this] { apply(); });
    error = new QLabel;
    error->setWordWrap(true);
    error->setStyleSheet("color: #ff9191");
    form->addRow(error);
    variableDock = new QDockWidget("Variables", this);
    variableDock->setObjectName("variableEditor");
    variableDock->setMinimumWidth(440);
    variableDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    variableDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable);
    auto *variableScroll = new QScrollArea;
    variableScroll->setWidgetResizable(true);
    auto *variableBody = new QWidget;
    auto *variableLayout = new QVBoxLayout(variableBody);
    auto *fields = new QWidget;
    variableForm = new QFormLayout(fields);
    variableLayout->addWidget(fields);
    auto *manage = new QPushButton("Add / delete named variables");
    variableLayout->addWidget(manage);
    variableLayout->addStretch();
    connect(manage, &QPushButton::clicked, this, [this] { manageVariables(); });
    variableScroll->setWidget(variableBody);
    variableDock->setWidget(variableScroll);
    addDockWidget(Qt::LeftDockWidgetArea, variableDock);
    variableDock->hide();
    rebuildVariables();
    auto *openVariables = new QPushButton("Open variable editor…");
    parameterForm->addRow(new QLabel("<b>Complex inputs</b>"));
    parameterForm->addRow(openVariables);
    auto *seeds = new QPushButton("History seeds…");
    parameterForm->addRow(seeds);
    historySize = new QSpinBox;
    historySize->setRange(1, 10000);
    historySize->setKeyboardTracking(false);
    historySize->setToolTip("Values retained per pixel, including z[n]. Use 3 for z[n-2]. Large buffers may exceed GPU resources.");
    parameterForm->addRow("History size", historySize);
    connect(historySize, &QSpinBox::valueChanged, this, [this] { apply(); });
    connect(seeds, &QPushButton::clicked, this, [this] { negativeHistoryDialog(); });
    auto *links = new QPushButton("Picking links…");
    parameterForm->addRow(links);
    connect(links, &QPushButton::clicked, this, [this] { linksDialog(); });
    connect(openVariables, &QPushButton::clicked, this, [this] {
        variableDock->show();
        variableDock->raise();
    });
    form->addRow(new QLabel("<b>Rendering</b>"));
    bailout = number(.001, 1e9);
    form->addRow("Escape radius", bailout);
    iterations = new QSpinBox;
    iterations->setRange(16, 10000);
    iterations->setSingleStep(100);
    iterations->setKeyboardTracking(false);
    form->addRow("Iterations", iterations);
    form = appearanceForm;
    form->addRow(new QLabel("<b>Colour mapping</b>"));
    auto *gradients = new QPushButton("Edit gradient library…");
    form->addRow(gradients);
    connect(gradients, &QPushButton::clicked, this, [this] { gradientDialog(); });
    palette = new QComboBox;
    palette->addItems(
        {"Spectrum", "Ember", "Ocean", "Monochrome", "Custom gradient"});
    form->addRow("Palette", palette);
    frequency = number(.00001, 10);
    frequency->setSingleStep(.005);
    phase = number(-100, 100);
    phase->setSingleStep(.05);
    form->addRow("Colour frequency", frequency);
    form->addRow("Colour phase", phase);
    smoothing = number(0, 1, 3);
    smoothing->setSingleStep(.1);
    smoothing->setToolTip("0: discrete escape bands; 1: continuous quadratic-reference colouring.");
    form->addRow("Colour smoothing", smoothing);
    for (auto *section : {fractalForm, parameterForm, appearanceForm}) {
        for (int row = 0; row < section->rowCount(); ++row) {
            auto *item = section->itemAt(row, QFormLayout::FieldRole);
            if (!item)
                continue;
            if (auto *numeric = qobject_cast<QAbstractSpinBox *>(item->widget())) {
                section->removeWidget(numeric);
                section->setWidget(row, QFormLayout::FieldRole, new NumericSlider(numeric));
            }
        }
    }
    coordinates = new QLabel("  READY   /   Drag to pan · Wheel to zoom");
    statusBar()->addWidget(coordinates, 1);
    for (auto *v : {bailout, frequency, phase, smoothing})
        connect(v, &QDoubleSpinBox::valueChanged, this, [this] { apply(); });
    connect(iterations, &QSpinBox::valueChanged, this, [this] { apply(); });
    connect(palette, &QComboBox::currentIndexChanged, this, [this] { apply(); });
    bindView(left);
    load();
    initGradients();
    setupTools();
    QTimer::singleShot(0, this, [this] { balance(); });
}
