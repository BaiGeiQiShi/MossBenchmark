/*-------------------------------------------------------------------------
 *
 * sequence.c
 *	  PostgreSQL sequences support code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/sequence.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/bufmask.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/relation.h"
#include "access/table.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_sequence.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/sequence.h"
#include "commands/tablecmds.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/resowner.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

/*
 * We don't want to log each fetching of a value from a sequence,
 * so we pre-log a few fetches in advance. In the event of
 * crash we can lose (skip over) as many values as we pre-logged.
 */
#define SEQ_LOG_VALS 32

/*
 * The "special area" of a sequence's buffer page looks like this.
 */
#define SEQ_MAGIC 0x1717

typedef struct sequence_magic
{
  uint32 magic;
} sequence_magic;

/*
 * We store a SeqTable item for every sequence we have touched in the current
 * session.  This is needed to hold onto nextval/currval state.  (We can't
 * rely on the relcache, since it's only, well, a cache, and may decide to
 * discard entries.)
 */
typedef struct SeqTableData
{
  Oid relid;               /* pg_class OID of this sequence (hash key) */
  Oid filenode;            /* last seen relfilenode of this sequence */
  LocalTransactionId lxid; /* xact in which we last did a seq op */
  bool last_valid;         /* do we have a valid "last" value? */
  int64 last;              /* value last returned by nextval */
  int64 cached;            /* last value already cached for nextval */
  /* if last != cached, we have not used up all the cached values */
  int64 increment; /* copy of sequence's increment field */
  /* note that increment is zero until we first do nextval_internal() */
} SeqTableData;

typedef SeqTableData *SeqTable;

static HTAB *seqhashtab = NULL; /* hash table for SeqTable items */

/*
 * last_used_seq is updated by nextval() to point to the last used
 * sequence.
 */
static SeqTableData *last_used_seq = NULL;

static void
fill_seq_with_data(Relation rel, HeapTuple tuple);
static Relation
lock_and_open_sequence(SeqTable seq);
static void
create_seq_hashtable(void);
static void
init_sequence(Oid relid, SeqTable *p_elm, Relation *p_rel);
static Form_pg_sequence_data
read_seq_tuple(Relation rel, Buffer *buf, HeapTuple seqdatatuple);
static void
init_params(ParseState *pstate, List *options, bool for_identity, bool isInit, Form_pg_sequence seqform, Form_pg_sequence_data seqdataform, bool *need_seq_rewrite, List **owned_by);
static void
do_setval(Oid relid, int64 next, bool iscalled);
static void
process_owned_by(Relation seqrel, List *owned_by, bool for_identity);

/*
 * DefineSequence
 *				Creates a new sequence relation
 */
ObjectAddress
DefineSequence(ParseState *pstate, CreateSeqStmt *seq)
{












































































































































}

/*
 * Reset a sequence to its initial value.
 *
 * The change is made transactionally, so that on failure of the current
 * transaction, the sequence will be restored to its previous state.
 * We do that by creating a whole new relfilenode for the sequence; so this
 * works much like the rewriting forms of ALTER TABLE.
 *
 * Caller is assumed to have acquired AccessExclusiveLock on the sequence,
 * which must not be released until end of transaction.  Caller is also
 * responsible for permissions checking.
 */
void
ResetSequence(Oid seq_relid)
{



































































}

/*
 * Initialize a sequence's relation with the specified tuple as content
 */
static void
fill_seq_with_data(Relation rel, HeapTuple tuple)
{








































































}

/*
 * AlterSequence
 *
 * Modify the definition of a sequence relation
 */
ObjectAddress
AlterSequence(ParseState *pstate, AlterSeqStmt *stmt)
{































































































}

void
DeleteSequenceTuple(Oid relid)
{















}

/*
 * Note: nextval with a text argument is no longer exported as a pg_proc
 * entry, but we keep it around to ease porting of C code that may have
 * called the function directly.
 */
Datum
nextval(PG_FUNCTION_ARGS)
{

















}

Datum
nextval_oid(PG_FUNCTION_ARGS)
{



}

int64
nextval_internal(Oid relid, bool check_permissions)
{



















































































































































































































































}

Datum
currval_oid(PG_FUNCTION_ARGS)
{























}

Datum
lastval(PG_FUNCTION_ARGS)
{




























}

