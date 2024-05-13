/*-------------------------------------------------------------------------
 *
 * inv_api.c
 *	  routines for manipulating inversion fs large objects. This file
 *	  contains the user-level large object application interface routines.
 *
 *
 * Note: we access pg_largeobject.data using its C struct declaration.
 * This is safe because it immediately follows pageno which is an int4 field,
 * and therefore the data field will always be 4-byte aligned, even if it
 * is in the short 1-byte-header format.  We have to detoast it since it's
 * quite likely to be in compressed or short format.  We also need to check
 * for NULLs, since initdb will mark loid and pageno but not data as NOT NULL.
 *
 * Note: many of these routines leak memory in CurrentMemoryContext, as indeed
 * does most of the backend code.  We expect that CurrentMemoryContext will
 * be a short-lived context.  Data that must persist across function calls
 * is kept either in CacheMemoryContext (the Relation structs) or in the
 * memory context given to inv_open (for LargeObjectDesc structs).
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/large_object/inv_api.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/genam.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "libpq/libpq-fs.h"
#include "miscadmin.h"
#include "storage/large_object.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

/*
 * GUC: backwards-compatibility flag to suppress LO permission checks
 */
bool lo_compat_privileges;

/*
 * All accesses to pg_largeobject and its index make use of a single Relation
 * reference, so that we only need to open pg_relation once per transaction.
 * To avoid problems when the first such reference occurs inside a
 * subtransaction, we execute a slightly klugy maneuver to assign ownership of
 * the Relation reference to TopTransactionResourceOwner.
 */
static Relation lo_heap_r = NULL;
static Relation lo_index_r = NULL;

/*
 * Open pg_largeobject and its index, if not already done in current xact
 */
static void
open_lo_relation(void)
{






















}

/*
 * Clean up at main transaction end
 */
void
close_lo_relation(bool isCommit)
{



























}

/*
 * Same as pg_largeobject.c's LargeObjectExists(), except snapshot to
 * read with can be specified.
 */
static bool
myLargeObjectExists(Oid loid, Snapshot snapshot)
{























}

/*
 * Extract data field from a pg_largeobject tuple, detoasting if needed
 * and verifying that the length is sane.  Returns data pointer (a bytea *),
 * data length, and an indication of whether to pfree the data pointer.
 */
static void
getdatafield(Form_pg_largeobject tuple, bytea **pdatafield, int *plen, bool *pfreeit)
{



















}

/*
 *	inv_create -- create a new large object
 *
 *	Arguments:
 *	  lobjId - OID to use for new large object, or InvalidOid to pick one
 *
 *	Returns:
 *	  OID of new object
 *
 * If lobjId is not InvalidOid, then an error occurs if the OID is already
 * in use.
 */
Oid
inv_create(Oid lobjId)
{



























}

/*
 *	inv_open -- access an existing large object.
 *
 * Returns a large object descriptor, appropriately filled in.
 * The descriptor and subsidiary data are allocated in the specified
 * memory context, which must be suitably long-lived for the caller's
 * purposes.  If the returned descriptor has a snapshot associated
 * with it, the caller must ensure that it also lives long enough,
 * e.g. by calling RegisterSnapshotOnOwner
 */
LargeObjectDesc *
inv_open(Oid lobjId, int flags, MemoryContext mcxt)
{







































































}

/*
 * Closes a large object descriptor previously made by inv_open(), and
 * releases the long-term memory used by it.
 */
void
inv_close(LargeObjectDesc *obj_desc)
{


}

/*
 * Destroys an existing large object (not to be confused with a descriptor!)
 *
 * Note we expect caller to have done any required permissions check.
 */
int
inv_drop(Oid lobjId)
{


















}

/*
 * Determine size of a large object
 *
 * NOTE: LOs can contain gaps, just like Unix files.  We actually return
 * the offset of the last byte + 1.
 */
static uint64
inv_getsize(LargeObjectDesc *obj_desc)
{











































}

int64
inv_seek(LargeObjectDesc *obj_desc, int64 offset, int whence)
{









































}

int64
inv_tell(LargeObjectDesc *obj_desc)
{








}

int
inv_read(LargeObjectDesc *obj_desc, char *buf, int nbytes)
{
























































































}

int
inv_write(LargeObjectDesc *obj_desc, const char *buf, int nbytes)
{































































































































































































}

void
inv_truncate(LargeObjectDesc *obj_desc, int64 len)
{










































































































































































}