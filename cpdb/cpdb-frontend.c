#include "cpdb-frontend.h"

/**
________________________________________________ cpdb_frontend_obj_t __________________________________________

**/
/**
 * These static functions are callbacks used by the actual cpdb_frontend_obj_t functions.
 */

cpdb_frontend_obj_t *cpdbGetNewFrontendObj(char *instance_name,
                                           cpdb_event_callback add_cb,
                                           cpdb_event_callback rem_cb)
{
    cpdb_frontend_obj_t *f = malloc(sizeof(cpdb_frontend_obj_t));
    
    f->skeleton = print_frontend_skeleton_new();
    f->connection = NULL;
    if (instance_name == NULL)
        f->bus_name = cpdbGetStringCopy(CPDB_DIALOG_BUS_NAME);
    else
        f->bus_name = cpdbConcat(CPDB_DIALOG_BUS_NAME, instance_name);
    f->add_cb = add_cb;
    f->rem_cb = rem_cb;
    f->num_backends = 0;
    f->backend = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       free,
                                       g_object_unref);
    f->num_printers = 0;
    f->printer = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       free,
                                       (GDestroyNotify) cpdbDeletePrinterObj);
    f->last_saved_settings = cpdbReadSettingsFromDisk();
    return f;
}

void cpdbDeleteFrontendObj(cpdb_frontend_obj_t *f)
{
    if (f == NULL)
        return;
    
    cpdbDisconnectFromDBus(f);

    if (f->skeleton)
        g_object_unref(f->skeleton);
    if (f->bus_name)
        free(f->bus_name);
    if (f->backend)
        g_hash_table_destroy(f->backend);
    if (f->printer)
        g_hash_table_destroy(f->printer);
    if (f->last_saved_settings)
        cpdbDeleteSettings(f->last_saved_settings);
    
    free(f);
}

static void on_printer_added(GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{
    cpdb_frontend_obj_t *f = (cpdb_frontend_obj_t *)user_data;
    cpdb_printer_obj_t *p = cpdbGetNewPrinterObj();
    
    /* If some previously saved settings were retrieved, 
     * use them in this new cpdb_printer_obj_t */
    if (f->last_saved_settings != NULL)
    {
        cpdbCopySettings(f->last_saved_settings, p->settings);
    }
    cpdbFillBasicOptions(p, parameters);
    cpdbAddPrinter(f, p);
    f->add_cb(p);
}

static void on_printer_removed(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer user_data)
{
    cpdb_frontend_obj_t *f = (cpdb_frontend_obj_t *)user_data;
    char *printer_id;
    char *backend_name;
    
    g_variant_get(parameters, "(ss)", &printer_id, &backend_name);
    cpdb_printer_obj_t *p = cpdbRemovePrinter(f, printer_id, backend_name);
    f->rem_cb(p);
}

static void on_name_acquired(GDBusConnection *connection,
                             const gchar *name,
                             gpointer user_data)
{
    GError *error = NULL;
    cpdb_frontend_obj_t *f = user_data;

    cpdbDebugLog("Acquired bus name", CPDB_DEBUG_LEVEL_INFO);
    
    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       CPDB_SIGNAL_PRINTER_ADDED,       //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       on_printer_added,                //callback
                                       user_data,                       //user_data
                                       NULL);

    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       CPDB_SIGNAL_PRINTER_REMOVED,     //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       on_printer_removed,              //callback
                                       user_data,                       //user_data
                                       NULL);

    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(f->skeleton),
                                     connection, 
                                     CPDB_DIALOG_OBJ_PATH,
                                     &error);
    if (error)
    {
        cpdbDebugLog2("Error exporting frontend interface",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return;
    }
    
    cpdbActivateBackends(f);
}

static void on_name_lost(GDBusConnection *connection,
                         const gchar *name,
                         gpointer user_data)
{
    cpdbDebugLog("Lost bus name", CPDB_DEBUG_LEVEL_INFO);
}

GDBusConnection *get_dbus_connection()
{
    gchar *bus_addr;
    GError *error = NULL;
    GDBusConnection *connection;
    
    bus_addr = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION,
                                               NULL,
                                               &error);
    
    connection = g_dbus_connection_new_for_address_sync(bus_addr,
                                                        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                        G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                        NULL,
                                                        NULL,
                                                        &error);
    if (error)
    {
        cpdbDebugLog2("Error acquiring bus connection",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    cpdbDebugLog("Acquired bus connection", CPDB_DEBUG_LEVEL_INFO);
    
    return connection;
}

void cpdbConnectToDBus(cpdb_frontend_obj_t *f)
{
    if ((f->connection = get_dbus_connection()) == NULL)
    {
        cpdbDebugLog("Couldn't connect to DBus", CPDB_DEBUG_LEVEL_ERR);
        return;
    }
    
    f->own_id = g_bus_own_name_on_connection(f->connection,
                                             f->bus_name,
                                             0,
                                             on_name_acquired,
                                             on_name_lost,
                                             f,
                                             NULL);
}

void cpdbDisconnectFromDBus(cpdb_frontend_obj_t *f)
{
    if (f->connection == NULL || g_dbus_connection_is_closed(f->connection))
        return;
    
    print_frontend_emit_stop_listing(f->skeleton);
    g_dbus_connection_flush_sync(f->connection, NULL, NULL);
    
    g_bus_unown_name(f->own_id);
    g_dbus_connection_close_sync(f->connection, NULL, NULL);
}

void cpdbActivateBackends(cpdb_frontend_obj_t *f)
{
    DIR *d;
    int len;
    struct dirent *dir;
    PrintBackend *proxy;
    char *backend_suffix;
    
    d = opendir(CPDB_BACKEND_INFO_DIR);
    len = strlen(CPDB_BACKEND_PREFIX);

    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (strncmp(CPDB_BACKEND_PREFIX, dir->d_name, len) == 0)
            {
                backend_suffix = cpdbGetStringCopy((dir->d_name) + len);
                cpdbDebugLog2("Found backend",
                              backend_suffix,
                              CPDB_DEBUG_LEVEL_INFO);

                proxy = cpdbCreateBackendFromFile(f->connection,
                                                  dir->d_name);

                g_hash_table_insert(f->backend, backend_suffix, proxy);
                f->num_backends++;

                print_backend_call_activate_backend(proxy, NULL, NULL, NULL);
            }
        }

        closedir(d);
    }
}

