/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "store-review-dialog.h"

struct _StoreReviewDialog
{
    GtkDialog parent_instance;
};

G_DEFINE_TYPE (StoreReviewDialog, store_review_dialog, gtk_dialog_get_type ())

static void
store_review_dialog_class_init (StoreReviewDialogClass *klass)
{
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/io/snapcraft/Store/store-review-dialog.ui");
}

static void
store_review_dialog_init (StoreReviewDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

StoreReviewDialog *
store_review_dialog_new (void)
{
    return g_object_new (store_review_dialog_get_type (), NULL);
}
