#include "cpdb-frontend.h"

/**
________________________________________________ cpdb_frontend_obj_t __________________________________________

**/
/**
 * These static functions are callbacks used by the actual cpdb_frontend_obj_t functions.
 */
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
    /** If some previously saved settings were retrieved, use them in this new cpdb_printer_obj_t **/
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

static void
on_name_acquired(GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
    CPDB_DEBUG_LOG("Acquired bus name", "", CPDB_DEBUG_LEVEL_INFO);
    cpdb_frontend_obj_t *f = (cpdb_frontend_obj_t *)user_data;
    f->connection = connection;
    GError *error = NULL;

    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       CPDB_SIGNAL_PRINTER_ADDED,            //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       on_printer_added,                //callback
                                       user_data,                       //user_data
                                       NULL);

    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       CPDB_SIGNAL_PRINTER_REMOVED,          //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       on_printer_removed,              //callback
                                       user_data,                       //user_data
                                       NULL);

    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(f->skeleton), connection, CPDB_DIALOG_OBJ_PATH, &error);
    if (error)
    {
        CPDB_DEBUG_LOG("Error connecting to D-Bus.", error->message, CPDB_DEBUG_LEVEL_ERR);
        return;
    }
    cpdbActivateBackends(f);
}

cpdb_frontend_obj_t *cpdbGetNewFrontendObj(char *instance_name, cpdb_event_callback add_cb, cpdb_event_callback rem_cb)
{
    cpdb_frontend_obj_t *f = malloc(sizeof(cpdb_frontend_obj_t));
    f->skeleton = print_frontend_skeleton_new();
    f->connection = NULL;
    if (!instance_name)
      f->bus_name = cpdbGetStringCopy(CPDB_DIALOG_BUS_NAME);
    else
    {
	char *tmp = malloc(sizeof(char) * (strlen(CPDB_DIALOG_BUS_NAME) + strlen(instance_name) + 1));
        sprintf(tmp, "%s%s", CPDB_DIALOG_BUS_NAME, instance_name);
        f->bus_name = tmp;
    }
    f->add_cb = add_cb;
    f->rem_cb = rem_cb;
    f->num_backends = 0;
    f->backend = g_hash_table_new(g_str_hash, g_str_equal);
    f->num_printers = 0;
    f->printer = g_hash_table_new(g_str_hash, g_str_equal);
    f->last_saved_settings = cpdbReadSettingsFromDisk();
    return f;
}

void cpdbConnectToDBus(cpdb_frontend_obj_t *f)
{
    g_bus_own_name(G_BUS_TYPE_SESSION,
                   f->bus_name,
                   0,                //flags
                   NULL,             //bus_acquired_handler
                   on_name_acquired, //name acquired handler
                   NULL,             //name_lost handler
                   f,                //user_data
                   NULL);            //user_data free function
}

void cpdbDisconnectFromDBus(cpdb_frontend_obj_t *f)
{
    print_frontend_emit_stop_listing(f->skeleton);
    g_dbus_connection_flush_sync(f->connection, NULL, NULL);
    g_dbus_connection_close_sync(f->connection, NULL, NULL);
}

void cpdbActivateBackends(cpdb_frontend_obj_t *f)
{
    DIR *d;
    struct dirent *dir;
    d = opendir(CPDB_DBUS_DIR);
    int len = strlen(CPDB_BACKEND_PREFIX);
    PrintBackend *proxy;

    char *backend_suffix;
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (strncmp(CPDB_BACKEND_PREFIX, dir->d_name, len) == 0)
            {
                backend_suffix = cpdbGetStringCopy((dir->d_name) + len);

		char *msg = malloc(sizeof(char) * (strlen(backend_suffix) + 20));
		sprintf(msg, "Found backend %s", backend_suffix);
                CPDB_DEBUG_LOG(msg, "", CPDB_DEBUG_LEVEL_INFO);
		free(msg);

                proxy = cpdbCreateBackendFromFile(dir->d_name);

                g_hash_table_insert(f->backend, backend_suffix, proxy);
                f->num_backends++;

                print_backend_call_activate_backend(proxy, NULL, NULL, NULL);
            }
        }

        closedir(d);
    }
}

