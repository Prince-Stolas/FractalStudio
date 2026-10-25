#include "mainwindow.h"
#include "ui_design.h"

void Playground::presetGallery() {
    QDialog dialog(this);
    dialog.setObjectName("presetGallery");
    dialog.setWindowTitle("Start from a preset");
    dialog.resize(650, 540);
    QVBoxLayout layout(&dialog);
    layout.setContentsMargins(24, 24, 24, 24);
    layout.setSpacing(16);
    QLabel title("A starting point, not a mode");
    title.setProperty("role", "title");
    layout.addWidget(&title);
    QLabel subtitle("Replace the active view’s equation and inputs. Your colour settings stay unchanged.");
    subtitle.setWordWrap(true);
    layout.addWidget(&subtitle);
    QListWidget gallery;
    gallery.setObjectName("presetChoices");
    const QStringList names = {"Mandelbrot", "Julia", "Burning ship", "Tricorn", "Cubic Multibrot", "Sine Julia"};
    const QStringList descriptions = {
        "The classic quadratic plane · z[n]^p + c",
        "Explore a fixed complex parameter · pixel-seeded orbit",
        "Fold real and imaginary components before each iteration",
        "A conjugate recurrence with threefold symmetry",
        "A third-power starting point · p remains an editable variable",
        "A transcendental recurrence · sin(z[n]) * a + c"};
    gallery.setSpacing(4);
    gallery.setUniformItemSizes(true);
    for (int i = 0; i < names.size(); ++i) {
        auto *item = new QListWidgetItem(studioIcon("preset"), names[i] + "\n" + descriptions[i]);
        item->setSizeHint(QSize(0, gallery.fontMetrics().lineSpacing() * 2 + 20));
        gallery.addItem(item);
    }
    layout.addWidget(&gallery, 1);
    QDialogButtonBox buttons(QDialogButtonBox::Cancel);
    auto *apply = buttons.addButton("Apply to active view", QDialogButtonBox::AcceptRole);
    apply->setProperty("primary", true);
    apply->setEnabled(false);
    layout.addWidget(&buttons);
    connect(&gallery, &QListWidget::currentRowChanged, &dialog, [apply](int row) { apply->setEnabled(row >= 0); });
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted && gallery.currentRow() >= 0)
        choosePreset(gallery.currentRow());
}

