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

#include <math.h>
#include <stdlib.h>

#include "ganv/canvas.h"
#include "ganv/port.h"
#include "ganv/module.h"

#include "./boilerplate.h"
#include "./ganv-private.h"
#include "./gettext.h"

static const double PORT_LABEL_HPAD = 4.0;
static const double PORT_LABEL_VPAD = 1.0;

G_DEFINE_TYPE(GanvPort, ganv_port, GANV_TYPE_BOX)

static GanvBoxClass* parent_class;

enum {
	PROP_0,
	PROP_IS_INPUT
};

static void
ganv_port_init(GanvPort* port)
{
	port->impl = G_TYPE_INSTANCE_GET_PRIVATE(
		port, GANV_TYPE_PORT, GanvPortImpl);

	port->impl->is_input = TRUE;
}

static void
ganv_port_destroy(GtkObject* object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(GANV_IS_PORT(object));

	GanvItem*   item   = GANV_ITEM(object);
	GanvPort*   port   = GANV_PORT(object);
	GanvCanvas* canvas = GANV_CANVAS(item->canvas);
	if (canvas) {
		if (port->impl->is_input) {
			ganv_canvas_for_each_edge_to(canvas,
			                             &port->box.node,
			                             ganv_edge_remove);
		} else {
			ganv_canvas_for_each_edge_from(canvas,
			                               &port->box.node,
			                               ganv_edge_remove);
		}
		item->canvas = NULL;
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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ganv_port_draw(GanvItem* item,
               cairo_t* cr,
               int cx, int cy,
               int width, int height)
{
	GanvPort* port = GANV_PORT(item);

	GanvItemClass* item_class = GANV_ITEM_CLASS(parent_class);
	item_class->draw(item, cr, cx, cy, width, height);

	if (port->impl->control) {
		GanvItem* const rect = GANV_ITEM(port->impl->control->rect);
		GANV_ITEM_GET_CLASS(rect)->draw(rect, cr, cx, cy, width, height);
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
	GanvPort* port = GANV_PORT(self);

	double px, py;
	g_object_get(G_OBJECT(self), "x", &px, "y", &py, NULL);

	*x  = px + ganv_box_get_width(&port->box);
	*y  = py + ganv_box_get_height(&port->box) / 2.0;
	*dx = 1.0;
	*dy = 0.0;

	ganv_item_i2w(GANV_ITEM(self)->parent, x, y);
}

static void
ganv_port_head_vector(const GanvNode* self,
                      const GanvNode* tail,
                      double*         x,
                      double*         y,
                      double*         dx,
                      double*         dy)
{
	GanvPort* port = GANV_PORT(self);

	double px, py;
	g_object_get(G_OBJECT(self), "x", &px, "y", &py, NULL);

	*x  = px;
	*y  = py + ganv_box_get_height(&port->box) / 2.0;
	*dx = -1.0;
	*dy = 0.0;

	ganv_item_i2w(GANV_ITEM(self)->parent, x, y);
}

static void
ganv_port_resize(GanvNode* self)
{
	GanvPort* port = GANV_PORT(self);
	GanvNode* node = GANV_NODE(self);

	double label_w, label_h;
	g_object_get(node->impl->label,
	             "width", &label_w,
	             "height", &label_h,
	             NULL);

	ganv_box_set_width(&port->box, label_w + (PORT_LABEL_HPAD * 2.0));
	ganv_box_set_height(&port->box, label_h + (PORT_LABEL_VPAD * 2.0));

	ganv_item_set(GANV_ITEM(node->impl->label),
	              "x", PORT_LABEL_HPAD,
	              "y", PORT_LABEL_VPAD,
	              NULL);

	if (parent_class->parent_class.resize) {
		parent_class->parent_class.resize(self);
	}
}

static void
ganv_port_set_width(GanvBox* box,
                    double   width)
{
	GanvPort* port = GANV_PORT(box);
	parent_class->set_width(box, width);
	if (port->impl->control) {
		ganv_port_set_control_value(port, port->impl->control->value);
	}
}

static void
ganv_port_set_height(GanvBox* box,
                     double   height)
{
	GanvPort* port = GANV_PORT(box);
	parent_class->set_height(box, height);
	if (port->impl->control) {
		double control_y1;
		g_object_get(port->impl->control->rect, "y1", &control_y1, NULL);
		ganv_item_set(GANV_ITEM(port->impl->control->rect),
		              "y2", control_y1 + height,
		              NULL);
	}
}

static gboolean
event(GanvItem* item, GdkEvent* event)
{
	GanvCanvas* canvas = GANV_CANVAS(item->canvas);

	return ganv_canvas_port_event(canvas, GANV_PORT(item), event);
}

static void
ganv_port_class_init(GanvPortClass* class)
{
	GObjectClass*   gobject_class = (GObjectClass*)class;
	GtkObjectClass* object_class  = (GtkObjectClass*)class;
	GanvItemClass*  item_class    = (GanvItemClass*)class;
	GanvNodeClass*  node_class    = (GanvNodeClass*)class;
	GanvBoxClass*   box_class     = (GanvBoxClass*)class;

	parent_class = GANV_BOX_CLASS(g_type_class_peek_parent(class));

	g_type_class_add_private(class, sizeof(GanvPortImpl));

	gobject_class->set_property = ganv_port_set_property;
	gobject_class->get_property = ganv_port_get_property;

	g_object_class_install_property(
		gobject_class, PROP_IS_INPUT, g_param_spec_boolean(
			"is-input",
			_("Is input"),
			_("Whether this port is an input, rather than an output."),
			0,
			G_PARAM_READWRITE));

	object_class->destroy = ganv_port_destroy;

	item_class->event = event;
	item_class->draw  = ganv_port_draw;

	node_class->tail_vector = ganv_port_tail_vector;
	node_class->head_vector = ganv_port_head_vector;
	node_class->resize      = ganv_port_resize;

	box_class->set_width  = ganv_port_set_width;
	box_class->set_height = ganv_port_set_height;
}

GanvPort*
ganv_port_new(GanvModule* module,
              gboolean    is_input,
              const char* first_prop_name, ...)
{
	GanvPort* port = GANV_PORT(
		g_object_new(ganv_port_get_type(), NULL));

	port->impl->is_input = is_input;

	GanvItem* item = GANV_ITEM(port);
	va_list args;
	va_start(args, first_prop_name);
	ganv_item_construct(item,
	                    GANV_ITEM(module),
	                    first_prop_name, args);
	va_end(args);

	GanvBox* box = GANV_BOX(port);
	box->impl->radius_tl           = (is_input ? 0.0 : 4.0);
	box->impl->radius_tr           = (is_input ? 4.0 : 0.0);
	box->impl->radius_br           = (is_input ? 4.0 : 0.0);
	box->impl->radius_bl           = (is_input ? 0.0 : 4.0);
	box->impl->coords.border_width = 1.0;

	GanvNode* node = GANV_NODE(port);
	node->impl->can_tail     = !is_input;
	node->impl->can_head     = is_input;
	node->impl->draggable    = FALSE;
	node->impl->border_width = 1.0;

	GanvCanvas* canvas = GANV_CANVAS(item->canvas);
	if (!node->impl->label) {
		const double depth   = ganv_module_get_empty_port_depth(module);
		const double breadth = ganv_module_get_empty_port_breadth(module);
		if (canvas->direction == GANV_DIRECTION_RIGHT) {
			ganv_box_set_width(box, depth);
			ganv_box_set_height(box, breadth);
		} else {
			ganv_box_set_width(box, breadth);
			ganv_box_set_height(box, depth);
		}
	}

	return port;
}

void
ganv_port_show_control(GanvPort* port)
{
	GanvPortControl* control = (GanvPortControl*)malloc(sizeof(GanvPortControl));
	port->impl->control = control;

	control->value     = 0.0f;
	control->min       = 0.0f;
	control->max       = 0.0f;
	control->is_toggle = FALSE;
	control->rect      = GANV_BOX(ganv_item_new(
		                              GANV_ITEM(port),
		                              ganv_box_get_type(),
		                              "x1", 0.0,
		                              "y1", 0.0,
		                              "x2", 0.0,
		                              "y2", ganv_box_get_height(&port->box),
		                              "fill-color", 0xFFFFFF80,
		                              "border-width", 0.0,
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
ganv_port_set_control_is_toggle(GanvPort* port,
                                gboolean  is_toggle)
{
	if (port->impl->control) {
		port->impl->control->is_toggle = is_toggle;
		ganv_port_set_control_value(port, port->impl->control->value);
	}
}

void
ganv_port_set_control_value(GanvPort* port,
                            float     value)
{
	GanvPortImpl* impl = port->impl;
	if (!impl->control) {
		return;
	}

	if (impl->control->is_toggle) {
		if (value != 0.0) {
			value = impl->control->max;
		} else {
			value = impl->control->min;
		}
	}

	if (value < impl->control->min) {
		impl->control->min = value;
	}
	if (value > impl->control->max) {
		impl->control->max = value;
	}

	if (impl->control->max == impl->control->min) {
		impl->control->max = impl->control->min + 1.0;
	}

	const int inf = isinf(value);
	if (inf == -1) {
		value = impl->control->min;
	} else if (inf == 1) {
		value = impl->control->max;
	}

	const double w = (value - impl->control->min)
		/ (impl->control->max - impl->control->min)
		* ganv_box_get_width(&port->box);

	if (isnan(w)) {
		return;
	}

	ganv_box_set_width(impl->control->rect, MAX(0.0, w - 1.0));
#if 0
	if (signal && _control->value == value)
		signal = false;
#endif

	impl->control->value = value;

	ganv_item_request_update(GANV_ITEM(port));
}

void
ganv_port_set_control_min(GanvPort* port,
                          float     min)
{
	if (port->impl->control) {
		port->impl->control->min = min;
		ganv_port_set_control_value(port, port->impl->control->value);
	}
}

void
ganv_port_set_control_max(GanvPort* port,
                          float     max)
{
	if (port->impl->control) {
		port->impl->control->max = max;
		ganv_port_set_control_value(port, port->impl->control->value);
	}
}

double
ganv_port_get_natural_width(const GanvPort* port)
{
	/*
	  Canvas* const canvas = _module->canvas();
	  if (canvas->direction() == Canvas::VERTICAL) {
	  return _module->empty_port_breadth();
	  } else*/
	if (port->box.node.impl->label) {
		double label_w;
		g_object_get(port->box.node.impl->label, "width", &label_w, NULL);
		return label_w + (PORT_LABEL_HPAD * 2.0);
	} else {
		//return _module->empty_port_depth();
		return 4.0;
	}
}

GanvModule*
ganv_port_get_module(const GanvPort* port)
{
	return GANV_MODULE(GANV_ITEM(port)->parent);
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
