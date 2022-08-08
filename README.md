# Frontend/Backend Communication Libraries for the Common Print Dialog Backends

This repository hosts the code for frontend and backend libraries for the Common Printing Dialog Backends (CPDB) project. These libraries allow the CPDB frontends (the print dialogs) and backends (the modules communicating with the different .printing systems) too communicate with each other via D-Bus.
The frontend library also provides some extra functionality to deal with Printers, Settings, etc. in a high level manner.

## Background

The Common Printing Dialog Backends (CPDB) project of OpenPrinting is about separating the print dialogs of different GUI toolkits and applications (GTK, Qt, LibreOffice, ...) from the different print technologies (CUPS/IPP, Google Cloud Print, ...) so that they can get developed independently and so always from all applications one can print with all print technologies and changes in the print technologies get supported quickly.

If one opens the print dialog, the dialog will not talk directly to CUPS, Google Cloud Print, or any other printing system. For this communication there are the backends. The dialog will find all available backend and sends commands to them, for listing all available printers, giving property/option lists for the selected printers, and printing on the selcted printer. This communication is done via D-Bus. So the backends are easily exchangeable and for getting support for a new print technology only its backend needs to get added.

## Dependencies

 - GLIB 2.0 :
`sudo apt install libglib2.0-dev`

 - LIBTOOL :
`sudo apt install libtool`
 
## Build and installation


    $ ./autogen
    $ ./configure
    $ make
    $ sudo make install
    $ sudo ldconfig


Use the sample frontend client to check that the library and the installed backends work as expected:

## Testing the library

The project also includes a sample command line frontend (using the `cpdb-libs-frontend` API) that you can use to test whether the installed libraries and print backends work as expected.

    $ cd demo/
    $ make
    $ ./print_frontend

The list of printers from various printers should start appearing automatically. Type `help` to get the list of available commands. Make sure to stop the frontend using the `stop` command only.

The library also provides support for serializing a printer. Use the `pickle-printer` command to serialize it, and run the `pickle_test` executable after that to deserialize and test it.
    

## Using the libraries for devloping print backends and dialogs.

To develop a frontend client(Eg. a print dialog), use the `cpdb-libs-frontend` library.

pkg-config support: `pkg-config --cflags --libs cpdb-libs-frontend`.
Header file :  `cpdb-libs-frontend.h` 

Similarly, to develop a print backend, you need to use the `cpdb-libs-backend` library.
pkg-config support: `pkg-config --cflags --libs cpdb-libs-backend`.
Header file: `cpdb-libs-backend.h` in your code.
