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

#ifndef GANV_EDGE_HPP
#define GANV_EDGE_HPP

#include "ganv/Canvas.hpp"
#include "ganv/Item.hpp"
#include "ganv/Node.hpp"
#include "ganv/edge.h"
#include "ganv/item.h"
#include "ganv/types.h"
#include "ganv/wrap.hpp"

#include <glib-object.h>
#include <glib.h>

#include <cstdint>

GANV_GLIB_WRAP(Edge)

namespace Ganv {

/** A edge (line) between two Node objects.
 *
 * @ingroup Ganv
 */
class Edge : public Item
{
public:
	Edge(Canvas&  canvas,
	     Node*    tail,
	     Node*    head,
	     uint32_t color = 0,
	     bool     show_arrowhead = false,
	     bool     curved = true)
		: Item(GANV_ITEM(ganv_edge_new(canvas.gobj(),
		                               tail->gobj(),
		                               head->gobj(),
		                               "color", color,
		                               "curved", static_cast<gboolean>(curved),
		                               "arrowhead", static_cast<gboolean>(show_arrowhead),
		                               nullptr)))
	{}

	Edge(GanvEdge* gobj)
		: Item(GANV_ITEM(gobj))
	{}

	Edge(const Edge& copy) = delete;
	Edge& operator=(const Edge& other) = delete;

	~Edge() override {
		if (_gobj && ganv_item_get_parent(_gobj)) {
			g_object_unref(_gobj);
		}
	}

	gboolean is_within(double x1, double y1, double x2, double y2) const {
		return ganv_edge_is_within(gobj(), x1, y1, x2, y2);
	}

	RW_PROPERTY(gboolean, constraining)
	RW_PROPERTY(gboolean, curved)
	RW_PROPERTY(gboolean, selected)
	RW_PROPERTY(gboolean, highlighted)
	RW_PROPERTY(guint,    color)
	RW_PROPERTY(gdouble,  handle_radius)

	METHODRETWRAP0(ganv_edge, Node*, get_tail)
	METHODRETWRAP0(ganv_edge, Node*, get_head)

	GanvEdge*       gobj()       { return reinterpret_cast<GanvEdge*>(_gobj); }
	const GanvEdge* gobj() const { return reinterpret_cast<GanvEdge*>(_gobj); }
};

} // namespace Ganv

#endif // GANV_EDGE_HPP
