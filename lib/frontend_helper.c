#include "frontend_helper.h"

/**
________________________________________________ FrontendObj __________________________________________

**/
/**
 * These static functions are callbacks used by the actual FrontendObj functions.
 */
static void on_printer_added(GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{
    FrontendObj *f = (FrontendObj *)user_data;
    PrinterObj *p = get_new_PrinterObj();
    /** If some previously saved settings were retrieved, use them in this new PrinterObj **/
    if (f->last_saved_settings != NULL)
    {
        copy_settings(f->last_saved_settings, p->settings);
    }
    fill_basic_options(p, parameters);
    add_printer(f, p);
    f->add_cb(f, p);
}

static void on_printer_removed(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer user_data)
{
    FrontendObj *f = (FrontendObj *)user_data;
    char *printer_id;
    char *backend_name;
    g_variant_get(parameters, "(ss)", &printer_id, &backend_name);
    PrinterObj *p = remove_printer(f, printer_id, backend_name);
    f->rem_cb(f, p);
}

static void
on_name_acquired(GDBusConnection *connection,
                 const gchar *name,
                 gpointer user_data)
{
    DBG_LOG("Acquired bus name", INFO);
    FrontendObj *f = (FrontendObj *)user_data;
    f->connection = connection;
    GError *error = NULL;

    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       PRINTER_ADDED_SIGNAL,            //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       on_printer_added,                //callback
                                       user_data,                       //user_data
                                       NULL);

    g_dbus_connection_signal_subscribe(connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       PRINTER_REMOVED_SIGNAL,          //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       on_printer_removed,              //callback
                                       user_data,                       //user_data
                                       NULL);

    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(f->skeleton), connection, DIALOG_OBJ_PATH, &error);
    if (error)
    {
        DBG_LOG("Error connecting to D-Bus.", ERR);
        return;
    }
    activate_backends(f);
}

FrontendObj *get_new_FrontendObj(char *instance_name, event_callback add_cb, event_callback rem_cb)
{
    FrontendObj *f = malloc(sizeof(FrontendObj));
    f->skeleton = print_frontend_skeleton_new();
    f->connection = NULL;
    if (!instance_name)
      f->bus_name = get_string_copy(DIALOG_BUS_NAME);
    else
    {
	char *tmp = malloc(sizeof(char) * (strlen(DIALOG_BUS_NAME) + strlen(instance_name) + 1));
        sprintf(tmp, "%s%s", DIALOG_BUS_NAME, instance_name);
        f->bus_name = tmp;
    }
    f->add_cb = add_cb;
    f->rem_cb = rem_cb;
    f->num_backends = 0;
    f->backend = g_hash_table_new(g_str_hash, g_str_equal);
    f->num_printers = 0;
    f->printer = g_hash_table_new(g_str_hash, g_str_equal);
    f->last_saved_settings = read_settings_from_disk();
    return f;
}

void connect_to_dbus(FrontendObj *f)
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

void disconnect_from_dbus(FrontendObj *f)
{
    print_frontend_emit_stop_listing(f->skeleton);
    g_dbus_connection_flush_sync(f->connection, NULL, NULL);
    g_dbus_connection_close_sync(f->connection, NULL, NULL);
}

void activate_backends(FrontendObj *f)
{
    DIR *d;
    struct dirent *dir;
    d = opendir(DBUS_DIR);
    int len = strlen(BACKEND_PREFIX);
    PrintBackend *proxy;

    char *backend_suffix;
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (strncmp(BACKEND_PREFIX, dir->d_name, len) == 0)
            {
                backend_suffix = get_string_copy((dir->d_name) + len);

		char *msg = malloc(sizeof(char) * (strlen(backend_suffix) + 20));
		sprintf(msg, "Found backend %s", backend_suffix);
                DBG_LOG(msg, INFO);
		free(msg);

                proxy = create_backend_from_file(dir->d_name);

                g_hash_table_insert(f->backend, backend_suffix, proxy);
                f->num_backends++;

                print_backend_call_activate_backend(proxy, NULL, NULL, NULL);
            }
        }

        closedir(d);
    }
}

