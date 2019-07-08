/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <glib/gi18n.h>
#include <libsoup/soup.h>
#include <snapd-glib/snapd-glib.h>

#include "store-model.h"
#include "store-odrs-client.h"

struct _StoreModel
{
    GObject parent_instance;

    StoreCache *cache;
    GPtrArray *categories;
    GPtrArray *installed;
    StoreOdrsClient *odrs_client;
    SoupSession *session;
    gchar *snapd_socket_path;
    GHashTable *snaps;
};

enum
{
    PROP_0,
    PROP_CATEGORIES,
    PROP_INSTALLED,
    PROP_LAST
};

G_DEFINE_TYPE (StoreModel, store_model, G_TYPE_OBJECT)

typedef struct
{
    StoreModel *self;
    gchar *section_name;
} FindSectionData;

static FindSectionData *
find_section_data_new (StoreModel *self, const gchar *section_name)
{
    FindSectionData *data = g_new0 (FindSectionData, 1);
    data->self = self;
    data->section_name = g_strdup (section_name);
    return data;
}

static void
find_section_data_free (FindSectionData *data)
{
    g_free (data->section_name);
    g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FindSectionData, find_section_data_free)

typedef struct
{
    StoreModel *self;
    gchar *uri;
    SoupMessage *message;
    gint orig_width;
    gint orig_height;
    gint width;
    gint height;
    GByteArray *buffer;
} GetImageData;

static GetImageData *
get_image_data_new (StoreModel *self, const gchar *uri, gint width, gint height)
{
    GetImageData *data = g_new0 (GetImageData, 1);
    data->self = self;
    data->uri = g_strdup (uri);
    data->width = width;
    data->height = height;
    data->buffer = g_byte_array_new ();
    return data;
}

static void
get_image_data_free (GetImageData *data)
{
    g_clear_pointer (&data->buffer, g_byte_array_unref);
    g_clear_pointer (&data->uri, g_free);
    g_clear_object (&data->message);
    g_clear_pointer (&data, g_free);
}

static void
set_review_counts (StoreModel *self, StoreApp *app)
{
    if (self->odrs_client == NULL)
        return;

    gint64 *ratings = NULL;
    if (store_app_get_appstream_id (app) != NULL)
        ratings = store_odrs_client_get_ratings (self->odrs_client, store_app_get_appstream_id (app));

    store_app_set_review_count_one_star (app, ratings != NULL ? ratings[0] : 0);
    store_app_set_review_count_two_star (app, ratings != NULL ? ratings[1] : 0);
    store_app_set_review_count_three_star (app, ratings != NULL ? ratings[2] : 0);
    store_app_set_review_count_four_star (app, ratings != NULL ? ratings[3] : 0);
    store_app_set_review_count_five_star (app, ratings != NULL ? ratings[4] : 0);
}

static GPtrArray *
load_cached_reviews (StoreModel *self, const gchar *name)
{
    if (self->cache == NULL)
        return NULL;

    g_autoptr(JsonNode) reviews_cache = store_cache_lookup_json (self->cache, "reviews", name, FALSE, NULL, NULL);
    if (reviews_cache == NULL)
        return NULL;

    g_autoptr(GPtrArray) reviews = g_ptr_array_new_with_free_func (g_object_unref);
    JsonArray *array = json_node_get_array (reviews_cache);
    for (guint i = 0; i < json_array_get_length (array); i++) {
        JsonNode *node = json_array_get_element (array, i);
        g_ptr_array_add (reviews, store_odrs_review_new_from_json (node));
    }

    return g_steal_pointer (&reviews);
}