/*
 * Main internal procedure that handles 2 & 3 arg forms of SETVAL.
 *
 * Note that the 3 arg version (which sets the is_called flag) is
 * only for use in pg_dump, and setting the is_called flag may not
 * work if multiple users are attached to the database and referencing
 * the sequence (unlikely if pg_dump is restoring it).
 *
 * It is necessary to have the 3 arg version so that pg_dump can
 * restore the state of a sequence exactly during data-only restores -
 * it is the only way to clear the is_called flag in an existing
 * sequence.
 */
static void
do_setval(Oid relid, int64 next, bool iscalled)
{






































































































}

/*
 * Implement the 2 arg setval procedure.
 * See do_setval for discussion.
 */
Datum
setval_oid(PG_FUNCTION_ARGS)
{






}

/*
 * Implement the 3 arg setval procedure.
 * See do_setval for discussion.
 */
Datum
setval3_oid(PG_FUNCTION_ARGS)
{







}

/*
 * Open the sequence and acquire lock if needed
 *
 * If we haven't touched the sequence already in this transaction,
 * we need to acquire a lock.  We arrange for the lock to
 * be owned by the top transaction, so that we don't need to do it
 * more than once per xact.
 */
static Relation
lock_and_open_sequence(SeqTable seq)
{




















}

/*
 * Creates the hash table for storing sequence data
 */
static void
create_seq_hashtable(void)
{







}

/*
 * Given a relation OID, open and lock the sequence.  p_elm and p_rel are
 * output parameters.
 */
static void
init_sequence(Oid relid, SeqTable *p_elm, Relation *p_rel)
{





















































}

/*
 * Given an opened sequence relation, lock the page buffer and find the tuple
 *
 * *buf receives the reference to the pinned-and-ex-locked buffer
 * *seqdatatuple receives the reference to the sequence tuple proper
 *		(this arg should point to a local variable of type
 *HeapTupleData)
 *
 * Function's return value points to the data payload of the tuple
 */
static Form_pg_sequence_data
read_seq_tuple(Relation rel, Buffer *buf, HeapTuple seqdatatuple)
{











































}

/*
 * init_params: process the options list of CREATE or ALTER SEQUENCE, and
 * store the values into appropriate fields of seqform, for changes that go
 * into the pg_sequence catalog, and fields of seqdataform for changes to the
 * sequence relation itself.  Set *need_seq_rewrite to true if we changed any
 * parameters that require rewriting the sequence's relation (interesting for
 * ALTER SEQUENCE).  Also set *owned_by to any OWNED BY option, or to NIL if
 * there is none.
 *
 * If isInit is true, fill any unspecified options with default values;
 * otherwise, do not change existing options that aren't explicitly overridden.
 *
 * Note: we force a sequence rewrite whenever we change parameters that affect
 * generation of future sequence values, even if the seqdataform per se is not
 * changed.  This allows ALTER SEQUENCE to behave transactionally.  Currently,
 * the only option that doesn't cause that is OWNED BY.  It's *necessary* for
 * ALTER SEQUENCE OWNED BY to not rewrite the sequence, because that would
 * break pg_upgrade by causing unwanted changes in the sequence's relfilenode.
 */
static void
init_params(ParseState *pstate, List *options, bool for_identity, bool isInit, Form_pg_sequence seqform, Form_pg_sequence_data seqdataform, bool *need_seq_rewrite, List **owned_by)
{













































































































































































































































































































































































}

/*
 * Process an OWNED BY option for CREATE/ALTER SEQUENCE
 *
 * Ownership permissions on the sequence are already checked,
 * but if we are establishing a new owned-by dependency, we must
 * enforce that the referenced table has the same owner and namespace
 * as the sequence.
 */
static void
process_owned_by(Relation seqrel, List *owned_by, bool for_identity)
{































































































}

/*
 * Return sequence parameters in a list of the form created by the parser.
 */
List *
sequence_options(Oid relid)
{






















}

/*
 * Return sequence parameters (formerly for use by information schema)
 */
Datum
pg_sequence_parameters(PG_FUNCTION_ARGS)
{











































}

/*
 * Return the last value from the sequence
 *
 * Note: This has a completely different meaning than lastval().
 */
Datum
pg_sequence_last_value(PG_FUNCTION_ARGS)
{

































}

void
seq_redo(XLogReaderState *record)
{
















































}

/*
 * Flush cached sequence information.
 */
void
ResetSequenceCaches(void)
{







}

/*
 * Mask a Sequence page before performing consistency checks on it.
 */
void
seq_mask(char *page, BlockNumber blkno)
{



}