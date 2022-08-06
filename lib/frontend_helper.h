#ifdef __cplusplus
extern "C" {
#endif

#ifndef _FRONTEND_HELPER_H_
#define _FRONTEND_HELPER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <dirent.h>
#include "common_helper.h"
#include "backend_interface.h"
#include "frontend_interface.h"
#include <cups/cups.h>

#define INFO 3
#define WARN 2
#define ERR 1

#define DEBUG_LEVEL INFO

#define DIALOG_BUS_NAME "org.openprinting.PrintFrontend"
#define DIALOG_OBJ_PATH "/"
#define DBUS_DIR "/usr/share/print-backends"
#define BACKEND_PREFIX "org.openprinting.Backend."
#define SETTINGS_FILE "~/.CPD-print-settings"

typedef struct _FrontendObj FrontendObj;
typedef struct _PrinterObj PrinterObj;
typedef struct _AsyncObj AsyncObj;
typedef struct _Settings Settings;
typedef struct _Options Options;
typedef struct _Option Option;
typedef struct _Job Job;

typedef int (*event_callback)(PrinterObj *);

/**
 * Callback for async functions
 * 
 * @param int 		: success
 * @param void * 	: user_data
 */
typedef void (*async_callback)(PrinterObj *, int, void *);

/*********************definitions ***************************/

/**
______________________________________ FrontendObj __________________________________________

**/

struct _FrontendObj
{
    PrintFrontend *skeleton;
    GDBusConnection *connection;

    char *bus_name;
    event_callback add_cb;
    event_callback rem_cb;

    int num_backends;
    GHashTable *backend; /**[backend name(like "CUPS" or "GCP")] ---> [BackendObj]**/

    int num_printers;
    GHashTable *printer; /**[printer name] --> [PrinterObj] **/

    Settings *last_saved_settings; /** The last saved settings to disk */
};

/**
 * Get a new FrontendObj instance
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
FrontendObj *get_new_FrontendObj(char *instance_name, event_callback add_cb, event_callback remove_cb);

/**
 * Start the frontend D-Bus Service
 */
void connect_to_dbus(FrontendObj *);

/**
 * Notify Backend services before stopping Frontend
 */
void disconnect_from_dbus(FrontendObj *);

/**
 * Discover the currently installed backends and activate them
 * 
 * 
 * Reads the DBUS_DIR folder to find the files installed by 
 * the respective backends , 
 * For eg:  org.openprinting.Backend.XYZ
 * 
 * XYZ = Backend suffix, using which it will be identified henceforth 
 */
void activate_backends(FrontendObj *);

/**
 * The default behaviour of FrontendObj is to use the
 * settings previously saved to disk the last time any print dialog ran.
 * 
 * To ignore the last saved settings, you need to explicitly call this function
 * after get_new_FrontendObj
 */
void ignore_last_saved_settings(FrontendObj *);

/**
 * Add the printer to the FrontendObj instance
 */
gboolean add_printer(FrontendObj *f, PrinterObj *p);

/**
 * Remove the printer from FrontendObj
 * 
 * @returns
 * The PrinterObj* struct corresponding to the printer just removed,
 * or NULL if the removal was unsuccesful 
 * 
 * The PrinterObj removed is not deallocated. 
 * The caller is responsible for deallocation
 */
PrinterObj *remove_printer(FrontendObj *f, char *printer_id, char *backend_name);
void refresh_printer_list(FrontendObj *f);

/**
 * Hide the remote printers of the CUPS backend
 */
void hide_remote_cups_printers(FrontendObj *f);
void unhide_remote_cups_printers(FrontendObj *f);

/**
 * Hide those (temporary) printers which have been discovered by the CUPS backend,
 * but haven't been yet set up locally
 */
void hide_temporary_cups_printers(FrontendObj *f);
void unhide_temporary_cups_printers(FrontendObj *f);

/**
 * Read the file installed by the backend and create a proxy object 
 * using the backend service name and object path.
 */
PrintBackend *create_backend_from_file(const char *);

/**
 * Find the PrinterObj instance with a particular id ans backend name.
 */
PrinterObj *find_PrinterObj(FrontendObj *, char *printer_id, char *backend_name);

/**
 * Get the default printer for a particular backend
 * 
 * @param backend_name : The name of the backend
 *                          Can be just the suffix("CUPS")
 *                          or
 *                          the complete name ("org.openprinting.Backend.CUPS")
 */
char *get_default_printer(FrontendObj *, char *backend_name);

/**
 * Get the list of (all/active) jobs from all the backends
 * 
 * @param j : pointer to a Job array; the retrieved Job list array is stored at this location
 * @param active_only : when set to true , retrieves only the active jobs; 
 *                      otherwise fetches all(active + completed + stopped) jobs
 *                      Retrieves jobs for all users.
 * 
 * returns : number of jobs(i.e. length of the Job array)
 * 
 */
int get_all_jobs(FrontendObj *, Job **j, gboolean active_only);

/*******************************************************************************************/

/**
______________________________________ PrinterObj __________________________________________

**/
struct _PrinterObj
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
    gboolean is_accepting_jobs;

    /** The more advanced options we get from the backend **/
    Options *options;

    /**The settings the user selects, and which will be used for printing the job**/
    Settings *settings;
};
PrinterObj *get_new_PrinterObj();

