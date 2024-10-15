#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>

#include <glib.h>
#include <libintl.h>

#include <cpdb/frontend.h>

#define BUFSIZE 1024

static const char* MESSAGE_PRINTER_NOT_FOUND = "Printer not found";

void display_help();
gpointer control_thread(gpointer user_data);
gpointer background_thread(gpointer user_data);

cpdb_frontend_obj_t *f;

static void printMedia(const cpdb_media_t *media)
{
    printf("[+] Media: %s\n", media->name);
    printf("   * width = %d\n", media->width);
    printf("   * length = %d\n", media->length);
    printf(" --> Supported margins: %d\n", media->num_margins);
    printf("     left, right, top, bottom\n");
    for (int i = 0; i < media->num_margins; i++)
    {
        printf("     * %d, %d, %d, %d,\n",
               media->margins[i].left,
               media->margins[i].right,
               media->margins[i].top,
               media->margins[i].bottom);
    }
    printf("\n");
}

static void printOption(const cpdb_option_t *opt)
{
    int i;
    
    printf("[+] %s\n", opt->option_name);
    printf(" --> GROUP: %s\n", opt->group_name);
    for (i = 0; i < opt->num_supported; i++)
    {
        printf("   * %s\n", opt->supported_values[i]);
    }
    printf(" --> DEFAULT: %s\n\n", opt->default_value);
}

static void printTranslations(cpdb_printer_obj_t *p)
{
    GHashTableIter iter;
    gpointer key, value;

    if (p->locale == NULL || p->translations == NULL)
    {
        printf("No translations found\n");
        return;
    }

    g_hash_table_iter_init(&iter, p->translations);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        printf("'%s' : '%s'\n", (char *)key, (char *)value);
    }
}

static void displayAllPrinters(cpdb_frontend_obj_t *f)
{
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, f->printer);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        const cpdb_printer_obj_t *p = value;
        cpdbPrintBasicOptions(p);
    }
}

static void acquire_details_callback(cpdb_printer_obj_t *p, int success, void *user_data)
{
    if (success)
        g_message("Details acquired for %s : %s\n", p->name, p->backend_name);
    else
        g_message("Could not acquire printer details for %s : %s\n", p->name, p->backend_name);
}

static void acquire_translations_callback(cpdb_printer_obj_t *p, int success, void *user_data)
{
    if (success)
    {
        g_message("Translations acquired for %s : %s\n", p->name, p->backend_name);
        printTranslations(p);
    }
    else
        g_message("Could not acquire printer translations for %s : %s\n", p->name, p->backend_name);
}

int main(int argc, char **argv)
{
    cpdb_printer_callback printer_cb = (cpdb_printer_callback)cpdbPrinterCallback;

    setlocale (LC_ALL, "");
    cpdbInit();

    f = cpdbGetNewFrontendObj(printer_cb);

    /** Uncomment the line below if you don't want to use the previously saved settings**/
    cpdbIgnoreLastSavedSettings(f);
    // Start the control thread
    GThread *thread = g_thread_new("control_thread", control_thread, NULL);
    cpdbStartBackendListRefreshing(f);
    g_thread_join(thread);
    cpdbStopBackendListRefreshing(f);
    cpdbDeleteFrontendObj(f);

    return 0;
}

