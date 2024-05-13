/*-------------------------------------------------------------------------
 *
 * expandedrecord.c
 *	  Functions for manipulating composite expanded objects.
 *
 * This module supports "expanded objects" (cf. expandeddatum.h) that can
 * store values of named composite types, domains over named composite types,
 * and record types (registered or anonymous).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/expandedrecord.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/tuptoaster.h"
#include "catalog/heap.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/expandedrecord.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

/* "Methods" required for an expanded object */
static Size
ER_get_flat_size(ExpandedObjectHeader *eohptr);
static void
ER_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size);

static const ExpandedObjectMethods ER_methods = {ER_get_flat_size, ER_flatten_into};

/* Other local functions */
static void
ER_mc_callback(void *arg);
static MemoryContext
get_short_term_cxt(ExpandedRecordHeader *erh);
static void
build_dummy_expanded_header(ExpandedRecordHeader *main_erh);
static pg_noinline void
check_domain_for_new_field(ExpandedRecordHeader *erh, int fnumber, Datum newValue, bool isnull);
static pg_noinline void
check_domain_for_new_tuple(ExpandedRecordHeader *erh, HeapTuple tuple);

/*
 * Build an expanded record of the specified composite type
 *
 * type_id can be RECORDOID, but only if a positive typmod is given.
 *
 * The expanded record is initially "empty", having a state logically
 * equivalent to a NULL composite value (not ROW(NULL, NULL, ...)).
 * Note that this might not be a valid state for a domain type;
 * if the caller needs to check that, call
 * expanded_record_set_tuple(erh, NULL, false, false).
 *
 * The expanded object will be a child of parentcontext.
 */
ExpandedRecordHeader *
make_expanded_record_from_typeid(Oid type_id, int32 typmod, MemoryContext parentcontext)
{
















































































































}

/*
 * Build an expanded record of the rowtype defined by the tupdesc
 *
 * The tupdesc is copied if necessary (i.e., if we can't just bump its
 * reference count instead).
 *
 * The expanded record is initially "empty", having a state logically
 * equivalent to a NULL composite value (not ROW(NULL, NULL, ...)).
 *
 * The expanded object will be a child of parentcontext.
 */
ExpandedRecordHeader *
make_expanded_record_from_tupdesc(TupleDesc tupdesc, MemoryContext parentcontext)
{




































































































}

/*
 * Build an expanded record of the same rowtype as the given expanded record
 *
 * This is faster than either of the above routines because we can bypass
 * typcache lookup(s).
 *
 * The expanded record is initially "empty" --- we do not copy whatever
 * tuple might be in the source expanded record.
 *
 * The expanded object will be a child of parentcontext.
 */
ExpandedRecordHeader *
make_expanded_record_from_exprecord(ExpandedRecordHeader *olderh, MemoryContext parentcontext)
{


















































































}

/*
 * Insert given tuple as the value of the expanded record
 *
 * It is caller's responsibility that the tuple matches the record's
 * previously-assigned rowtype.  (However domain constraints, if any,
 * will be checked here.)
 *
 * The tuple is physically copied into the expanded record's local storage
 * if "copy" is true, otherwise it's caller's responsibility that the tuple
 * will live as long as the expanded record does.
 *
 * Out-of-line field values in the tuple are automatically inlined if
 * "expand_external" is true, otherwise not.  (The combination copy = false,
 * expand_external = true is not sensible and not supported.)
 *
 * Alternatively, tuple can be NULL, in which case we just set the expanded
 * record to be empty.
 */
void
expanded_record_set_tuple(ExpandedRecordHeader *erh, HeapTuple tuple, bool copy, bool expand_external)
{





































































































































}

/*
 * make_expanded_record_from_datum: build expanded record from composite Datum
 *
 * This combines the functions of make_expanded_record_from_typeid and
 * expanded_record_set_tuple.  However, we do not force a lookup of the
 * tupdesc immediately, reasoning that it might never be needed.
 *
 * The expanded object will be a child of parentcontext.
 *
 * Note: a composite datum cannot self-identify as being of a domain type,
 * so we need not consider domain cases here.
 */
Datum
make_expanded_record_from_datum(Datum recorddatum, MemoryContext parentcontext)
{


























































}

/*
 * get_flat_size method for expanded records
 *
 * Note: call this in a reasonably short-lived memory context, in case of
 * memory leaks from activities such as detoasting.
 */
static Size
ER_get_flat_size(ExpandedObjectHeader *eohptr)
{









































































































}

/*
 * flatten_into method for expanded records
 */
static void
ER_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size)
{







































}

/*
 * Look up the tupdesc for the expanded record's actual type
 *
 * Note: code internal to this module is allowed to just fetch
 * erh->er_tupdesc if ER_FLAG_DVALUES_VALID is set; otherwise it should call
 * expanded_record_get_tupdesc.  This function is the out-of-line portion
 * of expanded_record_get_tupdesc.
 */
