#include "gui/MainWindow.h"
#include <QApplication>
#include <QAbstractButton>
#include <QCloseEvent>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QSlider>
#include <QTimer>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    MainWindow window;
    auto* history = window.findChild<QUndoStack*>();
    auto* canvas = window.findChild<Canvas*>();
    auto* layers = window.findChild<LayersPanel*>()->findChild<QListWidget*>();
    CHECK(history && canvas && layers);
    CHECK(!window.isWindowModified());
    CHECK(QMetaObject::invokeMethod(&window, "onAddShapeLayer", Qt::DirectConnection));
    CHECK(history->count() == 1 && layers->count() == 1);
    CHECK(window.isWindowModified());
    layers->setCurrentRow(0);
    Layer* selected = canvas->selectedLayer();
    CHECK(selected);
    history->undo();
    CHECK(!canvas->selectedLayer() && layers->count() == 0);
    CHECK(!window.isWindowModified());
    history->redo();
    CHECK(layers->count() == 1 && window.isWindowModified());
    layers->setCurrentRow(0);
    CHECK(canvas->selectedLayer() == selected);

    // Visibility changes rebuild the list: retrieve fresh items after each command.
    layers->item(0)->setCheckState(Qt::Unchecked);
    CHECK(!selected->visible);
    history->undo();
    CHECK(selected->visible && layers->item(0)->checkState() == Qt::Checked);
    CHECK(canvas->selectedLayer() == selected);

    // Simulate a keyboard/wheel value change (no sliderPressed/Released signals).
    auto* photo = window.findChild<PhotoAdjustmentsPanel*>();
    auto sliders = photo->findChildren<QSlider*>();
    CHECK(!sliders.isEmpty());
    QSlider* exposure = nullptr;
    for (auto* slider : sliders) {
        if (slider->minimum() == -500 && slider->maximum() == 500) {
            exposure = slider;
            break;
        }
    }
    CHECK(exposure);
    history->setClean();
    const auto before = selected->adjustments;
    exposure->setValue(exposure->value() + 10);
    CHECK(!history->isClean() && window.isWindowModified());
    CHECK(selected->adjustments != before);
    history->undo();
    CHECK(selected->adjustments == before && history->isClean());

    CHECK(QMetaObject::invokeMethod(&window, "onAddShapeLayer", Qt::DirectConnection));
    // Respond to modal dialogs without relying on screen coordinates or timing.
    bool sawPrompt = false;
    QTimer responder;
    QObject::connect(&responder, &QTimer::timeout, [&] {
        if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            sawPrompt = true;
            prompt->button(QMessageBox::Cancel)->click();
        }
    });
    responder.start(10);
    QCloseEvent cancelled;
    QApplication::sendEvent(&window, &cancelled);
    responder.stop();
    CHECK(sawPrompt && !cancelled.isAccepted() && window.isWindowModified());

    QObject::disconnect(&responder, nullptr, nullptr, nullptr);
    bool sawSaveDialog = false;
    QObject::connect(&responder, &QTimer::timeout, [&] {
        if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            prompt->button(QMessageBox::Save)->click();
        else if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            sawSaveDialog = true;
            dialog->reject();
        }
    });
    responder.start(10);
    QCloseEvent cancelledSave;
    QApplication::sendEvent(&window, &cancelledSave);
    responder.stop();
    CHECK(sawSaveDialog && !cancelledSave.isAccepted() && window.isWindowModified());

    QObject::disconnect(&responder, nullptr, nullptr, nullptr);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            prompt->button(QMessageBox::Discard)->click();
    });
    responder.start(10);
    QCloseEvent discarded;
    QApplication::sendEvent(&window, &discarded);
    responder.stop();
    CHECK(discarded.isAccepted());
    return 0;
}
