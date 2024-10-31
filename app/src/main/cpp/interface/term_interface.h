//
// Created by Rohit Paul on 16-10-2024.
//

#ifndef NYX_TERM_INTERFACE_H
#define NYX_TERM_INTERFACE_H

#include "emulation/term_emulator.h"
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/wait.h>

#define MAX_TERM_SESSIONS 5

#define BUFFER_SIZE 2048


static struct epoll_event event_que[MAX_TERM_SESSIONS];

static int epoll_fd = -1;
Term_emulator *sessions[MAX_TERM_SESSIONS] = {NULL};

int occupied_len = 0;


static void cleanup(Term_emulator *const emulator) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, emulator->fd, NULL);
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

_Noreturn static void *loop() {
    uint8_t buffer[BUFFER_SIZE];
    int read_from_fd = 0;
    Term_emulator *emulator;
    int events_ready = 0;
    while (true) {
        events_ready = epoll_wait(epoll_fd, event_que, occupied_len, -1);
        while (--events_ready >= 0) {
            emulator = event_que[events_ready].data.ptr;
            do {
                read_from_fd = read(emulator->fd, buffer, BUFFER_SIZE);
                process_byte(emulator, buffer, read_from_fd);
                //update_screen();
            } while (read_from_fd == BUFFER_SIZE);
        }
    }
}

void init_terminal_session_manager() {
    struct sigaction sa;
    pthread_t pthread;

    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    epoll_fd = epoll_create1(0);

    pthread_create(&pthread, NULL, loop, NULL);
    pthread_detach(pthread);
}

void add_term_session(const char *const cmd) {
    if (occupied_len >= MAX_TERM_SESSIONS) return;
    struct epoll_event event;

    event.data.ptr = sessions[occupied_len] = new_term_emulator(cmd);
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sessions[occupied_len]->fd, &event);
    occupied_len++;
}


#endif //NYX_TERM_INTERFACE_H
