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

/**
 * SECTION:as-store
 * @short_description: a hashed array store of applications
 * @include: appstream-glib.h
 * @stability: Stable
 *
 * This store contains both an array of #AsApp's but also a pair of hashes
 * to quickly retrieve an application from the ID or package name.
 *
 * Applications can also be removed, and the whole store can be loaded and
 * saved to a compressed XML file.
 *
 * See also: #AsApp
 */

#include "config.h"

#include "as-app-private.h"
#include "as-cleanup.h"
#include "as-node-private.h"
#include "as-store.h"
#include "as-utils-private.h"

#define AS_API_VERSION_NEWEST	0.6

typedef struct _AsStorePrivate	AsStorePrivate;
struct _AsStorePrivate
{
	gchar			*origin;
	gdouble			 api_version;
	GPtrArray		*array;		/* of AsApp */
	GHashTable		*hash_id;	/* of AsApp{id_full} */
	GHashTable		*hash_pkgname;	/* of AsApp{pkgname} */
	GPtrArray		*file_monitors;	/* of GFileMonitor */
};

G_DEFINE_TYPE_WITH_PRIVATE (AsStore, as_store, G_TYPE_OBJECT)

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

#define GET_PRIVATE(o) (as_store_get_instance_private (o))

/**
 * as_store_error_quark:
 *
 * Return value: An error quark.
 *
 * Since: 0.1.2
 **/
GQuark
as_store_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("AsStoreError");
	return quark;
}

/**
 * as_store_finalize:
 **/
static void
as_store_finalize (GObject *object)
{
	AsStore *store = AS_STORE (object);
	AsStorePrivate *priv = GET_PRIVATE (store);

	g_free (priv->origin);
	g_ptr_array_unref (priv->array);
	g_ptr_array_unref (priv->file_monitors);
	g_hash_table_unref (priv->hash_id);
	g_hash_table_unref (priv->hash_pkgname);

	G_OBJECT_CLASS (as_store_parent_class)->finalize (object);
}

/**
 * as_store_init:
 **/
static void
as_store_init (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->api_version = AS_API_VERSION_NEWEST;
	priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->hash_id = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       NULL,
					       NULL);
	priv->hash_pkgname = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    (GDestroyNotify) g_object_unref);
	priv->file_monitors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * as_store_class_init:
 **/
static void
as_store_class_init (AsStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/**
	 * AsStore::changed:
	 * @device: the #AsStore instance that emitted the signal
	 *
	 * The ::changed signal is emitted when the files backing the store
	 * have changed.
	 *
	 * Since: 0.1.2
	 **/
	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AsStoreClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	object_class->finalize = as_store_finalize;
}

/**
 * as_store_get_size:
 * @store: a #AsStore instance.
 *
 * Gets the size of the store after deduplication and prioritization has
 * taken place.
 *
 * Returns: the number of usable applications in the store
 *
 * Since: 0.1.0
 **/
guint
as_store_get_size (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), 0);
	return priv->array->len;
}

/**
 * as_store_get_apps:
 * @store: a #AsStore instance.
 *
 * Gets an array of all the valid applications in the store.
 *
 * Returns: (element-type AsApp) (transfer none): an array
 *
 * Since: 0.1.0
 **/
GPtrArray *
as_store_get_apps (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return priv->array;
}

/**
 * as_store_get_apps_by_metadata:
 * @store: a #AsStore instance.
 * @key: metadata key
 * @value: metadata value
 *
 * Gets an array of all the applications that match a specific metadata element.
 *
 * Returns: (element-type AsApp) (transfer container): an array
 *
 * Since: 0.1.4
 **/
GPtrArray *
as_store_get_apps_by_metadata (AsStore *store,
			       const gchar *key,
			       const gchar *value)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *apps;
	guint i;

	g_return_val_if_fail (AS_IS_STORE (store), NULL);

	/* find all the apps with this specific metadata key */
	apps = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (g_strcmp0 (as_app_get_metadata_item (app, key), value) != 0)
			continue;
		g_ptr_array_add (apps, g_object_ref (app));
	}
	return apps;
}

/**
 * as_store_get_app_by_id:
 * @store: a #AsStore instance.
 * @id: the application full ID.
 *
 * Finds an application in the store by ID.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.1.0
 **/
AsApp *
as_store_get_app_by_id (AsStore *store, const gchar *id)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return g_hash_table_lookup (priv->hash_id, id);
}

