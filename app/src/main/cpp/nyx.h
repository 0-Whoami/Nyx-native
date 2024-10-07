#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <wchar.h>
#include "emulator/utf.h"

#define TRUECOLOR(r, g, b)    (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)        (1 << 24 & (x))

#define MOUSE_LEFT_BUTTON  0
#define MOUSE_WHEELUP_BUTTON  64
#define MOUSE_WHEELDOWN_BUTTON  65

/* The supported terminal term_cursor styles. */
#define TERMINAL_CURSOR_STYLE_BLOCK  0
#define TERMINAL_CURSOR_STYLE_UNDERLINE  1
#define TERMINAL_CURSOR_STYLE_BAR  2
/**
 * Mouse moving while having left mouse button pressed.
 */
#define MOUSE_LEFT_BUTTON_MOVED  32
/**
 * Escape processing: Not currently in an escape sequence.
 */
#define ESC_NONE  0
/**
 * Escape processing: Have seen an ESC character - proceed to [.do_esc]
 */
#define ESC 1
/**
 * Escape processing: Have seen ESC POUND
 */
#define ESC_POUND  2
/**
 * Escape processing: Have seen ESC and a character-set-select ( char
 */
#define ESC_SELECT_LEFT_PAREN  3
/**
 * Escape processing: Have seen ESC and a character-set-select ) char
 */
#define ESC_SELECT_RIGHT_PAREN  4
/**
 * Escape processing: "ESC [" or CSI (Control Sequence Introducer).
 */
#define ESC_CSI  6
/**
 * Escape processing: ESC [ ?
 */
#define ESC_CSI_QUESTIONMARK  7
/**
 * Escape processing: ESC [ $
 */
#define ESC_CSI_DOLLAR  8
/**
 * Escape processing: ESC ] (AKA OSC - Operating System Controls)
 */
#define ESC_OSC  10
/**
 * Escape processing: ESC ] (AKA OSC - Operating System Controls) ESC
 */
#define ESC_OSC_ESC  11
/**
 * Escape processing: ESC [ >
 */
#define ESC_CSI_BIGGERTHAN  12
/**
 * Escape procession: "ESC P" or Device Control String (DCS)
 */
#define ESC_P  13
/**
 * Escape processing: CSI >
 */
#define ESC_CSI_QUESTIONMARK_ARG_DOLLAR  14
/**
 * Escape processing: CSI $ARGS ' '
 */
#define ESC_CSI_ARGS_SPACE  15
/**
 * Escape processing: CSI $ARGS '*'
 */
#define ESC_CSI_ARGS_ASTERIX  16
/**
 * Escape processing: CSI "
 */
#define ESC_CSI_DOUBLE_QUOTE  17
/**
 * Escape processing: CSI '
 */
#define ESC_CSI_SINGLE_QUOTE  18
/**
 * Escape processing: CSI !
 */
#define ESC_CSI_EXCLAMATION =19
/**
 * The number of parameter arguments. This name comes from the ANSI standard for terminal escape codes.
 */
#define MAX_ESCAPE_PARAMETERS  16
/**
 * Needs to be large enough to contain reasonable OSC 52 pastes.
 */
#define MAX_OSC_STRING_LENGTH  8192
/**
 * DECSET 1 - application term_cursor keys.
 */
#define DECSET_BIT_APPLICATION_CURSOR_KEYS  1
#define DECSET_BIT_REVERSE_VIDEO  1 << 1
/**
 * [...](<a href="http://www.vt100.net/docs/vt510-rm/DECOM">...</a>): "When DECOM is set, the home term_cursor position is at the upper-left
 * corner of the console, within the margins. The starting point for line numbers depends on the current top margin
 * setting. The term_cursor cannot move outside of the margins. When DECOM is reset_color, the home term_cursor position is at the
 * upper-left corner of the console. The starting point for line numbers is independent of the margins. The term_cursor
 * can move outside of the margins."
 */
#define DECSET_BIT_ORIGIN_MODE  1 << 2
/**
 * [...](<a href="http://www.vt100.net/docs/vt510-rm/DECAWM">...</a>): "If the DECAWM function is set, then graphic characters received when
 * the term_cursor is at the right border of the page appear at the beginning of the next line. Any text on the page
 * scrolls up if the term_cursor is at the end of the scrolling region. If the DECAWM function is reset_color, then graphic
 * characters received when the term_cursor is at the right border of the page replace characters already on the page."
 */
