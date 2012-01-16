/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * mmcli -- Control modem status & access information from the command line
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2012 - Google Inc.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <libmm-glib.h>

#include "mmcli.h"
#include "mmcli-common.h"

/* Context */
typedef struct {
    MMManager *manager;
    GCancellable *cancellable;
    MMObject *object;
    MMModemCdma *modem_cdma;
} Context;
static Context *ctx;

/* Options */
static gchar *activate_str;

static GOptionEntry entries[] = {
    { "cdma-activate", 0, 0, G_OPTION_ARG_STRING, &activate_str,
      "Provision the modem to use with a given carrier using OTA settings.",
      "[CARRIER]"
    },
    { NULL }
};

GOptionGroup *
mmcli_modem_cdma_get_option_group (void)
{
	GOptionGroup *group;

	group = g_option_group_new ("cdma",
	                            "CDMA options",
	                            "Show CDMA related options",
	                            NULL,
	                            NULL);
	g_option_group_add_entries (group, entries);

	return group;
}

gboolean
mmcli_modem_cdma_options_enabled (void)
{
    static guint n_actions = 0;
    static gboolean checked = FALSE;

    if (checked)
        return !!n_actions;

    n_actions = (!!activate_str);

    if (n_actions > 1) {
        g_printerr ("error: too many CDMA actions requested\n");
        exit (EXIT_FAILURE);
    }

    checked = TRUE;
    return !!n_actions;
}

static void
context_free (Context *ctx)
{
    if (!ctx)
        return;

    if (ctx->cancellable)
        g_object_unref (ctx->cancellable);
    if (ctx->modem_cdma)
        g_object_unref (ctx->modem_cdma);
    if (ctx->object)
        g_object_unref (ctx->object);
    if (ctx->manager)
        g_object_unref (ctx->manager);
    g_free (ctx);
}

void
mmcli_modem_cdma_shutdown (void)
{
    context_free (ctx);
}

static void
activate_process_reply (gboolean result,
                        const GError *error)
{
    if (!result) {
        g_printerr ("error: couldn't activate the modem: '%s'\n",
                    error ? error->message : "unknown error");
        exit (EXIT_FAILURE);
    }

    g_print ("successfully activated the modem\n");
}

static void
activate_ready (MMModemCdma  *modem_cdma,
                GAsyncResult *result,
                gpointer      nothing)
{
    gboolean operation_result;
    GError *error = NULL;

    operation_result = mm_modem_cdma_activate_finish (modem_cdma, result, &error);
    activate_process_reply (operation_result, error);

    mmcli_async_operation_done ();
}

static void
get_modem_ready (GObject      *source,
                 GAsyncResult *result,
                 gpointer      none)
{
    ctx->object = mmcli_get_modem_finish (result, &ctx->manager);
    ctx->modem_cdma = mm_object_get_modem_cdma (ctx->object);

    /* Request to activate the modem? */
    if (activate_str) {
        g_debug ("Asynchronously activating the modem...");
        mm_modem_cdma_activate (ctx->modem_cdma,
                                activate_str,
                                ctx->cancellable,
                                (GAsyncReadyCallback)activate_ready,
                                NULL);
        return;
    }

    g_warn_if_reached ();
}

void
mmcli_modem_cdma_run_asynchronous (GDBusConnection *connection,
                                   GCancellable    *cancellable)
{
    /* Initialize context */
    ctx = g_new0 (Context, 1);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);

    /* Get proper modem */
    mmcli_get_modem  (connection,
                      mmcli_get_common_modem_string (),
                      cancellable,
                      (GAsyncReadyCallback)get_modem_ready,
                      NULL);
}

void
mmcli_modem_cdma_run_synchronous (GDBusConnection *connection)
{
    GError *error = NULL;

    /* Initialize context */
    ctx = g_new0 (Context, 1);
    ctx->object = mmcli_get_modem_sync (connection,
                                        mmcli_get_common_modem_string (),
                                        &ctx->manager);
    ctx->modem_cdma = mm_object_get_modem_cdma (ctx->object);

    /* Request to activate the modem? */
    if (activate_str) {
        gboolean result;

        g_debug ("Synchronously activating the modem...");
        result = mm_modem_cdma_activate_sync (
            ctx->modem_cdma,
            activate_str,
            NULL,
            &error);
        activate_process_reply (result, error);
        return;
    }

    g_warn_if_reached ();
}
