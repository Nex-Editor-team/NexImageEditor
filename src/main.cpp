#include "ui/MainWindow.h"

#include <gtkmm.h>

#include <memory>

int main(int argc, char** argv) {
    auto app = Gtk::Application::create("org.neximageeditor.App");
    std::unique_ptr<MainWindow> window;
    app->signal_activate().connect([&] {
        if (!window) {
            window = std::make_unique<MainWindow>(*app);
            app->add_window(*window);
        }
        window->present();
    });
    return app->run(argc, argv);
}
