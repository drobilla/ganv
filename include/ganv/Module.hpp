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

#ifndef GANV_MODULE_HPP
#define GANV_MODULE_HPP

#include <ganv/Box.hpp>
#include <ganv/Canvas.hpp>
#include <ganv/Port.hpp>
#include <ganv/box.h>
#include <ganv/item.h>
#include <ganv/module.h>
#include <ganv/types.h>
#include <ganv/wrap.hpp>

#include <glib.h>
#include <gtkmm/widget.h>

#include <string>

GANV_GLIB_WRAP(Module)

namespace Ganv {

/** A rectangular Item which can hold a Port.
 *
 * @ingroup Ganv
 */
class Module : public Box
{
public:
	Module(Canvas&            canvas,
	       const std::string& name,
	       double             x                = 0,
	       double             y                = 0,
	       bool               show_title       = true)
		: Box(&canvas,
		      GANV_BOX(ganv_item_new(GANV_ITEM(canvas.root()),
		                             ganv_module_get_type(),
		                             "x", x,
		                             "y", y,
		                             "can-tail", FALSE,
		                             "can-head", FALSE,
		                             "radius-tl", 4.0,
		                             "radius-tr", 4.0,
		                             "radius-br", 4.0,
		                             "radius-bl", 4.0,
		                             "border-width", 2.0,
		                             "label", name.c_str(),
		                             "draggable", TRUE,
		                             nullptr)))
	{
		(void)show_title;
	}

	template<typename P, typename C>
	class iterator_base {
	public:
		iterator_base(GanvModule* m, guint i) : _module(m), _index(i) {}
		template<typename T, typename U>
		explicit iterator_base(const iterator_base<T, U>& i)
			: _module(i._module)
			, _index(i._index)
		{}
		P* operator*() const {
			return Glib::wrap(ganv_module_get_port(_module, _index));
		}
		P* operator->() const {
			return Glib::wrap(ganv_module_get_port(_module, _index));
		}
		iterator_base operator++(int) const {
			return iterator_base<P, C>(_index + 1);
		}
		iterator_base& operator++() {
			++_index; return *this;
		}
		bool operator==(const iterator_base<P, C>& i) const {
			return _index == i._index;
		}
		bool operator!=(const iterator_base<P, C>& i) const {
			return _index != i._index;
		}
	private:
		template<typename T, typename U> friend class iterator_base;
		GanvModule* _module;
		guint       _index;
	};

	using iterator       = iterator_base<Port, GanvPort>;
	using const_iterator = iterator_base<const Port, const GanvPort>;

	iterator       begin()       { return {gobj(), 0}; }
	iterator       end()         { return {gobj(), num_ports()}; }
	iterator       back()        { return {gobj(), num_ports() - 1}; }
	const_iterator begin() const { return {const_cast<GanvModule*>(gobj()), 0}; }
	const_iterator end()   const { return {const_cast<GanvModule*>(gobj()), num_ports()}; }
	const_iterator back()  const { return {const_cast<GanvModule*>(gobj()), num_ports() - 1}; }

	void embed(Gtk::Widget* widget) {
		ganv_module_embed(gobj(), widget ? widget->gobj() : nullptr);
	}

	Port* get_port(guint index) {
		return Glib::wrap(ganv_module_get_port(gobj(), index));
	}

	METHOD2(ganv_module, for_each_port, GanvPortFunc, f, void*, data)

	METHODRET0(ganv_module, guint, num_ports)

	RW_PROPERTY(gboolean, stacked)

	METHODRET0(ganv_module, double, get_empty_port_breadth)
	METHODRET0(ganv_module, double, get_empty_port_depth)

	GanvModule*       gobj()       { return GANV_MODULE(_gobj); }
	const GanvModule* gobj() const { return GANV_MODULE(_gobj); }
};

} // namespace Ganv

#endif // GANV_MODULE_HPP
