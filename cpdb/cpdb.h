#ifndef _CPDB_CPDB_H_
#define _CPDB_CPDB_H_

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <libintl.h>
#define _(String) dgettext (CPDB_GETTEXT_PACKAGE, String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#include <cpdb/backend-interface.h>
#include <cpdb/frontend-interface.h>

/* buffer sizes */
#define CPDB_BSIZE 512

/* Config files directory permissions, 
 * if needed to be created */
#define CPDB_USRCONFDIR_PERM 0755
#define CPDB_SYSCONFDIR_PERM 0755

/* Environment variables for printing debug info */
#define CPDB_DEBUG_LEVEL   "CPDB_DEBUG_LEVEL"
#define CPDB_DEBUG_LOGFILE "CPDB_DEBUG_LOGFILE"

#define CPDB_PRINTER_ADDED_ARGS "(sssssbss)"
#define CPDB_JOB_ARGS "(ssssssi)"
#define CPDB_JOB_ARRAY_ARGS "a(ssssssi)"
#define cpdbNewCStringArray(x) ((char **)(malloc(sizeof(char *) * x)))

typedef enum {
    CPDB_DEBUG_LEVEL_DEBUG,
    CPDB_DEBUG_LEVEL_INFO,
    CPDB_DEBUG_LEVEL_WARN,
    CPDB_DEBUG_LEVEL_ERROR,
} CpdbDebugLevel;

/**
 * Initializes CPDB.
 * It’s the responsibility of the main program to set the locale.
 */
void cpdbInit();

/* Convert string to gboolean */
gboolean cpdbGetBoolean(const char *);
/* Concatenate two strings */
char *cpdbConcat(const char *s1, const char *s2);
/* Concatenate two strings with separator "#" */
char *cpdbConcatSep(const char *s1, const char *s2);
/* Concatenate two paths */
char *cpdbConcatPath(const char *s1, const char *s2);
/* Get string copy */
char *cpdbGetStringCopy(const char *s);
/* Get directory for user configuration files */
char *cpdbGetUserConfDir();
/* Get directory for system wide configuration files */
char *cpdbGetSysConfDir();
/* Get absolute path from relative path */
char *cpdbGetAbsolutePath(const char *file_path);
/* Extract file name for path */
char *cpdbExtractFileName(const char* file_path);
/* Get a group for given option name */
char *cpdbGetGroup(const char *option_name);
/* Get translation for given group name */
char *cpdbGetGroupTranslation2(const char *group_name, const char *locale);

/* Format and print debug message for frontend */
void cpdbFDebugPrintf(CpdbDebugLevel msg_lvl, const char *fmt, ...);
/* Format and print debug message for backends */
void cpdbBDebugPrintf(CpdbDebugLevel msg_lvl, const char *backend_name, const char *fmt, ...);

/* Packing/Unpacking utility functions */
void cpdbUnpackStringArray(GVariant *variant, int num_val, char ***val);
GVariant *cpdbPackStringArray(int num_val, char **val);
GVariant *cpdbPackMediaArray(int num_val, int (*margins)[4]);


/*********LISTING OF ALL POSSIBLE GROUPS*****/
/**
 * Some standard group names.
 */

#define CPDB_GROUP_MEDIA        N_("Media")
#define CPDB_GROUP_COPIES       N_("Copies")
#define CPDB_GROUP_COLOR        N_("Color")
#define CPDB_GROUP_SCALING      N_("Scaling")
#define CPDB_GROUP_QUALITY      N_("Ouput Quality")
#define CPDB_GROUP_ADVANCED     N_("Advanced")
#define CPDB_GROUP_JOB_MGMT     N_("Job Management")
#define CPDB_GROUP_PAGE_MGMT    N_("Page Management")
#define CPDB_GROUP_FINISHINGS   N_("Finishings")


/*********LISTING OF ALL POSSIBLE OPTIONS*****/
/**
 * Some standard IPP option names.
 * While adding settings, use these as option names
 */

#define CPDB_OPTION_COPIES                  N_("copies")
#define CPDB_OPTION_COLLATE                 N_("multiple-document-handling")
#define CPDB_OPTION_COPIES_SUPPORTED        N_("multiple-document-jobs-supported")

#define CPDB_OPTION_MEDIA                   N_("media")
#define CPDB_OPTION_MEDIA_COL               N_("media-col")
#define CPDB_OPTION_MEDIA_TYPE              N_("media-type")
#define CPDB_OPTION_MEDIA_SOURCE            N_("media-source")
#define CPDB_OPTION_MARGIN_TOP              N_("media-top-margin")
#define CPDB_OPTION_MARGIN_BOTTOM           N_("media-bottom-margin")
#define CPDB_OPTION_MARGIN_LEFT             N_("media-left-margin")
#define CPDB_OPTION_MARGIN_RIGHT            N_("media-right-margin")

