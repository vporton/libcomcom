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

#include "config.h"
#include "libcomcom.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sysexits.h>
#include <limits.h>

#if !HAVE_DECL_EXECVPE
/* from https://github.com/canalplus/r7oss/blob/master/G5/src/klibc-1.5.15/usr/klibc/execvpe.c */
#define DEFAULT_PATH 	"/bin:/usr/bin:."

static int execvpe(const char *file, char *const *argv, char *const *envp)
{
	char path[PATH_MAX];
	const char *searchpath, *esp;
	size_t prefixlen, filelen, totallen;

	if (strchr(file, '/'))	/* Specific path */
		return execve(file, argv, envp);

	filelen = strlen(file);

	searchpath = getenv("PATH");
	if (!searchpath)
		searchpath = DEFAULT_PATH;

	errno = ENOENT;		/* Default errno, if execve() doesn't
				   change it */

	do {
		esp = strchr(searchpath, ':');
		if (esp)
			prefixlen = esp - searchpath;
		else
			prefixlen = strlen(searchpath);

		if (prefixlen == 0 || searchpath[prefixlen - 1] == '/') {
			totallen = prefixlen + filelen;
			if (totallen >= PATH_MAX)
				continue;
			memcpy(path, searchpath, prefixlen);
			memcpy(path + prefixlen, file, filelen);
		} else {
			totallen = prefixlen + filelen + 1;
			if (totallen >= PATH_MAX)
				continue;
			memcpy(path, searchpath, prefixlen);
			path[prefixlen] = '/';
			memcpy(path + prefixlen + 1, file, filelen);
		}
		path[totallen] = '\0';

		execve(path, argv, envp);
		if (errno == E2BIG  || errno == ENOEXEC ||
		    errno == ENOMEM || errno == ETXTBSY)
			break;	/* Report this as an error, no more search */

		searchpath = esp + 1;
	} while (esp);

	return -1;
}
#endif

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

struct sigaction old_sigchld;

void sigchld_handler(int sig, siginfo_t *info, void *context)
{
    /* TODO: Should we report an error is CLD_DUMPED? */
    if(info->si_pid == process.pid) {
            if(info->si_code != CLD_EXITED &&
               info->si_code != CLD_KILLED &&
               info->si_code != CLD_DUMPED)
        {
            return;
        }
        const char c = 'e';
        int wstatus;
        int len, res;
        int old_errno = errno;
        do {
            len = write(self[WRITE_END], &c, 1);
        } while(len == -1 && errno == EINTR);
        do {
            res = wait(&wstatus); /* otherwise the child becomes a zombie */
        } while(res == -1 && errno == EINTR);
        errno = old_errno;
    } else {
        if(old_sigchld.sa_flags & SA_SIGINFO) {
            old_sigchld.sa_sigaction(sig, info, context);
        } else {
            old_sigchld.sa_handler(sig);
        }
    }
}

static int myclose(int fd) {
    int res;
    do {
        res = close(fd);
    } while(res == -1 && errno == EINTR);
    return res;
}

static void clean_pipe(int pipe[2]) {
    if(pipe[0] != -1) {
        myclose(pipe[0]);
        pipe[0] = -1;
    }
    if(pipe[1] != -1) {
        myclose(pipe[1]);
        pipe[1] = -1;
    }
}

static int libcomcom_init_base(struct sigaction *old)
{
    /* No need to initialize static struct. */
    /*
    old_sigchld.sa_handler = NULL;
    old_sigchld.sa_flags = 0;
    */
    if(pipe(self)) return -1;
    struct sigaction sa;
    sa.sa_sigaction = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO/*|SA_NOCLDSTOP*/;
    if(sigaction(SIGCHLD, &sa, old)) {
        int save_errno = errno;
        clean_pipe(self);
        errno = save_errno;
        return -1;
    }
    return 0;
}

int libcomcom_init(void)
{
    return libcomcom_init_base(NULL);
}

int libcomcom_init_stratum(void)
{
    return libcomcom_init_base(&old_sigchld);
}

int libcomcom_init2(struct sigaction *old)
{
    old_sigchld = *old;
    return libcomcom_init_base(NULL);
}

