/*-------------------------------------------------------------------------
 *
 * username.c
 *	  get user name
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/common/username.c
 *
 *-------------------------------------------------------------------------
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <pwd.h>
#include <unistd.h>

#include "common/username.h"

/*
 * Returns the current user name in a static buffer
 * On error, returns NULL and sets *errstr to point to a palloc'd message
 */
const char *
get_user_name(char **errstr)
{































}

/*
 * Returns the current user name in a static buffer or exits
 */
const char *
get_user_name_or_exit(const char *progname)
{











}