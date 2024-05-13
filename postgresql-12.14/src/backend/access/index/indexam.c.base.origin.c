/*-------------------------------------------------------------------------
 *
 * indexam.c
 *	  general index access method routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/index/indexam.c
 *
 * INTERFACE ROUTINES
 *		index_open		- open an index relation by relation OID
 *		index_close		- close an index relation
 *		index_beginscan - start a scan of an index with amgettuple
 *		index_beginscan_bitmap - start a scan of an index with
 *amgetbitmap index_rescan	- restart a scan of an index index_endscan
 *- end a scan index_insert	- insert an index tuple into a relation
 *		index_markpos	- mark a scan position
 *		index_restrpos	- restore a scan position
 *		index_parallelscan_estimate - estimate shared memory for
 *parallel scan index_parallelscan_initialize - initialize parallel scan
 *		index_parallelrescan  - (re)start a parallel scan of an index
 *		index_beginscan_parallel - join parallel index scan
 *		index_getnext_tid	- get the next TID from a scan
 *		index_fetch_heap		- get the scan's next heap tuple
 *		index_getnext_slot	- get the next tuple from a scan
 *		index_getbitmap - get all tuples from a scan
 *		index_bulk_delete	- bulk deletion of index tuples
 *		index_vacuum_cleanup	- post-deletion cleanup of an index
 *		index_can_return	- does index support index-only scans?
 *		index_getprocid - get a support procedure OID
 *		index_getprocinfo - get a support procedure's lookup info
 *
 * NOTES
 *		This file contains the index_ routines which used
 *		to be a scattered collection of stuff in access/genam.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "catalog/index.h"
#include "catalog/pg_type.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/snapmgr.h"

/* ----------------------------------------------------------------
 *					macros used in index_ routines
 *
 * Note: the ReindexIsProcessingIndex() check in RELATION_CHECKS is there
 * to check that we don't try to scan or do retail insertions into an index
 * that is currently being rebuilt or pending rebuild.  This helps to catch
 * things that don't work when reindexing system catalogs.  The assertion
 * doesn't prevent the actual rebuild because we don't use RELATION_CHECKS
 * when calling the index AM's ambuild routine, and there is no reason for
 * ambuild to call its subsidiary routines through this file.
 * ----------------------------------------------------------------
 */
#define RELATION_CHECKS (AssertMacro(RelationIsValid(indexRelation)), AssertMacro(PointerIsValid(indexRelation->rd_indam)), AssertMacro(!ReindexIsProcessingIndex(RelationGetRelid(indexRelation))))

#define SCAN_CHECKS (AssertMacro(IndexScanIsValid(scan)), AssertMacro(RelationIsValid(scan->indexRelation)), AssertMacro(PointerIsValid(scan->indexRelation->rd_indam)))

#define CHECK_REL_PROCEDURE(pname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (indexRelation->rd_indam->pname == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      elog(ERROR, "function %s is not defined for index %s", CppAsString(pname), RelationGetRelationName(indexRelation));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  } while (0)

#define CHECK_SCAN_PROCEDURE(pname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (scan->indexRelation->rd_indam->pname == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      elog(ERROR, "function %s is not defined for index %s", CppAsString(pname), RelationGetRelationName(scan->indexRelation));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } while (0)

static IndexScanDesc
index_beginscan_internal(Relation indexRelation, int nkeys, int norderbys, Snapshot snapshot, ParallelIndexScanDesc pscan, bool temp_snap);

/* ----------------------------------------------------------------
 *				   index_ interface functions
 * ----------------------------------------------------------------
 */

/* ----------------
 *		index_open - open an index relation by relation OID
 *
 *		If lockmode is not "NoLock", the specified kind of lock is
 *		obtained on the index.  (Generally, NoLock should only be
 *		used if the caller knows it has some appropriate lock on the
 *		index already.)
 *
 *		An error is raised if the index does not exist.
 *
 *		This is a convenience routine adapted for indexscan use.
 *		Some callers may prefer to use relation_open directly.
 * ----------------
 */
Relation
index_open(Oid relationId, LOCKMODE lockmode)
{










}

/* ----------------
 *		index_close - close an index relation
 *
 *		If lockmode is not "NoLock", we then release the specified lock.
 *
 *		Note that it is often sensible to hold a lock beyond
 *index_close; in that case, the lock is released automatically at xact end.
 * ----------------
 */
void
index_close(Relation relation, LOCKMODE lockmode)
{











}

/* ----------------
 *		index_insert - insert an index tuple into a relation
 * ----------------
 */
