//
// Created by Rohit Paul on 15-11-2024.
//

#include <string.h>
#include <stdbool.h>
#include "common/common.h"
#include <malloc.h>
#include <android/keycodes.h>
#include "core/key_handler.h"


#define KEYMOD_ALT  (int)0x80000000
#define KEYMOD_CTRL  0x40000000
#define KEYMOD_SHIFT  0x20000000
#define KEYMOD_NUM_LOCK  0x10000000

#define HASH(string) string[0] << 16 | string[1] << 8 | string[2]

static size_t
transform_for_modifier(char **write_onto, const char *const start, const size_t start_len, const int key_mod,
                       const char last_char) {
    char modifier;
    switch (key_mod) {
        default:
            *write_onto = malloc(start_len + 1);
            memcpy(*write_onto, start, start_len);
            (*write_onto)[start_len] = last_char;
            return start_len + 1;
        case KEYMOD_SHIFT:
            modifier = '2';
            break;
        case KEYMOD_ALT:
            modifier = '3';
            break;
        case KEYMOD_SHIFT | KEYMOD_ALT:
            modifier = '4';
            break;
        case KEYMOD_CTRL:
            modifier = '5';
            break;
        case KEYMOD_SHIFT | KEYMOD_CTRL:
            modifier = '6';
            break;
        case KEYMOD_ALT | KEYMOD_CTRL:
            modifier = '7';
            break;
        case KEYMOD_SHIFT | KEYMOD_ALT | KEYMOD_CTRL:
            modifier = '8';
            break;
    }
    *write_onto = malloc(start_len + 3);
    memcpy(*write_onto, start, start_len);
    *write_onto[start_len] = ';';
    *write_onto[start_len + 1] = modifier;
    *write_onto[start_len + 2] = last_char;
    return start_len + 3;
}

static size_t foo0(char **write_onto, int keymod, bool cursor_app, char a) {
    if (keymod)
        return transform_for_modifier(write_onto, "\033[1", 3, keymod, a);
    else {
        *write_onto = malloc(3 * sizeof(char));
        (*write_onto)[0] = '\033';
        (*write_onto)[1] = cursor_app ? 'O' : '[';
        (*write_onto)[2] = a;
        return 3;
    }
}

#define place_holder(a) return foo0(write_onto,key_mod,cursor_app,a);
#define place_holder_1(a) return foo0(write_onto,key_mod,true,a);
#define place_holder_2(a) return transform_for_modifier(write_onto,(a),4,key_mod,'~');
#define place_holder_2a(a) return transform_for_modifier(write_onto,(a),3,key_mod,'~');
#define place_holder_3(a, b) if (keypad_app) return transform_for_modifier(write_onto,"\033O",2,key_mod,(a)); else {*write_onto=(b);return 1;}

