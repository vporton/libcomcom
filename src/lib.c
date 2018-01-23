/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * lib.c
 * Copyright (C) 2018 Victor Porton <porton@narod.ru>
 *
 * libcomcom is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libcomcom is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.";
 */

#include "libcomcom.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sysexits.h>
#include <limits.h>

#define READ_END  0
#define WRITE_END 1

int self[2]; /* process self-communication, see HACKING */

typedef struct my_process_t {
    pid_t pid;
    int child[2]; /* for errno */
    int stdin[2];
    int stdout[2];
    const char *input;
    size_t input_len;
    char *output;
    size_t output_len;
} my_process_t;

my_process_t process = { -1, {-1, -1}, {-1, -1}, {-1, -1}, NULL, 0, NULL, 0 };

void sigchld_handler(int sig)
{
    const char c = 'e';
    int wstatus;
    write(self[WRITE_END], &c, 1);
    (void)wait(&wstatus); /* otherwise the child becomes a zombie */
}

int libcomcom_init(void)
{
    if(pipe(self)) return -1;
    if(signal(SIGCHLD, sigchld_handler) == SIG_ERR) return -1;
    return 0;


static void clean_pipe(int pipe[2]) {
    if(pipe[0] != -1) {
        close(pipe[0]);
        pipe[0] = -1;
    }
    if(pipe[1] != -1) {
        close(pipe[1]);
        pipe[1] = -1;
    }
}

static void clean_process(my_process_t *process) {
    int save_errno = errno;
    clean_pipe(process->child);
    clean_pipe(process->stdin);
    clean_pipe(process->stdout);
    errno = save_errno;
}

static void clean_process_all(my_process_t *process) {
    int save_errno = errno;
    clean_process(process);
    if(process->output) {
        free(process->output);
        process->output = NULL;
    }
    errno = save_errno;
}

/* On failure -1 is returned and errno is set. */
/* TODO: return control on stdout close (don't confuse SIGCHLD of different processes) */
/* TODO: Support specifying command environment */
/* FIXME: It seems that EINTR is not always handled! */
int libcomcom_run_command (const char *input, size_t input_len,
                           const char **output, size_t *output_len,
                           const char *file, char *const argv[])
{
    process.input = input;
    process.input_len = input_len;
    process.output = malloc(1);
    if(!process.output) return -1;
    process.output_len = 0;
    if(pipe(process.child) || pipe(process.stdin) || pipe(process.stdout)) {
        clean_process(&process);
        return -1;
    }

    pid_t pid = fork();
    switch(pid)
    {
    case -1:
        clean_process(&process);
        break;
    case 0:
        /* TODO: what happens on error in the middle? */
        if(dup2(process.stdin[READ_END], STDIN_FILENO) == -1 ||
            close(process.stdin[WRITE_END]) ||
            dup2(process.stdout[WRITE_END], STDOUT_FILENO) == -1 ||
            close(process.stdout[READ_END]))
        {
            clean_process(&process);
            return -1;
        }

        /* https://stackoverflow.com/a/13710144/856090 trick */
        if(close(process.child[READ_END])) return -1;
        if(fcntl(process.child[WRITE_END], F_SETFD,
                 fcntl(process.child[WRITE_END], F_GETFD) | FD_CLOEXEC) == -1)
        {
            clean_process(&process);
            return -1;
        }

        execvp(file, argv);

        /* if reached here, it is execvp() failure */
        write(process.child[WRITE_END], &errno, sizeof(errno)); /* deliberately don't check error */
        _exit(EX_OSERR);
        break;

    /* https://stackoverflow.com/q/1584956/856090 & https://stackoverflow.com/q/13710003/856090 */
    default: /* parent process */
        close(process.child[WRITE_END]);
        process.child[WRITE_END] = -1;
        ssize_t count;
        /* read() will return 0 if execvpe() succeeded. */
        while((count = read(process.child[READ_END], &errno, sizeof(errno))) == -1)
            if(errno != EAGAIN && errno != EINTR) break;
        if(count) {
            clean_process_all(&process);
            return -1;
        }
    }

    struct pollfd fds[] = {
        { self[READ_END], POLLIN },
        { process.stdin[WRITE_END], POLLOUT },
        { process.stdout[READ_END], POLLIN },
    };
    for(;;) {
        /* FIXME: It seems after receiving SIGCHLD data to read/write may nevetheless remain.
           Solve this race condition. Also test it under strace and firejail. */
        switch(poll(fds, 3, 5000)) /* TODO: configurable timeout */
        {
        case -1:
            {
                int poll_errno = errno;
                kill(process.pid, SIGTERM); /* TODO: SIGKILL on a greater timeout? (here and in _terminate) */
                errno = poll_errno;
                clean_process_all(&process);
                return -1;
            }
            break;
        case 0:
            kill(pid, SIGTERM); /* TODO: SIGKILL on a greater timeout? */
            errno = ETIMEDOUT;
            clean_process_all(&process);
            return -1;
        default:
            if(fds[0].revents & POLLIN) {
                char dummy;
                read(self[READ_END], &dummy, 1);
                *output = process.output;
                *output_len = process.output_len;
                clean_process(&process);
                return 0;
            }
            /* No need to handle POLERR, as it just causes no more events. */
            if(fds[1].revents & POLLOUT) {
                if(process.input_len) { /* needed check? */
                    size_t count = process.input_len;
                    ssize_t real;
                    if(count > PIPE_BUF) count = PIPE_BUF; /* atomic write */
                    do {
                        real = write(process.stdin[WRITE_END], process.input, count);
                    } while(real == -1 && errno == EINTR);
                    if(real == -1 && errno != EAGAIN && errno != EPIPE) /* if EPIPE, then no more events, ignore it */
                        return -1;
                    if(real > 0) {
                        process.input += real;
                        process.input_len -= real;
                    }
                }
                if(!process.input_len)
                    close(process.stdin[WRITE_END]); /* let the child go */ /* FIXME: errno */
            }
            if(fds[2].revents & POLLIN) {
                char buf[PIPE_BUF];
                ssize_t real;
                do {
                    real = read(process.stdout[READ_END], buf, PIPE_BUF);
                } while(real == -1 && errno == EINTR);
                if(real == -1 && errno != EAGAIN && errno != EPIPE) { /* if EPIPE, then no more events, ignore it */
                    clean_process_all(&process);
                    return -1;
                }
                if(real > 0) {
                    process.output = realloc(process.output, process.output_len + real);
                    memcpy(process.output + process.output_len, buf, real);
                    process.output_len += real;
                }
            }
        }
    }

    clean_process_all(&process); /* TODO: Is this code reachable? */
    return -1;
}

/* Return -1 on error. */
int libcomcom_terminate(void)
{
    if(process.pid == -1) return 0;
    return kill(process.pid, SIGTERM);
}

static void default_terminate_handler(int sig)
{
    libcomcom_set_default_terminate();
}

/* Return -1 on error. */
int libcomcom_set_default_terminate(void)
{
    if(signal(SIGTERM, default_terminate_handler) == SIG_ERR) return -1;
    if(signal(SIGINT , default_terminate_handler) == SIG_ERR) return -1;
    return 0;
}
