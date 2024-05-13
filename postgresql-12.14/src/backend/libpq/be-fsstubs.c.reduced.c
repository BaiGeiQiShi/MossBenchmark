/*-------------------------------------------------------------------------
 *
 * be-fsstubs.c
 *	  Builtin functions for open/close/read/write operations on large
 *objects
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/libpq/be-fsstubs.c
 *
 * NOTES
 *	  This should be moved to a more appropriate place.  It is here
 *	  for lack of a better place.
 *
 *	  These functions store LargeObjectDesc structs in a private
 *MemoryContext, which means that large object descriptors hang around until we
 *destroy the context at transaction end.  It'd be possible to prolong the
 *lifetime of the context so that LO FDs are good across transactions (for
 *example, we could release the context only if we see that no FDs remain open).
 *	  But we'd need additional state in order to do the right thing at the
 *	  end of an aborted transaction.  FDs opened during an aborted xact
 *would still need to be closed, since they might not be pointing at valid
 *	  relations at all.  Locking semantics are also an interesting problem
 *	  if LOs stay open across transactions.  For now, we'll stick with the
 *	  existing documented semantics of LO FDs: they're only good within a
 *	  transaction.
 *
 *	  As of PostgreSQL 8.0, much of the angst expressed above is no longer
 *	  relevant, and in fact it'd be pretty easy to allow LO FDs to stay
 *	  open across transactions.  (Snapshot relevancy would still be an
 *issue.) However backwards compatibility suggests that we should stick to the
 *	  status quo.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xact.h"
#include "libpq/be-fsstubs.h"
#include "libpq/libpq-fs.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "storage/large_object.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

/* define this to enable debug logging */
/* #define FSDB 1 */
/* chunk size for lo_import/lo_export transfers */


/*
 * LO "FD"s are indexes into the cookies array.
 *
 * A non-null entry is a pointer to a LargeObjectDesc allocated in the
 * LO private memory context "fscxt".  The cookies array itself is also
 * dynamically allocated in that context.  Its current allocated size is
 * cookies_size entries, of which any unused entries will be NULL.
 */
static LargeObjectDesc **cookies = NULL;
static int cookies_size = 0;

static bool lo_cleanup_needed = false;
static MemoryContext fscxt = NULL;

static int
newLOfd(void);
static void
closeLOfd(int fd);
static Oid
lo_import_internal(text *filename, Oid lobjOid);

/*****************************************************************************
 *	File Interfaces for Large Objects
 *****************************************************************************/

Datum
be_lo_open(PG_FUNCTION_ARGS)
{
































}

Datum
be_lo_close(PG_FUNCTION_ARGS)
{














}

/*****************************************************************************
 *	Bare Read/Write operations --- these are not fmgr-callable!
 *
 *	We assume the large object supports byte oriented reads and seeks so
 *	that our work is easier.
 *
 *****************************************************************************/

int
lo_read(int fd, char *buf, int len)
{
























}

int
lo_write(int fd, const char *buf, int len)
{




















}

Datum
be_lo_lseek(PG_FUNCTION_ARGS)
{






















}

Datum
be_lo_lseek64(PG_FUNCTION_ARGS)
{













}

Datum
be_lo_creat(PG_FUNCTION_ARGS)
{






}

Datum
be_lo_create(PG_FUNCTION_ARGS)
{






}

Datum
be_lo_tell(PG_FUNCTION_ARGS)
{




















}

Datum
be_lo_tell64(PG_FUNCTION_ARGS)
{











}

Datum
be_lo_unlink(PG_FUNCTION_ARGS)
{































}

/*****************************************************************************
 *	Read/Write using bytea
 *****************************************************************************/

Datum
be_loread(PG_FUNCTION_ARGS)
{














}

Datum
be_lowrite(PG_FUNCTION_ARGS)
{








}

/*****************************************************************************
 *	 Import/Export of Large Object
 *****************************************************************************/

/*
 * lo_import -
 *	  imports a file as an (inversion) large object.
 */
Datum
be_lo_import(PG_FUNCTION_ARGS)
{



}

/*
 * lo_import_with_oid -
 *	  imports a file as an (inversion) large object specifying oid.
 */
Datum
be_lo_import_with_oid(PG_FUNCTION_ARGS)
{




}

static Oid
lo_import_internal(text *filename, Oid lobjOid)
{














































}

/*
 * lo_export -
 *	  exports an (inversion) large object.
 */
Datum
be_lo_export(PG_FUNCTION_ARGS)
{































































}

/*
 * lo_truncate -
 *	  truncate a large object to a specified length
 */
static void
lo_truncate_internal(int32 fd, int64 len)
{

















}

Datum
be_lo_truncate(PG_FUNCTION_ARGS)
{





}

Datum
be_lo_truncate64(PG_FUNCTION_ARGS)
{





}

/*
 * AtEOXact_LargeObject -
 *		 prepares large objects for transaction commit
 */
void
AtEOXact_LargeObject(bool isCommit)
{
  int i;

  if (!lo_cleanup_needed) {
    return; /* no LO operations in this xact */
  }
































/*
 * AtEOSubXact_LargeObject
 *		Take care of large objects at subtransaction commit/abort
 *
 * Reassign LOs created/opened during a committing subtransaction
 * to the parent subtransaction.  On abort, just close them.
 */
void
AtEOSubXact_LargeObject(bool isCommit, SubTransactionId mySubid,
                        SubTransactionId parentSubid)
{

















}

/*****************************************************************************
 *	Support routines for this file
 *****************************************************************************/

static int
newLOfd(void)
{



































}

static void
closeLOfd(int fd)
{













}

/*****************************************************************************
 *	Wrappers oriented toward SQL callers
 *****************************************************************************/

/*
 * Read [offset, offset+nbytes) within LO; when nbytes is -1, read to end.
 */
static bytea *
lo_get_fragment_internal(Oid loOid, int64 offset, int32 nbytes)
{











































}

/*
 * Read entire LO
 */
Datum
be_lo_get(PG_FUNCTION_ARGS)
{






}

/*
 * Read range within LO
 */
Datum
be_lo_get_fragment(PG_FUNCTION_ARGS)
{













}

/*
 * Create LO with initial contents given by a bytea argument
 */
Datum
be_lo_from_bytea(PG_FUNCTION_ARGS)
{













}

/*
 * Update range within LO
 */
Datum
be_lo_put(PG_FUNCTION_ARGS)
{
























}