PrintBackend *create_backend_from_file(const char *backend_file_name)
{
    PrintBackend *proxy;
    char *backend_name = get_string_copy(backend_file_name);

    char *path = malloc(sizeof(char) * (strlen(DBUS_DIR) + strlen(backend_file_name) + 2));
    sprintf(path, "%s/%s", DBUS_DIR, backend_file_name);

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
        DBG_LOG(msg, ERR);
	free(msg);
    }
    return proxy;
}

void ignore_last_saved_settings(FrontendObj *f)
{
    DBG_LOG("Ignoring previous settings", INFO);
    Settings *s = f->last_saved_settings;
    f->last_saved_settings = NULL;
    delete_Settings(s);
}

gboolean add_printer(FrontendObj *f, PrinterObj *p)
{
    p->backend_proxy = g_hash_table_lookup(f->backend, p->backend_name);

    if (p->backend_proxy == NULL)
    {
        char *msg = malloc(sizeof(char) * (strlen(p->backend_name) + 60));
        sprintf(msg, "Can't add printer. Backend %s doesn't exist", p->backend_name);
        DBG_LOG(msg, ERR);
	free(msg);
    }

    g_hash_table_insert(f->printer, concat(p->id, p->backend_name), p);
    f->num_printers++;
    return TRUE;
}

PrinterObj *remove_printer(FrontendObj *f, char *printer_id, char *backend_name)
{
    char *key = concat(printer_id, backend_name);
    if (g_hash_table_contains(f->printer, key))
    {
        PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
        g_hash_table_remove(f->printer, key);
        f->num_printers--;
        return p;
    }
    return NULL;
}

void refresh_printer_list(FrontendObj *f)
{
    print_frontend_emit_refresh_backend(f->skeleton);
}

void hide_remote_cups_printers(FrontendObj *f)
{
    print_frontend_emit_hide_remote_printers_cups(f->skeleton);
}

void unhide_remote_cups_printers(FrontendObj *f)
{
    print_frontend_emit_unhide_remote_printers_cups(f->skeleton);
}

void hide_temporary_cups_printers(FrontendObj *f)
{
    print_frontend_emit_hide_temporary_printers_cups(f->skeleton);
}

void unhide_temporary_cups_printers(FrontendObj *f)
{
    print_frontend_emit_unhide_temporary_printers_cups(f->skeleton);
}

PrinterObj *find_PrinterObj(FrontendObj *f, char *printer_id, char *backend_name)
{
    char *hashtable_key = malloc(sizeof(char) * (strlen(printer_id) + strlen(backend_name) + 2));
    sprintf(hashtable_key, "%s#%s", printer_id, backend_name);

    PrinterObj *p = g_hash_table_lookup(f->printer, hashtable_key);
    if (p == NULL)
    {
        DBG_LOG("Printer doesn't exist.\n", ERR);
    }
    free(hashtable_key);
    return p;
}

char *get_default_printer(FrontendObj *f, char *backend_name)
{
    PrintBackend *proxy = g_hash_table_lookup(f->backend, backend_name);
    if (!proxy)
    {
        proxy = create_backend_from_file(backend_name);
    }
    g_assert_nonnull(proxy);
    char *def;
    print_backend_call_get_default_printer_sync(proxy, &def, NULL, NULL);
    return def;
}

int get_all_jobs(FrontendObj *f, Job **j, gboolean active_only)
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
        	printf("get_all_jobs failed\n");
        	num_jobs[i] = 0;
        	
        }
        else
        {
        	printf("Call succeeded\n");
        }
        
        total_jobs += num_jobs[i];
        i++; /** off to the next backend **/
    }
    
    Job *jobs = g_new(Job, total_jobs);
    int n = 0;

    for (i = 0; i < f->num_backends; i++)
    {
    	if(num_jobs[i])
    		unpack_job_array(retval[i], num_jobs[i], jobs + n, backend_names[i]);
        n += num_jobs[i];
    }

    *j = jobs;
    return total_jobs;
}

/**
________________________________________________ PrinterObj __________________________________________
**/

