#ifndef _CPDB_FRONTEND_H_
#define _CPDB_FRONTEND_H_

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <cpdb/cpdb.h>

#define CPDB_DEBUG_LEVEL_INFO 3
#define CPDB_DEBUG_LEVEL_WARN 2
#define CPDB_DEBUG_LEVEL_ERR 1

#define CPDB_DEBUG_LEVEL CPDB_DEBUG_LEVEL_INFO

#define CPDB_DIALOG_BUS_NAME "org.openprinting.PrintFrontend"
#define CPDB_DIALOG_OBJ_PATH "/"
#define CPDB_DBUS_DIR "/usr/share/print-backends"
#define CPDB_BACKEND_PREFIX "org.openprinting.Backend."
#define CPDB_SETTINGS_FILE "~/.CPD-print-settings"

typedef struct cpdb_frontend_obj_s cpdb_frontend_obj_t;
typedef struct cpdb_printer_obj_s cpdb_printer_obj_t;
typedef struct cpdb_async_obj_s cpdb_async_obj_t;
typedef struct cpdb_settings_s cpdb_settings_t;
typedef struct cpdb_options_s cpdb_options_t;
typedef struct cpdb_option_s cpdb_option_t;
typedef struct cpdb_margin_s cpdb_margin_t;
typedef struct cpdb_media_s cpdb_media_t;
typedef struct cpdb_job_s cpdb_job_t;

typedef int (*cpdb_event_callback)(cpdb_printer_obj_t *);

/**
 * Callback for async functions
 *
 * @param int 		: success
 * @param void * 	: user_data
 */
typedef void (*cpdb_async_callback)(cpdb_printer_obj_t *, int, void *);

/*********************definitions ***************************/

/**
______________________________________ cpdb_frontend_obj_t __________________________________________

**/

struct cpdb_frontend_obj_s
{
    PrintFrontend *skeleton;
    GDBusConnection *connection;

    char *bus_name;
    cpdb_event_callback add_cb;
    cpdb_event_callback rem_cb;

    int num_backends;
    GHashTable *backend; /**[backend name(like "CUPS" or "GCP")] ---> [BackendObj]**/

    int num_printers;
    GHashTable *printer; /**[printer name] --> [cpdb_printer_obj_t] **/

    cpdb_settings_t *last_saved_settings; /** The last saved settings to disk */
};

/**
 * Get a new cpdb_frontend_obj_t instance
 *
 * @params
 *
 * instance name: The suffix to be used for the dbus name for Frontend
 *              supply NULL for no suffix
 *
 * add_cb : The callback function to call when a new printer is added
 * rem_cb : The callback function to call when a printer is removed
 *
 */
cpdb_frontend_obj_t *cpdbGetNewFrontendObj(char *instance_name, cpdb_event_callback add_cb, cpdb_event_callback remove_cb);

/**
 * Start the frontend D-Bus Service
 */
void cpdbConnectToDBus(cpdb_frontend_obj_t *);

/**
 * Notify Backend services before stopping Frontend
 */
void cpdbDisconnectFromDBus(cpdb_frontend_obj_t *);

/**
 * Discover the currently installed backends and activate them
 *
 *
 * Reads the CPDB_DBUS_DIR folder to find the files installed by
 * the respective backends ,
 * For eg:  org.openprinting.Backend.XYZ
 *
 * XYZ = Backend suffix, using which it will be identified henceforth
 */
void cpdbActivateBackends(cpdb_frontend_obj_t *);

/**
 * The default behaviour of cpdb_frontend_obj_t is to use the
 * settings previously saved to disk the last time any print dialog ran.
 *
 * To ignore the last saved settings, you need to explicitly call this function
 * after cpdbGetNewFrontendObj
 */
void cpdbIgnoreLastSavedSettings(cpdb_frontend_obj_t *);

/**
 * Add the printer to the cpdb_frontend_obj_t instance
 */
gboolean cpdbAddPrinter(cpdb_frontend_obj_t *f, cpdb_printer_obj_t *p);

