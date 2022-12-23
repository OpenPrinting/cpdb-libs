#include "cpdb.h"

/**
 * If CPDB has been initialized
 */
volatile gboolean initialized = FALSE;

/**
 * Table matching common IPP options prefix to groups
 */
const char *cpdbGroupTable[][2] = {

    {CPDB_OPTION_COPIES,                        CPDB_GROUP_COPIES},
    {CPDB_OPTION_COLLATE,                       CPDB_GROUP_COPIES}, 
    {CPDB_OPTION_COPIES_SUPPORTED,              CPDB_GROUP_COPIES},

    {CPDB_OPTION_MEDIA,                         CPDB_GROUP_MEDIA},
    {CPDB_OPTION_MEDIA_TYPE,                    CPDB_GROUP_MEDIA},

    {CPDB_OPTION_SIDES,                         CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_MIRROR,                        CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_BOOKLET,                       CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_PAGE_SET,                      CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_NUMBER_UP,                     CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_PAGE_BORDER,                   CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_PAGE_RANGES,                   CPDB_GROUP_PAGE_MGMT},
    {CPDB_OPTION_ORIENTATION,                   CPDB_GROUP_PAGE_MGMT},

    {CPDB_OPTION_POSITION,                      CPDB_GROUP_SCALING},
    {CPDB_OPTION_PRINT_SCALING,                 CPDB_GROUP_SCALING},
    {CPDB_OPTION_FIDELITY,                      CPDB_GROUP_SCALING},

    {CPDB_OPTION_PRINT_QUALITY,                 CPDB_GROUP_QUALITY},
    {CPDB_OPTION_RESOLUTION,                    CPDB_GROUP_QUALITY},
    {CPDB_OPTION_COLOR_MODE,                    CPDB_GROUP_QUALITY},
    
    {CPDB_OPTION_FINISHINGS,                    CPDB_GROUP_FINISHINGS},
    {CPDB_OPTION_OUTPUT_BIN,                    CPDB_GROUP_FINISHINGS},
    {CPDB_OPTION_PAGE_DELIVERY,                 CPDB_GROUP_FINISHINGS},

    {CPDB_OPTION_JOB_NAME,                      CPDB_GROUP_JOB_MGMT},
    {CPDB_OPTION_JOB_SHEETS,                    CPDB_GROUP_JOB_MGMT},
    {CPDB_OPTION_JOB_PRIORITY,                  CPDB_GROUP_JOB_MGMT},
    {CPDB_OPTION_JOB_HOLD_UNTIL,                CPDB_GROUP_JOB_MGMT},
    
};

void cpdbInit()
{
    if (!initialized)
    {
        bindtextdomain(CPDB_GETTEXT_PACKAGE, CPDB_LOCALEDIR);
        initialized = TRUE;
    }
}

gboolean cpdbGetBoolean(const char *g)
{
    if (!g)
        return FALSE;

    if (g_str_equal(g, "true"))
        return TRUE;

    return FALSE;
}

char *cpdbConcat(const char *s1, const char *s2)
{
    if (s1 == NULL)
        return cpdbGetStringCopy(s2);
    if (s2 == NULL)
        return cpdbGetStringCopy(s1);

    char *s = malloc(strlen(s1) + strlen(s2) + 1);
    sprintf(s, "%s%s", s1, s2);
    return s;
}

char *cpdbConcatSep(const char *s1, const char *s2)
{
    char *s = malloc(strlen(s1) + strlen(s2) + 2);
    sprintf(s, "%s#%s", s1, s2);
    return s;
}

char *cpdbConcatPath(const char *s1, const char *s2)
{
    char *s = malloc(strlen(s1) + strlen(s2) + 2);
    if (s1[strlen(s1) - 1] == '/')
        sprintf(s, "%s%s", s1, s2);
    else
        sprintf(s, "%s/%s", s1, s2);
    return s;
}

