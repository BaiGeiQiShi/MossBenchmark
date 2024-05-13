/*-------------------------------------------------------------------------
 *
 * relmapper.c
 *	  Catalog-to-filenode mapping
 *
 * For most tables, the physical file underlying the table is specified by
 * pg_class.relfilenode.  However, that obviously won't work for pg_class
 * itself, nor for the other "nailed" catalogs for which we have to be able
 * to set up working Relation entries without access to pg_class.  It also
 * does not work for shared catalogs, since there is no practical way to
 * update other databases' pg_class entries when relocating a shared catalog.
 * Therefore, for these special catalogs (henceforth referred to as "mapped
 * catalogs") we rely on a separately maintained file that shows the mapping
 * from catalog OIDs to filenode numbers.  Each database has a map file for
 * its local mapped catalogs, and there is a separate map file for shared
 * catalogs.  Mapped catalogs have zero in their pg_class.relfilenode entries.
 *
 * Relocation of a normal table is committed (ie, the new physical file becomes
 * authoritative) when the pg_class row update commits.  For mapped catalogs,
 * the act of updating the map file is effectively commit of the relocation.
 * We postpone the file update till just before commit of the transaction
 * doing the rewrite, but there is necessarily a window between.  Therefore
 * mapped catalogs can only be relocated by operations such as VACUUM FULL
 * and CLUSTER, which make no transactionally-significant changes: it must be
 * safe for the new file to replace the old, even if the transaction itself
 * aborts.  An important factor here is that the indexes and toast table of
 * a mapped catalog must also be mapped, so that the rewrites/relocations of
 * all these files commit in a single map file update rather than being tied
 * to transaction commit.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/relmapper.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "catalog/storage.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/lwlock.h"
#include "utils/inval.h"
#include "utils/relmapper.h"

/*
 * The map file is critical data: we have no automatic method for recovering
 * from loss or corruption of it.  We use a CRC so that we can detect
 * corruption.  To minimize the risk of failed updates, the map file should
 * be kept to no more than one standard-size disk sector (ie 512 bytes),
 * and we use overwrite-in-place rather than playing renaming games.
 * The struct layout below is designed to occupy exactly 512 bytes, which
 * might make filesystem updates a bit more efficient.
 *
 * Entries in the mappings[] array are in no particular order.  We could
 * speed searching by insisting on OID order, but it really shouldn't be
 * worth the trouble given the intended size of the mapping sets.
 */
#define RELMAPPER_FILENAME "pg_filenode.map"

#define RELMAPPER_FILEMAGIC 0x592717 /* version ID value */

#define MAX_MAPPINGS 62 /* 62 * 8 + 16 = 512 */

typedef struct RelMapping
{
  Oid mapoid;      /* OID of a catalog */
  Oid mapfilenode; /* its filenode number */
} RelMapping;

typedef struct RelMapFile
{
  int32 magic;        /* always RELMAPPER_FILEMAGIC */
  int32 num_mappings; /* number of valid RelMapping entries */
  RelMapping mappings[MAX_MAPPINGS];
  pg_crc32c crc; /* CRC of all above */
  int32 pad;     /* to make the struct size be 512 exactly */
} RelMapFile;

/*
 * State for serializing local and shared relmappings for parallel workers
 * (active states only).  See notes on active_* and pending_* updates state.
 */
typedef struct SerializedActiveRelMaps
{
  RelMapFile active_shared_updates;
  RelMapFile active_local_updates;
} SerializedActiveRelMaps;

/*
 * The currently known contents of the shared map file and our database's
 * local map file are stored here.  These can be reloaded from disk
 * immediately whenever we receive an update sinval message.
 */
static RelMapFile shared_map;
static RelMapFile local_map;

/*
 * We use the same RelMapFile data structure to track uncommitted local
 * changes in the mappings (but note the magic and crc fields are not made
 * valid in these variables).  Currently, map updates are not allowed within
 * subtransactions, so one set of transaction-level changes is sufficient.
 *
 * The active_xxx variables contain updates that are valid in our transaction
 * and should be honored by RelationMapOidToFilenode.  The pending_xxx
 * variables contain updates we have been told about that aren't active yet;
 * they will become active at the next CommandCounterIncrement.  This setup
 * lets map updates act similarly to updates of pg_class rows, ie, they
 * become visible only at the next CommandCounterIncrement boundary.
 *
 * Active shared and active local updates are serialized by the parallel
 * infrastructure, and deserialized within parallel workers.
 */