#define CPDB_OPTION_SIDES                   N_("sides")
#define CPDB_OPTION_MIRROR                  N_("mirror")
#define CPDB_OPTION_BOOKLET                 N_("booklet")
#define CPDB_OPTION_PAGE_SET                N_("page-set")
#define CPDB_OPTION_NUMBER_UP               N_("number-up")
#define CPDB_OPTION_NUMBER_UP_LAYOUT        N_("number-up-layout")
#define CPDB_OPTION_PAGE_BORDER             N_("page-border")
#define CPDB_OPTION_PAGE_RANGES             N_("page-ranges")
#define CPDB_OPTION_ORIENTATION             N_("orientation-requested")

#define CPDB_OPTION_POSITION                N_("position")
#define CPDB_OPTION_FIDELITY                N_("ipp-attribute-fidelity")
#define CPDB_OPTION_PRINT_SCALING           N_("print-scaling")

#define CPDB_OPTION_COLOR_MODE              N_("print-color-mode")
#define CPDB_OPTION_RESOLUTION              N_("printer-resolution")
#define CPDB_OPTION_PRINT_QUALITY           N_("print-quality")

#define CPDB_OPTION_FINISHINGS              N_("finishings")
#define CPDB_OPTION_OUTPUT_BIN              N_("output-bin")
#define CPDB_OPTION_PAGE_DELIVERY           N_("page-delivery")

#define CPDB_OPTION_JOB_NAME                N_("job-name")
#define CPDB_OPTION_JOB_SHEETS              N_("job-sheets")
#define CPDB_OPTION_BILLING_INFO            N_("billing-info")
#define CPDB_OPTION_JOB_PRIORITY            N_("job-priority")
#define CPDB_OPTION_JOB_HOLD_UNTIL          N_("job-hold-until")


/*********LISTING OF ALL POSSIBLE OPTION CHOICES*****/
/**
 * Some standard IPP option names.
 * While adding settings, use these as option names
 */

#define CPDB_PAGE_SET_ALL               N_("all")
#define CPDB_PAGE_SET_ODD               N_("odd")
#define CPDB_PAGE_SET_EVEN              N_("even")

#define CPDB_COLOR_MODE_COLOR           N_("color")
#define CPDB_COLOR_MODE_BW              N_("monochrome")
#define CPDB_COLOR_MODE_AUTO            N_("auto")

#define CPDB_PAGE_DELIVERY_SAME         N_("same-order")
#define CPDB_PAGE_DELIVERY_REVERSE      N_("reverse-order")

#define CPDB_COLLATE_ENABLED            N_("separate-documents-collated-copies")
#define CPDB_COLLATE_DISABLED           N_("separate-documents-uncollated-copies")

#define CPDB_QUALITY_DRAFT              N_("draft")
#define CPDB_QUALITY_NORMAL             N_("normal")
#define CPDB_QUALITY_HIGH               N_("high")

#define CPDB_SIDES_ONE_SIDED            N_("one-sided")
#define CPDB_SIDES_TWO_SIDED_SHORT      N_("two-sided-short")
#define CPDB_SIDES_TWO_SIDED_LONG       N_("two-sided-long")

#define CPDB_ORIENTATION_PORTRAIT       N_("3")
#define CPDB_ORIENTATION_LANDSCAPE      N_("4")
#define CPDB_ORIENTATION_RLANDSCAPE     N_("5")
#define CPDB_ORIENTATION_RPORTRAIT      N_("6")

#define CPDB_JOB_HOLD_NONE              N_("no-hold")
#define CPDB_JOB_HOLD_INDEFINITE        N_("indefinite")

#define CPDB_PRIORITY_URGENT            N_("urgent")
#define CPDB_PRIORITY_HIGH              N_("high")
#define CPDB_PRIORITY_MEDIUM            N_("medium")
#define CPDB_PRIORITY_LOW               N_("low")

#define CPDB_STATE_IDLE                 N_("idle")
#define CPDB_STATE_PRINTING             N_("printing")
#define CPDB_STATE_STOPPED              N_("stopped")

#define CPDB_SIGNAL_STOP_BACKEND N_("StopListing")
#define CPDB_SIGNAL_REFRESH_BACKEND N_("RefreshBackend")
#define CPDB_SIGNAL_PRINTER_ADDED N_("PrinterAdded")
#define CPDB_SIGNAL_PRINTER_REMOVED N_("PrinterRemoved")
#define CPDB_SIGNAL_HIDE_REMOTE N_("HideRemotePrinters")
#define CPDB_SIGNAL_UNHIDE_REMOTE N_("UnhideRemotePrinters")
#define CPDB_SIGNAL_HIDE_TEMP N_("HideTemporaryPrinters")
#define CPDB_SIGNAL_UNHIDE_TEMP N_("UnhideTemporaryPrinters")

#define CPDB_JOB_STATE_ABORTED N_("Aborted")
#define CPDB_JOB_STATE_CANCELLED N_("Cancelled")
#define CPDB_JOB_STATE_COMPLETED N_("Completed")
#define CPDB_JOB_STATE_HELD N_("Held")
#define CPDB_JOB_STATE_PENDING N_("Pending") 
#define CPDB_JOB_STATE_PRINTING N_("Printing")
#define CPDB_JOB_STATE_STOPPED N_("Stopped")

#ifdef __cplusplus
}
#endif

#endif /* !_CPDB_CPDB_H_ */
