/*-------------------------------------------------------------------------
 *
 * gistutil.c
 *	  utilities routines for the postgres GiST index access method.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gist/gistutil.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/gist_private.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "catalog/pg_opclass.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "utils/float.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"
#include "utils/lsyscache.h"

/*
 * Write itup vector to page, has no control of free space.
 */
void
gistfillbuffer(Page page, IndexTuple *itup, int len, OffsetNumber off)
{



















}

/*
 * Check space for itup vector on page
 */
bool
gistnospace(Page page, IndexTuple *itvec, int len, OffsetNumber todelete, Size freespace)
{
















}

bool
gistfitpage(IndexTuple *itvec, int len)
{










}

/*
 * Read buffer into itup vector
 */
IndexTuple *
gistextractpage(Page page, int *len /* out */)
{












}

/*
 * join two vectors into one
 */
IndexTuple *
gistjoinvector(IndexTuple *itvec, int *len, IndexTuple *additvec, int addlen)
{




}

/*
 * make plain IndexTupleVector
 */

IndexTupleData *
gistfillitupvec(IndexTuple *vec, int veclen, int *memlen)
{



















}

/*
 * Make unions of keys in IndexTuple vector (one union datum per index column).
 * Union Datums are returned into the attr/isnull arrays.
 * Resulting Datums aren't compressed.
 */
void
gistMakeUnionItVec(GISTSTATE *giststate, IndexTuple *itvec, int len, Datum *attr, bool *isnull)
{
















































}

/*
 * Return an IndexTuple containing the result of applying the "union"
 * method to the specified IndexTuple vector.
 */
IndexTuple
gistunion(Relation r, IndexTuple *itvec, int len, GISTSTATE *giststate)
{






}

/*
 * makes union of two key
 */
void
gistMakeUnionKey(GISTSTATE *giststate, int attno, GISTENTRY *entry1, bool isnull1, GISTENTRY *entry2, bool isnull2, Datum *dst, bool *dstisnull)
{





































}

bool
gistKeyIsEQ(GISTSTATE *giststate, int attno, Datum a, Datum b)
{




}

/*
 * Decompress all keys in tuple
 */
void
gistDeCompressAtt(GISTSTATE *giststate, Relation r, IndexTuple tuple, Page p, OffsetNumber o, GISTENTRY *attdata, bool *isnull)
{









}

/*
 * Forms union of oldtup and addtup, if union == oldtup then return NULL
 */
IndexTuple
gistgetadjusted(Relation r, IndexTuple oldtup, IndexTuple addtup, GISTSTATE *giststate)
{













































}

/*
 * Search an upper index page for the entry with lowest penalty for insertion
 * of the new index key contained in "it".
 *
 * Returns the index of the page entry to insert into.
 */
OffsetNumber
gistchoose(Relation r, Page p, IndexTuple it, /* it has compressed entry */
    GISTSTATE *giststate)
{




































































































































































}

/*
 * initialize a GiST entry with a decompressed version of key
 */
void
gistdentryinit(GISTSTATE *giststate, int nkey, GISTENTRY *e, Datum k, Relation r, Page pg, OffsetNumber o, bool l, bool isNull)
{























}

IndexTuple
gistFormTuple(GISTSTATE *giststate, Relation r, Datum attdata[], bool isnull[], bool isleaf)
{


























































}

/*
 * initialize a GiST entry with fetched value in key field
 */
static Datum
gistFetchAtt(GISTSTATE *giststate, int nkey, Datum k, Relation r)
{









}

/*
 * Fetch all keys in tuple.
 * Returns a new HeapTuple containing the originally-indexed data.
 */
HeapTuple
gistFetchTuple(GISTSTATE *giststate, Relation r, IndexTuple tuple)
{



























































}

float
gistpenalty(GISTSTATE *giststate, int attno, GISTENTRY *orig, bool isNullOrig, GISTENTRY *add, bool isNullAdd)
{






















}

/*
 * Initialize a new index page
 */
void
GISTInitBuffer(Buffer b, uint32 f)
{














}

/*
 * Verify that a freshly-read page looks sane.
 */
void
gistcheckpage(Relation rel, Buffer buf)
{




















}

/*
 * Allocate a new page (either by recycling, or by extending the index file)
 *
 * The returned buffer is already pinned and exclusive-locked
 *
 * Caller is responsible for initializing the page by calling GISTInitBuffer
 */
Buffer
gistNewBuffer(Relation r)
{













































































}

/* Can this page be recycled yet? */
bool
gistPageRecyclable(Page page)
{

























}

bytea *
gistoptions(Datum reloptions, bool validate)
{




















}

/*
 *	gistproperty() -- Check boolean properties of indexes.
 *
 * This is optional for most AMs, but is required for GiST because the core
 * property code doesn't support AMPROP_DISTANCE_ORDERABLE.  We also handle
 * AMPROP_RETURNABLE here to save opening the rel to call gistcanreturn.
 */
bool
gistproperty(Oid index_oid, int attno, IndexAMProperty prop, const char *propname, bool *res, bool *isnull)
{































































}

/*
 * Temporary and unlogged GiST indexes are not WAL-logged, but we need LSNs
 * to detect concurrent page splits anyway. This function provides a fake
 * sequence of LSNs for that purpose.
 */
XLogRecPtr
gistGetFakeLSN(Relation rel)
{



















}