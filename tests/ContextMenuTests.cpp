#include "gui/MainWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QListWidget>
#include <QMenu>
#include <QTimer>
#include <cstdlib>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

static QAction* action(QMenu* menu, const QString& text) {
    for (auto* a : menu->actions()) if (a->text() == text) return a;
    return nullptr;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    // Bound modal-menu failures even when running outside CTest.
    QTimer::singleShot(15000, [] { std::cerr << "Context menu test timed out\n"; std::exit(1); });
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    app.processEvents();
    QApplication::clipboard()->clear();
    auto* canvas = window.findChild<Canvas*>();
    auto* history = window.findChild<QUndoStack*>();
    auto* list = window.findChild<LayersPanel*>()->findChild<QListWidget*>();
    CHECK(canvas && history && list);
    CHECK(QMetaObject::invokeMethod(&window, "onAddShapeLayer", Qt::DirectConnection));
    CHECK(QMetaObject::invokeMethod(&window, "onAddShapeLayer", Qt::DirectConnection));
    list->setCurrentRow(1); // Deliberately select bottom layer first.
    Layer* bottom = canvas->selectedLayer();
    CHECK(bottom);

    bool checked = false;
    QTimer::singleShot(0, [&] {
        auto* menu = window.findChild<QMenu*>("layerContextMenu");
        if (!menu) return;
        checked = canvas->selectedLayer() != bottom
            && action(menu, "Duplicate Layer")->isEnabled()
            && !action(menu, "Move Layer Up")->isEnabled()
            && action(menu, "Move Layer Down")->isEnabled()
            && !action(menu, "Paste Layer")->isEnabled()
            && !action(menu, "Paste Image from System Clipboard")->isEnabled();
        action(menu, "Duplicate Layer")->trigger();
        menu->close();
    });
    const QPoint center = canvas->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, center, canvas->mapToGlobal(center));
    QApplication::sendEvent(canvas, &event);
    CHECK(checked && list->count() == 3);
    history->undo();
    CHECK(list->count() == 2);

    // Layer-list menu targets the clicked row, not the prior selection.
    checked = false;
    QTimer::singleShot(0, [&] {
        auto* menu = window.findChild<QMenu*>("layerContextMenu");
        if (!menu) return;
        checked = canvas->selectedLayer() == bottom && !action(menu, "Move Layer Down")->isEnabled();
        action(menu, "Hide Layer")->trigger();
        menu->close();
    });
    const QPoint bottomPos = list->visualItemRect(list->item(1)).center();
    CHECK(QMetaObject::invokeMethod(list, "customContextMenuRequested", Qt::DirectConnection,
                                   Q_ARG(QPoint, bottomPos)));
    CHECK(checked && !bottom->visible);
    history->undo();
    CHECK(bottom->visible);

    // Keyboard context menu preserves the selected target and Copy does not edit.
    const int indexBefore = history->index();
    checked = false;
    QTimer::singleShot(0, [&] {
        auto* menu = window.findChild<QMenu*>("layerContextMenu");
        if (!menu) return;
        checked = canvas->selectedLayer() == bottom;
        action(menu, "Copy Layer")->trigger();
        menu->close();
    });
    QContextMenuEvent keyboard(QContextMenuEvent::Keyboard, QPoint(), QPoint());
    QApplication::sendEvent(canvas, &keyboard);
    CHECK(checked && history->index() == indexBefore);

    // Empty-space menu clears selection, disables destructive actions, allows Paste.
    checked = false;
    QTimer::singleShot(0, [&] {
        auto* menu = window.findChild<QMenu*>("layerContextMenu");
        if (!menu) return;
        checked = !canvas->selectedLayer() && list->currentRow() == -1
            && !action(menu, "Delete Layer")->isEnabled()
            && !action(menu, "Transform")->isEnabled()
            && action(menu, "Paste Layer")->isEnabled();
        action(menu, "Paste Layer")->trigger();
        menu->close();
    });
    QContextMenuEvent empty(QContextMenuEvent::Mouse, QPoint(-20, -20), canvas->mapToGlobal(QPoint(5, 5)));
    QApplication::sendEvent(canvas, &empty);
    CHECK(checked && list->count() == 3 && canvas->selectedLayer());
    history->undo();
    CHECK(list->count() == 2);
    return 0;
}
