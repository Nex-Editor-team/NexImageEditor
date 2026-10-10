"""Application entry point."""
import sys

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication

from . import APP_NAME
from .mainwindow import MainWindow
from .styles import DARK_QSS


def main(argv=None) -> int:
    argv = sys.argv if argv is None else argv
    app = QApplication.instance() or QApplication(argv)
    app.setApplicationName(APP_NAME)
    app.setOrganizationName("Hyggshi OS Research & Technology")
    app.setStyle("Fusion")
    app.setStyleSheet(DARK_QSS)
    win = MainWindow(files=argv[1:])
    win.show()
    return app.exec()
