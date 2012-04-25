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

#ifndef GANV_BOX_HPP
#define GANV_BOX_HPP

#include "ganv/Node.hpp"
#include "ganv/box.h"

namespace Ganv {

class Box : public Node
{
public:
	Box(Canvas* canvas, GanvBox* gobj)
		: Node(canvas, GANV_NODE(gobj))
	{}

	RW_PROPERTY(const char*, label);

	METHODRET0(ganv_box, double, get_width)
	METHOD1(ganv_box, set_width, double, width)
	METHODRET0(ganv_box, double, get_height)
	METHOD1(ganv_box, set_height, double, height)

	GanvBox*       gobj()       { return GANV_BOX(_gobj); }
	const GanvBox* gobj() const { return GANV_BOX(_gobj); }
};

} // namespace Ganv

#endif // GANV_BOX_HPP