void Playground::refreshColourPanel() {
    if (!colourPanel)
        return;
    if (auto *old = colourPanel->layout()) {
        while (old->count()) {
            auto *item = old->takeAt(0);
            if (item->widget()) {
                item->widget()->hide();
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete old;
    }
    auto *form = new QFormLayout(colourPanel);
    form->setContentsMargins(0, 0, 0, 0);
    const auto &s = active()->settings;
    const auto &c = s.colouring;
    auto commit = [this] { active()->apply(); checkpoint(); };
    auto *mode = new QComboBox;
    mode->addItems({"Escape / interior", "Orbit trap"});
    mode->setCurrentIndex(c.mode);
    form->addRow("Colouring mode", mode);
    connect(mode, &QComboBox::currentIndexChanged, colourPanel, [this, commit](int index) {
        active()->settings.colouring.mode = index;
        commit();
        refreshColourPanel();
    });
    auto addPalette = [this, form, commit](bool interior) {
        auto *combo = new QComboBox;
        combo->addItems({"Spectrum", "Ember", "Ocean", "Monochrome"});
        for (auto entry : gradientLibrary)
            combo->addItem(entry.toObject()["name"].toString());
        const auto &settings = active()->settings;
        int index = interior ? settings.colouring.interiorPalette : settings.palette;
        QString name = interior ? settings.colouring.interiorGradientName : settings.gradientName;
        combo->setCurrentIndex(index >= 4 ? qMax(4, combo->findText(name)) : index);
        form->addRow("Gradient", combo);
        connect(combo, &QComboBox::currentIndexChanged, colourPanel, [this, interior, commit](int index) {
            auto &settings = active()->settings;
            if (interior) settings.colouring.interiorPalette = qMin(index, 4);
            else settings.palette = index >= 4 ? 5 : index;
            if (index >= 4 && index - 4 < gradientLibrary.size()) {
                auto entry = gradientLibrary[index - 4].toObject();
                if (interior) {
                    settings.colouring.interiorGradientName = entry["name"].toString();
                    settings.colouring.interiorStops = entry["stops"].toArray();
                } else {
                    settings.gradientName = entry["name"].toString();
                    settings.gradient = entry["stops"].toArray();
                }
            }
            if (!interior) {
                QSignalBlocker blocker(palette);
                palette->setCurrentIndex(index);
            }
            commit();
        });
    };
    auto *mapping = new QComboBox;
    mapping->addItems({"Absolute", "View-scaled"});
    mapping->setCurrentIndex(c.trapViewScaled);
    mapping->setToolTip("Absolute calibrates convergence in a fixed reference region. View-scaled calibrates the visible region, adjusts escape bands, and scales orbit-trap distances.");
    auto *mappingRow = new QWidget;
    auto *mappingLayout = new QHBoxLayout(mappingRow);
    mappingLayout->setContentsMargins(0, 0, 0, 0);
    mappingLayout->addWidget(mapping, 1);
    auto *calibration = new QToolButton;
    calibration->setText("…");
    calibration->setToolTip("Advanced convergence calibration region");
    mappingLayout->addWidget(calibration);
    form->addRow("Colour mapping", mappingRow);
    connect(calibration, &QToolButton::clicked, colourPanel, [this, commit] {
        QDialog dialog(this);
        dialog.setWindowTitle("Convergence calibration region");
        QFormLayout layout(&dialog);
        auto value = active()->settings.colouring;
        QCheckBox custom("Use custom reference region for Absolute mapping");
        custom.setChecked(value.customReference);
        layout.addRow(&custom);
        QDoubleSpinBox real, imag, span, aspect;
        for (auto *input : {&real, &imag, &span, &aspect}) {
            input->setDecimals(12);
            input->setRange(-1e15, 1e15);
        }
        span.setMinimum(1e-15);
        aspect.setRange(0.1, 10);
        real.setValue(value.referenceReal);
        imag.setValue(value.referenceImag);
        span.setValue(value.referenceSpan);
        aspect.setValue(value.referenceAspect);
        layout.addRow("Centre real", &real);
        layout.addRow("Centre imaginary", &imag);
        layout.addRow("Vertical span", &span);
        layout.addRow("Width / height", &aspect);
        QPushButton current("Use current view");
        layout.addRow(&current);
        connect(&current, &QPushButton::clicked, &dialog, [&] {
            custom.setChecked(true);
            real.setValue(double(active()->cx));
            imag.setValue(double(active()->cy));
            span.setValue(double(active()->span));
            aspect.setValue(double(active()->width()) / qMax(1, active()->height()));
        });
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        auto &colour = active()->settings.colouring;
        colour.customReference = custom.isChecked();
        colour.referenceReal = real.value();
        colour.referenceImag = imag.value();
        colour.referenceSpan = span.value();
        colour.referenceAspect = aspect.value();
        commit();
    });
    connect(mapping, &QComboBox::currentIndexChanged, colourPanel, [this, commit](int index) {
        active()->settings.colouring.trapViewScaled = index == 1;
        commit();
    });
    if (c.mode == 0) {
        for (bool interior : {false, true}) {
            auto *kind = new QComboBox;
            kind->addItems({"Solid", "Gradient"});
            bool gradient = interior ? c.interiorGradient : !c.escapeSolid;
            kind->setCurrentIndex(gradient);
            form->addRow(interior ? "Interior points" : "Escaping points", kind);
            connect(kind, &QComboBox::currentIndexChanged, colourPanel, [this, interior, commit](int index) {
                if (interior) active()->settings.colouring.interiorGradient = index == 1;
                else active()->settings.colouring.escapeSolid = index == 0;
                commit();
                refreshColourPanel();
            });
            if (gradient) {
                addPalette(interior);
            } else {
                QColor colour = interior ? c.interiorColour : c.escapeColour;
                auto *input = new QLineEdit(colour.name());
                input->setToolTip("Colour name or hexadecimal colour, for example #ff8800");
                form->addRow("Colour", input);
                connect(input, &QLineEdit::editingFinished, colourPanel, [this, interior, input, commit] {
                    auto &value = active()->settings.colouring;
                    QColor selected(input->text());
                    if (!selected.isValid()) {
                        input->setText((interior ? value.interiorColour : value.escapeColour).name());
                        return;
                    }
                    if (interior) value.interiorColour = selected;
                    else value.escapeColour = selected;
                    input->setText(selected.name());
                    commit();
                });
            }
        }
        if (c.interiorGradient) {
            auto *tolerance = new QDoubleSpinBox;
            tolerance->setDecimals(12);
            tolerance->setRange(1e-12, 1);
            tolerance->setValue(c.tolerance);
            tolerance->setKeyboardTracking(false);
            tolerance->setToolTip("Fixed-point speed when stable; otherwise residual progress at the iteration budget. Periodic or bounded does not imply convergence. No early termination.");
            form->addRow("Fixed-point tolerance", tolerance);
            connect(tolerance, &QDoubleSpinBox::valueChanged, colourPanel, [this, commit](double value) {
                active()->settings.colouring.tolerance = value;
                commit();
            });
        }
    } else {
        addPalette(false);
        auto *shape = new QComboBox;
        shape->addItems({"Point", "Circle", "Cross"});
        shape->setCurrentIndex(c.trap);
        form->addRow("Trap shape", shape);
        connect(shape, &QComboBox::currentIndexChanged, colourPanel, [this, commit](int index) {
            active()->settings.colouring.trap = index;
            commit();
            refreshColourPanel();
        });
        for (int component = 0; component < (c.trap == 1 ? 3 : 2); ++component) {
            auto *spin = new QDoubleSpinBox;
            spin->setDecimals(6);
            spin->setRange(component == 2 ? 0 : -1e9, 1e9);
            spin->setValue(component == 0 ? c.trapReal : component == 1 ? c.trapImag : c.trapRadius);
            spin->setKeyboardTracking(false);
            form->addRow(component == 0 ? "Trap centre real" : component == 1 ? "Trap centre imaginary" : "Circle radius", spin);
            connect(spin, &QDoubleSpinBox::valueChanged, colourPanel, [this, component, commit](double value) {
                auto &colouring = active()->settings.colouring;
                if (component == 0) colouring.trapReal = value;
                else if (component == 1) colouring.trapImag = value;
                else colouring.trapRadius = value;
                commit();
            });
        }
    }
}
