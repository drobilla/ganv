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

#ifndef GANV_BOX_HPP
#define GANV_BOX_HPP

#include <ganv/Node.hpp>
#include <ganv/box.h>
#include <ganv/node.h>
#include <ganv/types.h>
#include <ganv/wrap.hpp>

#include <glib.h>

namespace Ganv {

class Canvas;

class Box : public Node
{
public:
	Box(Canvas* canvas, GanvBox* gobj)
		: Node(canvas, GANV_NODE(gobj))
	{}

	RW_PROPERTY(gboolean, beveled)

	METHODRET0(ganv_box, double, get_x1)
	METHODRET0(ganv_box, double, get_y1)
	METHODRET0(ganv_box, double, get_x2)
	METHODRET0(ganv_box, double, get_y2)
	METHODRET0(ganv_box, double, get_width)
	METHOD1(ganv_box, set_width, double, width)
	METHODRET0(ganv_box, double, get_height)
	METHOD1(ganv_box, set_height, double, height)

	double get_border_width() const override {
		return ganv_box_get_border_width(gobj());
	}

	GanvBox*       gobj()       { return GANV_BOX(_gobj); }
	const GanvBox* gobj() const { return GANV_BOX(_gobj); }
};

} // namespace Ganv

#endif // GANV_BOX_HPP
