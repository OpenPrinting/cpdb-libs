#ifndef _CPDB_H_
#define _CPDB_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/types.h>
#include <pwd.h>
#include <cpdb/backend-interface.h>
#include <cpdb/frontend-interface.h>

#define CPDB_PRINTER_ADDED_ARGS "(sssssbss)"
#define CPDB_JOB_ARGS "(ssssssi)"
#define CPDB_JOB_ARRAY_ARGS "a(ssssssi)"
#define cpdbNewCStringArray(x) ((char **)(malloc(sizeof(char *) * x)))

/*********cpdb_option_t related functions*****************/


gboolean cpdbGetBoolean(const char *);
char *cpdbGetStringCopy(const char *);
void cpdbUnpackStringArray(GVariant *variant, int num_val, char ***val);
GVariant *cpdbPackStringArray(int num_val, char **val);
GVariant *cpdbPackMediaArray(int num_val, int (*margins)[4]);
char *cpdbGetAbsolutePath(const char *file_path);
char *cpdbExtractFileName(const char* file_path);
/*********LISTING OF ALL POSSIBLE OPTIONS*****/
//Rename these to something better if needed
/**
 * Some standard option names.
 * While adding settings, use these as option names
 */

#define CPDB_OPTION_COPIES "copies"
#define CPDB_OPTION_JOB_HOLD_UNTIL "job-hold-until"
#define CPDB_OPTION_JOB_NAME "job-name"
#define CPDB_OPTION_JOB_PRIORITY "job-priority"
#define CPDB_OPTION_MEDIA "media"
#define CPDB_OPTION_NUMBER_UP "number-up"
#define CPDB_OPTION_ORIENTATION "orientation-requested"
#define CPDB_OPTION_RESOLUTION "printer-resolution"
#define CPDB_OPTION_COLOR_MODE "print-color-mode"
#define CPDB_OPTION_SIDES "sides"


#define CPDB_COLOR_MODE_COLOR "color"
#define CPDB_COLOR_MODE_BW "monochrome"
#define CPDB_COLOR_MODE_AUTO "auto"

#define CPDB_QUALITY_DRAFT "draft"
#define CPDB_QUALITY_NORMAL "normal"
#define CPDB_QUALITY_HIGH "high"

#define CPDB_SIDES_ONE_SIDED "one-sided"
#define CPDB_SIDES_TWO_SIDED_SHORT "two-sided-short"
#define CPDB_SIDES_TWO_SIDED_LONG "two-sided-long"

#define CPDB_ORIENTATION_PORTRAIT "portrait"
#define CPDB_ORIENTATION_LANDSCAPE "landscape"

#define CPDB_PRIORITY_URGENT "urgent"
#define CPDB_PRIORITY_HIGH "high"
#define CPDB_PRIORITY_MEDIUM "medium"
#define CPDB_PRIORITY_LOW "low"

#define CPDB_STATE_IDLE "idle"
#define CPDB_STATE_PRINTING "printing"
#define CPDB_STATE_STOPPED "stopped"

#define CPDB_SIGNAL_STOP_BACKEND "StopListing"
#define CPDB_SIGNAL_REFRESH_BACKEND "RefreshBackend"
#define CPDB_SIGNAL_PRINTER_ADDED "PrinterAdded"
#define CPDB_SIGNAL_PRINTER_REMOVED "PrinterRemoved"
#define CPDB_SIGNAL_HIDE_REMOTE "HideRemotePrinters"
#define CPDB_SIGNAL_UNHIDE_REMOTE "UnhideRemotePrinters"
#define CPDB_SIGNAL_HIDE_TEMP "HideTemporaryPrinters"
#define CPDB_SIGNAL_UNHIDE_TEMP "UnhideTemporaryPrinters"

#define CPDB_JOB_STATE_ABORTED "Aborted"
#define CPDB_JOB_STATE_CANCELLED "Cancelled"
#define CPDB_JOB_STATE_COMPLETED "Completed"
#define CPDB_JOB_STATE_HELD "Held"
#define CPDB_JOB_STATE_PENDING "Pending" 
#define CPDB_JOB_STATE_PRINTING "Printing"
#define CPDB_JOB_STATE_STOPPED "Stopped"


#ifdef __cplusplus
}
#endif

#endif /* !_CPDB_H_ */
