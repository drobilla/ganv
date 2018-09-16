/* This file is part of Ganv.
 * Copyright 2007-2015 David Robillard <http://drobilla.net>
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

#include <math.h>
#include <stdlib.h>

#include "ganv/canvas.h"
#include "ganv/port.h"
#include "ganv/module.h"

#include "./boilerplate.h"
#include "./color.h"
#include "./ganv-private.h"
#include "./gettext.h"

static const double PORT_LABEL_HPAD = 4.0;
static const double PORT_LABEL_VPAD = 1.0;

static void
ganv_port_update_control_slider(GanvPort* port, float value, gboolean force);

G_DEFINE_TYPE_WITH_CODE(GanvPort, ganv_port, GANV_TYPE_BOX,
                        G_ADD_PRIVATE(GanvPort))

static GanvBoxClass* parent_class;

enum {
	PROP_0,
	PROP_IS_INPUT,
	PROP_IS_CONTROLLABLE
};

enum {
	PORT_VALUE_CHANGED,
	PORT_LAST_SIGNAL
};

static guint port_signals[PORT_LAST_SIGNAL];

static void
ganv_port_init(GanvPort* port)
{
	port->impl = ganv_port_get_instance_private(port);

	port->impl->control         = NULL;
	port->impl->value_label     = NULL;
	port->impl->is_input        = TRUE;
	port->impl->is_controllable = FALSE;
}

static void
ganv_port_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_PORT(object));

	GanvItem*   item   = GANV_ITEM(object);
	GanvPort*   port   = GANV_PORT(object);
	GanvCanvas* canvas = ganv_item_get_canvas(item);
	if (canvas) {
		if (port->impl->is_input) {
			ganv_canvas_for_each_edge_to(
				canvas, &port->box.node, (GanvEdgeFunc)ganv_edge_remove, NULL);
		} else {
			ganv_canvas_for_each_edge_from(
				canvas, &port->box.node, (GanvEdgeFunc)ganv_edge_remove, NULL);
		}
	}

	if (GTK_OBJECT_CLASS(parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
	}
}

static void
ganv_port_set_property(GObject*      object,
                       guint         prop_id,
                       const GValue* value,
                       GParamSpec*   pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_PORT(object));

	GanvPort* port = GANV_PORT(object);

	switch (prop_id) {
		SET_CASE(IS_INPUT, boolean, port->impl->is_input);
		SET_CASE(IS_CONTROLLABLE, boolean, port->impl->is_controllable);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_port_get_property(GObject*    object,
                       guint       prop_id,
                       GValue*     value,
                       GParamSpec* pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_PORT(object));

	GanvPort* port = GANV_PORT(object);

	switch (prop_id) {
		GET_CASE(IS_INPUT, boolean, port->impl->is_input);
		GET_CASE(IS_CONTROLLABLE, boolean, port->impl->is_controllable);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_port_update(GanvItem* item, int flags)
{
	GanvPort*        port = GANV_PORT(item);
	GanvPortPrivate* impl = port->impl;

	if (impl->control) {
		ganv_item_invoke_update(GANV_ITEM(impl->control->rect), flags);
	}

	if (impl->value_label) {
		ganv_item_invoke_update(GANV_ITEM(port->impl->value_label), flags);
	}

	GanvItemClass* item_class = GANV_ITEM_CLASS(parent_class);
	item_class->update(item, flags);
}

static void
ganv_port_draw(GanvItem* item,
               cairo_t* cr, double cx, double cy, double cw, double ch)
{
	GanvPort*   port   = GANV_PORT(item);
	GanvCanvas* canvas = ganv_item_get_canvas(item);

	// Draw Box
	GanvItemClass* item_class = GANV_ITEM_CLASS(parent_class);
	item_class->draw(item, cr, cx, cy, cw, ch);

	if (port->impl->control) {
		// Clip to port boundaries (to stay within radiused borders)
		cairo_save(cr);
		const double  pad    = GANV_NODE(port)->impl->border_width / 2.0;
		GanvBoxCoords coords = GANV_BOX(port)->impl->coords;
		ganv_item_i2w_pair(GANV_ITEM(port),
		                   &coords.x1, &coords.y1, &coords.x2, &coords.y2);
		ganv_box_path(GANV_BOX(port), cr,
		              coords.x1 + pad, coords.y1 + pad,
		              coords.x2 - pad, coords.y2 - pad,
		              -pad);
		cairo_clip(cr);

		GanvItem* const rect = GANV_ITEM(port->impl->control->rect);
		GANV_ITEM_GET_CLASS(rect)->draw(rect, cr, cx, cy, cw, ch);

		cairo_restore(cr);
	}

	if (ganv_canvas_get_direction(canvas) == GANV_DIRECTION_DOWN ||
	    !GANV_NODE(port)->impl->show_label) {
		return;
	}

	GanvItem* labels[2] = {
		GANV_ITEM(GANV_NODE(item)->impl->label),
		port->impl->value_label ? GANV_ITEM(port->impl->value_label) : NULL
	};
	for (int i = 0; i < 2; ++i) {
		if (labels[i] && (labels[i]->object.flags & GANV_ITEM_VISIBLE)) {
			GANV_ITEM_GET_CLASS(labels[i])->draw(
				labels[i], cr, cx, cy, cw, ch);
		}
	}
}

static void
ganv_port_tail_vector(const GanvNode* self,
                      const GanvNode* head,
                      double*         x,
                      double*         y,
                      double*         dx,
                      double*         dy)
{
	GanvPort*   port   = GANV_PORT(self);
	GanvItem*   item   = &port->box.node.item;
	GanvCanvas* canvas = ganv_item_get_canvas(item);

	const double px           = item->impl->x;
	const double py           = item->impl->y;
	const double border_width = GANV_NODE(port)->impl->border_width;

	switch (ganv_canvas_get_direction(canvas)) {
	case GANV_DIRECTION_RIGHT:
		*x  = px + ganv_box_get_width(&port->box) + (border_width / 2.0);
		*y  = py + ganv_box_get_height(&port->box) / 2.0;
		*dx = 1.0;
		*dy = 0.0;
		break;
	case GANV_DIRECTION_DOWN:
		*x  = px + ganv_box_get_width(&port->box) / 2.0;
		*y  = py + ganv_box_get_height(&port->box) + (border_width / 2.0);
		*dx = 0.0;
		*dy = 1.0;
		break;
	}

	ganv_item_i2w(item->impl->parent, x, y);
}

static void
ganv_port_head_vector(const GanvNode* self,
                      const GanvNode* tail,
                      double*         x,
                      double*         y,
                      double*         dx,
                      double*         dy)
{
	GanvPort*   port   = GANV_PORT(self);
	GanvItem*   item   = &port->box.node.item;
	GanvCanvas* canvas = ganv_item_get_canvas(item);

	const double px           = item->impl->x;
	const double py           = item->impl->y;
	const double border_width = GANV_NODE(port)->impl->border_width;

	switch (ganv_canvas_get_direction(canvas)) {
	case GANV_DIRECTION_RIGHT:
		*x  = px - (border_width / 2.0);
		*y  = py + ganv_box_get_height(&port->box) / 2.0;
		*dx = -1.0;
		*dy = 0.0;
		break;
	case GANV_DIRECTION_DOWN:
		*x  = px + ganv_box_get_width(&port->box) / 2.0;
		*y  = py - (border_width / 2.0);
		*dx = 0.0;
		*dy = -1.0;
		break;
	}

	ganv_item_i2w(item->impl->parent, x, y);
}

static void
ganv_port_place_labels(GanvPort* port)
{
	GanvCanvas*      canvas   = ganv_item_get_canvas(GANV_ITEM(port));
	GanvPortPrivate* impl     = port->impl;
	GanvText*        label    = GANV_NODE(port)->impl->label;
	const double     port_w   = ganv_box_get_width(&port->box);
	const double     port_h   = ganv_box_get_height(&port->box);
	double           vlabel_w = 0.0;
	if (impl->value_label) {
		const double vlabel_h = impl->value_label->impl->coords.height;
		vlabel_w = impl->value_label->impl->coords.width;
		if (ganv_canvas_get_direction(canvas) == GANV_DIRECTION_RIGHT) {
			ganv_item_set(GANV_ITEM(impl->value_label),
			              "x", PORT_LABEL_HPAD,
			              "y", (port_h - vlabel_h) / 2.0 - PORT_LABEL_VPAD,
			              NULL);
		} else {
			ganv_item_set(GANV_ITEM(impl->value_label),
			              "x", (port_w - vlabel_w) / 2.0,
			              "y", (port_h - vlabel_h) / 2.0 - PORT_LABEL_VPAD,
			              NULL);
		}
		vlabel_w += PORT_LABEL_HPAD;
	}
	if (label) {
		const double label_h = label->impl->coords.height;
		if (ganv_canvas_get_direction(canvas) == GANV_DIRECTION_RIGHT) {
			ganv_item_set(GANV_ITEM(label),
			              "x", vlabel_w + PORT_LABEL_HPAD,
			              "y", (port_h - label_h) / 2.0 - PORT_LABEL_VPAD,
			              NULL);
		}
	}
}

static void
ganv_port_resize(GanvNode* self)
{
	GanvPort* port   = GANV_PORT(self);
	GanvNode* node   = GANV_NODE(self);
	GanvText* label  = node->impl->label;
	GanvText* vlabel = port->impl->value_label;

	double label_w  = 0.0;
	double label_h  = 0.0;
	double vlabel_w = 0.0;
	double vlabel_h = 0.0;
	if (label && (GANV_ITEM(label)->object.flags & GANV_ITEM_VISIBLE)) {
		g_object_get(label, "width", &label_w, "height", &label_h, NULL);
	}
	if (vlabel && (GANV_ITEM(vlabel)->object.flags & GANV_ITEM_VISIBLE)) {
		g_object_get(vlabel, "width", &vlabel_w, "height", &vlabel_h, NULL);
	}

	if (label || vlabel) {
		double labels_w = label_w + PORT_LABEL_HPAD * 2.0;
		if (vlabel_w != 0.0) {
			labels_w += vlabel_w + PORT_LABEL_HPAD;
		}
		ganv_box_set_width(&port->box, labels_w);
		ganv_box_set_height(&port->box,
		                    MAX(label_h, vlabel_h) + (PORT_LABEL_VPAD * 2.0));

		ganv_port_place_labels(port);
	}

	if (GANV_NODE_CLASS(parent_class)->resize) {
		GANV_NODE_CLASS(parent_class)->resize(self);
	}
}

static void
ganv_port_redraw_text(GanvNode* node)
{
	GanvPort* port = GANV_PORT(node);
	if (port->impl->value_label) {
		ganv_text_layout(port->impl->value_label);
	}
	if (GANV_NODE_CLASS(parent_class)->redraw_text) {
		(*GANV_NODE_CLASS(parent_class)->redraw_text)(node);
	}
	ganv_port_place_labels(port);
}

static void
ganv_port_set_width(GanvBox* box,
                    double   width)
{
	GanvPort* port = GANV_PORT(box);
	parent_class->set_width(box, width);
	if (port->impl->control) {
		ganv_port_update_control_slider(port, port->impl->control->value, TRUE);
	}
	ganv_port_place_labels(port);
}

static void
ganv_port_set_height(GanvBox* box,
                     double   height)
{
	GanvPort* port = GANV_PORT(box);
	parent_class->set_height(box, height);
	if (port->impl->control) {
		ganv_item_set(GANV_ITEM(port->impl->control->rect),
		              "y1", box->impl->coords.border_width / 2.0,
		              "y2", height - box->impl->coords.border_width / 2.0,
		              NULL);
	}
	ganv_port_place_labels(port);
}

static gboolean
ganv_port_event(GanvItem* item, GdkEvent* event)
{
	GanvCanvas* canvas = ganv_item_get_canvas(item);

	return ganv_canvas_port_event(canvas, GANV_PORT(item), event);
}

static void
ganv_port_class_init(GanvPortClass* klass)
{
	GObjectClass*   gobject_class = (GObjectClass*)klass;
	GtkObjectClass* object_class  = (GtkObjectClass*)klass;
	GanvItemClass*  item_class    = (GanvItemClass*)klass;
	GanvNodeClass*  node_class    = (GanvNodeClass*)klass;
	GanvBoxClass*   box_class     = (GanvBoxClass*)klass;

	parent_class = GANV_BOX_CLASS(g_type_class_peek_parent(klass));

	gobject_class->set_property = ganv_port_set_property;
	gobject_class->get_property = ganv_port_get_property;

	g_object_class_install_property(
		gobject_class, PROP_IS_INPUT, g_param_spec_boolean(
			"is-input",
			_("Is input"),
			_("Whether this port is an input, rather than an output."),
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class, PROP_IS_CONTROLLABLE, g_param_spec_boolean(
			"is-controllable",
			_("Is controllable"),
			_("Whether this port can be controlled by the user."),
			0,
			G_PARAM_READWRITE));

	port_signals[PORT_VALUE_CHANGED]
	    = g_signal_new("value-changed",
	                   G_TYPE_FROM_CLASS(klass),
	                   G_SIGNAL_RUN_LAST,
	                   0,
	                   NULL, NULL,
	                   NULL,
	                   G_TYPE_NONE, 1,
	                   G_TYPE_DOUBLE);

	object_class->destroy = ganv_port_destroy;

	item_class->update = ganv_port_update;
	item_class->event  = ganv_port_event;
	item_class->draw   = ganv_port_draw;

	node_class->tail_vector = ganv_port_tail_vector;
	node_class->head_vector = ganv_port_head_vector;
	node_class->resize      = ganv_port_resize;
	node_class->redraw_text = ganv_port_redraw_text;

	box_class->set_width  = ganv_port_set_width;
	box_class->set_height = ganv_port_set_height;
}

GanvPort*
ganv_port_new(GanvModule* module,
              gboolean    is_input,
              const char* first_prop_name, ...)
{
	GanvPort* port = GANV_PORT(g_object_new(ganv_port_get_type(), NULL));

	port->impl->is_input = is_input;

	GanvItem* item = GANV_ITEM(port);
	va_list args;
	va_start(args, first_prop_name);
	ganv_item_construct(item,
	                    GANV_ITEM(module),
	                    first_prop_name, args);
	va_end(args);

	GanvBox* box = GANV_BOX(port);
	box->impl->coords.border_width = 1.0;

	GanvNode* node = GANV_NODE(port);
	node->impl->can_tail     = !is_input;
	node->impl->can_head     = is_input;
	node->impl->draggable    = FALSE;
	node->impl->border_width = 2.0;

	GanvCanvas* canvas = ganv_item_get_canvas(GANV_ITEM(port));
	ganv_port_set_direction(port, ganv_canvas_get_direction(canvas));

	return port;
}

void
ganv_port_set_direction(GanvPort*     port,
                        GanvDirection direction)
{
	GanvNode* node     = GANV_NODE(port);
	GanvBox*  box      = GANV_BOX(port);
	gboolean  is_input = port->impl->is_input;
	switch (direction) {
	case GANV_DIRECTION_RIGHT:
		box->impl->radius_tl = (is_input ? 0.0 : 5.0);
		box->impl->radius_tr = (is_input ? 5.0 : 0.0);
		box->impl->radius_br = (is_input ? 5.0 : 0.0);
		box->impl->radius_bl = (is_input ? 0.0 : 5.0);
		break;
	case GANV_DIRECTION_DOWN:
		box->impl->radius_tl = (is_input ? 0.0 : 5.0);
		box->impl->radius_tr = (is_input ? 0.0 : 5.0);
		box->impl->radius_br = (is_input ? 5.0 : 0.0);
		box->impl->radius_bl = (is_input ? 5.0 : 0.0);
		break;
	}

	node->impl->must_resize = TRUE;
	ganv_item_request_update(GANV_ITEM(node));
}

void
ganv_port_show_control(GanvPort* port)
{
	if (port->impl->control) {
		return;
	}

	const guint  color        = 0xFFFFFF66;
	const double border_width = GANV_NODE(port)->impl->border_width;

	GanvPortControl* control = (GanvPortControl*)malloc(sizeof(GanvPortControl));
	port->impl->control = control;

	control->value      = 0.0f;
	control->min        = 0.0f;
	control->max        = 1.0f;
	control->is_toggle  = FALSE;
	control->is_integer = FALSE;
	control->rect       = GANV_BOX(
		ganv_item_new(GANV_ITEM(port),
		              ganv_box_get_type(),
		              "x1", border_width / 2.0,
		              "y1", border_width / 2.0,
		              "x2", 0.0,
		              "y2", ganv_box_get_height(&port->box) - border_width / 2.0,
		              "fill-color", color,
		              "border-color", color,
		              "border-width", 0.0,
		              "managed", TRUE,
		              NULL));
	ganv_item_show(GANV_ITEM(control->rect));
}

void
ganv_port_hide_control(GanvPort* port)
{
	gtk_object_destroy(GTK_OBJECT(port->impl->control->rect));
	free(port->impl->control);
	port->impl->control = NULL;
}

void
ganv_port_set_value_label(GanvPort*   port,
                          const char* str)
{
	GanvPortPrivate* impl = port->impl;

	if (!str || str[0] == '\0') {
		if (impl->value_label) {
			gtk_object_destroy(GTK_OBJECT(impl->value_label));
			impl->value_label = NULL;
		}
	} else if (impl->value_label) {
		ganv_item_set(GANV_ITEM(impl->value_label),
		              "text", str,
		              NULL);
	} else {
		impl->value_label = GANV_TEXT(ganv_item_new(GANV_ITEM(port),
		                                            ganv_text_get_type(),
		                                            "text", str,
		                                            "color", DIM_TEXT_COLOR,
		                                            "managed", TRUE,
		                                            NULL));
	}
}

static void
ganv_port_update_control_slider(GanvPort* port, float value, gboolean force)
{
	GanvPortPrivate* impl = port->impl;
	if (!impl->control) {
		return;
	}

	// Clamp to toggle or integer value if applicable
	if (impl->control->is_toggle) {
		if (value != 0.0f) {
			value = impl->control->max;
		} else {
			value = impl->control->min;
		}
	} else if (impl->control->is_integer) {
		value = lrintf(value);
	}

	// Clamp to range
	if (value < impl->control->min) {
		value = impl->control->min;
	}
	if (value > impl->control->max) {
		value = impl->control->max;
	}

	if (!force && value == impl->control->value) {
		return;  // No change, do nothing
	}

	const double span = (ganv_box_get_width(&port->box) -
	                     GANV_NODE(port)->impl->border_width);

	const double w = (value - impl->control->min)
		/ (impl->control->max - impl->control->min)
		* span;

	if (isnan(w)) {
		return;  // Shouldn't happen, but ignore crazy values
	}

	// Redraw port
	impl->control->value = value;
	ganv_box_set_width(impl->control->rect, MAX(0.0, w));
	ganv_box_request_redraw(
		GANV_ITEM(port), &GANV_BOX(port)->impl->coords, FALSE);
}

void
ganv_port_set_control_is_toggle(GanvPort* port,
                                gboolean  is_toggle)
{
	if (port->impl->control) {
		port->impl->control->is_toggle = is_toggle;
		ganv_port_update_control_slider(port, port->impl->control->value, TRUE);
	}
}

void
ganv_port_set_control_is_integer(GanvPort* port,
                                 gboolean  is_integer)
{
	if (port->impl->control) {
		port->impl->control->is_integer = is_integer;
		const float rounded = rintf(port->impl->control->value);
		ganv_port_update_control_slider(port, rounded, TRUE);
	}
}

void
ganv_port_set_control_value(GanvPort* port,
                            float     value)
{
	ganv_port_update_control_slider(port, value, FALSE);
}

void
ganv_port_set_control_value_internal(GanvPort* port,
                                     float     value)
{
	// Update slider
	ganv_port_set_control_value(port, value);

	// Fire signal to notify user value has changed
	const double dvalue = port->impl->control->value;
	g_signal_emit(port, port_signals[PORT_VALUE_CHANGED], 0, dvalue, NULL);
}

void
ganv_port_set_control_min(GanvPort* port,
                          float     min)
{
	if (port->impl->control) {
		const gboolean force = port->impl->control->min != min;
		port->impl->control->min = min;
		if (port->impl->control->max < min) {
			port->impl->control->max = min;
		}
		ganv_port_update_control_slider(port, port->impl->control->value, force);
	}
}

void
ganv_port_set_control_max(GanvPort* port,
                          float     max)
{
	if (port->impl->control) {
		const gboolean force = port->impl->control->max != max;
		port->impl->control->max = max;
		if (port->impl->control->min > max) {
			port->impl->control->min = max;
		}
		ganv_port_update_control_slider(port, port->impl->control->value, force);
	}
}

double
ganv_port_get_natural_width(const GanvPort* port)
{
	GanvCanvas* const canvas = ganv_item_get_canvas(GANV_ITEM(port));
	GanvText* const   label  = port->box.node.impl->label;
	double            w      = 0.0;
	if (ganv_canvas_get_direction(canvas) == GANV_DIRECTION_DOWN) {
		w = ganv_module_get_empty_port_breadth(ganv_port_get_module(port));
	} else if (label && (GANV_ITEM(label)->object.flags & GANV_ITEM_VISIBLE)) {
		double label_w;
		g_object_get(port->box.node.impl->label, "width", &label_w, NULL);
		w = label_w + (PORT_LABEL_HPAD * 2.0);
	} else {
		w = ganv_module_get_empty_port_depth(ganv_port_get_module(port));
	}
	if (port->impl->value_label &&
	    (GANV_ITEM(port->impl->value_label)->object.flags
	     & GANV_ITEM_VISIBLE)) {
		double label_w;
		g_object_get(port->impl->value_label, "width", &label_w, NULL);
		w += label_w + PORT_LABEL_HPAD;
	}
	return w;
}

GanvModule*
ganv_port_get_module(const GanvPort* port)
{
	return GANV_MODULE(GANV_ITEM(port)->impl->parent);
}

float
ganv_port_get_control_value(const GanvPort* port)
{
	return port->impl->control ? port->impl->control->value : 0.0f;
}

float
ganv_port_get_control_min(const GanvPort* port)
{
	return port->impl->control ? port->impl->control->min : 0.0f;
}

float
ganv_port_get_control_max(const GanvPort* port)
{
	return port->impl->control ? port->impl->control->max : 0.0f;
}

gboolean
ganv_port_is_input(const GanvPort* port)
{
	return port->impl->is_input;
}

gboolean
ganv_port_is_output(const GanvPort* port)
{
	return !port->impl->is_input;
}
