/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "store-odrs-review.h"

struct _StoreOdrsReview
{
    GObject parent_instance;

    gchar *author;
    GDateTime *date_created;
    gchar *description;
    gint64 id;
    gint64 rating;
    gchar *summary;
    gboolean voted;
};

enum
{
    PROP_0,
    PROP_AUTHOR,
    PROP_DATE_CREATED,
    PROP_DESCRIPTION,
    PROP_ID,
    PROP_RATING,
    PROP_SUMMARY,
    PROP_VOTED,
    PROP_LAST
};

G_DEFINE_TYPE (StoreOdrsReview, store_odrs_review, G_TYPE_OBJECT)

static void
store_odrs_review_dispose (GObject *object)
{
    StoreOdrsReview *self = STORE_ODRS_REVIEW (object);

    g_clear_pointer (&self->author, g_free);
    g_clear_pointer (&self->date_created, g_date_time_unref);
    g_clear_pointer (&self->description, g_free);
    g_clear_pointer (&self->summary, g_free);

    G_OBJECT_CLASS (store_odrs_review_parent_class)->dispose (object);
}

static void
store_odrs_review_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    StoreOdrsReview *self = STORE_ODRS_REVIEW (object);

    switch (prop_id)
    {
    case PROP_AUTHOR:
        g_value_set_string (value, self->author);
        break;
    case PROP_DATE_CREATED:
        g_value_set_boxed (value, self->date_created);
        break;
    case PROP_DESCRIPTION:
        g_value_set_string (value, self->description);
        break;
    case PROP_ID:
        g_value_set_int64 (value, self->id);
        break;
    case PROP_RATING:
        g_value_set_int64 (value, self->rating);
        break;
    case PROP_SUMMARY:
        g_value_set_string (value, self->summary);
        break;
    case PROP_VOTED:
        g_value_set_boolean (value, self->voted);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
store_odrs_review_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    StoreOdrsReview *self = STORE_ODRS_REVIEW (object);

    switch (prop_id)
    {
    case PROP_AUTHOR:
        store_odrs_review_set_author (self, g_value_get_string (value));
        break;
    case PROP_DATE_CREATED:
        store_odrs_review_set_date_created (self, g_value_get_boxed (value));
        break;
    case PROP_DESCRIPTION:
        store_odrs_review_set_description (self, g_value_get_string (value));
        break;
    case PROP_ID:
        store_odrs_review_set_id (self, g_value_get_int64 (value));
        break;
    case PROP_RATING:
        store_odrs_review_set_rating (self, g_value_get_int64 (value));
        break;
    case PROP_SUMMARY:
        store_odrs_review_set_summary (self, g_value_get_string (value));
        break;
    case PROP_VOTED:
        store_odrs_review_set_voted (self, g_value_get_boolean (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
store_odrs_review_class_init (StoreOdrsReviewClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = store_odrs_review_dispose;
    G_OBJECT_CLASS (klass)->get_property = store_odrs_review_get_property;
    G_OBJECT_CLASS (klass)->set_property = store_odrs_review_set_property;

    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_AUTHOR,
                                     g_param_spec_string ("author", NULL, NULL, NULL, G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_DATE_CREATED,
                                     g_param_spec_boxed ("date-created", NULL, NULL, G_TYPE_DATE_TIME, G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_DESCRIPTION,
                                     g_param_spec_string ("description", NULL, NULL, NULL, G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_ID,
                                     g_param_spec_int64 ("id", NULL, NULL, G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_RATING,
                                     g_param_spec_int64 ("rating", NULL, NULL, G_MININT64, G_MAXINT64, 0, G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_SUMMARY,
                                     g_param_spec_string ("summary", NULL, NULL, NULL, G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (klass),
                                     PROP_VOTED,
                                     g_param_spec_boolean ("voted", NULL, NULL, FALSE, G_PARAM_READWRITE));
}

static void
store_odrs_review_init (StoreOdrsReview *self)
{
    self->author = g_strdup ("");
    self->description = g_strdup ("");
    self->summary = g_strdup ("");
}

StoreOdrsReview *
store_odrs_review_new (void)
{
    return g_object_new (store_odrs_review_get_type (), NULL);
}

StoreOdrsReview *
store_odrs_review_new_from_json (JsonNode *node)
{
    StoreOdrsReview *self = store_odrs_review_new ();

    JsonObject *object = json_node_get_object (node);
    if (json_object_has_member (object, "author"))
        store_odrs_review_set_author (self, json_object_get_string_member (object, "author"));
    if (json_object_has_member (object, "date-created")) {
        g_autoptr(GDateTime) date_created = g_date_time_new_from_unix_utc (json_object_get_int_member (object, "date-created"));
        store_odrs_review_set_date_created (self, date_created);
    }
    if (json_object_has_member (object, "description"))
        store_odrs_review_set_description (self, json_object_get_string_member (object, "description"));
    if (json_object_has_member (object, "id"))
        store_odrs_review_set_id (self, json_object_get_int_member (object, "id"));
    if (json_object_has_member (object, "rating"))
        store_odrs_review_set_rating (self, json_object_get_int_member (object, "rating"));
    if (json_object_has_member (object, "summary"))
        store_odrs_review_set_summary (self, json_object_get_string_member (object, "summary"));
    if (json_object_has_member (object, "voted"))
        store_odrs_review_set_voted (self, json_object_get_boolean_member (object, "voted"));

    return self;
}

JsonNode *
store_odrs_review_to_json (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), NULL);

    g_autoptr(JsonBuilder) builder = json_builder_new ();
    json_builder_begin_object (builder);
    json_builder_set_member_name (builder, "author");
    json_builder_add_string_value (builder, self->author);
    if (self->date_created) {
        json_builder_set_member_name (builder, "date-created");
        json_builder_add_int_value (builder, g_date_time_to_unix (self->date_created));
    }
    json_builder_set_member_name (builder, "description");
    json_builder_add_string_value (builder, self->description);
    json_builder_set_member_name (builder, "id");
    json_builder_add_int_value (builder, self->id);
    json_builder_set_member_name (builder, "rating");
    json_builder_add_int_value (builder, self->rating);
    json_builder_set_member_name (builder, "summary");
    json_builder_add_string_value (builder, self->summary);
    json_builder_set_member_name (builder, "voted");
    json_builder_add_boolean_value (builder, self->voted);
    json_builder_end_object (builder);

    return json_builder_get_root (builder);
}

void
store_odrs_review_set_author (StoreOdrsReview *self, const gchar *author)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    g_clear_pointer (&self->author, g_free);
    self->author = g_strdup (author);

    g_object_notify (G_OBJECT (self), "author");
}

const gchar *
store_odrs_review_get_author (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), NULL);
    return self->author;
}

void
store_odrs_review_set_date_created (StoreOdrsReview *self, GDateTime *date_created)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    if (self->date_created == date_created)
        return;
    g_clear_pointer (&self->date_created, g_date_time_unref);
    self->date_created = g_date_time_ref (date_created);

    g_object_notify (G_OBJECT (self), "date-created");
}

GDateTime *
store_odrs_review_get_date_created (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), NULL);
    return self->date_created;
}

void
store_odrs_review_set_description (StoreOdrsReview *self, const gchar *description)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    g_clear_pointer (&self->description, g_free);
    self->description = g_strdup (description);

    g_object_notify (G_OBJECT (self), "description");
}

