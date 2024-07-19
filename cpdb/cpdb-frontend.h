#ifndef _CPDB_CPDB_FRONTEND_H_
#define _CPDB_CPDB_FRONTEND_H_

#include <glib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include <cpdb/cpdb.h>

#define CPDB_DIALOG_BUS_NAME "org.openprinting.PrintFrontend"
#define CPDB_DIALOG_OBJ_PATH "/"
#define CPDB_BACKEND_PREFIX  "org.openprinting.Backend."

/* Names of default config files */
#define CPDB_PRINT_SETTINGS_FILE   "print-settings"
#define CPDB_DEFAULT_PRINTERS_FILE "default-printers"

/* Debug macros */
#define logdebug(...) cpdbFDebugPrintf(CPDB_DEBUG_LEVEL_DEBUG, __VA_ARGS__)
#define loginfo(...)  cpdbFDebugPrintf(CPDB_DEBUG_LEVEL_INFO, __VA_ARGS__)
#define logwarn(...)  cpdbFDebugPrintf(CPDB_DEBUG_LEVEL_WARN, __VA_ARGS__)
#define logerror(...) cpdbFDebugPrintf(CPDB_DEBUG_LEVEL_ERROR, __VA_ARGS__)

typedef struct cpdb_frontend_obj_s cpdb_frontend_obj_t;
typedef struct cpdb_printer_obj_s cpdb_printer_obj_t;
typedef struct cpdb_settings_s cpdb_settings_t;
typedef struct cpdb_options_s cpdb_options_t;
typedef struct cpdb_option_s cpdb_option_t;
typedef struct cpdb_margin_s cpdb_margin_t;
typedef struct cpdb_media_s cpdb_media_t;
typedef struct cpdb_job_s cpdb_job_t;

typedef enum cpdb_printer_update_e {
    CPDB_CHANGE_PRINTER_ADDED,
    CPDB_CHANGE_PRINTER_REMOVED,
    CPDB_CHANGE_PRINTER_STATE_CHANGED,
} cpdb_printer_update_t;

/**
 * Callback for printer updates
 * 
 * @param frontend_obj      Frontend instance
 * @param printer_obj       Printer object updated
 * @param update            Type of update
 */
typedef void (*cpdb_printer_callback)(cpdb_frontend_obj_t *frontend_obj, cpdb_printer_obj_t *printer_obj, cpdb_printer_update_t update);

/**
 * Callback for async functions
 *
 * @param printer_obj       Printer object
 * @param status	        Success status
 * @param user_data 	    User data
 */
typedef void (*cpdb_async_callback)(cpdb_printer_obj_t *printer_obj, int status, void *user_data);

/*********************definitions ***************************/

/**
______________________________________ cpdb_frontend_obj_t __________________________________________

**/

struct cpdb_frontend_obj_s
{
    GDBusConnection *connection;

    cpdb_printer_callback printer_cb;

    int num_backends;
    GHashTable *backend; /**[backend name(like "CUPS" or "GCP")] ---> [BackendObj]**/

    int num_printers;
    GHashTable *printer; /**[printer name] --> [cpdb_printer_obj_t] **/

    gboolean hide_remote;
    gboolean hide_temporary;
    gboolean stop_flag;

    cpdb_settings_t *last_saved_settings; /** The last saved settings to disk */

    GThread *background_thread;
};

/**
 * Get a new cpdb_frontend_obj_t instance.
 * 
 * @param instance_name     Unique name for the frontend object, can be NULL
 * @param printer_cb        Callback function for any printer updates
 * @param change            Type of update
 * 
 * @return                  Frontend instance
 */
cpdb_frontend_obj_t *cpdbGetNewFrontendObj(cpdb_printer_callback printer_cb);

/**
 * Free up a frontend instance.
 * 
 * @param frontend_obj      Frontend instance to be deleted
 */
void cpdbDeleteFrontendObj(cpdb_frontend_obj_t *frontend_obj);

