/*-------------------------------------------------------------------------
 *
 * expandeddatum.c
 *	  Support functions for "expanded" value representations.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/expandeddatum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/expandeddatum.h"
#include "utils/memutils.h"

/*
 * DatumGetEOHP
 *
 * Given a Datum that is an expanded-object reference, extract the pointer.
 *
 * This is a bit tedious since the pointer may not be properly aligned;
 * compare VARATT_EXTERNAL_GET_POINTER().
 */
ExpandedObjectHeader *
DatumGetEOHP(Datum d)
{







}

/*
 * EOH_init_header
 *
 * Initialize the common header of an expanded object.
 *
 * The main thing this encapsulates is initializing the TOAST pointers.
 */
void
EOH_init_header(ExpandedObjectHeader *eohptr, const ExpandedObjectMethods *methods, MemoryContext obj_context)
{













}

/*
 * EOH_get_flat_size
 * EOH_flatten_into
 *
 * Convenience functions for invoking the "methods" of an expanded object.
 */

Size
EOH_get_flat_size(ExpandedObjectHeader *eohptr)
{

}

void
EOH_flatten_into(ExpandedObjectHeader *eohptr, void *result, Size allocated_size)
{

}

/*
 * If the Datum represents a R/W expanded object, change it to R/O.
 * Otherwise return the original Datum.
 *
 * Caller must ensure that the datum is a non-null varlena value.  Typically
 * this is invoked via MakeExpandedObjectReadOnly(), which checks that.
 */
Datum
MakeExpandedObjectReadOnlyInternal(Datum d)
{













}

/*
 * Transfer ownership of an expanded object to a new parent memory context.
 * The object must be referenced by a R/W pointer, and what we return is
 * always its "standard" R/W pointer, which is certain to have the same
 * lifespan as the object itself.  (The passed-in pointer might not, and
 * in any case wouldn't provide a unique identifier if it's not that one.)
 */
Datum
TransferExpandedObject(Datum d, MemoryContext new_parent)
{










}

/*
 * Delete an expanded object (must be referenced by a R/W pointer).
 */
void
DeleteExpandedObject(Datum d)
{







}