static const gchar *
get_section_title (const gchar *name)
{
    if (strcmp (name, "development") == 0)
        /* Title for Development snap category */
        return _("Development");
    if (strcmp (name, "games") == 0)
        /* Title for Games snap category */
        return _("Games");
    if (strcmp (name, "social") == 0)
        /* Title for Social snap category */
        return _("Social");
    if (strcmp (name, "productivity") == 0)
        /* Title for Productivity snap category */
        return _("Productivity");
    if (strcmp (name, "utilities") == 0)
        /* Title for Utilities snap category */
        return _("Utilities");
    if (strcmp (name, "photo-and-video") == 0)
        /* Title for Photo and Video snap category */
        return _("Photo and Video");
    if (strcmp (name, "server-and-cloud") == 0)
        /* Title for Server and Cloud snap category */
        return _("Server and Cloud");
    if (strcmp (name, "security") == 0)
        /* Title for Security snap category */
        return _("Security");
    if (strcmp (name, "") == 0)
        /* Title for Security snap category */
        return _("Security");
    if (strcmp (name, "featured") == 0)
        /* Title for Featured snap category */
        return _("Editors picks"); /*_("Featured");*/
    if (strcmp (name, "devices-and-iot") == 0)
        /* Title for Devices and IoT snap category */
        return _("Devices and IoT");
    if (strcmp (name, "music-and-audio") == 0)
        /* Title for Music and Audio snap category */
        return _("Music and Audio");
    if (strcmp (name, "entertainment") == 0)
        /* Title for Entertainment snap category */
        return _("Entertainment");
    if (strcmp (name, "art-and-design") == 0)
        /* Title for Art and Design snap category */
        return _("Art and Design");
    if (strcmp (name, "finance") == 0)
        /* Title for Finance snap category */
        return _("Finance");
    if (strcmp (name, "news-and-weather") == 0)
        /* Title for News and Weather snap category */
        return _("News and Weather");
    if (strcmp (name, "science") == 0)
        /* Title for Science snap category */
        return _("Science");
    if (strcmp (name, "health-and-fitness") == 0)
        /* Title for Health and Fitness snap category */
        return _("Health and Fitness");
    if (strcmp (name, "education") == 0)
        /* Title for Education snap category */
        return _("Education");
    if (strcmp (name, "books-and-reference") == 0)
        /* Title for Books and Reference snap category */
        return _("Books and Reference");
    if (strcmp (name, "personalisation") == 0)
        /* Title for Personalisation snap category */
        return _("Personalisation");
    return name;
}

static const gchar *
get_section_summary (const gchar *name)
{
    if (strcmp (name, "featured") == 0)
        /* Summary for Featured snap category */
        return _("Here are this months hand-picked applications from our content team, we hope you like them!");

    return NULL;
}

static GPtrArray *
load_cached_category_apps (StoreModel *self, const gchar *section)
{
    g_autoptr(GPtrArray) apps = g_ptr_array_new_with_free_func (g_object_unref);

    if (self->cache == NULL)
        return g_steal_pointer (&apps);

    g_autoptr(JsonNode) sections_cache = store_cache_lookup_json (self->cache, "sections", section, FALSE, NULL, NULL);
    if (sections_cache == NULL)
        return g_steal_pointer (&apps);

    JsonArray *array = json_node_get_array (sections_cache);
    for (guint j = 0; j < json_array_get_length (array); j++) {
        JsonNode *node = json_array_get_element (array, j);

        const gchar *name = json_node_get_string (node);
        g_ptr_array_add (apps, store_model_get_snap (self, name));
    }

    return g_steal_pointer (&apps);
}

static GPtrArray *
load_cached_categories (StoreModel *self)
{
    g_autoptr(GPtrArray) categories = g_ptr_array_new_with_free_func (g_object_unref);

    if (self->cache == NULL)
        return g_steal_pointer (&categories);

    g_autoptr(JsonNode) sections_cache = store_cache_lookup_json (self->cache, "sections", "_index", FALSE, NULL, NULL);
    if (sections_cache == NULL)
        return g_steal_pointer (&categories);

    JsonArray *array = json_node_get_array (sections_cache);
    g_autoptr(GPtrArray) section_array = g_ptr_array_new ();
    for (guint i = 0; i < json_array_get_length (array); i++) {
        JsonNode *node = json_array_get_element (array, i);
        const gchar *section = json_node_get_string (node);

        StoreCategory *category = store_category_new ();
        g_ptr_array_add (categories, category);
        store_category_set_name (category, section);
        store_category_set_title (category, get_section_title (section));
        store_category_set_summary (category, get_section_summary (section));

        g_autoptr(GPtrArray) apps = load_cached_category_apps (self, section);
        store_category_set_apps (category, apps);
    }

    return g_steal_pointer (&categories);
}

