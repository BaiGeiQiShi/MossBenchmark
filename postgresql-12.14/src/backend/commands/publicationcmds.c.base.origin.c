/*-------------------------------------------------------------------------
 *
 * publicationcmds.c
 *		publication manipulation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		publicationcmds.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"

#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_publication_rel.h"

#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/publicationcmds.h"

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

/* Same as MAXNUMMESSAGES in sinvaladt.c */
#define MAX_RELCACHE_INVAL_MSGS 4096

static List *
OpenTableList(List *tables);
static void
CloseTableList(List *rels);
static void
PublicationAddTables(Oid pubid, List *rels, bool if_not_exists, AlterPublicationStmt *stmt);
static void
PublicationDropTables(Oid pubid, List *rels, bool missing_ok);

static void
parse_publication_options(List *options, bool *publish_given, bool *publish_insert, bool *publish_update, bool *publish_delete, bool *publish_truncate)
{











































































}

/*
 * Create new publication.
 */
ObjectAddress
CreatePublication(CreatePublicationStmt *stmt)
{






















































































}

/*
 * Change options of a publication.
 */
static void
AlterPublicationOptions(AlterPublicationStmt *stmt, Relation rel, HeapTuple tup)
{












































































}

/*
 * Add or remove table to/from publication.
 */
static void
AlterPublicationTables(AlterPublicationStmt *stmt, Relation rel, HeapTuple tup)
{



































































}

/*
 * Alter the existing publication.
 *
 * This is dispatcher function for AlterPublicationOptions and
 * AlterPublicationTables.
 */
void
AlterPublication(AlterPublicationStmt *stmt)
{

































}

/*
 * Remove the publication by mapping OID.
 */
void
RemovePublicationById(Oid pubid)
{


























}

/*
 * Remove relation from publication by mapping OID.
 */
void
RemovePublicationRelById(Oid proid)
{























}

/*
 * Open relations specified by a RangeVar list.
 * The returned tables are locked in ShareUpdateExclusiveLock mode.
 */
static List *
OpenTableList(List *tables)
{







































































}

/*
 * Close all relations in the list.
 */
static void
CloseTableList(List *rels)
{








}

/*
 * Add listed tables to the publication.
 */
static void
PublicationAddTables(Oid pubid, List *rels, bool if_not_exists, AlterPublicationStmt *stmt)
{























}

/*
 * Remove listed tables from the publication.
 */
static void
PublicationDropTables(Oid pubid, List *rels, bool missing_ok)
{























}

/*
 * Internal workhorse for changing a publication owner
 */
static void
AlterPublicationOwner_internal(Relation rel, HeapTuple tup, Oid newOwnerId)
{










































}

/*
 * Change publication owner -- by name
 */
ObjectAddress
AlterPublicationOwner(const char *name, Oid newOwnerId)
{



























}

/*
 * Change publication owner -- by OID
 */
void
AlterPublicationOwner_oid(Oid subid, Oid newOwnerId)
{

















}