#define DECSET_BIT_AUTOWRAP  1 << 3
/**
 * DECSET 25 - if the term_cursor should be enabled, [.isCursorEnabled].
 */
#define DECSET_BIT_CURSOR_ENABLED  1 << 4
#define DECSET_BIT_APPLICATION_KEYPAD  1 << 5
/**
 * DECSET 1000 - if to report mouse press&release events.
 */
#define DECSET_BIT_MOUSE_TRACKING_PRESS_RELEASE  1 << 6
/**
 * DECSET 1002 - like 1000, but report moving mouse while pressed.
 */
#define DECSET_BIT_MOUSE_TRACKING_BUTTON_EVENT  1 << 7
/**
 * DECSET 1004 - NOT implemented.
 */
#define DECSET_BIT_SEND_FOCUS_EVENTS  1 << 8
/**
 * DECSET 1006 - SGR-like mouse protocol (the modern sane choice).
 */
#define DECSET_BIT_MOUSE_PROTOCOL_SGR  1 << 9
/**
 * DECSET 2004 - see [.paste]
 */
#define DECSET_BIT_BRACKETED_PASTE_MODE  1 << 10
/**
 * Toggled with DECLRMM - [...](<a href="http://www.vt100.net/docs/vt510-rm/DECLRMM">...</a>)
 */
#define DECSET_BIT_LEFTRIGHT_MARGIN_MODE  1 << 11
/**
 * Not really DECSET bit... - [...](<a href="http://www.vt100.net/docs/vt510-rm/DECSACE">...</a>)
 */
#define DECSET_BIT_RECTANGULAR_CHANGEATTRIBUTE  1 << 12



typedef struct {
    int x, y, old_x, old_y, flags;
    Glyph attr;
} Cursor;

typedef struct {
    int arg[MAX_ESCAPE_PARAMETERS], index;
} CSIEsc;

typedef struct {
    char *buff;
    size_t len;
} OSC;

#define MODE_UTF8 0b0001
#define MODE_INSERT 0b0010
#define MODE_ALT 0b0100
#define MODE_LINE_DRAWING 0b1000

#define TRANSCRIPT 500
typedef struct {
    int fd, pid;
   uint_fast16_t row, col;
    int topMargin, bottomMargin, leftMargin, rightMargin;
    TermBuffer *screen, mainBuf, altBuf;
    Cursor cursor;
    bool *tabs, aboutToAutowrap;
    unsigned char mode;
    int esc;
    CSIEsc csiEsc;
    OSC oscEsc;
    uint_fast32_t lastEmittedCodepoint;
} Term;

#define isDecsetInternalBitSet(term, bit) (term->cursor.flags & bit)



void scrollDownOneLine(TermBuffer *buffer) {
    buffer->top++;
}

void scrollDown(Term *term) {
    if (term->leftMargin != 0 || term->rightMargin != term->col) {
        const int w = term->rightMargin - term->leftMargin;
        term->cursor.attr.cursor = ' ';        //TODO INITIALIZE THIS AT BEGINNING
        blockCopy(term->screen, term->leftMargin, term->topMargin + 1, w,
                  term->bottomMargin - term->topMargin - 1, term->leftMargin, term->topMargin);
        blockSet(term->screen, term->leftMargin, term->bottomMargin - 1, w, 1, term->cursor.attr);
    } else scrollDownOneLine(term->screen);
}


static void processCodePoint(Term *term, uint_fast32_t codePoint) {
    switch (codePoint) {

    }
}

