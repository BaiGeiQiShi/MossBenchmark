/*-------------------------------------------------------------------------
 *
 * execTuples.c
 *	  Routines dealing with TupleTableSlots.  These are used for resource
 *	  management associated with tuples (eg, releasing buffer pins for
 *	  tuples in disk buffers, or freeing the memory occupied by transient
 *	  tuples).  Slots also provide access abstraction that lets us implement
 *	  "virtual" tuples to reduce data-copying overhead.
 *
 *	  Routines dealing with the type information for tuples. Currently,
 *	  the type information for a tuple is an array of FormData_pg_attribute.
 *	  This information is needed by routines manipulating tuples
 *	  (getattribute, formtuple, etc.).
 *
 *
 *	 EXAMPLE OF HOW TABLE ROUTINES WORK
 *		Suppose we have a query such as SELECT emp.name FROM emp and we
 *have a single SeqScan node in the query plan.
 *
 *		At ExecutorStart()
 *		----------------
 *
 *		- ExecInitSeqScan() calls ExecInitScanTupleSlot() to construct a
 *		  TupleTableSlots for the tuples returned by the access method,
 *and ExecInitResultTypeTL() to define the node's return type.
 *ExecAssignScanProjectionInfo() will, if necessary, create another
 *TupleTableSlot for the tuples resulting from performing target list
 *projections.
 *
 *		During ExecutorRun()
 *		----------------
 *		- SeqNext() calls ExecStoreBufferHeapTuple() to place the tuple
 *		  returned by the access method into the scan tuple slot.
 *
 *		- ExecSeqScan() (via ExecScan), if necessary, calls
 *ExecProject(), putting the result of the projection in the result tuple slot.
 *If not necessary, it directly returns the slot returned by SeqNext().
 *
 *		- ExecutePlan() calls the output function.
 *
 *		The important thing to watch in the executor code is how
 *pointers to the slots containing tuples are passed instead of the tuples
 *		themselves.  This facilitates the communication of related
 *information (such as whether or not a tuple should be pfreed, what buffer
 *contains this tuple, the tuple's tuple descriptor, etc).  It also allows us to
 *avoid physically constructing projection tuples in many cases.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execTuples.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupdesc_details.h"
#include "access/tuptoaster.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

static TupleDesc
ExecTypeFromTLInternal(List *targetList, bool skipjunk);
static pg_attribute_always_inline void
slot_deform_heap_tuple(TupleTableSlot *slot, HeapTuple tuple, uint32 *offp, int natts);
static inline void
tts_buffer_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, Buffer buffer, bool transfer_pin);
static void
tts_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, bool shouldFree);

const TupleTableSlotOps TTSOpsVirtual;
const TupleTableSlotOps TTSOpsHeapTuple;
const TupleTableSlotOps TTSOpsMinimalTuple;
const TupleTableSlotOps TTSOpsBufferHeapTuple;

/*
 * TupleTableSlotOps implementations.
 */

/*
 * TupleTableSlotOps implementation for VirtualTupleTableSlot.
 */
static void
tts_virtual_init(TupleTableSlot *slot)
{
}

static void
tts_virtual_release(TupleTableSlot *slot)
{
}

static void
tts_virtual_clear(TupleTableSlot *slot)
{













}

/*
 * VirtualTupleTableSlots always have fully populated tts_values and
 * tts_isnull arrays.  So this function should never be called.
 */
static void
tts_virtual_getsomeattrs(TupleTableSlot *slot, int natts)
{

}

/*
 * VirtualTupleTableSlots never provide system attributes (except those
 * handled generically, such as tableoid).  We generally shouldn't get
 * here, but provide a user-friendly message if we do.
 */
static Datum
tts_virtual_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{





}

/*
 * To materialize a virtual slot all the datums that aren't passed by value
 * have to be copied into the slot's memory context.  To do so, compute the
 * required size, and allocate enough memory to store all attributes.  That's
 * good for cache hit ratio, but more importantly requires only memory
 * allocation/deallocation.
 */
static void
tts_virtual_materialize(TupleTableSlot *slot)
{





























































































}

static void
tts_virtual_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{



















}

static HeapTuple
tts_virtual_copy_heap_tuple(TupleTableSlot *slot)
{



}