const gchar *
store_odrs_review_get_description (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), NULL);
    return self->description;
}

void
store_odrs_review_set_id (StoreOdrsReview *self, gint64 id)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    self->id = id;

    g_object_notify (G_OBJECT (self), "id");
}

gint64
store_odrs_review_get_id (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), 0);
    return self->id;
}

void
store_odrs_review_set_rating (StoreOdrsReview *self, gint64 rating)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    if (self->rating == rating)
        return;
    self->rating = rating;

    g_object_notify (G_OBJECT (self), "rating");
}

gint64
store_odrs_review_get_rating (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), -1);
    return self->rating;
}

void
store_odrs_review_set_summary (StoreOdrsReview *self, const gchar *summary)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    g_clear_pointer (&self->summary, g_free);
    self->summary = g_strdup (summary);

    g_object_notify (G_OBJECT (self), "summary");
}

const gchar *
store_odrs_review_get_summary (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), NULL);
    return self->summary;
}

void
store_odrs_review_set_voted (StoreOdrsReview *self, gboolean voted)
{
    g_return_if_fail (STORE_IS_ODRS_REVIEW (self));

    if (self->voted == voted)
        return;
    self->voted = voted;

    g_object_notify (G_OBJECT (self), "voted");
}

gboolean
store_odrs_review_get_voted (StoreOdrsReview *self)
{
    g_return_val_if_fail (STORE_IS_ODRS_REVIEW (self), FALSE);
    return self->voted;
}