PrintBackend *cpdbCreateBackendFromFile(const char *backend_file_name)
{
    PrintBackend *proxy;
    char *backend_name = cpdbGetStringCopy(backend_file_name);

    char *path = malloc(sizeof(char) * (strlen(CPDB_DBUS_DIR) + strlen(backend_file_name) + 2));
    sprintf(path, "%s/%s", CPDB_DBUS_DIR, backend_file_name);

    FILE *file = fopen(path, "r");
    char obj_path[200];
    fscanf(file, "%s", obj_path);
    fclose(file);
    free(path);
    GError *error = NULL;
    proxy = print_backend_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, 0,
                                                 backend_name, obj_path, NULL, &error);

    if (error)
    {
        char *msg = malloc(sizeof(char) * (strlen(backend_name) + 40));
        sprintf(msg, "Error creating backend proxy for %s", backend_name);
        CPDB_DEBUG_LOG(msg, error->message, CPDB_DEBUG_LEVEL_ERR);
	free(msg);
    }
    return proxy;
}

void cpdbIgnoreLastSavedSettings(cpdb_frontend_obj_t *f)
{
    CPDB_DEBUG_LOG("Ignoring previous settings", "", CPDB_DEBUG_LEVEL_INFO);
    cpdb_settings_t *s = f->last_saved_settings;
    f->last_saved_settings = NULL;
    cpdbDeleteSettings(s);
}

gboolean cpdbAddPrinter(cpdb_frontend_obj_t *f, cpdb_printer_obj_t *p)
{
    p->backend_proxy = g_hash_table_lookup(f->backend, p->backend_name);

    if (p->backend_proxy == NULL)
    {
        char *msg = malloc(sizeof(char) * (strlen(p->backend_name) + 60));
        sprintf(msg, "Can't add printer. Backend %s doesn't exist", p->backend_name);
        CPDB_DEBUG_LOG(msg, "", CPDB_DEBUG_LEVEL_ERR);
	free(msg);
    }

    g_hash_table_insert(f->printer, cpdbConcat(p->id, p->backend_name), p);
    f->num_printers++;
    return TRUE;
}

cpdb_printer_obj_t *cpdbRemovePrinter(cpdb_frontend_obj_t *f, const char *printer_id, const char *backend_name)
{
    char *key = cpdbConcat(printer_id, backend_name);
    if (g_hash_table_contains(f->printer, key))
    {
        cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
        g_hash_table_remove(f->printer, key);
        f->num_printers--;
        free(key);
        return p;
    }
    
    free(key);
    return NULL;
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

cpdb_printer_obj_t *cpdbFindPrinterObj(cpdb_frontend_obj_t *f, const char *printer_id, const char *backend_name)
{
    char *hashtable_key = malloc(sizeof(char) * (strlen(printer_id) + strlen(backend_name) + 2));
    sprintf(hashtable_key, "%s#%s", printer_id, backend_name);

    cpdb_printer_obj_t *p = g_hash_table_lookup(f->printer, hashtable_key);
    if (p == NULL)
    {
        CPDB_DEBUG_LOG("Printer doesn't exist.\n", "", CPDB_DEBUG_LEVEL_ERR);
    }
    free(hashtable_key);
    return p;
}

char *cpdbGetDefaultPrinter(cpdb_frontend_obj_t *f, const char *backend_name)
{
    PrintBackend *proxy = g_hash_table_lookup(f->backend, backend_name);
    if (!proxy)
    {
        proxy = cpdbCreateBackendFromFile(backend_name);
    }
    g_assert_nonnull(proxy);
    char *def;
    print_backend_call_get_default_printer_sync(proxy, &def, NULL, NULL);
    return def;
}

int cpdbGetAllJobs(cpdb_frontend_obj_t *f, cpdb_job_t **j, gboolean active_only)
{
    
	/**inititalizing the arrays required for each of the backends **/
	
    /** num_jobs[] stores the number of jobs for each of the backends**/
    int *num_jobs = g_new(int, f->num_backends);
    
    /**backend_names[] stores the name for each of the backends**/
    char **backend_names = g_new0(char *, f->num_backends);
    
    /**retval[] stores the gvariant returned by the respective backend **/
    GVariant **retval = g_new(GVariant *, f->num_backends);
    
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, f->backend);
    int i = 0;
    int total_jobs = 0;
    
    gpointer key, value;
 
    /** Iterating over all the backends and getting each's active jobs**/
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        
        PrintBackend *proxy = (PrintBackend *)value;
        
        backend_names[i] = (char *)key;
        printf("Trying to get jobs for backend %s\n", backend_names[i]);
        
        GError *error = NULL;
        print_backend_call_get_all_jobs_sync(proxy, active_only, &(num_jobs[i]), &(retval[i]), NULL, &error);
        
        if(error)
        {
        	printf("cpdbGetAllJobs failed\n");
        	num_jobs[i] = 0;
        	
        }
        else
        {
        	printf("Call succeeded\n");
        }
        
        total_jobs += num_jobs[i];
        i++; /** off to the next backend **/
    }
    
    cpdb_job_t *jobs = g_new(cpdb_job_t, total_jobs);
    int n = 0;

    for (i = 0; i < f->num_backends; i++)
    {
    	if(num_jobs[i])
    		cpdbUnpackJobArray(retval[i], num_jobs[i], jobs + n, backend_names[i]);
        n += num_jobs[i];
    }

    *j = jobs;
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

