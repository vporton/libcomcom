/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * lib.h
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

#include <stddef.h>

int libcomcom_init(void);

int libcomcom_run_command (const char *input, size_t input_len,
                           const char **output, size_t *output_len,
                           const char *file, char *const argv[]);

/* Should be run for normal termination (not in SIGTERM/SEGINT handler. */
int libcomcom_destroy(void);

/* Usually should be run in SIGTERM and SIGINT handlers. */
int libcomcom_terminate(void);

/* Install SIGTERM and SIGINT handlers whcih call libcomcom_terminate(). */
int libcomcom_set_default_terminate(void);

#endif /* LIBCOMCOM_H */
