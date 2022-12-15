#include "cpdb.h"

gboolean cpdbGetBoolean(const char *g)
{
    if (!g)
        return FALSE;

    if (g_str_equal(g, "true"))
        return TRUE;

    return FALSE;
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
    {
        config_dir = malloc(strlen(env_xch) + 6);
        sprintf(config_dir, "%s/cpdb", env_xch);
    }
    else if (env_home = getenv("HOME"))
    {
        config_dir = malloc(strlen(env_home) + 14);
        sprintf(config_dir, "%s/.config/cpdb", env_home);
    }

    if (config_dir && (access(config_dir, F_OK) == 0 || mkdir(config_dir, CPDB_USRCONFDIR_PERM) == 0))
        return config_dir;

    return NULL;
}

char *cpdbGetSysConfDir()
{
    char *config_dir = NULL, *env_xcd, *path;

#ifdef CPDB_SYSCONFDIR
    config_dir = malloc(strlen(CPDB_SYSCONFDIR) + 6);
    sprintf(config_dir, "%s/cpdb", CPDB_SYSCONFDIR);
    if (access(config_dir, F_OK) == 0 || mkdir(config_dir, CPDB_SYSCONFDIR_PERM) == 0)
        return config_dir;
#endif

    if (env_xcd = getenv("XDG_CONFIG_DIRS"))
    {
        env_xcd = cpdbGetStringCopy(env_xcd);
        path = strtok(env_xcd, ":");
        while (path != NULL)
        {
            config_dir = malloc(strlen(path) + 6);
            sprintf(config_dir, "%s/cpdb", path);
            if (access(config_dir, F_OK) == 0 || mkdir(config_dir, CPDB_SYSCONFDIR_PERM) == 0)
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
    if (access(config_dir, F_OK) == 0 || mkdir(config_dir, CPDB_SYSCONFDIR_PERM) == 0)
        return config_dir;

    return NULL;
}