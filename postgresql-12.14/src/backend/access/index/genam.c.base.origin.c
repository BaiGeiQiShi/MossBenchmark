/*-------------------------------------------------------------------------
 *
 * genam.c
 *	  general index access method routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/index/genam.c
 *
 * NOTES
 *	  many of the old access method routines have been turned into
 *	  macros and moved to genam.h -cim 4/30/91
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "catalog/index.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/* ----------------------------------------------------------------
 *		general access method routines
 *
 *		All indexed access methods use an identical scan structure.
 *		We don't know how the various AMs do locking, however, so we
 *don't do anything about that here.
 *
 *		The intent is that an AM implementor will define a beginscan
 *routine that calls RelationGetIndexScan, to fill in the scan, and then does
 *		whatever kind of locking he wants.
 *
 *		At the end of a scan, the AM's endscan routine undoes the
 *locking, but does *not* call IndexScanEnd --- the higher-level index_endscan
 *		routine does that.  (We can't do it in the AM because
 *index_endscan still needs to touch the IndexScanDesc after calling the AM.)
 *
 *		Because of this, the AM does not have a choice whether to call
 *		RelationGetIndexScan or not; its beginscan routine must return
 *an object made by RelationGetIndexScan.  This is kinda ugly but not worth
 *cleaning up now.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	RelationGetIndexScan -- Create and fill an IndexScanDesc.
 *
 *		This routine creates an index scan structure and sets up initial
 *		contents for it.
 *
 *		Parameters:
 *				indexRelation -- index relation for scan.
 *				nkeys -- count of scan keys (index qual
 *conditions). norderbys -- count of index order-by operators.
 *
 *		Returns:
 *				An initialized IndexScanDesc.
 * ----------------
 */
IndexScanDesc
RelationGetIndexScan(Relation indexRelation, int nkeys, int norderbys)
{























































}

/* ----------------
 *	IndexScanEnd -- End an index scan.
 *
 *		This routine just releases the storage acquired by
 *		RelationGetIndexScan().  Any AM-level resources are
 *		assumed to already have been released by the AM's
 *		endscan routine.
 *
 *	Returns:
 *		None.
 * ----------------
 */
void
IndexScanEnd(IndexScanDesc scan)
{










}

/*
 * BuildIndexValueDescription
 *
 * Construct a string describing the contents of an index entry, in the
 * form "(key_name, ...)=(key_value, ...)".  This is currently used
 * for building unique-constraint and exclusion-constraint error messages,
 * so only key columns of the index are checked and printed.
 *
 * Note that if the user does not have permissions to view all of the
 * columns involved then a NULL is returned.  Returning a partial key seems
 * unlikely to be useful and we have no way to know which of the columns the
 * user provided (unlike in ExecBuildSlotValueDescription).
 *
 * The passed-in values/nulls arrays are the "raw" input to the index AM,
 * e.g. results of FormIndexDatum --- this is not necessarily what is stored
 * in the index, but it's what the user perceives to be stored.
 *
 * Note: if you change anything here, check whether
 * ExecBuildSlotPartitionKeyDescription() in execMain.c needs a similar
 * change.
 */
char *
BuildIndexValueDescription(Relation indexRelation, Datum *values, bool *isnull)
{

































































































}

/*
 * Get the latestRemovedXid from the table entries pointed at by the index
 * tuples being deleted.
 */
TransactionId
index_compute_xid_horizon_for_tuples(Relation irel, Relation hrel, Buffer ibuf, OffsetNumber *itemnos, int nitems)
{






















}

/* ----------------------------------------------------------------
 *		heap-or-index-scan access to system catalogs
 *
 *		These functions support system catalog accesses that normally
 *use an index but need to be capable of being switched to heap scans if the
 *system indexes are unavailable.
 *
 *		The specified scan keys must be compatible with the named index.
 *		Generally this means that they must constrain either all columns
 *		of the index, or the first K columns of an N-column index.
 *
 *		These routines could work with non-system tables, actually,
 *		but they're only useful when there is a known index to use with
 *		the given scan keys; so in practice they're only good for
 *		predetermined types of scans of system catalogs.
 * ----------------------------------------------------------------
 */

/*
 * systable_beginscan --- set up for heap-or-index scan
 *
 *	rel: catalog to scan, already opened and suitably locked
 *	indexId: OID of index to conditionally use
 *	indexOK: if false, forces a heap scan (see notes below)
 *	snapshot: time qual to use (NULL for a recent catalog snapshot)
 *	nkeys, key: scan keys
 *
 * The attribute numbers in the scan key should be set for the heap case.
 * If we choose to index, we reset them to 1..n to reference the index
 * columns.  Note this means there must be one scankey qualification per
 * index column!  This is checked by the Asserts in the normal, index-using
 * case, but won't be checked if the heapscan path is taken.
 *
 * The routine checks the normal cases for whether an indexscan is safe,
 * but caller can make additional checks and pass indexOK=false if needed.
 * In standard case indexOK can simply be constant TRUE.
 */
SysScanDesc
systable_beginscan(Relation heapRelation, Oid indexId, bool indexOK, Snapshot snapshot, int nkeys, ScanKey key)
{








































































}

/*
 * systable_getnext --- get next tuple in a heap-or-index scan
 *
 * Returns NULL if no more tuples available.
 *
 * Note that returned tuple is a reference to data in a disk buffer;
 * it must not be modified, and should be presumed inaccessible after
 * next getnext() or endscan() call.
 *
 * XXX: It'd probably make sense to offer a slot based interface, at least
 * optionally.
 */
HeapTuple
systable_getnext(SysScanDesc sysscan)
{





































}

/*
 * systable_recheck_tuple --- recheck visibility of most-recently-fetched tuple
 *
 * In particular, determine if this tuple would be visible to a catalog scan
 * that started now.  We don't handle the case of a non-MVCC scan snapshot,
 * because no caller needs that yet.
 *
 * This is useful to test whether an object was deleted while we waited to
 * acquire lock on it.
 *
 * Note: we don't actually *need* the tuple to be passed in, but it's a
 * good crosscheck that the caller is interested in the right tuple.
 */
bool
systable_recheck_tuple(SysScanDesc sysscan, HeapTuple tup)
{
















}

/*
 * systable_endscan --- close scan, release resources
 *
 * Note that it's still up to the caller to close the heap relation.
 */
void
systable_endscan(SysScanDesc sysscan)
{






















}

/*
 * systable_beginscan_ordered --- set up for ordered catalog scan
 *
 * These routines have essentially the same API as systable_beginscan etc,
 * except that they guarantee to return multiple matching tuples in
 * index order.  Also, for largely historical reasons, the index to use
 * is opened and locked by the caller, not here.
 *
 * Currently we do not support non-index-based scans here.  (In principle
 * we could do a heapscan and sort, but the uses are in places that
 * probably don't need to still work with corrupted catalog indexes.)
 * For the moment, therefore, these functions are merely the thinnest of
 * wrappers around index_beginscan/index_getnext.  The main reason for their
 * existence is to centralize possible future support of lossy operators
 * in catalog scans.
 */
SysScanDesc
systable_beginscan_ordered(Relation heapRelation, Relation indexRelation, Snapshot snapshot, int nkeys, ScanKey key)
{

























































}

/*
 * systable_getnext_ordered --- get next tuple in an ordered catalog scan
 */
HeapTuple
systable_getnext_ordered(SysScanDesc sysscan, ScanDirection direction)
{















}

/*
 * systable_endscan_ordered --- close scan, release resources
 */
void
systable_endscan_ordered(SysScanDesc sysscan)
{













}