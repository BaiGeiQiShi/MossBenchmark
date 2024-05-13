/*-------------------------------------------------------------------------
 *
 * tsearchcmds.c
 *
 *	  Routines for tsearch manipulation commands
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/tsearchcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_config_map.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_func.h"
#include "tsearch/ts_cache.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
MakeConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap);
static void
DropConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap);

/* --------------------- TS Parser commands ------------------------ */

/*
 * lookup a parser support function and return its OID (as a Datum)
 *
 * attnum is the pg_ts_parser column the function will go into
 */
static Datum
get_ts_parser_func(DefElem *defel, int attnum)
{


















































}

/*
 * make pg_depend entries for a new pg_ts_parser entry
 *
 * Return value is the address of said new entry.
 */
static ObjectAddress
makeParserDependencies(HeapTuple tuple)
{







































}

/*
 * CREATE TEXT SEARCH PARSER
 */
ObjectAddress
DefineTSParser(List *names, List *parameters)
{








































































































}

/*
 * Guts of TS parser deletion.
 */
void
RemoveTSParserById(Oid prsId)
{

















}

/* ---------------------- TS Dictionary commands -----------------------*/

/*
 * make pg_depend entries for a new pg_ts_dict entry
 *
 * Return value is address of the new entry
 */
static ObjectAddress
makeDictionaryDependencies(HeapTuple tuple)
{


























}

/*
 * verify that a template's init method accepts a proposed option list
 */
static void
verify_dictoptions(Oid tmplId, List *dictoptions)
{

















































}

/*
 * CREATE TEXT SEARCH DICTIONARY
 */
ObjectAddress
DefineTSDictionary(List *names, List *parameters)
{


























































































}

/*
 * Guts of TS dictionary deletion.
 */
void
RemoveTSDictionaryById(Oid dictId)
{

















}

/*
 * ALTER TEXT SEARCH DICTIONARY
 */
ObjectAddress
AlterTSDictionary(AlterTSDictionaryStmt *stmt)
{
























































































































}

/* ---------------------- TS Template commands -----------------------*/

/*
 * lookup a template support function and return its OID (as a Datum)
 *
 * attnum is the pg_ts_template column the function will go into
 */
static Datum
get_ts_template_func(DefElem *defel, int attnum)
{
































}

/*
 * make pg_depend entries for a new pg_ts_template entry
 */
static ObjectAddress
makeTSTemplateDependencies(HeapTuple tuple)
{






























}

/*
 * CREATE TEXT SEARCH TEMPLATE
 */
ObjectAddress
DefineTSTemplate(List *names, List *parameters)
{


















































































}

/*
 * Guts of TS template deletion.
 */
void
RemoveTSTemplateById(Oid tmplId)
{

















}

/* ---------------------- TS Configuration commands -----------------------*/

/*
 * Finds syscache tuple of configuration.
 * Returns NULL if no such cfg.
 */
static HeapTuple
GetTSConfigTuple(List *names)
{

















}

/*
 * make pg_depend entries for a new or updated pg_ts_config entry
 *
 * Pass opened pg_ts_config_map relation if there might be any config map
 * entries for the config.
 */
static ObjectAddress
makeConfigurationDependencies(HeapTuple tuple, bool removeOld, Relation mapRel)
{









































































}

/*
 * CREATE TEXT SEARCH CONFIGURATION
 */
ObjectAddress
DefineTSConfiguration(List *names, List *parameters, ObjectAddress *copied)
{
































































































































































}

/*
 * Guts of TS configuration deletion.
 */
void
RemoveTSConfigurationById(Oid cfgId)
{




































}

/*
 * ALTER TEXT SEARCH CONFIGURATION - main entry point
 */
ObjectAddress
AlterTSConfiguration(AlterTSConfigurationStmt *stmt)
{












































}

/*
 * Translate a list of token type names to an array of token type numbers
 */
static int *
getTokenTypes(Oid prsId, List *tokennames)
{














































}

/*
 * ALTER TEXT SEARCH CONFIGURATION ADD/ALTER MAPPING
 */
static void
MakeConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap)
{














































































































































}

/*
 * ALTER TEXT SEARCH CONFIGURATION DROP MAPPING
 */
static void
DropConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap)
{



















































}

/*
 * Serialize dictionary options, producing a TEXT datum from a List of DefElem
 *
 * This is used to form the value stored in pg_ts_dict.dictinitoption.
 * For the convenience of pg_dump, the output is formatted exactly as it
 * would need to appear in CREATE TEXT SEARCH DICTIONARY to reproduce the
 * same options.
 *
 * Note that we assume that only the textual representation of an option's
 * value is interesting --- hence, non-string DefElems get forced to strings.
 */
text *
serialize_deflist(List *deflist)
{






































}

/*
 * Deserialize dictionary options, reconstructing a List of DefElem from TEXT
 *
 * This is also used for prsheadline options, so for backward compatibility
 * we need to accept a few things serialize_deflist() will never emit:
 * in particular, unquoted and double-quoted values.
 */
List *
deserialize_deflist(Datum txt)
{
































































































































































































}