static MinimalTuple
tts_virtual_copy_minimal_tuple(TupleTableSlot *slot)
{



}

/*
 * TupleTableSlotOps implementation for HeapTupleTableSlot.
 */

static void
tts_heap_init(TupleTableSlot *slot)
{
}

static void
tts_heap_release(TupleTableSlot *slot)
{
}

static void
tts_heap_clear(TupleTableSlot *slot)
{














}

static void
tts_heap_getsomeattrs(TupleTableSlot *slot, int natts)
{





}

static Datum
tts_heap_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{














}

static void
tts_heap_materialize(TupleTableSlot *slot)
{





































}

static void
tts_heap_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{








}

static HeapTuple
tts_heap_get_heap_tuple(TupleTableSlot *slot)
{









}

static HeapTuple
tts_heap_copy_heap_tuple(TupleTableSlot *slot)
{









}

static MinimalTuple
tts_heap_copy_minimal_tuple(TupleTableSlot *slot)
{








}

static void
tts_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, bool shouldFree)
{














}

/*
 * TupleTableSlotOps implementation for MinimalTupleTableSlot.
 */

static void
tts_minimal_init(TupleTableSlot *slot)
{







}

static void
tts_minimal_release(TupleTableSlot *slot)
{
}

static void
tts_minimal_clear(TupleTableSlot *slot)
{













}

static void
tts_minimal_getsomeattrs(TupleTableSlot *slot, int natts)
{





}

static Datum
tts_minimal_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{





}

static void
tts_minimal_materialize(TupleTableSlot *slot)
{










































}

static void
tts_minimal_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{








}

static MinimalTuple
tts_minimal_get_minimal_tuple(TupleTableSlot *slot)
{








}

static HeapTuple
tts_minimal_copy_heap_tuple(TupleTableSlot *slot)
{








}

static MinimalTuple
tts_minimal_copy_minimal_tuple(TupleTableSlot *slot)
{








}

static void
tts_minimal_store_tuple(TupleTableSlot *slot, MinimalTuple mtup, bool shouldFree)
{





















}

/*
 * TupleTableSlotOps implementation for BufferHeapTupleTableSlot.
 */

static void
tts_buffer_heap_init(TupleTableSlot *slot)
{
}

static void
tts_buffer_heap_release(TupleTableSlot *slot)
{
}

static void
tts_buffer_heap_clear(TupleTableSlot *slot)
{



























}

static void
tts_buffer_heap_getsomeattrs(TupleTableSlot *slot, int natts)
{





}

static Datum
tts_buffer_heap_getsysattr(TupleTableSlot *slot, int attnum, bool *isnull)
{














}

static void
tts_buffer_heap_materialize(TupleTableSlot *slot)
{
























































}

static void
tts_buffer_heap_copyslot(TupleTableSlot *dstslot, TupleTableSlot *srcslot)
{


































}

static HeapTuple
tts_buffer_heap_get_heap_tuple(TupleTableSlot *slot)
{










}

static HeapTuple
tts_buffer_heap_copy_heap_tuple(TupleTableSlot *slot)
{










}

static MinimalTuple
tts_buffer_heap_copy_minimal_tuple(TupleTableSlot *slot)
{










}

static inline void
tts_buffer_heap_store_tuple(TupleTableSlot *slot, HeapTuple tuple, Buffer buffer, bool transfer_pin)
{


















































}

/*
 * slot_deform_heap_tuple
 *		Given a TupleTableSlot, extract data from the slot's physical
 *tuple into its Datum/isnull arrays.  Data is extracted up through the natts'th
 *column (caller must ensure this is a legal column number).
 *
 *		This is essentially an incremental version of heap_deform_tuple:
 *		on each call we extract attributes up to the one needed, without
 *		re-computing information about previously extracted attributes.
 *		slot->tts_nvalid is the number of attributes already extracted.
 *
 * This is marked as always inline, so the different offp for different types
 * of slots gets optimized away.
 */
static pg_attribute_always_inline void
slot_deform_heap_tuple(TupleTableSlot *slot, HeapTuple tuple, uint32 *offp, int natts)
{








































































































}

