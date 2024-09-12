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

#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) < (b) ? (b) : (a))
#define LEN(a)            (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)    ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)        (((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)        (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)        (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define MODBIT(x, set, bit)    ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r, g, b)    (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)        (1 << 24 & (x))
#define UTF_SIZ 4
#define MOUSE_LEFT_BUTTON  0
#define MOUSE_WHEELUP_BUTTON  64
#define MOUSE_WHEELDOWN_BUTTON  65
/**
 * Used for invalid data - [...](<a href="http://en.wikipedia.org/wiki/Replacement_character#Replacement_character">...</a>)
 */
#define UNICODE_REPLACEMENT_CHAR  0xFFFD
/* The supported terminal cursor styles. */
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
 * Escape processing: Have seen an ESC character - proceed to [.doEsc]
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
 * DECSET 1 - application cursor keys.
 */
#define DECSET_BIT_APPLICATION_CURSOR_KEYS  1
#define DECSET_BIT_REVERSE_VIDEO  1 << 1
/**
 * [...](<a href="http://www.vt100.net/docs/vt510-rm/DECOM">...</a>): "When DECOM is set, the home cursor position is at the upper-left
 * corner of the console, within the margins. The starting point for line numbers depends on the current top margin
 * setting. The cursor cannot move outside of the margins. When DECOM is reset, the home cursor position is at the
 * upper-left corner of the console. The starting point for line numbers is independent of the margins. The cursor
 * can move outside of the margins."
 */
#define DECSET_BIT_ORIGIN_MODE  1 << 2
/**
 * [...](<a href="http://www.vt100.net/docs/vt510-rm/DECAWM">...</a>): "If the DECAWM function is set, then graphic characters received when
 * the cursor is at the right border of the page appear at the beginning of the next line. Any text on the page
 * scrolls up if the cursor is at the end of the scrolling region. If the DECAWM function is reset, then graphic
 * characters received when the cursor is at the right border of the page replace characters already on the page."
 */
#define DECSET_BIT_AUTOWRAP  1 << 3
/**
 * DECSET 25 - if the cursor should be enabled, [.isCursorEnabled].
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

static inline void size(int fd, unsigned short row, unsigned short col) {
    const struct winsize sz = {.ws_row=row, .ws_col=col};
    ioctl(fd, TIOCSWINSZ, &sz);
}

static int
get_process_fd(char const *cmd, int *pProcessId, unsigned short rows, unsigned short columns) {
    int ptm = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

    char devname[64];
    grantpt(ptm);
    unlockpt(ptm);
    ptsname_r(ptm, devname, sizeof(devname));

    // Enable UTF-8 mode and disable flow control to prevent Ctrl+S from locking up the display.
    struct termios tios;
    tcgetattr(ptm, &tios);
    tios.c_iflag |= IUTF8;
    tios.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(ptm, TCSANOW, &tios);

    /** Set initial winsize. */
    size(ptm, rows, columns);

    pid_t pid = fork();
    if (pid > 0) {
        *pProcessId = (int) pid;
        return ptm;
    } else {
        // Clear signals which the Android java process may have blocked:
        sigset_t signals_to_unblock;
        sigfillset(&signals_to_unblock);
        sigprocmask(SIG_UNBLOCK, &signals_to_unblock, 0);

        close(ptm);
        setsid();

        int pts = open(devname, O_RDWR);
        if (pts < 0) exit(-1);

        dup2(pts, 0);
        dup2(pts, 1);
        dup2(pts, 2);

        DIR *self_dir = opendir("/proc/self/fd");
        if (self_dir != NULL) {
            int self_dir_fd = dirfd(self_dir);
            struct dirent *entry;
            while ((entry = readdir(self_dir)) != NULL) {
                int fd = atoi(entry->d_name);
                if (fd > 2 && fd != self_dir_fd) close(fd);
            }
            closedir(self_dir);
        }

        putenv("HOME=/data/data/com.termux/files/home");
        putenv("PWD=/data/data/com.termux/files/home");
        putenv("LANG=en_US.UTF-8");
        putenv("PREFIX=/data/data/com.termux/files/usr");
        putenv("PATH=/data/data/com.termux/files/usr/bin");
        putenv("TMPDIR=/data/data/com.termux/files/usr/tmp");
        putenv("COLORTERM=truecolor");
        putenv("TERM=xterm-256color");

        chdir("/data/data/com.termux/files/home/");
        execvp(cmd, NULL);
        _exit(1);
    }
}