void cpdbFillBasicOptions(cpdb_printer_obj_t *p, GVariant *gv)
{
    g_variant_get(gv, CPDB_PRINTER_ADDED_ARGS,
                  &(p->id),
                  &(p->name),
                  &(p->info),
                  &(p->location),
                  &(p->make_and_model),
                  &(p->cpdbIsAcceptingJobs),
                  &(p->state),
                  &(p->backend_name));
}

void cpdbPrintBasicOptions(cpdb_printer_obj_t *p)
{
    g_message(" Printer %s\n\
                name : %s\n\
                location : %s\n\
                info : %s\n\
                make and model : %s\n\
                accepting_jobs : %d\n\
                state : %s\n\
                backend: %s\n ",
              p->id,
              p->name,
              p->location,
              p->info,
              p->make_and_model,
              p->cpdbIsAcceptingJobs,
              p->state,
              p->backend_name);
}

gboolean cpdbIsAcceptingJobs(cpdb_printer_obj_t *p)
{
    GError *error = NULL;
    print_backend_call_is_accepting_jobs_sync(p->backend_proxy, p->id,
                                              &p->cpdbIsAcceptingJobs, NULL, &error);
    if (error)
        CPDB_DEBUG_LOG("Error retrieving accepting_jobs.", error->message, CPDB_DEBUG_LEVEL_ERR);

    return p->cpdbIsAcceptingJobs;
}

char *cpdbGetState(cpdb_printer_obj_t *p)
{
    GError *error = NULL;
    print_backend_call_get_printer_state_sync(p->backend_proxy, p->id, &p->state, NULL, &error);

    if (error)
        CPDB_DEBUG_LOG("Error retrieving printer state.", error->message, CPDB_DEBUG_LEVEL_ERR);

    return p->state;
}

cpdb_options_t *cpdbGetAllOptions(cpdb_printer_obj_t *p)
{
    /** 
     * If the options were previously queried, 
     * return them, instead of querying again.
    */
    if (p->options)
        return p->options;

    p->options = cpdbGetNewOptions();
    GError *error = NULL;
    int num_options;
    GVariant *var;
    print_backend_call_get_all_options_sync(p->backend_proxy, p->id,
                                            &num_options, &var, NULL, &error);
    if (!error)
    {
        cpdbUnpackOptions(var, num_options, p->options);
        return p->options;
    }
    else 
    {
        CPDB_DEBUG_LOG("Error retrieving printer options", error->message, CPDB_DEBUG_LEVEL_ERR);
        return NULL;
    }
}

cpdb_option_t *cpdbGetOption(cpdb_printer_obj_t *p, const char *name)
{
    cpdbGetAllOptions(p);
    if (!g_hash_table_contains(p->options->table, name))
        return NULL;
    return (cpdb_option_t *)(g_hash_table_lookup(p->options->table, name));
}

char *cpdbGetDefault(cpdb_printer_obj_t *p, const char *name)
{
    cpdb_option_t *o = cpdbGetOption(p, name);
    if (!o)
        return NULL;
    return o->default_value;
}

char *cpdbGetSetting(cpdb_printer_obj_t *p, const char *name)
{
    if (!g_hash_table_contains(p->settings->table, name))
        return NULL;
    return g_hash_table_lookup(p->settings->table, name);
}

char *cpdbGetCurrent(cpdb_printer_obj_t *p, const char *name)
{
    char *set = cpdbGetSetting(p, name);
    if (set)
        return set;

    return cpdbGetDefault(p, name);
}

