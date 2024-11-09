#ifndef KEY_HANDLER
#define KEY_HANDLER

#include <string.h>
#include <stdbool.h>
#include "common/utils.h"
#include <malloc.h>
#include <android/keycodes.h>


#define KEYMOD_ALT  0x80000000
#define KEYMOD_CTRL  0x40000000
#define KEYMOD_SHIFT  0x20000000
#define KEYMOD_NUM_LOCK  0x10000000

const static uint_least32_t map[49][2] = {{ 2306560, KEYMOD_SHIFT | AKEYCODE_MOVE_HOME },//#2
                                          { 2307072, KEYMOD_SHIFT | AKEYCODE_DPAD_LEFT },//#4
                                          { 2451712, KEYMOD_SHIFT | AKEYCODE_DPAD_RIGHT },//%i
                                          { 2766592, KEYMOD_SHIFT | AKEYCODE_MOVE_END },//*7
                                          { 4208384, AKEYCODE_MOVE_END },//@7
                                          { 4208640, AKEYCODE_NUMPAD_ENTER },//@8
                                          { 4600064, AKEYCODE_F11 },//F1
                                          { 4600320, AKEYCODE_F12 },//F2
                                          { 4600576, KEYMOD_SHIFT | AKEYCODE_F1 },//F3
                                          { 4600832, KEYMOD_SHIFT | AKEYCODE_F2 },//F4
                                          { 4601088, KEYMOD_SHIFT | AKEYCODE_F3 },//F5
                                          { 4601344, KEYMOD_SHIFT | AKEYCODE_F4 },//F6
                                          { 4601600, KEYMOD_SHIFT | AKEYCODE_F5 },//F7
                                          { 4601856, KEYMOD_SHIFT | AKEYCODE_F6 },//F8
                                          { 4602112, KEYMOD_SHIFT | AKEYCODE_F7 },//F9
                                          { 4604160, KEYMOD_SHIFT | AKEYCODE_F8 },//FA
                                          { 4604416, KEYMOD_SHIFT | AKEYCODE_F9 },//FB
                                          { 4604672, KEYMOD_SHIFT | AKEYCODE_F10 },//FC
                                          { 4604928, KEYMOD_SHIFT | AKEYCODE_F11 },//FD
                                          { 4605184, KEYMOD_SHIFT | AKEYCODE_F12 },//FE
                                          { 4927744, AKEYCODE_MOVE_HOME },//K1
                                          { 4928256, AKEYCODE_PAGE_UP },//K3
                                          { 4928512, AKEYCODE_MOVE_END },//K4
                                          { 4928768, AKEYCODE_PAGE_DOWN },//K5
                                          { 7024896, AKEYCODE_F1 },//k1
                                          { 7025152, AKEYCODE_F2 },//k2
                                          { 7025408, AKEYCODE_F3 },//k3
                                          { 7025664, AKEYCODE_F4 },//k4
                                          { 7025920, AKEYCODE_F5 },//k5
                                          { 7026176, AKEYCODE_F6 },//k6
                                          { 7026432, AKEYCODE_F7 },//k7
                                          { 7026688, AKEYCODE_F8 },//k8
                                          { 7026944, AKEYCODE_F9 },//k9
                                          { 7027456, AKEYCODE_F10 },//k;
                                          { 7029248, KEYMOD_SHIFT | AKEYCODE_TAB },//kB
                                          { 7029760, AKEYCODE_FORWARD_DEL },//kD
                                          { 7029838, KEYMOD_SHIFT | AKEYCODE_DPAD_DOWN },//kDN
                                          { 7030272, KEYMOD_SHIFT | AKEYCODE_DPAD_DOWN },//kF
                                          { 7031040, AKEYCODE_INSERT },//kI
                                          { 7032320, AKEYCODE_PAGE_UP },//kN
                                          { 7032832, AKEYCODE_PAGE_DOWN },//kP
                                          { 7033344, KEYMOD_SHIFT | AKEYCODE_DPAD_UP },//kR
                                          { 7034192, KEYMOD_SHIFT | AKEYCODE_DPAD_UP },//kUP
                                          { 7037440, AKEYCODE_DEL },//kb
                                          { 7037952, AKEYCODE_DPAD_DOWN },//kd
                                          { 7038976, AKEYCODE_MOVE_HOME },//kh
                                          { 7040000, AKEYCODE_DPAD_LEFT },//kl
                                          { 7041536, AKEYCODE_DPAD_RIGHT },//kr
                                          { 7042304, AKEYCODE_DPAD_UP }//ku
};