StoreCategory *
find_category (StoreModel *self, const gchar *section_name)
{
    for (guint i = 0; i < self->categories->len; i++) {
        StoreCategory *category = g_ptr_array_index (self->categories, i);
        if (g_strcmp0 (store_category_get_name (category), section_name) == 0)
            return category;
    }

    return NULL;
}

static void
get_category_snaps_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(FindSectionData) data = user_data;
    StoreModel *self = data->self;

    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) snaps = snapd_client_find_section_finish (SNAPD_CLIENT (object), result, NULL, &error);
    if (snaps == NULL) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;
        g_warning ("Failed to find snaps in category: %s", error->message);
        return;
    }

    g_autoptr(GPtrArray) apps = g_ptr_array_new_with_free_func (g_object_unref);
    for (guint i = 0; i < snaps->len; i++) {
        SnapdSnap *snap = g_ptr_array_index (snaps, i);
        g_autoptr(StoreSnapApp) app = store_model_get_snap (self, snapd_snap_get_name (snap));
        store_snap_app_update_from_search (app, snap);
        if (self->cache != NULL)
            store_app_save_to_cache (STORE_APP (app), self->cache);
        g_ptr_array_add (apps, g_steal_pointer (&app));
    }

    StoreCategory *category = find_category (self, data->section_name);
    if (category != NULL)
        store_category_set_apps (category, apps);

    /* Save in cache */
    if (self->cache != NULL) {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        json_builder_begin_array (builder);
        for (guint i = 0; i < snaps->len; i++) {
            SnapdSnap *snap = g_ptr_array_index (snaps, i);
            json_builder_add_string_value (builder, snapd_snap_get_name (snap));
        }
        json_builder_end_array (builder);
        g_autoptr(JsonNode) root = json_builder_get_root (builder);
        store_cache_insert_json (self->cache, "sections", data->section_name, FALSE, root, NULL, NULL);
    }
}

static void
get_sections_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_auto(GStrv) sections = snapd_client_get_sections_finish (SNAPD_CLIENT (object), result, &error);
    if (sections == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    StoreModel *self = g_task_get_source_object (task);

    g_clear_pointer (&self->categories, g_ptr_array_unref);
    self->categories = g_ptr_array_new_with_free_func (g_object_unref);
    for (int i = 0; sections[i] != NULL; i++) {
        StoreCategory *category = store_category_new ();
        g_ptr_array_add (self->categories, category);
        store_category_set_name (category, sections[i]);
        store_category_set_title (category, get_section_title (sections[i]));
        store_category_set_summary (category, get_section_summary (sections[i]));

        g_autoptr(GPtrArray) apps = load_cached_category_apps (self, sections[i]);
        store_category_set_apps (category, apps);

        g_autoptr(SnapdClient) client = snapd_client_new ();
        snapd_client_set_socket_path (client, self->snapd_socket_path);
        snapd_client_find_section_async (client, SNAPD_FIND_FLAGS_SCOPE_WIDE, sections[i], NULL, g_task_get_cancellable (task), get_category_snaps_cb, find_section_data_new (self, sections[i]));
    }

    /* Save in cache */
    if (self->cache != NULL) {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        json_builder_begin_array (builder);
        for (int i = 0; sections[i] != NULL; i++)
            json_builder_add_string_value (builder, sections[i]);
        json_builder_end_array (builder);
        g_autoptr(JsonNode) root = json_builder_get_root (builder);
        store_cache_insert_json (self->cache, "sections", "_index", FALSE, root, NULL, NULL);
    }

    g_object_notify (G_OBJECT (self), "categories");

    g_task_return_boolean (task, TRUE);
}

