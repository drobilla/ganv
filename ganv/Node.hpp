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

#ifndef GANV_NODE_HPP
#define GANV_NODE_HPP

#include <glib.h>
#include <assert.h>

#include "ganv/canvas-base.h"
#include "ganv/node.h"
#include "ganv/Item.hpp"

GANV_GLIB_WRAP(Node)

namespace Ganv {

class Canvas;
class Node;

/** An object a Edge can connect to.
 */
class Node : public Item {
public:
	Node(Canvas* canvas, GanvNode* gobj)
		: Item(GANV_ITEM(g_object_ref(gobj)))
	{
		g_signal_connect(gobj, "moved", G_CALLBACK(on_moved), this);
	}

	~Node() {
		g_object_unref(_gobj);
	}

	RW_PROPERTY(const char*, label)
	RW_PROPERTY(double, dash_length)
	RW_PROPERTY(double, dash_offset)
	RW_PROPERTY(double, border_width)
	RW_PROPERTY(guint, fill_color)
	RW_PROPERTY(guint, border_color)
	RW_PROPERTY(gboolean, can_tail)
	RW_PROPERTY(gboolean, can_head)
	RW_PROPERTY(gboolean, selected)
	RW_PROPERTY(gboolean, highlighted)
	RW_PROPERTY(gboolean, draggable)

	RW_OBJECT_PROPERTY(Node*, partner);

	METHOD1(ganv_node, tick, double, seconds);

	virtual gboolean is_within(double x1, double y1,
	                           double x2, double y2) const {
		return ganv_node_is_within(gobj(), x1, y1, x2, y2);
	}

	GanvNode*       gobj()       { return GANV_NODE(_gobj); }
	const GanvNode* gobj() const { return GANV_NODE(_gobj); }

	METHOD2(ganv_node, move, double, dx, double, dy)
	METHOD2(ganv_node, move_to, double, x, double, y)

	METHOD0(ganv_node, disconnect);

	sigc::signal<void, double, double> signal_moved;

	static void on_moved(GanvNode* node, double x, double y) {
		Glib::wrap(node)->signal_moved.emit(x, y);
	}
};

} // namespace Ganv

#endif // GANV_NODE_HPP