/**
 * Connect to DBus, activate the CPDB backends and fetch printers.
 * 
 * @param frontend_obj      Frontend instance to connect to DBus
 */
void cpdbConnectToDBus(cpdb_frontend_obj_t *frontend_obj);

GDBusConnection *cpdbGetDbusConnection();

/**
 * Disconnect from the DBus.
 * 
 * Use cpdbDeleteFrontendObj() directly instead
 * which internally calls this function before deallocating the instance
 * 
 * @param frontend_obj      Frontend instance connected to DBus
 */
void cpdbDisconnectFromDBus(cpdb_frontend_obj_t *frontend_obj);

/**
 * The default behaviour of cpdb_frontend_obj_t is to use the
 * settings previously saved to disk the last time any print dialog ran.
 *
 * To ignore the last saved settings, you need to explicitly call this function
 * after cpdbGetNewFrontendObj()
 * 
 * @param frontend_obj      Frontend instance
 */
void cpdbIgnoreLastSavedSettings(cpdb_frontend_obj_t *frontend_obj);

/**
 * Add the printer to the frontend instance
 * 
 * @param frontend_obj      Frontend instance
 * @param printer_obj       Printer object
 * 
 * @return                  Success status
 */
gboolean cpdbAddPrinter(cpdb_frontend_obj_t *frontend_obj, cpdb_printer_obj_t *printer_obj);

/**
 * Remove the printer from the frontend instance.
 *
 * @param frontend_obj      Frontend instance
 * @param printer_id        Printer ID
 * @param backend_name      Backend name
 * 
 * @return
 * The printer object corresponding to the printer just removed,
 * or NULL if the removal was unsuccesful.
 *
 * The caller is responsible for deallocating the printer object.
 */
cpdb_printer_obj_t *cpdbRemovePrinter(cpdb_frontend_obj_t *f, const char *printer_id, const char *backend_name);


/**
 * Hashtable lookup for hiding remote printers.
 * 
 * @param key              Key for the lookup
 * @param value            Value for the lookup
 * @param user_data        User data
 */
void hideRemoteLookup(gpointer key, gpointer value, gpointer user_data);

/**
 * Hashtable lookup for showing remote printers.
 * 
 * @param key              Key for the lookup
 * @param value            Value for the lookup
 * @param user_data        User data
 */
void showRemoteLookup(gpointer key, gpointer value, gpointer user_data);

/**
 * Hashtable lookup for hiding temporary printers.
 * 
 * @param key              Key for the lookup
 * @param value            Value for the lookup
 * @param user_data        User data
 */
void hideTemporaryLookup(gpointer key, gpointer value, gpointer user_data);

/**
 * Hashtable lookup for showing temporary printers.
 * 
 * @param key              Key for the lookup
 * @param value            Value for the lookup
 * @param user_data        User data
 */
void showTemporaryLookup(gpointer key, gpointer value, gpointer user_data);

/**
 * Hashtable lookup to stop listing printers.
 * 
 * @param key              Key for the lookup
 * @param value            Value for the lookup
 * @param user_data        User data
 */
void stopListingLookup(gpointer key, gpointer value, gpointer user_data);

/**
 * Hashtable lookup to get all printers.
 * 
 * @param key              Key for the lookup
 * @param value            Value for the lookup
 * @param user_data        User data
 */
void getAllPrintersLookup(gpointer key, gpointer value, gpointer user_data);

/**
 * Get all printers associated with the frontend object.
 * 
 * @param f                Frontend instance
 */
void cpdbGetAllPrinters(cpdb_frontend_obj_t *f);

/**
 * Print basic options for the printer object.
 * 
 * @param p                Printer object
 */
void cpdbPrintBasicOptions(const cpdb_printer_obj_t *p);

/**
 * Activate backends associated with the frontend object.
 * 
 * @param f                Frontend instance
 */
