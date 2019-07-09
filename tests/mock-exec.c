/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "mock-snapd.h"
#include "mock-odrs-server.h"

static GMainLoop *loop = NULL;

static void
exit_cb (GObject *object, GAsyncResult *result, gpointer user_data G_GNUC_UNUSED)
{
    g_autoptr(GError) error = NULL;
    if (!g_subprocess_wait_finish (G_SUBPROCESS (object), result, &error))
        g_printerr ("Error running snap-store: %s\n", error->message);

    if (g_subprocess_get_if_exited (G_SUBPROCESS (object)))
        g_printerr ("snapd-store exited with status %d\n", g_subprocess_get_exit_status (G_SUBPROCESS (object)));
    if (g_subprocess_get_if_signaled (G_SUBPROCESS (object)))
        g_printerr ("snapd-store terminated with signal %d\n", g_subprocess_get_term_sig (G_SUBPROCESS (object)));

    g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
    if (argc < 2) {
        g_printerr ("Usage: %s PATH_TO_SNAP_STORE\n", argv[0]);
        return EXIT_FAILURE;
    }
    const gchar *snap_store_path = argv[1]; // FIXME: Support argv[2..]

    loop = g_main_loop_new (NULL, FALSE);

    g_autoptr(MockSnapd) snapd = mock_snapd_new ();
    g_autoptr(GError) error = NULL;
    if (!mock_snapd_start (snapd, &error)) {
        g_printerr ("Failed to start mock snapd server: %s\n", error->message);
        return EXIT_FAILURE;
    }
    g_printerr ("snapd listening on socket %s\n", mock_snapd_get_socket_path (snapd));

    g_autoptr(MockOdrsServer) server = mock_odrs_server_new ();
    if (!mock_odrs_server_start (server, &error)) {
        g_printerr ("Failed to start mock ODRS server: %s\n", error->message);
        return EXIT_FAILURE;
    }
    g_printerr ("ODRS listening on port %u\n", mock_odrs_server_get_port (server));

    mock_snapd_add_store_section (snapd, "featured");
    mock_snapd_add_store_section (snapd, "games");

    MockSnap *snap = mock_snapd_add_store_snap (snapd, "alpha");
    mock_snap_add_store_section (snap, "featured");
    snap = mock_snapd_add_store_snap (snapd, "bravo");
    snap = mock_snapd_add_store_snap (snapd, "charlie");
    snap = mock_snapd_add_store_snap (snapd, "delta");
    mock_snap_add_store_section (snap, "games");
    snap = mock_snapd_add_store_snap (snapd, "echo");
    mock_snap_add_store_section (snap, "games");

    g_autofree gchar *odrs_server_arg = g_strdup_printf ("http://localhost:%d", mock_odrs_server_get_port (server));
    g_autoptr(GSubprocess) store_process = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE, &error, snap_store_path, "--odrs-server", odrs_server_arg, "--snapd-socket-path", mock_snapd_get_socket_path (snapd), NULL);
    if (store_process == NULL) {
        g_printerr ("Failed to start snap-store: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_subprocess_wait_async (store_process, NULL, exit_cb, NULL);

    g_main_loop_run (loop);

    return EXIT_SUCCESS;
}
