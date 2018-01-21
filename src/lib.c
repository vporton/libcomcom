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

char *libcomcom_run_command (const char *input, size_t input_len,
                             const char *file, char *const argv[], char *const envp[])
{
    process_t process;
    process.input = input;
    process.input_len = input_len;
    process.output = malloc(1);
    process.output_len = 0;
    pipe(process.self);
    pipe(process.child);
    pipe(process.stdin);
    pipe(process.stdout);

    pid_t pid = fork();
    switch(pid) {
    case -1:
        /* TODO */
        break;
    case 0:
        /* TODO: check for errors. */
        dup2(process.stdin[READ_END], STDIN_FILENO);
        close(process.stdin[READ_END]);
        dup2(process.stdout[WRITE_END], STDOUT_FILENO);
        close(process.stdout[WRITE_END]);

        /* https://stackoverflow.com/a/13710144/856090 trick */
        close(process.child[READ_END]);
        fcntl(process.child[WRITE_END], F_SETFD, fcntl(process.child[WRITE_END], F_GETFD) | FD_CLOEXEC));

        execvpe(file, argv, envp);

        /* execvpe() failure */
        write(process.child[WRITE_END], &errno, sizeof(errno)); /* deliberately don't check error */
        _exit(EX_OSERR);
        break;

    /* TODO: https://stackoverflow.com/q/1584956/856090 & https://stackoverflow.com/q/13710003/856090 */
    default:
        close(process.child[WRITE_END]);
        int errno_copy;
        ssize_t count;
        /* read() will return 0 if execvpe() happened. */
        while ((count = read(process.child[READ_END], &errno_copy, sizeof(errno_copy))) == -1)
            if (errno != EAGAIN && errno != EINTR) break;
        if(count) {
            fprintf(stderr, "execvpe: %s\n", strerror(err));
            return NULL; /* FIXME: Reap the child */
        }
    }
}