bool
index_insert(Relation indexRelation, Datum *values, bool *isnull, ItemPointer heap_t_ctid, Relation heapRelation, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{









}

/*
 * index_beginscan - start a scan of an index with amgettuple
 *
 * Caller must be holding suitable locks on the heap and the index.
 */
IndexScanDesc
index_beginscan(Relation heapRelation, Relation indexRelation, Snapshot snapshot, int nkeys, int norderbys)
{















}

/*
 * index_beginscan_bitmap - start a scan of an index with amgetbitmap
 *
 * As above, caller had better be holding some lock on the parent heap
 * relation, even though it's not explicitly mentioned here.
 */
IndexScanDesc
index_beginscan_bitmap(Relation indexRelation, Snapshot snapshot, int nkeys)
{











}

/*
 * index_beginscan_internal --- common code for index_beginscan variants
 */
static IndexScanDesc
index_beginscan_internal(Relation indexRelation, int nkeys, int norderbys, Snapshot snapshot, ParallelIndexScanDesc pscan, bool temp_snap)
{
























}

/* ----------------
 *		index_rescan  - (re)start a scan of an index
 *
 * During a restart, the caller may specify a new set of scankeys and/or
 * orderbykeys; but the number of keys cannot differ from what index_beginscan
 * was told.  (Later we might relax that to "must not exceed", but currently
 * the index AMs tend to assume that scan->numberOfKeys is what to believe.)
 * To restart the scan without changing keys, pass NULL for the key arrays.
 * (Of course, keys *must* be passed on the first call, unless
 * scan->numberOfKeys is zero.)
 * ----------------
 */
void
index_rescan(IndexScanDesc scan, ScanKey keys, int nkeys, ScanKey orderbys, int norderbys)
{
















}

/* ----------------
 *		index_endscan - end a scan
 * ----------------
 */
void
index_endscan(IndexScanDesc scan)
{























}

/* ----------------
 *		index_markpos  - mark a scan position
 * ----------------
 */
void
index_markpos(IndexScanDesc scan)
{




}

/* ----------------
 *		index_restrpos	- restore a scan position
 *
 * NOTE: this only restores the internal scan state of the index AM.  See
 * comments for ExecRestrPos().
 *
 * NOTE: For heap, in the presence of HOT chains, mark/restore only works
 * correctly if the scan's snapshot is MVCC-safe; that ensures that there's at
 * most one returnable tuple in each HOT chain, and so restoring the prior
 * state at the granularity of the index AM is sufficient.  Since the only
 * current user of mark/restore functionality is nodeMergejoin.c, this
 * effectively means that merge-join plans only work for MVCC snapshots.  This
 * could be fixed if necessary, but for now it seems unimportant.
 * ----------------
 */
void
index_restrpos(IndexScanDesc scan)
{















}

/*
 * index_parallelscan_estimate - estimate shared memory for parallel scan
 *
 * Currently, we don't pass any information to the AM-specific estimator,
 * so it can probably only return a constant.  In the future, we might need
 * to pass more information.
 */
Size
index_parallelscan_estimate(Relation indexRelation, Snapshot snapshot)
{



















}

/*
 * index_parallelscan_initialize - initialize parallel scan
 *
 * We initialize both the ParallelIndexScanDesc proper and the AM-specific
 * information which follows it.
 *
 * This function calls access method specific initialization routine to
 * initialize am specific information.  Call this just once in the leader
 * process; then, individual workers attach via index_beginscan_parallel.
 */
void
index_parallelscan_initialize(Relation heapRelation, Relation indexRelation, Snapshot snapshot, ParallelIndexScanDesc target)
{




















}

/* ----------------
 *		index_parallelrescan  - (re)start a parallel scan of an index
 * ----------------
 */
void
index_parallelrescan(IndexScanDesc scan)
{












}

/*
 * index_beginscan_parallel - join parallel index scan
 *
 * Caller must be holding suitable locks on the heap and the index.
 */
IndexScanDesc
index_beginscan_parallel(Relation heaprel, Relation indexrel, int nkeys, int norderbys, ParallelIndexScanDesc pscan)
{



















}

/* ----------------
 * index_getnext_tid - get the next TID from a scan
 *
 * The result is the next TID satisfying the scan keys,
 * or NULL if no more matching tuples exist.
 * ----------------
 */
ItemPointer
index_getnext_tid(IndexScanDesc scan, ScanDirection direction)
{




































}

/* ----------------
 *		index_fetch_heap - get the scan's next heap tuple
 *
 * The result is a visible heap tuple associated with the index TID most
 * recently fetched by index_getnext_tid, or NULL if no more matching tuples
 * exist.  (There can be more than one matching tuple because of HOT chains,
 * although when using an MVCC snapshot it should be impossible for more than
 * one such tuple to exist.)
 *
 * On success, the buffer containing the heap tup is pinned (the pin will be
 * dropped in a future index_getnext_tid, index_fetch_heap or index_endscan
 * call).
 *
 * Note: caller must check scan->xs_recheck, and perform rechecking of the
 * scan keys if required.  We do not do that here because we don't have
 * enough information to do it efficiently in the general case.
 * ----------------
 */
bool
index_fetch_heap(IndexScanDesc scan, TupleTableSlot *slot)
{























}

/* ----------------
 *		index_getnext_slot - get the next tuple from a scan
 *
 * The result is true if a tuple satisfying the scan keys and the snapshot was
 * found, false otherwise.  The tuple is stored in the specified slot.
 *
 * On success, resources (like buffer pins) are likely to be held, and will be
 * dropped by a future index_getnext_tid, index_fetch_heap or index_endscan
 * call).
 *
 * Note: caller must check scan->xs_recheck, and perform rechecking of the
 * scan keys if required.  We do not do that here because we don't have
 * enough information to do it efficiently in the general case.
 * ----------------
 */
bool
index_getnext_slot(IndexScanDesc scan, ScanDirection direction, TupleTableSlot *slot)
{































}

/* ----------------
 *		index_getbitmap - get all tuples at once from an index scan
 *
 * Adds the TIDs of all heap tuples satisfying the scan keys to a bitmap.
 * Since there's no interlock between the index scan and the eventual heap
 * access, this is only safe to use with MVCC-based snapshots: the heap
 * item slot could have been replaced by a newer tuple by the time we get
 * to it.
 *
 * Returns the number of matching tuples found.  (Note: this might be only
 * approximate, so it should only be used for statistical purposes.)
 * ----------------
 */
int64
index_getbitmap(IndexScanDesc scan, TIDBitmap *bitmap)
{
















}

/* ----------------
 *		index_bulk_delete - do mass deletion of index entries
 *
 *		callback routine tells whether a given main-heap tuple is
 *		to be deleted
 *
 *		return value is an optional palloc'd struct of statistics
 * ----------------
 */
IndexBulkDeleteResult *
index_bulk_delete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{






}

/* ----------------
 *		index_vacuum_cleanup - do post-deletion cleanup of an index
 *
 *		return value is an optional palloc'd struct of statistics
 * ----------------
 */
IndexBulkDeleteResult *
index_vacuum_cleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{






}

/* ----------------
 *		index_can_return
 *
 *		Does the index access method support index-only scans for the
 *given column?
 * ----------------
 */
bool
index_can_return(Relation indexRelation, int attno)
{









}

/* ----------------
 *		index_getprocid
 *
 *		Index access methods typically require support routines that are
 *		not directly the implementation of any WHERE-clause query
 *operator and so cannot be kept in pg_amop.  Instead, such routines are kept in
 *pg_amproc.  These registered procedure OIDs are assigned numbers according to
 *a convention established by the access method. The general index code doesn't
 *know anything about the routines involved; it just builds an ordered list of
 *them for each attribute on which an index is defined.
 *
 *		As of Postgres 8.3, support routines within an operator family
 *		are further subdivided by the "left type" and "right type" of
 *the query operator(s) that they support.  The "default" functions for a
 *		particular indexed attribute are those with both types equal to
 *		the index opclass' opcintype (note that this is subtly different
 *		from the indexed attribute's own type: it may be a
 *binary-compatible type instead).  Only the default functions are stored in
 *relcache entries --- access methods can use the syscache to look up
 *non-default functions.
 *
 *		This routine returns the requested default procedure OID for a
 *		particular indexed attribute.
 * ----------------
 */
RegProcedure
index_getprocid(Relation irel, AttrNumber attnum, uint16 procnum)
{















}

/* ----------------
 *		index_getprocinfo
 *
 *		This routine allows index AMs to keep fmgr lookup info for
 *		support procs in the relcache.  As above, only the "default"
 *		functions for any particular indexed attribute are cached.
 *
 * Note: the return value points into cached data that will be lost during
 * any relcache rebuild!  Therefore, either use the callinfo right away,
 * or save it only after having acquired some type of lock on the index rel.
 * ----------------
 */
FmgrInfo *
index_getprocinfo(Relation irel, AttrNumber attnum, uint16 procnum)
{









































}

/* ----------------
 *		index_store_float8_orderby_distances
 *
 *		Convert AM distance function's results (that can be inexact)
 *		to ORDER BY types and save them into
 *xs_orderbyvals/xs_orderbynulls for a possible recheck.
 * ----------------
 */
void
index_store_float8_orderby_distances(IndexScanDesc scan, Oid *orderByTypes, IndexOrderByDistance *distances, bool recheckOrderBy)
{


































































}