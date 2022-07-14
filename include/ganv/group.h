/* This file is part of Ganv.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

// IWYU pragma: no_include "ganv-private.h"

#ifndef GANV_GROUP_H
#define GANV_GROUP_H

#include "ganv/item.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

/* Based on GnomeCanvasGroup, by Federico Mena <federico@nuclecu.unam.mx>
 * and Raph Levien <raph@gimp.org>
 * Copyright 1997-2000 Free Software Foundation
 */

#define GANV_TYPE_GROUP            (ganv_group_get_type())
#define GANV_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GANV_TYPE_GROUP, GanvGroup))
#define GANV_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GANV_TYPE_GROUP, GanvGroupClass))
#define GANV_IS_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GANV_TYPE_GROUP))
#define GANV_IS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GANV_TYPE_GROUP))
#define GANV_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GANV_TYPE_GROUP, GanvGroupClass))

struct _GanvGroup;
struct _GanvGroupClass;

typedef struct _GanvGroup        GanvGroup;
typedef struct _GanvGroupPrivate GanvGroupPrivate;
typedef struct _GanvGroupClass   GanvGroupClass;

struct _GanvGroup {
	GanvItem          item;
	GanvGroupPrivate* impl;
};

struct _GanvGroupClass {
	GanvItemClass parent_class;

	/* Reserved for future expansion */
	gpointer spare_vmethods[4];
};

GType ganv_group_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif  /* GANV_GROUP_H */