const TupleTableSlotOps TTSOpsVirtual = {.base_slot_size = sizeof(VirtualTupleTableSlot),
    .init = tts_virtual_init,
    .release = tts_virtual_release,
    .clear = tts_virtual_clear,
    .getsomeattrs = tts_virtual_getsomeattrs,
    .getsysattr = tts_virtual_getsysattr,
    .materialize = tts_virtual_materialize,
    .copyslot = tts_virtual_copyslot,

    /*
     * A virtual tuple table slot can not "own" a heap tuple or a minimal
     * tuple.
     */
    .get_heap_tuple = NULL,
    .get_minimal_tuple = NULL,
    .copy_heap_tuple = tts_virtual_copy_heap_tuple,
    .copy_minimal_tuple = tts_virtual_copy_minimal_tuple};

const TupleTableSlotOps TTSOpsHeapTuple = {.base_slot_size = sizeof(HeapTupleTableSlot),
    .init = tts_heap_init,
    .release = tts_heap_release,
    .clear = tts_heap_clear,
    .getsomeattrs = tts_heap_getsomeattrs,
    .getsysattr = tts_heap_getsysattr,
    .materialize = tts_heap_materialize,
    .copyslot = tts_heap_copyslot,
    .get_heap_tuple = tts_heap_get_heap_tuple,

    /* A heap tuple table slot can not "own" a minimal tuple. */
    .get_minimal_tuple = NULL,
    .copy_heap_tuple = tts_heap_copy_heap_tuple,
    .copy_minimal_tuple = tts_heap_copy_minimal_tuple};

const TupleTableSlotOps TTSOpsMinimalTuple = {.base_slot_size = sizeof(MinimalTupleTableSlot),
    .init = tts_minimal_init,
    .release = tts_minimal_release,
    .clear = tts_minimal_clear,
    .getsomeattrs = tts_minimal_getsomeattrs,
    .getsysattr = tts_minimal_getsysattr,
    .materialize = tts_minimal_materialize,
    .copyslot = tts_minimal_copyslot,

    /* A minimal tuple table slot can not "own" a heap tuple. */
    .get_heap_tuple = NULL,
    .get_minimal_tuple = tts_minimal_get_minimal_tuple,
    .copy_heap_tuple = tts_minimal_copy_heap_tuple,
    .copy_minimal_tuple = tts_minimal_copy_minimal_tuple};

const TupleTableSlotOps TTSOpsBufferHeapTuple = {.base_slot_size = sizeof(BufferHeapTupleTableSlot),
    .init = tts_buffer_heap_init,
    .release = tts_buffer_heap_release,
    .clear = tts_buffer_heap_clear,
    .getsomeattrs = tts_buffer_heap_getsomeattrs,
    .getsysattr = tts_buffer_heap_getsysattr,
    .materialize = tts_buffer_heap_materialize,
    .copyslot = tts_buffer_heap_copyslot,
    .get_heap_tuple = tts_buffer_heap_get_heap_tuple,

    /* A buffer heap tuple table slot can not "own" a minimal tuple. */
    .get_minimal_tuple = NULL,
    .copy_heap_tuple = tts_buffer_heap_copy_heap_tuple,
    .copy_minimal_tuple = tts_buffer_heap_copy_minimal_tuple};

/* ----------------------------------------------------------------
 *				  tuple table create/delete functions
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		MakeTupleTableSlot
 *
 *		Basic routine to make an empty TupleTableSlot of given
 *		TupleTableSlotType. If tupleDesc is specified the slot's
 *descriptor is fixed for its lifetime, gaining some efficiency. If that's
 *		undesirable, pass NULL.
 * --------------------------------
 */
TupleTableSlot *
MakeTupleTableSlot(TupleDesc tupleDesc, const TupleTableSlotOps *tts_ops)
{













































}

/* --------------------------------
 *		ExecAllocTableSlot
 *
 *		Create a tuple table slot within a tuple table (which is just a
 *List).
 * --------------------------------
 */
TupleTableSlot *
ExecAllocTableSlot(List **tupleTable, TupleDesc desc, const TupleTableSlotOps *tts_ops)
{





}

/* --------------------------------
 *		ExecResetTupleTable
 *
 *		This releases any resources (buffer pins, tupdesc refcounts)
 *		held by the tuple table, and optionally releases the memory
 *		occupied by the tuple table data structure.
 *		It is expected that this routine be called by EndPlan().
 * --------------------------------
 */
