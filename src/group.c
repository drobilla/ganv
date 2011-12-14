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

/* Based on GnomeCanvasGroup, by Federico Mena <federico@nuclecu.unam.mx>
 * and Raph Levien <raph@gimp.org>
 * Copyright 1997-2000 Free Software Foundation
 */

#include <math.h>

#include "ganv/canvas-base.h"
#include "ganv/group.h"

#include "./gettext.h"
#include "./ganv-private.h"

enum {
	GROUP_PROP_0
};

G_DEFINE_TYPE(GanvGroup, ganv_group, GANV_TYPE_ITEM)

static GanvItemClass* group_parent_class;

static void
ganv_group_init(GanvGroup* group)
{
}

static void
ganv_group_set_property(GObject* gobject, guint param_id,
                        const GValue* value, GParamSpec* pspec)
{
	g_return_if_fail(GANV_IS_GROUP(gobject));

	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		break;
	}
}

static void
ganv_group_get_property(GObject* gobject, guint param_id,
                        GValue* value, GParamSpec* pspec)
{
	g_return_if_fail(GANV_IS_GROUP(gobject));

	switch (param_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, param_id, pspec);
		break;
	}
}

static void
ganv_group_destroy(GtkObject* object)
{
	GanvGroup* group;

	g_return_if_fail(GANV_IS_GROUP(object));

	group = GANV_GROUP(object);

	while (group->item_list) {
		// child is unref'ed by the child's group_remove().
		gtk_object_destroy(GTK_OBJECT(group->item_list->data));
	}
	if (GTK_OBJECT_CLASS(group_parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(group_parent_class)->destroy)(object);
	}
}

static void
ganv_group_update(GanvItem* item, int flags)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  i;

	group = GANV_GROUP(item);

	(*group_parent_class->update)(item, flags);

	double min_x = 0.0;
	double min_y = 0.0;
	double max_x = 0.0;
	double max_y = 0.0;

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		ganv_item_invoke_update(i, flags);

		min_x = fmin(min_x, fmin(i->x1, i->x2));
		min_y = fmin(min_y, fmin(i->y1, i->y2));
		max_x = fmax(max_x, fmax(i->x1, i->x2));
		max_y = fmax(max_y, fmax(i->y2, i->y2));
	}
	item->x1 = min_x;
	item->y1 = min_y;
	item->x2 = max_x;
	item->y2 = max_y;
}

static void
ganv_group_realize(GanvItem* item)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  i;

	group = GANV_GROUP(item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (!(i->object.flags & GANV_ITEM_REALIZED)) {
			(*GANV_ITEM_GET_CLASS(i)->realize)(i);
		}
	}

	(*group_parent_class->realize)(item);
}

static void
ganv_group_unrealize(GanvItem* item)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  i;

	group = GANV_GROUP(item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (i->object.flags & GANV_ITEM_REALIZED) {
			(*GANV_ITEM_GET_CLASS(i)->unrealize)(i);
		}
	}

	(*group_parent_class->unrealize)(item);
}

static void
ganv_group_map(GanvItem* item)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  i;

	group = GANV_GROUP(item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (!(i->object.flags & GANV_ITEM_MAPPED)) {
			(*GANV_ITEM_GET_CLASS(i)->map)(i);
		}
	}

	(*group_parent_class->map)(item);
}

static void
ganv_group_unmap(GanvItem* item)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  i;

	group = GANV_GROUP(item);

	for (list = group->item_list; list; list = list->next) {
		i = list->data;

		if (i->object.flags & GANV_ITEM_MAPPED) {
			(*GANV_ITEM_GET_CLASS(i)->unmap)(i);
		}
	}

	(*group_parent_class->unmap)(item);
}

static void
ganv_group_draw(GanvItem* item, cairo_t* cr,
                int x, int y, int width, int height)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  child = NULL;

	group = GANV_GROUP(item);

	for (list = group->item_list; list; list = list->next) {
		child = list->data;

		if (((child->object.flags & GANV_ITEM_VISIBLE)
		     && ((child->x1 < (x + width))
		         && (child->y1 < (y + height))
		         && (child->x2 > x)
		         && (child->y2 > y)))
		    || ((GTK_OBJECT_FLAGS(child) & GANV_ITEM_ALWAYS_REDRAW)
		        && (child->x1 < child->canvas->redraw_x2)
		        && (child->y1 < child->canvas->redraw_y2)
		        && (child->x2 > child->canvas->redraw_x1)
		        && (child->y2 > child->canvas->redraw_y2))) {
			if (GANV_ITEM_GET_CLASS(child)->draw) {
				(*GANV_ITEM_GET_CLASS(child)->draw)(
				    child, cr, x, y, width, height);
			}
		}
	}
}