static void emitCodePoint(Term *term, uint_fast32_t codePoint) {
    term->lastEmittedCodepoint = codePoint;
    if (term->mode & MODE_LINE_DRAWING) {
        // http://www.vt100.net/docs/vt102-ug/table5-15.html.
        switch (codePoint) {
            case '_':
                codePoint = L' '; // Blank.
                break;
            case '`':
                codePoint = L'◆'; // Diamond.
                break;
            case '0':
                codePoint = L'█'; // Solid block;
                break;
            case 'a':
                codePoint = L'▒'; // Checker board.
                break;
            case 'b':
                codePoint = L'␉'; // Horizontal tab.
                break;
            case 'c':
                codePoint = L'␌'; // Form feed.
                break;
            case 'd':
                codePoint = L'\r'; // Carriage return.
                break;
            case 'e':
                codePoint = L'␊'; // Linefeed.
                break;
            case 'f':
                codePoint = L'°'; // Degree.
                break;
            case 'g':
                codePoint = L'±'; // Plus-minus.
                break;
            case 'h':
                codePoint = L'\n'; // Newline.
                break;
            case 'i':
                codePoint = L'␋'; // Vertical tab.
                break;
            case 'j':
                codePoint = L'┘'; // Lower right corner.
                break;
            case 'k':
                codePoint = L'┐'; // Upper right corner.
                break;
            case 'l':
                codePoint = L'┌'; // Upper left corner.
                break;
            case 'm':
                codePoint = L'└'; // Left left corner.
                break;
            case 'n':
                codePoint = L'┼'; // Crossing lines.
                break;
            case 'o':
                codePoint = L'⎺'; // Horizontal line - scan 1.
                break;
            case 'p':
                codePoint = L'⎻'; // Horizontal line - scan 3.
                break;
            case 'q':
                codePoint = L'─'; // Horizontal line - scan 5.
                break;
            case 'r':
                codePoint = L'⎼'; // Horizontal line - scan 7.
                break;
            case 's':
                codePoint = L'⎽'; // Horizontal line - scan 9.
                break;
            case 't':
                codePoint = L'├'; // T facing rightwards.
                break;
            case 'u':
                codePoint = L'┤'; // T facing leftwards.
                break;
            case 'v':
                codePoint = L'┴'; // T facing upwards.
                break;
            case 'w':
                codePoint = L'┬'; // T facing downwards.
                break;
            case 'x':
                codePoint = L'│'; // Vertical line.
                break;
            case 'y':
                codePoint = L'≤'; // Less than or equal to.
                break;
            case 'z':
                codePoint = L'≥'; // Greater than or equal to.
                break;
            case '{':
                codePoint = L'π'; // Pi.
                break;
            case '|':
                codePoint = L'≠'; // Not equal to.
                break;
            case '}':
                codePoint = L'£'; // UK pound.
                break;
            case '~':
                codePoint = L'·'; // Centered dot.
                break;
        }
    }
    const bool autoWarp = isDecsetInternalBitSet(term, DECSET_BIT_AUTOWRAP), cursorInLastColumn =
            term->rightMargin - 1;
    int width = wcwidth(codePoint);
    if (width < 0) width = 0;
    if (cursorInLastColumn) {
        if (autoWarp) {
            if ((term->aboutToAutowrap && width == 1) || 2 == width) {
                term->screen->lines[term->cursor.y][term->cursor.x].effect |= CHARACTER_ATTRIBUTE_WARP;
                term->cursor.x = term->leftMargin;
                if (term->cursor.y + 1 < term->bottomMargin) term->cursor.y++;
                else
                    scrollDown(term);
            }
        } else if (width == 2) return;
    }
    if (term->mode & MODE_INSERT && 0 < width) {
        const int destCol = term->cursor.x + width;
        if (destCol < term->rightMargin)
            blockCopy(term->screen, term->cursor.x, term->cursor.y, term->rightMargin - destCol, 1,
                      destCol, term->cursor.y);
    }

}

static void *readFromFd(void *t) {
    Term *const term = (Term *) t;
    char buffer[512];
    static size_t readBytes, utfBuff = 0;
    while ((readBytes = read(term->fd, buffer + utfBuff, LEN(buffer) - utfBuff)) > 0) {
        utfBuff += readBytes;
        size_t wrote, charSize;
        uint_fast32_t c;
        for (wrote = 0; wrote < readBytes; wrote += charSize) {
            charSize = utf_decode(buffer + wrote, &c, readBytes - wrote);
            if (charSize == 0) break;
            processCodePoint(term, c);
        }
        utfBuff -= wrote;
        for (size_t i = utfBuff; i > 0; i++)
            buffer[i] = *(buffer + wrote + i);
    }
    return NULL;
}

static void *waitFor(void *const arg) {
    Term *term = (Term *) arg;
    waitpid(term->pid, NULL, 0);
    free(term);
    return NULL;
}

void initTerm(Term *const term, const char *const cmd, int col, int row) {
    term->fd = get_process_fd(cmd, &(term->pid), row, col);
    pthread_t readThread, waitThread;
    pthread_create(&readThread, NULL, readFromFd, term);
    pthread_create(&waitThread, NULL, waitFor, term);
    pthread_detach(readThread);
    pthread_detach(waitThread);
}