int cpdbGetActiveJobsCount(cpdb_printer_obj_t *p)
{
    int count;
    print_backend_call_get_active_jobs_count_sync(p->backend_proxy, p->id, &count, NULL, NULL);
    return count;
}
char *cpdbPrintFile(cpdb_printer_obj_t *p, const char *file_path)
{
    char *jobid;
    char *absolute_file_path = cpdbGetAbsolutePath(file_path);
    print_backend_call_print_file_sync(p->backend_proxy, p->id, absolute_file_path,
                                       p->settings->count,
                                       cpdbSerializeToGVariant(p->settings),
                                       "final-file-path-not-required",
                                       &jobid, NULL, NULL);
    free(absolute_file_path);
    if (jobid && jobid[0] != '0')
        CPDB_DEBUG_LOG("File printed successfully.\n", "", CPDB_DEBUG_LEVEL_INFO);
    else
        CPDB_DEBUG_LOG("Error printing file.\n", "", CPDB_DEBUG_LEVEL_ERR);

    cpdbSaveSettingsToDisk(p->settings);
    return jobid;
}
char *cpdbPrintFilePath(cpdb_printer_obj_t *p, const char *file_path, const char *final_file_path)
{
    char *result;
    char *absolute_file_path = cpdbGetAbsolutePath(file_path);
    char *absolute_final_file_path = cpdbGetAbsolutePath(final_file_path);
    print_backend_call_print_file_sync(p->backend_proxy, p->id, absolute_file_path,
                                       p->settings->count,
                                       cpdbSerializeToGVariant(p->settings),
                                       absolute_final_file_path,
                                       &result, NULL, NULL);
    free(absolute_file_path);
    free(absolute_final_file_path);
    if (result)
        CPDB_DEBUG_LOG("File printed successfully.\n", "", CPDB_DEBUG_LEVEL_INFO);
    else
        CPDB_DEBUG_LOG("Error printing file.\n", "", CPDB_DEBUG_LEVEL_ERR);

    return result;
}
void cpdbAddSettingToPrinter(cpdb_printer_obj_t *p, const char *name, const char *val)
{
    cpdbAddSetting(p->settings, name, val);
}
gboolean cpdbClearSettingFromPrinter(cpdb_printer_obj_t *p, const char *name)
{
    cpdbClearSetting(p->settings, name);
}
gboolean cpdbCancelJob(cpdb_printer_obj_t *p, const char *job_id)
{
    gboolean status;
    print_backend_call_cancel_job_sync(p->backend_proxy, job_id, p->id,
                                       &status, NULL, NULL);
    return status;
}
void cpdbPicklePrinterToFile(cpdb_printer_obj_t *p, const char *filename, const cpdb_frontend_obj_t *parent_dialog)
{

    print_backend_call_keep_alive_sync(p->backend_proxy, NULL, NULL);
    char *path = cpdbGetAbsolutePath(filename);
    FILE *fp = fopen(path, "w");

    const char *unique_bus_name = g_dbus_connection_get_unique_name(parent_dialog->connection);
    fprintf(fp, "%s#\n", unique_bus_name);
    fprintf(fp, "%s#\n", p->backend_name);
    fprintf(fp, "%s#\n", p->id);
    fprintf(fp, "%s#\n", p->name);
    fprintf(fp, "%s#\n", p->location);
    fprintf(fp, "%s#\n", p->info);
    fprintf(fp, "%s#\n", p->make_and_model);
    fprintf(fp, "%s#\n", p->state);
    fprintf(fp, "%d\n", p->cpdbIsAcceptingJobs);

    /** Not pickling the cpdb_options_t because it can be reconstructed by querying the backend */

    fprintf(fp, "%d\n", p->settings->count);
    GHashTableIter iter;
    gpointer key, value;
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

    char *path = cpdbGetAbsolutePath(filename);
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        free(path);
        return NULL;
    }

    cpdb_printer_obj_t *p = cpdbGetNewPrinterObj();

    char line[1024];

    fgets(line, 1024, fp);
    char *previous_parent_dialog = cpdbGetStringCopy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->backend_name = cpdbGetStringCopy(strtok(line, "#"));
    char *backend_file_name = malloc(sizeof(char) * (strlen(CPDB_BACKEND_PREFIX) + strlen(p->backend_name) + 1));
    sprintf(backend_file_name, "%s%s", CPDB_BACKEND_PREFIX, p->backend_name);
    p->backend_proxy = cpdbCreateBackendFromFile(backend_file_name);
    free(backend_file_name);
    print_backend_call_replace_sync(p->backend_proxy, previous_parent_dialog, NULL, NULL);

    fgets(line, 1024, fp);
    p->id = cpdbGetStringCopy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->name = cpdbGetStringCopy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->location = cpdbGetStringCopy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->info = cpdbGetStringCopy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->make_and_model = cpdbGetStringCopy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->state = cpdbGetStringCopy(strtok(line, "#"));

    fscanf(fp, "%d\n", &p->cpdbIsAcceptingJobs);

    int count;
    fscanf(fp, "%d\n", &count);

    char *name, *value;
    while (count--)
    {
        fgets(line, 1024, fp);
        name = strtok(line, "#");
        value = strtok(NULL, "#");
        printf("%s  : %s \n", name, value);
        cpdbAddSetting(p->settings, name, value);
    }

    fclose(fp);
    free(path);

    return p;
}

