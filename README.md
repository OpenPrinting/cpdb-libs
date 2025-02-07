# Frontend/Backend Communication Libraries for the Common Print Dialog Backends

This repository hosts the code for frontend and backend libraries for the Common Printing Dialog Backends (CPDB) project. These libraries allow the CPDB frontends (the print dialogs) and backends (the modules communicating with the different printing systems) to communicate with each other via D-Bus.

The frontend library also provides some extra functionality to deal with Printers, Settings, etc. in a high level manner.

## Background

The Common Print Dialog Backends (CPDB) project of OpenPrinting is about separating the print dialogs of different GUI toolkits and applications (GTK, Qt, LibreOffice, Firefox, Chromium, ...) from the different print technologies (CUPS/IPP, cloud printing services, ...) so that they can get developed independently and so always from all applications one can print with all print technologies and changes in the print technologies get supported quickly.

If one opens the print dialog, the dialog will not talk directly to CUPS, a cloud printing service, or any other printing system. For this communication there are the backends. The dialog will find all available backends and sends commands to them, for listing all available printers, giving property/option lists for the selected printer, and printing on the selected printer. This communication is done via D-Bus. So the backends are easily exchangeable and for getting support for a new print technology only its backend needs to get added.

## Dependencies

 - autoconf
 - autopoint
 - glib 2.0
 - libdbus
 - libtool

On Debian based distros, these can be installed by running: \
`sudo apt install autoconf autopoint libglib2.0-dev libdbus-1-dev libtool`

## Build and installation


    $ ./autogen.sh
    $ ./configure
    $ make
    $ sudo make install
    $ sudo ldconfig

Also install at least one of the backends (cpdb-backend-...).

NOTE: The communication protocol between frontends and backends has changed (Job data streaming via domain socket, printer list filteringvia D-Bus methods). Therefore use backends of at least version 2.0b6.

## Testing the library

The project also includes a sample command line frontend (using the `cpdb-libs-frontend` API) that you can use to test whether the installed libraries and print backends work as expected.

    $ cpdb-text-frontend

The list of printers from various print technologies should start appearing automatically. Type `help` to get the list of available commands. Make sure to stop the frontend using the `stop` command only.

The library also provides support for serializing a printer. Use the `pickle-printer` command to serialize it, and run the `cpdb-pickle-print` executable after that to deserialize and test it.


## Using the libraries for developing print backends and dialogs.

To develop a frontend client (eg. a print dialog), use the `libcpdb` and `libcpdb-frontend` libraries.

pkg-config support: `pkg-config --cflags --libs cpdb` and `pkg-config --cflags --libs cpdb-frontend`.
Header file: `cpdb/frontend.h`

Similarly, to develop a print backend, you need to use the only the `libcpdb` library.
pkg-config support: `pkg-config --cflags --libs cpdb`.
Header file: `cpdb/backend.h` or simply only `cpdb/cpdb.h`.

## How does it all work internally?

For integration in real life environments, especially if sandboxed packaging (like Snap or OCI containers) or other local access and interaction restrictions are involved, and also for making contributions like bug fixes or general code additions, you should know how CPDB internally works and for what the functions in the libraries are good for.

Therefore we will show here the workflow of using CPDB in a print dialog and which functions in frontend and backend get called.

The steps and function calls are principally the same for all print dialogs and also for the example/development/debugging text mode frontend `tools/cpdb-text-frontend.c`. Therefore we go through the latter here.

Note that all CPDB backends have exactly the same D-Bus interface. Same name, same methods, same signals, ... They only internally communicate with different print environments (CUPS, file, cloud printing, ...). There can be available any amount of backends at the same time. They are registered as D-Bus services and therefore they get started when accessing them. Any number of them can run at the same time.

There can also be any amount of frontends (print dialogs) be running at the same time and using the backends. Each backend keeps track of which frontend processes/instances are currently accessing (and serves out printer list updates to them), holds settings for each frontend, and shuts down when no accessing frontends are left.

The backends are user daemons communicating via the session bus, so they are independently running for each user.