gpointer control_thread(gpointer user_data)
{
    fflush(stdout);
    char buf[BUFSIZE];

    const char *locale = g_get_language_names()[0];

    cpdbConnectToDBus(f);
    displayAllPrinters(f);

    while (1)
    {
        printf("> ");
        fflush(stdout);
        const int ret = scanf("%1023s", buf);
        if (ret == EOF || strcmp(buf, "stop") == 0)
        {
            g_message("Stopping front end..\n");
	        return (NULL);
        }
        else if (strcmp(buf, "restart") == 0)
        {
            g_message("Restarting..\n");
            cpdbDisconnectFromDBus(f);
            cpdbConnectToDBus(f);
        }
        else if (strcmp(buf, "version") == 0)
        {
            printf("CPDB v%s\n", cpdbGetVersion());
        }
        else if (strcmp(buf, "get-all-printers") == 0)
        {
            cpdbGetAllPrinters(f);

        }
        else if (strcmp(buf, "hide-remote") == 0)
        {
            cpdbHideRemotePrinters(f);
            g_message("Hiding remote printers discovered by the backend..\n");
            f->hide_remote=TRUE;
        }
        else if (strcmp(buf, "unhide-remote") == 0)
        {
            cpdbUnhideRemotePrinters(f);
            g_message("Unhiding remote printers discovered by the backend..\n");
            f->hide_remote=FALSE;
        }
        else if (strcmp(buf, "hide-temporary") == 0)
        {
            cpdbHideTemporaryPrinters(f);
            g_message("Hiding temporary printers discovered by the backend..\n");
            f->hide_temporary=TRUE;
        }
        else if (strcmp(buf, "unhide-temporary") == 0)
        {
            cpdbUnhideTemporaryPrinters(f);
            g_message("Unhiding temporary printers discovered by the backend..\n");
            f->hide_temporary=FALSE;
        }
        else if (strcmp(buf, "get-all-options") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            g_message("Getting all attributes ..\n");
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);

            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }

            cpdb_options_t *opts = cpdbGetAllOptions(p);

            printf("Retrieved %d options.\n", opts->count);
            GHashTableIter iter;
            gpointer value;

            g_hash_table_iter_init(&iter, opts->table);
            while (g_hash_table_iter_next(&iter, NULL, &value))
            {
                printOption(value);
            }
        }
        else if (strcmp(buf, "get-all-media") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            g_message("Getting all attributes ..\n");
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);

            if (!p)
            {
              puts(MESSAGE_PRINTER_NOT_FOUND);
              continue;
            }

            cpdb_options_t *opts = cpdbGetAllOptions(p);

            printf("Retrieved %d medias.\n", opts->media_count);
            GHashTableIter iter;
            gpointer value;

            g_hash_table_iter_init(&iter, opts->media);
            while (g_hash_table_iter_next(&iter, NULL, &value))
            {
                printMedia(value);
            }
        }
        else if (strcmp(buf, "get-default") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE], option_name[BUFSIZE];
            scanf("%1023s%1023s%1023s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }

            char *ans = cpdbGetDefault(p, option_name);
            if (!ans)
                printf("cpdb_option_t %s doesn't exist.", option_name);
            else
                printf("Default : %s\n", ans);
        }
        else if (strcmp(buf, "get-setting") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE], setting_name[BUFSIZE];
            scanf("%1023s%1023s%1023s", setting_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            char *ans = cpdbGetSetting(p, setting_name);
            if (!ans)
                printf("Setting %s doesn't exist.\n", setting_name);
            else
                printf("Setting value : %s\n", ans);
        }
        else if (strcmp(buf, "get-current") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE], option_name[BUFSIZE];
            scanf("%1023s%1023s%1023s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            char *ans = cpdbGetCurrent(p, option_name);
            if (!ans)
                printf("cpdb_option_t %s doesn't exist.", option_name);
            else
                printf("Current value : %s\n", ans);
        }
        else if (strcmp(buf, "add-setting") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE], option_name[BUFSIZE], option_val[BUFSIZE];
            scanf("%1023s %1023s %1023s %1023s", option_name, option_val, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            printf("%s : %s\n", option_name, option_val);
            cpdbAddSettingToPrinter(p, option_name, option_val);
        }
        else if (strcmp(buf, "clear-setting") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE], option_name[BUFSIZE];
            scanf("%1023s%1023s%1023s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            cpdbClearSettingFromPrinter(p, option_name);
        }
        else if (strcmp(buf, "get-state") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            printf("%s\n", cpdbGetState(p));
        }
        else if (strcmp(buf, "is-accepting-jobs") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            printf("Accepting jobs ? : %d \n", cpdbIsAcceptingJobs(p));
        }
        else if (strcmp(buf, "help") == 0)
        {
            display_help();
        }
        else if (strcmp(buf, "ping") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            print_backend_call_ping_sync(p->backend_proxy, p->id, NULL, NULL);
        }
        else if (strcmp(buf, "get-default-printer") == 0)
        {
            cpdb_printer_obj_t *p = cpdbGetDefaultPrinter(f);
            if (p)
                printf("%s#%s\n", p->name, p->backend_name);
            else
                printf("No default printer found\n");
        }
        else if (strcmp(buf, "get-default-printer-for-backend") == 0)
        {
            char backend_name[BUFSIZE];
            scanf("%1023s", backend_name);
            /**
             * Backend name = The last part of the backend dbus service
             * Eg. "CUPS" or "GCP"
             */
            cpdb_printer_obj_t *p = cpdbGetDefaultPrinterForBackend(f, backend_name);
            if (p)
                printf("%s\n", p->name);
            else
                printf("No default printer for backend found\n");
        }
        else if (strcmp(buf, "set-user-default-printer") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (p)
            {
                if (cpdbSetUserDefaultPrinter(p))
                    printf("Set printer as user default\n");
                else
                    printf("Couldn't set printer as user default\n");
            }
            else
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
            }
        }
        else if (strcmp(buf, "set-system-default-printer") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (p)
            {
                if (cpdbSetSystemDefaultPrinter(p))
                    printf("Set printer as system default\n");
                else
                    printf("Couldn't set printer as system default\n");
            }
            else
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
            }
        }
        else if (strcmp(buf, "print-file") == 0)
        {
            char printer_id[BUFSIZE], backend_name[BUFSIZE], file_path[BUFSIZE];
            scanf("%1023s%1023s%1023s", file_path, printer_id, backend_name);
            /**
             * Try adding some settings here .. change them and experiment
             */
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            cpdbAddSettingToPrinter(p, "copies", "3");
            cpdbPrintFile(p, file_path);
        }
        else if (strcmp(buf, "pickle-printer") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            cpdbPicklePrinterToFile(p, "/tmp/.printer-pickle", f);
        }
        else if (strcmp(buf, "get-option-translation") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            char option_name[BUFSIZE];
            scanf("%1023s%1023s%1023s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            printf("%s\n", cpdbGetOptionTranslation(p, option_name, locale));
        }
        else if (strcmp(buf, "get-choice-translation") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            char option_name[BUFSIZE];
            char choice_name[BUFSIZE];
            scanf("%1023s%1023s%1023s%1023s", option_name, choice_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            printf("%s\n", cpdbGetChoiceTranslation(p, option_name, choice_name, locale));
        }
        else if (strcmp(buf, "get-group-translation") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            char group_name[BUFSIZE];
            scanf("%1023s%1023s%1023s", group_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            printf("%s\n", cpdbGetGroupTranslation(p, group_name, locale));
        }
        else if (strcmp(buf, "get-all-translations") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            cpdbGetAllTranslations(p, locale);
            printTranslations(p);
        }
        else if (strcmp(buf, "get-media-size") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            char media[BUFSIZE];
            int width, length;
            scanf("%1023s%1023s%1023s", media, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }
            int ok = cpdbGetMediaSize(p, media, &width, &length);
            if (ok)
                printf("%dx%d\n", width, length);
        }
        else if (strcmp(buf, "get-media-margins") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            char media[BUFSIZE];
            scanf("%1023s%1023s%1023s", media, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }

            cpdb_margin_t *margins;
            int num_margins = cpdbGetMediaMargins(p, media, &margins);
            for (int i = 0; i < num_margins; i++)
                printf("%d %d %d %d\n", margins[i].left, margins[i].right, margins[i].top, margins[i].bottom);
        }
        else if (strcmp(buf, "acquire-details") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }

            g_message("Acquiring printer details asynchronously...\n");
            cpdbAcquireDetails(p, acquire_details_callback, NULL);
	}
        else if (strcmp(buf, "acquire-translations") == 0)
        {
            char printer_id[BUFSIZE];
            char backend_name[BUFSIZE];
            scanf("%1023s%1023s", printer_id, backend_name);
            
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (!p)
            {
                puts(MESSAGE_PRINTER_NOT_FOUND);
                continue;
            }

            g_message("Acquiring printer translations asynchronously...\n");
            cpdbAcquireTranslations(p, locale, acquire_translations_callback, NULL);
        }
    }
    
}