char *cpdbGetHumanReadableOptionName(cpdb_printer_obj_t *p, const char *option_name)
{
    char *human_readable_name;
    GError *error = NULL;
    print_backend_call_get_human_readable_option_name_sync(p->backend_proxy, option_name,
                                                           &human_readable_name, NULL, &error);
    if(error) {
        CPDB_DEBUG_LOG("Error getting human readable option name", error->message, CPDB_DEBUG_LEVEL_ERR);
        return cpdbGetStringCopy(option_name);
    } else {
        return human_readable_name;
    }
}

char *cpdbGetHumanReadableChoiceName(cpdb_printer_obj_t *p, const char *option_name, const char* choice_name)
{
    char *human_readable_name;
    GError *error = NULL;
    print_backend_call_get_human_readable_choice_name_sync(p->backend_proxy, option_name, choice_name,
                                                           &human_readable_name, NULL, &error);
    if(error) {
        CPDB_DEBUG_LOG("Error getting human readable choice name", error->message, CPDB_DEBUG_LEVEL_ERR);
        return cpdbGetStringCopy(choice_name);
    } else {
        return human_readable_name;
    }
}

void cpdbGetMediaSize(cpdb_printer_obj_t *p, const char *media_size, int *width, int *length)
{
    GError *error = NULL;
    GVariant *var;
    print_backend_call_get_media_size_sync(p->backend_proxy, media_size,
                                            &var, NULL, &error);
    if (!error)
        g_variant_get(var, "(ii)", width, length);
    else 
    {
        CPDB_DEBUG_LOG("Error getting media size", error->message, CPDB_DEBUG_LEVEL_ERR);
    }
}

void acquire_details_cb(PrintBackend *proxy, GAsyncResult *res, gpointer user_data)
{
    cpdb_async_obj_t *a = user_data;
    
    cpdb_printer_obj_t *p = a->p;
    cpdb_async_callback caller_cb = a->caller_cb;
    
    p->options = cpdbGetNewOptions();
    GError *error = NULL;
    int num_options;
    GVariant *var;
    
    print_backend_call_get_all_options_finish (proxy, &num_options, &var, res, &error);
    
    if (!error)
    {
        cpdbUnpackOptions(var, num_options, p->options);
        caller_cb(p, TRUE, a->user_data);
    }
    else
    {
        CPDB_DEBUG_LOG("Error acquiring printer details", error->message, CPDB_DEBUG_LEVEL_ERR);
        caller_cb(p, FALSE, a->user_data);
    }
    
    free(a);
}

void cpdbAcquireDetails(cpdb_printer_obj_t *p, cpdb_async_callback caller_cb, void *user_data)
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
    
    print_backend_call_get_all_options(p->backend_proxy, p->id, 
                                        NULL, (GAsyncReadyCallback) acquire_details_cb, a);
}

/**
________________________________________________ cpdb_settings_t __________________________________________
**/
cpdb_settings_t *cpdbGetNewSettings()
{
    cpdb_settings_t *s = g_new0(cpdb_settings_t, 1);
    s->count = 0;
    s->table = g_hash_table_new(g_str_hash, g_str_equal);
    return s;
}

void cpdbCopySettings(const cpdb_settings_t *source, cpdb_settings_t *dest)
{
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, source->table);
    gpointer key, value;
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        cpdbAddSetting(dest, (char *)key, (char *)value);
    }
}
void cpdbAddSetting(cpdb_settings_t *s, const char *name, const char *val)
{
    char *prev = g_hash_table_lookup(s->table, name);
    if (prev)
    {
        /**
         * The value is already there, so replace it instead
         */
        g_hash_table_replace(s->table, cpdbGetStringCopy(name), cpdbGetStringCopy(val));
        free(prev);
    }
    else
    {
        g_hash_table_insert(s->table, cpdbGetStringCopy(name), cpdbGetStringCopy(val));
        s->count++;
    }
}