static void
get_snaps_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) snaps = snapd_client_get_snaps_finish (SNAPD_CLIENT (object), result, &error);
    if (snaps == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    StoreModel *self = g_task_get_source_object (task);

    g_clear_pointer (&self->installed, g_ptr_array_unref);
    self->installed = g_ptr_array_new_with_free_func (g_object_unref);
    for (guint i = 0; i < snaps->len; i++) {
        SnapdSnap *snap = g_ptr_array_index (snaps, i);
        g_autoptr(StoreSnapApp) app = store_model_get_snap (self, snapd_snap_get_name (snap));
        store_app_set_installed (STORE_APP (app), TRUE);
        store_snap_app_update_from_search (app, snap);
        if (self->cache != NULL)
            store_app_save_to_cache (STORE_APP (app), self->cache);
        g_ptr_array_add (self->installed, g_steal_pointer (&app));
    }

    g_object_notify (G_OBJECT (self), "installed");

    g_task_return_boolean (task, TRUE);
}

static void
ratings_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    if (!store_odrs_client_update_ratings_finish (STORE_ODRS_CLIENT (object), result, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    StoreModel *self = g_task_get_source_object (task);

    /* Update existing apps */
    GHashTableIter iter;
    g_hash_table_iter_init (&iter, self->snaps);
    gpointer key, value;
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        StoreSnapApp *snap = value;
        gint64 *ratings = store_odrs_client_get_ratings (self->odrs_client, store_app_get_appstream_id (STORE_APP (snap)));
        store_app_set_review_count_one_star (STORE_APP (snap), ratings != NULL ? ratings[0] : 0);
        store_app_set_review_count_two_star (STORE_APP (snap), ratings != NULL ? ratings[1] : 0);
        store_app_set_review_count_three_star (STORE_APP (snap), ratings != NULL ? ratings[2] : 0);
        store_app_set_review_count_four_star (STORE_APP (snap), ratings != NULL ? ratings[3] : 0);
        store_app_set_review_count_five_star (STORE_APP (snap), ratings != NULL ? ratings[4] : 0);
    }

    g_task_return_boolean (task, TRUE);
}

static void
reviews_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autofree gchar *user_skey = NULL;
    g_autoptr(GPtrArray) reviews = store_odrs_client_get_reviews_finish (STORE_ODRS_CLIENT (object), result, &user_skey, &error);
    if (reviews == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }
    // FIXME: Store and use review key

    StoreModel *self = g_task_get_source_object (task);
    StoreApp *app = g_task_get_task_data (task);

    store_app_set_reviews (app, reviews);

    /* Save in cache */
    if (self->cache != NULL) {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        json_builder_begin_array (builder);
        for (guint i = 0; i < reviews->len; i++) {
            StoreOdrsReview *review = g_ptr_array_index (reviews, i);
            json_builder_add_value (builder, store_odrs_review_to_json (review));
        }
        json_builder_end_array (builder);
        g_autoptr(JsonNode) root = json_builder_get_root (builder);
        store_cache_insert_json (self->cache, "reviews", store_app_get_name (app), FALSE, root, NULL, NULL);
    }

    g_task_return_boolean (task, TRUE);
}

static void
image_size_cb (GetImageData *data, gint width, gint height, GdkPixbufLoader *loader)
{
    data->orig_width = width;
    data->orig_height = height;

    if (data->width == 0 || data->height == 0)
        return;

    gint w, h;
    if (width * data->height > height * data->width) {
        w = data->width;
        h = height * data->width / width;
    }
    else {
        h = data->height;
        w = width * data->height / height;
    }

    gdk_pixbuf_loader_set_size (loader, w, h);
}

static GdkPixbuf *
process_image (GetImageData *image_data, GBytes *data, GError **error)
{
    g_autoptr(GdkPixbufLoader) loader = gdk_pixbuf_loader_new ();

    g_signal_connect_swapped (loader, "size-prepared", G_CALLBACK (image_size_cb), image_data);

    if (!gdk_pixbuf_loader_write_bytes (loader, data, error) ||
        !gdk_pixbuf_loader_close (loader, error))
        return NULL;

    return g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));
}