void cpdbActivateBackends(cpdb_frontend_obj_t *f);

/**
 * Start the background thread for refreshing the backend list.
 * 
 * @param f                Frontend instance
 */
void cpdbStartBackendListRefreshing(cpdb_frontend_obj_t *f);

/**
 * Stop the background thread for refreshing the backend list
 * 
 * @param f                Frontend instance
 */
void cpdbStopBackendListRefreshing(cpdb_frontend_obj_t *f);

/**
 * Start listing printers of the backend.
 * 
 * @param printer_cb       Callback function to be called when printers are listed
 * 
 * @return                 Frontend instance for the printer listing
 */
cpdb_frontend_obj_t *cpdbStartListingPrinters(cpdb_printer_callback printer_cb);

/**
 * Stop listing printers associated with the frontend object.
 * 
 * @param f                Frontend instance
 */
void cpdbStopListingPrinters(cpdb_frontend_obj_t *f);

/**
 * Hide the remote printers of the backend.
 * 
 * @param frontend_obj      Frontend instance
 */
void cpdbHideRemotePrinters(cpdb_frontend_obj_t *f);

/**
 * Unhide the remote printers of the backend.
 * 
 * @param frontend_obj      Frontend instance
 */
void cpdbUnhideRemotePrinters(cpdb_frontend_obj_t *f);

/**
 * Hide those (temporary) printers which have been discovered by the backend,
 * but haven't been yet set up locally.
 * 
 * @param frontend_obj      Frontend instance
 */
void cpdbHideTemporaryPrinters(cpdb_frontend_obj_t *f);

/**
 * Unhide those (temporary) printers which have been discovered by the backend,
 * but haven't been yet set up locally.
 * 
 * @param frontend_obj      Frontend instance
 */
void cpdbUnhideTemporaryPrinters(cpdb_frontend_obj_t *f);

/**
 * Read the file installed by the backend and create a proxy object
 * on the connection using the backend service name and object path
 */
PrintBackend *cpdbCreateBackend(GDBusConnection *, const char *);

/**
 * Find the cpdb_printer_obj_t instance with a particular id and backend name.
 * 
 * @param frontend_obj      Frontend instance
 * @param printer_id        Printer ID
 * @param backend_name      Backend name
 * 
 * @return                  Printer object if found, NULL otherwise.
 */
cpdb_printer_obj_t *cpdbFindPrinterObj(cpdb_frontend_obj_t *frontend_obj, const char *printer_id, const char *backend_name);

/**
 * Get the default printer for a particular CPDB backend.
 * 
 * @param frontend_obj      Frontend instance
 * @param backend_name      Backend name
 * 
 * @return                  Default printer for backend if found, NULL otherwise
 */
cpdb_printer_obj_t *cpdbGetDefaultPrinterForBackend(cpdb_frontend_obj_t *frontend_obj, const char *backend_name);

/**
 * Get the most preferred default printer.
 * Always returns a printer, unless there are no printers connected to the frontend.
 *
 * @param frontend_obj      Frontend instance
 * 
 * @return                  Default printer if any exists, NULL otherwise
 */
cpdb_printer_obj_t *cpdbGetDefaultPrinter(cpdb_frontend_obj_t *frontend_obj);

/**
 * Set a printer as user default.
 *
 * @param printer_obj       Printer object
 * 
 * @return                  TRUE on success, FALSE on failure
 */
gboolean cpdbSetUserDefaultPrinter(cpdb_printer_obj_t *p);

/**
 * Set a printer as system wide default.
 *
 * @param printer_obj       Printer object
 * 
 * @return                  TRUE on success, FALSE on failure
 */
gboolean cpdbSetSystemDefaultPrinter(cpdb_printer_obj_t *p);

/*******************************************************************************************/