void
ExecResetTupleTable(List *tupleTable, /* tuple table */
    bool shouldFree)                  /* true if we should free memory */
{






































}

/* --------------------------------
 *		MakeSingleTupleTableSlot
 *
 *		This is a convenience routine for operations that need a
 *standalone TupleTableSlot not gotten from the main executor tuple table.  It
 *makes a single slot of given TupleTableSlotType and initializes it to use the
 *		given tuple descriptor.
 * --------------------------------
 */
TupleTableSlot *
MakeSingleTupleTableSlot(TupleDesc tupdesc, const TupleTableSlotOps *tts_ops)
{



}

/* --------------------------------
 *		ExecDropSingleTupleTableSlot
 *
 *		Release a TupleTableSlot made with MakeSingleTupleTableSlot.
 *		DON'T use this on a slot that's part of a tuple table list!
 * --------------------------------
 */
void
ExecDropSingleTupleTableSlot(TupleTableSlot *slot)
{




















}

/* ----------------------------------------------------------------
 *				  tuple table slot accessor functions
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *		ExecSetSlotDescriptor
 *
 *		This function is used to set the tuple descriptor associated
 *		with the slot's tuple.  The passed descriptor must have lifespan
 *		at least equal to the slot's.  If it is a reference-counted
 *descriptor then the reference count is incremented for as long as the slot
 *holds a reference.
 * --------------------------------
 */
void
ExecSetSlotDescriptor(TupleTableSlot *slot, /* slot to change */
    TupleDesc tupdesc)                      /* new tuple descriptor */
{



































}

/* --------------------------------
 *		ExecStoreHeapTuple
 *
 *		This function is used to store an on-the-fly physical tuple into
 *a specified slot in the tuple table.
 *
 *		tuple:	tuple to store
 *		slot:	TTSOpsHeapTuple type slot to store it in
 *		shouldFree: true if ExecClearTuple should pfree() the tuple
 *					when done with it
 *
 * shouldFree is normally set 'true' for tuples constructed on-the-fly.  But it
 * can be 'false' when the referenced tuple is held in a tuple table slot
 * belonging to a lower-level executor Proc node.  In this case the lower-level
 * slot retains ownership and responsibility for eventually releasing the
 * tuple.  When this method is used, we must be certain that the upper-level
 * Proc node will lose interest in the tuple sooner than the lower-level one
 * does!  If you're not certain, copy the lower-level tuple with heap_copytuple
 * and let the upper-level table slot assume ownership of the copy!
 *
 * Return value is just the passed-in slot pointer.
 *
 * If the target slot is not guaranteed to be TTSOpsHeapTuple type slot, use
 * the, more expensive, ExecForceStoreHeapTuple().
 * --------------------------------
 */
TupleTableSlot *
ExecStoreHeapTuple(HeapTuple tuple, TupleTableSlot *slot, bool shouldFree)
{
















}

/* --------------------------------
 *		ExecStoreBufferHeapTuple
 *
 *		This function is used to store an on-disk physical tuple from a
 *buffer into a specified slot in the tuple table.
 *
 *		tuple:	tuple to store
 *		slot:	TTSOpsBufferHeapTuple type slot to store it in
 *		buffer: disk buffer if tuple is in a disk page, else
 *InvalidBuffer
 *
 * The tuple table code acquires a pin on the buffer which is held until the
 * slot is cleared, so that the tuple won't go away on us.
 *
 * Return value is just the passed-in slot pointer.
 *
 * If the target slot is not guaranteed to be TTSOpsBufferHeapTuple type slot,
 * use the, more expensive, ExecForceStoreHeapTuple().
 * --------------------------------
 */
TupleTableSlot *
ExecStoreBufferHeapTuple(HeapTuple tuple, TupleTableSlot *slot, Buffer buffer)
{

















}

/*
 * Like ExecStoreBufferHeapTuple, but transfer an existing pin from the caller
 * to the slot, i.e. the caller doesn't need to, and may not, release the pin.
 */
TupleTableSlot *
ExecStorePinnedBufferHeapTuple(HeapTuple tuple, TupleTableSlot *slot, Buffer buffer)
{

















}