PrintBackend *cpdbCreateBackendFromFile(GDBusConnection *connection,
                                        const char *backend_file_name)
{
    FILE *file;
    PrintBackend *proxy;
    GError *error = NULL;
    char *path, *backend_name;
    char obj_path[CPDB_BSIZE];
    
    backend_name = cpdbGetStringCopy(backend_file_name);
    path = cpdbConcatPath(CPDB_BACKEND_INFO_DIR, backend_file_name);
    
    if ((file = fopen(path, "r")) == NULL)
    {
        cpdbDebugLog("Error creating backend.", CPDB_DEBUG_LEVEL_ERR);
        cpdbDebugLog2("Couldn't open file for reading",
                      path,
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    fscanf(file, "%s", obj_path);
    
    fclose(file);
    free(path);
    
    proxy = print_backend_proxy_new_sync(connection,
                                         0,
                                         backend_name,
                                         obj_path,
                                         NULL,
                                         &error);
    if (error)
    {
        char *msg = malloc(sizeof(char) * (strlen(backend_name) + 40));
        sprintf(msg, "Error creating backend proxy for %s", backend_name);
        cpdbDebugLog2(msg, error->message, CPDB_DEBUG_LEVEL_ERR);
        free(msg);
    }
    
    return proxy;
}

void cpdbIgnoreLastSavedSettings(cpdb_frontend_obj_t *f)
{
    cpdbDebugLog("Ignoring previous settings", CPDB_DEBUG_LEVEL_INFO);
    cpdbDeleteSettings(f->last_saved_settings);
    f->last_saved_settings = NULL;
}

gboolean cpdbAddPrinter(cpdb_frontend_obj_t *f, 
                        cpdb_printer_obj_t *p)
{
    p->backend_proxy = g_hash_table_lookup(f->backend, p->backend_name);
    if (p->backend_proxy == NULL)
    {
        cpdbDebugLog2("Couldn't add printer, backend doesn't exist",
                      p->backend_name,
                      CPDB_DEBUG_LEVEL_ERR);
        return FALSE;
    }
    g_object_ref(p->backend_proxy);

    g_hash_table_insert(f->printer, cpdbConcatSep(p->id, p->backend_name), p);
    f->num_printers++;

    return TRUE;
}

cpdb_printer_obj_t *cpdbRemovePrinter(cpdb_frontend_obj_t *f,
                                      const char *printer_id,
                                      const char *backend_name)
{
    char *key;
    cpdb_printer_obj_t *p = NULL;

    key = cpdbConcatSep(printer_id, backend_name);
    if (g_hash_table_contains(f->printer, key))
    {
        p = cpdbFindPrinterObj(f, printer_id, backend_name);
        g_hash_table_remove(f->printer, key);
        f->num_printers--;
    }
    
    free(key);
    return p;
}

void cpdbRefreshPrinterList(cpdb_frontend_obj_t *f)
{
    print_frontend_emit_refresh_backend(f->skeleton);
}

void cpdbHideRemotePrinters(cpdb_frontend_obj_t *f)
{
    print_frontend_emit_hide_remote_printers(f->skeleton);
}

void cpdbUnhideRemotePrinters(cpdb_frontend_obj_t *f)
{
    print_frontend_emit_unhide_remote_printers(f->skeleton);
}

void cpdbHideTemporaryPrinters(cpdb_frontend_obj_t *f)
{
    print_frontend_emit_hide_temporary_printers(f->skeleton);
}

void cpdbUnhideTemporaryPrinters(cpdb_frontend_obj_t *f)
{
    print_frontend_emit_unhide_temporary_printers(f->skeleton);
}

cpdb_printer_obj_t *cpdbFindPrinterObj(cpdb_frontend_obj_t *f,
                                       const char *printer_id,
                                       const char *backend_name)
{
    char *hashtable_key;
    cpdb_printer_obj_t *p;

    if (printer_id == NULL || backend_name == NULL)
    {
        cpdbDebugLog("Invalid parameters: cpdbFindPrinterObj()", 
                     CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }

    hashtable_key = cpdbConcatSep(printer_id, backend_name);
    p = g_hash_table_lookup(f->printer, hashtable_key);
    if (p == NULL)
    {
        cpdbDebugLog("Couldn't find printer: Doesn't exist",
                     CPDB_DEBUG_LEVEL_WARN);
    }

    free(hashtable_key);
    return p;
}

cpdb_printer_obj_t *cpdbGetDefaultPrinterForBackend(cpdb_frontend_obj_t *f,
                                                    const char *backend_name)
{
    char *def;
    PrintBackend *proxy;

    proxy = g_hash_table_lookup(f->backend, backend_name);
    if (proxy == NULL)
    {
        cpdbDebugLog2("Couldn't find backend proxy",
                      backend_name,
                      CPDB_DEBUG_LEVEL_WARN);
    
        proxy = cpdbCreateBackendFromFile(f->connection, backend_name);
        if (proxy == NULL)
        {
            cpdbDebugLog2("Couldn't create backend proxy",
                          backend_name,
                          CPDB_DEBUG_LEVEL_ERR);
            return NULL;
        }
    }

    print_backend_call_get_default_printer_sync(proxy, &def, NULL, NULL);
    return cpdbFindPrinterObj(f, def, backend_name);
}

GList *cpdbLoadDefaultPrinters(char *path)
{
    FILE *fp;
    char buf[CPDB_BSIZE];
    GList *printers = NULL;

    if ((fp = fopen(path, "r")) == NULL)
    {
        cpdbDebugLog2("Couldn't open file for reading",
                      path,
                      CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        buf[strcspn(buf, "\r\n")] = 0;
        printers = g_list_prepend(printers, cpdbGetStringCopy(buf));
    }
    printers = g_list_reverse(printers);

    fclose(fp);
    return printers;
}

cpdb_printer_obj_t *cpdbGetDefaultPrinter(cpdb_frontend_obj_t *f)
{   
    gpointer key, value;
    GHashTableIter iter;
    char *conf_dir, *path, *printer_id, *backend_name;
    cpdb_printer_obj_t *default_printer = NULL;
    GList *printer, *user_printers, *system_printers, *printers = NULL;

    if (f->num_printers == 0 || f->num_backends == 0)
    {
        cpdbDebugLog("No printers found while obtaining default printer",
                     CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }
    
    /** Find a default printer from user config first,
     *  before trying system wide config **/
    conf_dir = cpdbGetUserConfDir();
    if (conf_dir)
    {
        path = cpdbConcatPath(conf_dir, CPDB_DEFAULT_PRINTERS_FILE);
        printers = g_list_concat(printers, cpdbLoadDefaultPrinters(path));
        free(path);
        free(conf_dir);
    }
    conf_dir = cpdbGetSysConfDir();
    if (conf_dir)
    {
        path = cpdbConcatPath(conf_dir, CPDB_DEFAULT_PRINTERS_FILE);
        printers = g_list_concat(printers, cpdbLoadDefaultPrinters(path));
        free(path);
        free(conf_dir);
    }
    
    for (printer = printers; printer != NULL; printer = printer->next)
    {
        printer_id = strtok(printer->data, "#"); 
        backend_name = strtok(NULL, "\n");

        default_printer = cpdbFindPrinterObj(f, printer_id, backend_name);
        if (default_printer)
        {
            g_list_free_full(printers, free);
            return default_printer;
        }
    }
    if (printers)
        g_list_free_full(printers, free);

    cpdbDebugLog("Couldn't find a valid default printer from config",
                 CPDB_DEBUG_LEVEL_INFO);

    /**  Fallback to default CUPS printer if CUPS backend exists **/
    default_printer = cpdbGetDefaultPrinterForBackend(f, "CUPS");
    if (default_printer)
        return default_printer;
    cpdbDebugLog("Couldn't find a valid default CUPS printer",
                 CPDB_DEBUG_LEVEL_INFO);
    
    /** Fallback to default FILE printer if FILE backend exists **/
    default_printer = cpdbGetDefaultPrinterForBackend(f, "FILE");
    if (default_printer)
        return default_printer;
    cpdbDebugLog("Couldn't find a valid default FILE printer",
                 CPDB_DEBUG_LEVEL_INFO);
    
    /** Fallback to the default printer of first backend found **/
    g_hash_table_iter_init(&iter, f->backend);
    g_hash_table_iter_next(&iter, &key, &value);

    backend_name = (char *) key;
    default_printer = cpdbGetDefaultPrinterForBackend(f, backend_name);
    if (default_printer)
        return default_printer;
    cpdbDebugLog("Couldn't find a valid backend", CPDB_DEBUG_LEVEL_WARN);
    
    /** Fallback to first printer found **/
    g_hash_table_iter_init(&iter, f->printer);
    g_hash_table_iter_next(&iter, &key, &value);
    default_printer = (cpdb_printer_obj_t *) value;

    return default_printer;
}

int cpdbSetDefaultPrinter(char *path,
                          cpdb_printer_obj_t *p)
{
    FILE *fp;
    char *printer_data;
    GList *printer, *next, *printers;
    
    printers = cpdbLoadDefaultPrinters(path);
    printer_data = cpdbConcatSep(p->id, p->backend_name);
    
    if ((fp = fopen(path, "w")) == NULL)
    {
        cpdbDebugLog2("Couldn't open file for writing",
                      path,
                      CPDB_DEBUG_LEVEL_ERR);
        return 0;
    }

    /* Delete duplicate entries */
    printer = printers;
    while (printer != NULL)
    {
        next = printer->next;
        if (strcmp(printer->data, printer_data) == 0)
        {
            free(printer->data);
            printers = g_list_delete_link(printers, printer);
        }

        printer = next;
    }

    printers = g_list_prepend(printers, printer_data);
    for (printer = printers; printer != NULL; printer = printer->next)
    {
        fprintf(fp, "%s\n", (char *)printer->data);
    }
    g_list_free_full(printers, free);

    fclose(fp);
    return 1;
}

int cpdbSetUserDefaultPrinter(cpdb_printer_obj_t *p)
{
    int ret;
    char *conf_dir, *path;

    if ((conf_dir = cpdbGetUserConfDir()) == NULL)
    {
        cpdbDebugLog2("Error setting default printer",
                      "Couldn't get user config directory",
                      CPDB_DEBUG_LEVEL_ERR);
        return 0;
    }
    path = cpdbConcatPath(conf_dir, CPDB_DEFAULT_PRINTERS_FILE);
    ret = cpdbSetDefaultPrinter(path, p);

    free(path);
    free(conf_dir);
    return ret;
}

int cpdbSetSystemDefaultPrinter(cpdb_printer_obj_t *p)
{
    int ret;
    char *conf_dir, *path;

    if ((conf_dir = cpdbGetSysConfDir()) == NULL)
    {
        cpdbDebugLog2("Error setting default printer",
                      "Couldn't get system config directory",
                      CPDB_DEBUG_LEVEL_ERR);
        return 0;
    }
    path = cpdbConcatPath(conf_dir, CPDB_DEFAULT_PRINTERS_FILE);
    ret = cpdbSetDefaultPrinter(path, p);

    free(path);
    free(conf_dir);
    return ret;
}

int cpdbGetAllJobs(cpdb_frontend_obj_t *f,
                   cpdb_job_t **j,
                   gboolean active_only)
{
    
	/**inititalizing the arrays required for each of the backends **/
	
    /** num_jobs[] stores the number of jobs for each of the backends**/
    int *num_jobs = g_new(int, f->num_backends);
    
    /**backend_names[] stores the name for each of the backends**/
    char **backend_names = g_new0(char *, f->num_backends);
    
    /**retval[] stores the gvariant returned by the respective backend **/
    GVariant **retval = g_new(GVariant *, f->num_backends);
    
    GError *error = NULL;
    gpointer key, value;
    GHashTableIter iter;
    int i = 0, total_jobs = 0;
 
    /** Iterating over all the backends and getting each's active jobs**/
    g_hash_table_iter_init(&iter, f->backend);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        PrintBackend *proxy = (PrintBackend *)value;
        
        backend_names[i] = (char *)key;
        cpdbDebugLog2("Trying to get jobs for backend",
                      backend_names[i],
                      CPDB_DEBUG_LEVEL_INFO);

        print_backend_call_get_all_jobs_sync(proxy,
                                             active_only,
                                             &(num_jobs[i]),
                                             &(retval[i]),
                                             NULL,
                                             &error);
        
        if(error)
        {
            cpdbDebugLog2("Call failed",
                          error->message,
                          CPDB_DEBUG_LEVEL_ERR);
        	num_jobs[i] = 0;
        	
        }
        else
        {
        	cpdbDebugLog("Call succeeded", CPDB_DEBUG_LEVEL_INFO);
        }
        
        total_jobs += num_jobs[i];
        i++; /** off to the next backend **/
    }
    
    cpdb_job_t *jobs = g_new(cpdb_job_t, total_jobs);
    int n = 0;

    for (i = 0; i < f->num_backends; i++)
    {
    	if(num_jobs[i])
        {
    		cpdbUnpackJobArray(retval[i],
                               num_jobs[i],
                               jobs + n,
                               backend_names[i]);
        }
        n += num_jobs[i];
    }
    *j = jobs;

    free(num_jobs);
    return total_jobs;
}

/**
________________________________________________ cpdb_printer_obj_t __________________________________________
**/

cpdb_printer_obj_t *cpdbGetNewPrinterObj()
{
    cpdb_printer_obj_t *p = malloc(sizeof(cpdb_printer_obj_t));
    p->options = NULL;
    p->settings = cpdbGetNewSettings();
    return p;
}

void cpdbDeletePrinterObj(cpdb_printer_obj_t *p)
{
    if (p == NULL)
        return;
    
    if (p->backend_name)
        free(p->backend_name);
    if (p->backend_proxy)
        g_object_unref(p->backend_proxy);
    if (p->options)
        cpdbDeleteOptions(p->options);

    if (p->settings)
        cpdbDeleteSettings(p->settings);
    
    free(p);
}

void cpdbFillBasicOptions(cpdb_printer_obj_t *p,
                          GVariant *gv)
{
    g_variant_get(gv, CPDB_PRINTER_ADDED_ARGS,
                  &(p->id),
                  &(p->name),
                  &(p->info),
                  &(p->location),
                  &(p->make_and_model),
                  &(p->accepting_jobs),
                  &(p->state),
                  &(p->backend_name));
}

void cpdbPrintBasicOptions(cpdb_printer_obj_t *p)
{
    printf("-------------------------\n");
    printf("Printer %s\n", p->id);
    printf("name: %s\n", p->name);
    printf("location: %s\n", p->location);
    printf("info: %s\n", p->info);
    printf("make and model: %s\n", p->make_and_model);
    printf("accepting jobs? %s\n", (p->accepting_jobs ? "yes" : "no"));
    printf("state: %s\n", p->state);
    printf("backend: %s\n", p->backend_name);
    printf("-------------------------\n\n");
}

gboolean cpdbIsAcceptingJobs(cpdb_printer_obj_t *p)
{
    GError *error = NULL;
    
    print_backend_call_is_accepting_jobs_sync(p->backend_proxy,
                                              p->id,
                                              &p->accepting_jobs,
                                              NULL,
                                              &error);
    if (error)
    {
        cpdbDebugLog2("Error retrieving accepting_jobs",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return FALSE;
    }

    return p->accepting_jobs;
}

char *cpdbGetState(cpdb_printer_obj_t *p)
{
    GError *error = NULL;
    
    print_backend_call_get_printer_state_sync(p->backend_proxy,
                                              p->id,
                                              &p->state,
                                              NULL,
                                              &error);
    if (error)
    {
        cpdbDebugLog2("Error retrieving printer state",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }

    return p->state;
}

cpdb_options_t *cpdbGetAllOptions(cpdb_printer_obj_t *p)
{
    if (p == NULL) 
    {
        cpdbDebugLog("Invalid params: cpdbGetAllOptions()", 
                     CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    /** 
     * If the options were previously queried, 
     * return them, instead of querying again.
    */
    if (p->options)
        return p->options;

    p->options = cpdbGetNewOptions();
    GError *error = NULL;
    int num_options, num_media;
    GVariant *var, *media_var;
    print_backend_call_get_all_options_sync(p->backend_proxy,
                                            p->id,
                                            &num_options,
                                            &var,
                                            &num_media,
                                            &media_var,
                                            NULL,
                                            &error);
    if (!error)
    {
        cpdbUnpackOptions(num_options,
                          var,
                          num_media,
                          media_var,
                          p->options);
        return p->options;
    }
    else 
    {
        cpdbDebugLog2("Error retrieving printer options",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
}

cpdb_option_t *cpdbGetOption(cpdb_printer_obj_t *p,
                             const char *name)
{
    if (p == NULL || name == NULL) 
    {
        cpdbDebugLog("Invalid params: cpdbGetOption()", 
                     CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    cpdbGetAllOptions(p);
    if (!g_hash_table_contains(p->options->table, name))
        return NULL;
    return (cpdb_option_t *)(g_hash_table_lookup(p->options->table, name));
}

char *cpdbGetDefault(cpdb_printer_obj_t *p,
                     const char *name)
{
    if (p == NULL || name == NULL)
    {
        cpdbDebugLog("Invalid params: cpdbGetDefault()", 
                     CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    cpdb_option_t *o = cpdbGetOption(p, name);
    if (!o)
        return NULL;
    return o->default_value;
}

char *cpdbGetSetting(cpdb_printer_obj_t *p,
                     const char *name)
{
    if (p == NULL || name == NULL)
    {
        cpdbDebugLog("Invalid params: cpdbGetSetting()", 
                     CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    if (!g_hash_table_contains(p->settings->table, name))
        return NULL;
    return g_hash_table_lookup(p->settings->table, name);
}

char *cpdbGetCurrent(cpdb_printer_obj_t *p,
                     const char *name)
{
    char *set = cpdbGetSetting(p, name);
    if (set)
        return set;

    return cpdbGetDefault(p, name);
}

int cpdbGetActiveJobsCount(cpdb_printer_obj_t *p)
{
    int count;
    GError *error;
    
    print_backend_call_get_active_jobs_count_sync(p->backend_proxy,
                                                  p->id,
                                                  &count,
                                                  NULL,
                                                  &error);
    if (error)
    {
        cpdbDebugLog2("Error getting active jobs count",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return -1;
    }
    
    return count;
}

char *cpdbPrintFile(cpdb_printer_obj_t *p,
                    const char *file_path)
{
    char *jobid, *absolute_file_path;
    GError *error = NULL;
    
    absolute_file_path = cpdbGetAbsolutePath(file_path);
    print_backend_call_print_file_sync(p->backend_proxy,
                                       p->id,
                                       absolute_file_path,
                                       p->settings->count,
                                       cpdbSerializeToGVariant(p->settings),
                                       "final-file-path-not-required",
                                       &jobid,
                                       NULL,
                                       &error);
    free(absolute_file_path);
                                       
    if (error)
    {
        cpdbDebugLog2("Error printing file",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    
    if (jobid == NULL || jobid == "")
    {
        cpdbDebugLog("Error printing file", CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    
    cpdbDebugLog2("File printed successfully",
                  jobid,
                  CPDB_DEBUG_LEVEL_INFO);
    cpdbSaveSettingsToDisk(p->settings);
    
    return jobid;
}

char *cpdbPrintFilePath(cpdb_printer_obj_t *p,
                        const char *file_path,
                        const char *final_file_path)
{
    char *result, *absolute_file_path, *absolute_final_file_path;
    GError *error = NULL;
    
    absolute_file_path = cpdbGetAbsolutePath(file_path);
    absolute_final_file_path = cpdbGetAbsolutePath(final_file_path);
    print_backend_call_print_file_sync(p->backend_proxy,
                                       p->id,
                                       absolute_file_path,
                                       p->settings->count,
                                       cpdbSerializeToGVariant(p->settings),
                                       absolute_final_file_path,
                                       &result,
                                       NULL,
                                       &error);
    free(absolute_file_path);
    free(absolute_final_file_path);
    
    if (error)
    {
        cpdbDebugLog2("Error printing file",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    
    if (result == NULL)
    {
        cpdbDebugLog("Error printing file", CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    
    cpdbDebugLog2("File printed successfully",
                  absolute_final_file_path,
                  CPDB_DEBUG_LEVEL_INFO);

    return result;
}

void cpdbAddSettingToPrinter(cpdb_printer_obj_t *p,
                             const char *name,
                             const char *val)
{
    if (p == NULL)
        return;

    cpdbAddSetting(p->settings, name, val);
}

gboolean cpdbClearSettingFromPrinter(cpdb_printer_obj_t *p,
                                     const char *name)
{
    cpdbClearSetting(p->settings, name);
}

gboolean cpdbCancelJob(cpdb_printer_obj_t *p,
                       const char *job_id)
{
    gboolean status;
    GError *error = NULL;
    
    print_backend_call_cancel_job_sync(p->backend_proxy,
                                       job_id,
                                       p->id,
                                       &status,
                                       NULL,
                                       &error);
    if (error)
    {
        cpdbDebugLog2("Error cancelling job",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return FALSE;
    }
    
    return status;
}

void cpdbPicklePrinterToFile(cpdb_printer_obj_t *p,
                             const char *filename,
                             const cpdb_frontend_obj_t *parent_dialog)
{
	FILE *fp;
	char *path;
    const char *unique_bus_name;
    GHashTableIter iter;
    gpointer key, value;
	
    print_backend_call_keep_alive_sync(p->backend_proxy, NULL, NULL);
    
    path = cpdbGetAbsolutePath(filename);
    if ((fp = fopen(path, "w")) == NULL)
    {
        cpdbDebugLog("Error pickling printer.", CPDB_DEBUG_LEVEL_ERR);
        cpdbDebugLog2("Couldn't open file for writing",
                      path,
                      CPDB_DEBUG_LEVEL_ERR);
        return;
    }

    unique_bus_name = g_dbus_connection_get_unique_name(parent_dialog->connection);
    if (unique_bus_name == NULL)
    {
        cpdbDebugLog2("Error pickling printer",
                     "Couldn't get unique bus name",
                     CPDB_DEBUG_LEVEL_ERR);
        return;
    }
    
    fprintf(fp, "%s#\n", unique_bus_name);
    fprintf(fp, "%s#\n", p->backend_name);
    fprintf(fp, "%s#\n", p->id);
    fprintf(fp, "%s#\n", p->name);
    fprintf(fp, "%s#\n", p->location);
    fprintf(fp, "%s#\n", p->info);
    fprintf(fp, "%s#\n", p->make_and_model);
    fprintf(fp, "%s#\n", p->state);
    fprintf(fp, "%d\n", p->accepting_jobs);

    /* Not pickling the cpdb_options_t, 
     * because it can be reconstructed by querying the backend */

    fprintf(fp, "%d\n", p->settings->count);
    g_hash_table_iter_init(&iter, p->settings->table);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        fprintf(fp, "%s#%s#\n", (char *)key, (char *)value);
    }
    
    fclose(fp);
    free(path);
}

cpdb_printer_obj_t *cpdbResurrectPrinterFromFile(const char *filename)
{
    FILE *fp;
    int count;
    char buf[CPDB_BSIZE];
    GDBusConnection *connection;
    char *name, *value, *path, *previous_parent_dialog, *backend_file_name;
    GError *error = NULL;
    cpdb_printer_obj_t *p;

    path = cpdbGetAbsolutePath(filename);
    if ((fp = fopen(path, "r")) == NULL)
    {
        cpdbDebugLog("Error resurrecting printer", CPDB_DEBUG_LEVEL_ERR);
        cpdbDebugLog2("Couldn't open file for reading",
                      path,
                      CPDB_DEBUG_LEVEL_ERR);
        free(path);
        return NULL;
    }

    p = cpdbGetNewPrinterObj();
    cpdbDebugLog2("Ressurecting printer from file",
                  path,
                  CPDB_DEBUG_LEVEL_INFO);
    free(path);

    fgets(buf, sizeof(buf), fp);
    previous_parent_dialog = cpdbGetStringCopy(strtok(buf, "#"));
    cpdbDebugLog2("Previous parent dialog",
                  previous_parent_dialog,
                  CPDB_DEBUG_LEVEL_INFO);

    fgets(buf, sizeof(buf), fp);
    p->backend_name = cpdbGetStringCopy(strtok(buf, "#"));
    cpdbDebugLog2("Backend name",
                  p->backend_name,
                  CPDB_DEBUG_LEVEL_INFO);
    
    backend_file_name = cpdbConcat(CPDB_BACKEND_PREFIX, p->backend_name);
    if ((connection = get_dbus_connection()) == NULL)
    {
        cpdbDebugLog("Error resurrecting printer", CPDB_DEBUG_LEVEL_ERR);
        fclose(fp);
        return NULL;
    }
    p->backend_proxy = cpdbCreateBackendFromFile(connection,
                                                 backend_file_name);
    free(backend_file_name);
    print_backend_call_replace_sync(p->backend_proxy, 
                                    previous_parent_dialog, 
                                    NULL, 
                                    &error);
    if (error)
    {
        cpdbDebugLog2("Error replacing resurrected printer", 
                      error->message, 
                      CPDB_DEBUG_LEVEL_ERR);
        fclose(fp);
        return NULL;
    }

    fgets(buf, sizeof(buf), fp);
    p->id = cpdbGetStringCopy(strtok(buf, "#"));

    fgets(buf, sizeof(buf), fp);
    p->name = cpdbGetStringCopy(strtok(buf, "#"));

    fgets(buf, sizeof(buf), fp);
    p->location = cpdbGetStringCopy(strtok(buf, "#"));

    fgets(buf, sizeof(buf), fp);
    p->info = cpdbGetStringCopy(strtok(buf, "#"));

    fgets(buf, sizeof(buf), fp);
    p->make_and_model = cpdbGetStringCopy(strtok(buf, "#"));

    fgets(buf, sizeof(buf), fp);
    p->state = cpdbGetStringCopy(strtok(buf, "#"));

    fscanf(fp, "%d\n", &p->accepting_jobs);
    
    cpdbPrintBasicOptions(p);

    fscanf(fp, "%d\n", &count);
    cpdbDebugLog("Settings: ", CPDB_DEBUG_LEVEL_INFO);
    while (count--)
    {
        fgets(buf, sizeof(buf), fp);
        name = strtok(buf, "#");
        value = strtok(NULL, "#");
        cpdbDebugLog2(name, value, CPDB_DEBUG_LEVEL_INFO);
        cpdbAddSetting(p->settings, name, value);
    }

    fclose(fp);
    return p;
}

char *cpdbGetOptionTranslation(cpdb_printer_obj_t *p,
                               const char *option_name,
                               const char *locale)
{
    char *translation;
    GError *error = NULL;
    
    print_backend_call_get_option_translation_sync(p->backend_proxy,
                                                   p->id,
                                                   option_name,
                                                   locale,
                                                   &translation,
                                                   NULL,
                                                   &error);
    if (error)
    {
        cpdbDebugPrintf(CPDB_DEBUG_LEVEL_ERR,
                        "Error getting translation : %s\n",
                        error->message);
        cpdbDebugPrintf(CPDB_DEBUG_LEVEL_ERR,
                        "option: %s, locale: %s\n",
                        option_name, locale);
        return NULL;
    }
    
    return cpdbGetStringCopy(translation);
}

char *cpdbGetChoiceTranslation(cpdb_printer_obj_t *p,
                               const char *option_name,
                               const char *choice_name,
                               const char *locale)
{
    char *translation;
    GError *error = NULL;
    
    print_backend_call_get_choice_translation_sync(p->backend_proxy,
                                                   p->id,
                                                   option_name,
                                                   choice_name,
                                                   locale,
                                                   &translation,
                                                   NULL,
                                                   &error);
    if (error)
    {
        cpdbDebugPrintf(CPDB_DEBUG_LEVEL_ERR,
                        "Error getting translation : %s\n",
                        error->message);
        cpdbDebugPrintf(CPDB_DEBUG_LEVEL_ERR,
                        "option: %s, choice: %s, locale: %s\n",
                        option_name, choice_name, locale);
        return NULL;
    }
    
    return cpdbGetStringCopy(translation);
}


char *cpdbGetGroupTranslation(cpdb_printer_obj_t *p,
                              const char *group_name,
                              const char *locale)
{
    char *translation;
    GError *error = NULL;
    
    print_backend_call_get_group_translation_sync(p->backend_proxy,
                                                  p->id,
                                                  group_name,
                                                  locale,
                                                  &translation,
                                                  NULL,
                                                  &error);

    if (error)
    {
        cpdbDebugPrintf(CPDB_DEBUG_LEVEL_ERR,
                        "Error getting translation : %s\n",
                        error->message);
        cpdbDebugPrintf(CPDB_DEBUG_LEVEL_ERR,
                        "group: %s, locale: %s\n",
                        group_name, locale);
        return NULL;
    }

    return cpdbGetStringCopy(translation);
}

char *cpdbGetHumanReadableOptionName(cpdb_printer_obj_t *p,
                                     const char *option_name)
{
    char *human_readable_name;
    GError *error = NULL;

    print_backend_call_get_human_readable_option_name_sync(p->backend_proxy, 
                                                           option_name,
                                                           &human_readable_name, 
                                                           NULL, 
                                                           &error);
    if(error)
    {
        cpdbDebugLog2("Error getting human readable option name", 
                      error->message, 
                      CPDB_DEBUG_LEVEL_ERR);
        return cpdbGetStringCopy(option_name);
    }
    
    return human_readable_name;
}

char *cpdbGetHumanReadableChoiceName(cpdb_printer_obj_t *p,
                                     const char *option_name,
                                     const char* choice_name)
{
    char *human_readable_name;
    GError *error = NULL;

    print_backend_call_get_human_readable_choice_name_sync(p->backend_proxy,
                                                           option_name,
                                                           choice_name,
                                                           &human_readable_name,
                                                           NULL,
                                                           &error);
    if(error)
    {
        cpdbDebugLog2("Error getting human readable choice name",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        return cpdbGetStringCopy(choice_name);
    }
    
    return human_readable_name;
}

cpdb_media_t *cpdbGetMedia(cpdb_printer_obj_t *p,
                           const char *media)
{
    cpdbGetAllOptions(p);
    return (cpdb_media_t *) g_hash_table_lookup(p->options->media, media);
}

int cpdbGetMediaSize(cpdb_printer_obj_t *p,
                     const char *media,
                     int *width,
                     int *length)
{    
    cpdb_media_t *m = cpdbGetMedia(p, media);
    if (m)
    {
        *width = m->width;
        *length = m->length;
        return 1;
    }

    return 0;
}

int cpdbGetMediaMargins(cpdb_printer_obj_t *p,
                        const char *media,
                        cpdb_margin_t **margins)
{
    int num_margins = 0;
    cpdb_media_t *m = cpdbGetMedia(p, media);

    if (m)
    {
        num_margins = m->num_margins;
        *margins = m->margins;
    }

    return num_margins;	
}

void acquire_details_cb(PrintBackend *proxy,
                        GAsyncResult *res,
                        gpointer user_data)
{
    cpdb_async_obj_t *a = user_data;
    
    cpdb_printer_obj_t *p = a->p;
    cpdb_async_callback caller_cb = a->caller_cb;
    
    p->options = cpdbGetNewOptions();
    GError *error = NULL;
    int num_options, num_media;
    GVariant *var, *media_var;
    
    print_backend_call_get_all_options_finish (proxy,
                                               &num_options,
                                               &var,
                                               &num_media,
                                               &media_var,
                                               res,
                                               &error);
    if (error)
    {
        cpdbDebugLog2("Error acquiring printer details",
                      error->message,
                      CPDB_DEBUG_LEVEL_ERR);
        caller_cb(p, FALSE, a->user_data);
    }
    else
    {
        cpdbUnpackOptions(num_options, var, num_media, media_var, p->options);
        caller_cb(p, TRUE, a->user_data);
    }
    
    free(a);
}

void cpdbAcquireDetails(cpdb_printer_obj_t *p,
                        cpdb_async_callback caller_cb,
                        void *user_data)
{
    if (p->options)
    {
        caller_cb(p, TRUE, user_data);
        return;
    }
    
    cpdb_async_obj_t *a = malloc(sizeof(cpdb_async_obj_t));
    a->p = p;
    a->caller_cb = caller_cb;
    a->user_data = user_data;
    
    print_backend_call_get_all_options(p->backend_proxy,
                                       p->id, 
                                       NULL,
                                       (GAsyncReadyCallback) acquire_details_cb,
                                       a);
}

/**
________________________________________________ cpdb_settings_t __________________________________________
**/
cpdb_settings_t *cpdbGetNewSettings()
{
    cpdb_settings_t *s = g_new0(cpdb_settings_t, 1);
    s->count = 0;
    s->table = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
    return s;
}

void cpdbCopySettings(const cpdb_settings_t *source,
                      cpdb_settings_t *dest)
{
    if (source == NULL || dest == NULL)
    {
        cpdbDebugLog("Invalid params: cpdbCopySettings()",
                     CPDB_DEBUG_LEVEL_WARN);
        return;
    }

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, source->table);
    gpointer key, value;
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        cpdbAddSetting(dest, (char *)key, (char *)value);
    }
}
void cpdbAddSetting(cpdb_settings_t *s, 
                    const char *name,
                    const char *val)
{
    if (s == NULL || name == NULL) 
    {
        cpdbDebugLog("Invalid params: cpdbAddSettings()",
                     CPDB_DEBUG_LEVEL_WARN);
        return;
    }

    char *prev = g_hash_table_lookup(s->table, name);
    if (prev)
    {
        /**
         * The value is already there, so replace it instead
         */
        g_hash_table_replace(s->table,
                             cpdbGetStringCopy(name),
                             cpdbGetStringCopy(val));
        free(prev);
    }
    else
    {
        g_hash_table_insert(s->table,
                            cpdbGetStringCopy(name),
                            cpdbGetStringCopy(val));
        s->count++;
    }
}

gboolean cpdbClearSetting(cpdb_settings_t *s, const char *name)
{
    if (s == NULL || name == NULL) 
    {
        cpdbDebugLog("Invalid params: cpdbClearSettings()",
                     CPDB_DEBUG_LEVEL_WARN);
        return FALSE;
    }

    if (g_hash_table_contains(s->table, name))
    {
        g_hash_table_remove(s->table, name);
        s->count--;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

GVariant *cpdbSerializeToGVariant(cpdb_settings_t *s)
{
    GVariantBuilder *builder;
    GVariant *variant;
    builder = g_variant_builder_new(G_VARIANT_TYPE("a(ss)"));

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, s->table);

    gpointer key, value;
    for (int i = 0; i < s->count; i++)
    {
        g_hash_table_iter_next(&iter, &key, &value);
        g_variant_builder_add(builder, "(ss)", key, value);
    }

    if (s->count == 0)
        g_variant_builder_add(builder, "(ss)", "NA", "NA");

    variant = g_variant_new("a(ss)", builder);
    return variant;
}

void cpdbSaveSettingsToDisk(cpdb_settings_t *s)
{
    FILE *fp;
    char *conf_dir, *path;
    GHashTableIter iter;
    gpointer key, value;

    if ((conf_dir = cpdbGetUserConfDir()) == NULL)
    {
        cpdbDebugLog2("Error saving settings to disk",
                      "Couldn't obtain user config directory",
                      CPDB_DEBUG_LEVEL_ERR);
        return;
    }
    path = cpdbConcatPath(conf_dir, CPDB_PRINT_SETTINGS_FILE);

    if ((fp = fopen(path, "w")) == NULL)
    {
        cpdbDebugLog("Error saving settings to disk.",
                     CPDB_DEBUG_LEVEL_WARN);
        cpdbDebugLog2("Couldn't open file for writing",
                      path,
                      CPDB_DEBUG_LEVEL_WARN);
        return;
    }
    fprintf(fp, "%d\n", s->count);
    
    g_hash_table_iter_init(&iter, s->table);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        fprintf(fp, "%s#%s#\n", (char *)key, (char *)value);
    }

    fclose(fp);
    free(path);
    free(conf_dir);
}

cpdb_settings_t *cpdbReadSettingsFromDisk()
{
    FILE *fp;
    int count;
    char *name, *value, *conf_dir, *path;
    char buf[CPDB_BSIZE];
    cpdb_settings_t *s;

    if ((conf_dir = cpdbGetUserConfDir()) == NULL)
    {
        cpdbDebugLog2("No previous settings found",
                      "Couldn't obtain user config directory",
                      CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
    path = cpdbConcatPath(conf_dir, CPDB_PRINT_SETTINGS_FILE);

    if ((fp = fopen(path, "r")) == NULL)
    {
        cpdbDebugLog("No previous settings found.",
                     CPDB_DEBUG_LEVEL_WARN);
        cpdbDebugLog2("Couldn't open file for reading:",
                      path,
                      CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    s = cpdbGetNewSettings();
    fscanf(fp, "%d\n", &count);
    
    sprintf(buf, "Retrieved %d settings from disk", count);
    cpdbDebugLog(buf, CPDB_DEBUG_LEVEL_INFO);
                  
    while (count--)
    {
        fgets(buf, sizeof(buf), fp);
        name = strtok(buf, "#");
        value = strtok(NULL, "#");
        cpdbDebugLog2(name,
                      value,
                      CPDB_DEBUG_LEVEL_INFO);
        cpdbAddSetting(s, name, value);
    }

    fclose(fp);
    free(path);
    free(conf_dir);
    return s;
}

void cpdbDeleteSettings(cpdb_settings_t *s)
{
    if (s == NULL)
        return;
    
    if (s->table)
        g_hash_table_destroy(s->table);
    
    free(s);
}
/**
________________________________________________ cpdb_options_t __________________________________________
**/
cpdb_options_t *cpdbGetNewOptions()
{
    cpdb_options_t *o = g_new0(cpdb_options_t, 1);
    o->count = 0;
    o->table = g_hash_table_new_full(g_str_hash,
                                     g_str_equal,
                                     NULL,
                                     (GDestroyNotify) cpdbDeleteOption);
    o->media = g_hash_table_new_full(g_str_hash,
                                     g_str_equal,
                                     NULL,
                                     (GDestroyNotify) cpdbDeleteMedia);
    return o;
}

void cpdbDeleteOptions(cpdb_options_t *opts)
{
    if (opts == NULL)
        return;
    
    if (opts->table)
        g_hash_table_destroy(opts->table);
    if (opts->media)
        g_hash_table_destroy(opts->media);

    free(opts);
}

/**************cpdb_option_t************************************/
void cpdbPrintOption(const cpdb_option_t *opt)
{
    int i;
    
    printf("[+] %s\n", opt->option_name);
    printf(" --> GROUP: %s\n", opt->group_name);
    for (i = 0; i < opt->num_supported; i++)
    {
        printf("   * %s\n", opt->supported_values[i]);
    }
    printf(" --> DEFAULT: %s\n\n", opt->default_value);
}

void cpdbDeleteOption(cpdb_option_t *opt)
{
    if (opt == NULL)
        return;
    
    if (opt->option_name)
        free(opt->option_name);
    if (opt->group_name)
        free(opt->group_name);
    if (opt->supported_values)
        free(opt->supported_values);
    if (opt->default_value)
        free(opt->default_value);

    free(opt);
}

/**************cpdb_option_t************************************/
void cpdbPrintMedia(cpdb_media_t *media)
{
    printf("[+] Media: %s\n", media->name);
    printf("   * width = %d\n", media->width);
    printf("   * length = %d\n", media->length);
    printf(" --> Supported margins: %d\n", media->num_margins);
    printf("       left, right, top, bottom\n");
    for (int i = 0; i < media->num_margins; i++)
    {
        printf("     * %d, %d, %d, %d,\n",
               media->margins[i].left,
               media->margins[i].right,
               media->margins[i].top,
               media->margins[i].bottom);
    }
    printf("\n");
}

void cpdbDeleteMedia(cpdb_media_t *media)
{
    if (media == NULL)
        return;
    
    if (media->name)
        free(media->name);
    if (media->margins)
        free(media->margins);
    
    free(media);
}

/**
 * ________________________________ cpdb_job_t __________________________
 */
void cpdbUnpackJobArray(GVariant *var,
                        int num_jobs,
                        cpdb_job_t *jobs,
                        char *backend_name)
{
    int i;
    char *str;
    GVariantIter *iter;
    g_variant_get(var, CPDB_JOB_ARRAY_ARGS, &iter);
    int size;
    char *jobid, *title, *printer, *user, *state, *submit_time;
    for (i = 0; i < num_jobs; i++)
    {
        g_variant_iter_loop(iter,
                            CPDB_JOB_ARGS,
                            &jobid,
                            &title,
                            &printer,
                            &user,
                            &state,
                            &submit_time,
                            &size);

        jobs[i].job_id = cpdbGetStringCopy(jobid);
        jobs[i].title = cpdbGetStringCopy(title);
        jobs[i].printer_id = cpdbGetStringCopy(printer);
        jobs[i].backend_name = backend_name;
        jobs[i].user = cpdbGetStringCopy(user);
        jobs[i].state = cpdbGetStringCopy(state);
        jobs[i].submitted_at = cpdbGetStringCopy(submit_time);
        jobs[i].size = size;

        //printf("Printer %s ; state %s \n",printer, state);
    }
}
/**
 * ________________________________utility functions__________________________
 */

void cpdbUnpackOptions(int num_options,
                       GVariant *var,
                       int num_media,
                       GVariant *media_var,
                       cpdb_options_t *options)
{
    options->count = num_options;
    int i, j;
    char *str;
    GVariantIter *iter;
    GVariantIter *array_iter;
    char *name, *default_val, *group;
    int num_sup;
    g_variant_get(var, "a(sssia(s))", &iter);
    cpdb_option_t *opt;
    for (i = 0; i < num_options; i++)
    {
        opt = g_new0(cpdb_option_t, 1);
        g_variant_iter_loop(iter,
                            "(sssia(s))",
                            &name,
                            &group,
                            &default_val,
                            &num_sup,
                            &array_iter);
        opt->option_name = cpdbGetStringCopy(name);
        opt->group_name = cpdbGetStringCopy(group);
        opt->default_value = cpdbGetStringCopy(default_val);
        opt->num_supported = num_sup;
        opt->supported_values = cpdbNewCStringArray(num_sup);
        for (j = 0; j < num_sup; j++)
        {
            g_variant_iter_loop(array_iter, "(s)", &str);
            opt->supported_values[j] = cpdbGetStringCopy(str);
        }
        g_hash_table_insert(options->table, (gpointer)opt->option_name, (gpointer)opt);
    }
    
    options->media_count = num_media;
    int width, length, num_mar;
    GVariantIter *media_iter, *margin_iter;
    g_variant_get(media_var, "a(siiia(iiii))", &media_iter);
    cpdb_media_t *media;
    for (i = 0; i < num_media; i++)
    {
		media = g_new0(cpdb_media_t, 1);
		g_variant_iter_loop(media_iter, "(siiia(iiii))",
							&name, &width, &length, &num_mar, &margin_iter);
		media->name = cpdbGetStringCopy(name);
		media->width = width;
		media->length = length;
		media->num_margins = num_mar;
		media->margins = malloc(sizeof(cpdb_printer_obj_t) * num_mar);
		for (j = 0; j < num_mar; j++)
		{
			g_variant_iter_loop(margin_iter, "(iiii)", 
								&media->margins[j].left, 
								&media->margins[j].right, 
								&media->margins[j].top, 
								&media->margins[j].bottom);
		}
		g_hash_table_insert(options->media, (gpointer)media->name, (gpointer) media);
	}
    
}
/************************************************************************************************/
