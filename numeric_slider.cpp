#include "numeric_slider.h"

double NumericSlider::value() const {
    return realEditor ? realEditor->value() : intEditor->value();
}

void NumericSlider::sync() {
    QSignalBlocker block(slider);
    slider->setValue(qRound(qBound(0.0, (value() - lower) / (upper - lower), 1.0) * 10000));
}

NumericSlider::NumericSlider(QAbstractSpinBox *input)
    : editor(input), realEditor(qobject_cast<QDoubleSpinBox *>(input)),
      intEditor(qobject_cast<QSpinBox *>(input)) {
    double minimum = realEditor ? realEditor->minimum() : intEditor->minimum();
    double maximum = realEditor ? realEditor->maximum() : intEditor->maximum();
    lower = qMax(minimum, value() - 1);
    upper = qMin(maximum, value() + 1);
    if (intEditor) {
        lower = minimum;
        upper = maximum;
    }
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *row = new QHBoxLayout;
    row->addWidget(editor, 1);
    auto *button = new QToolButton;
    button->setText("↔");
    button->setToolTip("Configure slider range or return to numeric input");
    row->addWidget(button);
    layout->addLayout(row);
    slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 10000);
    slider->hide();
    layout->addWidget(slider);
    connect(slider, &QSlider::valueChanged, this, [this](int position) {
        double next = lower + (upper - lower) * position / 10000.;
        if (realEditor)
            realEditor->setValue(next);
        else
            intEditor->setValue(qRound(next));
    });
    if (realEditor)
        connect(realEditor, &QDoubleSpinBox::valueChanged, this, [this] { sync(); });
    else
        connect(intEditor, &QSpinBox::valueChanged, this, [this] { sync(); });
    connect(button, &QToolButton::clicked, this, [this, minimum, maximum] {
        QDialog dialog(this);
        dialog.setWindowTitle("Slider range");
        QFormLayout form(&dialog);
        QCheckBox enabled("Show slider");
        enabled.setChecked(!slider->isVisibleTo(this) ? false : true);
        QDoubleSpinBox from, to;
        for (auto *field : {&from, &to}) {
            field->setDecimals(realEditor ? realEditor->decimals() : 0);
            field->setRange(minimum, maximum);
        }
        from.setValue(lower);
        to.setValue(upper);
        form.addRow(&enabled);
        form.addRow("Minimum", &from);
        form.addRow("Maximum", &to);
        QLabel message;
        form.addRow(&message);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            if (from.value() >= to.value()) {
                message.setText("Minimum must be less than maximum.");
                return;
            }
            dialog.accept();
        });
        if (dialog.exec() == QDialog::Accepted) {
            lower = from.value();
            upper = to.value();
            slider->setVisible(enabled.isChecked());
            slider->setToolTip(QString("%1 to %2").arg(lower, 0, 'g', 12).arg(upper, 0, 'g', 12));
            sync();
        }
    });
    sync();
}

QJsonObject NumericSlider::state() const {
    return {{"lower", lower}, {"upper", upper}, {"enabled", !slider->isHidden()}};
}

void NumericSlider::restoreState(QJsonObject state) {
    double lo = state["lower"].toDouble(NAN), hi = state["upper"].toDouble(NAN);
    double minimum = realEditor ? realEditor->minimum() : intEditor->minimum(),
           maximum = realEditor ? realEditor->maximum() : intEditor->maximum();
    if (!std::isfinite(lo) || !std::isfinite(hi) || lo >= hi || lo < minimum || hi > maximum)
        return;
    lower = lo;
    upper = hi;
    slider->setVisible(state["enabled"].toBool());
    sync();
}

bool NumericSlider::test() {
    lower = realEditor ? -0.5 : 16;
    upper = realEditor ? 0.5 : 116;
    slider->setValue(5000);
    bool ok = std::abs(value() - (lower + upper) * .5) < 1e-8;
    slider->setValue(10000);
    return ok && std::abs(value() - upper) < 1e-8;
}