/**
 * as_store_get_app_by_pkgname:
 * @store: a #AsStore instance.
 * @pkgname: the package name.
 *
 * Finds an application in the store by package name.
 *
 * Returns: (transfer none): a #AsApp or %NULL
 *
 * Since: 0.1.0
 **/
AsApp *
as_store_get_app_by_pkgname (AsStore *store, const gchar *pkgname)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_return_val_if_fail (AS_IS_STORE (store), NULL);
	return g_hash_table_lookup (priv->hash_pkgname, pkgname);
}

/**
 * as_store_remove_app:
 * @store: a #AsStore instance.
 * @app: a #AsApp instance.
 *
 * Removes an application from the store if it exists.
 *
 * Since: 0.1.0
 **/
void
as_store_remove_app (AsStore *store, AsApp *app)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_hash_table_remove (priv->hash_id, as_app_get_id_full (app));
	g_ptr_array_remove (priv->array, app);
}

/**
 * as_store_add_app:
 * @store: a #AsStore instance.
 * @app: a #AsApp instance.
 *
 * Adds an application to the store. If a lower priority application has already
 * been added then this new application will replace it.
 *
 * Additionally only applications where the kind is known will be added.
 *
 * Since: 0.1.0
 **/
void
as_store_add_app (AsStore *store, AsApp *app)
{
	AsApp *item;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *pkgnames;
	const gchar *id;
	const gchar *pkgname;
	guint i;

	/* have we recorded this before? */
	id = as_app_get_id_full (app);
	if (id == NULL) {
		g_warning ("application has no ID set");
		return;
	}
	item = g_hash_table_lookup (priv->hash_id, id);
	if (item != NULL) {

		/* the previously stored app is higher priority */
		if (as_app_get_priority (item) >
		    as_app_get_priority (app)) {
			g_debug ("ignoring duplicate AppStream entry: %s", id);
			return;
		}

		/* same priority */
		if (as_app_get_priority (item) ==
		    as_app_get_priority (app)) {
			g_debug ("merging duplicate AppStream entries: %s", id);
			as_app_subsume_full (item, app,
					     AS_APP_SUBSUME_FLAG_BOTH_WAYS);
			return;
		}

		/* this new item has a higher priority than the one we've
		 * previously stored */
		g_debug ("replacing duplicate AppStream entry: %s", id);
		g_hash_table_remove (priv->hash_id, id);
		g_ptr_array_remove (priv->array, item);
	}

	/* success, add to array */
	g_ptr_array_add (priv->array, g_object_ref (app));
	g_hash_table_insert (priv->hash_id,
			     (gpointer) as_app_get_id_full (app),
			     app);
	pkgnames = as_app_get_pkgnames (app);
	for (i = 0; i < pkgnames->len; i++) {
		pkgname = g_ptr_array_index (pkgnames, i);
		g_hash_table_insert (priv->hash_pkgname,
				     g_strdup (pkgname),
				     g_object_ref (app));
	}
}

/**
 * as_store_match_addons:
 **/
static void
as_store_match_addons (AsStore *store)
{
	AsApp *app;
	AsApp *parent;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GPtrArray *plugin_ids;
	const gchar *tmp;
	guint i;
	guint j;

	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		if (as_app_get_id_kind (app) != AS_ID_KIND_ADDON)
			continue;
		plugin_ids = as_app_get_extends (app);
		if (plugin_ids->len == 0) {
			g_warning ("%s was of type plugin but had no plugin_for",
				   as_app_get_id_full (app));
			continue;
		}
		for (j = 0; j < plugin_ids->len; j++) {
			tmp = g_ptr_array_index (plugin_ids, j);
			parent = g_hash_table_lookup (priv->hash_id, tmp);
			if (parent == NULL) {
				g_warning ("%s had no parent of '%s'",
					   as_app_get_id_full (app), tmp);
				continue;
			}
			as_app_add_addon (parent, app);
		}
	}
}

/**
 * as_store_from_root:
 **/