/**
______________________________________ cpdb_printer_obj_t __________________________________________

**/
struct cpdb_printer_obj_s
{
    PrintBackend *backend_proxy; /** The proxy object of the backend the printer is associated with **/
    char *backend_name;          /** Backend name ,("CUPS"/ "GCP") also used as suffix */

    /**The basic attributes first**/

    char *id;
    char *name;
    char *location;
    char *info;
    char *make_and_model;
    char *state;
    gboolean accepting_jobs;

    /** The more advanced options we get from the backend **/
    cpdb_options_t *options;

    /**The settings the user selects, and which will be used for printing the job**/
    cpdb_settings_t *settings;

    /** Translations **/
    char *locale;
    GHashTable *translations;
};

/**
 * Get a new empty printer object.
 * 
 * @return                  Printer object
 */
cpdb_printer_obj_t *cpdbGetNewPrinterObj();

/**
 * Free up a printer object.
 * 
 * @param printer_obj       Printer object
 */
void cpdbDeletePrinterObj(cpdb_printer_obj_t *printer_obj);

/**
 * Print basic printer info to debug logs.
 * 
 * @param printer_obj       Printer object
 */
void cpdbDebugPrinter(const cpdb_printer_obj_t *printer_obj);

/**
 * Update and return whether printer is accepting jobs.
 * 
 * @param printer_obj       Printer object
 * 
 * @return                  TRUE if printer is accepting jobs, otherwise FALSE
 */
gboolean cpdbIsAcceptingJobs(cpdb_printer_obj_t *printer_obj);

/**
 * Update and return printer status.
 * 
 * @param printer_obj       Printer object
 * 
 * @return                  Printer status
 */
char *cpdbGetState(cpdb_printer_obj_t *printer_obj);

/**
 * Get all the different options and values supported by a printer.
 * 
 * @param printer_obj       Printer object
 * 
 * @return                  Options struct
 */
cpdb_options_t *cpdbGetAllOptions(cpdb_printer_obj_t *printer_obj);

/**
 * Get a single cpdb_option_t struct corresponding to an option name for a printer.
 *
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * 
 * @return                  Option struct if it exists, otherwise NULL
 */
cpdb_option_t *cpdbGetOption(cpdb_printer_obj_t *printer_obj, const char *option_name);

/**
 * Get the default option value for a printer.
 *
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * 
 * @return                  Default value if exists.
 *                          "NA" if option is present, but default value isn't set.
 *                          NULL if option doesn't exist.
 */
char *cpdbGetDefault(cpdb_printer_obj_t *p, const char *option_name);

/**
 * Get the option value set for a printer.
 * 
 * @param printer_obj       Printer object
 * @param option_name       Option name
 *
 * @return                  Value if set, NULL otherwise
 */
char *cpdbGetSetting(cpdb_printer_obj_t *p, const char *option_name);

/**
 * Get the option value set for a printer. If not set, get the default option value.
 *
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * 
 * @return                  Current value for an option
 */
char *cpdbGetCurrent(cpdb_printer_obj_t *printer_obj, const char *option_name);

/**
 * Submit a file for printing, using the settings set previously.
 * 
 * @param printer_obj       Printer object
 * @param file_path         Path of file to print
 * 
 * @return                  Job ID if created, NULL otherwise
 */
char *cpdbPrintFile(cpdb_printer_obj_t *printer_obj, const char *file_path);

/**
 * Submit a file for printing with a job title, using the settings set previously.
 * 
 * @param p                Printer object
 * @param file_path        Path of file to print
 * @param title            Job title
 * 
 * @return                 Job ID if created, NULL otherwise
 */
char *cpdbPrintFileWithJobTitle(cpdb_printer_obj_t *p, const char *file_path, const char *title);

/**
 * Print using a file descriptor, using the settings set previously.
 * 
 * @param p                Printer object
 * @param jobid            Job ID
 * @param title            Job title
 * @param socket_path      Socket path
 * 
 * @return                 File descriptor
 */
