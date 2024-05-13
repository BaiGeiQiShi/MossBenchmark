/*-------------------------------------------------------------------------
 *
 * proclang.c
 *	  PostgreSQL LANGUAGE support code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/commands/proclang.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_pltemplate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/proclang.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parser.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  bool tmpltrusted;    /* trusted? */
  bool tmpldbacreate;  /* db owner allowed to create? */
  char *tmplhandler;   /* name of handler function */
  char *tmplinline;    /* name of anonymous-block handler, or NULL */
  char *tmplvalidator; /* name of validator function, or NULL */
  char *tmpllibrary;   /* path of shared library */
} PLTemplate;

static ObjectAddress
create_proc_lang(const char *languageName, bool replace, Oid languageOwner, Oid handlerOid, Oid inlineOid, Oid valOid, bool trusted);
static PLTemplate *
find_language_template(const char *languageName);

/*
 * CREATE LANGUAGE
 */
ObjectAddress
CreateProceduralLanguage(CreatePLangStmt *stmt)
{



























































































































































































}

/*
 * Guts of language creation.
 */
static ObjectAddress
create_proc_lang(const char *languageName, bool replace, Oid languageOwner, Oid handlerOid, Oid inlineOid, Oid valOid, bool trusted)
{































































































































}

/*
 * Look to see if we have template information for the given language name.
 */
static PLTemplate *
find_language_template(const char *languageName)
{































































}

/*
 * This just returns true if we have a valid template for a given language
 */
bool
PLTemplateExists(const char *languageName)
{

}

/*
 * Guts of language dropping.
 */
void
DropProceduralLanguageById(Oid langOid)
{
















}

/*
 * get_language_oid - given a language name, look up the OID
 *
 * If missing_ok is false, throw an error if language name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_language_oid(const char *langname, bool missing_ok)
{








}