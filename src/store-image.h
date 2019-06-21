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

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (StoreImage, store_image, STORE, IMAGE, GtkImage)

StoreImage *store_image_new      (void);

void        store_image_set_size (StoreImage *image, guint size);

void        store_image_set_url  (StoreImage *image, const gchar *url);

G_END_DECLS
