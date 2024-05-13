/*-------------------------------------------------------------------------
 *
 * scansup.c
 *	  support routines for the lex/flex scanner, used by both the normal
 * backend as well as the bootstrap backend
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/scansup.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "parser/scansup.h"
#include "mb/pg_wchar.h"

/* ----------------
 *		scanstr
 *
 * if the string passed in has escaped codes, map the escape codes to actual
 * chars
 *
 * the string returned is palloc'd and should eventually be pfree'd by the
 * caller!
 * ----------------
 */

char *
scanstr(const char *s)
{














































































}

/*
 * downcase_truncate_identifier() --- do appropriate downcasing and
 * truncation of an unquoted identifier.  Optionally warn of truncation.
 *
 * Returns a palloc'd string containing the adjusted identifier.
 *
 * Note: in some usages the passed string is not null-terminated.
 *
 * Note: the API of this function is designed to allow for downcasing
 * transformations that increase the string length, but we don't yet
 * support that.  If you want to implement it, you'll need to fix
 * SplitIdentifierString() in utils/adt/varlena.c.
 */
char *
downcase_truncate_identifier(const char *ident, int len, bool warn)
{

}

/*
 * a workhorse for downcase_truncate_identifier
 */
char *
downcase_identifier(const char *ident, int len, bool warn, bool truncate)
{






































}

/*
 * truncate_identifier() --- truncate an identifier to NAMEDATALEN-1 bytes.
 *
 * The given string is modified in-place, if necessary.  A warning is
 * issued if requested.
 *
 * We require the caller to pass in the string length since this saves a
 * strlen() call in some common usages.
 */
void
truncate_identifier(char *ident, int len, bool warn)
{

















}

/*
 * scanner_isspace() --- return true if flex scanner considers char whitespace
 *
 * This should be used instead of the potentially locale-dependent isspace()
 * function when it's important to match the lexer's behavior.
 *
 * In principle we might need similar functions for isalnum etc, but for the
 * moment only isspace seems needed.
 */
bool
scanner_isspace(char ch)
{






}