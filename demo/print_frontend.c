#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>

#include <cpdb/frontend.h>

void display_help();
gpointer parse_commands(gpointer user_data);
cpdb_frontend_obj_t *f;

static int add_printer_callback(cpdb_printer_obj_t *p)
{
    //printf("print_frontend.c : Printer %s added!\n", p->name);
    cpdbPrintBasicOptions(p);
}

static int remove_printer_callback(cpdb_printer_obj_t *p)
{
    g_message("Removed Printer %s : %s!\n", p->name, p->backend_name);
}

static void acquire_details_callback(cpdb_printer_obj_t *p, int success, void *user_data)
{
	if (success)
		g_message("Details acquired for %s : %s\n", p->name, p->backend_name);
	else
		g_message("Could not acquire printer details for %s : %s\n", p->name, p->backend_name);
}

int main(int argc, char **argv)
{
    cpdb_event_callback add_cb = (cpdb_event_callback)add_printer_callback;
    cpdb_event_callback rem_cb = (cpdb_event_callback)remove_printer_callback;

    char *dialog_bus_name = malloc(300);
    if (argc > 1) //this is for creating multiple instances of a dialog simultaneously
        f = cpdbGetNewFrontendObj(argv[1], add_cb, rem_cb);
    else
        f = cpdbGetNewFrontendObj(NULL, add_cb, rem_cb);

    /** Uncomment the line below if you don't want to use the previously saved settings**/
    //cpdbIgnoreLastSavedSettings(f);
    g_thread_new("parse_commands_thread", parse_commands, NULL);
    cpdbConnectToDBus(f);
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
}