static void
cached_image_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GBytes) data = store_cache_lookup_finish (STORE_CACHE (object), result, &error);
    if (data == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    GetImageData *image_data = g_task_get_task_data (task);

    g_autoptr(GdkPixbuf) pixbuf = process_image (image_data, data, &error);
    if (pixbuf == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_task_return_pointer (task, g_steal_pointer (&pixbuf), g_object_unref);
}

static void
read_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GBytes) data = g_input_stream_read_bytes_finish (G_INPUT_STREAM (object), result, &error);
    if (data == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    StoreModel *self = g_task_get_source_object (task);
    GetImageData *image_data = g_task_get_task_data (task);

    g_byte_array_append (image_data->buffer, g_bytes_get_data (data, NULL), g_bytes_get_size (data));

    /* Read until EOF */
    if (g_bytes_get_size (data) != 0) {
        GCancellable *cancellable = g_task_get_cancellable (task);
        g_input_stream_read_bytes_async (G_INPUT_STREAM (object), 65535, G_PRIORITY_DEFAULT, cancellable, read_cb, g_steal_pointer (&task));
        return;
    }

    g_autoptr(GBytes) full_data = g_bytes_new_static (image_data->buffer->data, image_data->buffer->len);
    g_autoptr(GdkPixbuf) pixbuf = process_image (image_data, full_data, &error);
    if (pixbuf == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    /* Save in cache */
    if (self->cache != NULL) {
        g_autoptr(JsonBuilder) builder = json_builder_new ();
        json_builder_begin_object (builder);
        json_builder_set_member_name (builder, "uri");
        json_builder_add_string_value (builder, image_data->uri);
        json_builder_set_member_name (builder, "width");
        json_builder_add_int_value (builder, image_data->orig_width);
        json_builder_set_member_name (builder, "height");
        json_builder_add_int_value (builder, image_data->orig_height);
        const gchar *etag = soup_message_headers_get_one (image_data->message->response_headers, "ETag");
        if (etag != NULL) {
            json_builder_set_member_name (builder, "etag");
            json_builder_add_string_value (builder, etag);
        }
        const gchar *cache_control = soup_message_headers_get_one (image_data->message->response_headers, "Cache-Control");
        if (cache_control != NULL) {
            g_autoptr(GHashTable) params = soup_header_parse_param_list (cache_control);
            const gchar *max_age = g_hash_table_lookup (params, "max-age");
            if (max_age != NULL) {
                json_builder_set_member_name (builder, "max-age");
                json_builder_add_int_value (builder, g_ascii_strtoull (max_age, NULL, 10));
            }
        }
        json_builder_end_object (builder);
        g_autoptr(JsonNode) root = json_builder_get_root (builder);
        store_cache_insert_json (self->cache, "image-metadata", image_data->uri, TRUE, root, g_task_get_cancellable (task), NULL);
        store_cache_insert (self->cache, "images", image_data->uri, TRUE, full_data, g_task_get_cancellable (task), NULL);
    }

    g_task_return_pointer (task, g_steal_pointer (&pixbuf), g_object_unref);
}

static void
send_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GInputStream) stream = soup_session_send_finish (SOUP_SESSION (object), result, &error);
    if (stream == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    GetImageData *image_data = g_task_get_task_data (task);
    SoupMessage *msg = image_data->message;

    if (msg->status_code != SOUP_STATUS_OK) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Server returned status code %d", msg->status_code); // FIXME: Report 304 errors better
        return;
    }

    GCancellable *cancellable = g_task_get_cancellable (task);
    g_input_stream_read_bytes_async (stream, 65535, G_PRIORITY_DEFAULT, cancellable, read_cb, g_steal_pointer (&task));
}

static void
search_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) snaps = snapd_client_find_finish (SNAPD_CLIENT (object), result, NULL, &error);
    if (snaps == NULL) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    StoreModel *self = g_task_get_source_object (task);

    g_autoptr(GPtrArray) apps = g_ptr_array_new_with_free_func (g_object_unref);
    for (guint i = 0; i < snaps->len; i++) {
        SnapdSnap *snap = g_ptr_array_index (snaps, i);
        g_autoptr(StoreSnapApp) app = store_model_get_snap (self, snapd_snap_get_name (snap));
        store_snap_app_update_from_search (app, snap);
        if (self->cache != NULL)
            store_app_save_to_cache (STORE_APP (app), self->cache);
        g_ptr_array_add (apps, g_steal_pointer (&app));
    }

    g_task_return_pointer (task, g_steal_pointer (&apps), (GDestroyNotify) g_ptr_array_unref);
}