PrinterObj *get_new_PrinterObj()
{
    PrinterObj *p = malloc(sizeof(PrinterObj));
    p->options = NULL;
    p->settings = get_new_Settings();
    return p;
}

void fill_basic_options(PrinterObj *p, GVariant *gv)
{
    g_variant_get(gv, PRINTER_ADDED_ARGS,
                  &(p->id),
                  &(p->name),
                  &(p->info),
                  &(p->location),
                  &(p->make_and_model),
                  &(p->is_accepting_jobs),
                  &(p->state),
                  &(p->backend_name));
}

void print_basic_options(PrinterObj *p)
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
              p->is_accepting_jobs,
              p->state,
              p->backend_name);
}

gboolean is_accepting_jobs(PrinterObj *p)
{
    GError *error = NULL;
    print_backend_call_is_accepting_jobs_sync(p->backend_proxy, p->id,
                                              &p->is_accepting_jobs, NULL, &error);
    if (error)
        DBG_LOG("Error retrieving accepting_jobs.", ERR);

    return p->is_accepting_jobs;
}

char *get_state(PrinterObj *p)
{
    GError *error = NULL;
    print_backend_call_get_printer_state_sync(p->backend_proxy, p->id, &p->state, NULL, &error);

    if (error)
        DBG_LOG("Error retrieving printer state.", ERR);

    return p->state;
}

Options *get_all_options(PrinterObj *p)
{
    /** 
     * If the options were previously queried, 
     * return them, instead of querying again.
    */
    if (p->options)
        return p->options;

    p->options = get_new_Options();
    GError *error = NULL;
    int num_options;
    GVariant *var;
    print_backend_call_get_all_options_sync(p->backend_proxy, p->id,
                                            &num_options, &var, NULL, &error);
    unpack_options(var, num_options, p->options);
    return p->options;
}

Option *get_Option(PrinterObj *p, char *name)
{
    get_all_options(p);
    if (!g_hash_table_contains(p->options->table, name))
        return NULL;
    return (Option *)(g_hash_table_lookup(p->options->table, name));
}

char *get_default(PrinterObj *p, char *name)
{
    Option *o = get_Option(p, name);
    if (!o)
        return NULL;
    return o->default_value;
}

char *get_setting(PrinterObj *p, char *name)
{
    if (!g_hash_table_contains(p->settings->table, name))
        return NULL;
    return g_hash_table_lookup(p->settings->table, name);
}

char *get_current(PrinterObj *p, char *name)
{
    char *set = get_setting(p, name);
    if (set)
        return set;

    return get_default(p, name);
}

