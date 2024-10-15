#include "cpdb-frontend.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>

static void                 fetchPrinterListFromBackend     (cpdb_frontend_obj_t *      frontend_obj,
                                                             const char *               backend);
                                             
static GList *              cpdbLoadDefaultPrinters         (const char *               path);

static int                  cpdbSetDefaultPrinter           (const char *               path,
                                                             cpdb_printer_obj_t *       printer_obj);

static void                 cpdbDeleteTranslations          (cpdb_printer_obj_t *       printer_obj);

static void                 cpdbUnpackOptions               (int                        num_options,
                                                             GVariant *                 var,
                                                             int                        num_media,
                                                             GVariant *                 media_var,
                                                             cpdb_options_t *           options);
static void                 cpdbUnpackJobArray              (GVariant *                 var,
                                                             int                        num_jobs,
                                                             cpdb_job_t *               jobs,
                                                             char *                     backend_name);
static GHashTable *         cpdbUnpackTranslations          (GVariant *                 translations);
static void                 add_to_hash_table               (gpointer                   key,
                                                             gpointer                   value, 
                                                             gpointer                   user_data);

/**
________________________________________________ cpdb_frontend_obj_t __________________________________________

**/

static void free_printer_object(void* printer)
{
    cpdbDeletePrinterObj(printer);
}

cpdb_frontend_obj_t *cpdbGetNewFrontendObj(cpdb_printer_callback printer_cb)
{
    cpdb_frontend_obj_t *f = g_new0(cpdb_frontend_obj_t, 1);
    
    f->connection = NULL;
    f->printer_cb = printer_cb;
    f->num_backends = 0;
    f->backend = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       free,
                                       g_object_unref);
    f->num_printers = 0;
    f->printer = g_hash_table_new_full(g_str_hash,
                                       g_str_equal,
                                       free,
                                       free_printer_object);
    f->last_saved_settings = cpdbReadSettingsFromDisk();
    return f;
}

void cpdbDeleteFrontendObj(cpdb_frontend_obj_t *f)
{
    if (f == NULL)
        return;
    logdebug("Deleting frontend obj \n");

    cpdbDisconnectFromDBus(f);


    if (f->backend)
        g_hash_table_destroy(f->backend);
    if (f->printer)
        g_hash_table_destroy(f->printer);
    if (f->last_saved_settings)
        cpdbDeleteSettings(f->last_saved_settings);
    
    free(f);
}

void cpdbPrinterCallback(cpdb_frontend_obj_t *f, cpdb_printer_obj_t *p, cpdb_printer_update_t change)
{
    switch(change)
    {
    case CPDB_CHANGE_PRINTER_ADDED:
        g_message("Added printer %s : %s!\n", p->name, p->backend_name);
        break;

    case CPDB_CHANGE_PRINTER_REMOVED:
        g_message("Removed printer %s : %s!\n", p->name, p->backend_name);
        cpdbDeletePrinterObj(p);
        break;
    
    case CPDB_CHANGE_PRINTER_STATE_CHANGED:
        g_message("Printer state changed for %s : %s to \"%s\"", p->name, p->backend_name, p->state);
        break;
    }
}

void cpdbOnPrinterAdded(GDBusConnection *connection,
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
    f->printer_cb(f, p, CPDB_CHANGE_PRINTER_ADDED);
}

void cpdbOnPrinterRemoved(GDBusConnection *connection,
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
    f->printer_cb(f, p, CPDB_CHANGE_PRINTER_REMOVED);
}

void cpdbOnPrinterStateChanged(GDBusConnection *connection,
                               const gchar *sender_name,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *signal_name,
                               GVariant *parameters,
                               gpointer user_data)
{
    cpdb_frontend_obj_t *f = (cpdb_frontend_obj_t *) user_data;
    gboolean printer_is_accepting_jobs;
    char *printer_id, *printer_state, *backend_name;

    g_variant_get(parameters, "(ssbs)", &printer_id, &printer_state,
                    &printer_is_accepting_jobs, &backend_name);
    cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
    if (p->state)
        free(p->state);
    p->state = g_strdup(printer_state);
    p->accepting_jobs = printer_is_accepting_jobs;
    f->printer_cb(f, p, CPDB_CHANGE_PRINTER_STATE_CHANGED);
}

GDBusConnection *cpdbGetDbusConnection()
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

    g_free(bus_addr);
    if (error)
    {
        logerror("Error acquiring bus connection : %s\n", error->message);
        return NULL;
    }
    logdebug("Acquired bus connection\n");
    return connection;
}

void cpdbConnectToDBus(cpdb_frontend_obj_t *f)
{
    GError *error = NULL;

    if ((f->connection = cpdbGetDbusConnection()) == NULL)
    {
        loginfo("Couldn't connect to DBus\n");
        return;
    }
    
    g_dbus_connection_signal_subscribe(f->connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       CPDB_SIGNAL_PRINTER_ADDED,       //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       cpdbOnPrinterAdded,                //callback
                                       f,                            //user_data
                                       NULL);

    g_dbus_connection_signal_subscribe(f->connection,
                                       NULL,                            //Sender name
                                       "org.openprinting.PrintBackend", //Sender interface
                                       CPDB_SIGNAL_PRINTER_REMOVED,     //Signal name
                                       NULL,                            /**match on all object paths**/
                                       NULL,                            /**match on all arguments**/
                                       0,                               //Flags
                                       cpdbOnPrinterRemoved,              //callback
                                       f,                            //user_data
                                       NULL);
    g_dbus_connection_signal_subscribe(f->connection,
                                       NULL,                                //Sender name
                                       "org.openprinting.PrintBackend",     //Sender interface
                                       CPDB_SIGNAL_PRINTER_STATE_CHANGED,   //Signal name
                                       NULL,                                /**match on all object paths**/
                                       NULL,                                /**match on all arguments**/
                                       0,                                   //Flags
                                       cpdbOnPrinterStateChanged,            //callback
                                       f,                                //user_data
                                       NULL);


    if (error)
    {
        logerror("Error exporting frontend interface : %s\n", error->message);
        return;
    }
    
    cpdbActivateBackends(f); 
    
}

void stopListingLookup(gpointer key, gpointer value, gpointer user_data){
    PrintBackend *proxy = value;
    GError *error = NULL; 
    print_backend_call_do_listing_sync(proxy, false, NULL, &error);
    if (error) {
        logerror("Error in DBus call doListing: %s\n", error->message);
        g_error_free(error);
    }
}

void cpdbDisconnectFromDBus(cpdb_frontend_obj_t *f)
{
    if (f->connection == NULL || g_dbus_connection_is_closed(f->connection))
    {
        logwarn("Already disconnected from DBus\n");
        return;
    }
    g_hash_table_foreach(f->backend, stopListingLookup, NULL);
    g_dbus_connection_flush_sync(f->connection, NULL, NULL);
    g_dbus_connection_close_sync(f->connection, NULL, NULL);
    g_clear_object(&f->connection);
}

