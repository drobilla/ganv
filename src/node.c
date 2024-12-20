/* This file is part of Ganv.
 * Copyright 2007-2016 David Robillard <http://drobilla.net>
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

#include "boilerplate.h"
#include "color.h"
#include "ganv-marshal.h"
#include "ganv-private.h"
#include "gettext.h"

#include <ganv/canvas.h>
#include <ganv/edge.h>
#include <ganv/item.h>
#include <ganv/node.h>
#include <ganv/text.h>
#include <ganv/types.h>

#include <cairo.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <math.h>
#include <stddef.h>

guint signal_moved;

G_DEFINE_TYPE_WITH_CODE(GanvNode, ganv_node, GANV_TYPE_ITEM,
                        G_ADD_PRIVATE(GanvNode))

static GanvItemClass* parent_class;

enum {
	PROP_0,
	PROP_CANVAS,
	PROP_PARTNER,
	PROP_LABEL,
	PROP_SHOW_LABEL,
	PROP_DASH_LENGTH,
	PROP_DASH_OFFSET,
	PROP_BORDER_WIDTH,
	PROP_FILL_COLOR,
	PROP_BORDER_COLOR,
	PROP_CAN_TAIL,
	PROP_CAN_HEAD,
	PROP_IS_SOURCE,
	PROP_SELECTED,
	PROP_HIGHLIGHTED,
	PROP_DRAGGABLE,
	PROP_GRABBED
};

static void
ganv_node_init(GanvNode* node)
{
	GanvNodePrivate* impl =
	    (GanvNodePrivate*)ganv_node_get_instance_private(node);

	node->impl = impl;

	impl->partner      = NULL;
	impl->label        = NULL;
	impl->dash_length  = 0.0;
	impl->dash_offset  = 0.0;
	impl->border_width = 2.0;
	impl->fill_color   = DEFAULT_FILL_COLOR;
	impl->border_color = DEFAULT_BORDER_COLOR;
	impl->can_tail     = FALSE;
	impl->can_head     = FALSE;
	impl->is_source    = FALSE;
	impl->selected     = FALSE;
	impl->highlighted  = FALSE;
	impl->draggable    = FALSE;
	impl->show_label   = TRUE;
	impl->grabbed      = FALSE;
	impl->must_resize  = FALSE;
#ifdef GANV_FDGL
	impl->force.x       = 0.0;
	impl->force.y       = 0.0;
	impl->vel.x         = 0.0;
	impl->vel.y         = 0.0;
	impl->connected     = FALSE;
#endif
}

static void
ganv_node_realize(GanvItem* item)
{
	GANV_ITEM_CLASS(parent_class)->realize(item);
	ganv_canvas_add_node(ganv_item_get_canvas(item), GANV_NODE(item));
}

static void
ganv_node_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_NODE(object));

	GanvNode*        node = GANV_NODE(object);
	GanvNodePrivate* impl = node->impl;
	if (impl->label) {
		g_object_unref(impl->label);
		impl->label = NULL;
	}

	GanvItem* item = GANV_ITEM(object);
	ganv_node_disconnect(node);
	if (item->impl->canvas) {
		ganv_canvas_remove_node(item->impl->canvas, node);
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}

	impl->partner      = NULL;
	item->impl->canvas = NULL;
}

/// Expands the canvas to contain a moved/resized node if necessary
static void
ganv_node_expand_canvas(GanvNode* node)
{
	GanvItem*   item   = GANV_ITEM(node);
	GanvCanvas* canvas = ganv_item_get_canvas(item);

	if (item->impl->parent == ganv_canvas_root(canvas)) {
		const double pad = 10.0;

		double x1 = 0.0;
		double y1 = 0.0;
		double x2 = 0.0;
		double y2 = 0.0;
		ganv_item_get_bounds(item, &x1, &y1, &x2, &y2);

		ganv_item_i2w(item, &x1, &y1);
		ganv_item_i2w(item, &x2, &y2);

		double canvas_w = 0.0;
		double canvas_h = 0.0;
		ganv_canvas_get_size(canvas, &canvas_w, &canvas_h);
		if (x2 + pad > canvas_w || y2 + pad > canvas_h) {
			ganv_canvas_resize(canvas,
			                   MAX(x1 + pad, canvas_w),
			                   MAX(y2 + pad, canvas_h));
		}
	}
}

static void
ganv_node_update(GanvItem* item, int flags)
{
	GanvNode* node = GANV_NODE(item);
	if (node->impl->must_resize) {
		ganv_node_resize(node);
		node->impl->must_resize = FALSE;
	}

	if (node->impl->label) {
		ganv_item_invoke_update(GANV_ITEM(node->impl->label), flags);
	}

	GANV_ITEM_CLASS(parent_class)->update(item, flags);

	ganv_node_expand_canvas(node);
}

static void
ganv_node_draw(GanvItem* item,
               cairo_t* cr, double cx, double cy, double cw, double ch)
{
	(void)item;
	(void)cr;
	(void)cx;
	(void)cy;
	(void)cw;
	(void)ch;

	/* TODO: Label is not drawn here because ports need to draw control
	   rects then the label on top.  I can't see a way of solving this since
	   there's no single time parent class draw needs to be called, so perhaps
	   label shouldn't be part of this class... */
}