int cpdbPrintFD(cpdb_printer_obj_t *p, char **jobid, const char *title, char **socket_path);

/**
 * Print to a socket, using the settings set previously.
 * 
 * @param p                Printer object
 * @param jobid            Job ID
 * @param title            Job title
 * 
 * @return                 Socket if created, NULL otherwise
 */
char *cpdbPrintSocket(cpdb_printer_obj_t *p, char **jobid, const char *title);

/**
 * Refresh the printer list for a specific backend.
 * 
 * @param f                Frontend instance
 * @param backend          Backend name
 * 
 * @return                 True if the refresh was successful, false otherwise
 */
bool cpdbRefreshPrinterList(cpdb_frontend_obj_t *f, const char *backend);

/**
 * Callback function for printer events.
 * 
 * @param f                Frontend instance
 * @param p                Printer object
 * @param change           Type of change (added, removed, state changed)
 */
void cpdbPrinterCallback(cpdb_frontend_obj_t *f, cpdb_printer_obj_t *p, cpdb_printer_update_t change);

/**
 * Callback function for when a printer is added.
 * 
 * @param connection       DBus connection
 * @param sender_name      Sender name
 * @param object_path      Object path
 * @param interface_name   Interface name
 * @param signal_name      Signal name
 * @param parameters       Signal parameters
 * @param user_data        User data
 */
void cpdbOnPrinterAdded(GDBusConnection *connection, const gchar *sender_name,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *signal_name, GVariant *parameters,
                        gpointer user_data);

/**
 * Callback function for when a printer is removed.
 * 
 * @param connection       DBus connection
 * @param sender_name      Sender name
 * @param object_path      Object path
 * @param interface_name   Interface name
 * @param signal_name      Signal name
 * @param parameters       Signal parameters
 * @param user_data        User data
 */
void cpdbOnPrinterRemoved(GDBusConnection *connection, const gchar *sender_name,
                          const gchar *object_path, const gchar *interface_name,
                          const gchar *signal_name, GVariant *parameters,
                          gpointer user_data);

/**
 * Callback function for when a printer's state changes.
 * 
 * @param connection       DBus connection
 * @param sender_name      Sender name
 * @param object_path      Object path
 * @param interface_name   Interface name
 * @param signal_name      Signal name
 * @param parameters       Signal parameters
 * @param user_data        User data
 */
void cpdbOnPrinterStateChanged(GDBusConnection *connection, const gchar *sender_name,
                               const gchar *object_path, const gchar *interface_name,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data);

/**
 * Fill basic options for a printer from a GVariant.
 * 
 * @param p                Printer object
 * @param gv               GVariant containing the options
 */
void cpdbFillBasicOptions(cpdb_printer_obj_t *p, GVariant *gv);

/**
 * Set an option value for a printer.
 * Updates the value if one is already set.
 *
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * @param value             Value to set
 */
void cpdbAddSettingToPrinter(cpdb_printer_obj_t *printer_obj, const char *option_name, const char *value);

/**
 * Clear the option value set for a printer.
 *
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * 
 * @return                  TRUE if set value successfully cleared, FALSE otherwise
 */
gboolean cpdbClearSettingFromPrinter(cpdb_printer_obj_t *printer_obj, const char *option_name);

/**
 * Serialize the cpdb_printer_obj_t and save it to a file
 * This also keeps the respective backend of the printer alive.
 *
 * This cpdb_printer_obj_t* can then be resurrecuted from the file using the
 * cpdbResurrectPrinterFromFile() function.
 * 
 * @param printer_obj       Printer object
 * @param file              File path to save to
 * @param frontend_obj      Frontend instance
 */
void cpdbPicklePrinterToFile(cpdb_printer_obj_t *p, const char *file, const cpdb_frontend_obj_t *frontend_obj);

/**
 * Recreates a cpdb_printer_obj_t from its serialized form stored in the given format
 * and returns it.
 *
 * @param file              File path to read from
 * 
 * @return                  Printer object if deserialization was succesfull, NULL otherwise
 */
