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

typedef gpointer gobject;

/**
   A case in a switch statement in a set_properties implementation.
   @prop: Property enumeration ID.
   @type: Name of the value type, e.g. uint for guint.
   @field: Field to set to the new value.
*/
#define SET_CASE(prop, type, field) \
	case PROP_##prop: { \
		const g##type tmp = g_value_get_##type(value); \
		if (field != tmp) { \
			field = tmp; \
			GanvItem* item = GANV_ITEM(object); \
			if (item->canvas) { \
				ganv_item_request_update(item); \
			} \
		} \
		break; \
	}

/**
   A case in a switch statement in a get_properties implementation.
   @prop: Property enumeration ID.
   @type: Name of the value type, e.g. uint for guint.
   @field: Field to set to the new value.
*/
#define GET_CASE(prop, type, field) \
	case PROP_##prop: \
	g_value_set_##type(value, field); \
	break;
