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

#define READ_END 0
#define WRITE_END 1

typedef struct my_process_t {
    int self[2]; /* TODO: Make global? */
    int child[2]; /* for errno */
    int stdin[2];
    int stdout[2];
    const char *input;
    size_t input_len;
    char *output;
    size_t output_len;
} my_process_t;

my_process_t process;

void sigchld_handler(int sig)
{
    const char c = 'e';
    int wstatus;
    write(process.self[WRITE_END], &c, 1);
    (void)wait(&wstatus); /* otherwise the child becomes a zombie */
}

int libcomcom_init(void)
{
    if(!pipe(process.self)) return -1;
    if(signal(SIGCHLD, sigchld_handler) == SIG_ERR) return -1;
    return 0;
}

/* On failure -1 is returned and errno is set. */
/* FIXME: deallocate output on error. */
/* TODO: return control on stdout close (don't confuse SIGCHLD of different processes) */
/* TODO: Support specifying command environment */
int libcomcom_run_command (const char *input, size_t input_len,
                           const char **output, size_t *output_len,
                           const char *file, char *const argv[])
{
    process.input = input;
    process.input_len = input_len;
    process.output = malloc(1);
    if(!process.output) return -1;
    process.output_len = 0;
    /* FIXME: Close the files after the end. */
    if(!pipe(process.child)) return -1;
    if(!pipe(process.stdin)) return -1;
    if(!pipe(process.stdout)) return -1;

    pid_t pid = fork();
    switch(pid)
    {
    case -1:
        return -1;
        break;
    case 0:
        /* TODO: what happens on error in the middle? */
        if(dup2(process.stdin[READ_END], STDIN_FILENO) == -1) return -1;
        if(close(process.stdin[READ_END])) return -1;
        if(dup2(process.stdout[WRITE_END], STDOUT_FILENO) == -1) return -1;
        if(close(process.stdout[WRITE_END])) return -1;

        /* https://stackoverflow.com/a/13710144/856090 trick */
        if(close(process.child[READ_END])) return -1;
        if(fcntl(process.child[WRITE_END], F_SETFD,
                 fcntl(process.child[WRITE_END], F_GETFD) | FD_CLOEXEC) == -1)
            return -1;

        execvp(file, argv);

        /* if reached here, it is execvpe() failure */
        write(process.child[WRITE_END], &errno, sizeof(errno)); /* deliberately don't check error */
        _exit(EX_OSERR);
        break;

    /* https://stackoverflow.com/q/1584956/856090 & https://stackoverflow.com/q/13710003/856090 */
    default: /* parent process */
        close(process.child[WRITE_END]);
        int errno_copy;
        ssize_t count;
        /* read() will return 0 if execvpe() succeeded. */
        while((count = read(process.child[READ_END], &errno_copy, sizeof(errno_copy))) == -1)
            if(errno != EAGAIN && errno != EINTR) break;
        if(count) return -1;
    }

    struct pollfd fds[] = {
        { process.self[READ_END], POLLIN },
        { process.stdin[WRITE_END], POLLOUT },
        { process.stdout[READ_END], POLLIN },
    };
    switch(poll(fds, 3, 5000)) /* TODO: configurable timeout */
    {
    case -1:
        {
            int poll_errno = errno;
            kill(pid, SIGTERM); /* TODO: SIGKILL on a greater timeout? */
            errno = poll_errno;
        }
        break;
    case 0:
        kill(pid, SIGTERM); /* TODO: SIGKILL on a greater timeout? */
        errno = ETIMEDOUT;
        break;
    default:
        if(fds[0].revents & POLLIN) {
            char dummy;
            read(process.self[READ_END], &dummy, 1);
            *output = process.output;
            *output_len = process.output_len;
            return 0;
        }
        if(fds[1].revents & POLLOUT) { /* TODO: POLLERR */
            if(process.input_len) { /* needed check? */
                size_t count = process.input_len;
                if(count > PIPE_BUF) count = PIPE_BUF; /* atomic write */
                ssize_t real = write(process.stdin[WRITE_END], process.input, count);
                if(real == -1) ; /* TODO: EPIPE, EAGAIN, etc. */
                process.input += real;
                process.input_len -= real;
            }
        }
        if(fds[2].revents & POLLIN) { /* TODO: POLLERR */
            char buf[PIPE_BUF];
            ssize_t real = read(process.stdout[READ_END], buf, PIPE_BUF);
            if(real == -1) ; /* TODO: EPIPE, EAGAIN, etc. */
            if(real > 0) {
                process.output = realloc(process.output, process.output_len + real);
                memcpy(process.output + process.output_len, buf, real);
                process.output_len += real;
            }
        }
    }

    /* FIXME: Close the files after the end. */
    return 0;
}
