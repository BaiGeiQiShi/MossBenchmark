/* Convert a broken-down timestamp to a string.  */

/*
 * Copyright 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Based on the UCB version with the copyright notice appearing above.
 *
 * This is ANSIish only when "multibyte character == plain character".
 *
 * IDENTIFICATION
 *	  src/timezone/strftime.c
 */

#include "postgres.h"

#include <fcntl.h>

#include "private.h"

struct lc_time_T
{
  const char *mon[MONSPERYEAR];
  const char *month[MONSPERYEAR];
  const char *wday[DAYSPERWEEK];
  const char *weekday[DAYSPERWEEK];
  const char *X_fmt;
  const char *x_fmt;
  const char *c_fmt;
  const char *am;
  const char *pm;
  const char *date_fmt;
};

#define Locale (&C_time_locale)

static const struct lc_time_T C_time_locale = {{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}, {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}, {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}, {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}, /* X_fmt */
    "%H:%M:%S",                                                                                                                                                                                                                                                                                                                                                                                     /*
                                                                                                                                                                                                                                                                                                                                                                                                     * x_fmt
                                                                                                                                                                                                                                                                                                                                                                                                     *
                                                                                                                                                                                                                                                                                                                                                                                                     * C99 and later require this format. Using just numbers (as here) makes
                                                                                                                                                                                                                                                                                                                                                                                                     * Quakers happier; it's also compatible with SVR4.
                                                                                                                                                                                                                                                                                                                                                                                                     */
    "%m/%d/%y",                                                                                                                                                                                                                                                                                                                                                                                     /*
                                                                                                                                                                                                                                                                                                                                                                                                     * c_fmt
                                                                                                                                                                                                                                                                                                                                                                                                     *
                                                                                                                                                                                                                                                                                                                                                                                                     * C99 and later require this format. Previously this code used "%D %X",* but we now conform to C99. Note that "%a %b %d %H:%M:%S %Y" is used by
                                                                                                                                                                                                                                                                                                                                                                                                     * Solaris 2.3.
                                                                                                                                                                                                                                                                                                                                                                                                     */
    "%a %b %e %T %Y",                                                                                                                                                                                                                                                                                                                                                                               /* am */
    "AM",                                                                                                                                                                                                                                                                                                                                                                                           /* pm */
    "PM",                                                                                                                                                                                                                                                                                                                                                                                           /* date_fmt */
    "%a %b %e %H:%M:%S %Z %Y"};

enum warn
{
  IN_NONE,
  IN_SOME,
  IN_THIS,
  IN_ALL
};

static char *
_add(const char *, char *, const char *);
static char *
_conv(int, const char *, char *, const char *);
static char *
_fmt(const char *, const struct pg_tm *, char *, const char *, enum warn *);
static char *
_yconv(int, int, bool, bool, char *, char const *);

/*
 * Convert timestamp t to string s, a caller-allocated buffer of size maxsize,* using the given format pattern.
 *
 * See also timestamptz_to_str.
 */
size_t
pg_strftime(char *s, size_t maxsize, const char *format, const struct pg_tm *t)
{


















}

static char *
_fmt(const char *format, const struct pg_tm *t, char *pt, const char *ptlim, enum warn *warnp)
{















































































































































































































































































































































}

static char *
_conv(int n, const char *format, char *pt, const char *ptlim)
{




}

static char *
_add(const char *str, char *pt, const char *ptlim)
{





}

/*
 * POSIX and the C Standard are unclear or inconsistent about
 * what %C and %y do if the year is negative or exceeds 9999.
 * Use the convention that %C concatenated with %y yields the
 * same output as %Y, and that %Y contains at least 4 bytes,* with more only if necessary.
 */

static char *
_yconv(int a, int b, bool convert_top, bool convert_yy, char *pt, const char *ptlim)
{

































}