EXTRA_DIST = interface

BUILT_SOURCES = backend-interface.h 

lib_LTLIBRARIES	= libcpdb.la libcpdb-frontend.la

libcpdb_la_SOURCES = cpdb.c \
		     cpdb.h
nodist_libcpdb_la_SOURCES = backend-interface.c 

libcpdb_la_CPPFLAGS  = $(GLIB_CFLAGS)
libcpdb_la_CPPFLAGS += $(GIO_CFLAGS)
libcpdb_la_CPPFLAGS += $(GIOUNIX_CFLAGS)
libcpdb_la_CPPFLAGS += -I..

libcpdb_la_LIBADD  = $(GLIB_LIBS)
libcpdb_la_LIBADD += $(GIO_LIBS)
libcpdb_la_LIBADD += $(GIOUNIX_LIBS)

libcpdb_la_LDFLAGS = -no-undefined -version-info 3


libcpdb_frontend_la_SOURCES = cpdb-frontend.c \
			      cpdb-frontend.h

libcpdb_frontend_la_CPPFLAGS  = $(GLIB_CFLAGS)
libcpdb_frontend_la_CPPFLAGS += $(GIO_CFLAGS)
libcpdb_frontend_la_CPPFLAGS += $(GIOUNIX_CFLAGS)
libcpdb_frontend_la_CPPFLAGS += -I..

libcpdb_frontend_la_LIBADD  = -lpthread -lm -lcrypt
libcpdb_frontend_la_LIBADD += $(GLIB_LIBS)
libcpdb_frontend_la_LIBADD += $(GIO_LIBS)
libcpdb_frontend_la_LIBADD += $(GIOUNIX_LIBS)

libcpdb_frontend_la_LDFLAGS = -no-undefined -version-info 3


cpdb_headersdir = $(includedir)/cpdb
cpdb_headers_HEADERS = cpdb.h \
		       cpdb-frontend.h \
		       backend.h \
		       frontend.h
nodist_cpdb_headers_HEADERS = backend-interface.h


backend-interface.c backend-interface.h: interface/org.openprinting.Backend.xml
	$(AM_V_GEN)gdbus-codegen --generate-c-code backend-interface \
	--interface-prefix org.openprinting \
	interface/org.openprinting.Backend.xml


genfiles = *-interface.*


clean-local:
	rm -f $(genfiles)
