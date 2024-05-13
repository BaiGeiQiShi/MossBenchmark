/*-------------------------------------------------------------------------
 *
 * reinit.c
 *	  Reinitialization of unlogged relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/file/reinit.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>

#include "common/relpath.h"
#include "storage/copydir.h"
#include "storage/fd.h"
#include "storage/reinit.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

static void
ResetUnloggedRelationsInTablespaceDir(const char *tsdirname, int op);
static void
ResetUnloggedRelationsInDbspaceDir(const char *dbspacedirname, int op);

typedef struct
{
  char oid[OIDCHARS + 1];
} unlogged_relation_entry;

/*
 * Reset unlogged relations from before the last restart.
 *
 * If op includes UNLOGGED_RELATION_CLEANUP, we remove all forks of any
 * relation with an "init" fork, except for the "init" fork itself.
 *
 * If op includes UNLOGGED_RELATION_INIT, we copy the "init" fork to the main
 * fork.
 */
void
ResetUnloggedRelations(int op)
{











































}

/*
 * Process one tablespace directory for ResetUnloggedRelations
 */
static void
ResetUnloggedRelationsInTablespaceDir(const char *tsdirname, int op)
{





































}

/*
 * Process one per-dbspace directory for ResetUnloggedRelations
 */
static void
ResetUnloggedRelationsInDbspaceDir(const char *dbspacedirname, int op)
{























































































































































































































}

/*
 * Basic parsing of putative relation filenames.
 *
 * This function returns true if the file appears to be in the correct format
 * for a non-temporary relation and false otherwise.
 *
 * NB: If this function returns true, the caller is entitled to assume that
 * *oidchars has been set to the a value no more than OIDCHARS, and thus
 * that a buffer of OIDCHARS+1 characters is sufficient to hold the OID
 * portion of the filename.  This is critical to protect against a possible
 * buffer overrun.
 */
bool
parse_filename_for_nontemp_relation(const char *name, int *oidchars, ForkNumber *fork)
{
















































}