static void
install_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    GTask *task = user_data;

    g_autoptr(GError) error = NULL;
    gboolean r = snapd_client_install2_finish (SNAPD_CLIENT (object), result, &error);
    if (!r && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    StoreModel *self = g_task_get_source_object (task);
    StoreApp *app = g_task_get_task_data (task);

    store_app_set_progress (app, NULL);

    if (!r) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to install snap: %s", error->message);
        return;
    }

    store_app_set_installed (app, TRUE);
    g_ptr_array_add (self->installed, g_object_ref (app));
    g_object_notify (G_OBJECT (self), "installed");

    g_task_return_boolean (task, TRUE);
}

static void
remove_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    GTask *task = user_data;

    g_autoptr(GError) error = NULL;
    gboolean r = snapd_client_remove_finish (SNAPD_CLIENT (object), result, &error);
    if (!r && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    StoreModel *self = g_task_get_source_object (task);
    StoreApp *app = g_task_get_task_data (task);

    store_app_set_progress (app, NULL);

    if (!r) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to remove snap: %s", error->message);
        return;
    }

    store_app_set_installed (app, FALSE);
    g_ptr_array_remove (self->installed, app);
    g_object_notify (G_OBJECT (self), "installed");

    g_task_return_boolean (task, TRUE);
}

static void
refresh_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    g_autoptr(GTask) task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) snaps = snapd_client_find_finish (SNAPD_CLIENT (object), result, NULL, &error);
    if (snaps == NULL) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to get snap information: %s", error->message);
        return;
    }

    if (snaps->len != 1) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Snap find returned %d results, expected 1", snaps->len);
        return;
    }

    StoreSnapApp *app = g_task_get_task_data (task);
    SnapdSnap *snap = g_ptr_array_index (snaps, 0);

    store_snap_app_update_from_search (app, snap);

    g_task_return_boolean (task, TRUE);
}

static void
store_model_dispose (GObject *object)
{
    StoreModel *self = STORE_MODEL (object);

    g_clear_object (&self->cache);
    g_clear_pointer (&self->categories, g_ptr_array_unref);
    g_clear_pointer (&self->installed, g_ptr_array_unref);
    g_clear_object (&self->odrs_client);
    g_clear_object (&self->session);
    g_clear_pointer (&self->snapd_socket_path, g_free);
    g_clear_pointer (&self->snaps, g_hash_table_unref);

    G_OBJECT_CLASS (store_model_parent_class)->dispose (object);
}

static void
store_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    StoreModel *self = STORE_MODEL (object);

    switch (prop_id)
    {
    case PROP_CATEGORIES:
        g_value_set_boxed (value, self->categories);
        break;
    case PROP_INSTALLED:
        g_value_set_boxed (value, self->installed);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
store_model_set_property (GObject *object, guint prop_id, const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
store_model_class_init (StoreModelClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = store_model_dispose;
    G_OBJECT_CLASS (klass)->get_property = store_model_get_property;
    G_OBJECT_CLASS (klass)->set_property = store_model_set_property;

    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_CATEGORIES,
                                     g_param_spec_boxed ("categories", NULL, NULL, G_TYPE_PTR_ARRAY, G_PARAM_READABLE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_INSTALLED,
                                     g_param_spec_boxed ("installed", NULL, NULL, G_TYPE_PTR_ARRAY, G_PARAM_READABLE));
}

static void
store_model_init (StoreModel *self)
{
    self->cache = store_cache_new ();
    self->categories = g_ptr_array_new_with_free_func (g_object_unref);;
    self->installed = g_ptr_array_new_with_free_func (g_object_unref);;
    self->odrs_client = store_odrs_client_new ();
    self->session = soup_session_new ();
    self->snaps = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

StoreModel *
store_model_new (void)
{
    return g_object_new (store_model_get_type (), NULL);
}

void
store_model_set_cache (StoreModel *self, StoreCache *cache)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_set_object (&self->cache, cache);
}

StoreCache *
store_model_get_cache (StoreModel *self)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);

    return self->cache;
}