static void
ganv_node_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_NODE(object));

	GanvNode*        node = GANV_NODE(object);
	GanvNodePrivate* impl = node->impl;

	switch (prop_id) {
		SET_CASE(DASH_LENGTH, double, impl->dash_length)
		SET_CASE(DASH_OFFSET, double, impl->dash_offset)
		SET_CASE(BORDER_WIDTH, double, impl->border_width)
		SET_CASE(FILL_COLOR, uint, impl->fill_color)
		SET_CASE(BORDER_COLOR, uint, impl->border_color)
		SET_CASE(CAN_TAIL, boolean, impl->can_tail)
		SET_CASE(CAN_HEAD, boolean, impl->can_head)
		SET_CASE(IS_SOURCE, boolean, impl->is_source)
		SET_CASE(HIGHLIGHTED, boolean, impl->highlighted)
		SET_CASE(DRAGGABLE, boolean, impl->draggable)
		SET_CASE(GRABBED, boolean, impl->grabbed)
	case PROP_PARTNER:
		impl->partner = (GanvNode*)g_value_get_object(value);
		break;
	case PROP_SELECTED:
		if (impl->selected != g_value_get_boolean(value)) {
			GanvItem* item = GANV_ITEM(object);
			impl->selected = g_value_get_boolean(value);
			if (item->impl->canvas) {
				if (impl->selected) {
					ganv_canvas_select_node(ganv_item_get_canvas(item), node);
				} else {
					ganv_canvas_unselect_node(ganv_item_get_canvas(item), node);
				}
				ganv_item_request_update(item);
			}
		}
		break;
	case PROP_CANVAS:
		if (!GANV_ITEM(object)->impl->parent) {
			GanvCanvas* canvas = GANV_CANVAS(g_value_get_object(value));
			g_object_set(object, "parent", ganv_canvas_root(canvas), NULL);
			ganv_canvas_add_node(canvas, node);
		} else {
			g_warning("Cannot change `canvas' property after construction");
		}
		break;
	case PROP_LABEL:
		ganv_node_set_label(node, g_value_get_string(value));
		break;
	case PROP_SHOW_LABEL:
		ganv_node_set_show_label(node, g_value_get_boolean(value));
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

	GanvNode*        node = GANV_NODE(object);
	GanvNodePrivate* impl = node->impl;

	switch (prop_id) {
		GET_CASE(PARTNER, object, impl->partner)
		GET_CASE(LABEL, string, impl->label ? impl->label->impl->text : NULL)
		GET_CASE(DASH_LENGTH, double, impl->dash_length)
		GET_CASE(DASH_OFFSET, double, impl->dash_offset)
		GET_CASE(BORDER_WIDTH, double, impl->border_width)
		GET_CASE(FILL_COLOR, uint, impl->fill_color)
		GET_CASE(BORDER_COLOR, uint, impl->border_color)
		GET_CASE(CAN_TAIL, boolean, impl->can_tail)
		GET_CASE(CAN_HEAD, boolean, impl->can_head)
		GET_CASE(IS_SOURCE, boolean, impl->is_source)
		GET_CASE(SELECTED, boolean, impl->selected)
		GET_CASE(HIGHLIGHTED, boolean, impl->highlighted)
		GET_CASE(DRAGGABLE, boolean, impl->draggable)
		GET_CASE(GRABBED, boolean, impl->grabbed)
	case PROP_CANVAS:
		g_value_set_object(value, ganv_item_get_canvas(GANV_ITEM(object)));
		break;
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
	(void)head;

	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(self));

	*x = GANV_ITEM(self)->impl->x;
	*y = GANV_ITEM(self)->impl->y;

	switch (ganv_canvas_get_direction(canvas)) {
	case GANV_DIRECTION_RIGHT:
		*dx = 1.0;
		*dy = 0.0;
		break;
	case GANV_DIRECTION_DOWN:
		*dx = 0.0;
		*dy = 1.0;
		break;
	}

	ganv_item_i2w(GANV_ITEM(self)->impl->parent, x, y);
}

