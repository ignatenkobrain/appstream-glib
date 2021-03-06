/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__APPSTREAM_GLIB_PRIVATE_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_APP_PRIVATE_H
#define __AS_APP_PRIVATE_H

#include <glib-object.h>

#include "as-app.h"

G_BEGIN_DECLS

typedef enum {
	AS_APP_PROBLEM_NONE			= 0,
	AS_APP_PROBLEM_NO_XML_HEADER		= 1 << 0,
	AS_APP_PROBLEM_NO_COPYRIGHT_INFO	= 1 << 1,
	AS_APP_PROBLEM_DEPRECATED_LICENCE	= 1 << 2,
	AS_APP_PROBLEM_MULTIPLE_ENTRIES		= 1 << 3,
	AS_APP_PROBLEM_INTLTOOL_NAME		= 1 << 4,
	AS_APP_PROBLEM_INTLTOOL_SUMMARY		= 1 << 5,
	AS_APP_PROBLEM_INTLTOOL_DESCRIPTION	= 1 << 6,
	AS_APP_PROBLEM_LAST
} AsAppProblems;

AsAppProblems	 as_app_get_problems		(AsApp		*app);
guint		 as_app_get_name_size		(AsApp		*app);
guint		 as_app_get_comment_size	(AsApp		*app);
guint		 as_app_get_description_size	(AsApp		*app);

GNode		*as_app_node_insert		(AsApp		*app,
						 GNode		*parent,
						 gdouble	 api_version);
gboolean	 as_app_node_parse		(AsApp		*app,
						 GNode		*node,
						 GError		**error);

G_END_DECLS

#endif /* __AS_APP_PRIVATE_H */
