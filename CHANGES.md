# CHANGES - Common Print Dialog Backends - Libraries - v2.0b8 - 2025-12-10

## CHANGES IN V2.0b8 (10th December 2025)

- Bump soname of both libraries from 2 to 3
  Usually, we do consider an API stable before the first release
  candidate, so we removed some API functions between the 2.0b5
  and 2.0b7 releases, but some distros included 2.0b5 and it so
  we are bumping the soname to make distro's build systems happy.
  (Pull request #84)

- Move freeing of printer objects from cpdbPrinterCallback() to
  cpdbOnPrinterRemoved() for API consistence
  (Issue #79, Pull request #83)

- Allow NULL for the callback in cpdbGetNewFrontendObj()
  If NULL is supplied for the callback, the default callback
  cpdbPrinterCallback() is used, which just logs the printer updates.
  (Pull request #82)


## CHANGES IN V2.0b7 (19th February 2025)

- Add capability to check CPDB version at runtime
  This allows third-party applications to easily manage different CPDB
  releases. This is especially important when shipping pre-built
  binaries of such applications implementing CPDB, as the end users
  may be running outdated and potentially vulnerable versions of CPDB
  (Pull request #49).

- Allow to extract translations from table
  (Pull request #58)

- `cpdb-text-frontend`: Get locale via GLib
  Instead of only supporting the LANGUAGE environment variable that's
  not necessarily set, use GLib's `g_get_language_names` to detect the
  locale to use in the text frontend which takes other environment
  variables into account as well (Pull request #51).

- `cpdb-text-frontend`: Quit on EOF (Ctrl + D)
  Quit the text frontend when scanf returns EOF, which e.g. happens
  when the user presses Ctrl + D. Previously, pressing Ctrl + D would
  result in the loop running indefinitely, printing ">" without end
  (Pull request #38).

- Added API function `cpdbRefreshPrinterList()` to refresh the printer list
  (Pull request #35)

- New API function `cpdbPrinterCallback()`
  - Was `printer_callback()` in `cpdb-text-frontend`
  (Pull request #36)

- Turn cpdbFillBasicOptions() into a frontend API function
  (Pull request #37)

- Turn some static functions into frontend API functions
  - `on_printer_added()` -> `cpdbOnPrinterAdded()`
  - `on_printer_removed()` -> `cpdbOnPrinterRemoved()`
  - `on_printer_state_changed()` -> `cpdbOnPrinterStateChanged()`
  (Pull request #36)

- Turned static function `get_dbus_connection()` into API function
  `cpdbGetDbusConnection()`
  (Pull request #35)

- Removed unused parameter `instance_name` from API functions
  `cpdbGetNewFrontendObj()` and `cpdbStartListingPrinters()`
  (Pull request #41)

- `cpdb-text-frontend`: Add null check for get-default-printer
  Instead of crashing, asking for the default printer for a
  non-existing backend now prints a decent error message (Pull request
  #50).

- `cpdbConcatPath()`: Actually check `XDG_CONFIG_DIRS`
  Instead of using `CPDB_SYSCONFDIR` again as it was already done
  earlier, and doing so in a loop, use the actual path extracted from
  the `XDG_CONFIG_DIRS` environment variable (Pull request #48).

- frontend: Check file before creating print job
  Try to open the file to be printed at the given file path before
  creating a print job using `cpdbPrintFD()`, to avoid creation of a
  "dead" job which needs to get manually canceled (Pull request #78).

- Correct DBus calls to add, delete, and state change of printers
  (Pull request #44)

- Pass backend name to `cpdbRefreshPrinterList()` as `const char*`
  Use `const char*` instead of `char*` for the `backend` param of
  `cpdbRefreshPrinterList()`. `const char*` is the usual way to pass C
  strings. That also simplifies using the function (Pull request #40).

- `cpdb-text-frontend`: Don't crash when printer doesn't exist
  Instead of crashing due to a NULL dereference, print a message and
  skip further processing of the current command when
  `cpdbFindPrinterObj` returns NULL, e.g. because the user has entered
  the name of a nonexisting printer or backend (Pull request #39).

- Replace `cpdbGetStringCopy()` with `g_strdup()`
  GLib's `g_strdup()` already provides the same functionality as
  `cpdbGetStringCopy()`, so there's no need to have a custom
  implementation and even make that part of the public API (Pull
  request #42).

- Replace `cpdbConcat()` with `g_strconcat()`
  GLib's `g_strconcat` already provides the functionality to
  concatenate strings, so there's no need to have `cpdbConcat` as a
  custom implementation and even make that part of the public API
  (Pull request #55).

- Drop several unused variables
  (Pull requests #52, #64, #68)

- Drop unused `cpdb_job_t` and `cpdbUnpackJobArray()`
  Got unused since we stream the print data through a Unix domain
  socket (Pull request #30). Removed with pull request #73.

- Drop unused `cpdbExtractFileName()` (Pull request #56)

- Drop unused `own_id` member of frontend data structure
  (Pull request #46)

- Fixed tons of memory leaks
  - `cpdbGetSysConfDir()` (Pull request #47)
  - `cpdbUnpackOptions()` (Pull request #53)
  - `cpdb-text-frontend`: Fix memory leak in `add-setting()` (Pull request #54)
  - `cpdbGetDefaultPrinterForBackend()` (Pull request #57)
  - `cpdbCreateBackend()` (Pull request #59)
  - `cpdbRefreshPrinterList()` (Pull request #60)
  - Fixing a memory leak in `f->printer` hasg table (Pull request #61)
  - `cpdbPrintFileWithJobTitle()` (Pull request #62)
  - `cpdb-text-frontend`: Fix memory leak in `print-file()` (Pull request #63)
  - Unref `GVariantBuilder` to fix memory leak (Pull request #65)
  - `cpdbActivateBackends()` (Pull request #66)
  - Log and free GError in call to `doListing` (Pull request #67)
  - Delete printer objects when deleting hash table (Pull request #71)
  - `cpdbDeletePrinterObj()` (Pull request #72)
  - `cpdbDeleteOption()` (Pull request #74)
  - More in `cpdb-text-frontend` (Pull requests #75, #76)
  - `cpdbGet...Translation()` (Pull request #77)

- Fix potential double-free in `cpdbResurrectPrinterFromFile()`
  (Pull request #69)

- README.md: Explained inner workings of CPDB and gave containerization hints

- Documentation for newly added API functions
  Added header comments for auto-generating developer documentation
  (Pull request #43).


## CHANGES IN V2.0b6 (18th June 2024)

 - Stream print data through a Unix domain socket
   To ease making a Snap from the CPDB backend for CUPS (and other
   CPDB backends in the future) we now transfer the print job data
   from the dialog to the backend via a Unix domain socket and not by
   dropping the data into a file (PR #30).

 - Add newly appearing backends while the dialog is open
   CPDB Backends can get installed or removed at any time, also while
   a print dialog is open. Now a background thread is added to observe
   the come and go of backends and to update the printer list
   appropriately. New API functions are
   cpdbStartBackendListRefreshing() and cpdbStopBackendListRefreshing,
   to start and stop this thread (PR #32).

 - Added support for CPDB backends running permanently
   Do not only find backends as registered D-Bus services (*.service) files
   but also permanently running backends which are not necessarily registered
   D-Bus services

 - Let the frontend not be a D-Bus service, only the backends
   To control hiding temporary or remote printers in the backend's
   printer list we have added methods to the D-Bus service provided by
   the backends now. Before, the frontends were also D-Bus services
   just to send signals to the backends for controling the filtering
   (PR #32).

 - Convenience API functions to start/stop listing printers
   Added convenience API functions cpdbStartListingPrinters() and
   cpdbStopListingPrinters() to be called when opening and closing the
   print dialog, resp. (PR #32).

 - Removed API functions cpdbGetAllJobs(), cpdbGetActiveJobsCount(),
   cpdbCancelJob(), and cpdbPrintFilePath() and the corresponding
   D-Bus methods (PR #30).

 - Removed support for the "FILE" CPDB backend (PR #30).

 - cpdbActivateBackends(): Fixed crash caused by wrong unreferencing

 - cpdbConnectToDBus(): Use g_main_context_get_thread_default() for wait loop
   g_main_context_get_thread_default() always returns a valid context and
   never NULL. This way we avoid crashes.

 - Fixed crash when Handling saved settings
   Let cpdbReadSettingsFromDisk() return an empty data structure
   instead of NULL when there are no saved settings. Also let
   cpdbIgnoreLastSavedSettings() always output and empty data
   structure.

 - Removed the commands, "get-all-jobs", "get-active-jobs-count", and
   "cancel-job" from the "cpdb-text-frontend" utility (PR #30).

 - cpdb-text-frontend: Removed unnecessary g_main_loop
   The g_main_loop is not actually needed, the main thread can just wait
   for the background thread using g_thread_join().

 - cpdb-text-frontend: Shut down cleanly with "stop" command
   Instead of committing suicide with "exit(0);" we actually quit the
   main loop now.

 - cpdb-text-frontend: Spawn command interpreter not before the start
   of the main loop, to assure that "stop" command quits main loop
   and we do not fall into an infinite loop.

 - cpdb-text-frontend: acquire_translations_callback(): Only issue the
   success message and list the translation if the loading of the
   translations actually succeeded.


## CHANGES IN V2.0b5 (2nd August 2023)

 - Removed browsing for backends via file system
   The frontend should only shout into the D-Bus to find out which
   backends are available and to communicate with them. Depending on
   the way (for example sandboxed packaging, like Snap) how the
   frontend and backand are installed the frontend cannot access the
   host's or the backend's file systems (PR #27).

 - Limit scanned string length in `scanf()`/`fscanf()` functions
   cpdb-libs uses the `fscanf()` and `scanf()` functions to parse
   command lines and configuration files, dropping the read string
   components into fixed-length buffers, but does not limit the length
   of the strings to be read by `fscanf()` and `scanf()` causing
   buffer overflows when a string is longer than 1023 characters
   (CVE-2023-34095).

 - Fixed memory bugs leading to leaks and crashes (PR #26)

 - Build system: Removed unnecessary lines in `tools/Makefile.am`
   Removed the `TESTdir` and `TEST_SCRIPTS` entries in
   `tools/Makefile.am`.  They are not needed and let `make install`
   try to install `run-tests.sh` in the source directory, where it
   already is, causing an error.


## CHANGES IN V2.0b4 (20th March 2023)

 - Added test script for `make test`/`make check`
   The script tools/run-tests.sh runs the `cpdb-text-frontend` text mode
   example frontend and stops it by supplying "stop" to its standard
   input.

 - Allow changing the backend info directory via env variable
   To make it possible to test backends which are not installed into
   the system, one can now set the environment variable
   CPDB_BACKEND_INFO_DIR to the directory where the backend info file
   for the backend is, for example in its source tree.

 - Install sample frontend with `make install`
   We use the sample frontend `cpdb-text-frontend` for several tests now,
   especially "make check" and also the autopkgtests in the
   Debian/Ubuntu packages. They are also useful for backend developers
   for manual testing.

 - Renamed develping/debug tools
   As we install the development and debugging tools now, they should
   be more easily identifiable as part of CPDB. Therefore they get
   `cpdb-`-prefixed names.

 - `cpdb-text-frontend`: Use larger and more easily adjustable string
   buffers

 - Fixed segfault in the frontend library
   `cpdbResurrectPrinterFromFile()`, when called with an invalid file
   name, caused a crash.


## CHANGES IN V2.0b3 (20th February 2023)

- Added functions to fetch all printer strings translations (PR #23)
  * Added `cpdbGetAllTranslations()` to synchronously fetch all
    printer string translations
  * Added `cpdbAcquireTranslations()` to asychronously fetch them.
  * Removed `get-human-readable-option`/`choice-name` methods
  * Removed `cpdb_async_obj_t` from `cpdb-frontend.h` as that is meant
    for internal use.


## CHANGES IN V2.0b2 (13th February 2023)

- Options groups: To allow better structuring of options in print
  dialogs, common options are categorized in groups, like media, print
  quality, color, finishing, ... This can be primarily done by the
  backends but the frontend library can do fallback/default
  assignments for options not assigned by the backend.

- Many print dialogs have a "Color" option group (usually shown on one
  tab), so also have one in cpdb-libs to match with the dialogs and
  more easily map the options into the dialogs.

- Add macros for new options and choices, also add "Color" group

- Synchronous printer fetching upon backend activation (PR #21) Made
  `cpdbConnectToDbus()` wait until all backends activate

- Backends will automatically signal any printer updates instead of
  the frontend having to manually ask them (PR #21)

- Add `printer-state-changed` signal (PR #21)
  * Changed function callback type definition for printer updates
  * Added callback to frontends for changes in printer state

- Translation support: Translations of option, choice, and group names
  are now supported, not only English human-readable strings. And
  Translations can be provided by the backends (also polling them from
  the print service) and by the frontend library.

- Use autopoint instead of gettextize

- Enable reconnecting to dbus (PR #14)

- Debug logging: Now backends forward their log messages to the
  frontend library for easier debugging.

- Use <glib/gi18n.h> instead of redefining macros (Issue #20)

- Add functions to free objects (PR #15)

- Remove hardcoded paths and follow XDG base dir specs (PR #14)

- Added javadoc comments to function declarations (PR #21)

- Build system: Let "make dist" also create .tar.bz2 and .tar.xz

- Add the dependency on libdbus to README.md
  libdbus-1-dev is needed to configure pkg-config variables for
  backends

- COPYING: Added Priydarshi Singh


## CHANGES IN V2.0b1 (11th December 2022)

- Added interfaces to get human readable option and settings names
    
  Print attributes/options and their choices are usually defined in a
  machine-readable form which is more made for easy typing in a
  command line, not too long, no special characters, always in English
  and human-readable form for GUI (print dialogs), more verbose for
  easier understanding, with spaces and other special characters,
  translated, ...

  Older backends without human-readable strings can still be used. In
  such a case it is recommended that the dialog does either its own
  conversion or simply shows the machine-readable string as a last
  mean.

- Added get_media_size() function to retrieve media dimensions for a
  given "media" option value

- Support for media sizes to have multiple margin variants (like
  standard and borderless)

- Support for configurable user and system-wide default printers

- Acquire printer details asynchronously (non blocking)

- Made cpdb-libs completely CUPS-neutral

  Removed CUPS-specific functions from the frontend library functions
  and the dependency on libcups, renamed CUPS-base function and signal
  names

- Use "const" qualifiers on suitable function parameters

- DBG_LOG now includes error-message

- Renamed all API functions, data types and constants
    
  To make sure that the resources of libcpdb and libcpdb-frontend do
  not conflict with the ones of any other library used by a frontend
  or backend created with CPDB, all functions, data types, and
  constants of CPDB got renamed to be unique to CPDB.
    
  Here we follow the rules of CUPS and cups-filters (to get unique
  rules for all libraries by OpenPrinting): API functions are in
  camelCase and with "cpdb" prefix, data types all-lowercase, with '_'
  as word separator, and constants are all-uppercase, also with '_' as
  word separator, and with "CPDB_" prefix.

- Renamed and re-organized source files to make all more
  standards-conforming and naming more consistent.
    
- All headers go to /usr/include/cpdb now: Base API headers cpdb.h and
  cpdb-frontend.h, interface headers (and also part of the API)
  backend-interface.h and frontend-interface.h, and the convenience
  header files backend.h and frontend.h (include exactly the headers
  needed).
    
- Bumped soname of the libraries to 2.
    
- Check settings pointer for NULL before freeing it

- NULL check on input parameters for many functions

- Fixed incompatibility with newer versions of glib()

  glib.h cannot get included inside 'extern "C" { ... }'

- Corrected AC_INIT() in configure.ac: Bug report destination,
  directory for "make dist".

- README.md: Fixed typos and updated usage instructions

- Updated .gitignore

