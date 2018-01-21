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
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sysexits.h>

#define READ_END 0
#define WRITE_END 1

typedef struct process_t {
    int self[2]; /* TODO: Make global? */
    int child[2]; /* for errno */
    int stdin[2];
    int stdout[2];
    const char *input;
    size_t input_len;
    char *output;
    size_t output_len;
} process_t;

/* On failure NULL is returned and errno is set. */
char *libcomcom_run_command (const char *input, size_t input_len,
                             const char *file, char *const argv[], char *const envp[])
{
    process_t process;
    process.input = input;
    process.input_len = input_len;
    process.output = malloc(1);
    if(!process.output) return NULL;
    process.output_len = 0;
    if(!pipe(process.self)) return NULL;
    if(!pipe(process.child)) return NULL;
    if(!pipe(process.stdin)) return NULL;
    if(!pipe(process.stdout)) return NULL;

    pid_t pid = fork();
    switch(pid)
    {
    case -1:
        return NULL;
        break;
    case 0:
        /* TODO: what happens on error in the middle? */
        if(dup2(process.stdin[READ_END], STDIN_FILENO) == -1) return NULL;
        if(close(process.stdin[READ_END])) return NULL;
        if(dup2(process.stdout[WRITE_END], STDOUT_FILENO) == -1) return NULL;
        if(close(process.stdout[WRITE_END])) return NULL;

        /* https://stackoverflow.com/a/13710144/856090 trick */
        if(close(process.child[READ_END])) return NULL;
        if(fcntl(process.child[WRITE_END], F_SETFD,
                 fcntl(process.child[WRITE_END], F_GETFD) | FD_CLOEXEC) == -1)
            return NULL;

        execvpe(file, argv, envp);

        /* if reached here, it is execvpe() failure */
        write(process.child[WRITE_END], &errno, sizeof(errno)); /* deliberately don't check error */
        _exit(EX_OSERR);
        break;

    /* https://stackoverflow.com/q/1584956/856090 & https://stackoverflow.com/q/13710003/856090 */
    default: /* parent process */
        close(process.child[WRITE_END]);
        int errno_copy;
        ssize_t count;
        /* read() will return 0 if execvpe() happened. */
        while ((count = read(process.child[READ_END], &errno_copy, sizeof(errno_copy))) == -1)
            if (errno != EAGAIN && errno != EINTR) break;
        if(count) return NULL; /* FIXME: Reap the child */
    }
}