static gboolean
as_store_from_root (AsStore *store,
		    GNode *root,
		    const gchar *icon_root,
		    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	GNode *apps;
	GNode *n;
	const gchar *tmp;
	_cleanup_free_ gchar *icon_path = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	apps = as_node_find (root, "components");
	if (apps == NULL) {
		apps = as_node_find (root, "applications");
		if (apps == NULL) {
			g_set_error_literal (error,
					     AS_STORE_ERROR,
					     AS_STORE_ERROR_FAILED,
					     "No valid root node specified");
			return FALSE;
		}
	}

	/* get version */
	tmp = as_node_get_attribute (apps, "version");
	if (tmp != NULL)
		priv->api_version = g_ascii_strtod (tmp, NULL);

	/* set in the XML file */
	tmp = as_node_get_attribute (apps, "origin");
	if (tmp != NULL)
		as_store_set_origin (store, tmp);

	/* if we have an origin either from the XML or _set_origin() */
	if (priv->origin != NULL) {
		if (icon_root == NULL)
			icon_root = "/usr/share/app-info/icons/";
		icon_path = g_build_filename (icon_root,
					      priv->origin,
					      NULL);
	}
	for (n = apps->children; n != NULL; n = n->next) {
		_cleanup_error_free_ GError *error_local = NULL;
		_cleanup_object_unref_ AsApp *app = NULL;
		if (as_node_get_tag (n) != AS_TAG_APPLICATION)
			continue;
		app = as_app_new ();
		if (icon_path != NULL)
			as_app_set_icon_path (app, icon_path, -1);
		as_app_set_source_kind (app, AS_APP_SOURCE_KIND_APPSTREAM);
		if (!as_app_node_parse (app, n, &error_local)) {
			g_set_error (error,
				     AS_STORE_ERROR,
				     AS_STORE_ERROR_FAILED,
				     "Failed to parse root: %s",
				     error_local->message);
			return FALSE;
		}
		as_store_add_app (store, app);
	}

	/* add addon kinds to their parent AsApp */
	as_store_match_addons (store);

	return TRUE;
}

/**
 * as_store_from_file:
 * @store: a #AsStore instance.
 * @file: a #GFile.
 * @icon_root: the icon path, or %NULL for the default.
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Parses an AppStream XML file and adds any valid applications to the store.
 *
 * If the root node does not have a 'origin' attribute, then the method
 * as_store_set_origin() should be called *before* this function if cached
 * icons are required.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.0
 **/
gboolean
as_store_from_file (AsStore *store,
		    GFile *file,
		    const gchar *icon_root,
		    GCancellable *cancellable,
		    GError **error)
{
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_node_unref_ GNode *root = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	root = as_node_from_file (file,
				  AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				  cancellable,
				  &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse file: %s",
			     error_local->message);
		return FALSE;
	}
	return as_store_from_root (store, root, icon_root, error);
}

/**
 * as_store_from_xml:
 * @store: a #AsStore instance.
 * @data: XML data
 * @data_len: Length of @data, or -1 if NULL terminated
 * @icon_root: the icon path, or %NULL for the default.
 * @error: A #GError or %NULL.
 *
 * Parses AppStream XML file and adds any valid applications to the store.
 *
 * If the root node does not have a 'origin' attribute, then the method
 * as_store_set_origin() should be called *before* this function if cached
 * icons are required.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.1
 **/
gboolean
as_store_from_xml (AsStore *store,
		   const gchar *data,
		   gssize data_len,
		   const gchar *icon_root,
		   GError **error)
{
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_node_unref_ GNode *root = NULL;

	g_return_val_if_fail (AS_IS_STORE (store), FALSE);

	root = as_node_from_xml (data, data_len,
				 AS_NODE_FROM_XML_FLAG_LITERAL_TEXT,
				 &error_local);
	if (root == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse XML: %s",
			     error_local->message);
		return TRUE;
	}
	return as_store_from_root (store, root, icon_root, error);
}

/**
 * as_store_apps_sort_cb:
 **/
static gint
as_store_apps_sort_cb (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (as_app_get_id (AS_APP (*(AsApp **) a)),
			  as_app_get_id (AS_APP (*(AsApp **) b)));
}

/**
 * as_store_to_xml:
 * @store: a #AsStore instance.
 * @flags: the AsNodeToXmlFlags, e.g. %AS_NODE_INSERT_FLAG_NONE.
 *
 * Outputs an XML representation of all the applications in the store.
 *
 * Returns: A #GString
 *
 * Since: 0.1.0
 **/