static RelMapFile active_shared_updates;
static RelMapFile active_local_updates;
static RelMapFile pending_shared_updates;
static RelMapFile pending_local_updates;

/* non-export function prototypes */
static void
apply_map_update(RelMapFile *map, Oid relationId, Oid fileNode, bool add_okay);
static void
merge_map_updates(RelMapFile *map, const RelMapFile *updates, bool add_okay);
static void
load_relmap_file(bool shared, bool lock_held);
static void
write_relmap_file(bool shared, RelMapFile *newmap, bool write_wal, bool send_sinval, bool preserve_files, Oid dbid, Oid tsid, const char *dbpath);
static void
perform_relmap_update(bool shared, const RelMapFile *updates);

/*
 * RelationMapOidToFilenode
 *
 * The raison d' etre ... given a relation OID, look up its filenode.
 *
 * Although shared and local relation OIDs should never overlap, the caller
 * always knows which we need --- so pass that information to avoid useless
 * searching.
 *
 * Returns InvalidOid if the OID is not known (which should never happen,
 * but the caller is in a better position to report a meaningful error).
 */
Oid
RelationMapOidToFilenode(Oid relationId, bool shared)
{












































}

/*
 * RelationMapFilenodeToOid
 *
 * Do the reverse of the normal direction of mapping done in
 * RelationMapOidToFilenode.
 *
 * This is not supposed to be used during normal running but rather for
 * information purposes when looking at the filesystem or xlog.
 *
 * Returns InvalidOid if the OID is not known; this can easily happen if the
 * relfilenode doesn't pertain to a mapped relation.
 */
Oid
RelationMapFilenodeToOid(Oid filenode, bool shared)
{












































}

/*
 * RelationMapUpdateMap
 *
 * Install a new relfilenode mapping for the specified relation.
 *
 * If immediate is true (or we're bootstrapping), the mapping is activated
 * immediately.  Otherwise it is made pending until CommandCounterIncrement.
 */
void
RelationMapUpdateMap(Oid relationId, Oid fileNode, bool shared, bool immediate)
{



























































}

/*
 * apply_map_update
 *
 * Insert a new mapping into the given map variable, replacing any existing
 * mapping for the same relation.
 *
 * In some cases the caller knows there must be an existing mapping; pass
 * add_okay = false to draw an error if not.
 */
static void
apply_map_update(RelMapFile *map, Oid relationId, Oid fileNode, bool add_okay)
{
























}

/*
 * merge_map_updates
 *
 * Merge all the updates in the given pending-update map into the target map.
 * This is just a bulk form of apply_map_update.
 */
static void
merge_map_updates(RelMapFile *map, const RelMapFile *updates, bool add_okay)
{






}

/*
 * RelationMapRemoveMapping
 *
 * Remove a relation's entry in the map.  This is only allowed for "active"
 * (but not committed) local mappings.  We need it so we can back out the
 * entry for the transient target file when doing VACUUM FULL/CLUSTER on
 * a mapped relation.
 */
void
RelationMapRemoveMapping(Oid relationId)
{














}

/*
 * RelationMapInvalidate
 *
 * This routine is invoked for SI cache flush messages.  We must re-read
 * the indicated map file.  However, we might receive a SI message in a
 * process that hasn't yet, and might never, load the mapping files;
 * for example the autovacuum launcher, which *must not* try to read
 * a local map since it is attached to no particular database.
 * So, re-read only if the map is valid now.
 */
void
RelationMapInvalidate(bool shared)
{














}

/*
 * RelationMapInvalidateAll
 *
 * Reload all map files.  This is used to recover from SI message buffer
 * overflow: we can't be sure if we missed an inval message.
 * Again, reload only currently-valid maps.
 */
void
RelationMapInvalidateAll(void)
{








}

/*
 * AtCCI_RelationMap
 *
 * Activate any "pending" relation map updates at CommandCounterIncrement time.
 */
void
AtCCI_RelationMap(void)
{










}

