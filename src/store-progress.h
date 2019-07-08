/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (StoreProgress, store_progress, STORE, PROGRESS, GObject)

StoreProgress *store_progress_new       (void);

void           store_progress_set_done  (StoreProgress *progress, gint64 done);

gint64         store_progress_get_done  (StoreProgress *progress);

void           store_progress_set_label (StoreProgress *progress, const gchar *label);

const gchar   *store_progress_get_label (StoreProgress *progress);

void           store_progress_set_total (StoreProgress *progress, gint64 total);

gint64         store_progress_get_total (StoreProgress *progress);

G_END_DECLS