/*
 * Store a minimal tuple into TTSOpsMinimalTuple type slot.
 *
 * If the target slot is not guaranteed to be TTSOpsMinimalTuple type slot,
 * use the, more expensive, ExecForceStoreMinimalTuple().
 */
TupleTableSlot *
ExecStoreMinimalTuple(MinimalTuple mtup, TupleTableSlot *slot, bool shouldFree)
{














}

/*
 * Store a HeapTuple into any kind of slot, performing conversion if
 * necessary.
 */
void
ExecForceStoreHeapTuple(HeapTuple tuple, TupleTableSlot *slot, bool shouldFree)
{

































}

/*
 * Store a MinimalTuple into any kind of slot, performing conversion if
 * necessary.
 */
void
ExecForceStoreMinimalTuple(MinimalTuple mtup, TupleTableSlot *slot, bool shouldFree)
{





















}

/* --------------------------------
 *		ExecStoreVirtualTuple
 *			Mark a slot as containing a virtual tuple.
 *
 * The protocol for loading a slot with virtual tuple data is:
 *		* Call ExecClearTuple to mark the slot empty.
 *		* Store data into the Datum/isnull arrays.
 *		* Call ExecStoreVirtualTuple to mark the slot valid.
 * This is a bit unclean but it avoids one round of data copying.
 * --------------------------------
 */
TupleTableSlot *
ExecStoreVirtualTuple(TupleTableSlot *slot)
{











}

/* --------------------------------
 *		ExecStoreAllNullTuple
 *			Set up the slot to contain a null in every column.
 *
 * At first glance this might sound just like ExecClearTuple, but it's
 * entirely different: the slot ends up full, not empty.
 * --------------------------------
 */
TupleTableSlot *
ExecStoreAllNullTuple(TupleTableSlot *slot)
{
















}

/*
 * Store a HeapTuple in datum form, into a slot. That always requires
 * deforming it and storing it in virtual form.
 *
 * Until the slot is materialized, the contents of the slot depend on the
 * datum.
 */
void
ExecStoreHeapTupleDatum(Datum data, TupleTableSlot *slot)
{













}

/*
 * ExecFetchSlotHeapTuple - fetch HeapTuple representing the slot's content
 *
 * The returned HeapTuple represents the slot's content as closely as
 * possible.
 *
 * If materialize is true, the contents of the slots will be made independent
 * from the underlying storage (i.e. all buffer pins are released, memory is
 * allocated in the slot's context).
 *
 * If shouldFree is not-NULL it'll be set to true if the returned tuple has
 * been allocated in the calling memory context, and must be freed by the
 * caller (via explicit pfree() or a memory context reset).
 *
 * NB: If materialize is true, modifications of the returned tuple are
 * allowed. But it depends on the type of the slot whether such modifications
 * will also affect the slot's contents. While that is not the nicest
 * behaviour, all such modifications are in the process of being removed.
 */
HeapTuple
ExecFetchSlotHeapTuple(TupleTableSlot *slot, bool materialize, bool *shouldFree)
{




























}

/* --------------------------------
 *		ExecFetchSlotMinimalTuple
 *			Fetch the slot's minimal physical tuple.
 *
 *		If the given tuple table slot can hold a minimal tuple,
 *indicated by a non-NULL get_minimal_tuple callback, the function returns the
 *minimal tuple returned by that callback. It assumes that the minimal tuple
 *		returned by the callback is "owned" by the slot i.e. the slot is
 *		responsible for freeing the memory consumed by the tuple. Hence
 *it sets *shouldFree to false, indicating that the caller should not free the
 *		memory consumed by the minimal tuple. In this case the returned
 *minimal tuple should be considered as read-only.
 *
 *		If that callback is not supported, it calls copy_minimal_tuple
 *callback which is expected to return a copy of minimal tuple representing the
 *		contents of the slot. In this case *shouldFree is set to true,
 *		indicating the caller that it should free the memory consumed by
 *the minimal tuple. In this case the returned minimal tuple may be written up.
 * --------------------------------
 */
MinimalTuple
ExecFetchSlotMinimalTuple(TupleTableSlot *slot, bool *shouldFree)
{






















}

/* --------------------------------
 *		ExecFetchSlotHeapTupleDatum
 *			Fetch the slot's tuple as a composite-type Datum.
 *
 *		The result is always freshly palloc'd in the caller's memory
 *context.
 * --------------------------------
 */
