/*-------------------------------------------------------------------------
 *
 * proto.c
 *		logical replication protocol functions
 *
 * Copyright (c) 2015-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		src/backend/replication/logical/proto.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "replication/logicalproto.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/*
 * Protocol message flags.
 */
#define LOGICALREP_IS_REPLICA_IDENTITY 1

#define TRUNCATE_CASCADE (1 << 0)
#define TRUNCATE_RESTART_SEQS (1 << 1)

static void
logicalrep_write_attrs(StringInfo out, Relation rel);
static void
logicalrep_write_tuple(StringInfo out, Relation rel, HeapTuple tuple);

static void
logicalrep_read_attrs(StringInfo in, LogicalRepRelation *rel);
static void
logicalrep_read_tuple(StringInfo in, LogicalRepTupleData *tuple);

static void
logicalrep_write_namespace(StringInfo out, Oid nspid);
static const char *
logicalrep_read_namespace(StringInfo in);

/*
 * Write BEGIN to the output stream.
 */
void
logicalrep_write_begin(StringInfo out, ReorderBufferTXN *txn)
{






}

/*
 * Read transaction BEGIN from the stream.
 */
void
logicalrep_read_begin(StringInfo in, LogicalRepBeginData *begin_data)
{








}

/*
 * Write COMMIT to the output stream.
 */
void
logicalrep_write_commit(StringInfo out, ReorderBufferTXN *txn, XLogRecPtr commit_lsn)
{











}

/*
 * Read transaction COMMIT from the stream.
 */
void
logicalrep_read_commit(StringInfo in, LogicalRepCommitData *commit_data)
{












}

/*
 * Write ORIGIN to the output stream.
 */
void
logicalrep_write_origin(StringInfo out, const char *origin, XLogRecPtr origin_lsn)
{







}

/*
 * Read ORIGIN from the output stream.
 */
char *
logicalrep_read_origin(StringInfo in, XLogRecPtr *origin_lsn)
{





}

/*
 * Write INSERT to the output stream.
 */
void
logicalrep_write_insert(StringInfo out, Relation rel, HeapTuple newtuple)
{







}

/*
 * Read INSERT from stream.
 *
 * Fills the new tuple.
 */
LogicalRepRelId
logicalrep_read_insert(StringInfo in, LogicalRepTupleData *newtup)
{















}

/*
 * Write UPDATE to the output stream.
 */
void
logicalrep_write_update(StringInfo out, Relation rel, HeapTuple oldtuple, HeapTuple newtuple)
{






















}

/*
 * Read UPDATE from stream.
 */
LogicalRepRelId
logicalrep_read_update(StringInfo in, bool *has_oldtuple, LogicalRepTupleData *oldtup, LogicalRepTupleData *newtup)
{



































}

/*
 * Write DELETE to the output stream.
 */
void
logicalrep_write_delete(StringInfo out, Relation rel, HeapTuple oldtuple)
{

















}

/*
 * Read DELETE from stream.
 *
 * Fills the old tuple.
 */
LogicalRepRelId
logicalrep_read_delete(StringInfo in, LogicalRepTupleData *oldtup)
{
















}

/*
 * Write TRUNCATE to the output stream.
 */
void
logicalrep_write_truncate(StringInfo out, int nrelids, Oid relids[], bool cascade, bool restart_seqs)
{






















}

/*
 * Read TRUNCATE from stream.
 */
List *
logicalrep_read_truncate(StringInfo in, bool *cascade, bool *restart_seqs)
{


















}

/*
 * Write relation description to the output stream.
 */
void
logicalrep_write_rel(StringInfo out, Relation rel)
{

















}

/*
 * Read the relation info from stream and return as LogicalRepRelation.
 */
LogicalRepRelation *
logicalrep_read_rel(StringInfo in)
{















}

/*
 * Write type info to the output stream.
 *
 * This function will always write base type info.
 */
void
logicalrep_write_typ(StringInfo out, Oid typoid)
{





















}

/*
 * Read type info from the output stream.
 */
void
logicalrep_read_typ(StringInfo in, LogicalRepTyp *ltyp)
{





}

/*
 * Write a tuple to the outputstream, in the most efficient format possible.
 */
static void
logicalrep_write_tuple(StringInfo out, Relation rel, HeapTuple tuple)
{






























































}

/*
 * Read tuple in remote format from stream.
 *
 * The returned tuple points into the input stringinfo.
 */
static void
logicalrep_read_tuple(StringInfo in, LogicalRepTupleData *tuple)
{











































}

/*
 * Write relation attributes to the stream.
 */
static void
logicalrep_write_attrs(StringInfo out, Relation rel)
{
























































}

/*
 * Read relation attribute names from the stream.
 */
static void
logicalrep_read_attrs(StringInfo in, LogicalRepRelation *rel)
{




































}

/*
 * Write the namespace name or empty string for pg_catalog (to save space).
 */
static void
logicalrep_write_namespace(StringInfo out, Oid nspid)
{















}

/*
 * Read the namespace name while treating empty string as pg_catalog.
 */
static const char *
logicalrep_read_namespace(StringInfo in)
{








}