#define CHARACTER_ATTRIBUTE_BOLD               (1 << 0)
#define CHARACTER_ATTRIBUTE_ITALIC             (1 << 1)
#define CHARACTER_ATTRIBUTE_UNDERLINE          (1 << 2)
#define CHARACTER_ATTRIBUTE_BLINK              (1 << 3)
#define CHARACTER_ATTRIBUTE_INVERSE            (1 << 4)
#define CHARACTER_ATTRIBUTE_INVISIBLE          (1 << 5)
#define CHARACTER_ATTRIBUTE_STRIKETHROUGH      (1 << 6)
#define CHARACTER_ATTRIBUTE_PROTECTED          (1 << 7)
#define CHARACTER_ATTRIBUTE_DIM                (1 << 8)
#define CHARACTER_ATTRIBUTE_WARP               (1 << 9)
#define CHARACTER_ATTRIBUTE_WIDE               (1 << 10)

#define COLOR_INDEX_FOREGROUND  256
#define COLOR_INDEX_BACKGROUND 257
#define COLOR_INDEX_CURSOR  258
#define NUM_INDEXED_COLORS  259


typedef struct {
    uint_fast32_t c/*Character*/, fg, bg;
    unsigned short mode;
} Glyph;

typedef Glyph *Row;

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

#define MODE_UTF8 1<<0
#define MODE_INSERT 1<<1
#define MODE_ALT 1<<2
#define MODE_LINE_DRAWING 1<<3
typedef struct {
    Row *lines;
    int top;
} TermBuffer;
typedef struct {
    int fd, pid;
    unsigned short row, col;
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

static const unsigned char utfMask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const unsigned char utfByte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const uint_fast32_t utfMin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static const uint_fast32_t utfMax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static inline uint_fast32_t utf8Byte(const unsigned char c, size_t *const i) {
    for (*i = 0; *i < sizeof(utfMask); ++*i) {
        if ((c & utfMask[*i]) == utfByte[*i]) return c & ~utfMask[*i];
    }
    return 0;
}

static inline size_t utf8validate(uint_fast32_t *const u, size_t i) {
    if (!BETWEEN(*u, utfMin[i], utfMax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
        *u = UNICODE_REPLACEMENT_CHAR;
    for (i = 1; *u > utfMax[i]; ++i);

    return i;
}

size_t utf_decode(const char *b, uint_fast32_t *const c, const size_t _len) {
    size_t i, j, len, type;
    uint_fast32_t unDecoded;

    *c = UNICODE_REPLACEMENT_CHAR;
    if (!_len)
        return 0;
    unDecoded = utf8Byte(b[0], &len);
    if (!BETWEEN(len, 1, UTF_SIZ))
        return 1;
    for (i = 1, j = 1; i < _len && j < len; ++i, ++j) {
        unDecoded = (unDecoded << 6) | utf8Byte(b[i], &type);
        if (type != 0)
            return j;
    }
    if (j < len)
        return 0;
    *c = unDecoded;
    utf8validate(c, len);
    return len;
}

#define isDecsetInternalBitSet(term, bit) (term->cursor.flags & bit)

static void
blockCopy(TermBuffer *buffer, const int sx, const int sy, int w, const int h, const int dx,
          const int dy) {
    if (w == 0) return;
    const bool copyingUp = sy > dy;
    for (int y = 0; y < h; y++) {
        const int y2 = buffer->top + (copyingUp ? y : h - y - 1);
        Glyph *src = buffer->lines[dy + y2] + dx, *dst =
                buffer->lines[sy + y2] + sx; //TODO CHECK IF NULL
        while (--w)
            *src++ = *dst++;
    }
}

static void blockSet(TermBuffer *buffer, const int sx, const int sy, const int w, const int h,
                     const Glyph glyph) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            buffer->lines[buffer->top + sy + y][sx + x] = glyph;//TODO CHECK NULL
}

void scrollDownOneLine(TermBuffer *buffer) {
    buffer->top++;
}

void scrollDown(Term *term) {
    if (term->leftMargin != 0 || term->rightMargin != term->col) {
        const int w = term->rightMargin - term->leftMargin;
        term->cursor.attr.c = ' ';        //TODO INITIALIZE THIS AT BEGINNING
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
                term->screen->lines[term->cursor.y][term->cursor.x].mode |= CHARACTER_ATTRIBUTE_WARP;
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

