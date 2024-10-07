#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <bits/fortify/string.h>
#include "utils/utils.h"
#include "utf.h"

static int get_fd(char const *cmd, int *pProcessId,uint16_t rows,uint16_t columns) {
    int ptm = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

    char devname[64];
    grantpt(ptm);
    unlockpt(ptm);
    ptsname_r(ptm, devname, sizeof(devname));

    // Enable UTF-8 effect and disable flow control to prevent Ctrl+S from locking up the display.
    struct termios tios;
    tcgetattr(ptm, &tios);
    tios.c_iflag |= IUTF8;
    tios.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(ptm, TCSANOW, &tios);

    /** Set initial winsize. */
    const struct winsize sz = {.ws_row=rows, .ws_col=columns};
    ioctl(ptm, TIOCSWINSZ, &sz);

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