cpdb_printer_obj_t *cpdbResurrectPrinterFromFile(const char *file);

/**
 * Get the translation for an option name provided by a printer.
 * 
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * @param lang              BCP47 language tag to be used for translation
 * 
 * @return                  Translated name
 */
char *cpdbGetOptionTranslation(cpdb_printer_obj_t *printer_obj, const char *option_name, const char *lang);

/**
 * Get the translation for an option value provided by a printer.
 * 
 * @param printer_obj       Printer object
 * @param option_name       Option name
 * @param choice_name       Option value
 * @param lang              BCP47 language tag to be used for translation
 * 
 * @return                  Translated value
 */
char *cpdbGetChoiceTranslation(cpdb_printer_obj_t *printer_obj, const char *option_name, const char *choice_name, const char *lang);

/**
 * Get the translation for an option group provided by a printer.
 * 
 * @param printer_obj       Printer object
 * @param group_name        Group name
 * @param lang              BCP47 language tag to be used for translation
 * 
 * @return                  Translated name
 */
char *cpdbGetGroupTranslation(cpdb_printer_obj_t *printer_obj, const char *group_name, const char *lang);


/**
 * Get translations for all strings provided by a printer.
 *
 * @param printer_obj       Printer object
 * @param lang              BCP47 language tag to be used for translation
 */
void cpdbGetAllTranslations(cpdb_printer_obj_t *printer_obj, const char *lang);

/**
 * Get the cpdb_media_t struct corresponding to a media-size supported by a printer.
 *
 * @param printer_obj       Printer object
 * @param media_name        Media-size name
 * 
 * @return                  Media struct if media-size exists, NULL otherwise
 */
cpdb_media_t *cpdbGetMedia(cpdb_printer_obj_t *printer_obj, const char *media_name);

/**
 * Get the dimensions of a media-size supported by a printer.
 *
 * @param printer_obj       Printer object
 * @param media_name        Media-size name
 * @param width             Address for storing media width
 * @param length            Address for storing media length
 *
 * @return                  1 if media-size exists, 0 otherwise
 */
int cpdbGetMediaSize(cpdb_printer_obj_t *printer_obj, const char *media_name, int *width, int *length);

/**
 * Get the media margins for a media-size supported by a printer.
 * 
 * @param printer_obj       Printer object
 * @param media_name        Media-size name
 * @param margins           Address for storing margins array
 *
 * @returns                 Number of margins supported
 */
int cpdbGetMediaMargins(cpdb_printer_obj_t *printer_obj, const char *media_name, cpdb_margin_t **margins);

/**
 * Asynchronously fetch printer details and options.
 *
 * @param printer_obj       Printer object
 * @param caller_cb         Callback function
 * @param user_data         User data to pass to callback function
 * 
 */
void cpdbAcquireDetails(cpdb_printer_obj_t *printer_obj, cpdb_async_callback caller_cb, void *user_data);

/**
 * Asynchronously fetch all printer strings translations,
 * which can then be obtained using cpdbGet[...]Translation() functions.
 * For synchronous version, look at cpdbGetAllTranslations().
 *
 * @param printer_obj       Printer object
 * @param lang              BCP47 language tag to be used for translation
 * @param caller_cb         Callback function
 * @param user_data         User data to pass to callback functions
 */
void cpdbAcquireTranslations(cpdb_printer_obj_t *printer_obj, const char *lang, cpdb_async_callback caller_cb, void *user_data);

/************************************************************************************************/
/**
______________________________________ cpdb_settings_t __________________________________________

**/

/**
 * Takes care of the settings the user sets with the help of the dialog.
 * These settings will be used when sending a print job
 */
struct cpdb_settings_s
{
    int count;
    GHashTable *table; /** [name] --> [value] **/
    // planned functions:
    //  serialize settings into a GVariant of type a(ss)
};

