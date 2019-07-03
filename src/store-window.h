/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <gtk/gtk.h>

#include "store-app.h"
#include "store-application.h"
#include "store-cache.h"
#include "store-category.h"
#include "store-odrs-client.h"
#include "store-snap-pool.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (StoreWindow, store_window, STORE, WINDOW, GtkApplicationWindow)

StoreWindow *store_window_new             (StoreApplication *application);

void         store_window_set_cache       (StoreWindow *window, StoreCache *cache);

void         store_window_set_categories  (StoreWindow *window, GPtrArray *categories);

void         store_window_set_odrs_client (StoreWindow *window, StoreOdrsClient *client);

void         store_window_set_snap_pool   (StoreWindow *window, StoreSnapPool *pool);

void         store_window_load            (StoreWindow *self);

void         store_window_show_app        (StoreWindow *self, StoreApp *app);

void         store_window_show_category   (StoreWindow *self, StoreCategory *category);

G_END_DECLS