static void fetchPrinterListFromBackend(cpdb_frontend_obj_t *f, const char *backend)
{
    int num_printers;
    GVariantIter iter;
    GVariant *printers, *printer;
    PrintBackend *proxy;
    GError *error = NULL;
    cpdb_printer_obj_t *p;

    if ((proxy = g_hash_table_lookup(f->backend, backend)) == NULL)
    {
        logerror("Couldn't get %s proxy object\n", backend);
        return;
    }
    print_backend_call_get_all_printers_sync (proxy, &num_printers,
                                                &printers, NULL, &error);
    if (error)
    {
        logerror("Error getting %s printer list : %s\n", backend, error->message);
        return;
    }
    logdebug("Fetched %d printers from backend %s\n", num_printers, backend);
    g_variant_iter_init(&iter, printers);
    while (g_variant_iter_loop(&iter, "(v)", &printer))
    {
        p = cpdbGetNewPrinterObj();
        cpdbFillBasicOptions(p, printer);
        if (f->last_saved_settings != NULL)
            cpdbCopySettings(f->last_saved_settings, p->settings);
        cpdbAddPrinter(f, p);
    }
}

bool cpdbRefreshPrinterList(cpdb_frontend_obj_t *f, const char *backend)
{ 
    int num_printers; 
    GVariantIter iter; 
    GVariant *printers, *printer;
    PrintBackend *proxy; 
    GError *error = NULL; 
    cpdb_printer_obj_t *p; 
 
    if ((proxy = g_hash_table_lookup(f->backend, backend)) == NULL) 
    { 
        logerror("Couldn't get %s proxy object\n", backend); 
        return false; 
    } 
    print_backend_call_get_all_printers_sync (proxy, &num_printers, 
                                                &printers, NULL, &error); 
    if (error) 
    { 
        logerror("Error getting %s printer list : %s\n", backend, error->message); 
        return false; 
    } 
    logdebug("Fetched %d printers from backend %s\n", num_printers, backend); 
    g_variant_iter_init(&iter, printers); 
    while (g_variant_iter_loop(&iter, "(v)", &printer)) 
    { 
        p = cpdbGetNewPrinterObj(); 
        cpdbFillBasicOptions(p, printer); 
        if (f->last_saved_settings != NULL) 
            cpdbCopySettings(f->last_saved_settings, p->settings); 
        cpdbAddPrinter(f, p); 
    } 
 
    GHashTableIter iterator; 
    gpointer key, value; 
 
    g_hash_table_iter_init(&iterator, f->printer); 
 
    while (g_hash_table_iter_next(&iterator, &key, &value)) { 
        cpdb_printer_obj_t* printer_obj = (cpdb_printer_obj_t*)value; 
        char* backend_name = printer_obj->backend_name; 
 
        // Compare the backend_name with the provided one 
        if (strcmp(backend_name, backend) == 0) { 
            // Check if backend_name is not in the printers hashtable 
            g_variant_iter_init(&iter, printers); 
            int printer_exists = 0;
            while (g_variant_iter_loop(&iter, "(v)", &printer)) 
            { 
                cpdb_printer_obj_t *temp = cpdbGetNewPrinterObj();
                cpdbFillBasicOptions(temp, printer);
                if (strcmp(temp->name, printer_obj->name) == 0){
                    printer_exists = 1;
                    break;
                }
                cpdbDeletePrinterObj(temp);
            }
            if(printer_exists == 0) cpdbRemovePrinter(f, printer_obj->id, backend_name);
        }
    }
    return true;
}

// Helper function to add existing backends to a hash table
static void add_to_hash_table(gpointer key, gpointer value, gpointer user_data) {
    GHashTable *hash_table = (GHashTable *)user_data;
    g_hash_table_add(hash_table, key);
}

void cpdbActivateBackends(cpdb_frontend_obj_t *f) {
    int len, i;
    char *service_name, *backend_suffix;
    GDBusProxy *dbus_proxy;
    PrintBackend *backend_proxy;
    GVariantIter iter;
    GError *error = NULL;
    GVariant *service_names, *service_names_tuple;
    GHashTable *existing_backends;
    GHashTableIter hash_iter;
    gpointer key, value;
    const char * const name_lists[] = {
        "ListNames",
        "ListActivatableNames",
        NULL
    };

    // Create a hash table to track existing backends
    existing_backends = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_foreach(f->backend, add_to_hash_table, existing_backends);

    logdebug("Activating backends\n");
    dbus_proxy = g_dbus_proxy_new_sync(f->connection,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       "org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       NULL,
                                       &error);
    if (error) {
        logerror("Error getting dbus proxy: %s", error->message);
        g_error_free(error);
        g_hash_table_destroy(existing_backends);
        return;
    }

    for (i = 0; name_lists[i]; i++) {
        service_names_tuple = g_dbus_proxy_call_sync(dbus_proxy,
                                                     name_lists[i],
                                                     NULL,
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL,
                                                     &error);
        if (error) {
            logerror("Couldn't get service names (%s): %s",
                     name_lists[i], error->message);
            g_error_free(error);
            continue;
        }

        service_names = g_variant_get_child_value(service_names_tuple, 0);

        len = strlen(CPDB_BACKEND_PREFIX);
        g_variant_iter_init(&iter, service_names);
        while (g_variant_iter_next(&iter, "s", &service_name)) {
            if (g_str_has_prefix(service_name, CPDB_BACKEND_PREFIX)) {
                backend_suffix = g_strdup(service_name + len);
                if (!g_hash_table_lookup(f->backend, backend_suffix)) {
                    loginfo("Found backend %s (%s)\n", backend_suffix,
                            i ? "Starting now" : "Already running");
                    backend_proxy = cpdbCreateBackend(f->connection, service_name);
                    if (backend_proxy) {
                        g_hash_table_insert(f->backend, strdup(backend_suffix), backend_proxy);
                        f->num_backends++;
                        if (!g_hash_table_contains(existing_backends, backend_suffix)) {
                            fetchPrinterListFromBackend(f, backend_suffix);
                        }
                    }
                }
                g_hash_table_remove(existing_backends, backend_suffix);
                g_free(backend_suffix);
            }
            g_free(service_name);
        }

        g_variant_unref(service_names);
        g_variant_unref(service_names_tuple);
    }

    // Remove backends that are no longer present
    g_hash_table_iter_init(&hash_iter, existing_backends);
    while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
        loginfo("Removing backend %s\n", (char *)key);
        g_hash_table_remove(f->backend, key);
    }

    g_hash_table_destroy(existing_backends);
    g_object_unref(dbus_proxy);

    if (f->hide_remote) cpdbHideRemotePrinters(f);
    if (f->hide_temporary) cpdbHideTemporaryPrinters(f);
}

gpointer background_thread(gpointer user_data) {
    cpdb_frontend_obj_t *f = (cpdb_frontend_obj_t *)user_data;
    while (1) {
        for (int i = 0; i < 50; i ++) {
            if (f->stop_flag) break;
            usleep(100000);
        }
        if (f->stop_flag) break;
        cpdbActivateBackends(f);
    }
    return NULL;
}