/**
 * Fill the basic options of PrinterObj from the GVariant returned with the printerAdded signal
 */
void fill_basic_options(PrinterObj *, GVariant *);

/**
 * Print the basic options of PrinterObj
 */
void print_basic_options(PrinterObj *);
gboolean is_accepting_jobs(PrinterObj *);
char *get_state(PrinterObj *);

/**
 * Get all the advanced supported options for the printer.
 * This function populates the 'options' variable of the PrinterObj structure, 
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
Options *get_all_options(PrinterObj *);

/**
 * Get a single Option struct corresponding to a particular name.
 * 
 * @returns
 * Option* if the option was found
 * NULL if the option with desired name doesn't exist 
 */
Option *get_Option(PrinterObj *p, char *name);

/**
 * Get the default value corresponding to the option name
 * 
 * @returns
 * default value(char*) if the option with the desired name exists
 * "NA" if the option is present , but default value isn't set
 * NULL if the option with the particular name doesn't exist.
 * 
 */
char *get_default(PrinterObj *p, char *name);

/**
 * Get the value of the setting corresponding to the name
 * 
 * @returns
 * setting value(char*) if the setting with the desired name exists
 * NULL if the setting with the particular name doesn't exist.
 * 
 */
char *get_setting(PrinterObj *p, char *name);

/**
 * Get the 'current value' of the attribute with the particular name
 * 
 * If the setting with that name exists, that is returned , 
 * else the default value is returned;
 * i.e. , the settings override the defaults
 */
char *get_current(PrinterObj *p, char *name);

/**
 * Get number of active jobs(pending + paused + printing)
 * for the printer
 */
int get_active_jobs_count(PrinterObj *);

/**
 * Submits a single file for printing, using the settings stored in 
 * p->settings
 */
char *print_file(PrinterObj *p, char *file_path);
char *print_file_path(PrinterObj *p, char *file_path, char *final_file_path);

/**
 * Wrapper for the add_setting(Settings* , ..) function.
 * Adds the desired setting to p->settings.
 * Updates the value if the setting already exits.
 * 
 * @param name : name of the setting
 * @param val : value of the setting
 */
void add_setting_to_printer(PrinterObj *p, char *name, char *val);

/**
 * Wrapper for the clear_setting(Settings* , ..) function.
 * clear the desired setting from p->settings.
 * 
 * @param name : name of the setting
 */
gboolean clear_setting_from_printer(PrinterObj *p, char *name);

/**
 * Cancel a job on the printer
 * 
 * @returns 
 * TRUE if job cancellation was successful
 * FALSE otherwise
 */
gboolean cancel_job(PrinterObj *p, char *job_id);

/**
 * Serialize the PrinterObj and save it to a file
 * This also keeps the respective backend of the printer alive.
 * 
 * This PrinterObj* can then be resurrecuted from the file using the
 * resurrect_printer_from_file() function
 */
