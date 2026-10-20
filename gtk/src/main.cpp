#include "ui/themewatcher.h"
#include "ui/window.h"

#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/application.h>

#include <memory>

namespace {

// Smoke-test hook: with DNFHELPER_GTK_SNAPSHOT=<folder>, every visible window is rendered
// to <folder>/<window title>.png once the app has settled, and the application quits.
void saveSnapshot(GtkWidget *widget, const std::string &path)
{
    const int width = gtk_widget_get_width(widget);
    const int height = gtk_widget_get_height(widget);
    GdkPaintable *paintable = gtk_widget_paintable_new(widget);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot), width, height);
    if (GskRenderNode *node = gtk_snapshot_free_to_node(snapshot)) {
        GskRenderer *renderer = gtk_native_get_renderer(GTK_NATIVE(widget));
        const graphene_rect_t bounds = GRAPHENE_RECT_INIT(
            0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        GdkTexture *texture = gsk_renderer_render_texture(renderer, node, &bounds);
        gdk_texture_save_to_png(texture, path.c_str());
        g_object_unref(texture);
        gsk_render_node_unref(node);
    }
    g_object_unref(paintable);
}

void scheduleSnapshot(const Glib::RefPtr<Gtk::Application> &application)
{
    const std::string folder = Glib::getenv("DNFHELPER_GTK_SNAPSHOT");
    if (folder.empty())
        return;
    Glib::signal_timeout().connect_once(
        [application, folder] {
            GListModel *toplevels = gtk_window_get_toplevels();
            for (guint i = 0; i < g_list_model_get_n_items(toplevels); ++i) {
                auto *window = static_cast<GtkWidget *>(g_list_model_get_item(toplevels, i));
                const char *title = gtk_window_get_title(GTK_WINDOW(window));
                std::string name = title && *title ? title : "window-" + std::to_string(i);
                for (char &c : name) {
                    if (!g_ascii_isalnum(c))
                        c = '-';
                }
                if (gtk_widget_get_visible(window) && gtk_widget_get_realized(window))
                    saveSnapshot(window, Glib::build_filename(folder, name + ".png"));
                g_object_unref(window);
            }
            application->quit();
        },
        3000);
}

} // namespace

int main(int argc, char *argv[])
{
    // Snapshot runs must not hand over to an already running instance.
    auto flags = Gio::Application::Flags::DEFAULT_FLAGS;
    if (!Glib::getenv("DNFHELPER_GTK_SNAPSHOT").empty())
        flags |= Gio::Application::Flags::NON_UNIQUE;
    auto application = Gtk::Application::create("com.linuxgamerlife.lgl-dnf-helper", flags);
    std::unique_ptr<dnfhelper::Window> window;
    const auto ensureWindow = [&]() -> dnfhelper::Window & {
        if (!window)
            window = std::make_unique<dnfhelper::Window>(application);
        return *window;
    };

    application->signal_startup().connect([&] {
        Glib::set_application_name("LGL DNF Helper");
        // Installed builds also get the icon from the desktop file's Icon= entry.
        Gtk::Window::set_default_icon_name("lgl-dnf-helper");
        dnfhelper::installApplicationStylesheet();
        application->add_action("quit", [&] {
            if (window)
                window->set_visible(false);
            application->quit();
        });
    });
    application->signal_activate().connect([&] {
        ensureWindow().present();
        scheduleSnapshot(application);
    });

    const int status = application->run(argc, argv);
    window.reset();
    return status;
}