// Start the background thread
void cpdbStartBackendListRefreshing(cpdb_frontend_obj_t *f) {
    f->stop_flag = FALSE;
    f->background_thread = g_thread_new("background_thread", background_thread, f);
}

// Stop the background thread
void cpdbStopBackendListRefreshing(cpdb_frontend_obj_t *f) {
    f->stop_flag = TRUE;
    g_thread_join(f->background_thread);
}

cpdb_frontend_obj_t *cpdbStartListingPrinters(cpdb_printer_callback printer_cb){
    cpdbInit();
    cpdb_frontend_obj_t *f = cpdbGetNewFrontendObj(printer_cb);
    cpdbConnectToDBus(f);
    cpdbStartBackendListRefreshing(f); // Start bg task to check for backends coming/going
    return f;
}

void cpdbStopListingPrinters(cpdb_frontend_obj_t *f){
    cpdbStopBackendListRefreshing(f); // Stop bg task
    cpdbDeleteFrontendObj(f);
}

PrintBackend *cpdbCreateBackend(GDBusConnection *connection,
                                const char *service_name)
{
    PrintBackend *proxy;
    GError *error = NULL;

    proxy = print_backend_proxy_new_sync(connection,
                                         0,
                                         service_name,
                                         CPDB_BACKEND_OBJ_PATH,
                                         NULL,
                                         &error);
    if (error)
    {
        logerror("Error creating backend proxy for %s : %s\n",
                    service_name, error->message);
        return NULL;
    }
    return proxy;
}

void cpdbIgnoreLastSavedSettings(cpdb_frontend_obj_t *f)
{
    loginfo("Ignoring previous settings\n");
    cpdbDeleteSettings(f->last_saved_settings);
    f->last_saved_settings = cpdbGetNewSettings();
}

gboolean cpdbAddPrinter(cpdb_frontend_obj_t *f, 
                        cpdb_printer_obj_t *p)
{
    p->backend_proxy = g_hash_table_lookup(f->backend, p->backend_name);
    if (p->backend_proxy == NULL)
    {
        logerror("Couldn't add printer %s : Backend doesn't exist %s\n",
                    p->id, p->backend_name);
        return FALSE;
    }
    g_object_ref(p->backend_proxy);

    loginfo("Adding printer %s %s\n", p->id, p->backend_name);
    cpdbDebugPrinter(p);
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

    loginfo("Removing printer %s %s\n", printer_id, backend_name);
    key = cpdbConcatSep(printer_id, backend_name);
    if (g_hash_table_contains(f->printer, key))
    {
        p = cpdbFindPrinterObj(f, printer_id, backend_name);
        g_hash_table_remove(f->printer, key);
        f->num_printers--;
    }
    else
    {
        logwarn("Printer %s %s not found\n", printer_id, backend_name);
    }
    
    free(key);
    return p;
}

void cpdbPrintBasicOptions(const cpdb_printer_obj_t *p)
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

void getAllPrintersLookup(gpointer key, gpointer value, gpointer user_data){
    PrintBackend *proxy = value;
    GError *error = NULL; 
    
    int num_printers;
    GVariantIter iter;
    GVariant *printers, *printer;
    cpdb_printer_obj_t *p;

    print_backend_call_get_filtered_printer_list_sync (proxy, &num_printers,
                                                &printers, NULL, &error);
    if (error)
    {
        logerror("Error getting printer list : %s\n", error->message);
        return;
    }
    g_variant_iter_init(&iter, printers);
    while (g_variant_iter_loop(&iter, "(v)", &printer))
    {
        p = cpdbGetNewPrinterObj();
        cpdbFillBasicOptions(p, printer);
        cpdbPrintBasicOptions(p);
        cpdbDeletePrinterObj(p);
    }
}

void cpdbGetAllPrinters(cpdb_frontend_obj_t *f)
{
    loginfo("Fetching all printers\n");
    g_hash_table_foreach(f->backend, getAllPrintersLookup, NULL);    
}

void hideRemoteLookup(gpointer key, gpointer value, gpointer user_data){
    PrintBackend *proxy = value;
    GError *error = NULL; 
    print_backend_call_show_remote_printers_sync(proxy, false, NULL,
                                       &error);
}

void cpdbHideRemotePrinters(cpdb_frontend_obj_t *f)
{
    loginfo("Hiding remote printers\n");
    g_hash_table_foreach(f->backend, hideRemoteLookup, NULL);
    
}

void showRemoteLookup(gpointer key, gpointer value, gpointer user_data){
    PrintBackend *proxy = value;
    GError *error = NULL; 
    print_backend_call_show_remote_printers_sync(proxy, true, NULL,
                                       &error);
}

void cpdbUnhideRemotePrinters(cpdb_frontend_obj_t *f)
{
    loginfo("Unhiding remote printers\n");
    g_hash_table_foreach(f->backend, showRemoteLookup, NULL);
    
}

void hideTemporaryLookup(gpointer key, gpointer value, gpointer user_data){
    PrintBackend *proxy = value;
    GError *error = NULL; 
    print_backend_call_show_temporary_printers_sync(proxy, false, NULL,
                                       &error);
}

void cpdbHideTemporaryPrinters(cpdb_frontend_obj_t *f)
{
    loginfo("Hiding temporary printers\n");
    g_hash_table_foreach(f->backend, hideTemporaryLookup, NULL);
    
}

void showTemporaryLookup(gpointer key, gpointer value, gpointer user_data){
    PrintBackend *proxy = value;
    GError *error = NULL; 
    print_backend_call_show_temporary_printers_sync(proxy, true, NULL,
                                       &error);
}

void cpdbUnhideTemporaryPrinters(cpdb_frontend_obj_t *f)
{
    loginfo("Unhiding temporary printers\n");
    g_hash_table_foreach(f->backend, showTemporaryLookup, NULL);
    
}

cpdb_printer_obj_t *cpdbFindPrinterObj(cpdb_frontend_obj_t *f,
                                       const char *printer_id,
                                       const char *backend_name)
{
    char *hashtable_key;
    cpdb_printer_obj_t *p;

    if (printer_id == NULL || backend_name == NULL)
    {
        logwarn("Invalid parameters: cpdbFindPrinterObj()\n");
        return NULL;
    }

    hashtable_key = cpdbConcatSep(printer_id, backend_name);
    p = g_hash_table_lookup(f->printer, hashtable_key);
    if (p == NULL)
    {
        logwarn("Couldn't find printer %s %s : Doesn't exist\n",
                printer_id, backend_name);
    }

    free(hashtable_key);
    return p;
}