int get_active_jobs_count(PrinterObj *p)
{
    int count;
    print_backend_call_get_active_jobs_count_sync(p->backend_proxy, p->id, &count, NULL, NULL);
    return count;
}
char *print_file(PrinterObj *p, char *file_path)
{
    char *jobid;
    char *absolute_file_path = get_absolute_path(file_path);
    print_backend_call_print_file_sync(p->backend_proxy, p->id, absolute_file_path,
                                       p->settings->count,
                                       serialize_to_gvariant(p->settings),
                                       "final-file-path-not-required",
                                       &jobid, NULL, NULL);
    free(absolute_file_path);
    if (jobid && jobid[0] != '0')
        DBG_LOG("File printed successfully.\n", INFO);
    else
        DBG_LOG("Error printing file.\n", ERR);

    save_to_disk(p->settings);
    return jobid;
}
char *print_file_path(PrinterObj *p, char *file_path, char *final_file_path)
{
    char *result;
    char *absolute_file_path = get_absolute_path(file_path);
    char *absolute_final_file_path = get_absolute_path(final_file_path);
    print_backend_call_print_file_sync(p->backend_proxy, p->id, absolute_file_path,
                                       p->settings->count,
                                       serialize_to_gvariant(p->settings),
                                       absolute_final_file_path,
                                       &result, NULL, NULL);
    free(absolute_file_path);
    free(absolute_final_file_path);
    if (result)
        DBG_LOG("File printed successfully.\n", INFO);
    else
        DBG_LOG("Error printing file.\n", ERR);

    return result;
}
void add_setting_to_printer(PrinterObj *p, char *name, char *val)
{
    add_setting(p->settings, name, val);
}
gboolean clear_setting_from_printer(PrinterObj *p, char *name)
{
    clear_setting(p->settings, name);
}
gboolean cancel_job(PrinterObj *p, char *job_id)
{
    gboolean status;
    print_backend_call_cancel_job_sync(p->backend_proxy, job_id, p->id,
                                       &status, NULL, NULL);
    return status;
}
void pickle_printer_to_file(PrinterObj *p, const char *filename, const FrontendObj *parent_dialog)
{

    print_backend_call_keep_alive_sync(p->backend_proxy, NULL, NULL);
    char *path = get_absolute_path(filename);
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
    fprintf(fp, "%d\n", p->is_accepting_jobs);

    /** Not pickling the Options because it can be reconstructed by querying the backend */

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
PrinterObj *resurrect_printer_from_file(const char *filename)
{

    char *path = get_absolute_path(filename);
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        free(path);
        return NULL;
    }

    PrinterObj *p = get_new_PrinterObj();

    char line[1024];

    fgets(line, 1024, fp);
    char *previous_parent_dialog = get_string_copy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->backend_name = get_string_copy(strtok(line, "#"));
    char *backend_file_name = malloc(sizeof(char) * (strlen(BACKEND_PREFIX) + strlen(p->backend_name) + 1));
    sprintf(backend_file_name, "%s%s", BACKEND_PREFIX, p->backend_name);
    p->backend_proxy = create_backend_from_file(backend_file_name);
    free(backend_file_name);
    print_backend_call_replace_sync(p->backend_proxy, previous_parent_dialog, NULL, NULL);

    fgets(line, 1024, fp);
    p->id = get_string_copy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->name = get_string_copy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->location = get_string_copy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->info = get_string_copy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->make_and_model = get_string_copy(strtok(line, "#"));

    fgets(line, 1024, fp);
    p->state = get_string_copy(strtok(line, "#"));

    fscanf(fp, "%d\n", &p->is_accepting_jobs);

    int count;
    fscanf(fp, "%d\n", &count);

    char *name, *value;
    while (count--)
    {
        fgets(line, 1024, fp);
        name = strtok(line, "#");
        value = strtok(NULL, "#");
        printf("%s  : %s \n", name, value);
        add_setting(p->settings, name, value);
    }

    fclose(fp);
    free(path);

    return p;
}

char *get_human_readable_option_name(PrinterObj *p, char *option_name)
{
    char *human_readable_name;
    GError *error = NULL;
    print_backend_call_get_human_readable_option_name_sync(p->backend_proxy, option_name,
                                                           &human_readable_name, NULL, &error);
    if(error) {
        DBG_LOG("Error getting human readable option name", ERR);
        return option_name;
    } else {
        return human_readable_name;
    }
}

char *get_human_readable_choice_name(PrinterObj *p, char *option_name, char* choice_name)
{
    char *human_readable_name;
    GError *error = NULL;
    print_backend_call_get_human_readable_choice_name_sync(p->backend_proxy, option_name, choice_name,
                                                           &human_readable_name, NULL, &error);
    if(error) {
        DBG_LOG("Error getting human readable choice name", ERR);
        return choice_name;
    } else {
        return human_readable_name;
    }
}

void get_media_size(PrinterObj *p, const char *media_size, int *width, int *length)
{
    GError *error = NULL;
    GVariant *var;
    print_backend_call_get_media_size_sync(p->backend_proxy, media_size,
                                            &var, NULL, &error);
    g_variant_get(var, "(ii)", width, length);
}

/**
________________________________________________ Settings __________________________________________
**/
Settings *get_new_Settings()
{
    Settings *s = g_new0(Settings, 1);
    s->count = 0;
    s->table = g_hash_table_new(g_str_hash, g_str_equal);
    return s;
}

