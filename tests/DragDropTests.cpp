#include "gui/MainWindow.h"
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>
#include <iostream>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    QImage image(16, 12, QImage::Format_ARGB32);
    image.fill(Qt::red);
    const QString first = dir.filePath("ảnh một.png");
    const QString second = dir.filePath("second.PNG");
    CHECK(image.save(first, "PNG") && image.save(second, "PNG"));
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    app.processEvents();
    auto* canvas = window.findChild<Canvas*>();
    auto* history = window.findChild<QUndoStack*>();
    auto* list = window.findChild<LayersPanel*>()->findChild<QListWidget*>();
    CHECK(canvas && canvas->acceptDrops() && history && list);
    canvas->setZoom(0.5);
    const QPoint point(canvas->width() / 2 + 20, canvas->height() / 2 + 10);
    // Account for half pixels when the widget size is odd.
    const QPointF expected(960 + (point.x() - canvas->width() / 2.0) / 0.5,
                           540 + (point.y() - canvas->height() / 2.0) / 0.5);
    QMimeData files;
    files.setUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second),
                   QUrl::fromLocalFile(first), QUrl("https://example.com/not-downloaded.png")});
    QDragEnterEvent enter(point, Qt::CopyAction | Qt::MoveAction, &files,
                          Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &enter);
    CHECK(enter.isAccepted() && enter.dropAction() == Qt::CopyAction);
    QDragMoveEvent move(point, Qt::CopyAction, &files, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &move);
    CHECK(move.isAccepted());
    QDropEvent drop(point, Qt::CopyAction | Qt::MoveAction, &files, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &drop);
    CHECK(drop.isAccepted() && drop.dropAction() == Qt::CopyAction);
    CHECK(history->count() == 1 && list->count() == 2 && window.isWindowModified());
    CHECK(canvas->selectedLayer() && canvas->selectedLayer()->name == "second");
    CHECK(canvas->selectedLayer()->transform.center == expected);
    CHECK(QFile::exists(first) && QFile::exists(second));
    history->undo();
    CHECK(list->count() == 0 && !canvas->selectedLayer() && history->isClean());
    history->redo();
    CHECK(list->count() == 2);

    QMimeData remote;
    remote.setUrls({QUrl("https://example.com/image.png"), QUrl::fromLocalFile(dir.path())});
    QDragEnterEvent rejected(point, Qt::CopyAction, &remote, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &rejected);
    CHECK(!rejected.isAccepted());
    QMimeData text;
    text.setText("not an image");
    QDragEnterEvent rejectedText(point, Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &rejectedText);
    CHECK(!rejectedText.isAccepted());

    const QString corrupt = dir.filePath("corrupt.png");
    QFile bad(corrupt);
    CHECK(bad.open(QIODevice::WriteOnly));
    bad.write("invalid image");
    bad.close();
    QMimeData broken;
    broken.setUrls({QUrl::fromLocalFile(corrupt)});
    QDragEnterEvent brokenEnter(point, Qt::CopyAction, &broken, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &brokenEnter);
    QDropEvent brokenDrop(point, Qt::CopyAction, &broken, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &brokenDrop);
    CHECK(list->count() == 2 && history->count() == 1);
    auto* warning = window.findChild<QMessageBox*>();
    CHECK(warning && warning->text().contains("corrupt.png"));
    warning->close();
    return 0;
}
