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

#ifndef GANV_ITEM_HPP
#define GANV_ITEM_HPP

#include <ganv/Canvas.hpp>
#include <ganv/item.h>
#include <ganv/wrap.hpp>

#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib.h>
#include <gobject/gclosure.h>
#include <gtk/gtk.h>
#include <sigc++/signal.h>
#include <sigc++/trackable.h>

GANV_GLIB_WRAP(Item)

namespace Ganv {

/** An item on the canvas.
 */
class Item : public sigc::trackable {
public:
	explicit Item(GanvItem* gobj)
		: _gobj(gobj)
	{
		ganv_item_set_wrapper(gobj, this);
		if (gobj && ganv_item_get_parent(gobj)) {
			g_signal_connect(
				G_OBJECT(_gobj), "event", G_CALLBACK(on_item_event), this);
		}
	}

	Item(const Item&) = delete;
	Item& operator=(const Item&) = delete;

	Item(Item&&)  = delete;
	Item& operator=(Item&&) = delete;

	virtual ~Item() {
		gtk_object_destroy(GTK_OBJECT(_gobj));
	}

	RW_PROPERTY(double, x)
	RW_PROPERTY(double, y)

	METHOD0(ganv_item, raise)
	METHOD0(ganv_item, lower)
	METHOD2(ganv_item, move, double, dx, double, dy)
	METHOD0(ganv_item, show)
	METHOD0(ganv_item, hide)
	METHOD2(ganv_item, i2w, double*, x, double*, y)
	METHOD2(ganv_item, w2i, double*, x, double*, y)
	METHOD0(ganv_item, grab_focus)

	Canvas* canvas() const {
		return Glib::wrap(ganv_item_get_canvas(_gobj));
	}

	GanvItem* gobj() const { return _gobj; }

	SIGNAL1(event, GdkEvent*)
	SIGNAL1(click, GdkEventButton*)

protected:
	GanvItem* _gobj;

private:
	static gboolean on_item_event(GanvItem*, GdkEvent* ev, void* item)
	{
		return static_cast<Item*>(item)->signal_event().emit(ev);
	}
};

} // namespace Ganv

#endif // GANV_ITEM_HPP