gpointer parse_commands(gpointer user_data)
{
    printf("parse_commands\n");
    fflush(stdout);
    char buf[100];
    while (1)
    {
        printf("> ");
        fflush(stdout);
        scanf("%s", buf);
        if (strcmp(buf, "stop") == 0)
        {
            cpdbDisconnectFromDBus(f);
            g_message("Stopping front end..\n");
            exit(0);
        }
        else if (strcmp(buf, "refresh") == 0)
        {
            cpdbRefreshPrinterList(f);
            g_message("Getting changes in printer list..\n");
        }
        else if (strcmp(buf, "hide-remote") == 0)
        {
            cpdbHideRemotePrinters(f);
            g_message("Hiding remote printers discovered by the backend..\n");
        }
        else if (strcmp(buf, "unhide-remote") == 0)
        {
            cpdbUnhideRemotePrinters(f);
            g_message("Unhiding remote printers discovered by the backend..\n");
        }
        else if (strcmp(buf, "hide-temporary") == 0)
        {
            cpdbHideTemporaryPrinters(f);
            g_message("Hiding remote printers discovered by the backend..\n");
        }
        else if (strcmp(buf, "unhide-temporary") == 0)
        {
            cpdbUnhideTemporaryPrinters(f);
            g_message("Unhiding remote printers discovered by the backend..\n");
        }
        else if (strcmp(buf, "get-all-options") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            g_message("Getting all attributes ..\n");
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);

            if(p == NULL)
              continue;

            cpdb_options_t *opts = cpdbGetAllOptions(p);

            printf("Retrieved %d options.\n", opts->count);
            GHashTableIter iter;
            gpointer value;

            g_hash_table_iter_init(&iter, opts->table);
            while (g_hash_table_iter_next(&iter, NULL, &value))
            {
                cpdbPrintOption(value);
            }
        }
        else if (strcmp(buf, "get-default") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100];
            scanf("%s%s%s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            char *ans = cpdbGetDefault(p, option_name);
            if (!ans)
                printf("cpdb_option_t %s doesn't exist.", option_name);
            else
                printf("Default : %s\n", ans);
        }
        else if (strcmp(buf, "get-setting") == 0)
        {
            char printer_id[100], backend_name[100], setting_name[100];
            scanf("%s%s%s", setting_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            char *ans = cpdbGetSetting(p, setting_name);
            if (!ans)
                printf("Setting %s doesn't exist.\n", setting_name);
            else
                printf("Setting value : %s\n", ans);
        }
        else if (strcmp(buf, "get-current") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100];
            scanf("%s%s%s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            char *ans = cpdbGetCurrent(p, option_name);
            if (!ans)
                printf("cpdb_option_t %s doesn't exist.", option_name);
            else
                printf("Current value : %s\n", ans);
        }
        else if (strcmp(buf, "add-setting") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100], option_val[100];
            scanf("%s %s %s %s", option_name, option_val, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            printf("%s : %s\n", option_name, option_val);
            cpdbAddSettingToPrinter(p, cpdbGetStringCopy(option_name), cpdbGetStringCopy(option_val));
        }
        else if (strcmp(buf, "clear-setting") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100];
            scanf("%s%s%s", option_name, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            cpdbClearSettingFromPrinter(p, option_name);
        }
        else if (strcmp(buf, "get-state") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            printf("%s\n", cpdbGetState(p));
        }
        else if (strcmp(buf, "is-accepting-jobs") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            printf("Accepting jobs ? : %d \n", cpdbIsAcceptingJobs(p));
        }
        else if (strcmp(buf, "help") == 0)
        {
            display_help();
        }
        else if (strcmp(buf, "ping") == 0)
        {
            char printer_id[100], backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            print_backend_call_ping_sync(p->backend_proxy, p->id, NULL, NULL);
        }
        else if (strcmp(buf, "get-default-printer") == 0)
        {
            cpdb_printer_obj_t *p = cpdbGetDefaultPrinter(f);
            printf("%s#%s\n", p->name, p->backend_name);
        }
        else if (strcmp(buf, "get-default-printer-for-backend") == 0)
        {
            char backend_name[100];
            scanf("%s", backend_name);
            /**
             * Backend name = The last part of the backend dbus service
             * Eg. "CUPS" or "GCP"
             */
            printf("%s\n", cpdbGetDefaultPrinterForBackend(f, backend_name));
        }
        else if (strcmp(buf, "make-user-default-printer") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (p)
            {
                if (cpdbMakeUserDefault(p))
                    printf("Marked printer as user default\n");
                else
                    printf("Couldn't mark printer as user default\n");
            }
        }
        else if (strcmp(buf, "make-system-default-printer") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (p)
            {
                if (cpdbMakeSystemDefault(p))
                    printf("Marked printer as system default\n");
                else
                    printf("Couldn't mark printer as system default\n");
            }
        }
        else if (strcmp(buf, "print-file") == 0)
        {
            char printer_id[100], backend_name[100], file_path[200];
            scanf("%s%s%s", file_path, printer_id, backend_name);
            /**
             * Try adding some settings here .. change them and experiment
             */
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);

            if(strcmp(backend_name, "FILE") == 0)
            {
              char final_file_path[200];
              printf("Please give the final file path: ");
              scanf("%s", final_file_path);
              cpdbPrintFilePath(p, file_path, final_file_path);
              continue;
            }

            cpdbAddSettingToPrinter(p, "copies", "3");
            cpdbPrintFile(p, file_path);
        }
        else if (strcmp(buf, "get-active-jobs-count") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            printf("%d jobs currently active.\n", cpdbGetActiveJobsCount(p));
        }
        else if (strcmp(buf, "get-all-jobs") == 0)
        {
            int active_only;
            scanf("%d", &active_only);
            cpdb_job_t *j;
            int x = cpdbGetAllJobs(f, &j, active_only);
            printf("Total %d jobs\n", x);
            int i;
            for (i = 0; i < x; i++)
            {
                printf("%s .. %s  .. %s  .. %s  .. %s\n", j[i].job_id, j[i].title, j[i].printer_id, j[i].state, j[i].submitted_at);
            }
        }
        else if (strcmp(buf, "cancel-job") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char job_id[100];
            scanf("%s%s%s", job_id, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if (cpdbCancelJob(p, job_id))
                printf("cpdb_job_t %s has been cancelled.\n", job_id);
            else
                printf("Unable to cancel job %s\n", job_id);
        }
        else if (strcmp(buf, "pickle-printer") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char job_id[100];
            scanf("%s%s", printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            cpdbPicklePrinterToFile(p, "/tmp/.printer-pickle", f);
        }
        else if (strcmp(buf, "get-human-readable-option-name") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char option_name[100];
            scanf("%s%s%s", printer_id, backend_name, option_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            printf("%s\n", cpdbGetHumanReadableOptionName(p, option_name));
        }
        else if (strcmp(buf, "get-human-readable-choice-name") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char option_name[100];
            char choice_name[100];
            scanf("%s%s%s%s", printer_id, backend_name, option_name, choice_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            printf("%s\n", cpdbGetHumanReadableChoiceName(p, option_name, choice_name));
        }
        else if (strcmp(buf, "get-media-size") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char media[100];
            int width, length;
            scanf("%s%s%s", media, printer_id, backend_name);
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            cpdbGetMediaSize(p, media, &width, &length);
            printf("%dx%d\n", width, length);
        }
        else if (strcmp(buf, "get-media-margins") == 0)
        {
			char printer_id[100];
			char backend_name[100];
			char media[100];
			scanf("%s%s%s", media, printer_id, backend_name);
			cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
			
			cpdb_margin_t *margins;
			int num_margins = cpdbGetMediaMargins(p, media, &margins);
			for (int i = 0; i < num_margins; i++)
				printf("%d %d %d %d\n", margins[i].left, margins[i].right, margins[i].top, margins[i].bottom);
		}
        else if (strcmp(buf, "acquire-details") == 0)
        {
			char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            
            cpdb_printer_obj_t *p = cpdbFindPrinterObj(f, printer_id, backend_name);
            if(p == NULL)
              continue;

			g_message("Acquiring printer details asynchronously...\n");
            cpdbAcquireDetails(p, acquire_details_callback, NULL);
		}
    }
}

void display_help()
{
    g_message("Available commands .. ");
    printf("%s\n", "stop");
    printf("%s\n", "refresh");
    printf("%s\n", "hide-remote");
    printf("%s\n", "unhide-remote");
    printf("%s\n", "hide-temporary");
    printf("%s\n", "unhide-temporary");
    //printf("%s\n", "ping <printer id> ");
    printf("%s\n", "get-default-printer <backend name>");
    printf("%s\n", "get-default-printer-for-backend <backend name>");
    printf("%s\n", "make-user-default-printer <printer id> <backend name>");
    printf("%s\n", "make-system-default-printer <printer id> <backend name>");
    printf("%s\n", "print-file <file path> <printer_id> <backend_name>");
    printf("%s\n", "get-active-jobs-count <printer-name> <backend-name>");
    printf("%s\n", "get-all-jobs <0 for all jobs; 1 for only active>");
    printf("%s\n", "get-state <printer id> <backend name>");
    printf("%s\n", "is-accepting-jobs <printer id> <backend name(like \"CUPS\")>");
    printf("%s\n", "cancel-job <job-id> <printer id> <backend name>");

	printf("%s\n", "acquire-details <printer id> <backend name>");
    printf("%s\n", "get-all-options <printer-name> <backend-name>");
    printf("%s\n", "get-default <option name> <printer id> <backend name>");
    printf("%s\n", "get-setting <option name> <printer id> <backend name>");
    printf("%s\n", "get-current <option name> <printer id> <backend name>");
    printf("%s\n", "add-setting <option name> <option value> <printer id> <backend name>");
    printf("%s\n", "clear-setting <option name> <printer id> <backend name>");
    printf("%s\n", "get-media-size <media> <printer id> <backend name>");
    printf("%s\n", "get-media-margins <media> <printer id> <backend name>");
    printf("%s\n", "get-human-readable-option-name <printer id> <backend name> <option name>");
    printf("%s\n", "get-human-readable-choice-name <printer id> <backend name> <option name> <choice name>");

    printf("pickle-printer <printer id> <backend name>\n");
}
