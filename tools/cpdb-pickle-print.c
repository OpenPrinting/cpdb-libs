#include <stdio.h>
#include <cpdb/frontend.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage : %s filepath_to_print\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    cpdb_printer_obj_t *p = cpdbResurrectPrinterFromFile("~/.printer-pickle");
    if (p == NULL)
    {
        printf("No serialized printer found. "
               "You must first 'pickle' a printer using the "
               "'pickle-printer' command inside cpdb-text-frontend\n");
        exit(EXIT_FAILURE);
    }
    cpdbPrintFile(p, argv[1]);
}
