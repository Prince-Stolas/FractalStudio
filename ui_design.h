#pragma once
#include <QtWidgets>

inline QIcon studioIcon(const QString &name) {
    QIcon icon;
    for (int size : {24, 48}) {
        QPixmap image(size, size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 24.0, size / 24.0);
        painter.setPen(QPen(QColor("#c8c8c8"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        if (name == "split" || name == "splitVertical") {
            painter.drawRect(QRectF(4, 5, 16, 14));
            if (name == "split")
                painter.drawLine(12, 5, 12, 19);
            else
                painter.drawLine(4, 12, 20, 12);
        } else if (name == "close") {
            painter.drawLine(7, 7, 17, 17);
            painter.drawLine(17, 7, 7, 17);
        } else if (name == "reset") {
            QPainterPath path;
            path.moveTo(5, 9);
            path.cubicTo(8, 3, 19, 5, 19, 12);
            path.cubicTo(19, 18, 12, 21, 7, 17);
            painter.drawPath(path);
            painter.drawPolyline(QPolygonF{QPointF(5, 4), QPointF(5, 9), QPointF(10, 9)});
        } else if (name == "full") {
            for (int x : {4, 20}) {
                for (int y : {4, 20}) {
                    painter.drawLine(x, y, x == 4 ? 9 : 15, y);
                    painter.drawLine(x, y, x, y == 4 ? 9 : 15);
                }
            }
        } else if (name == "open") {
            painter.drawPolyline(QPolygonF{QPointF(4, 18), QPointF(4, 6), QPointF(10, 6),
                QPointF(12, 9), QPointF(20, 9), QPointF(17, 18), QPointF(4, 18),
                QPointF(7, 11), QPointF(19, 11)});
        } else if (name == "save") {
            painter.drawRoundedRect(QRectF(5, 4, 14, 16), 1, 1);
            painter.drawRect(QRectF(8, 4, 7, 6));
            painter.drawRect(QRectF(8, 14, 8, 6));
        } else if (name == "preset") {
            for (int x : {5, 14})
                for (int y : {5, 14})
                    painter.drawRect(QRectF(x, y, 5, 5));
        } else {
            painter.drawLine(5, 7, 19, 7);
            painter.drawLine(5, 17, 19, 17);
            painter.drawEllipse(QPointF(9, 7), 2, 2);
            painter.drawEllipse(QPointF(15, 17), 2, 2);
        }
        painter.end();
        icon.addPixmap(image);
    }
    return icon;
}

inline void applyStudioTheme(QApplication &app) {
    app.setStyle("Fusion");
    app.setFont(QFont("Segoe UI", 9));
    QPalette colors;
    colors.setColor(QPalette::Window, QColor("#282828"));
    colors.setColor(QPalette::WindowText, QColor("#dedede"));
    colors.setColor(QPalette::Base, QColor("#202020"));
    colors.setColor(QPalette::AlternateBase, QColor("#303030"));
    colors.setColor(QPalette::Text, QColor("#dedede"));
    colors.setColor(QPalette::Button, QColor("#353535"));
    colors.setColor(QPalette::ButtonText, QColor("#dedede"));
    colors.setColor(QPalette::Highlight, QColor("#585858"));
    colors.setColor(QPalette::HighlightedText, Qt::white);
    colors.setColor(QPalette::Disabled, QPalette::Text, QColor("#777777"));
    app.setPalette(colors);
    app.setStyleSheet(R"(
        QMainWindow::separator { background: #1a1a1a; width: 3px; height: 3px; }
        QToolBar { background: #2b2b2b; border: 0; border-bottom: 1px solid #181818; spacing: 3px; padding: 2px 6px; }
        QToolButton { border: 1px solid transparent; border-radius: 2px; padding: 4px 6px; }
        QToolButton:hover, QPushButton:hover { background: #414141; border-color: #5a5a5a; }
        QDockWidget::title { background: #303030; padding: 6px; font-weight: 500; }
        QTabWidget::pane { border: 0; border-top: 1px solid #444444; }
        QTabBar::tab { padding: 8px 12px; color: #aaaaaa; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: #eeeeee; border-bottom-color: #bbbbbb; background: #333333; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #202020; border: 1px solid #454545; border-radius: 2px; min-height: 23px; padding: 2px 5px; selection-background-color: #585858; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #888888; }
        QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 18px; border: 0; border-left: 1px solid #3c3c3c; }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/ui/icons/chevron-up.svg); width: 10px; height: 6px; }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/ui/icons/chevron-down.svg); width: 10px; height: 6px; }
        QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 18px; border: 0; border-left: 1px solid #3c3c3c; }
        QPushButton { background: #353535; border: 1px solid #4c4c4c; border-radius: 2px; padding: 6px 10px; }
        QPushButton[primary="true"] { background: #4a4a4a; border-color: #777777; }
        QScrollArea { border: 0; }
        QMenuBar { background: #222222; padding: 0; }
        QMenuBar::item { padding: 4px 9px; }
        QMenu { padding: 4px; border: 1px solid #505050; }
        QMenu::item { padding: 6px 24px; }
        QStatusBar { background: #222222; color: #aaaaaa; border-top: 1px solid #3c3c3c; }
        QLabel[role="eyebrow"] { color: #999999; font-size: 10px; font-weight: 500; }
        QLabel[role="title"] { font-size: 15px; font-weight: 600; color: #eeeeee; }
        QSplitter::handle { background: #111111; }
        QListWidget { border: 1px solid #454545; border-radius: 2px; padding: 4px; }
        QListWidget::item { padding: 10px; border-bottom: 1px solid #3b3b3b; }
        QListWidget::item:selected { background: #484848; }
    )");
}