static void
ganv_node_default_head_vector(const GanvNode* self,
                              const GanvNode* tail,
                              double*         x,
                              double*         y,
                              double*         dx,
                              double*         dy)
{
	(void)tail;

	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(self));

	*x = GANV_ITEM(self)->impl->x;
	*y = GANV_ITEM(self)->impl->y;

	switch (ganv_canvas_get_direction(canvas)) {
	case GANV_DIRECTION_RIGHT:
		*dx = -1.0;
		*dy = 0.0;
		break;
	case GANV_DIRECTION_DOWN:
		*dx = 0.0;
		*dy = -1.0;
		break;
	}

	ganv_item_i2w(GANV_ITEM(self)->impl->parent, x, y);
}

void
ganv_node_get_draw_properties(const GanvNode* node,
                              double*         dash_length,
                              double*         border_color,
                              double*         fill_color)
{
	GanvNodePrivate* impl = node->impl;

	*dash_length  = impl->dash_length;
	*border_color = impl->border_color;
	*fill_color   = impl->fill_color;

	if (impl->selected) {
		*dash_length  = 4.0;
		*border_color = highlight_color(impl->border_color, 0x40);
	}

	if (impl->highlighted) {
		*border_color = highlight_color(impl->border_color, 0x40);
		*fill_color   = impl->fill_color;
	}
}

void
ganv_node_set_label(GanvNode* node, const char* str)
{
	GanvNodePrivate* impl = node->impl;
	if (!str || str[0] == '\0') {
		if (impl->label) {
			gtk_object_destroy(GTK_OBJECT(impl->label));
			impl->label = NULL;
		}
	} else if (impl->label) {
		ganv_item_set(GANV_ITEM(impl->label),
		              "text", str,
		              NULL);
	} else {
		impl->label = GANV_TEXT(ganv_item_new(GANV_ITEM(node),
		                                      ganv_text_get_type(),
		                                      "text", str,
		                                      "color", DEFAULT_TEXT_COLOR,
		                                      "managed", TRUE,
		                                      NULL));
	}

	impl->must_resize = TRUE;
	ganv_item_request_update(GANV_ITEM(node));
}

void
ganv_node_set_show_label(GanvNode* node, gboolean show)
{
	if (node->impl->label) {
		if (show) {
			ganv_item_show(GANV_ITEM(node->impl->label));
		} else {
			ganv_item_hide(GANV_ITEM(node->impl->label));
		}
	}
	node->impl->show_label = show;
	ganv_item_request_update(GANV_ITEM(node));
}

static void
ganv_node_default_tick(GanvNode* self,
                       double    seconds)
{
	GanvNode* node = GANV_NODE(self);
	node->impl->dash_offset = seconds * 8.0;
	ganv_item_request_update(GANV_ITEM(self));
}

static void
ganv_node_default_disconnect(GanvNode* node)
{
	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(node));
	if (canvas) {
		ganv_canvas_for_each_edge_on(
			canvas, node, (GanvEdgeFunc)ganv_edge_disconnect, NULL);
	}
}