/**
 * Get a new empty settings object.
 * 
 * @return                  Settings object
 */
cpdb_settings_t *cpdbGetNewSettings();

/**
 * Copy settings from source to destination.
 * The previous values in dest will be overwritten.
 * 
 * @param source            Source settings
 * @param dest              Destination settings
 */
void cpdbCopySettings(const cpdb_settings_t *source, cpdb_settings_t *dest);

/**
 * Add the particular 'setting' to the settings object.
 * If the setting already exists, its value is updated instead.
 *
 * @param setting_obj       Settings object
 * @param option_name       Option name
 * @param value             Option value
 */
void cpdbAddSetting(cpdb_settings_t *setting_obj, const char *option_name, const char *value);

/**
 * Clear the setting specified by @name
 *
 * @return                  TRUE , if the setting was cleared
 *                          FALSE , if the setting wasn't there and thus couldn't be cleared
 */
gboolean cpdbClearSetting(cpdb_settings_t *, const char *name);

/**
 * Serialize the cpdb_settings_t struct into a GVariant of type a(ss)
 * so that it can be sent as an argument over D-Bus
 */
GVariant *cpdbSerializeToGVariant(cpdb_settings_t *s);

/**
 * Save the settings to disk,
 * i.e write them to CPDB_PRINT_SETTINGS_FILE
 * 
 * @param settings_obj      Settings object
 */
void cpdbSaveSettingsToDisk(cpdb_settings_t *settings_obj);

/**
 * Reads the serialized settings stored in
 * CPDB_PRINT_SETTINGS_FILE and creates a cpdb_settings_t* struct from it.
 * The caller is responsible for freeing the returned settings object.
 * 
 * @return                  Settings object
 */
cpdb_settings_t *cpdbReadSettingsFromDisk();

/**
 * Free up a settings object.
 * 
 * @param settings_obj      Settings object
 */
void cpdbDeleteSettings(cpdb_settings_t *settings_obj);

/************************************************************************************************/
/**
______________________________________ cpdb_options_t __________________________________________

**/
struct cpdb_options_s
{
    int count;
    int media_count;
    GHashTable *table; /**[name] --> cpdb_option_t struct**/
    GHashTable *media; /**[name] --> cpdb_media_t struct**/
};

/**
 * Get an empty cpdb_options_t struct with no 'options' in it
 * 
 * @return                  Options object
 */
cpdb_options_t *cpdbGetNewOptions();

/**
 * Free up an options object.
 * 
 * @param options           Options object
 */
void cpdbDeleteOptions(cpdb_options_t *);

/************************************************************************************************/
/**
______________________________________ cpdb_option_t __________________________________________

**/
struct cpdb_option_s
{
    char *option_name;
    char *group_name;
    int num_supported;
    char **supported_values;
    char *default_value;
};

/**
 * @param opt               Option object
 */
void cpdbDeleteOption(cpdb_option_t *);

/************************************************************************************************/

/**
______________________________________ cpdb_margin_t __________________________________________

**/

struct cpdb_margin_s
{
    int left;
    int right;
    int top;
    int bottom;
};

/************************************************************************************************/
/**
______________________________________ cpdb_media_t __________________________________________

**/

struct cpdb_media_s
{
    char *name;
    int width;
    int length;
    int num_margins;
    cpdb_margin_t *margins;
};

/**
 * Free up a media-size object.
 * 
 * @param media             Media-size object
 */
void cpdbDeleteMedia(cpdb_media_t *media);

/************************************************************************************************/
/**
______________________________________ cpdb_job_t __________________________________________

**/
struct cpdb_job_s
{
    char *job_id;
    char *title;
    char *printer_id;
    char *backend_name;
    char *user;
    char *state;
    char *submitted_at;
    int size;
};

#ifdef __cplusplus
}
#endif

#endif /* !_CPDB_CPDB_FRONTEND_H_ */
