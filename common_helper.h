#ifndef _COMMON_HELPER_H_
#define _COMMON_HELPER_H_

/*********LISTING OF ALL POSSIBLE OPTIONS*****/
//Rename these to something better if needed

#define MEDIA_A4 "A4"
#define MEDIA_A3 "A3"
#define MEDIA_A5 "A5"
#define MEDIA_A6 "A6"
#define MEDIA_LEGAL "LEGAL"
#define MEDIA_LETTER "LETTER"
#define MEDIA_PHOTO "PHOTO"
#define MEDIA_TABLOID "TABLOID"
#define MEDIA_ENV "ENV10"

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

#define STOP_BACKEND_SIGNAL "StopListing"
#define ACTIVATE_BACKEND_SIGNAL "GetBackend"
#define REFRESH_BACKEND_SIGNAL "RefreshBackend"
#define PRINTER_ADDED_SIGNAL "PrinterAdded"
#define PRINTER_REMOVED_SIGNAL "PrinterRemoved"

gboolean get_boolean(gchar *);

#endif