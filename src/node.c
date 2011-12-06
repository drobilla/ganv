/* This file is part of Ganv.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Ganv is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Ganv is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ganv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libgnomecanvas/libgnomecanvas.h>

#include "ganv/canvas.h"
#include "ganv/node.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./ganv-private.h"
#include "./gettext.h"

G_DEFINE_TYPE(GanvNode, ganv_node, GNOME_TYPE_CANVAS_GROUP)

static GnomeCanvasGroupClass* parent_class;

enum {
	PROP_0,
	PROP_PARTNER,
	PROP_LABEL,
	PROP_DASH_LENGTH,
	PROP_DASH_OFFSET,
	PROP_BORDER_WIDTH,
	PROP_FILL_COLOR,
	PROP_BORDER_COLOR,
	PROP_CAN_TAIL,
	PROP_CAN_HEAD,
	PROP_SELECTED,
	PROP_HIGHLIGHTED,
	PROP_DRAGGABLE
};

static gboolean
on_event(GanvNode* node, GdkEvent* event)
{
	return GANV_NODE_GET_CLASS(node)->on_event(node, event);
}

static void
ganv_node_init(GanvNode* node)
{
	node->partner      = NULL;
	node->label        = NULL;
	node->dash_length  = 0.0;
	node->dash_offset  = 0.0;
	node->border_width = 2.0;
	node->fill_color   = DEFAULT_FILL_COLOR;
	node->border_color = DEFAULT_BORDER_COLOR;
	node->can_tail     = FALSE;
	node->can_head     = FALSE;
	node->selected     = FALSE;
	node->highlighted  = FALSE;
	node->draggable    = FALSE;

	g_signal_connect(G_OBJECT(node),
	                 "event", G_CALLBACK(on_event), node);
}

static void
ganv_node_realize(GnomeCanvasItem* item)
{
	GNOME_CANVAS_ITEM_CLASS(parent_class)->realize(item);
	ganv_canvas_add_node(GANV_CANVAS(item->canvas),
	                     GANV_NODE(item));
}

static void
ganv_node_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_NODE(object));

	GanvNode* node = GANV_NODE(object);
	if (node->label) {
		gtk_object_destroy(GTK_OBJECT(node->label));
		node->label = NULL;
	}

	GnomeCanvasItem* item = GNOME_CANVAS_ITEM(object);
	ganv_node_disconnect(node);
	if (item->canvas) {
		ganv_canvas_remove_node(GANV_CANVAS(item->canvas), node);
	}

	node->partner = NULL;

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_node_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_NODE(object));

	GanvNode* node = GANV_NODE(object);

	switch (prop_id) {
		SET_CASE(PARTNER, object, node->partner);
		SET_CASE(DASH_LENGTH, double, node->dash_length);
		SET_CASE(DASH_OFFSET, double, node->dash_offset);
		SET_CASE(BORDER_WIDTH, double, node->border_width);
		SET_CASE(FILL_COLOR, uint, node->fill_color);
		SET_CASE(BORDER_COLOR, uint, node->border_color);
		SET_CASE(CAN_TAIL, boolean, node->can_tail)
			SET_CASE(CAN_HEAD, boolean, node->can_head)
			SET_CASE(SELECTED, boolean, node->selected)
			SET_CASE(HIGHLIGHTED, boolean, node->highlighted)
			SET_CASE(DRAGGABLE, boolean, node->draggable)
	case PROP_LABEL:
		ganv_node_set_label(node, g_value_get_string(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_node_get_property(GObject*    object,
                       guint       prop_id,
                       GValue*     value,
                       GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_NODE(object));

	GanvNode* node = GANV_NODE(object);

	typedef char* gstring;

	switch (prop_id) {
		GET_CASE(PARTNER, object, node->partner);
		GET_CASE(LABEL, string, node->label->text);
		GET_CASE(DASH_LENGTH, double, node->dash_length);
		GET_CASE(DASH_OFFSET, double, node->dash_offset);
		GET_CASE(BORDER_WIDTH, double, node->border_width);
		GET_CASE(FILL_COLOR, uint, node->fill_color);
		GET_CASE(BORDER_COLOR, uint, node->border_color);
		GET_CASE(CAN_TAIL, boolean, node->can_tail)
			GET_CASE(CAN_HEAD, boolean, node->can_head)
			GET_CASE(SELECTED, boolean, node->selected)
			GET_CASE(HIGHLIGHTED, boolean, node->highlighted)
			GET_CASE(DRAGGABLE, boolean, node->draggable)
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_node_default_tail_vector(const GanvNode* self,
                              const GanvNode* head,
                              double*         x,
                              double*         y,
                              double*         dx,
                              double*         dy)
{
	g_object_get(G_OBJECT(self),
	             "x", x,
	             "y", y,
	             NULL);

	*dx = 1.0;
	*dy = 0.0;
	gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(self)->parent, x, y);
}

static void
ganv_node_default_head_vector(const GanvNode* self,
                              const GanvNode* tail,
                              double*         x,
                              double*         y,
                              double*         dx,
                              double*         dy)
{
	g_object_get(G_OBJECT(self),
	             "x", x,
	             "y", y,
	             NULL);

	*dx = -1.0;
	*dy = 0.0;
	gnome_canvas_item_i2w(GNOME_CANVAS_ITEM(self)->parent, x, y);
}

void
ganv_node_get_draw_properties(const GanvNode* node,
                              double*         dash_length,
                              double*         border_color,
                              double*         fill_color)
{
	*dash_length  = node->dash_length;
	*border_color = node->border_color;
	*fill_color   = node->fill_color;

	if (node->selected) {
		*dash_length  = 4.0;
		*border_color = highlight_color(node->border_color, 0x20);
	}

	if (node->highlighted) {
		*fill_color   = highlight_color(node->fill_color, 0x20);
		*border_color = highlight_color(node->border_color, 0x20);
	}
}

const char*
ganv_node_get_label(const GanvNode* node)
{
	return node->label ? node->label->text : NULL;
}

void
ganv_node_set_label(GanvNode* node, const char* str)
{
	if (str[0] == '\0' || !str) {
		if (node->label) {
			gtk_object_destroy(GTK_OBJECT(node->label));
			node->label = NULL;
		}
	} else if (node->label) {
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(node->label),
		                      "text", str,
		                      NULL);
	} else {
		node->label = GANV_TEXT(gnome_canvas_item_new(
			                        GNOME_CANVAS_GROUP(node),
			                        ganv_text_get_type(),
			                        "text", str,
			                        "color", 0xFFFFFFFF,
			                        NULL));
	}

	GanvNodeClass* klass = GANV_NODE_GET_CLASS(node);
	if (klass->resize) {
		klass->resize(node);
	}
}

static void
ganv_node_default_tick(GanvNode* self,
                       double          seconds)
{
	GanvNode* node = GANV_NODE(self);
	node->dash_offset = seconds * 8.0;
	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(self));
}

static void
ganv_node_default_disconnect(GanvNode* node)
{
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(node)->canvas);
	if (canvas) {
		ganv_canvas_for_each_edge_on(canvas, node, ganv_edge_remove);
	}
}

static void
ganv_node_default_move(GanvNode* node,
                       double    dx,
                       double    dy)
{
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(node)->canvas);
	gnome_canvas_item_move(GNOME_CANVAS_ITEM(node), dx, dy);
	ganv_canvas_for_each_edge_on(canvas, node,
	                             ganv_edge_update_location);

}

static void
ganv_node_default_move_to(GanvNode* node,
                          double    x,
                          double    y)
{
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(node)->canvas);
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(node),
	                      "x", x,
	                      "y", y,
	                      NULL);
	if (node->can_tail) {
		ganv_canvas_for_each_edge_from(
			canvas, node, ganv_edge_update_location);
	} else if (node->can_head) {
		ganv_canvas_for_each_edge_to(
			canvas, node, ganv_edge_update_location);
	}
}

static gboolean
ganv_node_default_on_event(GanvNode* node,
                           GdkEvent* event)
{
	GanvCanvas* canvas = GANV_CANVAS(GNOME_CANVAS_ITEM(node)->canvas);

	// FIXME: put these somewhere better
	static double last_x, last_y;
	static double drag_start_x, drag_start_y;
	static gboolean dragging = FALSE;

	/*
	  double click_x,
	  gnome_canvas_item_w2i(GNOME_CANVAS_ITEM(GNOME_CANVAS_ITEM(node)->parent),
	  &click_x, &click_y);
	*/

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(node),
		                      "highlighted", TRUE, NULL);
		return TRUE;

	case GDK_LEAVE_NOTIFY:
		gnome_canvas_item_set(GNOME_CANVAS_ITEM(node),
		                      "highlighted", FALSE, NULL);
		return TRUE;

	case GDK_BUTTON_PRESS:
		drag_start_x = event->button.x;
		drag_start_y = event->button.y;
		last_x       = event->button.x;
		last_y       = event->button.y;
		if (!canvas->locked && node->draggable && event->button.button == 1) {
			gnome_canvas_item_grab(
				GNOME_CANVAS_ITEM(node),
				GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK|GDK_BUTTON_PRESS_MASK,
				ganv_canvas_get_move_cursor(canvas),
				event->button.time);
			dragging = TRUE;
			return TRUE;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (dragging) {
			gboolean selected;
			g_object_get(G_OBJECT(node), "selected", &selected, NULL);
			gnome_canvas_item_ungrab(GNOME_CANVAS_ITEM(node), event->button.time);
			dragging = FALSE;
			if (event->button.x != drag_start_x || event->button.y != drag_start_y) {
				// Dragged
				// FIXME: emit moved signal
				ganv_canvas_selection_move_finished(canvas);
			} else {
				// Clicked
				if (selected) {
					ganv_canvas_unselect_node(canvas, node);
				} else {
					if (!(event->button.state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK))) {
						ganv_canvas_clear_selection(canvas);
					}
					ganv_canvas_select_node(canvas, node);
				}

			}
			return TRUE;
		}
		break;

	case GDK_MOTION_NOTIFY:
		if ((dragging && (event->motion.state & GDK_BUTTON1_MASK))) {
			gboolean selected;
			g_object_get(G_OBJECT(node), "selected", &selected, NULL);

			double new_x = event->motion.x;
			double new_y = event->motion.y;

			if (event->motion.is_hint) {
				int t_x;
				int t_y;
				GdkModifierType state;
				gdk_window_get_pointer(event->motion.window, &t_x, &t_y, &state);
				new_x = t_x;
				new_y = t_y;
			}

			const double dx = new_x - last_x;
			const double dy = new_y - last_y;
			if (selected) {
				ganv_canvas_move_selected_items(canvas, dx, dy);
			} else {
				ganv_node_move(node, dx, dy);
			}

			last_x = new_x;
			last_y = new_y;
			return TRUE;
		}

	default:
		break;
	}

	return FALSE;
}