size_t get_code(char **write_onto, unsigned int key_code, unsigned int key_mod, bool cursor_app, bool keypad_app);

size_t get_code_from_term_cap(char **write_onto, const char *string, size_t len, bool cursorApp, bool keypadApp);

static size_t
transform_for_modifier(char **write_onto, const char *const start, const size_t start_len, const unsigned int key_mod, const char last_char) {
    char modifier;
    switch(key_mod) {
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

static size_t foo0(char **write_onto, uint keymod, bool cursor_app, char a) {
    if(keymod)
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

size_t get_code(char **write_onto, const unsigned int key_code, unsigned int key_mod, const bool cursor_app, const bool keypad_app) {
    bool num_lock_on = 0 != (key_mod & KEYMOD_NUM_LOCK);
    key_mod &= ~KEYMOD_NUM_LOCK;
    switch(key_code) {
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
            const int len = 1 + (0 != (key_mod & KEYMOD_ALT));
            int index = len;
            *write_onto = malloc(index--);
            if(index)
                (*write_onto)[0] = '\033';
            (*write_onto)[index] = (0 == (key_mod & KEYMOD_CTRL)) ? 0x7F : 0x08;
            return len;
        }
        case AKEYCODE_NUM_LOCK:
            if(keypad_app) {
                *write_onto = "\033OP";
                return 3;
            } else
                return 0;
        case AKEYCODE_SPACE:
            if(0 == (key_mod & KEYMOD_CTRL))
                return 0;
            else
                *write_onto = "\0";
            return 1;
        case AKEYCODE_TAB:
            if(0 == (key_mod & KEYMOD_SHIFT)) {
                *write_onto = "\011";
                return 1;
            } else {
                *write_onto = "\033[Z";
                return 2;
            }
        case AKEYCODE_ENTER:
            if(0 == (key_mod & KEYMOD_ALT)) {
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
            if(num_lock_on) {
                if(keypad_app) {
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
            if(num_lock_on)
                place_holder_3('p', "0")
            else
                return transform_for_modifier(write_onto, "\033[2", 3, key_mod, '~');
        case AKEYCODE_NUMPAD_1:
            if(num_lock_on)
                place_holder_3('q', "1")
            else
                place_holder('F')
        case AKEYCODE_NUMPAD_2:
            if(num_lock_on)
                place_holder_3('r', "2")
            else
                place_holder('B')
        case AKEYCODE_NUMPAD_3:
            if(num_lock_on)
                place_holder_3('s', "3")
            else {
                *write_onto = "\033[6~";
                return 4;
            }
        case AKEYCODE_NUMPAD_4:
            if(num_lock_on)
                place_holder_3('t', "4")
            else
                place_holder('D')
        case AKEYCODE_NUMPAD_5:
            place_holder_3('u', "5")
        case AKEYCODE_NUMPAD_6:
            if(num_lock_on)
                place_holder_3('v', "6")
            else
                place_holder('C')
        case AKEYCODE_NUMPAD_7:
            if(num_lock_on)
                place_holder_3('w', "7")
            else
                place_holder('H')
        case AKEYCODE_NUMPAD_8:
            if(num_lock_on)
                place_holder_3('x', "8")
            else
                place_holder('A')
        case AKEYCODE_NUMPAD_9:
            if(num_lock_on)
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

size_t get_code_from_term_cap(char **write_onto, const char *const string, const size_t len, const bool cursorApp, const bool keypadApp) {
    if(len > 3 || len < 2) return 0;
    {
        unsigned int key_mode = 0;
        const uint_least32_t hash = string[0] << 16 | string[1] << 8 | (string[2] * (len != 2));
        unsigned int key_code = map_search(hash, 0, map, 49);
        if(0 == key_code)return 0;

        if(0 != (key_code & KEYMOD_SHIFT)) {
            key_mode |= KEYMOD_SHIFT;
            key_code &= ~KEYMOD_SHIFT;
        }
        if(0 != (key_code & KEYMOD_CTRL)) {
            key_mode |= KEYMOD_CTRL;
            key_code &= ~KEYMOD_CTRL;
        }
        if(0 != (key_code & KEYMOD_ALT)) {
            key_mode |= KEYMOD_ALT;
            key_code &= ~KEYMOD_ALT;
        }
        if(0 != (key_code & KEYMOD_NUM_LOCK)) {
            key_mode |= KEYMOD_NUM_LOCK;
            key_code &= ~KEYMOD_NUM_LOCK;
        }
        return get_code(write_onto, key_code, key_mode, cursorApp, keypadApp);
    }
}

#endif
