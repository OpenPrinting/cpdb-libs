#ifndef _COMMON_HELPER_H_
#define _COMMON_HELPER_H_


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/types.h>
#include <pwd.h>
#include "backend_interface.h"
#include "frontend_interface.h"

#define PRINTER_ADDED_ARGS "(sssssbss)"
#define JOB_ARGS "(ssssssi)"
#define JOB_ARRAY_ARGS "a(ssssssi)"
#define new_cstring_array(x) ((char **)(malloc(sizeof(char *) * x)))

/*********Option related functions*****************/


gboolean get_boolean(const char *);
char *get_string_copy(const char *);
void unpack_string_array(GVariant *variant, int num_val, char ***val);
GVariant *pack_string_array(int num_val, char **val);
char *get_absolute_path(const char *file_path);
char *extract_file_name(const char* file_path);
/*********LISTING OF ALL POSSIBLE OPTIONS*****/
//Rename these to something better if needed
/**
 * Some standard option names.
 * While adding settings, use these as option names
 */

#define CPD_OPTION_COPIES "copies"
#define CPD_OPTION_JOB_HOLD_UNTIL "job-hold-until"
#define CPD_OPTION_JOB_NAME "job-name"
#define CPD_OPTION_JOB_PRIORITY "job-priority"
#define CPD_OPTION_MEDIA "media"
#define CPD_OPTION_NUMBER_UP "number-up"
#define CPD_OPTION_ORIENTATION "orientation-requested"
#define CPD_OPTION_RESOLUTION "printer-resolution"
#define CPD_OPTION_COLOR_MODE "print-color-mode"
#define CPD_OPTION_SIDES "sides"


#define COLOR_MODE_COLOR "color"
#define COLOR_MODE_BW "monochrome"
#define COLOR_MODE_AUTO "auto"

#define QUALITY_DRAFT "draft"
#define QUALITY_NORMAL "normal"
#define QUALITY_HIGH "high"

#define SIDES_ONE_SIDED "one-sided"
#define SIDES_TWO_SIDED_SHORT "two-sided-short"
#define SIDES_TWO_SIDED_LONG "two-sided-long"

#define ORIENTATION_PORTRAIT "portrait"
#define ORIENTATION_LANDSCAPE "landscape"

#define PRIOIRITY_URGENT "urgent"
#define PRIOIRITY_HIGH "high"
#define PRIOIRITY_MEDIUM "medium"
#define PRIOIRITY_LOW "low"

#define STATE_IDLE "idle"
#define STATE_PRINTING "printing"
#define STATE_STOPPED "stopped"

#define STOP_BACKEND_SIGNAL "StopListing"
#define REFRESH_BACKEND_SIGNAL "RefreshBackend"
#define PRINTER_ADDED_SIGNAL "PrinterAdded"
#define PRINTER_REMOVED_SIGNAL "PrinterRemoved"
#define HIDE_REMOTE_SIGNAL "HideRemotePrinters"
#define UNHIDE_REMOTE_SIGNAL "UnhideRemotePrinters"
#define HIDE_TEMP_SIGNAL "HideTemporaryPrinters"
#define UNHIDE_TEMP_SIGNAL "UnhideTemporaryPrinters"

#define JOB_STATE_ABORTED "Aborted"
#define JOB_STATE_CANCELLED "Cancelled"
#define JOB_STATE_COMPLETED "Completed"
#define JOB_STATE_HELD "Held"
#define JOB_STATE_PENDING "Pending" 
#define JOB_STATE_PRINTING "Printing"
#define JOB_STATE_STOPPED "Stopped"


#endif