void copy_settings(const Settings *source, Settings *dest)
{
    GHashTableIter iter;
    g_hash_table_iter_init(&iter, source->table);
    gpointer key, value;
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        add_setting(dest, (char *)key, (char *)value);
    }
}
void add_setting(Settings *s, const char *name, const char *val)
{
    char *prev = g_hash_table_lookup(s->table, name);
    if (prev)
    {
        /**
         * The value is already there, so replace it instead
         */
        g_hash_table_replace(s->table, get_string_copy(name), get_string_copy(val));
        free(prev);
    }
    else
    {
        g_hash_table_insert(s->table, get_string_copy(name), get_string_copy(val));
        s->count++;
    }
}

gboolean clear_setting(Settings *s, char *name)
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

GVariant *serialize_to_gvariant(Settings *s)
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

void save_to_disk(Settings *s)
{
    char *path = get_absolute_path(SETTINGS_FILE);
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

Settings *read_settings_from_disk()
{
    char *path = get_absolute_path(SETTINGS_FILE);
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        DBG_LOG("No previous settings found.", WARN);
        return NULL;
    }
    Settings *s = get_new_Settings();
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
        add_setting(s, name, value);
    }
    fclose(fp);
    free(path);
    return s;
}

void delete_Settings(Settings *s)
{
    GHashTable *h = s->table;
    free(s);
    g_hash_table_destroy(h);
}
/**
________________________________________________ Options __________________________________________
**/
Options *get_new_Options()
{
    Options *o = g_new0(Options, 1);
    o->count = 0;
    o->table = g_hash_table_new(g_str_hash, g_str_equal);
    return o;
}

/**************Option************************************/
void print_option(const Option *opt)
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
 * ________________________________ Job __________________________
 */
void unpack_job_array(GVariant *var, int num_jobs, Job *jobs, char *backend_name)
{
    int i;
    char *str;
    GVariantIter *iter;
    g_variant_get(var, JOB_ARRAY_ARGS, &iter);
    int size;
    char *jobid, *title, *printer, *user, *state, *submit_time;
    for (i = 0; i < num_jobs; i++)
    {
        g_variant_iter_loop(iter, JOB_ARGS, &jobid, &title, &printer, &user, &state, &submit_time, &size);
        jobs[i].job_id = get_string_copy(jobid);
        jobs[i].title = get_string_copy(title);
        jobs[i].printer_id = get_string_copy(printer);
        jobs[i].backend_name = backend_name;
        jobs[i].user = get_string_copy(user);
        jobs[i].state = get_string_copy(state);
        jobs[i].submitted_at = get_string_copy(submit_time);
        jobs[i].size = size;

        //printf("Printer %s ; state %s \n",printer, state);
    }
}
/**
 * ________________________________utility functions__________________________
 */

void DBG_LOG(const char *msg, int msg_level)
{
    if (DEBUG_LEVEL >= msg_level)
    {
        printf("%s\n", msg);
        fflush(stdout);
    }
}
char *concat(char *printer_id, char *backend_name)
{
    char *str = malloc(sizeof(char) * (strlen(printer_id) + strlen(backend_name) + 2));
    sprintf(str, "%s#%s", printer_id, backend_name);
    return str;
}

void unpack_options(GVariant *var, int num_options, Options *options)
{
    options->count = num_options;
    int i, j;
    char *str;
    GVariantIter *iter;
    GVariantIter *array_iter;
    char *name, *default_val;
    int num_sup;
    g_variant_get(var, "a(ssia(s))", &iter);
    Option *opt;
    for (i = 0; i < num_options; i++)
    {
        opt = g_new0(Option, 1);
        g_variant_iter_loop(iter, "(ssia(s))", &name, &default_val,
                            &num_sup, &array_iter);
        opt->option_name = get_string_copy(name);
        opt->default_value = get_string_copy(default_val);
        opt->num_supported = num_sup;
        opt->supported_values = new_cstring_array(num_sup);
        for (j = 0; j < num_sup; j++)
        {
            g_variant_iter_loop(array_iter, "(s)", &str);
            opt->supported_values[j] = get_string_copy(str);
        }
        g_hash_table_insert(options->table, (gpointer)opt->option_name, (gpointer)opt);
    }
}
/************************************************************************************************/
