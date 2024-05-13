/*-------------------------------------------------------------------------
 *
 * collationcmds.c
 *	  collation-related commands support code
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/collationcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_collation.h"
#include "commands/alter.h"
#include "commands/collationcmds.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/pg_locale.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  char *localename; /* name of locale, as per "locale -a" */
  char *alias;      /* shortened alias for same */
  int enc;          /* encoding */
} CollAliasData;

/*
 * CREATE COLLATION
 */
ObjectAddress
DefineCollation(ParseState *pstate, List *names, List *parameters, bool if_not_exists)
{











































































































































































































































}

/*
 * Subroutine for ALTER COLLATION SET SCHEMA and RENAME
 *
 * Is there a collation with the same name of the given collation already in
 * the given namespace?  If so, raise an appropriate error message.
 */
void
IsThereCollationInNamespace(const char *collname, Oid nspOid)
{











}

/*
 * ALTER COLLATION
 */
ObjectAddress
AlterCollation(AlterCollationStmt *stmt)
{



































































}

Datum
pg_collation_actual_version(PG_FUNCTION_ARGS)
{



























}

/* will we use "locale -a" in pg_import_system_collations? */
#if defined(HAVE_LOCALE_T) && !defined(WIN32)
#define READ_LOCALE_A_OUTPUT
#endif

#if defined(READ_LOCALE_A_OUTPUT) || defined(USE_ICU)
/*
 * Check a string to see if it is pure ASCII
 */
static bool
is_all_ascii(const char *str)
{









}
#endif /* READ_LOCALE_A_OUTPUT || USE_ICU */

#ifdef READ_LOCALE_A_OUTPUT
/*
 * "Normalize" a libc locale name, stripping off encoding tags such as
 * ".utf8" (e.g., "en_US.utf8" -> "en_US", but "br_FR.iso885915@euro"
 * -> "br_FR@euro").  Return true if a new, different name was
 * generated.
 */
static bool
normalize_libc_locale_name(char *new, const char *old)
{
























}

/*
 * qsort comparator for CollAliasData items
 */
static int
cmpaliases(const void *a, const void *b)
{





}
#endif /* READ_LOCALE_A_OUTPUT */

#ifdef USE_ICU
/*
 * Get the ICU language tag for a locale name.
 * The result is a palloc'd string.
 */
static char *
get_icu_language_tag(const char *localename)
{
  char buf[ULOC_FULLNAME_CAPACITY];
  UErrorCode status;

  status = U_ZERO_ERROR;
  uloc_toLanguageTag(localename, buf, sizeof(buf), true, &status);
  if (U_FAILURE(status))
  {
    ereport(ERROR, (errmsg("could not convert locale name \"%s\" to language tag: %s", localename, u_errorName(status))));
  }

  return pstrdup(buf);
}

/*
 * Get a comment (specifically, the display name) for an ICU locale.
 * The result is a palloc'd string, or NULL if we can't get a comment
 * or find that it's not all ASCII.  (We can *not* accept non-ASCII
 * comments, because the contents of template0 must be encoding-agnostic.)
 */
static char *
get_icu_locale_comment(const char *localename)
{
  UErrorCode status;
  UChar displayname[128];
  int32 len_uchar;
  int32 i;
  char *result;

  status = U_ZERO_ERROR;
  len_uchar = uloc_getDisplayName(localename, "en", displayname, lengthof(displayname), &status);
  if (U_FAILURE(status))
  {
    return NULL; /* no good reason to raise an error */
  }

  /* Check for non-ASCII comment (can't use is_all_ascii for this) */
  for (i = 0; i < len_uchar; i++)
  {
    if (displayname[i] > 127)
    {
      return NULL;
    }
  }

  /* OK, transcribe */
  result = palloc(len_uchar + 1);
  for (i = 0; i < len_uchar; i++)
  {
    result[i] = displayname[i];
  }
  result[len_uchar] = '\0';

  return result;
}
#endif /* USE_ICU */

/*
 * pg_import_system_collations: add known system collations to pg_collation
 */
Datum
pg_import_system_collations(PG_FUNCTION_ARGS)
{





































































































































































































































}