void display_help()
{
    g_message("Available commands .. ");
    printf("%s\n", "stop");
    printf("%s\n", "restart");
    printf("%s\n", "hide-remote");
    printf("%s\n", "unhide-remote");
    printf("%s\n", "hide-temporary");
    printf("%s\n", "unhide-temporary");
    printf("%s\n", "get-all-printers");
    //printf("%s\n", "ping <printer id> ");
    printf("%s\n", "get-default-printer");
    printf("%s\n", "get-default-printer-for-backend <backend name>");
    printf("%s\n", "set-user-default-printer <printer id> <backend name>");
    printf("%s\n", "set-system-default-printer <printer id> <backend name>");
    printf("%s\n", "print-file <file path> <printer_id> <backend_name>");
    printf("%s\n", "get-state <printer id> <backend name>");
    printf("%s\n", "is-accepting-jobs <printer id> <backend name(like \"CUPS\")>");
    printf("%s\n", "acquire-details <printer id> <backend name>");
    printf("%s\n", "acquire-translations <printer id> <backend name>");
    printf("%s\n", "get-all-options <printer-name> <backend-name>");
    printf("%s\n", "get-default <option name> <printer id> <backend name>");
    printf("%s\n", "get-setting <option name> <printer id> <backend name>");
    printf("%s\n", "get-current <option name> <printer id> <backend name>");
    printf("%s\n", "add-setting <option name> <option value> <printer id> <backend name>");
    printf("%s\n", "clear-setting <option name> <printer id> <backend name>");
    printf("%s\n", "get-media-size <media> <printer id> <backend name>");
    printf("%s\n", "get-media-margins <media> <printer id> <backend name>");
    printf("%s\n", "get-option-translation <option> <printer id> <backend name>");
    printf("%s\n", "get-choice-translation <option> <choice> <printer id> <backend name>");
    printf("%s\n", "get-group-translation <group> <printer id> <backend name>");
    printf("%s\n", "get-all-translations <printer id> <backend name>");
    printf("%s\n", "pickle-printer <printer id> <backend name>\n");
}
