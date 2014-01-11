/* This file is part of Ganv.
 * Copyright 2007-2012 David Robillard <http://drobilla.net>
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

#ifndef GANV_ITEM_HPP
#define GANV_ITEM_HPP

#include <assert.h>

#include <glib.h>

#include <sigc++/signal.h>
#include <sigc++/trackable.h>

#include "ganv/Canvas.hpp"
#include "ganv/item.h"
#include "ganv/wrap.hpp"

namespace Ganv {

class Canvas;

/** An item on the canvas.
 */
class Item : public sigc::trackable {
public:
	Item(GanvItem* gobj)
		: _gobj(gobj)
	{
		GQuark wrapper_key = g_quark_from_string("ganvmm");
		if (gobj && gobj->parent) {
			g_object_set_qdata(G_OBJECT(_gobj), wrapper_key, this);
			g_signal_connect(
				G_OBJECT(_gobj), "event", G_CALLBACK(on_item_event), this);
		}
	}

	virtual ~Item() {
		gtk_object_destroy(GTK_OBJECT(_gobj));
	}

	RW_PROPERTY(double, x)
	RW_PROPERTY(double, y)

	METHOD0(ganv_item, show);
	METHOD0(ganv_item, hide);
	METHOD2(ganv_item, move, double, dx, double, dy);

	GanvItem* property_parent() const {
		GanvItem* parent;
		g_object_get(G_OBJECT(_gobj), "parent", &parent, NULL);
		return parent;
	}

	Canvas* canvas() const {
		return Glib::wrap(GANV_CANVAS(_gobj->canvas));
	}

	GanvItem* gobj() const { return _gobj; }

	SIGNAL1(event, GdkEvent*)
	SIGNAL1(click, GdkEventButton*)

protected:
	GanvItem* const _gobj;

private:
	static gboolean
	on_item_event(GanvItem* canvasitem,
	              GdkEvent* ev,
	              void*     item)
	{
		return ((Item*)item)->signal_event().emit(ev);
	}
};

} // namespace Ganv

#endif // GANV_ITEM_HPP