size_t get_code(char **write_onto, const int key_code, int key_mod, const bool cursor_app,
                const bool keypad_app) {
    bool num_lock_on = 0 != (key_mod & KEYMOD_NUM_LOCK);
    key_mod &= ~KEYMOD_NUM_LOCK;
    switch (key_code) {
        case AKEYCODE_DPAD_CENTER:
            *write_onto = "\015";
            return 1;
        case AKEYCODE_DPAD_UP:
            place_holder('A')
        case AKEYCODE_DPAD_DOWN:
            place_holder('B')
        case AKEYCODE_DPAD_RIGHT:
            place_holder('C')
        case AKEYCODE_DPAD_LEFT:
            place_holder('D')
        case AKEYCODE_MOVE_HOME:
            place_holder('H')
        case AKEYCODE_MOVE_END:
            place_holder('F')
        case AKEYCODE_F1:
            place_holder_1('P')
        case AKEYCODE_F2:
            place_holder_1('Q')
        case AKEYCODE_F3:
            place_holder_1('R')
        case AKEYCODE_F4:
            place_holder_1('S')
        case AKEYCODE_F5:
            place_holder_2("\033[15")
        case AKEYCODE_F6:
            place_holder_2("\033[17")
        case AKEYCODE_F7:
            place_holder_2("\033[18")
        case AKEYCODE_F8:
            place_holder_2("\033[19")
        case AKEYCODE_F9:
            place_holder_2("\033[20")
        case AKEYCODE_F10:
            place_holder_2("\033[21")
        case AKEYCODE_F11:
            place_holder_2("\033[23")
        case AKEYCODE_F12:
            place_holder_2("\033[24")
        case AKEYCODE_SYSRQ:
            *write_onto = "\033[32~";
            return 5;
        case AKEYCODE_BREAK:
            *write_onto = "\033[34~";
            return 5;
        case AKEYCODE_ESCAPE:
        case AKEYCODE_BACK:
            *write_onto = "\033";
            return 1;
        case AKEYCODE_INSERT:
            place_holder_2a("\033[2")
        case AKEYCODE_FORWARD_DEL:
            place_holder_2a("\033[3")
        case AKEYCODE_PAGE_UP:
            place_holder_2a("\033[5")
        case AKEYCODE_PAGE_DOWN:
            place_holder_2a("\033[6")
        case AKEYCODE_DEL: {
            const size_t len = 1 + (0 != (key_mod & KEYMOD_ALT));
            size_t index = len;
            *write_onto = malloc(index);
            if (--index)
                (*write_onto)[0] = '\033';
            (*write_onto)[index] = (0 == (key_mod & KEYMOD_CTRL)) ? 0x7F : 0x08;
            return len;
        }
        case AKEYCODE_NUM_LOCK:
            if (keypad_app) {
                *write_onto = "\033OP";
                return 3;
            } else
                return 0;
        case AKEYCODE_SPACE:
            if (0 == (key_mod & KEYMOD_CTRL))
                return 0;
            else
                *write_onto = "\0";
            return 1;
        case AKEYCODE_TAB:
            if (0 == (key_mod & KEYMOD_SHIFT)) {
                *write_onto = "\011";
                return 1;
            } else {
                *write_onto = "\033[Z";
                return 2;
            }
        case AKEYCODE_ENTER:
            if (0 == (key_mod & KEYMOD_ALT)) {
                *write_onto = "\r";
                return 1;
            } else {
                *write_onto = "\033\r";
                return 2;
            }
        case AKEYCODE_NUMPAD_ENTER:
            place_holder_3('M', "\n")
        case AKEYCODE_NUMPAD_MULTIPLY:
            place_holder_3('j', "*")
        case AKEYCODE_NUMPAD_ADD:
            place_holder_3('k', "+")
        case AKEYCODE_NUMPAD_COMMA:
            *write_onto = ",";
            return 1;
        case AKEYCODE_NUMPAD_DOT:
            if (num_lock_on) {
                if (keypad_app) {
                    *write_onto = "\033On";
                    return 3;
                } else {
                    *write_onto = ".";
                    return 1;
                }
            } else return transform_for_modifier(write_onto, "\033[3", 3, key_mod, '~');
        case AKEYCODE_NUMPAD_SUBTRACT:
            place_holder_3('m', "-")
        case AKEYCODE_NUMPAD_DIVIDE:
            place_holder_3('o', "/")
        case AKEYCODE_NUMPAD_0:
            if (num_lock_on)
                place_holder_3('p', "0")
            else
                return transform_for_modifier(write_onto, "\033[2", 3, key_mod, '~');
        case AKEYCODE_NUMPAD_1:
            if (num_lock_on)
                place_holder_3('q', "1")
            else
                place_holder('F')
        case AKEYCODE_NUMPAD_2:
            if (num_lock_on)
                place_holder_3('r', "2")
            else
                place_holder('B')
        case AKEYCODE_NUMPAD_3:
            if (num_lock_on)
                place_holder_3('s', "3")
            else {
                *write_onto = "\033[6~";
                return 4;
            }
        case AKEYCODE_NUMPAD_4:
            if (num_lock_on)
                place_holder_3('t', "4")
            else
                place_holder('D')
        case AKEYCODE_NUMPAD_5:
            place_holder_3('u', "5")
        case AKEYCODE_NUMPAD_6:
            if (num_lock_on)
                place_holder_3('v', "6")
            else
                place_holder('C')
        case AKEYCODE_NUMPAD_7:
            if (num_lock_on)
                place_holder_3('w', "7")
            else
                place_holder('H')
        case AKEYCODE_NUMPAD_8:
            if (num_lock_on)
                place_holder_3('x', "8")
            else
                place_holder('A')
        case AKEYCODE_NUMPAD_9:
            if (num_lock_on)
                place_holder_3('y', "9")
            else {
                *write_onto = "\033[5~";
                return 4;
            }
        case AKEYCODE_NUMPAD_EQUALS:
            place_holder_3('X', "=")
        default:
            return 0;
    }
}