The frontends contain nothing specific to the print environments (like CUPS), all that is in the backends.

In the following explanations (and also in the full source code) pay special attention to functions with names `g_dbus_...()` which are D-Bus library functions doing D-Bus operations and `print_backend_...()` which are calls of methods of the backend's D-Bus interface. The latter are auto-generated from the D-Bus interface definition XML file `cpdb/interface/org.openprinting.Backend.xml`.

**Finding all the running CPDB backends and listing all available printers**

Let us see the `main()` function of `tools/cpdb-text-frontend.c`:

```
int main(int argc, char **argv)
{
```

Here we define a callback function which is called when a printer appears, disappears, or changes in the list of available printers. The default function we use here is for just logging the fact. In an actual GUI dialog it can also be used to update the printer list in the GUI.
```
    cpdb_printer_callback printer_cb = (cpdb_printer_callback)cpdbPrinterCallback;

    setlocale (LC_ALL, "");
```

Initialization of CPDB. It only stes up the text domain for localization, no D-Bus magic yet:
```
    cpdbInit();
```

Create the frontend object. This fills in the data structure for the frontend. The structure is holding the lists of available backends and available printers, and also the user settings, also no D-Bus magic here:
```
    f = cpdbGetNewFrontendObj(printer_cb);
```

We call `cpdbIgnoreLastSavedSettings(f);` only in applications which serve for testing, to assure defined starting conditions. In actual production applications, as print dialogs, we do not do this call.
```
    /** Uncomment the line below if you don't want to use the previously saved settings**/
    cpdbIgnoreLastSavedSettings(f);
```

As this text mode app is interactive, we spawn a sub-thread to handle the user commands:
```
    // Start the control thread
    GThread *thread = g_thread_new("control_thread", control_thread, NULL);
```

Now we kick of the listing of printers, this function starts to look up available backends via D-Bus and adds them to a list for querying, then queries them for an initial list of available printers and then keeps listening for signals about new, removed, and changed printers from the backends, so that callback functions can update the list of available printers and trigger refreshes of the frontend UI.
```
    cpdbStartBackendListRefreshing(f);
```

Now the background threads for printer list updating and user interaction are running, this is also the case when we have a print dialog open and see the list of available printers in it. When the user gives the `stop` command to quit the program, the user interaction thread ends and the `g_thread_join(thread)` function call returns.
```
    g_thread_join(thread);
```

Now we clean up. First we stop listening for printer list updates and shut down all D-Bus connections, then we free up the frontend data structure:
```
    cpdbStopBackendListRefreshing(f);
    cpdbDeleteFrontendObj(f);

    return 0;
}
```

The sub-thread spawned to for user interaction, `control_thread()`, calls 

```
cpdbConnectToDBus(f);
```

in its beginning, before it starts taking commands from the user. This function establishes the D-Bus connection, discovers the backends, and polls each backend's available printers to make up the initial list.
```
void cpdbConnectToDBus(cpdb_frontend_obj_t *f)
{
    GError *error = NULL;
```

Here we create a D-Bus connection, on the session bus, as we are running the CPDB backends as user daemons (see below):
```
    if ((f->connection = cpdbGetDbusConnection()) == NULL)
    {
        loginfo("Couldn't connect to DBus\n");
        return;
    }
```

Here we subscribe to the signals from the backends, assigning callback functions to the signals identified by the sender interface name and the actual signal name. The sender, which is the CPDB backend, does not need to be running for that. We need just the correct names and all is registered locally, client-side. There can be even more than one sender with the same interface and signal names. As we do not specify the sender name, all senders are accepted.
```
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
```

With this call we will discover the backends and make them giving us the initial lists of available printers:
```
    cpdbActivateBackends(f); 
    
}
```

Here is how the `cpdbGetDbusConnection()` function creates our D-Bus connection to the session bus:
```
    bus_addr = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION,
                                               NULL,
                                               &error);
    
    connection = g_dbus_connection_new_for_address_sync(bus_addr,
                                                        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                        G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                        NULL,
                                                        NULL,
                                                        &error);
```