GString *
as_store_to_xml (AsStore *store, AsNodeToXmlFlags flags)
{
	AsApp *app;
	AsStorePrivate *priv = GET_PRIVATE (store);
	GNode *node_apps;
	GNode *node_root;
	GString *xml;
	guint i;
	gchar version[6];

	/* get XML text */
	node_root = as_node_new ();
	if (priv->api_version >= 0.6) {
		node_apps = as_node_insert (node_root, "components", NULL, 0, NULL);
	} else {
		node_apps = as_node_insert (node_root, "applications", NULL, 0, NULL);
	}

	/* set origin attribute */
	if (priv->origin != NULL)
		as_node_add_attribute (node_apps, "origin", priv->origin, -1);

	/* set version attribute */
	if (priv->api_version > 0.1f) {
		g_ascii_formatd (version, sizeof (version),
				 "%.1f", priv->api_version);
		as_node_add_attribute (node_apps, "version", version, -1);
	}

	/* sort by ID */
	g_ptr_array_sort (priv->array, as_store_apps_sort_cb);

	/* add applications */
	for (i = 0; i < priv->array->len; i++) {
		app = g_ptr_array_index (priv->array, i);
		as_app_node_insert (app, node_apps, priv->api_version);
	}
	xml = as_node_to_xml (node_root, flags);
	as_node_unref (node_root);
	return xml;
}

/**
 * as_store_to_file:
 * @store: a #AsStore instance.
 * @file: file
 * @flags: the AsNodeToXmlFlags, e.g. %AS_NODE_INSERT_FLAG_NONE.
 * @cancellable: A #GCancellable, or %NULL
 * @error: A #GError or %NULL
 *
 * Outputs a compressed XML file of all the applications in the store.
 *
 * Returns: A #GString
 *
 * Since: 0.1.0
 **/
gboolean
as_store_to_file (AsStore *store,
		  GFile *file,
		  AsNodeToXmlFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_object_unref_ GOutputStream *out2 = NULL;
	_cleanup_object_unref_ GOutputStream *out = NULL;
	_cleanup_object_unref_ GZlibCompressor *compressor = NULL;
	_cleanup_string_free_ GString *xml = NULL;

	/* compress as a gzip file */
	compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
	out = g_memory_output_stream_new_resizable ();
	out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));
	xml = as_store_to_xml (store, flags);
	if (!g_output_stream_write_all (out2, xml->str, xml->len,
					NULL, NULL, &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to write stream: %s",
			     error_local->message);
		return FALSE;
	}
	if (!g_output_stream_close (out2, NULL, &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to close stream: %s",
			     error_local->message);
		return FALSE;
	}

	/* write file */
	if (!g_file_replace_contents (file,
		g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
		g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (out)),
				      NULL,
				      FALSE,
				      G_FILE_CREATE_NONE,
				      NULL,
				      cancellable,
				      &error_local)) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to write file: %s",
			     error_local->message);
		return FALSE;
	}
	return TRUE;
}

/**
 * as_store_get_origin:
 * @store: a #AsStore instance.
 *
 * Gets the metadata origin, which is used to locate icons.
 *
 * Returns: the origin string, or %NULL if unset
 *
 * Since: 0.1.1
 **/
const gchar *
as_store_get_origin (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->origin;
}

/**
 * as_store_set_origin:
 * @store: a #AsStore instance.
 * @origin: the origin, e.g. "fedora-21"
 *
 * Sets the metadata origin, which is used to locate icons.
 *
 * Since: 0.1.1
 **/
void
as_store_set_origin (AsStore *store, const gchar *origin)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	g_free (priv->origin);
	priv->origin = g_strdup (origin);
}

/**
 * as_store_get_api_version:
 * @store: a #AsStore instance.
 *
 * Gets the AppStream API version.
 *
 * Returns: the #AsNodeInsertFlags, or 0 if unset
 *
 * Since: 0.1.1
 **/
gdouble
as_store_get_api_version (AsStore *store)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	return priv->api_version;
}

/**
 * as_store_set_api_version:
 * @store: a #AsStore instance.
 * @api_version: the API version
 *
 * Sets the AppStream API version.
 *
 * Since: 0.1.1
 **/
void
as_store_set_api_version (AsStore *store, gdouble api_version)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	priv->api_version = api_version;
}

/**
 * as_store_guess_origin_fallback:
 */
static gboolean
as_store_guess_origin_fallback (AsStore *store,
				const gchar *filename,
				GError **error)
{
	gchar *tmp;
	_cleanup_free_ gchar *origin_fallback;

	/* the first component of the file (e.g. "fedora-20.xml.gz)
	 * is used for the icon directory as we might want to clean up
	 * the icons manually if they are installed in /var/cache */
	origin_fallback = g_path_get_basename (filename);
	tmp = g_strstr_len (origin_fallback, -1, ".xml");
	if (tmp == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "AppStream metadata name %s not valid, "
			     "expected .xml[.*]",
			     filename);
		return FALSE;
	}
	tmp[0] = '\0';

	/* load this specific file */
	as_store_set_origin (store, origin_fallback);
	return TRUE;
}

