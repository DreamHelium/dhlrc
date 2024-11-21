#include "module.h"
#include "main_gtk.h"
#include "../translation.h"

extern int
start_point (int    argc,
      char **argv, const char* prpath)
{
#ifdef G_OS_WIN32
        char* dir = g_strconcat(prpath, "..\\lib\\gtk", NULL);
        AddDllDirectory(dir);
        g_free(dir);
#endif
  GtkApplication *app;
  int status;

#ifdef GLIB_AVAILABLE_IN_2_74
  app = gtk_application_new ("cn.dh.dhlrc", G_APPLICATION_DEFAULT_FLAGS);
#else
  app = gtk_application_new ("cn.dh.dhlrc", G_APPLICATION_FLAGS_NONE);
#endif
  g_signal_connect (app, "activate", G_CALLBACK (gtk_app_activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);

  return status;
}

extern const char* module_name()
{
    return "gtk";
}

extern DhStrArray* module_name_array()
{
    return NULL;
}

extern const char* module_description()
{
    return _("The GTK interface of dhlrc");
}

extern const char* help_description()
{
    return NULL;
}