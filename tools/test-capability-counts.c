#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <cpdb/frontend.h>

#define TEST_BACKEND "TESTCAPS"

static int claimed(const char *printer_id)
{
    if (strcmp(printer_id, "big") == 0)
        return G_MAXINT;
    if (strcmp(printer_id, "negative") == 0)
        return -1;
    return 5;
}

static gboolean on_get_all_capabilities(PrintBackend *b, GDBusMethodInvocation *inv,
                                        const gchar *printer_id, const gchar *locale,
                                        gpointer user_data)
{
    int n = claimed(printer_id);

    print_backend_complete_get_all_capabilities(b, inv, 1,
        g_variant_new_parsed("[('media-type', 'Media Type', 'media', 'Media', 3, "
                             "'plain', %i, [('plain', 'Plain')], 0, 0)]", n),
        1,
        g_variant_new_parsed("[('a4', 'A4', 21000, 29700, %i, [(1, 2, 3, 4)])]", n));
    return TRUE;
}

static gboolean on_get_all_options(PrintBackend *b, GDBusMethodInvocation *inv,
                                   const gchar *printer_id, gpointer user_data)
{
    int n = claimed(printer_id);

    print_backend_complete_get_all_options(b, inv, 1,
        g_variant_new_parsed("[('media-type', 'media', 'plain', %i, [('plain',)])]", n),
        1,
        g_variant_new_parsed("[('a4', 21000, 29700, %i, [(1, 2, 3, 4)])]", n));
    return TRUE;
}

static void run_backend(int ready_fd)
{
    GDBusConnection *c = cpdbGetDbusConnection();
    PrintBackend *skel = print_backend_skeleton_new();
    GVariant *ret;

    g_signal_connect(skel, "handle-get-all-capabilities",
                     G_CALLBACK(on_get_all_capabilities), NULL);
    g_signal_connect(skel, "handle-get-all-options",
                     G_CALLBACK(on_get_all_options), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skel), c,
                                     CPDB_BACKEND_OBJ_PATH, NULL);
    ret = g_dbus_connection_call_sync(c, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                      "org.freedesktop.DBus", "RequestName",
                                      g_variant_new("(su)", CPDB_BACKEND_PREFIX TEST_BACKEND, 4),
                                      NULL, 0, -1, NULL, NULL);
    if (ret)
        g_variant_unref(ret);
    if (write(ready_fd, "x", 1) != 1)
        _exit(1);
    g_main_loop_run(g_main_loop_new(NULL, FALSE));
}

static int check(const char *what, int got, int want)
{
    printf("%s %s: count %d, want %d\n", got == want ? "PASS" : "FAIL", what, got, want);
    return got != want;
}

static int check_printer(GDBusConnection *c, const char *printer_id)
{
    cpdb_printer_obj_t *p = cpdbGetNewPrinterObj();
    cpdb_capabilities_t *caps;
    cpdb_options_t *opts;
    cpdb_capability_t *cap;
    cpdb_option_t *opt;
    cpdb_capability_media_t *cm;
    cpdb_media_t *m;
    char what[128];
    int want = strcmp(printer_id, "negative") == 0 ? 0 : 1;
    int fail = 0;

    p->id = g_strdup(printer_id);
    p->backend_name = g_strdup(TEST_BACKEND);
    p->backend_proxy = cpdbCreateBackend(c, CPDB_BACKEND_PREFIX TEST_BACKEND);

    caps = cpdbGetAllCapabilities(p, "en");
    cap = caps ? g_hash_table_lookup(caps->table, "media-type") : NULL;
    cm = caps ? g_hash_table_lookup(caps->media, "a4") : NULL;
    snprintf(what, sizeof(what), "%s capability choices", printer_id);
    fail |= check(what, cap ? cap->num_supported : -1, want);
    snprintf(what, sizeof(what), "%s capability media margins", printer_id);
    fail |= check(what, cm ? cm->num_margins : -1, want);

    opts = cpdbGetAllOptions(p);
    opt = opts ? g_hash_table_lookup(opts->table, "media-type") : NULL;
    m = opts ? g_hash_table_lookup(opts->media, "a4") : NULL;
    snprintf(what, sizeof(what), "%s option choices", printer_id);
    fail |= check(what, opt ? opt->num_supported : -1, want);
    snprintf(what, sizeof(what), "%s option media margins", printer_id);
    fail |= check(what, m ? m->num_margins : -1, want);

    cpdbDeleteCapabilities(caps);
    cpdbDeletePrinterObj(p);
    return fail;
}

int main(int argc, char **argv)
{
    const char *defaults[] = {"short", "negative", "big"};
    const char **ids = argc > 1 ? (const char **)argv + 1 : defaults;
    int num_ids = argc > 1 ? argc - 1 : 3;
    GDBusConnection *c;
    int i, fds[2], fail = 0;
    pid_t backend;
    char ready;

    setvbuf(stdout, NULL, _IONBF, 0);
    cpdbInit();
    if (pipe(fds) != 0)
        return 1;
    if ((backend = fork()) == 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        close(fds[0]);
        run_backend(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    if (read(fds[0], &ready, 1) != 1 || (c = cpdbGetDbusConnection()) == NULL)
    {
        puts("FAIL test backend did not start");
        kill(backend, SIGTERM);
        return 1;
    }

    for (i = 0; i < num_ids; i++)
        fail |= check_printer(c, ids[i]);

    kill(backend, SIGTERM);
    return fail;
}