cpdb_printer_obj_t *cpdbGetDefaultPrinterForBackend(cpdb_frontend_obj_t *f,
                                                    const char *backend_name)
{
    char *def = NULL;
    char *service_name;
    GError *error = NULL;
    PrintBackend *proxy;
    cpdb_printer_obj_t *p = NULL;
    
    proxy = g_hash_table_lookup(f->backend, backend_name);
    if (proxy == NULL)
    {
        logwarn("Couldn't find backend proxy for %s\n", backend_name);
        service_name = g_strconcat(CPDB_BACKEND_PREFIX, backend_name, NULL);
        proxy = cpdbCreateBackend(f->connection, service_name);
        free(service_name);
        if (proxy == NULL)
        {
            logerror("Error getting default printer for backend : Couldn't get backend proxy\n");
            return NULL;
        }
    }

    print_backend_call_get_default_printer_sync(proxy, &def, NULL, &error);
    if (error)
    {
        logerror("Error getting default printer for backend : %s\n", error->message);
        return NULL;
    }
    
    p = cpdbFindPrinterObj(f, def, backend_name);
    g_free(def);
    if (p)
        logdebug("Obtained default printer %s for backend %s\n", p->id, backend_name);
    return p;
}

GList *cpdbLoadDefaultPrinters(const char *path)
{
    FILE *fp;
    char buf[CPDB_BSIZE];
    GList *printers = NULL;

    if ((fp = fopen(path, "r")) == NULL)
    {
        logwarn("Error loading default printers : Couldn't open %s for reading\n",
                path);
        return NULL;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        buf[strcspn(buf, "\r\n")] = 0;
        printers = g_list_prepend(printers, g_strdup(buf));
    }
    printers = g_list_reverse(printers);
    logdebug("Loaded default printers from %s\n", path);

    fclose(fp);
    return printers;
}

cpdb_printer_obj_t *cpdbGetDefaultPrinter(cpdb_frontend_obj_t *f)
{   
    gpointer key, value;
    GHashTableIter iter;
    char *conf_dir, *path, *printer_id, *backend_name;
    cpdb_printer_obj_t *default_printer = NULL;
    GList *printer, *printers = NULL;

    if (f->num_printers == 0 || f->num_backends == 0)
    {
        logwarn("Couldn't get default printer : No printers found\n");
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
            goto found;
        }
    }
    if (printers)
        g_list_free_full(printers, free);

    logdebug("Couldn't find a valid default printer from config files\n");

    /**  Fallback to default CUPS printer if CUPS backend exists **/
    default_printer = cpdbGetDefaultPrinterForBackend(f, "CUPS");
    if (default_printer)
        goto found;
    logdebug("Couldn't find a valid default CUPS printer\n");
    
    /** Fallback to default FILE printer if FILE backend exists **/
    default_printer = cpdbGetDefaultPrinterForBackend(f, "FILE");
    if (default_printer)
        goto found;
    logdebug("Couldn't find a valid default FILE printer\n");
    
    /** Fallback to the default printer of first backend found **/
    g_hash_table_iter_init(&iter, f->backend);
    g_hash_table_iter_next(&iter, &key, &value);

    backend_name = (char *) key;
    default_printer = cpdbGetDefaultPrinterForBackend(f, backend_name);
    if (default_printer)
        goto found;
    logdebug("Couldn't find a valid default %s printer\n", backend_name);
    
    /** Fallback to first printer found **/
    g_hash_table_iter_init(&iter, f->printer);
    g_hash_table_iter_next(&iter, &key, &value);
    default_printer = (cpdb_printer_obj_t *) value;
    if (!default_printer)
    {
        logerror("Couldn't find a valid printer\n");
        return NULL;
    }

found:
    logdebug("Found default printer %s %s\n",
                default_printer->id, default_printer->backend_name);
    return default_printer;
}

int cpdbSetDefaultPrinter(const char *path,
                          cpdb_printer_obj_t *p)
{
    FILE *fp;
    char *printer_data;
    GList *printer, *next, *printers;
    
    printers = cpdbLoadDefaultPrinters(path);
    printer_data = cpdbConcatSep(p->id, p->backend_name);
    
    if ((fp = fopen(path, "w")) == NULL)
    {
        logerror("Error setting default printer : Couldn't open %s for writing\n",
                    path);
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
    loginfo("Saved default printers to %s", path);

    fclose(fp);
    return 1;
}

int cpdbSetUserDefaultPrinter(cpdb_printer_obj_t *p)
{
    int ret;
    char *conf_dir, *path;

    if ((conf_dir = cpdbGetUserConfDir()) == NULL)
    {
        logerror("Error setting default printer : Couldn't get system config dir\n");
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
        logerror("Error setting default printer : Couldn't get system config dir\n");
        return 0;
    }
    path = cpdbConcatPath(conf_dir, CPDB_DEFAULT_PRINTERS_FILE);
    ret = cpdbSetDefaultPrinter(path, p);

    free(path);
    free(conf_dir);
    return ret;
}
/**
________________________________________________ cpdb_printer_obj_t __________________________________________
**/

cpdb_printer_obj_t *cpdbGetNewPrinterObj()
{
    cpdb_printer_obj_t *p = g_new0 (cpdb_printer_obj_t, 1);
    p->options = NULL;
    p->settings = cpdbGetNewSettings();
    return p;
}

static void cpdbDeleteTranslations(cpdb_printer_obj_t *p)
{
    g_free(p->locale);
    if (p->translations)
        g_hash_table_destroy(p->translations);

    p->locale = NULL;
    p->translations = NULL;
}

void cpdbDeletePrinterObj(cpdb_printer_obj_t *p)
{
    if (p == NULL)
        return;
    
    logdebug("Deleting printer object %s\n", p->id);

    g_free(p->backend_name);
    g_free(p->id);
    g_free(p->name);
    g_free(p->location);
    g_free(p->info);
    g_free(p->state);
    g_free(p->make_and_model);

    if (p->backend_proxy)
        g_object_unref(p->backend_proxy);
    if (p->options)
        cpdbDeleteOptions(p->options);
    if (p->settings)
        cpdbDeleteSettings(p->settings);
    cpdbDeleteTranslations(p);
    
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

void cpdbDebugPrinter(const cpdb_printer_obj_t *p)
{
    logdebug("-------------------------\n");
    logdebug("Printer %s\n", p->id);
    logdebug("name: %s\n", p->name);
    logdebug("location: %s\n", p->location);
    logdebug("info: %s\n", p->info);
    logdebug("make and model: %s\n", p->make_and_model);
    logdebug("accepting jobs? %s\n", (p->accepting_jobs ? "yes" : "no"));
    logdebug("state: %s\n", p->state);
    logdebug("backend: %s\n", p->backend_name);
    logdebug("-------------------------\n\n");
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
        logerror("Error getting accepting_jobs status for %s %s : %s\n",
                    p->id, p->backend_name, error->message);
        return FALSE;
    }

    logdebug("Obtained accepting_jobs=%d; for %s %s\n", 
                p->accepting_jobs, p->id, p->backend_name);
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
        logerror("Error getting printer state for %s %s : %s\n",
                    p->id, p->backend_name, error->message);
        return NULL;
    }

    logdebug("Obtained state=%s; for %s %s\n", 
                p->state, p->id, p->backend_name);
    return p->state;
}

