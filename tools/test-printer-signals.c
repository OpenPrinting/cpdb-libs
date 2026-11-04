#include <stdio.h>
#include <cpdb/frontend.h>

static int removed;
static char removed_id[64];
static gboolean done;

static void printer_cb(cpdb_frontend_obj_t *f, cpdb_printer_obj_t *p,
                       cpdb_printer_update_t change)
{
    if (change == CPDB_CHANGE_PRINTER_REMOVED)
    {
        removed++;
        snprintf(removed_id, sizeof(removed_id), "%s", p->id);
    }
}

static void on_done(GDBusConnection *connection, const gchar *sender,
                    const gchar *path, const gchar *interface,
                    const gchar *signal, GVariant *parameters,
                    gpointer user_data)
{
    done = TRUE;
}

static void emit(GDBusConnection *c, const char *signal, GVariant *parameters)
{
    g_dbus_connection_emit_signal(c, NULL, CPDB_BACKEND_OBJ_PATH,
                                  "org.openprinting.PrintBackend",
                                  signal, parameters, NULL);
}

int main(void)
{
    cpdb_frontend_obj_t *f;
    cpdb_printer_obj_t *p;
    GDBusConnection *peer;
    PrintBackend *proxy;
    gint64 deadline;
    int fail = 0;

    cpdbInit();
    f = cpdbGetNewFrontendObj(printer_cb);
    cpdbIgnoreLastSavedSettings(f);
    cpdbConnectToDBus(f);
    if (f->connection == NULL || (peer = cpdbGetDbusConnection()) == NULL)
    {
        puts("FAIL no session bus");
        return 1;
    }

    proxy = print_backend_proxy_new_sync(f->connection,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                         G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                         CPDB_BACKEND_PREFIX "TEST",
                                         CPDB_BACKEND_OBJ_PATH, NULL, NULL);
    g_hash_table_insert(f->backend, g_strdup("TEST"), proxy);
    p = cpdbGetNewPrinterObj();
    p->id = g_strdup("p1");
    p->name = g_strdup("p1");
    p->backend_name = g_strdup("TEST");
    p->state = g_strdup("idle");
    cpdbAddPrinter(f, p);

    g_dbus_connection_signal_subscribe(f->connection, NULL,
                                       "org.openprinting.PrintBackend",
                                       "TestDone", NULL, NULL, 0,
                                       on_done, NULL, NULL);
    emit(peer, CPDB_SIGNAL_PRINTER_STATE_CHANGED,
         g_variant_new("(ssbs)", "unknown", "stopped", FALSE, "TEST"));
    emit(peer, CPDB_SIGNAL_PRINTER_REMOVED,
         g_variant_new("(ss)", "unknown", "TEST"));
    emit(peer, CPDB_SIGNAL_PRINTER_REMOVED,
         g_variant_new("(ss)", "p1", "TEST"));
    emit(peer, "TestDone", NULL);
    g_dbus_connection_flush_sync(peer, NULL, NULL);

    deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (!done && g_get_monotonic_time() < deadline)
        g_main_context_iteration(NULL, TRUE);

    if (!done)
    {
        puts("FAIL signals were not delivered");
        fail = 1;
    }
    if (removed == 1 && strcmp(removed_id, "p1") == 0)
        puts("PASS removal callback ran once, for p1");
    else
    {
        printf("FAIL removal callback ran %d times, last for '%s'\n",
               removed, removed_id);
        fail = 1;
    }
    if (f->num_printers == 0 && cpdbFindPrinterObj(f, "p1", "TEST") == NULL)
        puts("PASS p1 is gone from the printer list");
    else
    {
        printf("FAIL printer list still has %d printers\n", f->num_printers);
        fail = 1;
    }

    g_object_unref(peer);
    cpdbDeleteFrontendObj(f);
    return fail;
}
