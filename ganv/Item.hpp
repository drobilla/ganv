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

#ifndef GANV_ITEM_HPP
#define GANV_ITEM_HPP

#include <assert.h>

#include <glib.h>
#include <libgnomecanvas/gnome-canvas.h>

#include <sigc++/signal.h>
#include <sigc++/trackable.h>

#include "ganv/wrap.hpp"
#include "ganv/Canvas.hpp"

namespace Ganv {

class Canvas;

/** An item on the canvas.
 */
class Item : public sigc::trackable {
public:
	Item(GnomeCanvasItem* gobj)
		: _gobj(gobj)
	{
		GQuark wrapper_key = g_quark_from_string("ganvmm");
		if (gobj && gobj->parent) {
			g_object_set_qdata(G_OBJECT(_gobj), wrapper_key, this);
			g_signal_connect(G_OBJECT(_gobj),
			                 "event", G_CALLBACK(on_item_event), this);
		}
	}

	virtual ~Item() {}
	RW_PROPERTY(double, x)
	RW_PROPERTY(double, y)

	METHOD0(gnome_canvas_item, show);
	METHOD0(gnome_canvas_item, hide);
	METHOD0(gnome_canvas_item, raise_to_top);
	METHOD2(gnome_canvas_item, move, double, dx, double, dy);

	GnomeCanvasItem* property_parent() const {
		GnomeCanvasItem* parent;
		g_object_get(G_OBJECT(_gobj), "parent", &parent, NULL);
		return parent;
	}

	Canvas* canvas() const {
		return Glib::wrap(GANV_CANVAS(_gobj->canvas));
	}

	GnomeCanvasItem* gobj() const { return _gobj; }

	SIGNAL(event, GdkEvent*)
	SIGNAL(click, GdkEventButton*)

protected:
	GnomeCanvasItem* const _gobj;

private:
	static gboolean
	on_item_event(GnomeCanvasItem* canvasitem,
	              GdkEvent*        ev,
	              void*            item)
	{
		return ((Item*)item)->on_event(ev);
	}
};

} // namespace Ganv

#endif // GANV_ITEM_HPP
