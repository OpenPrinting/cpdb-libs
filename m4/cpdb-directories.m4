dnl Directory stuff for CPDB.

dnl Fix "prefix" variable if it hasn't been specified...
AS_IF([test "$prefix" = "NONE"], [
    prefix="/usr/local"
])

dnl Fix "exec_prefix" variable if it hasn't been specified...
AS_IF([test "$exec_prefix" = "NONE"], [
    AS_IF([test "$prefix" = "/usr/local"], [
	exec_prefix="/usr/local"
    ], [
	exec_prefix="$prefix"
    ])
])

dnl Fix "bindir" variable...
AS_IF([test "$bindir" = "\${exec_prefix}/bin"], [
    bindir="$exec_prefix/bin"
])

dnl Fix "sbindir" variable...
AS_IF([test "$sbindir" = "\${exec_prefix}/sbin"], [
    sbindir="$exec_prefix/sbin"
])

dnl Fix "datarootdir" variable if it hasn't been specified...
AS_IF([test "$datarootdir" = "\${prefix}/share"], [
    AS_IF([test "$prefix" = "/usr/local"], [
	datarootdir="/usr/local/share"
    ], [
	datarootdir="$prefix/share"
    ])
])

dnl Fix "datadir" variable if it hasn't been specified...
AS_IF([test "$datadir" = "\${prefix}/share"], [
    AS_IF([test "$prefix" = "/usr/local"], [
	datadir="/usr/local/share"
    ], [
	datadir="$prefix/share"
    ])
], [test "$datadir" = "\${datarootdir}"], [
    datadir="$datarootdir"
])

dnl Fix "includedir" variable if it hasn't been specified...
AS_IF([test "$includedir" = "\${prefix}/include" -a "$prefix" = "/usr/local"], [
    includedir="/usr/local/include"
])

dnl Fix "localstatedir" variable if it hasn't been specified...
AS_IF([test "$localstatedir" = "\${prefix}/var"], [
    AS_IF([test "$prefix" = "/usr/local"], [
    localstatedir="/usr/local/var"
    ], [
	localstatedir="$prefix/var"
    ])
])

dnl Fix "sysconfdir" variable if it hasn't been specified...
AS_IF([test "$sysconfdir" = "\${prefix}/etc"], [
    AS_IF([test "$prefix" = "/usr/local"], [
    sysconfdir="/usr/local/etc"
    ], [
	sysconfdir="$prefix/etc"
    ])
])

dnl Fix "libdir" variable...
AS_IF([test "$libdir" = "\${exec_prefix}/lib"], [
    AS_CASE(["$host_os_name"], [linux*], [
	AS_IF([test -d /usr/lib64 -a ! -d /usr/lib64/fakeroot], [
	    libdir="$exec_prefix/lib64"
	], [
	    libdir="$exec_prefix/lib"
	])
    ], [*], [
	libdir="$exec_prefix/lib"
    ])
])

dnl Fix "sysconfdir" variable if it hasn't been specified...
AS_IF([test "$sysconfdir" = "\${prefix}/etc"], [
    AS_IF([test "$prefix" = "/usr/local"], [
    sysconfdir="/usr/local/etc"
    ], [
	sysconfdir="$prefix/etc"
    ])
])


AC_DEFINE_UNQUOTED([CPDB_LOCALEDIR], ["$localedir"], [Location for locale files])
AC_DEFINE_UNQUOTED([CPDB_SYSCONFDIR], ["$sysconfdir"], [Location for system-wide configuration files])

# The info directory which will be read by the frontend
CPDB_BACKEND_INFO_DIR="$datadir/print-backends"
AC_SUBST([CPDB_BACKEND_INFO_DIR])
AC_DEFINE_UNQUOTED([CPDB_BACKEND_INFO_DIR], ["$CPDB_BACKEND_INFO_DIR"])

# The directory for the backend executables
CPDB_BACKEND_EXEC_DIR="$libdir/print-backends"
AC_SUBST([CPDB_BACKEND_EXEC_DIR])