Datum
ExecFetchSlotHeapTupleDatum(TupleTableSlot *slot)
{


















}

/* ----------------------------------------------------------------
 *				convenience initialization routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *		ExecInitResultTypeTL
 *
 *		Initialize result type, using the plan node's targetlist.
 * ----------------
 */
void
ExecInitResultTypeTL(PlanState *planstate)
{



}

/* --------------------------------
 *		ExecInit{Result,Scan,Extra}TupleSlot[TL]
 *
 *		These are convenience routines to initialize the specified slot
 *		in nodes inheriting the appropriate state.
 *ExecInitExtraTupleSlot is used for initializing special-purpose slots.
 * --------------------------------
 */

/* ----------------
 *		ExecInitResultTupleSlotTL
 *
 *		Initialize result tuple slot, using the tuple descriptor
 *previously computed with ExecInitResultTypeTL().
 * ----------------
 */
void
ExecInitResultSlot(PlanState *planstate, const TupleTableSlotOps *tts_ops)
{








}

/* ----------------
 *		ExecInitResultTupleSlotTL
 *
 *		Initialize result tuple slot, using the plan node's targetlist.
 * ----------------
 */
void
ExecInitResultTupleSlotTL(PlanState *planstate, const TupleTableSlotOps *tts_ops)
{


}

/* ----------------
 *		ExecInitScanTupleSlot
 * ----------------
 */
void
ExecInitScanTupleSlot(EState *estate, ScanState *scanstate, TupleDesc tupledesc, const TupleTableSlotOps *tts_ops)
{





}

/* ----------------
 *		ExecInitExtraTupleSlot
 *
 * Return a newly created slot. If tupledesc is non-NULL the slot will have
 * that as its fixed tupledesc. Otherwise the caller needs to use
 * ExecSetSlotDescriptor() to set the descriptor before use.
 * ----------------
 */
TupleTableSlot *
ExecInitExtraTupleSlot(EState *estate, TupleDesc tupledesc, const TupleTableSlotOps *tts_ops)
{

}

/* ----------------
 *		ExecInitNullTupleSlot
 *
 * Build a slot containing an all-nulls tuple of the given type.
 * This is used as a substitute for an input tuple when performing an
 * outer join.
 * ----------------
 */
TupleTableSlot *
ExecInitNullTupleSlot(EState *estate, TupleDesc tupType, const TupleTableSlotOps *tts_ops)
{



}

/* ---------------------------------------------------------------
 *      Routines for setting/accessing attributes in a slot.
 * ---------------------------------------------------------------
 */

/*
 * Fill in missing values for a TupleTableSlot.
 *
 * This is only exposed because it's needed for JIT compiled tuple
 * deforming. That exception aside, there should be no callers outside of this
 * file.
 */
void
slot_getmissingattrs(TupleTableSlot *slot, int startAttNum, int lastAttNum)
{
























}

/*
 * slot_getsomeattrs_int - workhorse for slot_getsomeattrs()
 */
void
slot_getsomeattrs_int(TupleTableSlot *slot, int attnum)
{





















}

/* ----------------------------------------------------------------
 *		ExecTypeFromTL
 *
 *		Generate a tuple descriptor for the result tuple of a
 *targetlist. (A parse/plan tlist must be passed, not an ExprState tlist.) Note
 *that resjunk columns, if any, are included in the result.
 *
 *		Currently there are about 4 different places where we create
 *		TupleDescriptors.  They should all be merged, or perhaps
 *		be rewritten to call BuildDesc().
 * ----------------------------------------------------------------
 */
TupleDesc
ExecTypeFromTL(List *targetList)
{

}

/* ----------------------------------------------------------------
 *		ExecCleanTypeFromTL
 *
 *		Same as above, but resjunk columns are omitted from the result.
 * ----------------------------------------------------------------
 */
TupleDesc
ExecCleanTypeFromTL(List *targetList)
{

}

static TupleDesc
ExecTypeFromTLInternal(List *targetList, bool skipjunk)
{





























}

/*
 * ExecTypeFromExprList - build a tuple descriptor from a list of Exprs
 *
 * This is roughly like ExecTypeFromTL, but we work from bare expressions
 * not TargetEntrys.  No names are attached to the tupledesc's columns.
 */
