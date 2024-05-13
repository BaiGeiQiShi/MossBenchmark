/*-------------------------------------------------------------------------
 *
 * regis.c
 *		Fast regex subset
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/tsearch/regis.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "tsearch/dicts/regis.h"
#include "tsearch/ts_locale.h"

#define RS_IN_ONEOF 1
#define RS_IN_ONEOF_IN 2
#define RS_IN_NONEOF 3
#define RS_IN_WAIT 4

/*
 * Test whether a regex is of the subset supported here.
 * Keep this in sync with RS_compile!
 */
bool
RS_isRegis(const char *str)
{






















































}

static RegisNode *
newRegisNode(RegisNode *prev, int len)
{








}

void
RS_compile(Regis *r, bool issuffix, const char *str)
{
































































































}

void
RS_free(Regis *r)
{










}

static bool
mb_strchr(char *str, char *c)
{


























}

bool
RS_execute(Regis *r, char *str)
{

















































}