/**
 * Remove the printer from cpdb_frontend_obj_t
 *
 * @returns
 * The cpdb_printer_obj_t* struct corresponding to the printer just removed,
 * or NULL if the removal was unsuccesful
 *
 * The cpdb_printer_obj_t removed is not deallocated.
 * The caller is responsible for deallocation
 */
cpdb_printer_obj_t *cpdbRemovePrinter(cpdb_frontend_obj_t *f, const char *printer_id, const char *backend_name);
void cpdbRefreshPrinterList(cpdb_frontend_obj_t *f);

/**
 * Hide the remote printers of the backend
 */
void cpdbHideRemotePrinters(cpdb_frontend_obj_t *f);
void cpdbUnhideRemotePrinters(cpdb_frontend_obj_t *f);

/**
 * Hide those (temporary) printers which have been discovered by the backend,
 * but haven't been yet set up locally
 */
void cpdbHideTemporaryPrinters(cpdb_frontend_obj_t *f);
void cpdbUnhideTemporaryPrinters(cpdb_frontend_obj_t *f);

/**
 * Read the file installed by the backend and create a proxy object
 * using the backend service name and object path.
 */
PrintBackend *cpdbCreateBackendFromFile(const char *);

/**
 * Find the cpdb_printer_obj_t instance with a particular id ans backend name.
 */
cpdb_printer_obj_t *cpdbFindPrinterObj(cpdb_frontend_obj_t *, const char *printer_id, const char *backend_name);

/**
 * Get the default printer for a particular backend
 *
 * @param backend_name : The name of the backend
 *                          Can be just the suffix("CUPS")
 *                          or
 *                          the complete name ("org.openprinting.Backend.CUPS")
 */
char *cpdbGetDefaultPrinterForBackend(cpdb_frontend_obj_t *, const char *backend_name);

/**
 * Returns a GList of all default printers in given config file
 *
 * @param path : Relative path of the config file with default printers
 *
 */
GList *cpdbLoadDefaultPrinters(char *path);

/**
 * Get a single default printer
 * Always returns a printer, unless there are no printers connected to the frontend
 *
 */
cpdb_printer_obj_t *cpdbGetDefaultPrinter(cpdb_frontend_obj_t *);

/**
 * Set a printer as default in the given config file
 * 
 * @param path : Relative path of the config file with default printers
 * @param p : PrinterObj to mark as default
 *
 * @returns : 1 on success, 0 on failure
 */
int cpdbSetDefaultPrinter(char *path, cpdb_printer_obj_t *p);

/**
 * Set a printer as user default
 * Takes care of duplicate entries in the config file
 *
 * @returns : 1 on success, 0 on failure
 */
int cpdbSetUserDefaultPrinter(cpdb_printer_obj_t *p);

/**
 * Set a printer as system wide default
 * Takes care of duplicate entries in the config file
 *
 * @returns : 1 on success, 0 on failure
 */
int cpdbSetSystemDefaultPrinter(cpdb_printer_obj_t *p);

/**
 * Get the list of (all/active) jobs from all the backends
 *
 * @param j : pointer to a cpdb_job_t array; the retrieved job list array is stored at this location
 * @param active_only : when set to true , retrieves only the active jobs;
 *                      otherwise fetches all(active + completed + stopped) jobs
 *                      Retrieves jobs for all users.
 *
 * returns : number of jobs(i.e. length of the cpdb_job_t array)
 *
 */
int cpdbGetAllJobs(cpdb_frontend_obj_t *, cpdb_job_t **j, gboolean active_only);

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
    gboolean cpdbIsAcceptingJobs;

    /** The more advanced options we get from the backend **/
    cpdb_options_t *options;

    /**The settings the user selects, and which will be used for printing the job**/
    cpdb_settings_t *settings;
};

cpdb_printer_obj_t *cpdbGetNewPrinterObj();

/**
 * Fill the basic options of cpdb_printer_obj_t from the GVariant returned with the printerAdded signal
 */
void cpdbFillBasicOptions(cpdb_printer_obj_t *, GVariant *);

/**
 * Print the basic options of cpdb_printer_obj_t
 */
void cpdbPrintBasicOptions(cpdb_printer_obj_t *);
gboolean cpdbIsAcceptingJobs(cpdb_printer_obj_t *);
char *cpdbGetState(cpdb_printer_obj_t *);

