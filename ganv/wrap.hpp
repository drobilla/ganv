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

#ifndef GANV_WRAP_HPP
#define GANV_WRAP_HPP

#include <glib.h>

#define CONNECT_PROP_SIGNAL(gobj, name, notify, handler) \
	g_signal_connect(gobj, "notify::" #name, \
	                 G_CALLBACK(notify), &_signal_##name); \
	_signal_##name.connect(sigc::mem_fun(this, handler));

#define SIGNAL1(name, argtype) \
public: \
	virtual bool on_##name(argtype) { return true; } \
	sigc::signal<bool, argtype>& signal_##name() { return _signal_##name; } \
private: \
	sigc::signal<bool, argtype> _signal_##name;

#define RW_PROPERTY(type, name) \
	virtual type get_##name() const { \
		type value; \
		g_object_get(G_OBJECT(_gobj), #name, &value, nullptr); \
		return value; \
	} \
	virtual void set_##name(type value) { \
		g_object_set(G_OBJECT(_gobj), #name, value, nullptr); \
	} \
	SIGNAL1(name, type) \
	public:

#define RW_OBJECT_PROPERTY(type, name) \
	type get_##name() const { \
		if (!_gobj) return nullptr; \
		Ganv##type ptr; \
		g_object_get(G_OBJECT(_gobj), #name, &ptr, nullptr); \
		return Glib::wrap(ptr); \
	} \
	void set_##name(type value) { \
		if (!_gobj) return; \
		ganv_item_set(GANV_ITEM(_gobj), \
		              #name, value->gobj(), \
		              nullptr); \
	}

#define METHOD0(prefix, name) \
	virtual void name() { \
		prefix##_##name(gobj()); \
	}

#define METHOD1(prefix, name, t1, a1) \
	virtual void name(t1 a1) { \
		prefix##_##name(gobj(), a1); \
	}

#define METHODRET0(prefix, ret, name) \
	virtual ret name() const { \
		return prefix##_##name(gobj()); \
	}

#define METHODRET1(prefix, ret, name, t1, a1) \
	virtual ret name(t1 a1) { \
		return prefix##_##name(gobj(), a1); \
	}

#define METHODRET2(prefix, ret, name, t1, a1, t2, a2) \
	virtual ret name(t1 a1, t2 a2) { \
		return prefix##_##name(gobj(), a1, a2); \
	}

#define METHODRETWRAP0(prefix, ret, name) \
	virtual ret name() const { \
		if (gobj()) { \
			return Glib::wrap(prefix##_##name(gobj())); \
		} \
		return nullptr; \
	}

#define METHOD2(prefix, name, t1, a1, t2, a2) \
	virtual void name(t1 a1, t2 a2) { \
		prefix##_##name(gobj(), a1, a2); \
	}

#define METHOD3(prefix, name, t1, a1, t2, a2, t3, a3) \
	virtual void name(t1 a1, t2 a2, t3 a3) { \
		prefix##_##name(gobj(), a1, a2, a3); \
	}

#define METHOD4(prefix, name, t1, a1, t2, a2, t3, a3, t4, a4) \
	virtual void name(t1 a1, t2 a2, t3 a3, t4 a4) { \
		prefix##_##name(gobj(), a1, a2, a3, a4); \
	}

#define GANV_GLIB_WRAP(Name) \
	namespace Ganv { \
	class Name; \
	} \
	namespace Glib { \
	/** Return a Ganv::CPPType wrapper for a CType. */ \
	static inline Ganv::Name* \
	wrap(Ganv##Name* gobj) \
	{ \
		if (gobj) { \
			return static_cast<Ganv::Name*>(ganv_item_get_wrapper(GANV_ITEM(gobj))); \
		} \
		return nullptr; \
	} \
	/** Return a Ganv::CPPType wrapper for a CType. */ \
	static inline const Ganv::Name* \
	wrap(const Ganv##Name* gobj) \
	{ \
		if (gobj) { \
			return static_cast<const Ganv::Name*>(ganv_item_get_wrapper(GANV_ITEM(gobj))); \
		} \
		return nullptr; \
	} \
	}

#endif  // GANV_WRAP_HPP