TupleDesc
ExecTypeFromExprList(List *exprList)
{
















}

/*
 * ExecTypeSetColNames - set column names in a RECORD TupleDesc
 *
 * Column names must be provided as an alias list (list of String nodes).
 */
void
ExecTypeSetColNames(TupleDesc typeInfo, List *namesList)
{
































}

/*
 * BlessTupleDesc - make a completed tuple descriptor useful for SRFs
 *
 * Rowtype Datums returned by a function must contain valid type information.
 * This happens "for free" if the tupdesc came from a relcache entry, but
 * not if we have manufactured a tupdesc for a transient RECORD datatype.
 * In that case we have to notify typcache.c of the existence of the type.
 */
TupleDesc
BlessTupleDesc(TupleDesc tupdesc)
{






}

/*
 * TupleDescGetAttInMetadata - Build an AttInMetadata structure based on the
 * supplied TupleDesc. AttInMetadata can be used in conjunction with C strings
 * to produce a properly formed tuple.
 */
AttInMetadata *
TupleDescGetAttInMetadata(TupleDesc tupdesc)
{







































}

/*
 * BuildTupleFromCStrings - build a HeapTuple given user data in C string form.
 * values is an array of C strings, one for each attribute of the return tuple.
 * A NULL string pointer indicates we want to create a NULL field.
 */
HeapTuple
BuildTupleFromCStrings(AttInMetadata *attinmeta, char **values)
{


















































}

/*
 * HeapTupleHeaderGetDatum - convert a HeapTupleHeader pointer to a Datum.
 *
 * This must *not* get applied to an on-disk tuple; the tuple should be
 * freshly made by heap_form_tuple or some wrapper routine for it (such as
 * BuildTupleFromCStrings).  Be sure also that the tupledesc used to build
 * the tuple has a properly "blessed" rowtype.
 *
 * Formerly this was a macro equivalent to PointerGetDatum, relying on the
 * fact that heap_form_tuple fills in the appropriate tuple header fields
 * for a composite Datum.  However, we now require that composite Datums not
 * contain any external TOAST pointers.  We do not want heap_form_tuple itself
 * to enforce that; more specifically, the rule applies only to actual Datums
 * and not to HeapTuple structures.  Therefore, HeapTupleHeaderGetDatum is
 * now a function that detects whether there are externally-toasted fields
 * and constructs a new tuple with inlined fields if so.  We still need
 * heap_form_tuple to insert the Datum header fields, because otherwise this
 * code would have no way to obtain a tupledesc for the tuple.
 *
 * Note that if we do build a new tuple, it's palloc'd in the current
 * memory context.  Beware of code that changes context between the initial
 * heap_form_tuple/etc call and calling HeapTuple(Header)GetDatum.
 *
 * For performance-critical callers, it could be worthwhile to take extra
 * steps to ensure that there aren't TOAST pointers in the output of
 * heap_form_tuple to begin with.  It's likely however that the costs of the
 * typcache lookup and tuple disassembly/reassembly are swamped by TOAST
 * dereference costs, so that the benefits of such extra effort would be
 * minimal.
 *
 * XXX it would likely be better to create wrapper functions that produce
 * a composite Datum from the field values in one step.  However, there's
 * enough code using the existing APIs that we couldn't get rid of this
 * hack anytime soon.
 */
Datum
HeapTupleHeaderGetDatum(HeapTupleHeader tuple)
{


















}

/*
 * Functions for sending tuples to the frontend (or other specified destination)
 * as though it is a SELECT result. These are used by utility commands that
 * need to project directly to the destination and don't need or want full
 * table function capability. Currently used by EXPLAIN and SHOW ALL.
 */
TupOutputState *
begin_tup_output_tupdesc(DestReceiver *dest, TupleDesc tupdesc, const TupleTableSlotOps *tts_ops)
{










}

/*
 * write a single tuple
 */
void
do_tup_output(TupOutputState *tstate, Datum *values, bool *isnull)
{


















}

/*
 * write a chunk of text, breaking at newline characters
 *
 * Should only be used with a single-TEXT-attribute tupdesc.
 */
void
do_text_output_multiline(TupOutputState *tstate, const char *txt)
{

























}

void
end_tup_output(TupOutputState *tstate)
{




}