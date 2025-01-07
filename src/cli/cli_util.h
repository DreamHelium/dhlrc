#ifndef CLI_UTIL_H
#define CLI_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

void dhlrc_clear_screen();
void dhlrc_add_args_common(void* arg, char c, const char* first, const char* second, const char* description);

#ifdef __cplusplus
}
#endif


#endif /* CLI_UTIL_H */