/*-------------------------------------------------------------------------
 * relation.c
 *	   PostgreSQL logical replication
 *
 * Copyright (c) 2016-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/relation.c
 *
 * NOTES
 *	  This file contains helper functions for logical replication relation
 *	  mapping cache.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/relation.h"
#include "access/table.h"
#include "catalog/namespace.h"
#include "catalog/pg_subscription_rel.h"
#include "executor/executor.h"
#include "nodes/makefuncs.h"
#include "replication/logicalrelation.h"
#include "replication/worker_internal.h"
#include "utils/inval.h"

static MemoryContext LogicalRepRelMapContext = NULL;

static HTAB *LogicalRepRelMap = NULL;

/*
 * Relcache invalidation callback for our relation map cache.
 */
static void
logicalrep_relmap_invalidate_cb(Datum arg, Oid reloid)
{





































}

/*
 * Initialize the relation map cache.
 */
static void
logicalrep_relmap_init(void)
{

















}

/*
 * Free the entry of a relation map cache.
 */
static void
logicalrep_relmap_free_entry(LogicalRepRelMapEntry *entry)
{

























}

/*
 * Add new entry or update existing entry in the relation map cache.
 *
 * Called when new relation mapping is sent by the publisher to update
 * our expected view of incoming data from said publisher.
 */
void
logicalrep_relmap_update(LogicalRepRelation *remoterel)
{






































}

/*
 * Find attribute index in TupleDesc struct by attribute name.
 *
 * Returns -1 if not found.
 */
static int
logicalrep_rel_att_by_name(LogicalRepRelation *remoterel, const char *attname)
{











}

/*
 * Open the local relation associated with the remote one.
 *
 * Rebuilds the Relcache mapping if it was invalidated by local DDL.
 */
LogicalRepRelMapEntry *
logicalrep_rel_open(LogicalRepRelId remoteid, LOCKMODE lockmode)
{












































































































































































}

/*
 * Close the previously opened logical relation.
 */
void
logicalrep_rel_close(LogicalRepRelMapEntry *rel, LOCKMODE lockmode)
{


}
