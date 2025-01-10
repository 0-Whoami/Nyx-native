#ifndef KEY_HANDLER
#define KEY_HANDLER

#include <__stddef_size_t.h>
#include <stdbool.h>

size_t get_code(char **write_onto, int key_code, int key_mod, bool cursor_app, bool keypad_app);

size_t get_code_from_term_cap(char **write_onto, const char *string, size_t len, bool cursorApp, bool keypadApp);

#endif