cpdb_options_t *cpdbGetAllOptions(cpdb_printer_obj_t *p)
{
    if (p == NULL) 
    {
        logwarn("Invalid params: cpdbGetAllOptions()\n");
        return NULL;
    }

    /** 
     * If the options were previously queried, 
     * return them, instead of querying again.
    */
    if (p->options)
        return p->options;

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
    if (error)
    {
        logerror("Error getting printer options for %s %s : %s\n",
                    p->id, p->backend_name, error->message);
        return NULL;
    }

    loginfo("Obtained %d options and %d media for %s %s\n",
            num_options, num_media, p->id, p->backend_name);
    p->options = cpdbGetNewOptions();
    cpdbUnpackOptions(num_options, var, num_media, media_var, p->options);
    return p->options;
}

cpdb_option_t *cpdbGetOption(cpdb_printer_obj_t *p,
                             const char *name)
{
    if (p == NULL || name == NULL) 
    {
        logwarn("Invalid params: cpdbGetOption()\n");
        return NULL;
    }
    cpdbGetAllOptions(p);
    return (cpdb_option_t *)(g_hash_table_lookup(p->options->table, name));
}

char *cpdbGetDefault(cpdb_printer_obj_t *p,
                     const char *name)
{
    if (p == NULL || name == NULL)
    {
        logwarn("Invalid params: cpdbGetDefault()\n");
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
        logwarn("Invalid params: cpdbGetSetting()\n");
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

static void cpdbDebugPrintSettings(cpdb_settings_t *s)
{
    gpointer key, value;
    GHashTableIter iter;

    g_hash_table_iter_init(&iter, s->table);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        logdebug("%s -> %s\n", (char *) key, (char *) value);
    }
}

char *cpdbPrintFile(cpdb_printer_obj_t *p,
                    const char *file_path)
{
    const char *title = "";
    return cpdbPrintFileWithJobTitle(p, file_path, title);
}

char *cpdbPrintFileWithJobTitle(cpdb_printer_obj_t *p,
                    const char *file_path, const char *title)
{
    char *jobid = NULL;
    char *socket_path = NULL;
    int fd = cpdbPrintFD(p, &jobid, title, &socket_path);
    if (fd == -1) {
        logerror("Error connecting to backend for printing file %s on %s %s: %s\n",
                 file_path, p->id, p->backend_name, strerror(errno));
        g_free(socket_path);
        return NULL;
    }

    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        close(fd);
        logerror("Error opening file %s on %s %s: %s\n",
                 file_path, p->id, p->backend_name, strerror(errno));
        g_free(socket_path);
        return NULL;
    }

    char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (write(fd, buffer, bytesRead) != bytesRead) {
            fclose(file);
            close(fd);
            unlink(socket_path);
            g_free(socket_path);
            logerror("Error sending file %s on %s %s: %s\n",
                     file_path, p->id, p->backend_name, strerror(errno));
            return NULL;
        }
    }

    fclose(file);
    close(fd);
    unlink(socket_path);
    g_free(socket_path);

    return jobid;
}

int cpdbPrintFD(cpdb_printer_obj_t *p,
                char **jobid, const char *title, char **socket_path)
{
    *socket_path = cpdbPrintSocket(p, jobid, title);
    if (*socket_path == NULL) {
        logerror("Error getting socket for job on %s %s: %s\n",
                 p->id, p->backend_name, strerror(errno));
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        logerror("Error creating fd for job %s on %s %s with socket %s: %s\n",
                 *jobid, p->id, p->backend_name, *socket_path, strerror(errno));
        return -1;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, *socket_path, sizeof(server_addr.sun_path) - 1);

    int res = connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (res == -1) {
        logerror("Error connecting to socket for %s on %s %s, socket %s: %s\n",
                 *jobid, p->id, p->backend_name, *socket_path, strerror(errno));
        close(fd);  // Close the socket in case of an error
        return -1;
    }

    return fd;
}

char *cpdbPrintSocket(cpdb_printer_obj_t *p, char **jobid, const char *title)
{
    char *socket;
    GError *error = NULL;   
    cpdbDebugPrintSettings(p->settings);
    print_backend_call_print_socket_sync(p->backend_proxy,
                                       p->id,
                                       p->settings->count,
                                       cpdbSerializeToGVariant(p->settings),
                                       title,
                                       jobid,
                                       &socket,
                                       NULL,
                                       &error);
                                       
    if (error) {
        logerror("Error opening socket on %s %s : %s\n", 
                    p->id, p->backend_name, error->message);
        return NULL;
    }
    
    if (*jobid == NULL || **jobid == '\0') {
        logerror("Error while trying to create a job on %s %s: Couldn't create a job\n", 
                    p->id, p->backend_name);
        return NULL;
    }

    if (socket == NULL || *socket == '\0') {
        logerror("Error opening socket on %s %s: Couldn't create a socket\n", 
                    p->id, p->backend_name);
        return NULL;
    }
    
    loginfo("Socket opened for printing job %s on %s %s successfully: %s\n",
            *jobid, p->id, p->backend_name, socket);
    cpdbSaveSettingsToDisk(p->settings);
    return socket;
}

void cpdbAddSettingToPrinter(cpdb_printer_obj_t *p,
                             const char *name,
                             const char *val)
{
    if (p == NULL || name == NULL)
    {
        logwarn("Invalid params: cpdbAddSettingToPrinter()\n");
        return;
    }

    cpdbAddSetting(p->settings, name, val);
}

