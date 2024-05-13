/*-------------------------------------------------------------------------
 *
 * sharedfileset.c
 *	  Shared temporary file management.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/file/sharedfileset.c
 *
 * SharedFileSets provide a temporary namespace (think directory) so that
 * files can be discovered by name, and a shared ownership semantics so that
 * shared files survive until the last user detaches.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_tablespace.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "storage/dsm.h"
#include "storage/sharedfileset.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

static void
SharedFileSetOnDetach(dsm_segment *segment, Datum datum);
static void
SharedFileSetPath(char *path, SharedFileSet *fileset, Oid tablespace);
static void
SharedFilePath(char *path, SharedFileSet *fileset, const char *name);
static Oid
ChooseTablespace(const SharedFileSet *fileset, const char *name);

/*
 * Initialize a space for temporary files that can be opened for read-only
 * access by other backends.  Other backends must attach to it before
 * accessing it.  Associate this SharedFileSet with 'seg'.  Any contained
 * files will be deleted when the last backend detaches.
 *
 * Files will be distributed over the tablespaces configured in
 * temp_tablespaces.
 *
 * Under the covers the set is one or more directories which will eventually
 * be deleted when there are no backends attached.
 */
void
SharedFileSetInit(SharedFileSet *fileset, dsm_segment *seg)
{





































}

/*
 * Attach to a set of directories that was created with SharedFileSetInit.
 */
void
SharedFileSetAttach(SharedFileSet *fileset, dsm_segment *seg)
{





















}

/*
 * Create a new file in the given set.
 */
File
SharedFileSetCreate(SharedFileSet *fileset, const char *name)
{




















}

/*
 * Open a file that was created with SharedFileSetCreate(), possibly in
 * another backend.
 */
File
SharedFileSetOpen(SharedFileSet *fileset, const char *name)
{







}

/*
 * Delete a file that was created with SharedFileSetCreate().
 * Return true if the file existed, false if didn't.
 */
bool
SharedFileSetDelete(SharedFileSet *fileset, const char *name, bool error_on_failure)
{





}

/*
 * Delete all files in the set.
 */
void
SharedFileSetDeleteAll(SharedFileSet *fileset)
{













}

/*
 * Callback function that will be invoked when this backend detaches from a
 * DSM segment holding a SharedFileSet that it has created or attached to.  If
 * we are the last to detach, then try to remove the directories and
 * everything in them.  We can't raise an error on failures, because this runs
 * in error cleanup paths.
 */
static void
SharedFileSetOnDetach(dsm_segment *segment, Datum datum)
{




















}

/*
 * Build the path for the directory holding the files backing a SharedFileSet
 * in a given tablespace.
 */
static void
SharedFileSetPath(char *path, SharedFileSet *fileset, Oid tablespace)
{




}

/*
 * Sorting hat to determine which tablespace a given shared temporary file
 * belongs in.
 */
static Oid
ChooseTablespace(const SharedFileSet *fileset, const char *name)
{



}

/*
 * Compute the full path of a file in a SharedFileSet.
 */
static void
SharedFilePath(char *path, SharedFileSet *fileset, const char *name)
{




}