char *cpdbGetStringCopy(const char *str)
{
    if (str == NULL)
        return NULL;
    char *s = malloc(sizeof(char) * (strlen(str) + 1));
    strcpy(s, str);
    return s;
}

void cpdbUnpackStringArray(GVariant *variant, int num_val, char ***val)
{
    GVariantIter *iter;

    if (!num_val)
    {
        printf("No supported values found.\n");
        *val = NULL;
        return;
    }
    char **array = (char **)malloc(sizeof(char *) * num_val);

    g_variant_get(variant, "a(s)", &iter);

    int i = 0;
    char *str;
    for (i = 0; i < num_val; i++)
    {
        g_variant_iter_loop(iter, "(s)", &str);
        array[i] = cpdbGetStringCopy(str);
        printf(" %s\n", str);
    }
    *val = array;
}

GVariant *cpdbPackStringArray(int num_val, char **val)
{
    GVariantBuilder *builder;
    GVariant *values;
    builder = g_variant_builder_new(G_VARIANT_TYPE("a(s)"));
    for (int i = 0; i < num_val; i++)
    {
        // g_message("%s", val[i]);
        g_variant_builder_add(builder, "(s)", val[i]);
    }

    if (num_val == 0)
        g_variant_builder_add(builder, "(s)", "NA");

    values = g_variant_new("a(s)", builder);
    return values;
}

GVariant *cpdbPackMediaArray(int num_val, int (*margins)[4])
{
	GVariantBuilder *builder;
	GVariant *values;
	builder = g_variant_builder_new(G_VARIANT_TYPE("a(iiii)"));
	for (int i = 0; i < num_val; i++)
	{
		g_variant_builder_add(builder, "(iiii)", 
							  margins[i][0], margins[i][1], margins[i][2], margins[i][3]);
	}
	
	values = g_variant_new("a(iiii)", builder);
	return values;
}

char *cpdbGetAbsolutePath(const char *file_path)
{
    if (!file_path)
        return NULL;

    if (file_path[0] == '/')
        return cpdbGetStringCopy(file_path);

    if (file_path[0] == '~')
    {
        struct passwd *passwdEnt = getpwuid(getuid());
        char *home = passwdEnt->pw_dir;
	if (home == NULL)
	  return NULL;
	if (file_path[1] != '/' && file_path[1] != '\0')
	  return NULL;
	char *fp = malloc(sizeof(char) * (strlen(home) + strlen(file_path)));
	if (fp == NULL)
	  return NULL;
        sprintf(fp, "%s%s", home, file_path + 1);
        return fp;
    }
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL)
      return NULL;
    char *fp = malloc(sizeof(char) * (strlen(cwd) + strlen(file_path) + 2));
    if (fp == NULL)
      return NULL;
    sprintf(fp, "%s/%s", cwd, file_path);
    free(cwd);
    printf("%s\n", fp);
    return fp;
}
char *cpdbExtractFileName(const char *file_path)
{
    if (!file_path)
        return NULL;

    char *file_name_ptr = (char *)file_path;

    char *x = (char *)file_path;
    char c = *x;
    while (c)
    {
        if (c == '/')
            file_name_ptr = x;

        x++;
        c = *x;
    }
    return file_name_ptr;
}

char *cpdbGetUserConfDir()
{
    char *config_dir = NULL, *env_xch, *env_home;

    if (env_xch = getenv("XDG_CONFIG_HOME"))
        config_dir = cpdbConcatPath(env_xch, "cpdb");
    else if (env_home = getenv("HOME"))
        config_dir = cpdbConcatPath(env_home, ".config/cpdb");

    if (config_dir && (access(config_dir, R_OK) == 0 || mkdir(config_dir, CPDB_USRCONFDIR_PERM) == 0))
        return config_dir;

    return NULL;
}

