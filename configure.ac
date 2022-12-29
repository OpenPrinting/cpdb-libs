#                                               -*- Autoconf -*- 
# Process this file with autoconf to produce a configure script. 
 
AC_PREREQ([2.69])

dnl Package name and version...
AC_INIT([cpdb-libs], [2.0b1], [https://github.com/OpenPrinting/cpdb-libs/issues], [cpdb-libs], [https://github.com/OpenPrinting/cpdb-libs])

AC_CONFIG_SRCDIR([cpdb/cpdb-frontend.h])

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall foreign])

# Check for archiver
AM_PROG_AR

# Check for GNU gettext
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.21])

LT_INIT

dnl Include configure tests for different areas...
sinclude(m4/cpdb-directories.m4)
sinclude(m4/cpdb-common.m4)

AC_CONFIG_FILES([Makefile
                 cpdb/Makefile
                 cpdb-backend.pc
                 cpdb-frontend.pc
                 cpdb.pc])

AC_OUTPUT

dnl Make sure m4 scripts are executable
chmod +x m4
