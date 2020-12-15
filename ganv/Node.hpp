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

#ifndef GANV_NODE_HPP
#define GANV_NODE_HPP

#include "ganv/Item.hpp"
#include "ganv/item.h"
#include "ganv/node.h"
#include "ganv/types.h"
#include "ganv/wrap.hpp"

#include <glib-object.h>
#include <glib.h>
#include <glibmm/object.h>
#include <gobject/gclosure.h>
#include <sigc++/signal.h>

GANV_GLIB_WRAP(Node) // IWYU pragma: keep

namespace Ganv {

class Canvas;

/** An object a Edge can connect to.
 */
class Node : public Item {
public:
	Node(Canvas*, GanvNode* gobj)
		: Item(GANV_ITEM(g_object_ref(gobj)))
	{
		g_signal_connect(gobj, "moved", G_CALLBACK(on_moved), this);
		CONNECT_PROP_SIGNAL(gobj, selected, on_notify_bool, &Node::on_selected)
	}

	~Node() override {
		g_object_unref(_gobj);
	}

	RW_PROPERTY(gboolean, can_tail)
	RW_PROPERTY(gboolean, can_head)
	RW_PROPERTY(gboolean, is_source)

	gboolean is_within(double x1, double y1, double x2, double y2) const {
		return ganv_node_is_within(gobj(), x1, y1, x2, y2);
	}

	RW_PROPERTY(const char*, label)
	RW_PROPERTY(double, border_width)
	RW_PROPERTY(double, dash_length)
	RW_PROPERTY(double, dash_offset)
	RW_PROPERTY(guint, fill_color)
	RW_PROPERTY(guint, border_color)
	RW_PROPERTY(gboolean, selected)
	RW_PROPERTY(gboolean, highlighted)
	RW_PROPERTY(gboolean, draggable)
	RW_PROPERTY(gboolean, grabbed)

	RW_OBJECT_PROPERTY(Node*, partner)

	GanvNode*       gobj()       { return GANV_NODE(_gobj); }
	const GanvNode* gobj() const { return GANV_NODE(_gobj); }

	void move(const double dx, const double dy) override {
		ganv_node_move(gobj(), dx, dy);
	}

	METHOD2(ganv_node, move_to, double, x, double, y)

	METHOD0(ganv_node, disconnect)

	sigc::signal<void, double, double>& signal_moved() {
		return _signal_moved;
	}

private:
	sigc::signal<void, double, double> _signal_moved;

	static void on_moved(GanvNode* node, double x, double y) {
		Glib::wrap(node)->_signal_moved.emit(x, y);
	}

	/* GCC 4.6 can't handle this
	template<typename T>
	static void on_notify(GObject* gobj, GParamSpec* pspec, gpointer signal) {
		T value;
		g_object_get(gobj, g_param_spec_get_name(pspec), &value, nullptr);
		((sigc::signal<bool, T>*)signal)->emit(value);
	}
	*/
	static void on_notify_bool(GObject*    gobj,
	                           GParamSpec* pspec,
	                           gpointer    signal) {
		gboolean value = FALSE;
		g_object_get(gobj, g_param_spec_get_name(pspec), &value, nullptr);
		static_cast<sigc::signal<bool, gboolean>*>(signal)->emit(value);
	}
};

} // namespace Ganv

#endif // GANV_NODE_HPP