Now we have a look at the `cpdbActivateBackends()` function to see how the backends get discovered. Note that we cannot just look into the file system which backends are available, as we and the backend could be in separate containers.
```
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
```

We need to poll the D-Bus twice to get the full list of backends, once, we poll the D-Bus services (all services, not only CPDB backends) which are actually running and second, we poll services which are not running but registered for being started on-demand.
```
    const char * const name_lists[] = {
        "ListNames",
        "ListActivatableNames",
        NULL
    };
```

We create a copy of the hash table of the backends we had discovered previously (is empty on the first call of `cpdbActivateBackends()`). Every backend which we discover again now we will remove from the list. This way the disappeared backends remain and we can remove them from our hash table in the frontend data structure.
```
    // Create a hash table to track existing backends
    existing_backends = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_foreach(f->backend, add_to_hash_table, existing_backends);
```

We create a D-Bus proxy to do the search for services:
```
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
```

Do the 2 searches to find all D-Bus services available on the session bus, once the running and once the registered but not running services:
```
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
```

Note that this list contains all D-Bus services available on the session bus, not only the CPDB backends.
```
        service_names = g_variant_get_child_value(service_names_tuple, 0);
```

We go through all service names in the list ...
```
        len = strlen(CPDB_BACKEND_PREFIX);
        g_variant_iter_init(&iter, service_names);
        while (g_variant_iter_next(&iter, "s", &service_name)) {
```

We consider only CPDB backends. Their names start with `org.openprinting.Backend.` (the value of the constant `CPDB_BACKEND_PREFIX`):
```
            if (g_str_has_prefix(service_name, CPDB_BACKEND_PREFIX)) {
                backend_suffix = g_strdup(service_name + len);
                if (!g_hash_table_lookup(f->backend, backend_suffix)) {
```

New backend found, log the fact and connect to it via D-Bus, getting a proxy. This connection also starts not yet running backends.
```
                    loginfo("Found backend %s (%s)\n", backend_suffix,
                            i ? "Starting now" : "Already running");
                    backend_proxy = cpdbCreateBackend(f->connection, service_name);
```

On success we add the new backend to the frontend's hash table and poll its printer list:
```
                    if (backend_proxy) {
                        g_hash_table_insert(f->backend, strdup(backend_suffix), backend_proxy);
                        f->num_backends++;
                        if (!g_hash_table_contains(existing_backends, backend_suffix)) {
                            fetchPrinterListFromBackend(f, backend_suffix);
                        }
                    }
                }
```

Remove the found backend from the copy of our backend hash table, so that only the disappeared backends remain:
```
                g_hash_table_remove(existing_backends, backend_suffix);
```

Clean up ...
```
                g_free(backend_suffix);
            }
            g_free(service_name);
        }

        g_variant_unref(service_names);
        g_variant_unref(service_names_tuple);
    }
```

Remove disappeared backends, according to the remaining entries in the copy of the backend table:
```
    // Remove backends that are no longer present
    g_hash_table_iter_init(&hash_iter, existing_backends);
    while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
        loginfo("Removing backend %s\n", (char *)key);
        g_hash_table_remove(f->backend, key);
    }
```

Clean up ...
```
    g_hash_table_destroy(existing_backends);
    g_object_unref(dbus_proxy);
```

Apply filters to optionally not show remote printers or temporary print queues:
```
    if (f->hide_remote) cpdbHideRemotePrinters(f);
    if (f->hide_temporary) cpdbHideTemporaryPrinters(f);
}
```

The individual D-Bus proxies to the backends are created with cpdbCreateBackend():
```
    proxy = print_backend_proxy_new_sync(connection,
                                         0,
                                         service_name,
                                         CPDB_BACKEND_OBJ_PATH,
                                         NULL,
                                         &error);
```

Up to now we have only discovered backends, now we will finally call a D-Bus method on the backends, at this time for getting their lists of available printers. `fetchPrinterListFromBackend()`
```
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
```

