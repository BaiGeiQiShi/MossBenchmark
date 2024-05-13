/*-------------------------------------------------------------------------
 *
 * tupdesc.c
 *	  POSTGRES tuple descriptor support code
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/common/tupdesc.c
 *
 * NOTES
 *	  some of the executor utility code such as "ExecTypeFromTL" should be
 *	  moved here.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupdesc_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/hashutils.h"
#include "utils/resowner_private.h"
#include "utils/syscache.h"

/*
 * CreateTemplateTupleDesc
 *		This function allocates an empty tuple descriptor structure.
 *
 * Tuple type ID information is initially set for an anonymous record type;
 * caller can overwrite this if needed.
 */
TupleDesc
CreateTemplateTupleDesc(int natts)
{































}

/*
 * CreateTupleDesc
 *		This function allocates a new TupleDesc by copying a given
 *		Form_pg_attribute array.
 *
 * Tuple type ID information is initially set for an anonymous record type;
 * caller can overwrite this if needed.
 */
TupleDesc
CreateTupleDesc(int natts, Form_pg_attribute *attrs)
{











}

/*
 * CreateTupleDescCopy
 *		This function creates a new TupleDesc by copying from an
 *existing TupleDesc.
 *
 * !!! Constraints and defaults are not copied !!!
 */
TupleDesc
CreateTupleDescCopy(TupleDesc tupdesc)
{




























}

/*
 * CreateTupleDescCopyConstr
 *		This function creates a new TupleDesc by copying from an
 *existing TupleDesc (including its constraints and defaults).
 */
TupleDesc
CreateTupleDescCopyConstr(TupleDesc tupdesc)
{








































































}

/*
 * TupleDescCopy
 *		Copy a tuple descriptor into caller-supplied memory.
 *		The memory may be shared memory mapped at any address, and must
 *		be sufficient to hold TupleDescSize(src) bytes.
 *
 * !!! Constraints and defaults are not copied !!!
 */
void
TupleDescCopy(TupleDesc dst, TupleDesc src)
{


























}

/*
 * TupleDescCopyEntry
 *		This function copies a single attribute structure from one tuple
 *		descriptor to another.
 *
 * !!! Constraints and defaults are not copied !!!
 */
void
TupleDescCopyEntry(TupleDesc dst, AttrNumber dstAttno, TupleDesc src, AttrNumber srcAttno)
{

































}

/*
 * Free a TupleDesc including all substructure
 */
void
FreeTupleDesc(TupleDesc tupdesc)
{

























































}

/*
 * Increment the reference count of a tupdesc, and log the reference in
 * CurrentResourceOwner.
 *
 * Do not apply this to tupdescs that are not being refcounted.  (Use the
 * macro PinTupleDesc for tupdescs of uncertain status.)
 */
void
IncrTupleDescRefCount(TupleDesc tupdesc)
{





}

/*
 * Decrement the reference count of a tupdesc, remove the corresponding
 * reference from CurrentResourceOwner, and free the tupdesc if no more
 * references remain.
 *
 * Do not apply this to tupdescs that are not being refcounted.  (Use the
 * macro ReleaseTupleDesc for tupdescs of uncertain status.)
 */
void
DecrTupleDescRefCount(TupleDesc tupdesc)
{







}

/*
 * Compare two TupleDesc structures for logical equality
 *
 * Note: we deliberately do not check the attrelid and tdtypmod fields.
 * This allows typcache.c to use this routine to see if a cached record type
 * matches a requested type, and is harmless for relcache.c's uses.
 * We don't compare tdrefcount, either.
 */
bool
equalTupleDescs(TupleDesc tupdesc1, TupleDesc tupdesc2)
{


















































































































































































































}

/*
 * hashTupleDesc
 *		Compute a hash value for a tuple descriptor.
 *
 * If two tuple descriptors would be considered equal by equalTupleDescs()
 * then their hash value will be equal according to this function.
 *
 * Note that currently contents of constraint are not hashed - it'd be a bit
 * painful to do so, and conflicts just due to constraints are unlikely.
 */
uint32
hashTupleDesc(TupleDesc desc)
{











}

/*
 * TupleDescInitEntry
 *		This function initializes a single attribute structure in
 *		a previously allocated tuple descriptor.
 *
 * If attributeName is NULL, the attname field is set to an empty string
 * (this is for cases where we don't know or need a name for the field).
 * Also, some callers use this function to change the datatype-related fields
 * in an existing tupdesc; they pass attributeName = NameStr(att->attname)
 * to indicate that the attname field shouldn't be modified.
 *
 * Note that attcollation is set to the default for the specified datatype.
 * If a nondefault collation is needed, insert it afterwards using
 * TupleDescInitEntryCollation.
 */
void
TupleDescInitEntry(TupleDesc desc, AttrNumber attributeNumber, const char *attributeName, Oid oidtypeid, int32 typmod, int attdim)
{
































































}

/*
 * TupleDescInitBuiltinEntry
 *		Initialize a tuple descriptor without catalog access.  Only
 *		a limited range of builtin types are supported.
 */
void
TupleDescInitBuiltinEntry(TupleDesc desc, AttrNumber attributeNumber, const char *attributeName, Oid oidtypeid, int32 typmod, int attdim)
{













































































}

/*
 * TupleDescInitEntryCollation
 *
 * Assign a nondefault collation to a previously initialized tuple descriptor
 * entry.
 */
void
TupleDescInitEntryCollation(TupleDesc desc, AttrNumber attributeNumber, Oid collationid)
{








}

/*
 * BuildDescForRelation
 *
 * Given a relation schema (list of ColumnDef nodes), build a TupleDesc.
 *
 * Note: the default assumption is no OIDs; caller may modify the returned
 * TupleDesc if it wants OIDs.  Also, tdtypeid will need to be filled in
 * later on.
 */
TupleDesc
BuildDescForRelation(List *schema)
{






















































































}

/*
 * BuildDescFromLists
 *
 * Build a TupleDesc given lists of column names (as String nodes),
 * column type OIDs, typmods, and collation OIDs.
 *
 * No constraints are generated.
 *
 * This is essentially a cut-down version of BuildDescForRelation for use
 * with functions returning RECORD.
 */
TupleDesc
BuildDescFromLists(List *names, List *types, List *typmods, List *collations)
{

































}