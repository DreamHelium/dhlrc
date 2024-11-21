#ifndef MAIN_CLI_H
#define MAIN_CLI_H

#include "dh_string_util.h"

#ifdef __cplusplus
extern "C"
{
#endif
    extern const char* module_name();
    extern DhStrArray* module_name_array();
    extern const char* module_description();
    extern const char* help_description();
    extern int start_point(int argc, char** argv, const char* prpath);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_H */