static double
ganv_group_point(GanvItem* item, double x, double y, int cx, int cy,
                 GanvItem** actual_item)
{
	GanvGroup* group;
	GList*     list;
	GanvItem*  child, * point_item;
	int        x1, y1, x2, y2;
	double     gx, gy;
	double     dist, best;
	int        has_point;

	group = GANV_GROUP(item);

	x1 = cx - item->canvas->close_enough;
	y1 = cy - item->canvas->close_enough;
	x2 = cx + item->canvas->close_enough;
	y2 = cy + item->canvas->close_enough;

	best         = 0.0;
	*actual_item = NULL;

	gx = x;
	gy = y;

	dist = 0.0; /* keep gcc happy */

	for (list = group->item_list; list; list = list->next) {
		child = list->data;

		if ((child->x1 > x2) || (child->y1 > y2) || (child->x2 < x1) || (child->y2 < y1)) {
			continue;
		}

		point_item = NULL; /* cater for incomplete item implementations */

		if ((child->object.flags & GANV_ITEM_VISIBLE)
		    && GANV_ITEM_GET_CLASS(child)->point) {
			dist = GANV_ITEM_GET_CLASS(child)->point(
				child,
				gx - child->x, gy - child->y,
				cx, cy,
				&point_item);
			has_point = TRUE;
		} else {
			has_point = FALSE;
		}

		if (has_point
		    && point_item
		    && ((int)(dist * item->canvas->pixels_per_unit + 0.5)
		        <= item->canvas->close_enough)) {
			best         = dist;
			*actual_item = point_item;
		}
	}

	return best;
}

static void
ganv_group_bounds(GanvItem* item, double* x1, double* y1, double* x2, double* y2)
{
	GanvGroup* group;
	GanvItem*  child;
	GList*     list;
	double     tx1, ty1, tx2, ty2;
	double     minx, miny, maxx, maxy;
	int        set;

	group = GANV_GROUP(item);

	/* Get the bounds of the first visible item */

	child = NULL; /* Unnecessary but eliminates a warning. */

	set = FALSE;

	for (list = group->item_list; list; list = list->next) {
		child = list->data;

		if (child->object.flags & GANV_ITEM_VISIBLE) {
			set = TRUE;
			ganv_item_get_bounds(child, &minx, &miny, &maxx, &maxy);
			break;
		}
	}

	/* If there were no visible items, return an empty bounding box */

	if (!set) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

	/* Now we can grow the bounds using the rest of the items */

	list = list->next;

	for (; list; list = list->next) {
		child = list->data;

		if (!(child->object.flags & GANV_ITEM_VISIBLE)) {
			continue;
		}

		ganv_item_get_bounds(child, &tx1, &ty1, &tx2, &ty2);

		if (tx1 < minx) {
			minx = tx1;
		}

		if (ty1 < miny) {
			miny = ty1;
		}

		if (tx2 > maxx) {
			maxx = tx2;
		}

		if (ty2 > maxy) {
			maxy = ty2;
		}
	}

	*x1 = minx;
	*y1 = miny;
	*x2 = maxx;
	*y2 = maxy;
}

static void
ganv_group_add(GanvItem* parent, GanvItem* item)
{
	GanvGroup* group = GANV_GROUP(parent);
	g_object_ref_sink(G_OBJECT(item));

	if (!group->item_list) {
		group->item_list     = g_list_append(group->item_list, item);
		group->item_list_end = group->item_list;
	} else {
		group->item_list_end = g_list_append(group->item_list_end, item)->next;
	}

	if (group->item.object.flags & GANV_ITEM_REALIZED) {
		(*GANV_ITEM_GET_CLASS(item)->realize)(item);
	}

	if (group->item.object.flags & GANV_ITEM_MAPPED) {
		(*GANV_ITEM_GET_CLASS(item)->map)(item);
	}

	g_object_notify(G_OBJECT(item), "parent");
}

static void
ganv_group_remove(GanvItem* parent, GanvItem* item)
{
	GanvGroup* group = GANV_GROUP(parent);
	GList* children;

	g_return_if_fail(GANV_IS_GROUP(group));
	g_return_if_fail(GANV_IS_ITEM(item));

	for (children = group->item_list; children; children = children->next) {
		if (children->data == item) {
			if (item->object.flags & GANV_ITEM_MAPPED) {
				(*GANV_ITEM_GET_CLASS(item)->unmap)(item);
			}

			if (item->object.flags & GANV_ITEM_REALIZED) {
				(*GANV_ITEM_GET_CLASS(item)->unrealize)(item);
			}

			/* Unparent the child */

			item->parent = NULL;
			g_object_unref(G_OBJECT(item));

			/* Remove it from the list */

			if (children == group->item_list_end) {
				group->item_list_end = children->prev;
			}

			group->item_list = g_list_remove_link(group->item_list, children);
			g_list_free(children);
			break;
		}
	}
}

static void
ganv_group_class_init(GanvGroupClass* class)
{
	GObjectClass*   gobject_class;
	GtkObjectClass* object_class;
	GanvItemClass*  item_class;

	gobject_class = (GObjectClass*)class;
	object_class  = (GtkObjectClass*)class;
	item_class    = (GanvItemClass*)class;

	group_parent_class = g_type_class_peek_parent(class);

	gobject_class->set_property = ganv_group_set_property;
	gobject_class->get_property = ganv_group_get_property;

	object_class->destroy = ganv_group_destroy;

	item_class->add       = ganv_group_add;
	item_class->remove    = ganv_group_remove;
	item_class->update    = ganv_group_update;
	item_class->realize   = ganv_group_realize;
	item_class->unrealize = ganv_group_unrealize;
	item_class->map       = ganv_group_map;
	item_class->unmap     = ganv_group_unmap;
	item_class->draw      = ganv_group_draw;
	item_class->point     = ganv_group_point;
	item_class->bounds    = ganv_group_bounds;
}