gboolean cpdbClearSetting(cpdb_settings_t *s, const char *name)
{
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
    char *path = cpdbGetAbsolutePath(CPDB_SETTINGS_FILE);
    FILE *fp = fopen(path, "w");

    fprintf(fp, "%d\n", s->count);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, s->table);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        fprintf(fp, "%s#%s#\n", (char *)key, (char *)value);
    }
    fclose(fp);
    free(path);
}

cpdb_settings_t *cpdbReadSettingsFromDisk()
{
    char *path = cpdbGetAbsolutePath(CPDB_SETTINGS_FILE);
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        CPDB_DEBUG_LOG("No previous settings found.", "", CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }
    cpdb_settings_t *s = cpdbGetNewSettings();
    int count;
    fscanf(fp, "%d\n", &count);

    printf("Retrieved %d settings from disk.\n", count);
    char line[1024];

    char *name, *value;
    while (count--)
    {
        fgets(line, 1024, fp);
        name = strtok(line, "#");
        value = strtok(NULL, "#");
        printf("%s  : %s \n", name, value);
        cpdbAddSetting(s, name, value);
    }
    fclose(fp);
    free(path);
    return s;
}

void cpdbDeleteSettings(cpdb_settings_t *s)
{
    GHashTable *h = s->table;
    free(s);
    g_hash_table_destroy(h);
}
/**
________________________________________________ cpdb_options_t __________________________________________
**/
cpdb_options_t *cpdbGetNewOptions()
{
    cpdb_options_t *o = g_new0(cpdb_options_t, 1);
    o->count = 0;
    o->table = g_hash_table_new(g_str_hash, g_str_equal);
    return o;
}

/**************cpdb_option_t************************************/
void cpdbPrintOption(const cpdb_option_t *opt)
{
    gboolean ismedia = FALSE;
    if (strcmp(opt->option_name, "media") == 0)
        ismedia = TRUE;

    g_message("%s", opt->option_name);
    int i;
    for (i = 0; i < opt->num_supported; i++)
    {
        printf(" %s\n", opt->supported_values[i]);
    }
    printf("****DEFAULT: %s\n", opt->default_value);
}

/**
 * ________________________________ cpdb_job_t __________________________
 */
void cpdbUnpackJobArray(GVariant *var, int num_jobs, cpdb_job_t *jobs, char *backend_name)
{
    int i;
    char *str;
    GVariantIter *iter;
    g_variant_get(var, CPDB_JOB_ARRAY_ARGS, &iter);
    int size;
    char *jobid, *title, *printer, *user, *state, *submit_time;
    for (i = 0; i < num_jobs; i++)
    {
        g_variant_iter_loop(iter, CPDB_JOB_ARGS, &jobid, &title, &printer, &user, &state, &submit_time, &size);
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

void CPDB_DEBUG_LOG(const char *msg, const char *error, int msg_level)
{
    if (CPDB_DEBUG_LEVEL >= msg_level)
    {
        if (strlen(error) == 0) printf("%s\n", msg);
        else printf("%s: %s\n", msg, error);
        fflush(stdout);
    }
}
char *cpdbConcat(const char *printer_id, const char *backend_name)
{
    char *str = malloc(sizeof(char) * (strlen(printer_id) + strlen(backend_name) + 2));
    sprintf(str, "%s#%s", printer_id, backend_name);
    return str;
}

void cpdbUnpackOptions(GVariant *var, int num_options, cpdb_options_t *options)
{
    options->count = num_options;
    int i, j;
    char *str;
    GVariantIter *iter;
    GVariantIter *array_iter;
    char *name, *default_val;
    int num_sup;
    g_variant_get(var, "a(ssia(s))", &iter);
    cpdb_option_t *opt;
    for (i = 0; i < num_options; i++)
    {
        opt = g_new0(cpdb_option_t, 1);
        g_variant_iter_loop(iter, "(ssia(s))", &name, &default_val,
                            &num_sup, &array_iter);
        opt->option_name = cpdbGetStringCopy(name);
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
}
/************************************************************************************************/