static void clean_process(my_process_t *process) {
    int save_errno = errno;
    clean_pipe(process->child);
    clean_pipe(process->stdin);
    clean_pipe(process->stdout);
    process->pid = -1;
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

int libcomcom_run_command (const char *input, size_t input_len,
                           const char **output, size_t *output_len,
                           const char *file, char *const argv[],
                           char *const envp[],
                           int timeout)
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
        return -1;
        break;
    case 0: /* child process */
        if(dup2(process.stdin[READ_END], STDIN_FILENO) == -1 ||
            myclose(process.stdin[WRITE_END]) ||
            dup2(process.stdout[WRITE_END], STDOUT_FILENO) == -1 ||
            myclose(process.stdout[READ_END]))
        {
            return -1;
        }

        if(myclose(self[READ_END])) return -1;
        if(myclose(self[WRITE_END])) return -1;

        /* https://stackoverflow.com/a/13710144/856090 trick */
        if(myclose(process.child[READ_END])) return -1;
        if(fcntl(process.child[WRITE_END], F_SETFD,
                 fcntl(process.child[WRITE_END], F_GETFD) | FD_CLOEXEC) == -1)
        {
            return -1;
        }

        execvpe(file, argv, envp);

        /* If reached here, it is execvpe() failure. */
        /* No need to check EINTR, because there is no signal handlers. */
        write(process.child[WRITE_END], &errno, sizeof(errno)); /* deliberately don't check error */
        _exit(EX_OSERR);
        break;

    /* https://stackoverflow.com/q/1584956/856090 & https://stackoverflow.com/q/13710003/856090 */
    default: /* parent process */
        if(myclose(process.child[WRITE_END])) {
            process.child[WRITE_END] = -1;
            clean_process(&process);
            return -1;
        }
        if(myclose(process.stdout[WRITE_END])) {
            process.stdout[WRITE_END] = -1;
            clean_process(&process);
            return -1;
        }
        if(myclose(process.stdin[READ_END])) {
            process.stdin[READ_END] = -1;
            clean_process(&process);
            return -1;
        }

        process.pid = pid;

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
        /* FIXME: timeout should apply to the entire time commands run, not on i/o operation. */
        switch(poll(fds, 3, timeout))
        {
        case -1:
            if(errno != EINTR) {
                int poll_errno = errno;
                kill(process.pid, SIGTERM);
                errno = poll_errno;
                clean_process_all(&process);
                return -1;
            }
            break;
        case 0:
            kill(process.pid, SIGTERM);
            errno = ETIMEDOUT;
            clean_process_all(&process);
            return -1;
        default:
            if(fds[0].revents & POLLIN) {
                char dummy;
                int dummy_len;
                do {
                    dummy_len = read(self[READ_END], &dummy, 1);
                } while(dummy_len == -1 && errno == EINTR);

                /* Process is now terminated, read the remaining stdout cache. */
                for(;;) {
                    char buf[PIPE_BUF]; /* I think, we can safely increase this buffer. */
                    ssize_t real;
                    do {
                        real = read(process.stdout[READ_END], buf, PIPE_BUF);
                    } while(real == -1 && errno == EINTR);
                    if(real == 0) break;
                    if(real == -1) {
                        clean_process_all(&process);
                        return -1;
                    }
                    if(real > 0) {
                        process.output = realloc(process.output, process.output_len + real);
                        memcpy(process.output + process.output_len, buf, real);
                        process.output_len += real;
                    }
                }
                clean_process(&process);
                *output = process.output;
                *output_len = process.output_len;
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
                    if(myclose(process.stdin[WRITE_END])) { /* let the child go */
                        clean_process_all(&process);
                        return -1;
                    }
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

    /* unreachable code */
    /*
    clean_process_all(&process);
    return -1;
    */
}

int libcomcom_terminate(void)
{
    libcomcom_destroy();
    if(process.pid == -1) return 0;
    kill(process.pid, SIGTERM);
    return 0;
}

int libcomcom_destroy(void)
{
    if(sigaction(SIGCHLD, &old_sigchld, NULL)) return -1;

    if(myclose(self[READ_END])) {
        myclose(self[WRITE_END]);
        return -1;
    }
    if(myclose(self[WRITE_END])) {
        return -1;
    }
    return 0;
}

static void default_terminate_handler(int sig)
{
    int old_errno = errno;
    libcomcom_terminate();
    errno = old_errno;
}

int libcomcom_set_default_terminate(void)
{
    struct sigaction sa;
    sa.sa_handler = default_terminate_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGTERM, &sa, NULL)) return -1;
    if(sigaction(SIGINT , &sa, NULL)) return -1;
    return 0;
}

int libcomcom_reset_default_terminate(void)
{
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGTERM, &sa, NULL)) return -1;
    if(sigaction(SIGINT , &sa, NULL)) return -1;
    return 0;
}
