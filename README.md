# Frontend/Backend Communication Libraries for the Common Print Dialog Backends

This repository hosts the code for frontend and backend libraries for the Common Printing Dialog Backends (CPDB) project. These libraries allow the CPDB frontends (the print dialogs) and backends (the modules communicating with the different printing systems) to communicate with each other via D-Bus.

The frontend library also provides some extra functionality to deal with Printers, Settings, etc. in a high level manner.

## Background

The Common Print Dialog Backends (CPDB) project of OpenPrinting is about separating the print dialogs of different GUI toolkits and applications (GTK, Qt, LibreOffice, Firefox, Chromium, ...) from the different print technologies (CUPS/IPP, cloud printing services, ...) so that they can get developed independently and so always from all applications one can print with all print technologies and changes in the print technologies get supported quickly.

If one opens the print dialog, the dialog will not talk directly to CUPS, a cloud printing service, or any other printing system. For this communication there are the backends. The dialog will find all available backends and sends commands to them, for listing all available printers, giving property/option lists for the selected printer, and printing on the selected printer. This communication is done via D-Bus. So the backends are easily exchangeable and for getting support for a new print technology only its backend needs to get added.

## Dependencies

 - autoconf
 - autopoint
 - glib 2.0
 - libdbus
 - libtool

On Debian based distros, these can be installed by running: \
`sudo apt install autoconf autopoint libglib2.0-dev libdbus-1-dev libtool`

## Build and installation


    $ ./autogen.sh
    $ ./configure
    $ make
    $ sudo make install
    $ sudo ldconfig

Also install at least one of the backends (cpdb-backend-...).

## Testing the library

The project also includes a sample command line frontend (using the `cpdb-libs-frontend` API) that you can use to test whether the installed libraries and print backends work as expected.

    $ cd demo/
    $ make
    $ ./print_frontend

The list of printers from various print technologies should start appearing automatically. Type `help` to get the list of available commands. Make sure to stop the frontend using the `stop` command only.

The library also provides support for serializing a printer. Use the `pickle-printer` command to serialize it, and run the `pickle_test` executable after that to deserialize and test it.


## Using the libraries for developing print backends and dialogs.

To develop a frontend client (eg. a print dialog), use the `libcpdb` and `libcpdb-frontend` libraries.

pkg-config support: `pkg-config --cflags --libs cpdb` and `pkg-config --cflags --libs cpdb-frontend`.
Header file: `cpdb/frontend.h`

Similarly, to develop a print backend, you need to use the only the `libcpdb` library.
pkg-config support: `pkg-config --cflags --libs cpdb`.
Header file: `cpdb/backend.h` or simply only `cpdb/cpdb.h`.