/**
 * as_store_load_app_info_file:
 */
static gboolean
as_store_load_app_info_file (AsStore *store,
			     const gchar *path_xml,
			     const gchar *icon_root,
			     GCancellable *cancellable,
			     GError **error)
{
	_cleanup_object_unref_ GFile *file = NULL;

	/* guess this based on the name */
	if (!as_store_guess_origin_fallback (store, path_xml, error))
		return FALSE;

	/* load this specific file */
	g_debug ("Loading AppStream XML %s with icon path %s",
		 path_xml, icon_root);
	file = g_file_new_for_path (path_xml);
	return as_store_from_file (store,
				   file,
				   icon_root,
				   cancellable,
				   error);
}

/**
 * as_store_cache_changed_cb:
 */
static void
as_store_cache_changed_cb (GFileMonitor *monitor,
			   GFile *file, GFile *other_file,
			   GFileMonitorEvent event_type,
			   AsStore *store)
{
	g_debug ("Emitting ::changed()");
	g_signal_emit (store, signals[SIGNAL_CHANGED], 0);
}

/**
 * as_store_monitor_directory:
 **/
static gboolean
as_store_monitor_directory (AsStore *store,
			    const gchar *path,
			    GCancellable *cancellable,
			    GError **error)
{
	AsStorePrivate *priv = GET_PRIVATE (store);
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	_cleanup_object_unref_ GFileMonitor *monitor = NULL;

	file = g_file_new_for_path (path);
	monitor = g_file_monitor_directory (file,
					    G_FILE_MONITOR_NONE,
					    cancellable,
					    &error_local);
	if (monitor == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to monitor %s: %s",
			     path, error_local->message);
		return FALSE;
	}
	g_signal_connect (monitor, "changed",
			  G_CALLBACK (as_store_cache_changed_cb),
			  store);
	g_ptr_array_add (priv->file_monitors, g_object_ref (monitor));
	return TRUE;
}

/**
 * as_store_load_app_info:
 **/
