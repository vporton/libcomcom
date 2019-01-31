/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * libcomcom.h
 * Copyright (C) 2018 Victor Porton <porton@narod.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef LIBCOMCOM_H
#define LIBCOMCOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <signal.h>

/**
 * Initialize the library. Call it before libcomcom_run_command().
 * Note that this erases the old SIGCHLD handler (if any).
 * @return 0 on success and -1 on error (also sets `errno`).
 *
 * You should usually also initialize SIGTERM/SIGINT signal handlers.
 */
int libcomcom_init(void);

/**
 * Initialize the library. Call it before libcomcom_run_command().
 * The `old` signal action is stored internally (and restored by
 * libcomcom_destroy() or libcomcom_terminate()).
 * The old signal handler (the one obtained from `old` variable) is also
 * called from our SIGCHLD handler.
 * @return 0 on success and -1 on error (also sets `errno`).
 *
 * You should usually also initialize SIGTERM/SIGINT signal handlers.
 */
int libcomcom_init2(struct sigaction *old);

/**
 * Initialize the library. Call it before libcomcom_run_command().
 * This function is like libcomcom_init2(), but the old SIGCHLD signal handler
 * is obtained automatically (by sigaction() library function).
 *
 * WARNING: If before calling this SIGCHLD handler was set by signal()
 * (not by sigaction()), then this function may not work (leads to undefined
 * behavior) on some non-Unix systems.
 * @return 0 on success and -1 on error (also sets `errno`).
 *
 * You should usually also initialize SIGTERM/SIGINT signal handlers.
 */
int libcomcom_init_stratum(void);

/**
 * Runs an OS command.
 * @param input passed to command stdin
 * @param input_len the length of the string passed to stdin
 * @param output at this location is stored the command's stdout (call `free()` after use)
 * @param output_len at this location is stored the length of command's stdout
 * @param file the command to run (PATH used)
 * @param argv arguments for the command to run
 * @param envp environment for the command to run (pass `NULL` to duplicate our environment)
 * @param timeout timeout in milliseconds, -1 means infinite timeout
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_run_command(const char *input, size_t input_len,
                          const char **output, size_t *output_len,
                          const char *file, char *const argv[],
                          char *const envp[],
                          int timeout);

/**
 * Should be run for normal termination (not in SIGTERM/SIGINT handler)
 * of our program.
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_destroy(void);

/**
 * Usually should be run in SIGTERM and SIGINT handlers.
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_terminate(void);

/**
 * Install SIGTERM and SIGINT handler which calls libcomcom_terminate().
 * @return 0 on success and -1 on error (also sets `errno`).
 * 
 * You are recommended to use libcomcom_set_default_terminate2()
 * instead.
 */
int libcomcom_set_default_terminate(void);

/**
 * Uninstall SIGTERM and SIGINT handler which calls libcomcom_terminate().
 * @return 0 on success and -1 on error (also sets `errno`).
 * 
 * You are recommended to use libcomcom_reset_default_terminate2()
 * instead.
 */
int libcomcom_reset_default_terminate(void);

/**
 * Install SIGTERM and SIGINT handler which calls libcomcom_terminate().
 * @return 0 on success and -1 on error (also sets `errno`).
 *
 * The installed signal handler also calls old signal handler automatically.
 *
 * WARNING: If before calling these handlers were set by signal()
 * (not by sigaction()), then this function may not work (leads to undefined
 * behavior) on some non-Unix systems.
 */
int libcomcom_set_default_terminate2(void);

/**
 * Resets to signal handlers which were before calling
 * libcomcom_set_default_terminate2().
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_reset_default_terminate2(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBCOMCOM_H */
