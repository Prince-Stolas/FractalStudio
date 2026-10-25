#pragma once
#include <QtWidgets>

// All application glyphs share a 24-unit canvas and a single rounded stroke.
inline QIcon studioIcon(const QString &name) {
    QIcon icon;
    for (int size : {24, 48}) {
        QPixmap image(size, size);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 24.0, size / 24.0);
        p.setPen(QPen(QColor("#bdc7d3"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        if (name == "split") {
            p.drawRoundedRect(QRectF(4, 5, 16, 14), 2, 2);
            p.drawLine(12, 5, 12, 19);
        } else if (name == "reset") {
            p.drawArc(QRectF(5, 5, 14, 14), 30 * 16, 290 * 16);
            p.drawPolyline(QPolygonF{QPointF(4, 5), QPointF(5, 10), QPointF(10, 8)});
        } else if (name == "full") {
            for (int x : {4, 20}) for (int y : {4, 20}) {
                p.drawLine(x, y, x == 4 ? 9 : 15, y);
                p.drawLine(x, y, x, y == 4 ? 9 : 15);
            }
        } else if (name == "open") {
            p.drawPolyline(QPolygonF{QPointF(4, 18), QPointF(4, 6), QPointF(10, 6), QPointF(12, 9), QPointF(20, 9), QPointF(17, 18), QPointF(4, 18), QPointF(7, 11), QPointF(19, 11)});
        } else if (name == "save") {
            p.drawRoundedRect(QRectF(5, 4, 14, 16), 1, 1);
            p.drawRect(QRectF(8, 4, 7, 6));
            p.drawRect(QRectF(8, 14, 8, 6));
        } else if (name == "preset") {
            for (int x : {5, 14}) for (int y : {5, 14}) p.drawRoundedRect(QRectF(x, y, 5, 5), 1, 1);
        } else {
            p.drawLine(5, 7, 19, 7); p.drawLine(5, 17, 19, 17);
            p.drawEllipse(QPointF(9, 7), 2, 2); p.drawEllipse(QPointF(15, 17), 2, 2);
        }
        p.end();
        icon.addPixmap(image);
    }
    return icon;
}

inline void applyStudioTheme(QApplication &app) {
    app.setStyle("Fusion");
    QFont font("Segoe UI", 10);
    app.setFont(font);
    QPalette colors;
    colors.setColor(QPalette::Window, QColor("#23262b"));
    colors.setColor(QPalette::WindowText, QColor("#dbe1e8"));
    colors.setColor(QPalette::Base, QColor("#191c21"));
    colors.setColor(QPalette::AlternateBase, QColor("#282d34"));
    colors.setColor(QPalette::Text, QColor("#dbe1e8"));
    colors.setColor(QPalette::Button, QColor("#30353c"));
    colors.setColor(QPalette::ButtonText, QColor("#dbe1e8"));
    colors.setColor(QPalette::Highlight, QColor("#426f79"));
    colors.setColor(QPalette::HighlightedText, Qt::white);
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor("#737d89"));
    app.setPalette(colors);
    app.setStyleSheet(R"(
        QMainWindow::separator { background: #171a1e; width: 4px; height: 4px; }
        QToolBar { background: #25292f; border: 0; border-bottom: 1px solid #111418; spacing: 8px; padding: 9px; }
        QToolButton { border: 1px solid transparent; border-radius: 4px; padding: 6px; }
        QToolButton:hover, QPushButton:hover { background: #39434b; border-color: #52606c; }
        QDockWidget::title { background: #292e35; padding: 10px; font-weight: 600; }
        QTabWidget::pane { border: 0; border-top: 1px solid #3a414b; }
        QTabBar::tab { padding: 12px 14px; color: #929eab; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: #e3ecee; border-bottom-color: #78b6b8; background: #2b3138; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #191d22; border: 1px solid #3a414b; border-radius: 4px; min-height: 28px; padding: 2px 5px; selection-background-color: #426f79; }
        QLineEdit:focus, QComboBox:focus { border-color: #78b6b8; }
        QPushButton { background: #30373f; border: 1px solid #444e59; border-radius: 4px; padding: 8px 12px; }
        QPushButton[primary="true"] { background: #365c64; border-color: #5c8990; }
        QScrollArea { border: 0; }
        QMenuBar { background: #1c1f24; padding: 3px; }
        QMenuBar::item { padding: 5px 10px; }
        QMenu { padding: 6px; border: 1px solid #424b56; }
        QMenu::item { padding: 8px 28px; }
        QStatusBar { background: #1c1f24; color: #919eac; border-top: 1px solid #363d46; }
        QLabel[role="eyebrow"] { color: #8e9ba9; font-size: 10px; font-weight: 600; }
        QLabel[role="title"] { font-size: 19px; font-weight: 600; color: #edf1f5; }
        QSplitter::handle { background: #111418; }
        QListWidget { border: 1px solid #3a414b; border-radius: 4px; padding: 6px; }
        QListWidget::item { padding: 14px; border-bottom: 1px solid #303740; }
        QListWidget::item:selected { background: #36525b; }
    )");
}