char *cpdbGetSysConfDir()
{
    char *config_dir = NULL, *env_xcd, *path;

#ifdef CPDB_SYSCONFDIR
    config_dir = cpdbConcatPath(CPDB_SYSCONFDIR, "cpdb");
    if (access(config_dir, R_OK) == 0 || mkdir(config_dir, CPDB_SYSCONFDIR_PERM) == 0)
        return config_dir;
#endif

    if (env_xcd = getenv("XDG_CONFIG_DIRS"))
    {
        env_xcd = cpdbGetStringCopy(env_xcd);
        path = strtok(env_xcd, ":");
        while (path != NULL)
        {
            config_dir = cpdbConcatPath(CPDB_SYSCONFDIR, "cpdb");
            if (access(config_dir, R_OK) == 0 || mkdir(config_dir, CPDB_SYSCONFDIR_PERM) == 0)
            {
                free(env_xcd);
                return config_dir;
            }

            free(config_dir);
            path = strtok(NULL, ":");
        }
        free(env_xcd);
    }
        
    config_dir = "/etc/cpdb";
    if (access(config_dir, R_OK) == 0 || mkdir(config_dir, CPDB_SYSCONFDIR_PERM) == 0)
        return config_dir;

    return NULL;
}

char *cpdbGetGroup(const char *option_name)
{
    if (option_name == NULL)
    {
        cpdbDebugLog("Invalid params: cpdbGetCommonGroup()",
                     CPDB_DEBUG_LEVEL_WARN);
        return NULL;
    }

    int num_group = sizeof(cpdbGroupTable) / sizeof(cpdbGroupTable[0]);
    for (int i = 0; i < num_group; i++)
    {
        if (strncmp(option_name, cpdbGroupTable[i][0], strlen(cpdbGroupTable[i][0])) == 0)
            return cpdbGetStringCopy(cpdbGroupTable[i][1]);
    }

    return cpdbGetStringCopy(CPDB_GROUP_ADVANCED);
}

char *cpdbGetGroupTranslation2(const char *group_name, const char *lang)
{
    setenv("LANGUAGE", lang, 1);

    {
        extern int  _nl_msg_cat_cntr;
        ++_nl_msg_cat_cntr;
    }

    return cpdbGetStringCopy(_(group_name));
}

void cpdbDebugLog(const char *msg,
                  CpdbDebugLevel msg_lvl)
{
    FILE *log_file = NULL;
    char *env_cdl, *env_cdlf;
    CpdbDebugLevel dbg_lvl;

    if (msg == NULL)
        return;

    dbg_lvl = CPDB_DEBUG_LEVEL_ERR;
    if (env_cdl = getenv("CPDB_DEBUG_LEVEL"))
    {
        if (strncasecmp(env_cdl, "info", 4) == 0)
            dbg_lvl = CPDB_DEBUG_LEVEL_INFO;
        else if (strncasecmp(env_cdl, "warn", 4) == 0)
            dbg_lvl = CPDB_DEBUG_LEVEL_WARN;
    }

    if (env_cdlf = getenv("CPDB_DEBUG_LOGFILE"))
        log_file = fopen(env_cdlf, "a");

    if (msg_lvl >= dbg_lvl)
    {
        if (log_file)
            fprintf(log_file, "%s\n", msg);
        else
            fprintf(stderr, "%s\n", msg);
    }
}

void cpdbDebugLog2(const char *msg1,
                   const char *msg2,
                   CpdbDebugLevel msg_lvl)
{
    if (msg2 == NULL)
    {
        cpdbDebugLog(msg1, msg_lvl);
        return;
    }

    char *msg = malloc(strlen(msg1) + strlen(msg2) + 3);
    sprintf(msg, "%s: %s", msg1, msg2);
    cpdbDebugLog(msg, msg_lvl);
    free(msg);
}

void cpdbDebugPrintf(CpdbDebugLevel msg_lvl, const char *fmt, ...)
{
	va_list argptr;
	char msg[CPDB_BSIZE];
	
	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);
	
	cpdbDebugLog(msg, msg_lvl);
}
