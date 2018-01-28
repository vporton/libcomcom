/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * libcomcom.h
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

#ifndef LIBCOMCOM_H
#define LIBCOMCOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * Initialize the library. Call it before libcomcom_run_command().
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_init(void);

/**
 * Runs an OS command.
 * @param input passed to command stdin
 * @param input_len the length of the string passed to stdin
 * @param output at this location is stored the command's stdout (call `free()` after use)
 * @param output_len at this location is stored the length of command's stdout
 * @param file the command to run (PATH used)
 * @param argv arguments for the command to run
 * @param envp environment for the command to run (pass `environ` to duplicate our environment)
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
 * If your program needs to handle SIGTERM or SIGINT in another way,
 * use libcomcom_terminate() instead.
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_set_default_terminate(void);

/**
 * Uninstall SIGTERM and SIGINT handler which calls libcomcom_terminate().
 * If your program needs to handle SIGTERM or SIGINT in another way,
 * use libcomcom_terminate() instead.
 * @return 0 on success and -1 on error (also sets `errno`).
 */
int libcomcom_reset_default_terminate(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBCOMCOM_H */
