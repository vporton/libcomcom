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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <check.h>
#include "libcomcom.h"

void setup(void)
{
    libcomcom_init();
    libcomcom_set_default_terminate();
}

void teardown(void)
{
    libcomcom_destroy();
}

START_TEST(test_short_cat)
{
    char buf[3] = "qwe";
    const char *output;
    size_t output_len;
    char *const argv[] = { "cat", NULL };
    int res;
    res = libcomcom_run_command(buf, sizeof(buf),
                                &output, &output_len,
                                "cat", argv,
                                5000);
    if(res == -1)
        ck_abort_msg(strerror(errno));
    ck_assert_int_eq(sizeof(buf), output_len);
    ck_assert(!memcmp(output, buf, sizeof(buf)));
}
END_TEST

START_TEST(test_long_cat)
{
    char buf[1000000];
    const char *output;
    size_t output_len;
    char *const argv[] = { "cat", NULL };
    int res;
    for(int i=0; i<sizeof(buf); ++i)
        buf[i] = i%3;
    res = libcomcom_run_command(buf, sizeof(buf),
                                &output, &output_len,
                                "cat", argv,
                                5000);
    if(res == -1)
        ck_abort_msg(strerror(errno));
    ck_assert_int_eq(sizeof(buf), output_len);
    ck_assert(!memcmp(output, buf, sizeof(buf)));
}
END_TEST

/* A past version of this crashed Linux: https://bugzilla.kernel.org/show_bug.cgi?id=198549 */
START_TEST(test_long_dd)
{
    char buf[1000000];
    const char *output;
    size_t output_len;
    char *const argv[] = { "dd", "bs=100000", "count=10", "iflag=fullblock", NULL };
    int res;
    for(int i=0; i<sizeof(buf); ++i)
        buf[i] = i%3;
    res = libcomcom_run_command(buf, sizeof(buf),
                                &output, &output_len,
                                "dd", argv,
                                5000);
    if(res == -1)
        ck_abort_msg(strerror(errno));
    ck_assert_int_eq(sizeof(buf), output_len);
    ck_assert(!memcmp(output, buf, sizeof(buf)));
}
END_TEST

Suite * cat_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Running cat or dd command");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_short_cat);
    tcase_add_test(tc_core, test_long_cat);
    tcase_add_test(tc_core, test_long_dd);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(int argc, char** argv)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = cat_suite();
    sr = srunner_create(s);
    srunner_add_suite(sr, cat_suite());

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

