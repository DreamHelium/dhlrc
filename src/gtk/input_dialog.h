#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void DhInputFunc(const char*);

void dh_input_dialog_new(const char* title, const char* content, const char* tip, const char* default_input, GtkWindow* parent, DhInputFunc func);
/* If using the .new method, should free by yourself */
char* dh_input_dialog_get_text();
char* dh_input_dialog_new_no_func(const char* title, const char* content, const char* tip, const char* default_input, GtkWindow* parent);

G_END_DECLS

#endif /* INPUT_DIALOG_H */