/**
 * Get all the advanced supported options for the printer.
 * This function populates the 'options' variable of the cpdb_printer_obj_t structure,
 * and returns the same.
 *
 * If the options haven't been fetched before, they are fetched from the backend.
 * Else, they previously fetched 'options' are returned
 *
 * Each option has
 *  option name,
 *  default value,
 *  number of supported values,
 *  array of supported values
 */
cpdb_options_t *cpdbGetAllOptions(cpdb_printer_obj_t *);

/**
 * Get a single cpdb_option_t struct corresponding to a particular name.
 *
 * @returns
 * Option if the option was found
 * NULL if the option with desired name doesn't exist
 */
cpdb_option_t *cpdbGetOption(cpdb_printer_obj_t *p, const char *name);

/**
 * Get the default value corresponding to the option name
 *
 * @returns
 * default value(char*) if the option with the desired name exists
 * "NA" if the option is present , but default value isn't set
 * NULL if the option with the particular name doesn't exist.
 *
 */
char *cpdbGetDefault(cpdb_printer_obj_t *p, const char *name);

/**
 * Get the value of the setting corresponding to the name
 *
 * @returns
 * setting value(char*) if the setting with the desired name exists
 * NULL if the setting with the particular name doesn't exist.
 *
 */
char *cpdbGetSetting(cpdb_printer_obj_t *p, const char *name);

/**
 * Get the 'current value' of the attribute with the particular name
 *
 * If the setting with that name exists, that is returned ,
 * else the default value is returned;
 * i.e. , the settings override the defaults
 */
char *cpdbGetCurrent(cpdb_printer_obj_t *p, const char *name);

/**
 * Get number of active jobs(pending + paused + printing)
 * for the printer
 */
int cpdbGetActiveJobsCount(cpdb_printer_obj_t *);

/**
 * Submits a single file for printing, using the settings stored in
 * p->settings
 */
char *cpdbPrintFile(cpdb_printer_obj_t *p, const char *file_path);
char *cpdbPrintFilePath(cpdb_printer_obj_t *p, const char *file_path, const char *final_file_path);

/**
 * Wrapper for the cpdbAddSetting(cpdb_settings_t* , ..) function.
 * Adds the desired setting to p->settings.
 * Updates the value if the setting already exits.
 *
 * @param name : name of the setting
 * @param val : value of the setting
 */
void cpdbAddSettingToPrinter(cpdb_printer_obj_t *p, const char *name, const char *val);

/**
 * Wrapper for the cpdbClearSetting(cpdb_settings_t* , ..) function.
 * clear the desired setting from p->settings.
 *
 * @param name : name of the setting
 */
gboolean cpdbClearSettingFromPrinter(cpdb_printer_obj_t *p, const char *name);

/**
 * Cancel a job on the printer
 *
 * @returns
 * TRUE if job cancellation was successful
 * FALSE otherwise
 */
gboolean cpdbCancelJob(cpdb_printer_obj_t *p, const char *job_id);

/**
 * Serialize the cpdb_printer_obj_t and save it to a file
 * This also keeps the respective backend of the printer alive.
 *
 * This cpdb_printer_obj_t* can then be resurrecuted from the file using the
 * cpdbResurrectPrinterFromFile() function
 */
void cpdbPicklePrinterToFile(cpdb_printer_obj_t *p, const char *filename, const cpdb_frontend_obj_t *parent_dialog);

/**
 * Recreates a cpdb_printer_obj_t from its serialized form stored in the given format
 * and returns it.
 *
 * @returns
 * the cpdb_printer_obj_t* if deserialization was succesfull
 * NULL otherwise
 */
cpdb_printer_obj_t *cpdbResurrectPrinterFromFile(const char *filename);

/**
 * Finds the human readable English name of the setting.
 *
 * @param option_name : name of the setting
 */
char *cpdbGetHumanReadableOptionName(cpdb_printer_obj_t *p, const char *option_name);

/**
 * Finds the human readable English name of the choice for the given setting.
 *
 * @param option_name : name of the setting
 * @param choice_name : value of the choice
 */
