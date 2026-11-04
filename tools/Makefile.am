toolsdir = $(bindir)
tools_PROGRAMS = \
	cpdb-text-frontend \
	cpdb-pickle-print

cpdb_text_frontend_SOURCES = cpdb-text-frontend.c
cpdb_text_frontend_LDADD = \
	-L../cpdb/.libs \
	../cpdb/libcpdb-frontend.la \
	../cpdb/libcpdb.la \
	$(GLIB_LIBS)
cpdb_text_frontend_CFLAGS = \
	-Wno-unused-result \
	-I .. \
	$(GLIB_CFLAGS)

cpdb_pickle_print_SOURCES = cpdb-pickle-print.c
cpdb_pickle_print_LDADD = \
	-L../cpdb/.libs \
	../cpdb/libcpdb-frontend.la \
	../cpdb/libcpdb.la \
	$(GLIB_LIBS)
cpdb_pickle_print_CFLAGS = \
	-I .. \
	$(GLIB_CFLAGS)

# ================================
# Tests ("make test"/"make check")
# ================================

check_PROGRAMS = test-capability-counts

test_capability_counts_SOURCES = test-capability-counts.c
test_capability_counts_LDADD = $(cpdb_text_frontend_LDADD) $(GIO_LIBS)
test_capability_counts_CFLAGS = -I .. $(GLIB_CFLAGS) $(GIO_CFLAGS)

TESTS = \
        run-tests.sh

EXTRA_DIST = \
        run-tests.sh
