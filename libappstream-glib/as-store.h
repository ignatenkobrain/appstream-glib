/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
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

#if !defined (__APPSTREAM_GLIB_H) && !defined (AS_COMPILATION)
#error "Only <appstream-glib.h> can be included directly."
#endif

#ifndef __AS_STORE_H
#define __AS_STORE_H

#include <glib-object.h>
#include <gio/gio.h>

#include "as-app.h"
#include "as-node.h"

#define AS_TYPE_STORE		(as_store_get_type())
#define AS_STORE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), AS_TYPE_STORE, AsStore))
#define AS_STORE_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), AS_TYPE_STORE, AsStoreClass))
#define AS_IS_STORE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), AS_TYPE_STORE))
#define AS_IS_STORE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), AS_TYPE_STORE))
#define AS_STORE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), AS_TYPE_STORE, AsStoreClass))

G_BEGIN_DECLS

typedef struct _AsStore		AsStore;
typedef struct _AsStoreClass	AsStoreClass;

struct _AsStore
{
	GObject			parent;
};

struct _AsStoreClass
{
	GObjectClass		parent_class;
	void			(*changed)	(AsStore	*store);
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
};

/**
 * AsStoreLoadFlags:
 * @AS_STORE_LOAD_FLAG_NONE:			No extra flags to use
 * @AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM:		The system app-info AppStream data
 * @AS_STORE_LOAD_FLAG_APP_INFO_USER:		The per-user app-info AppStream data
 * @AS_STORE_LOAD_FLAG_APP_INSTALL:		The ubuntu-specific app-install data
 *
 * The flags to use when loading the store.
 **/
typedef enum {
	AS_STORE_LOAD_FLAG_NONE			= 0,	/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM	= 1,	/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APP_INFO_USER	= 2,	/* Since: 0.1.2 */
	AS_STORE_LOAD_FLAG_APP_INSTALL		= 4,	/* Since: 0.1.2 */
	/*< private >*/
	AS_STORE_LOAD_FLAG_LAST
} AsStoreLoadFlags;

/**
 * AsStoreError:
 * @AS_STORE_ERROR_FAILED:			Generic failure
 *
 * The error type.
 **/
typedef enum {
	AS_STORE_ERROR_FAILED,
	/*< private >*/
	AS_STORE_ERROR_LAST
} AsStoreError;

#define	AS_STORE_ERROR				as_store_error_quark ()

GType		 as_store_get_type		(void);
AsStore		*as_store_new			(void);
GQuark		 as_store_error_quark		(void);

/* getters */
guint		 as_store_get_size		(AsStore	*store);

/* object methods */
gboolean	 as_store_from_file		(AsStore	*store,
						 GFile		*file,
						 const gchar	*icon_root,
						 GCancellable	*cancellable,
						 GError		**error);
gboolean	 as_store_from_xml		(AsStore	*store,
						 const gchar	*data,
						 gssize		 data_len,
						 const gchar	*icon_root,
						 GError		**error);
gboolean	 as_store_load			(AsStore	*store,
						 AsStoreLoadFlags flags,
						 GCancellable	*cancellable,
						 GError		**error);
GPtrArray	*as_store_get_apps		(AsStore	*store);
GPtrArray	*as_store_get_apps_by_metadata	(AsStore	*store,
						 const gchar	*key,
						 const gchar	*value);
AsApp		*as_store_get_app_by_id		(AsStore	*store,
						 const gchar	*id);
AsApp		*as_store_get_app_by_pkgname	(AsStore	*store,
						 const gchar	*pkgname);
void		 as_store_add_app		(AsStore	*store,
						 AsApp		*app);
void		 as_store_remove_app		(AsStore	*store,
						 AsApp		*app);
GString		*as_store_to_xml		(AsStore	*store,
						 AsNodeToXmlFlags flags);
gboolean	 as_store_to_file		(AsStore	*store,
						 GFile		*file,
						 AsNodeToXmlFlags flags,
						 GCancellable	*cancellable,
						 GError		**error);
const gchar	*as_store_get_origin		(AsStore	*store);
void		 as_store_set_origin		(AsStore	*store,
						 const gchar	*origin);
gdouble		 as_store_get_api_version	(AsStore	*store);
void		 as_store_set_api_version	(AsStore	*store,
						 gdouble	 api_version);

G_END_DECLS

#endif /* __AS_STORE_H */