char *cpdbGetHumanReadableChoiceName(cpdb_printer_obj_t *p, const char *option_name, const char *choice_name);

/**
 * Get a single cpdb_media_t struct corresponding to the give media name
 *
 * @param media : name of media-size
 */
cpdb_media_t *cpdbGetMedia(cpdb_printer_obj_t *p, const char *media);

/**
 * Finds the dimension for a given media-size
 *
 * @param media : name of media-size
 * @param width : address of width of media-size to be returned
 * @param length : address of length of media-size to be returned
 */
void cpdbGetMediaSize(cpdb_printer_obj_t *p, const char *media, int *width, int *length);

/**
 * Find the margins for a given media-size
 *
 * @param media : name of media-size
 * @param margins : margins array
 */
int cpdbGetMediaMargins(cpdb_printer_obj_t *p, const char *media, cpdb_margin_t **margins);

struct cpdb_async_obj_s
{
    cpdb_printer_obj_t *p;
    cpdb_async_callback caller_cb;
    void *user_data;
};

/**
 * Asynchronously acquires printer details
 *
 * @param caller_cb : callback function
 */
void cpdbAcquireDetails(cpdb_printer_obj_t *p, cpdb_async_callback caller_cb, void *user_data);

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
 * Get an empty cpdb_settings_t struct with no 'settings' in it
 */
cpdb_settings_t *cpdbGetNewSettings();

/**
 * Copy settings from source to dest;
 *
 * The previous values in dest will be overwritten
 */
void cpdbCopySettings(const cpdb_settings_t *source, cpdb_settings_t *dest);

/**
 * Add the particular 'setting' to the cpdb_settings_t struct
 * If the setting already exists, its value is updated instead.
 *
 * Eg. cpdbAddSetting(s,"copies","1")
 */
void cpdbAddSetting(cpdb_settings_t *, const char *name, const char *val);

/**
 * Clear the setting specified by @name
 *
 * @returns
 * TRUE , if the setting was cleared
 * FALSE , if the setting wasn't there and thus couldn't be cleared
 */
gboolean cpdbClearSetting(cpdb_settings_t *, const char *name);

/**
 * Serialize the cpdb_settings_t struct into a GVariant of type a(ss)
 * so that it can be sent as an argument over D-Bus
 */
GVariant *cpdbSerializeToGVariant(cpdb_settings_t *s);

/**
 * Save the settings to disk ,
 * i.e write them to CPDB_SETTINGS_FILE
 */
void cpdbSaveSettingsToDisk(cpdb_settings_t *s);

/**
 * Reads the serialized settings stored in
 *  CPDB_SETTINGS_FILE and creates a cpdb_settings_t* struct from it
 *
 * The caller is responsible for freeing the returned cpdb_settings_t*
 */
cpdb_settings_t *cpdbReadSettingsFromDisk();

void cpdbDeleteSettings(cpdb_settings_t *);

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
 */
cpdb_options_t *cpdbGetNewOptions();

/************************************************************************************************/
/**
______________________________________ cpdb_option_t __________________________________________

**/
struct cpdb_option_s
{
    const char *option_name;
    int num_supported;
    char **supported_values;
    char *default_value;
};
void cpdbPrintOption(const cpdb_option_t *opt);

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
    const char *name;
    int width;
    int length;
    int num_margins;
    cpdb_margin_t *margins;
};

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
void cpdbUnpackJobArray(GVariant *var, int num_jobs, cpdb_job_t *jobs, char *backend_name);
/**
 * ________________________________utility functions__________________________
 */

void CPDB_DEBUG_LOG(const char *msg, const char *error, int msg_level);
char *cpdbConcat(const char *printer_id, const char *backend_name);
/**
 * 'Unpack' (Deserialize) the GVariant returned in cpdbGetAllOptions
 * and fill the cpdb_options_t structure approriately
 */
void cpdbUnpackOptions(int num_options, GVariant *var, int num_media, GVariant *media_var, cpdb_options_t *options);

#ifdef __cplusplus
}
#endif

#endif /* !_CPDB_FRONTEND_H_ */