static gboolean
as_store_load_app_info (AsStore *store,
			const gchar *path,
			GCancellable *cancellable,
			GError **error)
{
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_free_ gchar *icon_root = NULL;
	_cleanup_free_ gchar *path_xml = NULL;

	/* watch the directory for changes */
	if (!as_store_monitor_directory (store, path, cancellable, error))
		return FALSE;

	/* search all files */
	path_xml = g_build_filename (path, "xmls", NULL);
	if (!g_file_test (path_xml, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path_xml, 0, &error_local);
	if (dir == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to open %s: %s",
			     path_xml, error_local->message);
		return FALSE;
	}
	icon_root = g_build_filename (path, "icons", NULL);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *filename_xml;
		filename_xml = g_build_filename (path_xml, tmp, NULL);
		if (!as_store_load_app_info_file (store,
						  filename_xml,
						  icon_root,
						  cancellable,
						  error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_store_add_app_install_screenshot:
 **/
static void
as_store_add_app_install_screenshot (AsApp *app)
{
	GPtrArray *pkgnames;
	const gchar *pkgname;
	_cleanup_free_ gchar *url = NULL;
	_cleanup_object_unref_ AsImage *im = NULL;
	_cleanup_object_unref_ AsScreenshot *ss = NULL;

	/* get the default package name */
	pkgnames = as_app_get_pkgnames (app);
	if (pkgnames->len == 0)
		return;
	pkgname = g_ptr_array_index (pkgnames, 0);
	url = g_build_filename ("http://screenshots.debian.net/screenshot",
				pkgname, NULL);

	/* screenshots.debian.net doesn't specify a size, so this is a guess */
	im = as_image_new ();
	as_image_set_url (im, url, -1);
	as_image_set_width (im, 800);
	as_image_set_height (im, 600);

	/* add screenshot without a caption */
	ss = as_screenshot_new ();
	as_screenshot_add_image (ss, im);
	as_app_add_screenshot (app, ss);
}

/**
 * as_store_load_app_install_file:
 **/
static gboolean
as_store_load_app_install_file (AsStore *store,
				const gchar *filename,
				const gchar *path_icons,
				GError **error)
{
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_object_unref_ AsApp *app;

	app = as_app_new ();
	if (!as_app_parse_file (app,
				filename,
				AS_APP_PARSE_FLAG_USE_HEURISTICS,
				&error_local)) {
		if (g_error_matches (error_local,
				     AS_APP_ERROR,
				     AS_APP_ERROR_INVALID_TYPE)) {
			/* Ubuntu include non-apps here too... */
			return TRUE;
		}
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to parse %s: %s",
			     filename,
			     error_local->message);
		return FALSE;
	}
	if (as_app_get_icon_kind (app) == AS_ICON_KIND_UNKNOWN)
		as_app_set_icon_kind (app, AS_ICON_KIND_CACHED);
	as_app_set_icon_path (app, path_icons, -1);
	as_store_add_app_install_screenshot (app);
	as_store_add_app (store, app);
	return TRUE;
}

/**
 * as_store_load_app_install:
 **/
static gboolean
as_store_load_app_install (AsStore *store,
			   const gchar *path,
			   GCancellable *cancellable,
			   GError **error)
{
	const gchar *tmp;
	_cleanup_dir_close_ GDir *dir = NULL;
	_cleanup_error_free_ GError *error_local = NULL;
	_cleanup_free_ gchar *path_desktop = NULL;
	_cleanup_free_ gchar *path_icons = NULL;

	path_desktop = g_build_filename (path, "desktop", NULL);
	if (!g_file_test (path_desktop, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path_desktop, 0, &error_local);
	if (dir == NULL) {
		g_set_error (error,
			     AS_STORE_ERROR,
			     AS_STORE_ERROR_FAILED,
			     "Failed to open %s: %s",
			     path_desktop,
			     error_local->message);
		return FALSE;
	}

	path_icons = g_build_filename (path, "icons", NULL);
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		_cleanup_free_ gchar *filename = NULL;
		if (!g_str_has_suffix (tmp, ".desktop"))
			continue;
		filename = g_build_filename (path_desktop, tmp, NULL);
		if (!as_store_load_app_install_file (store,
						     filename,
						     path_icons,
						     error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_store_load:
 * @store: a #AsStore instance.
 * @flags: #AsStoreLoadFlags, e.g. %AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM
 * @cancellable: a #GCancellable.
 * @error: A #GError or %NULL.
 *
 * Loads the store from the default locations.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.2
 **/
gboolean
as_store_load (AsStore *store,
	       AsStoreLoadFlags flags,
	       GCancellable *cancellable,
	       GError **error)
{
	const gchar * const * data_dirs;
	const gchar *tmp;
	gchar *path;
	guint i;
	_cleanup_ptrarray_unref_ GPtrArray *app_info = NULL;

	/* system locations */
	app_info = g_ptr_array_new_with_free_func (g_free);
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_SYSTEM) > 0) {
		data_dirs = g_get_system_data_dirs ();
		for (i = 0; data_dirs[i] != NULL; i++) {
			path = g_build_filename (data_dirs[i], "app-info", NULL);
			g_ptr_array_add (app_info, path);
		}
		path = g_build_filename (LOCALSTATEDIR, "lib", "app-info", NULL);
		g_ptr_array_add (app_info, path);
		path = g_build_filename (LOCALSTATEDIR, "cache", "app-info", NULL);
		g_ptr_array_add (app_info, path);
	}

	/* per-user locations */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INFO_USER) > 0) {
		path = g_build_filename (g_get_user_data_dir (), "app-info", NULL);
		g_ptr_array_add (app_info, path);
	}

	/* load each app-info path if it exists */
	for (i = 0; i < app_info->len; i++) {
		tmp = g_ptr_array_index (app_info, i);
		if (!g_file_test (tmp, G_FILE_TEST_EXISTS))
			continue;
		if (!as_store_load_app_info (store, tmp, cancellable, error))
			return FALSE;
	}

	/* ubuntu specific */
	if ((flags & AS_STORE_LOAD_FLAG_APP_INSTALL) > 0) {
		if (!as_store_load_app_install (store,
						"/usr/share/app-install",
						cancellable,
						error))
			return FALSE;
	}
	return TRUE;
}

/**
 * as_store_new:
 *
 * Creates a new #AsStore.
 *
 * Returns: (transfer full): a #AsStore
 *
 * Since: 0.1.0
 **/
AsStore *
as_store_new (void)
{
	AsStore *store;
	store = g_object_new (AS_TYPE_STORE, NULL);
	return AS_STORE (store);
}
