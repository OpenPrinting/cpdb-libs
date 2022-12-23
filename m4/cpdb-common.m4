dnl Common configuration stuff for CPDB.

# Check for a C compiler 
AC_PROG_CC 

PKG_CHECK_MODULES([GIO],[gio-2.0]) 
PKG_CHECK_MODULES([GIOUNIX],[gio-unix-2.0]) 
PKG_CHECK_MODULES([GLIB],[glib-2.0]) 

# Checks for header files. 
AC_CHECK_HEADERS([stdlib.h string.h unistd.h sys/stat.h]) 
 
# Checks for typedefs, structures, and compiler characteristics. 
AC_TYPE_SIZE_T 
 
# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([access getcwd mkdir getenv setenv])

AC_DEFINE([CPDB_GETTEXT_PACKAGE], ["cpdb2.0"], [Domain for CPDB package])