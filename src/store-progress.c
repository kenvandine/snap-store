/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "store-progress.h"

struct _StoreProgress
{
    GObject parent_instance;

    gint64 done;
    gchar *label;
    gint64 total;
};

enum
{
    PROP_0,
    PROP_DONE,
    PROP_LABEL,
    PROP_TOTAL,
    PROP_LAST
};

G_DEFINE_TYPE (StoreProgress, store_progress, G_TYPE_OBJECT)

static void
store_progress_dispose (GObject *object)
{
    StoreProgress *self = STORE_PROGRESS (object);

    g_clear_pointer (&self->label, g_free);

    G_OBJECT_CLASS (store_progress_parent_class)->dispose (object);
}

static void
store_progress_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    StoreProgress *self = STORE_PROGRESS (object);

    switch (prop_id)
    {
    case PROP_DONE:
        g_value_set_int64 (value, self->done);
        break;
    case PROP_LABEL:
        g_value_set_string (value, self->label);
        break;
    case PROP_TOTAL:
        g_value_set_int64 (value, self->total);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
store_progress_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    StoreProgress *self = STORE_PROGRESS (object);

    switch (prop_id)
    {
    case PROP_DONE:
        store_progress_set_done (self, g_value_get_int64 (value));
        break;
    case PROP_LABEL:
        store_progress_set_label (self, g_value_get_string (value));
        break;
    case PROP_TOTAL:
        store_progress_set_total (self, g_value_get_int64 (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
store_progress_class_init (StoreProgressClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = store_progress_dispose;
    G_OBJECT_CLASS (klass)->get_property = store_progress_get_property;
    G_OBJECT_CLASS (klass)->set_property = store_progress_set_property;
}

static void
store_progress_init (StoreProgress *self)
{
    self->label = g_strdup ("");
}

StoreProgress *
store_progress_new (void)
{
    return g_object_new (store_progress_get_type (), NULL);
}

void
store_progress_set_done (StoreProgress *self, gint64 done)
{
    g_return_if_fail (STORE_IS_PROGRESS (self));

    if (self->done == done)
        return;

    self->done = done;
    g_object_notify (G_OBJECT (self), "done");
}

gint64
store_progress_get_done (StoreProgress *self)
{
    g_return_val_if_fail (STORE_IS_PROGRESS (self), 0);
    return self->done;
}

void
store_progress_set_label (StoreProgress *self, const gchar *label)
{
    g_return_if_fail (STORE_IS_PROGRESS (self));

    if (g_strcmp0 (self->label, label) == 0)
        return;

    g_clear_pointer (&self->label, g_free);
    self->label = g_strdup (label);
    g_object_notify (G_OBJECT (self), "label");
}

const gchar *
store_progress_get_label (StoreProgress *self)
{
    g_return_val_if_fail (STORE_IS_PROGRESS (self), NULL);
    return self->label;
}

void
store_progress_set_total (StoreProgress *self, gint64 total)
{
    g_return_if_fail (STORE_IS_PROGRESS (self));

    if (self->total == total)
        return;

    self->total = total;
    g_object_notify (G_OBJECT (self), "total");
}

gint64
store_progress_get_total (StoreProgress *self)
{
    g_return_val_if_fail (STORE_IS_PROGRESS (self), 0);
    return self->total;
}