void
store_model_set_odrs_server_uri (StoreModel *self, const gchar *uri)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    store_odrs_client_set_server_uri (self->odrs_client, uri);
}

const gchar *
store_model_get_odrs_server (StoreModel *self)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);

    return store_odrs_client_get_server_uri (self->odrs_client);
}

void
store_model_set_snapd_socket_path (StoreModel *self, const gchar *path)
{
    g_return_if_fail (STORE_IS_MODEL (self));
    g_free (self->snapd_socket_path);
    self->snapd_socket_path = g_strdup (path);
    // FIXME: Update existing StoreSnapApp objects
}

void
store_model_load (StoreModel *self)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    self->categories = load_cached_categories (self);
    g_object_notify (G_OBJECT (self), "categories");
}

StoreSnapApp *
store_model_get_snap (StoreModel *self, const gchar *name)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);

    StoreSnapApp *snap = g_hash_table_lookup (self->snaps, name);
    if (snap == NULL) {
        snap = store_snap_app_new ();
        store_app_set_name (STORE_APP (snap), name);
        g_hash_table_insert (self->snaps, g_strdup (name), snap); // FIXME: Use a weak ref to clean out when no-longer used
    }

    if (self->cache != NULL)
        store_app_update_from_cache (STORE_APP (snap), self->cache);
    set_review_counts (self, STORE_APP (snap));
    g_autoptr(GPtrArray) reviews = load_cached_reviews (self, name);
    if (reviews != NULL)
        store_app_set_reviews (STORE_APP (snap), reviews);

    return g_object_ref (snap);
}

GPtrArray *
store_model_get_categories (StoreModel *self)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);
    return self->categories;
}

void
store_model_update_categories_async (StoreModel *self,
                                     GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);
    g_autoptr(SnapdClient) client = snapd_client_new ();
    snapd_client_set_socket_path (client, self->snapd_socket_path);
    snapd_client_get_sections_async (client, cancellable, get_sections_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

gboolean
store_model_update_categories_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

GPtrArray *
store_model_get_installed (StoreModel *self)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);
    return self->installed;
}

void
store_model_update_installed_async (StoreModel *self,
                                    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);
    g_autoptr(SnapdClient) client = snapd_client_new ();
    snapd_client_set_socket_path (client, self->snapd_socket_path);
    snapd_client_get_snaps_async (client, SNAPD_GET_SNAPS_FLAGS_NONE, NULL, cancellable, get_snaps_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

gboolean
store_model_update_installed_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
store_model_update_ratings_async (StoreModel *self,
                                  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);
    store_odrs_client_update_ratings_async (self->odrs_client, cancellable, ratings_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

gboolean
store_model_update_ratings_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
store_model_update_reviews_async (StoreModel *self, StoreApp *app,
                                  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);

    if (self->odrs_client == NULL) {
        g_task_return_boolean (task, TRUE);
        return;
    }

    g_task_set_task_data (task, g_object_ref (app), g_object_unref);
    store_odrs_client_get_reviews_async (self->odrs_client, store_app_get_appstream_id (app), NULL, NULL, 40, cancellable, reviews_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

gboolean
store_model_update_reviews_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
store_model_search_async (StoreModel *self, const gchar *query,
                          GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);
    g_autoptr(SnapdClient) client = snapd_client_new ();
    snapd_client_set_socket_path (client, self->snapd_socket_path);
    snapd_client_find_async (client, SNAPD_FIND_FLAGS_SCOPE_WIDE, query, cancellable, search_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

GPtrArray *
store_model_search_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

gboolean
store_model_get_cached_image_metadata_sync (StoreModel *self, const gchar *uri, gchar **etag, gint64 *width, gint64 *height, GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);

    g_autoptr(JsonNode) node = store_cache_lookup_json (self->cache, "image-metadata", uri, TRUE, cancellable, error);
    if (node == NULL)
        return FALSE;

    JsonObject *o = json_node_get_object (node);
    if (etag != NULL && json_object_has_member (o, "etag"))
        *etag = g_strdup (json_object_get_string_member (o, "etag"));
    if (width != NULL && json_object_has_member (o, "width"))
        *width = json_object_get_int_member (o, "width");
    if (height != NULL && json_object_has_member (o, "height"))
        *height = json_object_get_int_member (o, "height");

    return TRUE;
}

void
store_model_get_cached_image_async (StoreModel *self, const gchar *uri, gint width, gint height,
                                    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);
    if (self->cache == NULL) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "No cache");
        return;
    }

    g_task_set_task_data (task, get_image_data_new (self, uri, width, height), (GDestroyNotify) get_image_data_free);
    store_cache_lookup_async (self->cache, "images", uri, TRUE, cancellable, cached_image_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

GdkPixbuf *
store_model_get_cached_image_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
store_model_get_image_async (StoreModel *self, const gchar *uri, const gchar *etag, gint width, gint height,
                             GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_autoptr(GTask) task = g_task_new (self, cancellable, callback, callback_data);
    GetImageData *image_data = get_image_data_new (self, uri, width, height);
    g_task_set_task_data (task, image_data, (GDestroyNotify) get_image_data_free);
    image_data->message = soup_message_new ("GET", uri);
    if (etag != NULL)
        soup_message_headers_append (image_data->message->request_headers, "If-None-Match", etag);
    soup_session_send_async (self->session, image_data->message, cancellable, send_cb, g_steal_pointer (&task)); // FIXME: Combine cancellables
}

GdkPixbuf *
store_model_get_image_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), NULL);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