static void
ganv_node_default_move(GanvNode* node,
                       double    dx,
                       double    dy)
{
	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(node));
	ganv_item_move(GANV_ITEM(node), dx, dy);
	ganv_canvas_for_each_edge_on(
		canvas, node, (GanvEdgeFunc)ganv_edge_update_location, NULL);
	ganv_item_request_update(GANV_ITEM(node));
}

static void
ganv_node_default_move_to(GanvNode* node,
                          double    x,
                          double    y)
{
	GanvItem*   item   = GANV_ITEM(node);
	GanvCanvas* canvas = ganv_item_get_canvas(item);
	item->impl->x = x;
	item->impl->y = y;
	if (node->impl->can_tail) {
		ganv_canvas_for_each_edge_from(
			canvas, node, (GanvEdgeFunc)ganv_edge_update_location, NULL);
	} else if (node->impl->can_head) {
		ganv_canvas_for_each_edge_to(
			canvas, node, (GanvEdgeFunc)ganv_edge_update_location, NULL);
	}
	ganv_item_request_update(GANV_ITEM(node));
}

static void
ganv_node_default_resize(GanvNode* node)
{
	GanvItem* item = GANV_ITEM(node);
	if (GANV_IS_NODE(item->impl->parent)) {
		ganv_node_resize(GANV_NODE(item->impl->parent));
	}
	node->impl->must_resize = FALSE;
}

static void
ganv_node_default_redraw_text(GanvNode* node)
{
	if (node->impl->label) {
		ganv_text_layout(node->impl->label);
		node->impl->must_resize = TRUE;
		ganv_item_request_update(GANV_ITEM(node));
	}
}

