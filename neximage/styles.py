"""Photoshop-inspired dark theme (Qt stylesheet on top of the Fusion style)."""

DARK_QSS = """
QWidget { background: #323232; color: #d4d4d4; font-size: 12px; }
QMainWindow { background: #262626; }
QMainWindow::separator { background: #1c1c1c; width: 3px; height: 3px; }
QMenuBar { background: #262626; border-bottom: 1px solid #1c1c1c; }
QMenuBar::item { background: transparent; padding: 4px 9px; }
QMenuBar::item:selected { background: #3d3d3d; }
QMenu { background: #2b2b2b; border: 1px solid #1c1c1c; padding: 3px 0; }
QMenu::item { padding: 4px 34px 4px 26px; }
QMenu::item:selected { background: #31659c; color: #ffffff; }
QMenu::item:disabled { color: #6b6b6b; }
QMenu::separator { height: 1px; background: #444; margin: 4px 6px; }
QMenu::indicator { width: 12px; height: 12px; left: 6px; }
QToolBar { background: #323232; border: none; spacing: 2px; padding: 2px; }
QToolBar#Toolbox { border-right: 1px solid #1c1c1c; padding: 4px 3px; }
QToolBar#OptionsBar { background: #3a3a3a; border-bottom: 1px solid #1c1c1c; min-height: 34px; }
QToolButton { background: transparent; border: 1px solid transparent; border-radius: 3px; padding: 3px; }
QToolButton:hover { background: #474747; border-color: #5a5a5a; }
QToolButton:checked, QToolButton:pressed { background: #5a5a5a; border-color: #2d6099; }
QDockWidget { titlebar-close-icon: none; }
QDockWidget::title { background: #2a2a2a; padding: 5px 8px; text-align: left; border-bottom: 1px solid #1c1c1c; }
QTabWidget::pane { border: none; background: #3a3a3a; }
QTabBar::tab { background: #2a2a2a; color: #a0a0a0; padding: 6px 14px; border: 1px solid #1c1c1c;
               border-bottom: none; margin-right: 1px; min-width: 90px; }
QTabBar::tab:selected { background: #424242; color: #ffffff; }
QTabBar::tab:hover:!selected { background: #333333; }
QTabBar::close-button { subcontrol-position: right; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QFontComboBox {
    background: #262626; border: 1px solid #171717; border-radius: 2px; padding: 2px 4px; min-height: 18px;
    selection-background-color: #31659c; }
QComboBox QAbstractItemView { background: #262626; border: 1px solid #171717; selection-background-color: #31659c; }
QComboBox::drop-down { border: none; width: 16px; }
QPushButton { background: #4a4a4a; border: 1px solid #1c1c1c; padding: 5px 14px; border-radius: 3px; }
QPushButton:hover { background: #555555; }
QPushButton:default { border: 1px solid #2d6099; }
QListWidget, QUndoView { background: #2b2b2b; border: 1px solid #1c1c1c; outline: none; }
QListWidget::item { padding: 3px; border-bottom: 1px solid #303030; }
QListWidget::item:selected { background: #31659c; color: #ffffff; }
QUndoView::item:selected { background: #31659c; }
QSlider::groove:horizontal { height: 4px; background: #1a1a1a; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #2d6099; border-radius: 2px; }
QSlider::handle:horizontal { background: #cfcfcf; width: 10px; margin: -5px 0; border-radius: 5px; }
QCheckBox::indicator { width: 13px; height: 13px; border: 1px solid #1c1c1c; background: #262626; border-radius: 2px; }
QCheckBox::indicator:checked { background: #31659c; }
QStatusBar { background: #262626; border-top: 1px solid #1c1c1c; }
QStatusBar::item { border: none; }
QLabel#OptHint { color: #8c8c8c; }
QLabel#OptLabel { color: #b5b5b5; }
QLabel#Welcome { color: #8c8c8c; font-size: 14px; }
QToolTip { background: #1e1e1e; color: #eeeeee; border: 1px solid #555555; padding: 3px; }
QScrollBar:vertical { background: #2a2a2a; width: 12px; }
QScrollBar::handle:vertical { background: #555555; border-radius: 5px; min-height: 20px; margin: 2px; }
QScrollBar:horizontal { background: #2a2a2a; height: 12px; }
QScrollBar::handle:horizontal { background: #555555; border-radius: 5px; min-width: 20px; margin: 2px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
"""
