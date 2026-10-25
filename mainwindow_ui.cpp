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
    for (int i = 0; i < names.size(); ++i)
        gallery.addItem(new QListWidgetItem(studioIcon("preset"), names[i] + "\n" + descriptions[i]));
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