static void
ganv_node_class_init(GanvNodeClass* class)
{
	GObjectClass*         gobject_class = (GObjectClass*)class;
	GtkObjectClass*       object_class  = (GtkObjectClass*)class;
	GnomeCanvasItemClass* item_class    = (GnomeCanvasItemClass*)class;

	parent_class = GNOME_CANVAS_GROUP_CLASS(g_type_class_peek_parent(class));

	gobject_class->set_property = ganv_node_set_property;
	gobject_class->get_property = ganv_node_get_property;

	g_object_class_install_property(
		gobject_class, PROP_PARTNER, g_param_spec_object(
			"partner",
			_("partner"),
			_("\
Partners are nodes that should be visually aligned to correspond to each \
other, even if they are not necessarily connected (e.g. for separate modules \
representing the inputs and outputs of a single thing).  When the canvas is \
arranged, the partner will be aligned as if there was an edge from this node \
to its partner."),
			GANV_TYPE_NODE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_LABEL, g_param_spec_string(
			"label",
			_("label"),
			_("the text to display"),
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_LENGTH, g_param_spec_double(
			"dash-length",
			_("border dash length"),
			_("length of dashes, or zero for no dashing"),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_OFFSET, g_param_spec_double(
			"dash-offset",
			_("border dash offset"),
			_("offset for dashes (useful for 'rubber band' animation)."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_BORDER_WIDTH, g_param_spec_double(
			"border-width",
			_("border width"),
			_("width of the border around this node."),
			0.0, G_MAXDOUBLE,
			2.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_FILL_COLOR, g_param_spec_uint(
			"fill-color",
			_("fill color"),
			_("color of internal area"),
			0, G_MAXUINT,
			DEFAULT_FILL_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_BORDER_COLOR, g_param_spec_uint(
			"border-color",
			_("border color"),
			_("color of border area"),
			0, G_MAXUINT,
			DEFAULT_BORDER_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CAN_TAIL, g_param_spec_boolean(
			"can-tail",
			_("can tail"),
			_("whether this object can be the tail of an edge"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CAN_HEAD, g_param_spec_boolean(
			"can-head",
			_("can head"),
			_("whether this object can be the head of an edge"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_SELECTED, g_param_spec_boolean(
			"selected",
			_("selected"),
			_("whether this object is selected"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HIGHLIGHTED, g_param_spec_boolean(
			"highlighted",
			_("highlighted"),
			_("whether this object is highlighted"),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DRAGGABLE, g_param_spec_boolean(
			"draggable",
			_("draggable"),
			_("whether this object is draggable"),
			0,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_node_destroy;

	item_class->realize = ganv_node_realize;

	class->disconnect        = ganv_node_default_disconnect;
	class->move              = ganv_node_default_move;
	class->move_to           = ganv_node_default_move_to;
	class->tick              = ganv_node_default_tick;
	class->tail_vector       = ganv_node_default_tail_vector;
	class->head_vector       = ganv_node_default_head_vector;
	class->on_event          = ganv_node_default_on_event;
}