/*
 * AtEOXact_RelationMap
 *
 * Handle relation mapping at main-transaction commit or abort.
 *
 * During commit, this must be called as late as possible before the actual
 * transaction commit, so as to minimize the window where the transaction
 * could still roll back after committing map changes.  Although nothing
 * critically bad happens in such a case, we still would prefer that it
 * not happen, since we'd possibly be losing useful updates to the relations'
 * pg_class row(s).
 *
 * During abort, we just have to throw away any pending map changes.
 * Normal post-abort cleanup will take care of fixing relcache entries.
 * Parallel worker commit/abort is handled by resetting active mappings
 * that may have been received from the leader process.  (There should be
 * no pending updates in parallel workers.)
 */
void
AtEOXact_RelationMap(bool isCommit, bool isParallelWorker)
{



































}

/*
 * AtPrepare_RelationMap
 *
 * Handle relation mapping at PREPARE.
 *
 * Currently, we don't support preparing any transaction that changes the map.
 */
void
AtPrepare_RelationMap(void)
{




}

/*
 * CheckPointRelationMap
 *
 * This is called during a checkpoint.  It must ensure that any relation map
 * updates that were WAL-logged before the start of the checkpoint are
 * securely flushed to disk and will not need to be replayed later.  This
 * seems unlikely to be a performance-critical issue, so we use a simple
 * method: we just take and release the RelationMappingLock.  This ensures
 * that any already-logged map update is complete, because write_relmap_file
 * will fsync the map file before the lock is released.
 */
void
CheckPointRelationMap(void)
{


}

/*
 * RelationMapFinishBootstrap
 *
 * Write out the initial relation mapping files at the completion of
 * bootstrap.  All the mapped files should have been made known to us
 * via RelationMapUpdateMap calls.
 */
void
RelationMapFinishBootstrap(void)
{











}

/*
 * RelationMapInitialize
 *
 * This initializes the mapper module at process startup.  We can't access the
 * database yet, so just make sure the maps are empty.
 */
void
RelationMapInitialize(void)
{









}

/*
 * RelationMapInitializePhase2
 *
 * This is called to prepare for access to pg_database during startup.
 * We should be able to read the shared map file now.
 */
void
RelationMapInitializePhase2(void)
{












}

/*
 * RelationMapInitializePhase3
 *
 * This is called as soon as we have determined MyDatabaseId and set up
 * DatabasePath.  At this point we should be able to read the local map file.
 */
void
RelationMapInitializePhase3(void)
{












}

/*
 * EstimateRelationMapSpace
 *
 * Estimate space needed to pass active shared and local relmaps to parallel
 * workers.
 */
Size
EstimateRelationMapSpace(void)
{

}

/*
 * SerializeRelationMap
 *
 * Serialize active shared and local relmap state for parallel workers.
 */
void
SerializeRelationMap(Size maxSize, char *startAddress)
{







}

/*
 * RestoreRelationMap
 *
 * Restore active shared and local relmap state within a parallel worker.
 */
void
RestoreRelationMap(char *startAddress)
{










}

/*
 * load_relmap_file -- load data from the shared or local map file
 *
 * Because the map file is essential for access to core system catalogs,
 * failure to read it is a fatal error.
 *
 * Note that the local case requires DatabasePath to be set up.
 */
static void
load_relmap_file(bool shared, bool lock_held)
{












































































}

/*
 * Write out a new shared or local map file with the given contents.
 *
 * The magic number and CRC are automatically updated in *newmap.  On
 * success, we copy the data to the appropriate permanent static variable.
 *
 * If write_wal is true then an appropriate WAL message is emitted.
 * (It will be false for bootstrap and WAL replay cases.)
 *
 * If send_sinval is true then a SI invalidation message is sent.
 * (This should be true except in bootstrap case.)
 *
 * If preserve_files is true then the storage manager is warned not to
 * delete the files listed in the map.
 *
 * Because this may be called during WAL replay when MyDatabaseId,
 * DatabasePath, etc aren't valid, we require the caller to pass in suitable
 * values.  The caller is also responsible for being sure no concurrent
 * map update could be happening.
 */
static void
write_relmap_file(bool shared, RelMapFile *newmap, bool write_wal, bool send_sinval, bool preserve_files, Oid dbid, Oid tsid, const char *dbpath)
{




















































































































































}

/*
 * Merge the specified updates into the appropriate "real" map,
 * and write out the changes.  This function must be used for committing
 * updates during normal multiuser operation.
 */
static void
perform_relmap_update(bool shared, const RelMapFile *updates)
{









































}

/*
 * RELMAP resource manager's routines
 */
void
relmap_redo(XLogReaderState *record)
{






































}