`print_backend_call_get_all_printers_sync()` is a C function auto-generated from the D-Bus interface definition XML file `cpdb/interface/org.openprinting.Backend.xml`. The auto-generated C code is landed in the file `cpdb/backend-interface.c`. This backend method returns the list of available printers. In case of the CUPS backend the information is obtained from CUPS.
```
    print_backend_call_get_all_printers_sync (proxy, &num_printers,
                                              &printers, NULL, &error);
    if (error)
    {
        logerror("Error getting %s printer list : %s\n", backend, error->message);
        return;
    }
```

Here we fill the list items into data structure and into the frontend's list of available printers.
```
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
```

Now we have the initial list of printers. `cpdbConnectToDBus()` returns, our text mode application displays the peinter list on the screen and waits for user commands. A print dialog would open its main window with the list of available printers now.

**Keeping the printer list up-to-date**

While or text mode application is running, or while a print dialog is open, printers can appear and disappear, or properties of them can change. This we reflect in the list of available printers in real time.

For that our backend D-Bus interface `cpdb/interface/org.openprinting.Backend.xml` defines some signals which the backends emit when a change in the printer list happens:
```
        <signal name="PrinterAdded">
            <arg name="printer_id" direction="out" type="s"/>
            <arg name="printer_name" direction="out" type="s" />
            <arg name="printer_info" direction="out" type="s" />
            <arg name="printer_location" direction="out" type="s" />
            <arg name="printer_make_and_model" direction="out" type="s" />
            <arg name="printer_is_accepting_jobs" direction="out" type="b" />
            <arg name="printer_state" direction="out" type="s" />
            <arg name="backend_name" direction="out" type="s"/>
        </signal>
        <signal name="PrinterRemoved">
            <arg name="printer_id" type="s" direction="out"/>
            <arg name="backend_name" type="s" direction="out"/>
        </signal>
        <signal name="PrinterStateChanged">
            <arg name="printer_id" type="s" direction="out"/>
            <arg name="printer_state" type="s" direction="out"/>
            <arg name="printer_is_accepting_jobs" type="b" direction="out"/>
            <arg name="backend_name" type="s" direction="out"/>
        </signal>
```
The signals provide all the information needed to be able to do the frontend's printer list update.

In the initial D-Bus connection setup we called the function `cpdbGetDbusConnection()` in which we subscribed to these three signals, assigning signal handler functions to them: `cpdbOnPrinterAdded()`, `cpdbOnPrinterRemoved()`, `cpdbOnPrinterStateChanged()`. These functions update the frontend's printer list based on the parameters they receive along with the signals, not needing to do any further D-Bus interaction.

**Sending a print job**

Initiating printing is just a D-Bus method call on the backend which provides the printer, the tricky part is transfering the job's print data.

We cannot just drop it into a (temporary) file and tell the file's path and name to the backend, as frontend and backend can reside in different containers/sandboxes and therefore not share a file system. We also cannot pump a stream of several megabytes of data through the D-Bus, as it is not designed for that.

What we do is setting up a Unix domain socket and stream the data through it.

Our text-mode application prints just local files specified by the user, a GUI-application's print dialog can also deliver print job data as a stream through a file descriptor. The CPDB API provides the functions `cpdbPrintFile()`, `cpdbPrintFileWithJobTitle()`, `cpdbPrintFD()`, and `cpdbPrintSocket()` (all in `cpdb/cpdb-frontend.c`) send print jobs to a given printer.

All these functions end up calling the `cpdbPrintSocket()` function which calls the backend's D-Bus method
```
print_backend_call_print_socket_sync()
```
supplying job-relevant metadata, as the title and the option settings, and receives a per-job Unix domain socket and a job ID from the method. Now, to complete the job one just feeds the job data stream into the socket. The backend gets it then without any filtering and passes it on to the printing system. By the domanin socket being per-job, there is no problem if several jobs get submitted in parallel (print environment queues them to print them one after the other in case they are for the same printer).

**Further oprations**

