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

#ifndef GANV_WRAP_HPP
#define GANV_WRAP_HPP

#include <glib.h>
#define RW_PROPERTY(type, name) \
	virtual type get_##name() const { \
		type value; \
		g_object_get(G_OBJECT(_gobj), #name, &value, NULL); \
		return value; \
	} \
	virtual void set_##name(type value) { \
		g_object_set(G_OBJECT(_gobj), #name, value, NULL); \
	}

#define RW_OBJECT_PROPERTY(type, name) \
	type get_##name() const { \
		if (!_gobj) return NULL; \
		Ganv##type ptr; \
		g_object_get(G_OBJECT(_gobj), #name, &ptr, NULL); \
		return Glib::wrap(ptr); \
	} \
	void set_##name(type value) { \
		if (!_gobj) return; \
		ganv_item_set(GANV_ITEM(_gobj), \
		              #name, value->gobj(), \
		              NULL); \
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

#define METHODRETWRAP0(prefix, ret, name) \
	virtual ret name() const { \
		if (gobj()) { \
			return Glib::wrap(prefix##_##name(gobj())); \
		} else { \
			return NULL; \
		} \
	}

#define METHOD2(prefix, name, t1, a1, t2, a2) \
	virtual void name(t1 a1, t2 a2) { \
		prefix##_##name(gobj(), a1, a2); \
	}

#define SIGNAL1(name, argtype) \
public: \
	virtual bool on_##name(argtype arg) { \
		return _signal_##name.emit(arg); \
	} \
	sigc::signal<bool, argtype>& signal_##name() { return _signal_##name; } \
private: \
	sigc::signal<bool, argtype> _signal_##name;

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
			GQuark key = g_quark_from_string("ganvmm"); \
			return (Ganv::Name*)g_object_get_qdata(G_OBJECT(gobj), key); \
		} else { \
			return NULL; \
		} \
	} \
	}

#endif  // GANV_WRAP_HPP