size_t get_code_from_term_cap(char **write_onto, const char *const string, const size_t len, const bool cursorApp,
                              const bool keypadApp) {
    if (len > 3 || len < 2) return 0;
    {
        int key_mode = 0;
        int key_code = 0;
        switch (HASH(string)) {
            case HASH("%i"):
                key_code = KEYMOD_SHIFT | AKEYCODE_DPAD_RIGHT;
                break;
            case HASH("#2"):
                key_code = KEYMOD_SHIFT | AKEYCODE_MOVE_HOME; // Shifted home
                break;
            case HASH("#4"):
                key_code = KEYMOD_SHIFT | AKEYCODE_DPAD_LEFT;
                break;
            case HASH("*7"):
                key_code = KEYMOD_SHIFT | AKEYCODE_MOVE_END; // Shifted end key
                break;
            case HASH("k1"):
                key_code = AKEYCODE_F1;
                break;
            case HASH("k2"):
                key_code = AKEYCODE_F2;
                break;
            case HASH("k3"):
                key_code = AKEYCODE_F3;
                break;
            case HASH("k4"):
                key_code = AKEYCODE_F4;
                break;
            case HASH("k5"):
                key_code = AKEYCODE_F5;
                break;
            case HASH("k6"):
                key_code = AKEYCODE_F6;
                break;
            case HASH("k7"):
                key_code = AKEYCODE_F7;
                break;
            case HASH("k8"):
                key_code = AKEYCODE_F8;
                break;
            case HASH("k9"):
                key_code = AKEYCODE_F9;
                break;
            case HASH("k;"):
                key_code = AKEYCODE_F10;
                break;
            case HASH("F1"):
                key_code = AKEYCODE_F11;
                break;
            case HASH("F2"):
                key_code = AKEYCODE_F12;
                break;
            case HASH("F3"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F1;
                break;
            case HASH("F4"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F2;
                break;
            case HASH("F5"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F3;
                break;
            case HASH("F6"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F4;
                break;
            case HASH("F7"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F5;
                break;
            case HASH("F8"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F6;
                break;
            case HASH("F9"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F7;
                break;
            case HASH("FA"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F8;
                break;
            case HASH("FB"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F9;
                break;
            case HASH("FC"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F10;
                break;
            case HASH("FD"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F11;
                break;
            case HASH("FE"):
                key_code = KEYMOD_SHIFT | AKEYCODE_F12;
                break;

            case HASH("kb"):
                key_code = AKEYCODE_DEL; // backspace key
                break;
            case HASH("kd"):
                key_code = AKEYCODE_DPAD_DOWN; // terminfo=kcud1, down-arrow key
                break;
            case HASH("kh"):
                key_code = AKEYCODE_MOVE_HOME;
                break;
            case HASH("kl"):
                key_code = AKEYCODE_DPAD_LEFT;
                break;
            case HASH("kr"):
                key_code = AKEYCODE_DPAD_RIGHT;
                break;
                // K1=Upper left of keypad:
                // t_K1 <kHome> keypad home key
                // t_K3 <kPageUp> keypad page-up key
                // t_K4 <kEnd> keypad end key
                // t_K5 <kPageDown> keypad page-down key
            case HASH("K1"):
                key_code = AKEYCODE_MOVE_HOME;
                break;
            case HASH("K3"):
                key_code = AKEYCODE_PAGE_UP;
                break;
            case HASH("K4"):
                key_code = AKEYCODE_MOVE_END;
                break;
            case HASH("K5"):
                key_code = AKEYCODE_PAGE_DOWN;
                break;
            case HASH("ku"):
                key_code = AKEYCODE_DPAD_UP;
                break;
            case HASH("kB"):
                key_code = KEYMOD_SHIFT | AKEYCODE_TAB; // termcap=kB, terminfo=kcbt: Back-tab
                break;
            case HASH("kD"):
                key_code = AKEYCODE_FORWARD_DEL; // terminfo=kdch1, delete-character key
                break;
            case HASH("kDN"):// non-standard shifted arrow down
            case HASH("kF"):
                key_code = KEYMOD_SHIFT | AKEYCODE_DPAD_DOWN; // terminfo=kind, scroll-forward key
                break;
            case HASH("kI"):
                key_code = AKEYCODE_INSERT;
                break;
            case HASH("kN"):
                key_code = AKEYCODE_PAGE_UP;
                break;
            case HASH("kP"):
                key_code = AKEYCODE_PAGE_DOWN;
                break;
            case HASH("kR"): // terminfo=kri, scroll-backward key
            case HASH("kUP"):
                key_code = KEYMOD_SHIFT | AKEYCODE_DPAD_UP; // non-standard shifted up
                break;
            case HASH("@7"):
                key_code = AKEYCODE_MOVE_END;
                break;
            case HASH("@8"):
                key_code = AKEYCODE_NUMPAD_ENTER;
        }
        if (0 == key_code)return 0;

        if (0 != (key_code & KEYMOD_SHIFT)) {
            key_mode |= KEYMOD_SHIFT;
            key_code &= ~KEYMOD_SHIFT;
        }
        if (0 != (key_code & KEYMOD_CTRL)) {
            key_mode |= KEYMOD_CTRL;
            key_code &= ~KEYMOD_CTRL;
        }
        if (0 != (key_code & KEYMOD_ALT)) {
            key_mode |= KEYMOD_ALT;
            key_code &= ~KEYMOD_ALT;
        }
        if (0 != (key_code & KEYMOD_NUM_LOCK)) {
            key_mode |= KEYMOD_NUM_LOCK;
            key_code &= ~KEYMOD_NUM_LOCK;
        }
        return get_code(write_onto, key_code, key_mode, cursorApp, keypadApp);
    }
}
