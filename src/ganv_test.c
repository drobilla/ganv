/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#include "ganv/ganv.h"

static void
on_window_destroy(GtkWidget* widget, gpointer data)
{
	gtk_main_quit();
}

static void
on_connect(GanvCanvas* canvas, GanvNode* tail, GanvNode* head, void* data)
{
	ganv_edge_new(canvas, tail, head, "color", 0xFFFFFFFF, NULL);
}

static void
on_disconnect(GanvCanvas* canvas, GanvNode* tail, GanvNode* head, void* data)
{
	ganv_canvas_remove_edge_between(canvas, tail, head);
}

int
main(int argc, char** argv)
{
	gtk_init(&argc, &argv);

	GtkWindow* win = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_title(win, "Ganv Test");
	g_signal_connect(win, "destroy",
	                 G_CALLBACK(on_window_destroy), NULL);

	GanvCanvas* canvas = ganv_canvas_new(1024, 768);
	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(canvas));

	GanvModule* module = ganv_module_new(canvas,
	                                     "x", 10.0,
	                                     "y", 10.0,
	                                     "draggable", TRUE,
	                                     "label", "test",
	                                     NULL);

	GanvPort* port = ganv_port_new(module, FALSE,
	                               "label", "Signal",
	                               NULL);

	GanvPort* cport = ganv_port_new(module, TRUE,
	                                "label", "Control",
	                                NULL);
	ganv_port_show_control(cport);

	//GtkWidget* entry = gtk_entry_new();
	//ganv_module_embed(module, entry);

	GanvPort* tport = ganv_port_new(module, TRUE,
	                                "label", "Toggle",
	                                NULL);
	ganv_port_show_control(tport);
	ganv_port_set_control_is_toggle(tport, TRUE);

	ganv_item_show(GANV_ITEM(module));
	ganv_item_raise_to_top(GANV_ITEM(module));

	GanvModule* module2 = ganv_module_new(canvas,
	                                      "x", 200.0,
	                                      "y", 10.0,
	                                      "draggable", TRUE,
	                                      "label", "test2",
	                                      NULL);

	GanvPort* port2 = ganv_port_new(module2, TRUE,
	                                "label", "Signal",
	                                NULL);

	g_signal_connect(canvas, "connect",
	                 G_CALLBACK(on_connect), canvas);

	g_signal_connect(canvas, "disconnect",
	                 G_CALLBACK(on_disconnect), canvas);

	ganv_item_show(GANV_ITEM(module2));
	ganv_item_raise_to_top(GANV_ITEM(module2));

	gtk_widget_show_all(GTK_WIDGET(win));
	gtk_window_present(win);
	gtk_main();

	return 0;
}
