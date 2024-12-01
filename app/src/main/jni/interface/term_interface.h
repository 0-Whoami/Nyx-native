//
// Created by Rohit Paul on 16-10-2024.
//

#ifndef NYX_TERM_INTERFACE_H
#define NYX_TERM_INTERFACE_H

#include <sys/epoll.h>
#include <pthread.h>
#include <sys/wait.h>

#define MAX_TERM_SESSIONS 5


static struct epoll_event event_que[MAX_TERM_SESSIONS];

static int epoll_fd = -1;
static pthread_t pthread;
//Term_emulator *sessions[MAX_TERM_SESSIONS];

//int occupied_len;

extern void init_terminal_session_manager(void);

extern void add_term_session(const char *);
/*

static void cleanup(Term_emulator *const emulator) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, FD, NULL);
    destroy_term_emulator(emulator);
    occupied_len--;
}

static void handle_sigchld(__attribute__((unused)) int sig) {
    Term_emulator *emulator;
    for (int i = 0; i < occupied_len; i++) {
        emulator = sessions[i];
        if (waitpid(emulator->pid, NULL, WNOHANG) > 0) cleanup(emulator);
    }
}

static void *loop(void *__attribute__((unused)) arg) {
    static int events_ready = 0;
    while (events_ready != -1) {
        events_ready = epoll_wait(epoll_fd, event_que, occupied_len + 1, -1);
        while (--events_ready >= 0)
            read_data(event_que[events_ready].data.ptr);
    }
    return NULL;
}

void init_terminal_session_manager(void) {
    struct sigaction sa;


    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
    epoll_fd = epoll_create1(0);

    pthread_create(&pthread, NULL, loop, NULL);
    pthread_detach(pthread);
}

void add_term_session(const char *const cmd) {
    if (occupied_len < MAX_TERM_SESSIONS) {
        struct epoll_event event;

        event.data.ptr = sessions[occupied_len] = new_term_emulator(cmd);
        event.events = EPOLLIN;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sessions[occupied_len]->fd, &event);
        occupied_len++;
    }
}

#define kill_term_session(index) kill(sessions[index]->pid,SIGTERM)
*/

#endif //NYX_TERM_INTERFACE_H
