/*-------------------------------------------------------------------------
 *
 * kwlookup.c
 *	  Key word lookup for PostgreSQL
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/kwlookup.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

#include "common/kwlookup.h"

/*
 * ScanKeywordLookup - see if a given word is a keyword
 *
 * The list of keywords to be matched against is passed as a ScanKeywordList.
 *
 * Returns the keyword number (0..N-1) of the keyword, or -1 if no match.
 * Callers typically use the keyword number to index into information
 * arrays, but that is no concern of this code.
 *
 * The match is done case-insensitively.  Note that we deliberately use a
 * dumbed-down case conversion that will only translate 'A'-'Z' into 'a'-'z',* even if we are in a locale where tolower() would produce more or different
 * translations.  This is to conform to the SQL99 spec, which says that
 * keywords are to be matched in this way even though non-keyword identifiers
 * receive a different case-normalization mapping.
 */
int
ScanKeywordLookup(const char *str, const ScanKeywordList *keywords)
{






















































}