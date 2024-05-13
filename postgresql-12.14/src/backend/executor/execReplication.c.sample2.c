/*-------------------------------------------------------------------------
 *
 * execReplication.c
 *	  miscellaneous executor routines for logical replication
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/execReplication.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/*
 * Setup a ScanKey for a search in the relation 'rel' for a tuple 'key' that
 * is setup to match 'rel' (*NOT* idxrel!).
 *
 * Returns whether any column contains NULLs.
 *
 * This is not generic routine, it expects the idxrel to be replication
 * identity of a rel and meet all limitations associated with that.
 */
static bool
build_replindex_scan_key(ScanKey skey, Relation rel, Relation idxrel, TupleTableSlot *searchslot)
{



















































}

/*
 * Search the relation 'rel' for tuple using the index.
 *
 * If a matching tuple is found, lock it with lockmode, fill the slot with its
 * contents, and return true.  Return false otherwise.
 */
bool
RelationFindReplTupleByIndex(Relation rel, Oid idxoid, LockTupleMode lockmode, TupleTableSlot *searchslot, TupleTableSlot *outslot)
{























































































}

/*
 * Compare the tuples in the slots by checking if they have equal values.
 */
static bool
tuples_equal(TupleTableSlot *slot1, TupleTableSlot *slot2)
{













































}

/*
 * Search the relation 'rel' for tuple using the sequential scan.
 *
 * If a matching tuple is found, lock it with lockmode, fill the slot with its
 * contents, and return true.  Return false otherwise.
 *
 * Note that this stops on the first matching tuple.
 *
 * This can obviously be quite slow on tables that have more than few rows.
 */
bool
RelationFindReplTupleSeq(Relation rel, LockTupleMode lockmode, TupleTableSlot *searchslot, TupleTableSlot *outslot)
{


























































































}

/*
 * Insert tuple represented in the slot to the relation, update the indexes,
 * and execute any constraints and per-row triggers.
 *
 * Caller is responsible for opening the indexes.
 */
void
ExecSimpleRelationInsert(EState *estate, TupleTableSlot *slot)
{

























































}

/*
 * Find the searchslot tuple and update it with data in the slot,
 * update the indexes, and execute any constraints and per-row triggers.
 *
 * Caller is responsible for opening the indexes.
 */
void
ExecSimpleRelationUpdate(EState *estate, EPQState *epqstate, TupleTableSlot *searchslot, TupleTableSlot *slot)
{




















































}

/*
 * Find the searchslot tuple and delete it, and execute any constraints
 * and per-row triggers.
 *
 * Caller is responsible for opening the indexes.
 */
void
ExecSimpleRelationDelete(EState *estate, EPQState *epqstate, TupleTableSlot *searchslot)
{





















}

/*
 * Check if command can be executed with current replica identity.
 */
void
CheckCmdReplicaIdentity(Relation rel, CmdType cmd)
{
  PublicationActions *pubactions;

  /* We only need to do checks for UPDATE and DELETE. */
  if (cmd != CMD_UPDATE && cmd != CMD_DELETE)
  {
    return;
  }

  /* If relation has replica identity we are always good. */
  if (rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL || OidIsValid(RelationGetReplicaIndex(rel)))
  {
    return;
  }

  /*
   * This is either UPDATE OR DELETE and there is no replica identity.
   *
   * Check if the table publishes UPDATES or DELETES.
   */
  pubactions = GetRelationPublicationActions(rel);
  if (cmd == CMD_UPDATE && pubactions->pubupdate)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot update table \"%s\" because it does not have a replica identity and publishes updates", RelationGetRelationName(rel)), errhint("To enable updating the table, set REPLICA IDENTITY using ALTER TABLE.")));
  }
  else if (cmd == CMD_DELETE && pubactions->pubdelete)
  {

  }
}

/*
 * Check if we support writing into specific relkind.
 *
 * The nspname and relname are only needed for error reporting.
 */
void
CheckSubscriptionRelkind(char relkind, const char *nspname, const char *relname)
{

















}