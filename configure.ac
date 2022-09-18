#                                               -*- Autoconf -*- 
# Process this file with autoconf to produce a configure script. 
 
AC_PREREQ([2.69]) 
AC_INIT([cpdb-libs], [2.0.0], [https://github.com/OpenPrinting/cpdb-libs/issues], [cpdb-libs], [https://github.com/OpenPrinting/cpdb-libs])
AM_INIT_AUTOMAKE([-Wall foreign]) 
AC_CONFIG_SRCDIR([cpdb/cpdb-frontend.h])

: ${CFLAGS=""}

# Check for a C compiler 
AC_PROG_CC 

#Check for archiver
AM_PROG_AR

LT_INIT

PKG_CHECK_MODULES([GIO],[gio-2.0]) 
PKG_CHECK_MODULES([GIOUNIX],[gio-unix-2.0]) 
PKG_CHECK_MODULES([GLIB],[glib-2.0]) 

# Checks for header files. 
AC_CHECK_HEADERS([stdlib.h string.h unistd.h]) 
 
# Checks for typedefs, structures, and compiler characteristics. 
AC_TYPE_SIZE_T 
 
# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([getcwd])

default_printers_file="CPD-default-printers"

AC_ARG_WITH([user-configdir],
[ AS_HELP_STRING([--with-user-configdir], [Specify the directory for CPD's user config file]) ],
[
    AS_MKDIR_P([$withval])
    AS_CASE(
        ["$withval"],
        ["*/"], [
            AC_DEFINE_UNQUOTED(CPDB_USER_CONFIG_DIR, "$withval"),
            AC_DEFINE_UNQUOTED(CPDB_USER_DEFAULT_PRINTERS, "$withval$default_printers_file")
        ],
        [
            AC_DEFINE_UNQUOTED(CPDB_USER_CONFIG_DIR, "$withval/"),
            AC_DEFINE_UNQUOTED(CPDB_USER_DEFAULT_PRINTERS, "$withval/$default_printers_file")
        ]
    )
],
[
    config_dir=~/.config/cpdb/
    AS_MKDIR_P([$config_dir])
    AC_DEFINE_UNQUOTED(CPDB_USER_CONFIG_DIR, "$config_dir")
    AC_DEFINE_UNQUOTED(CPDB_USER_DEFAULT_PRINTERS, "$config_dir$default_printers_file")
])

AC_ARG_WITH([system-configdir],
[ AS_HELP_STRING([--with-system-configdir], [Specify the directory for CPD's system wide config file]) ],
[
    AS_MKDIR_P([$withval])
    AS_CASE(
        ["$withval"],
        ["*/"], [
            AC_DEFINE_UNQUOTED(CPDB_SYSTEM_CONFIG_DIR, "$withval"),
            AC_DEFINE_UNQUOTED(CPDB_SYSTEM_DEFAULT_PRINTERS, "$withval$default_printers_file")
        ],
        [
            AC_DEFINE_UNQUOTED(CPDB_SYSTEM_CONFIG_DIR, "$withval/"),
            AC_DEFINE_UNQUOTED(CPDB_SYSTEM_DEFAULT_PRINTERS, "$withval/$default_printers_file")
        ]
    )
],
[
    config_dir=/etc/cpdb/
    AS_MKDIR_P([$config_dir])
    AC_DEFINE_UNQUOTED(CPDB_SYSTEM_CONFIG_DIR, "$config_dir")
    AC_DEFINE_UNQUOTED(CPDB_SYSTEM_DEFAULT_PRINTERS, "$config_dir$default_printers_file")
])

AC_CONFIG_FILES([Makefile
                 cpdb/Makefile
                 cpdb-backend.pc
                 cpdb-frontend.pc
                 cpdb.pc])

AC_OUTPUT