void pickle_printer_to_file(PrinterObj *p, const char *filename , const FrontendObj* parent_dialog);

/**
 * Recreates a PrinterObj from its serialized form stored in the given format 
 * and returns it.
 * 
 * @returns
 * the PrinterObj* if deserialization was succesfull
 * NULL otherwise
 */
PrinterObj *resurrect_printer_from_file(const char *filename);

/**
 * Finds the human readable English name of the setting.
 *
 * @param option_name : name of the setting
 */
char *get_human_readable_option_name(PrinterObj *p, char *option_name);

/**
 * Finds the human readable English name of the choice for the given setting.
 *
 * @param option_name : name of the setting
 * @param choice_name : value of the choice
 */
char *get_human_readable_choice_name(PrinterObj *p, char *option_name, char *choice_name);

/**
 * Finds the dimension for a given media-size
 *
 * @param media : name of media-size
 * @param width : address of width of media-size to be returned
 * @param length : address of length of media-size to be returned
 */
void get_media_size(PrinterObj *p, const char *media, int *width, int *length);


struct _AsyncObj {
	PrinterObj *p;
	async_callback caller_cb;
	void *user_data;
};

/**
 * Asynchronously acquires printer details
 * 
 * @param caller_cb : callback function
 */
void acquire_details(PrinterObj *p, async_callback caller_cb, void *user_data);

/************************************************************************************************/
/**
______________________________________ Settings __________________________________________

**/

/**
 * Takes care of the settings the user sets with the help of the dialog.
 * These settings will be used when sending a print job
 */
struct _Settings
{
    int count;
    GHashTable *table; /** [name] --> [value] **/
    //planned functions:
    // serialize settings into a GVariant of type a(ss)
};

/**
 * Get an empty Settings struct with no 'settings' in it
 */
Settings *get_new_Settings();

/**
 * Copy settings from source to dest;
 * 
 * The previous values in dest will be overwritten
 */
void copy_settings(const Settings *source, Settings *dest);

/**
 * Add the particular 'setting' to the Settings struct
 * If the setting already exists, its value is updated instead.
 * 
 * Eg. add_setting(s,"copies","1") 
 */
void add_setting(Settings *, const char *name, const char *val);

/**
 * Clear the setting specified by @name
 * 
 * @returns
 * TRUE , if the setting was cleared
 * FALSE , if the setting wasn't there and thus couldn't be cleared
 */
gboolean clear_setting(Settings *, char *name);

/**
 * Serialize the Settings struct into a GVariant of type a(ss)
 * so that it can be sent as an argument over D-Bus
 */
GVariant *serialize_to_gvariant(Settings *s);

/**
 * Save the settings to disk ,
 * i.e write them to SETTINGS_FILE
 */
void save_to_disk(Settings *s);

/**
 * Reads the serialized settings stored in
 *  SETTINGS_FILE and creates a Settings* struct from it
 * 
 * The caller is responsible for freeing the returned Settings*
 */
Settings *read_settings_from_disk();

void delete_Settings(Settings *);

/************************************************************************************************/
/**
______________________________________ Options __________________________________________

**/
struct _Options
{
    int count;
    GHashTable *table; /**[name] --> Option struct**/
};

/**
 * Get an empty Options struct with no 'options' in it
 */
Options *get_new_Options();

/************************************************************************************************/
/**
______________________________________ Option __________________________________________

**/
struct _Option
{
    const char *option_name;
    int num_supported;
    char **supported_values;
    char *default_value;
};
void print_option(const Option *opt);

/************************************************************************************************/
/**
______________________________________ Job __________________________________________

**/
struct _Job
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
void unpack_job_array(GVariant *var, int num_jobs, Job *jobs, char *backend_name);
/**
 * ________________________________utility functions__________________________
 */

void DBG_LOG(const char *msg, const char *error, int msg_level);
char *concat(char *printer_id, char *backend_name);
/**
 * 'Unpack' (Deserialize) the GVariant returned in get_all_options
 * and fill the Options structure approriately
 */
void unpack_options(GVariant *var, int num_options, Options *options);
#endif

#ifdef __cplusplus
}
#endif
