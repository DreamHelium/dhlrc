#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void DhInputFunc(const char*);

void dh_input_dialog_new(const char* title, const char* content, const char* tip, const char* default_input, GtkWindow* parent, DhInputFunc func);
char* dh_input_dialog_get_text();

G_END_DECLS

#endif /* INPUT_DIALOG_H */