void
store_model_install_async (StoreModel *self, StoreApp *app, StoreChannel *channel, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_assert (STORE_IS_SNAP_APP (app)); // FIXME

    g_autoptr(StoreProgress) progress = store_progress_new ();
    store_app_set_progress (app, progress);

    g_autoptr(SnapdClient) client = snapd_client_new ();
    snapd_client_set_socket_path (client, self->snapd_socket_path);
    GTask *task = g_task_new (self, cancellable, callback, callback_data); // FIXME: Need to combine cancellables?
    g_task_set_task_data (task, g_object_ref (app), g_object_unref);
    const gchar *channel_name = NULL;
    if (channel != NULL)
        store_channel_get_name (channel);
    snapd_client_install2_async (client, SNAPD_INSTALL_FLAGS_NONE, store_app_get_name (app), channel_name, NULL, NULL, NULL, cancellable, install_cb, task);
}

gboolean
store_model_install_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    return g_task_propagate_boolean (G_TASK (result), error);
}

void
store_model_remove_async (StoreModel *self, StoreApp *app, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_assert (STORE_IS_SNAP_APP (app)); // FIXME

    g_autoptr(StoreProgress) progress = store_progress_new ();
    store_app_set_progress (app, progress);

    g_autoptr(SnapdClient) client = snapd_client_new ();
    snapd_client_set_socket_path (client, self->snapd_socket_path);
    GTask *task = g_task_new (self, cancellable, callback, callback_data); // FIXME: Need to combine cancellables?
    g_task_set_task_data (task, g_object_ref (app), g_object_unref);
    snapd_client_remove_async (client, store_app_get_name (app), NULL, NULL, cancellable, remove_cb, task);
}

gboolean
store_model_remove_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    return g_task_propagate_boolean (G_TASK (result), error);
}

void
store_model_refresh_async (StoreModel *self, StoreApp *app, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_MODEL (self));

    g_assert (STORE_IS_SNAP_APP (app)); // FIXME

    g_autoptr(SnapdClient) client = snapd_client_new ();
    snapd_client_set_socket_path (client, self->snapd_socket_path);
    GTask *task = g_task_new (self, cancellable, callback, callback_data); // FIXME: Need to combine cancellables?
    g_task_set_task_data (task, g_object_ref (app), g_object_unref);
    snapd_client_find_async (client, SNAPD_FIND_FLAGS_MATCH_NAME, store_app_get_name (app), cancellable, refresh_cb, task);
}

gboolean
store_model_refresh_finish (StoreModel *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_MODEL (self), FALSE);
    return g_task_propagate_boolean (G_TASK (result), error);
}