TupleDesc
expanded_record_fetch_tupdesc(ExpandedRecordHeader *erh)
{












































}

/*
 * Get a HeapTuple representing the current value of the expanded record
 *
 * If valid, the originally stored tuple is returned, so caller must not
 * scribble on it.  Otherwise, we return a HeapTuple created in the current
 * memory context.  In either case, no attempt has been made to inline
 * out-of-line toasted values, so the tuple isn't usable as a composite
 * datum.
 *
 * Returns NULL if expanded record is empty.
 */
HeapTuple
expanded_record_get_tuple(ExpandedRecordHeader *erh)
{














}

/*
 * Memory context reset callback for cleaning up external resources
 */
static void
ER_mc_callback(void *arg)
{















}

/*
 * DatumGetExpandedRecord: get a writable expanded record from an input argument
 *
 * Caution: if the input is a read/write pointer, this returns the input
 * argument; so callers must be sure that their changes are "safe", that is
 * they cannot leave the record in a corrupt state.
 */
ExpandedRecordHeader *
DatumGetExpandedRecord(Datum d)
{












}

/*
 * Create the Datum/isnull representation of an expanded record object
 * if we didn't do so already.  After calling this, it's OK to read the
 * dvalues/dnulls arrays directly, rather than going through get_field.
 *
 * Note that if the object is currently empty ("null"), this will change
 * it to represent a row of nulls.
 */
void
deconstruct_expanded_record(ExpandedRecordHeader *erh)
{























































}

/*
 * Look up a record field by name
 *
 * If there is a field named "fieldname", fill in the contents of finfo
 * and return "true".  Else return "false" without changing *finfo.
 */
bool
expanded_record_lookup_field(ExpandedRecordHeader *erh, const char *fieldname, ExpandedRecordFieldInfo *finfo)
{

































}

/*
 * Fetch value of record field
 *
 * expanded_record_get_field is the frontend for this; it handles the
 * easy inline-able cases.
 */
Datum
expanded_record_fetch_field(ExpandedRecordHeader *erh, int fnumber, bool *isnull)
{






























}

/*
 * Set value of record field
 *
 * If the expanded record is of domain type, the assignment will be rejected
 * (without changing the record's state) if the domain's constraints would
 * be violated.
 *
 * If expand_external is true and newValue is an out-of-line value, we'll
 * forcibly detoast it so that the record does not depend on external storage.
 *
 * Internal callers can pass check_constraints = false to skip application
 * of domain constraints.  External callers should never do that.
 */
void
expanded_record_set_field_internal(ExpandedRecordHeader *erh, int fnumber, Datum newValue, bool isnull, bool expand_external, bool check_constraints)
{

































































































































}

/*
 * Set all record field(s)
 *
 * Caller must ensure that the provided datums are of the right types
 * to match the record's previously assigned rowtype.
 *
 * If expand_external is true, we'll forcibly detoast out-of-line field values
 * so that the record does not depend on external storage.
 *
 * Unlike repeated application of expanded_record_set_field(), this does not
 * guarantee to leave the expanded record in a non-corrupt state in event
 * of an error.  Typically it would only be used for initializing a new
 * expanded record.  Also, because we expect this to be applied at most once
 * in the lifespan of an expanded record, we do not worry about any cruft
 * that detoasting might leak.
 */
void
expanded_record_set_fields(ExpandedRecordHeader *erh, const Datum *newValues, const bool *isnulls, bool expand_external)
{





















































































































}

/*
 * Construct (or reset) working memory context for short-term operations.
 *
 * This context is used for domain check evaluation and for detoasting.
 *
 * If we don't have a short-lived memory context, make one; if we have one,
 * reset it to get rid of any leftover cruft.  (It is a tad annoying to need a
 * whole context for this, since it will often go unused --- but it's hard to
 * avoid memory leaks otherwise.  We can make the context small, at least.)
 */
static MemoryContext
get_short_term_cxt(ExpandedRecordHeader *erh)
{









}

/*
 * Construct "dummy header" for checking domain constraints.
 *
 * Since we don't want to modify the state of the expanded record until
 * we've validated the constraints, our approach is to set up a dummy
 * record header containing the new field value(s) and then pass that to
 * domain_check.  We retain the dummy header as part of the expanded
 * record's state to save palloc cycles, but reinitialize (most of)
 * its contents on each use.
 */
static void
build_dummy_expanded_header(ExpandedRecordHeader *main_erh)
{

















































































}

/*
 * Precheck domain constraints for a set_field operation
 */
static pg_noinline void
check_domain_for_new_field(ExpandedRecordHeader *erh, int fnumber, Datum newValue, bool isnull)
{







































































}

/*
 * Precheck domain constraints for a set_tuple operation
 */
static pg_noinline void
check_domain_for_new_tuple(ExpandedRecordHeader *erh, HeapTuple tuple)
{



















































}