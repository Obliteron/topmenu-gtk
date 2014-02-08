#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include "../global.h"

#include "topmenu-client.h"

#define OBJECT_DATA_KEY_PLUG "topmenu-plug"

static gboolean handle_plug_delete(GtkPlug *plug, GdkEvent *event, GdkWindow *window)
{
	return TRUE; // Prevent deletion of plug window
}

static gboolean handle_widget_button_event(GtkWidget *widget, GdkEvent *event, GtkPlug *plug)
{
	if (event->type != GDK_BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE) {
		return FALSE;
	}
	if (event->button.button == 1) {
		return FALSE;
	}

	GdkWindow *socket = gtk_plug_get_socket_window(plug);
	if (socket) {
		GdkScreen *screen = gdk_window_get_screen(socket);
		GdkWindow *root = gdk_screen_get_root_window(screen);
		Display *dpy = GDK_WINDOW_XDISPLAY(socket);
		Window xwin = GDK_WINDOW_XWINDOW(socket);

		if (event->type == GDK_BUTTON_PRESS) {
			gdk_display_pointer_ungrab(gtk_widget_get_display(widget),
									   GDK_CURRENT_TIME);
		}

		XEvent e;
		long mask = event->type == GDK_BUTTON_PRESS ? ButtonPressMask : ButtonReleaseMask;
		e.type = event->type == GDK_BUTTON_PRESS ? ButtonPress : ButtonRelease;
		e.xbutton.window = xwin;
		e.xbutton.display = dpy;
		e.xbutton.root = GDK_WINDOW_XWINDOW(root);
		e.xbutton.time = event->button.time;
		e.xbutton.button = event->button.button;
		e.xbutton.state = event->button.state;
		e.xbutton.x = event->button.x;
		e.xbutton.y = event->button.y;
		e.xbutton.x_root = event->button.x_root;
		e.xbutton.y_root = event->button.y_root;
		e.xbutton.same_screen = True;

		gdk_error_trap_push();
		XSendEvent(dpy, xwin, True, mask, &e);
		g_debug("Forwarding button %d %s event to 0x%lx",
				e.xbutton.button,
				event->type == GDK_BUTTON_PRESS ? "press" : "release",
				xwin);
		gdk_flush();
		gdk_error_trap_pop();

		return TRUE;
	}

	return FALSE;
}

void topmenu_client_connect_window_widget(GdkWindow *window, GtkWidget *widget)
{
	Display *display = GDK_WINDOW_XDISPLAY(window);

	if (g_object_get_data(G_OBJECT(window), OBJECT_DATA_KEY_PLUG)) {
		topmenu_client_disconnect_window(window);
	}

	Window xwin = GDK_WINDOW_XID(window);
	GtkPlug *plug = GTK_PLUG(gtk_plug_new(0));
	gtk_container_add(GTK_CONTAINER(plug), widget);
	g_signal_connect_object(plug, "delete-event",
	                        G_CALLBACK(handle_plug_delete), window, 0);
	g_signal_connect_object(widget, "button-press-event",
		                        G_CALLBACK(handle_widget_button_event), plug, 0);
	g_signal_connect_object(widget, "button-release-event",
		                        G_CALLBACK(handle_widget_button_event), plug, 0);
	gtk_widget_show(GTK_WIDGET(plug));

	// Gtk+ should keep a reference to plug as it is a "top level" widget.
	// Otherwise I failed to parse the documentation.
	g_warn_if_fail(G_OBJECT(plug)->ref_count == 1);

	Window plug_xwin = gtk_plug_get_id(plug);

	Atom atom = XInternAtom(display, ATOM_TOPMENU_WINDOW, False);

	XChangeProperty(display, xwin, atom,
	                XA_WINDOW, 32, PropModeReplace,
	                (unsigned char*)&plug_xwin, 1);

	g_object_set_data_full(G_OBJECT(window), OBJECT_DATA_KEY_PLUG, plug,
	                       (GDestroyNotify) &gtk_widget_destroy);
}

void topmenu_client_disconnect_window(GdkWindow *window)
{
	Display *display = GDK_WINDOW_XDISPLAY(window);

	gpointer window_data = g_object_steal_data(G_OBJECT(window), OBJECT_DATA_KEY_PLUG);
	g_return_if_fail(window_data);

	Window xwin = GDK_WINDOW_XID(window);

	GtkPlug *plug = GTK_PLUG(window_data);
	g_return_if_fail(plug);

	Atom atom = XInternAtom(display, ATOM_TOPMENU_WINDOW, False);

	XDeleteProperty(display, xwin, atom);

	g_warn_if_fail(G_OBJECT(plug)->ref_count == 1);
	gtk_widget_destroy(plug);
}