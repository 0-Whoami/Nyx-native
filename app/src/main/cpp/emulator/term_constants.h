#define UNICODE_REPLACEMENT_CHARACTER 0xFFFD
enum MOUSE_ACTIONS {
    MOUSE_LEFT_BUTTON = 0, MOUSE_WHEELUP_BUTTON = 64, MOUSE_WHEELDOWN_BUTTON = 65, MOUSE_LEFT_BUTTON_MOVED = 32
};
enum ESCAPE_STATES {
    ESC_NONE = 0,
    ESC = 1,
    ESC_POUND = 2,
    ESC_SELECT_LEFT_PAREN = 3,
    ESC_SELECT_RIGHT_PAREN = 4,
    ESC_CSI = 6,
    ESC_CSI_QUESTIONMARK = 7,
    ESC_CSI_DOLLAR = 8,
    ESC_PERCENT = 9,
    ESC_OSC = 10,
    ESC_OSC_ESC = 11,
    ESC_CSI_BIGGERTHAN = 12,
    ESC_P = 13,
    ESC_CSI_QUESTIONMARK_ARG_DOLLAR = 14,
    ESC_CSI_ARGS_SPACE = 15,
    ESC_CSI_ARGS_ASTERIX = 16,
    ESC_CSI_DOUBLE_QUOTE = 17,
    ESC_CSI_SINGLE_QUOTE = 18,
    ESC_CSI_EXCLAMATION = 19,
    ESC_APC = 20,
    ESC_APC_ESCAPE = 21
};

#define MAX_ESCAPE_PARAMETERS 16
#define MAX_OSC_STRING_LENGTH 8192
typedef struct {
    unsigned short APPLICATION_CURSOR_KEYS: 1;
    unsigned short REVERSE_VIDEO: 1;
    unsigned short ORIGIN_MODE: 1;
    unsigned short AUTOWRAP: 1;
    unsigned short CURSOR_ENABLED: 1;
    unsigned short APPLICATION_KEYPAD: 1;
    unsigned short MOUSE_TRACKING_PRESS_RELEASE: 1;
    unsigned short MOUSE_TRACKING_BUTTON_EVENT: 1;
    unsigned short SEND_FOCUS_EVENTS: 1;
    unsigned short MOUSE_PROTOCOL_SGR: 1;
    unsigned short BRACKETED_PASTE_MODE: 1;
    unsigned short LEFTRIGHT_MARGIN_MODE: 1;
    unsigned short RECTANGULAR_CHANGEATTRIBUTE: 1;
} DECSET_BIT;

typedef struct {
    unsigned char MODE_INSERT: 1, ABOUT_TO_AUTO_WRAP: 1, G0: 1, G1: 1, USE_G0: 1;
} TERM_MODE;