All the other operations (like setting options, find default printer, terminating the communication, ...) are just calls of D-Bus methods on the backend from which the printer to be accessed comes from. All needed onformation exchange is done by the parameters of the method call and the data which the method returns.

## Containerization/Sandboxing

For more convenience and security operating systems and applications make more and more use of containerization, sandboxed packaging. Applications are running in isolated containers/sanboxes and so they can communicate with each other and with the host system only through well-defined interfaces.

We also want to allow this for the Common Print Dialog Backends. It should be possible that applications with print functionality (frontends) can be in their container and each backend in another container. So we need to be able to communicate over container boundaries.

First we avoid using the local file system, for example we do not search the local file system for available backends nor drop we files there to get picked up by the backend in order to print them.

The frontend and backends, both running as the user who started the frontend, communicate by **D-Bus**, via the **session bus**. The backends are **user daemons**. They are run for each user individually.

The **frontend communicates with multiple backends, not knowing beforehand which backends are available**. Each backend has the **same D-Bus interface**, providing the same methods and signals with the same names.

**Multiple frontends can communicate with the same set of backends at the same time.** The running backends take note of each frontend currently communicating with them.

Backends are started on-demand, as soon as a frontend tries to access them. They terminate automatically if no frontend is communicating with them any more.

The frontend creates a proxy on the session D-Bus itself and asks for a list of all running D-Bus services and all D-Bus services available for on-demand launch:
```
    bus_addr = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION,
                                               NULL,
                                               &error);
    connection = g_dbus_connection_new_for_address_sync(bus_addr,
                                                        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                        G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                        NULL,
                                                        NULL,
                                                        &error);

    dbus_proxy = g_dbus_proxy_new_sync(f->connection,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       "org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       NULL,
                                       &error);

    const char * const name_lists[] = {
        "ListNames",
        "ListActivatableNames",
        NULL
    };
    for (i = 0; name_lists[i]; i++) {
        service_names_tuple = g_dbus_proxy_call_sync(dbus_proxy,
                                                     name_lists[i],
                                                     NULL,
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL,
                                                     &error);
    }
```

This gives a list of **all** D-Bus services on the session bus. We identify the CPDB backends by their service names starting with `org.openprinting.Backend.`. We call their D-Bus method to list the available printers which makes the not yet running ones getting started.

Create a proxy for each individual backend (specified by service name, like `org.openprinting.Backend.cups`) to talk with them:
```
    proxy = print_backend_proxy_new_sync(connection,
                                         0,
                                         service_name,
                                         CPDB_BACKEND_OBJ_PATH,
                                         NULL,
                                         &error);
```

Call D-Bus methods of the backends, here list all available printers:
```
print_backend_call_get_all_printers_sync (proxy, &num_printers,
                                          &printers, NULL, &error);

```

Subscribe to signals, here to add a newly discovered printer:
```
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
```

For printing we also **use domain sockets**, to transfer the job data. We obtain a per-job domain socket via D-Bus method call for actual printing:
```
    print_backend_call_print_socket_sync(p->backend_proxy,
                                         p->id,
                                         p->settings->count,
                                         cpdbSerializeToGVariant(p->settings),
                                         title,
                                         $jobid,
                                         &socket,
                                         NULL,
                                         &error);
```
To complete the job we only have to feed the job data into the socket and close the file in the end.

**So the frontend's containerization must allow a rather free operation on the session D-Bus, not only single-client-to-single-service connections. We especially need to access the D-Bus itself to list all services and then talk to freely chosen services where we do not know beforehand which ones.**

**The backend's containerization must allow D-Bus access to the service from multiple clients simultaneously, where it is not known beforehand which clients.**

**There is also a way of sharing domain sockets needed, as for example the CUPS Snap does iwth CUPS' socket, only that here we need to provide a directory for bind-mounting which can hold several sockets.**

**The frontend does not need to communicate with any of the print environments, like CUPS, and the backends only need to communicate with the print environment they are made for. Generally only user access and not admin access to the print environments is needed, so listing available printers, setting options, and printing, not creating queues, removing jobs, ...**