gboolean cpdbClearSettingFromPrinter(cpdb_printer_obj_t *p,
                                     const char *name)
{
    if (p == NULL || name == NULL)
    {
        logwarn("Invalid params: cpdbClearSettingFromPrinter()\n");
        return FALSE;
    }
    return cpdbClearSetting(p->settings, name);
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
    GError *error = NULL;
	
    print_backend_call_keep_alive_sync(p->backend_proxy, NULL, &error);
    if (error)
    {
        logerror("Error keeping backend %s alive : %s\n",
                    p->backend_name, error->message);
        return;
    }
    loginfo("Keeping backend %s alive\n", p->backend_name);
    
    path = cpdbGetAbsolutePath(filename);
    if ((fp = fopen(path, "w")) == NULL)
    {
        logerror("Error pickling printer %s %s : Couldn't open %s for writing\n",
                    p->id, p->backend_name, path);
        return;
    }

    unique_bus_name = g_dbus_connection_get_unique_name(parent_dialog->connection);
    if (unique_bus_name == NULL)
    {
        logerror("Error pickling printer %s %s: Couldn't get unique bus name\n",
                    p->id, p->backend_name);
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
    loginfo("Pickled printer %s %s to %s\n",
            p->id, p->backend_name, path);
    
    fclose(fp);
    free(path);
}

cpdb_printer_obj_t *cpdbResurrectPrinterFromFile(const char *filename)
{
    FILE *fp;
    int count;
    char buf[CPDB_BSIZE];
    GDBusConnection *connection;
    char *name, *value, *path = NULL;
    char *service_name = NULL, *previous_parent_dialog = NULL;
    GError *error = NULL;
    cpdb_printer_obj_t *p;

    path = cpdbGetAbsolutePath(filename);
    if ((fp = fopen(path, "r")) == NULL)
    {
        logerror("Error resurrecting printer : Couldn't open %s for reading\n",
		 path);
        goto failed;
    }

    p = cpdbGetNewPrinterObj();

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    previous_parent_dialog = g_strdup(strtok(buf, "#"));

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->backend_name = g_strdup(strtok(buf, "#"));
    
    service_name = g_strconcat(CPDB_BACKEND_PREFIX, p->backend_name, NULL);
    if ((connection = cpdbGetDbusConnection()) == NULL)
    {
        logerror("Error resurrecting printer : Couldn't get dbus connection\n");
        goto failed;
    }
    p->backend_proxy = cpdbCreateBackend(connection,
                                         service_name);
    free(service_name);
    print_backend_call_replace_sync(p->backend_proxy, 
                                    previous_parent_dialog, 
                                    NULL, 
                                    &error);
    if (error)
    {
        logerror("Error replacing resurrected printer : %s\n",
                    error->message); 
        goto failed;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->id = g_strdup(strtok(buf, "#"));

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->name = g_strdup(strtok(buf, "#"));

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->location = g_strdup(strtok(buf, "#"));

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->info = g_strdup(strtok(buf, "#"));

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->make_and_model = g_strdup(strtok(buf, "#"));

    if (fgets(buf, sizeof(buf), fp) == NULL)
        goto parse_error;
    p->state = g_strdup(strtok(buf, "#"));

    if (fscanf(fp, "%d\n", &p->accepting_jobs) == 0)
        goto parse_error;
    
    cpdbDebugPrinter(p);

    if (fscanf(fp, "%d\n", &count) == 0)
        goto parse_error;
    while (count--)
    {
        if (fgets(buf, sizeof(buf), fp) == NULL)
            goto parse_error;
        name = strtok(buf, "#");
        value = strtok(NULL, "#");
        cpdbAddSetting(p->settings, name, value);
    }
    loginfo("Resurrected printer %s %s from %s\n", 
            p->id, p->backend_name, filename);

    fclose(fp);
    free(path);
    free(previous_parent_dialog);
    return p;

parse_error:
    logerror("Error resurrecting printer : Coudln't parse %s\n", path);
    
failed:
    if (fp)
        fclose(fp);
    free(path);
    if (service_name)
        free(service_name);
    if (previous_parent_dialog)
        free(previous_parent_dialog);
    return NULL;
}

char *cpdbGetOptionTranslation(cpdb_printer_obj_t *p,
                               const char *option_name,
                               const char *locale)
{
    char *name_key, *translation;
    GError *error = NULL;

    if (p == NULL || option_name == NULL || locale == NULL)
    {
        logwarn("Invalid paramaters: cpdbGetOptionTranslation()\n");
        return NULL;
    }

    if (p->locale != NULL && strcmp(p->locale, locale) == 0)
    {
        name_key = cpdbConcatSep(CPDB_OPT_PREFIX, option_name);
        translation = g_hash_table_lookup(p->translations, name_key);
        free(name_key);
        if (translation)
        {
            logdebug("Found translation=%s; for option=%s;locale=%s;printer=%s#%s;\n",
                        translation, option_name, locale, p->id, p->backend_name);
            return g_strdup(translation);
        }
    }

    print_backend_call_get_option_translation_sync(p->backend_proxy,
                                                   p->id,
                                                   option_name,
                                                   locale,
                                                   &translation,
                                                   NULL,
                                                   &error);
    if (error)
    {
        logerror("Error getting translation for option=%s;locale=%s;printer=%s#%s; : %s\n",
                    option_name, locale,
                    p->id, p->backend_name, error->message);
        return NULL;
    }
    
    logdebug("Obtained translation=%s; for option=%s;locale=%s;printer=%s#%s;\n",
                translation, option_name, locale, p->id, p->backend_name);
    return g_strdup(translation);
}

char *cpdbGetChoiceTranslation(cpdb_printer_obj_t *p,
                               const char *option_name,
                               const char *choice_name,
                               const char *locale)
{
    char *name_key, *choice_key, *translation;
    GError *error = NULL;

    if (p == NULL || option_name == NULL || choice_name == NULL || locale == NULL)
    {
        logwarn("Invalid paramaters: cpdbGetChoiceTranslation()\n");
        return NULL;
    }

    if (p->locale != NULL && strcmp(p->locale, locale) == 0)
    {
        name_key = cpdbConcatSep(CPDB_OPT_PREFIX, option_name);
        choice_key = cpdbConcatSep(name_key, choice_name);
        translation = g_hash_table_lookup(p->translations, choice_key);
        free(name_key);
        free(choice_key);
        if (translation)
        {
            logdebug("Found translation=%s; for option=%s;choice=%s;locale=%s;printer=%s#%s;\n",
                        translation, option_name, choice_name, locale, 
                        p->id, p->backend_name);
            return g_strdup(translation);
        }
    }
    
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
        logerror("Error getting translation for option=%s;choice=%s;locale=%s;printer=%s#%s; : %s\n",
                    option_name, choice_name, locale,
                    p->id, p->backend_name, error->message);
        return NULL;
    }
    
    logdebug("Obtained translation=%s; for option=%s;choice=%s;locale=%s;printer=%s#%s;\n",
                translation, option_name, choice_name, locale, 
                p->id, p->backend_name);
    return g_strdup(translation);
}


char *cpdbGetGroupTranslation(cpdb_printer_obj_t *p,
                              const char *group_name,
                              const char *locale)
{
    char *group_key, *translation;
    GError *error = NULL;

    if (p == NULL || group_name == NULL || locale == NULL)
    {
        logwarn("Invalid paramaters: cpdbGetGroupTranslation()\n");
        return NULL;
    }

    if (p->locale != NULL && strcmp(p->locale, locale) == 0)
    {
        group_key = cpdbConcatSep(CPDB_GRP_PREFIX, group_name);
        translation = g_hash_table_lookup(p->translations, group_key);
        free(group_key);
        if (translation)
        {
            logdebug("Found translation=%s; for group=%s;locale=%s;printer=%s#%s;\n",
                        translation, group_name, locale, p->id, p->backend_name);
            return g_strdup(translation);
        }
    }
    
    print_backend_call_get_group_translation_sync(p->backend_proxy,
                                                  p->id,
                                                  group_name,
                                                  locale,
                                                  &translation,
                                                  NULL,
                                                  &error);

    if (error)
    {
        logerror("Error getting translation for group=%s;locale=%s;printer=%s#%s; : %s\n",
                    group_name, locale,
                    p->id, p->backend_name, error->message);
        return NULL;
    }
    
    logdebug("Obtained translation=%s; for group=%s;locale=%s;printer=%s#%s;\n",
                translation, group_name, locale, p->id, p->backend_name);
    return g_strdup(translation);
}

void cpdbGetAllTranslations(cpdb_printer_obj_t *p,
                            const char *locale)
{
    GVariant *translations;
    GError *error = NULL;

    if (p == NULL || locale == NULL)
    {
        logwarn("Invalid parameters: cpdbGetAllTranslations()\n");
        return;
    }

    if (p->locale != NULL && strcmp(p->locale, locale) == 0)
        return;

    print_backend_call_get_all_translations_sync(p->backend_proxy,
                                                 p->id,
                                                 locale,
                                                 &translations,
                                                 NULL,
                                                 &error);
    if (error)
    {
        logerror("Error getting printer translations in %s for %s %s : %s\n",
                    locale, p->id, p->backend_name, error->message);
        return;
    }
    logdebug("Fetched translations for printer %s %s\n", p->id, p->backend_name);

    cpdbDeleteTranslations(p);
    p->locale = g_strdup(locale);
    p->translations = cpdbUnpackTranslations(translations);
}

//To be used with cpdbGetAllTranslations only
char *cpdbGetChoiceTranslationFromTable(cpdb_printer_obj_t *p,
                               const char *option_name,
                               const char *choice_name,
                               const char *locale)
{
    char *name_key = NULL;
    char *choice_key = NULL;
    char *translation = NULL;

    if (p == NULL || option_name == NULL || choice_name == NULL || locale == NULL)
    {
        logwarn("Invalid parameters: cpdbGetChoiceTranslationFromTable()\n");
        return NULL;
    }

    if (p->locale != NULL && strcmp(p->locale, locale) == 0)
    {
        name_key = cpdbConcatSep(CPDB_OPT_PREFIX, option_name);
        choice_key = cpdbConcatSep(name_key, choice_name);
        translation = g_hash_table_lookup(p->translations, choice_key);
        free(name_key);
        free(choice_key);
        if (translation)
        {
            logdebug("Found translation=%s; for option=%s;choice=%s;locale=%s;printer=%s#%s;\n",
                        translation, option_name, choice_name, locale, 
                        p->id, p->backend_name);
            return g_strdup(translation);
        }
        else
        {
            return g_strdup(choice_name);
        }
    }
    else
    {
        logwarn("Locale does not exist in table. Use cpdbGetChoiceTranslation instead\n");
        return NULL;
    }
}

//To be used with cpdbGetAllTranslations only
char *cpdbGetOptionTranslationFromTable(cpdb_printer_obj_t *p,
                               const char *option_name,
                               const char *locale)
{
    char *name_key = NULL;
    char *translation = NULL;

    if (p == NULL || option_name == NULL || locale == NULL)
    {
        logwarn("Invalid parameters: cpdbGetOptionTranslationFromTable()\n");
        return NULL;
    }

    if (p->locale != NULL && strcmp(p->locale, locale) == 0)
    {
        name_key = cpdbConcatSep(CPDB_OPT_PREFIX, option_name);
        translation = g_hash_table_lookup(p->translations, name_key);
        free(name_key);
        if (translation)
        {
            logdebug("Found translation=%s; for option=%s;locale=%s;printer=%s#%s;\n",
                        translation, option_name, locale, p->id, p->backend_name);
            return g_strdup(translation);
        }
        else
        {
            return g_strdup(option_name);
        }
    }
    else
    {
        logwarn("Locale does not exist in table. Use cpdbGetOptionTranslation instead\n");
        return NULL;
    }
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

typedef struct {
    cpdb_printer_obj_t *p;
    cpdb_async_callback caller_cb;
    void *user_data;
} cpdb_async_details_obj_t;

void acquire_details_cb(PrintBackend *proxy,
                        GAsyncResult *res,
                        gpointer user_data)
{
    cpdb_async_details_obj_t *a = user_data;
    
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
        logerror("Error acquiring printer details for %s %s : %s\n",
                    p->id, p->backend_name, error->message);
        if (caller_cb)
            caller_cb(p, FALSE, a->user_data);
    }
    else
    {
        loginfo("Acquired %d options and %d media for %s %s\n",
                num_options, num_media, p->id, p->backend_name);
        cpdbUnpackOptions(num_options, var, num_media, media_var, p->options);
        if (caller_cb)
            caller_cb(p, TRUE, a->user_data);
    }
    
    free(a);
}

void cpdbAcquireDetails(cpdb_printer_obj_t *p,
                        cpdb_async_callback caller_cb,
                        void *user_data)
{
    if (p == NULL)
    {
        logwarn("Invalid parameters: cpdbAcquireDetails()\n");
        return;
    }

    if (p->options)
    {
        if (caller_cb)
            caller_cb(p, TRUE, user_data);
        return;
    }
    
    cpdb_async_details_obj_t *a = g_new0(cpdb_async_details_obj_t, 1);
    a->p = p;
    a->caller_cb = caller_cb;
    a->user_data = user_data;
    
    logdebug("Acquiring printer details for %s %s\n", p->id, p->backend_name);
    print_backend_call_get_all_options(p->backend_proxy,
                                       p->id, 
                                       NULL,
                                       (GAsyncReadyCallback) acquire_details_cb,
                                       a);
}


typedef struct {
    cpdb_printer_obj_t *p;
    char *locale;
    cpdb_async_callback caller_cb;
    void *user_data;
} cpdb_async_translations_obj_t;


static void acquire_translations_cb(PrintBackend *proxy,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
    GError *error = NULL;
    GVariant *translations;

    cpdb_async_translations_obj_t *a = user_data;
    cpdb_printer_obj_t *p = a->p;

    print_backend_call_get_all_translations_finish(proxy, &translations,
                                                    res, &error);
    if (error)
    {
        logerror("Error getting printer translations for %s %s : %s\n",
                    p->id, p->backend_name, error->message);
        a->caller_cb(p, FALSE, a->user_data);
    }
    else
    {
        cpdbDeleteTranslations(p);
        p->locale = g_strdup(a->locale);
        p->translations = cpdbUnpackTranslations(translations);
        a->caller_cb(p, TRUE, a->user_data);
    }

    free(a->locale);
    free(a);
}

void cpdbAcquireTranslations(cpdb_printer_obj_t *p,
                             const char *locale,
                             cpdb_async_callback caller_cb,
                             void *user_data)
{
    if (p == NULL || locale == NULL)
    {
        logwarn("Invalid parameters: cpdbAcquireTranslations()\n");
        return;
    }

    if (p->locale != NULL && strcmp(locale, p->locale) == 0)
    {
        caller_cb(p, TRUE, user_data);
        return;
    }

    cpdb_async_translations_obj_t *a = g_new0(cpdb_async_translations_obj_t, 1);
    a->p = p;
    a->locale = g_strdup(locale);
    a->caller_cb = caller_cb;
    a->user_data = user_data;

    logdebug("Acquiring printer translations for %s %s\n",
                p->id, p->backend_name);
    print_backend_call_get_all_translations(p->backend_proxy,
                                            p->id,
                                            locale,
                                            NULL,
                                            (GAsyncReadyCallback) acquire_translations_cb,
                                            a);
}

/**
________________________________________________ cpdb_settings_t __________________________________________
**/
cpdb_settings_t *cpdbGetNewSettings()
{
    cpdb_settings_t *s = g_new0(cpdb_settings_t, 1);
    s->count = 0;
    s->table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    return s;
}

void cpdbCopySettings(const cpdb_settings_t *source,
                      cpdb_settings_t *dest)
{
    if (source == NULL || dest == NULL)
    {
        logwarn("Invalid params: cpdbCopySettings()\n");
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
        logwarn("Invalid params: cpdbAddSettings()\n");
        return;
    }

    gboolean new_entry = g_hash_table_insert(s->table,
                                             g_strdup(name),
                                             g_strdup(val));
    if (new_entry)
        s->count++;
}

gboolean cpdbClearSetting(cpdb_settings_t *s, const char *name)
{
    if (s == NULL || name == NULL) 
    {
        logwarn("Invalid params: cpdbClearSetting()\n");
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
    g_variant_builder_unref(builder);
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
        logerror("Error saving settings to disk : Couldn't obtain user config dir\n");
        return;
    }
    path = cpdbConcatPath(conf_dir, CPDB_PRINT_SETTINGS_FILE);

    if ((fp = fopen(path, "w")) == NULL)
    {
        logerror("Error saving settings to disk : Couldn't open %s for writing\n",
                    path);
        return;
    }
    fprintf(fp, "%d\n", s->count);
    
    g_hash_table_iter_init(&iter, s->table);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        fprintf(fp, "%s#%s#\n", (char *)key, (char *)value);
    }
    loginfo("Saved %d settings on disk to %s\n", s->count, path);

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

    s = cpdbGetNewSettings();

    if ((conf_dir = cpdbGetUserConfDir()) == NULL)
    {
        logerror("No previous settings found : Couldn't obtain user config dir\n");
        return s;
    }
    path = cpdbConcatPath(conf_dir, CPDB_PRINT_SETTINGS_FILE);

    if ((fp = fopen(path, "r")) == NULL)
    {
        loginfo("No previous settings found : Couldn't open %s for reading\n",
                    path);
        free(path);
        free(conf_dir);
        
        return s;
    }

    if (fscanf(fp, "%d\n", &count) == 0)
    {
        logerror("Error getting settings from disk : Couldn't parse %s\n",
                    path);
        fclose(fp);
        free(path);
        free(conf_dir);
        return s;
    }
    while (count--)
    {
        if (fgets(buf, sizeof(buf), fp) == NULL)
            break;
        name = strtok(buf, "#");
        value = strtok(NULL, "#");
        cpdbAddSetting(s, name, value);
    }
    loginfo("Retrievied %d settings from disk at %s\n", s->count, path);

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
                                     g_free,
                                     (GDestroyNotify) cpdbDeleteOption);
    o->media_count = 0;
    o->media = g_hash_table_new_full(g_str_hash,
                                     g_str_equal,
                                     g_free,
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
        logdebug("jobid=%s;\n", jobid);
        jobs[i].job_id = g_strdup(jobid);
        logdebug("title=%s;\n", title);
        jobs[i].title = g_strdup(title);
        logdebug("printer=%s;\n", printer);
        jobs[i].printer_id = g_strdup(printer);
        logdebug("backend_name=%s;\n", backend_name);
        jobs[i].backend_name = backend_name;
        logdebug("user=%s;\n", user);
        jobs[i].user = g_strdup(user);
        logdebug("state=%s;\n", state);
        jobs[i].state = g_strdup(state);
        logdebug("submit_time=%s;\n", submit_time);
        jobs[i].submitted_at = g_strdup(submit_time);
        logdebug("size=%d;\n", size);
        jobs[i].size = size;
    }
}
/**
 * ________________________________utility functions__________________________
 */

