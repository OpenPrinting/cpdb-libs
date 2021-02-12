#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>

#include <cpdb-libs-frontend.h>

void display_help();
gpointer parse_commands(gpointer user_data);
FrontendObj *f;

static int add_printer_callback(PrinterObj *p)
{
    //printf("print_frontend.c : Printer %s added!\n", p->name);
    print_basic_options(p);
}

static int remove_printer_callback(PrinterObj *p)
{
    g_message("Removed Printer %s : %s!\n", p->name, p->backend_name);
}

int main(int argc, char **argv)
{
    event_callback add_cb = (event_callback)add_printer_callback;
    event_callback rem_cb = (event_callback)remove_printer_callback;

    char *dialog_bus_name = malloc(300);
    if (argc > 1) //this is for creating multiple instances of a dialog simultaneously
        f = get_new_FrontendObj(argv[1], add_cb, rem_cb);
    else
        f = get_new_FrontendObj(NULL, add_cb, rem_cb);

    /** Uncomment the line below if you don't want to use the previously saved settings**/
    //ignore_last_saved_settings(f);
    g_thread_new("parse_commands_thread", parse_commands, NULL);
    connect_to_dbus(f);
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
            disconnect_from_dbus(f);
            g_message("Stopping front end..\n");
            exit(0);
        }
        else if (strcmp(buf, "refresh") == 0)
        {
            refresh_printer_list(f);
            g_message("Getting changes in printer list..\n");
        }
        else if (strcmp(buf, "hide-remote-cups") == 0)
        {
            hide_remote_cups_printers(f);
            g_message("Hiding remote printers discovered by the cups backend..\n");
        }
        else if (strcmp(buf, "unhide-remote-cups") == 0)
        {
            unhide_remote_cups_printers(f);
            g_message("Unhiding remote printers discovered by the cups backend..\n");
        }
        else if (strcmp(buf, "hide-temporary-cups") == 0)
        {
            hide_temporary_cups_printers(f);
            g_message("Hiding remote printers discovered by the cups backend..\n");
        }
        else if (strcmp(buf, "unhide-temporary-cups") == 0)
        {
            unhide_temporary_cups_printers(f);
            g_message("Unhiding remote printers discovered by the cups backend..\n");
        }
        else if (strcmp(buf, "get-all-options") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            g_message("Getting all attributes ..\n");
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);

            if(p == NULL)
              continue;

            Options *opts = get_all_options(p);

            printf("Retrieved %d options.\n", opts->count);
            GHashTableIter iter;
            gpointer value;

            g_hash_table_iter_init(&iter, opts->table);
            while (g_hash_table_iter_next(&iter, NULL, &value))
            {
                print_option(value);
            }
        }
        else if (strcmp(buf, "get-default") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100];
            scanf("%s%s%s", option_name, printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            char *ans = get_default(p, option_name);
            if (!ans)
                printf("Option %s doesn't exist.", option_name);
            else
                printf("Default : %s\n", ans);
        }
        else if (strcmp(buf, "get-setting") == 0)
        {
            char printer_id[100], backend_name[100], setting_name[100];
            scanf("%s%s%s", setting_name, printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            char *ans = get_setting(p, setting_name);
            if (!ans)
                printf("Setting %s doesn't exist.\n", setting_name);
            else
                printf("Setting value : %s\n", ans);
        }
        else if (strcmp(buf, "get-current") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100];
            scanf("%s%s%s", option_name, printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            char *ans = get_current(p, option_name);
            if (!ans)
                printf("Option %s doesn't exist.", option_name);
            else
                printf("Current value : %s\n", ans);
        }
        else if (strcmp(buf, "add-setting") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100], option_val[100];
            scanf("%s %s %s %s", option_name, option_val, printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            printf("%s : %s\n", option_name, option_val);
            add_setting_to_printer(p, get_string_copy(option_name), get_string_copy(option_val));
        }
        else if (strcmp(buf, "clear-setting") == 0)
        {
            char printer_id[100], backend_name[100], option_name[100];
            scanf("%s%s%s", option_name, printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            clear_setting_from_printer(p, option_name);
        }
        else if (strcmp(buf, "get-state") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            printf("%s\n", get_state(p));
        }
        else if (strcmp(buf, "is-accepting-jobs") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            printf("Accepting jobs ? : %d \n", is_accepting_jobs(p));
        }
        else if (strcmp(buf, "help") == 0)
        {
            display_help();
        }
        else if (strcmp(buf, "ping") == 0)
        {
            char printer_id[100], backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            print_backend_call_ping_sync(p->backend_proxy, p->id, NULL, NULL);
        }
        else if (strcmp(buf, "get-default-printer") == 0)
        {
            char backend_name[100];
            scanf("%s", backend_name);
            /**
             * Backend name = The last part of the backend dbus service
             * Eg. "CUPS" or "GCP"
             */
            printf("%s\n", get_default_printer(f, backend_name));
        }
        else if (strcmp(buf, "print-file") == 0)
        {
            char printer_id[100], backend_name[100], file_path[200];
            scanf("%s%s%s", file_path, printer_id, backend_name);
            /**
             * Try adding some settings here .. change them and experiment
             */
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);

            if(strcmp(backend_name, "FILE") == 0)
            {
              char final_file_path[200];
              printf("Please give the final file path: ");
              scanf("%s", final_file_path);
              print_file_path(p, file_path, final_file_path);
              continue;
            }

            add_setting_to_printer(p, "copies", "3");
            print_file(p, file_path);
        }
        else if (strcmp(buf, "get-active-jobs-count") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            scanf("%s%s", printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            printf("%d jobs currently active.\n", get_active_jobs_count(p));
        }
        else if (strcmp(buf, "get-all-jobs") == 0)
        {
            int active_only;
            scanf("%d", &active_only);
            Job *j;
            int x = get_all_jobs(f, &j, active_only);
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
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            if (cancel_job(p, job_id))
                printf("Job %s has been cancelled.\n", job_id);
            else
                printf("Unable to cancel job %s\n", job_id);
        }
        else if (strcmp(buf, "pickle-printer") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char job_id[100];
            scanf("%s%s", printer_id, backend_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            pickle_printer_to_file(p, "/tmp/.printer-pickle", f);
        }
        else if (strcmp(buf, "get-human-readable-option-name") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char option_name[100];
            scanf("%s%s%s", printer_id, backend_name, option_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            printf("%s\n", get_human_readable_option_name(p, option_name));
        }
        else if (strcmp(buf, "get-human-readable-choice-name") == 0)
        {
            char printer_id[100];
            char backend_name[100];
            char option_name[100];
            char choice_name[100];
            scanf("%s%s%s%s", printer_id, backend_name, option_name, choice_name);
            PrinterObj *p = find_PrinterObj(f, printer_id, backend_name);
            printf("%s\n", get_human_readable_choice_name(p, option_name, choice_name));
        }
    }
}

void display_help()
{
    g_message("Available commands .. ");
    printf("%s\n", "stop");
    printf("%s\n", "refresh");
    printf("%s\n", "hide-remote-cups");
    printf("%s\n", "unhide-remote-cups");
    printf("%s\n", "hide-temporary-cups");
    printf("%s\n", "unhide-temporary-cups");
    //printf("%s\n", "ping <printer id> ");
    printf("%s\n", "get-default-printer <backend name>");
    printf("print-file <file path> <printer_id> <backend_name>\n");
    printf("get-active-jobs-count <printer-name> <backend-name>\n");
    printf("get-all-jobs <0 for all jobs; 1 for only active>\n");
    printf("%s\n", "get-state <printer id> <backend name>");
    printf("%s\n", "is-accepting-jobs <printer id> <backend name(like \"CUPS\")>");
    printf("%s\n", "cancel-job <job-id> <printer id> <backend name>");

    printf("get-all-options <printer-name> <backend-name>\n");
    printf("%s\n", "get-default <option name> <printer id> <backend name>");
    printf("%s\n", "get-setting <option name> <printer id> <backend name>");
    printf("%s\n", "get-current <option name> <printer id> <backend name>");
    printf("%s\n", "add-setting <option name> <option value> <printer id> <backend name>");
    printf("%s\n", "clear-setting <option name> <printer id> <backend name>");
    printf("%s\n", "get-human-readable-option-name <printer id> <backend name> <option name>");
    printf("%s\n", "get-human-readable-choice-name <printer id> <backend name> <option name> <choice name>");

    printf("pickle-printer <printer id> <backend name>\n");
}