static gboolean
ganv_node_default_event(GanvItem* item,
                        GdkEvent* event)
{
	GanvNode*   node   = GANV_NODE(item);
	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(node));

	// FIXME: put these somewhere better
	static double   last_x       = NAN;
	static double   last_y       = NAN;
	static double   drag_start_x = NAN;
	static double   drag_start_y = NAN;
	static gboolean dragging     = FALSE;

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		ganv_item_raise(GANV_ITEM(node));
		node->impl->highlighted = TRUE;
		ganv_item_request_update(item);
		return TRUE;

	case GDK_LEAVE_NOTIFY:
		ganv_item_lower(GANV_ITEM(node));
		node->impl->highlighted = FALSE;
		ganv_item_request_update(item);
		return TRUE;

	case GDK_BUTTON_PRESS:
		drag_start_x = event->button.x;
		drag_start_y = event->button.y;
		last_x       = event->button.x;
		last_y       = event->button.y;
		if (!ganv_canvas_get_locked(canvas) && node->impl->draggable && event->button.button == 1) {
			ganv_canvas_grab_item(
				GANV_ITEM(node),
				GDK_POINTER_MOTION_MASK|GDK_BUTTON_RELEASE_MASK|GDK_BUTTON_PRESS_MASK,
				ganv_canvas_get_move_cursor(canvas),
				event->button.time);
			node->impl->grabbed = TRUE;
			dragging = TRUE;
			return TRUE;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (dragging) {
			gboolean selected = FALSE;
			g_object_get(G_OBJECT(node), "selected", &selected, NULL);
			ganv_canvas_ungrab_item(GANV_ITEM(node), event->button.time);
			node->impl->grabbed = FALSE;
			dragging = FALSE;
			if (event->button.x != drag_start_x || event->button.y != drag_start_y) {
				ganv_canvas_contents_changed(canvas);
				if (selected) {
					ganv_canvas_selection_move_finished(canvas);
				} else {
					const double x = GANV_ITEM(node)->impl->x;
					const double y = GANV_ITEM(node)->impl->y;
					g_signal_emit(node, signal_moved, 0, x, y, NULL);
				}
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
			gboolean selected = FALSE;
			g_object_get(G_OBJECT(node), "selected", &selected, NULL);

			double new_x = event->motion.x;
			double new_y = event->motion.y;

			if (event->motion.is_hint) {
				int             t_x   = 0;
				int             t_y   = 0;
				GdkModifierType state = (GdkModifierType)0;
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
ganv_node_class_init(GanvNodeClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;

	parent_class = GANV_ITEM_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_node_set_property;
	gobject_class->get_property = ganv_node_get_property;

	g_object_class_install_property(
		gobject_class, PROP_CANVAS, g_param_spec_object(
			"canvas",
			_("Canvas"),
			_("The canvas this node is on."),
			GANV_TYPE_CANVAS,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_PARTNER, g_param_spec_object(
			"partner",
			_("Partner"),
			_("Partners are nodes that should be visually aligned to correspond"
			  " to each other, even if they are not necessarily connected (e.g."
			  " for separate modules representing the inputs and outputs of a"
			  " single thing).  When the canvas is arranged, the partner will"
			  " be aligned as if there was an edge from this node to its"
			  " partner."),
			GANV_TYPE_NODE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_LABEL, g_param_spec_string(
			"label",
			_("Label"),
			_("The text to display as a label on this node."),
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_SHOW_LABEL, g_param_spec_boolean(
			"show-label",
			_("Show label"),
			_("Whether or not to show the label."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_LENGTH, g_param_spec_double(
			"dash-length",
			_("Border dash length"),
			_("Length of border dashes, or zero for no dashing."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DASH_OFFSET, g_param_spec_double(
			"dash-offset",
			_("Border dash offset"),
			_("Start offset for border dashes, used for selected animation."),
			0.0, G_MAXDOUBLE,
			0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_BORDER_WIDTH, g_param_spec_double(
			"border-width",
			_("Border width"),
			_("Width of the border line."),
			0.0, G_MAXDOUBLE,
			2.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_FILL_COLOR, g_param_spec_uint(
			"fill-color",
			_("Fill color"),
			_("Color of internal area."),
			0, G_MAXUINT,
			DEFAULT_FILL_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_BORDER_COLOR, g_param_spec_uint(
			"border-color",
			_("Border color"),
			_("Color of border line."),
			0, G_MAXUINT,
			DEFAULT_BORDER_COLOR,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CAN_TAIL, g_param_spec_boolean(
			"can-tail",
			_("Can tail"),
			_("Whether this node can be the tail of an edge."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_CAN_HEAD, g_param_spec_boolean(
			"can-head",
			_("Can head"),
			_("Whether this object can be the head of an edge."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_IS_SOURCE, g_param_spec_boolean(
			"is-source",
			_("Is source"),
			_("Whether this object should be positioned at the start of signal flow."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_SELECTED, g_param_spec_boolean(
			"selected",
			_("Selected"),
			_("Whether this object is selected."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_HIGHLIGHTED, g_param_spec_boolean(
			"highlighted",
			_("Highlighted"),
			_("Whether this object is highlighted."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_DRAGGABLE, g_param_spec_boolean(
			"draggable",
			_("Draggable"),
			_("Whether this object is draggable."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_GRABBED, g_param_spec_boolean(
			"grabbed",
			_("Grabbed"),
			_("Whether this object is grabbed by the user."),
			0,
			G_PARAM_READWRITE));

	signal_moved = g_signal_new("moved",
	                            ganv_node_get_type(),
	                            G_SIGNAL_RUN_FIRST,
	                            0, NULL, NULL,
	                            ganv_marshal_VOID__DOUBLE_DOUBLE,
	                            G_TYPE_NONE,
	                            2,
	                            G_TYPE_DOUBLE,
	                            G_TYPE_DOUBLE,
	                            0);

	object_class->destroy = ganv_node_destroy;

	item_class->realize = ganv_node_realize;
	item_class->event   = ganv_node_default_event;
	item_class->update  = ganv_node_update;
	item_class->draw    = ganv_node_draw;

	klass->disconnect  = ganv_node_default_disconnect;
	klass->move        = ganv_node_default_move;
	klass->move_to     = ganv_node_default_move_to;
	klass->resize      = ganv_node_default_resize;
	klass->redraw_text = ganv_node_default_redraw_text;
	klass->tick        = ganv_node_default_tick;
	klass->tail_vector = ganv_node_default_tail_vector;
	klass->head_vector = ganv_node_default_head_vector;
}

gboolean
ganv_node_can_tail(const GanvNode* self)
{
	return self->impl->can_tail;
}

gboolean
ganv_node_can_head(const GanvNode* self)
{
	return self->impl->can_head;
}

void
ganv_node_set_is_source(const GanvNode* node, gboolean is_source)
{
	node->impl->is_source = is_source;
}

gboolean
ganv_node_is_within(const GanvNode* node,
                    double          x1,
                    double          y1,
                    double          x2,
                    double          y2)
{
	return GANV_NODE_GET_CLASS(node)->is_within(node, x1, y1, x2, y2);
}

void
ganv_node_tick(GanvNode* node,
               double    seconds)
{
	GanvNodeClass* klass = GANV_NODE_GET_CLASS(node);
	if (klass->tick) {
		klass->tick(node, seconds);
	}
}

void
ganv_node_tail_vector(const GanvNode* self,
                      const GanvNode* head,
                      double*         x1,
                      double*         y1,
                      double*         x2,
                      double*         y2)
{
	GANV_NODE_GET_CLASS(self)->tail_vector(
		self, head, x1, y1, x2, y2);
}

void
ganv_node_head_vector(const GanvNode* self,
                      const GanvNode* tail,
                      double*         x1,
                      double*         y1,
                      double*         x2,
                      double*         y2)
{
	GANV_NODE_GET_CLASS(self)->head_vector(
		self, tail, x1, y1, x2, y2);
}

const char*
ganv_node_get_label(const GanvNode* node)
{
	return node->impl->label ? node->impl->label->impl->text : NULL;
}

double
ganv_node_get_border_width(const GanvNode* node)
{
	return node->impl->border_width;
}

void
ganv_node_set_border_width(const GanvNode* node, double border_width)
{
	node->impl->border_width = border_width;
	ganv_item_request_update(GANV_ITEM(node));
}

double
ganv_node_get_dash_length(const GanvNode* node)
{
	return node->impl->dash_length;
}

void
ganv_node_set_dash_length(const GanvNode* node, double dash_length)
{
	node->impl->dash_length = dash_length;
	ganv_item_request_update(GANV_ITEM(node));
}

double
ganv_node_get_dash_offset(const GanvNode* node)
{
	return node->impl->dash_offset;
}

void
ganv_node_set_dash_offset(const GanvNode* node, double dash_offset)
{
	node->impl->dash_offset = dash_offset;
	ganv_item_request_update(GANV_ITEM(node));
}

guint
ganv_node_get_fill_color(const GanvNode* node)
{
	return node->impl->fill_color;
}

void
ganv_node_set_fill_color(const GanvNode* node, guint fill_color)
{
	node->impl->fill_color = fill_color;
	ganv_item_request_update(GANV_ITEM(node));
}

guint
ganv_node_get_border_color(const GanvNode* node)
{
	return node->impl->border_color;
}

void
ganv_node_set_border_color(const GanvNode* node, guint border_color)
{
	node->impl->border_color = border_color;
	ganv_item_request_update(GANV_ITEM(node));
}

GanvNode*
ganv_node_get_partner(const GanvNode* node)
{
	return node->impl->partner;
}

void
ganv_node_move(GanvNode* node,
               double    dx,
               double    dy)
{
	GANV_NODE_GET_CLASS(node)->move(node, dx, dy);
}

void
ganv_node_move_to(GanvNode* node,
                  double    x,
                  double    y)
{
	GANV_NODE_GET_CLASS(node)->move_to(node, x, y);
}

void
ganv_node_resize(GanvNode* node)
{
	GANV_NODE_GET_CLASS(node)->resize(node);
	node->impl->must_resize = FALSE;
}

void
ganv_node_redraw_text(GanvNode* node)
{
	GANV_NODE_GET_CLASS(node)->redraw_text(node);
}

void
ganv_node_disconnect(GanvNode* node)
{
	GANV_NODE_GET_CLASS(node)->disconnect(node);
}

gboolean
ganv_node_is_selected(GanvNode* node)
{
	gboolean selected = FALSE;
	g_object_get(node, "selected", &selected, NULL);
	return selected;
}