void cpdbUnpackOptions(int num_options,
                       GVariant *opts_var,
                       int num_media,
                       GVariant *media_var,
                       cpdb_options_t *options)
{
    cpdb_option_t *opt;
    cpdb_media_t *media;
    int i, j, num, width, length, l, r, t, b;
    GVariantIter *iter, *sub_iter;
    char *str, *name, *def, *group;
    
    options->count = num_options;
    g_variant_get(opts_var, "a(sssia(s))", &iter);
    i = 0;
    while (g_variant_iter_loop(iter, "(sssia(s))",
                               &name, &group, &def, &num, &sub_iter))
    {
        if (i >= num_options)
        {
            logwarn("array of options contains more than expected amount");
            g_free(name);
            g_free(group);
            g_free(def);
            g_variant_iter_free(sub_iter);
            break;
        }

        opt = g_new0(cpdb_option_t, 1);
        logdebug("name=%s;\n", name);
        opt->option_name = g_strdup(name);
        logdebug("group=%s;\n", group);
        opt->group_name = g_strdup(group);
        logdebug("default=%s;\n", def);
        opt->default_value = g_strdup(def);
        logdebug("num_choices=%d;\n", num);
        opt->num_supported = num;
        logdebug("choices:\n");
        opt->supported_values = cpdbNewCStringArray(num);

        j = 0;
        while (g_variant_iter_loop(sub_iter, "(s)", &str))
        {
            if (j >= num)
            {
                logwarn("array of values contains more than expected amount");
                g_free(str);
                break;
            }

            logdebug("  %s;\n", str);
            opt->supported_values[j] = g_strdup(str);
            j++;
        }
        g_hash_table_insert(options->table, g_strdup(opt->option_name), opt);
        i++;
    }
    g_variant_iter_free(iter);
    
    options->media_count = num_media;
    g_variant_get(media_var, "a(siiia(iiii))", &iter);
    i = 0;
    while (g_variant_iter_loop(iter, "(siiia(iiii))",
                               &name, &width, &length, &num, &sub_iter))
    {
        if (i >= num_media)
        {
            logwarn("array of media contains more than expected amount");
            g_free(name);
            g_variant_iter_free(sub_iter);
            break;
        }

        media = g_new0(cpdb_media_t, 1);
        logdebug("name=%s;\n", name);
        media->name = g_strdup(name);
        logdebug("width=%d;\n", width);
        media->width = width;
        logdebug("length=%d;\n", length);
        media->length = length;
        logdebug("num_margins=%d;\n", num);
        media->num_margins = num;
        media->margins = g_new0(cpdb_margin_t, num);

        j = 0;
        while (g_variant_iter_loop(sub_iter, "(iiii)", &l, &r, &t, &b))
        {
            if (j >= num)
            {
                logwarn("array of margins contains more than expected amount");
                break;
            }

            logdebug("    %d,%d,%d,%d;\n", l, r, t, b);
            media->margins[j].left = l;
            media->margins[j].right = r;
            media->margins[j].top = t; 
            media->margins[j].bottom = b;
            j++;
        }
        g_hash_table_insert(options->media, g_strdup(media->name), media);
        i++;
    }
    g_variant_iter_free(iter);
}

static GHashTable *cpdbUnpackTranslations (GVariant *variant)
{
    GVariantIter iter;
    gchar *key, *value;
    GHashTable *translations;

    translations = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_variant_iter_init(&iter, variant);
    while (g_variant_iter_loop(&iter, CPDB_TL_ARGS, &key, &value))
    {
        logdebug("Fetched translation '%s' : '%s'\n", key, value);
        g_hash_table_insert(translations,
                            g_strdup(key), g_strdup(value));
    }

    return translations;
}


/************************************************************************************************/
