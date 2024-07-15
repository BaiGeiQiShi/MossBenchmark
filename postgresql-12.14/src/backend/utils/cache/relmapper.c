                                                                            
   
               
                                 
   
                                                                           
                                                                          
                                                                            
                                                                           
                                                                         
                                                                              
                                                                            
                                                                             
                                                                            
                                                                          
                                                                               
   
                                                                                
                                                                              
                                                                             
                                                                          
                                                                            
                                                                           
                                                                              
                                                                            
                                                                            
                                                                             
                                                                             
                          
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
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

   
                                                                             
                                                                      
                                                                            
                                                                         
                                                                     
                                                                          
                                                       
   
                                                                         
                                                                         
                                                                  
   
#define RELMAPPER_FILENAME "pg_filenode.map"

#define RELMAPPER_FILEMAGIC 0x592717                       

#define MAX_MAPPINGS 62                        

typedef struct RelMapping
{
  Oid mapoid;                            
  Oid mapfilenode;                          
} RelMapping;

typedef struct RelMapFile
{
  int32 magic;                                        
  int32 num_mappings;                                         
  RelMapping mappings[MAX_MAPPINGS];
  pg_crc32c crc;                       
  int32 pad;                                                 
} RelMapFile;

   
                                                                           
                                                                             
   
typedef struct SerializedActiveRelMaps
{
  RelMapFile active_shared_updates;
  RelMapFile active_local_updates;
} SerializedActiveRelMaps;

   
                                                                          
                                                                    
                                                             
   
static RelMapFile shared_map;
static RelMapFile local_map;

   
                                                                        
                                                                           
                                                                             
                                                                           
   
                                                                              
                                                                       
                                                                             
                                                                            
                                                                        
                                                                     
   
                                                                         
                                                             
   
static RelMapFile active_shared_updates;
static RelMapFile active_local_updates;
static RelMapFile pending_shared_updates;
static RelMapFile pending_local_updates;

                                    
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

   
                            
   
                                                                      
   
                                                                            
                                                                            
              
   
                                                                          
                                                                         
   
Oid
RelationMapOidToFilenode(Oid relationId, bool shared)
{
  const RelMapFile *map;
  int32 i;

                                                                     
  if (shared)
  {
    map = &active_shared_updates;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (relationId == map->mappings[i].mapoid)
      {
        return map->mappings[i].mapfilenode;
      }
    }
    map = &shared_map;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (relationId == map->mappings[i].mapoid)
      {
        return map->mappings[i].mapfilenode;
      }
    }
  }
  else
  {
    map = &active_local_updates;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (relationId == map->mappings[i].mapoid)
      {
        return map->mappings[i].mapfilenode;
      }
    }
    map = &local_map;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (relationId == map->mappings[i].mapoid)
      {
        return map->mappings[i].mapfilenode;
      }
    }
  }

  return InvalidOid;
}

   
                            
   
                                                             
                             
   
                                                                        
                                                                
   
                                                                             
                                                     
   
Oid
RelationMapFilenodeToOid(Oid filenode, bool shared)
{
  const RelMapFile *map;
  int32 i;

                                                                     
  if (shared)
  {
    map = &active_shared_updates;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (filenode == map->mappings[i].mapfilenode)
      {
        return map->mappings[i].mapoid;
      }
    }
    map = &shared_map;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (filenode == map->mappings[i].mapfilenode)
      {
        return map->mappings[i].mapoid;
      }
    }
  }
  else
  {
    map = &active_local_updates;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (filenode == map->mappings[i].mapfilenode)
      {
        return map->mappings[i].mapoid;
      }
    }
    map = &local_map;
    for (i = 0; i < map->num_mappings; i++)
    {
      if (filenode == map->mappings[i].mapfilenode)
      {
        return map->mappings[i].mapoid;
      }
    }
  }

  return InvalidOid;
}

   
                        
   
                                                                 
   
                                                                           
                                                                             
   
void
RelationMapUpdateMap(Oid relationId, Oid fileNode, bool shared, bool immediate)
{
  RelMapFile *map;

  if (IsBootstrapProcessingMode())
  {
       
                                                                       
       
    if (shared)
    {
      map = &shared_map;
    }
    else
    {
      map = &local_map;
    }
  }
  else
  {
       
                                                                         
                                                                        
                                                               
       
    if (GetCurrentTransactionNestLevel() > 1)
    {
      elog(ERROR, "cannot change relation mapping within subtransaction");
    }

    if (IsInParallelMode())
    {
      elog(ERROR, "cannot change relation mapping in parallel mode");
    }

    if (immediate)
    {
                                            
      if (shared)
      {
        map = &active_shared_updates;
      }
      else
      {
        map = &active_local_updates;
      }
    }
    else
    {
                           
      if (shared)
      {
        map = &pending_shared_updates;
      }
      else
      {
        map = &pending_local_updates;
      }
    }
  }
  apply_map_update(map, relationId, fileNode, true);
}

   
                    
   
                                                                            
                                  
   
                                                                          
                                             
   
static void
apply_map_update(RelMapFile *map, Oid relationId, Oid fileNode, bool add_okay)
{
  int32 i;

                                    
  for (i = 0; i < map->num_mappings; i++)
  {
    if (relationId == map->mappings[i].mapoid)
    {
      map->mappings[i].mapfilenode = fileNode;
      return;
    }
  }

                                       
  if (!add_okay)
  {
    elog(ERROR, "attempt to apply a mapping to unmapped relation %u", relationId);
  }
  if (map->num_mappings >= MAX_MAPPINGS)
  {
    elog(ERROR, "ran out of space in relation map");
  }
  map->mappings[map->num_mappings].mapoid = relationId;
  map->mappings[map->num_mappings].mapfilenode = fileNode;
  map->num_mappings++;
}

   
                     
   
                                                                              
                                                 
   
static void
merge_map_updates(RelMapFile *map, const RelMapFile *updates, bool add_okay)
{
  int32 i;

  for (i = 0; i < updates->num_mappings; i++)
  {
    apply_map_update(map, updates->mappings[i].mapoid, updates->mappings[i].mapfilenode, add_okay);
  }
}

   
                            
   
                                                                            
                                                                          
                                                                         
                      
   
void
RelationMapRemoveMapping(Oid relationId)
{
  RelMapFile *map = &active_local_updates;
  int32 i;

  for (i = 0; i < map->num_mappings; i++)
  {
    if (relationId == map->mappings[i].mapoid)
    {
                                     
      map->mappings[i] = map->mappings[map->num_mappings - 1];
      map->num_mappings--;
      return;
    }
  }
  elog(ERROR, "could not find temporary mapping for relation %u", relationId);
}

   
                         
   
                                                                         
                                                                        
                                                                     
                                                                     
                                                               
                                             
   
void
RelationMapInvalidate(bool shared)
{
  if (shared)
  {
    if (shared_map.magic == RELMAPPER_FILEMAGIC)
    {
      load_relmap_file(true, false);
    }
  }
  else
  {
    if (local_map.magic == RELMAPPER_FILEMAGIC)
    {
      load_relmap_file(false, false);
    }
  }
}

   
                            
   
                                                                         
                                                             
                                            
   
void
RelationMapInvalidateAll(void)
{
  if (shared_map.magic == RELMAPPER_FILEMAGIC)
  {
    load_relmap_file(true, false);
  }
  if (local_map.magic == RELMAPPER_FILEMAGIC)
  {
    load_relmap_file(false, false);
  }
}

   
                     
   
                                                                                
   
void
AtCCI_RelationMap(void)
{
  if (pending_shared_updates.num_mappings != 0)
  {
    merge_map_updates(&active_shared_updates, &pending_shared_updates, true);
    pending_shared_updates.num_mappings = 0;
  }
  if (pending_local_updates.num_mappings != 0)
  {
    merge_map_updates(&active_local_updates, &pending_local_updates, true);
    pending_local_updates.num_mappings = 0;
  }
}

   
                        
   
                                                                
   
                                                                            
                                                                          
                                                                         
                                                                        
                                                                              
                    
   
                                                                     
                                                                        
                                                                        
                                                                          
                                            
   
void
AtEOXact_RelationMap(bool isCommit, bool isParallelWorker)
{
  if (isCommit && !isParallelWorker)
  {
       
                                                                     
                                                                       
                                       
       
    Assert(pending_shared_updates.num_mappings == 0);
    Assert(pending_local_updates.num_mappings == 0);

       
                                                                          
       
    if (active_shared_updates.num_mappings != 0)
    {
      perform_relmap_update(true, &active_shared_updates);
      active_shared_updates.num_mappings = 0;
    }
    if (active_local_updates.num_mappings != 0)
    {
      perform_relmap_update(false, &active_local_updates);
      active_local_updates.num_mappings = 0;
    }
  }
  else
  {
                                                                         
    Assert(!isParallelWorker || pending_shared_updates.num_mappings == 0);
    Assert(!isParallelWorker || pending_local_updates.num_mappings == 0);

    active_shared_updates.num_mappings = 0;
    active_local_updates.num_mappings = 0;
    pending_shared_updates.num_mappings = 0;
    pending_local_updates.num_mappings = 0;
  }
}

   
                         
   
                                       
   
                                                                               
   
void
AtPrepare_RelationMap(void)
{
  if (active_shared_updates.num_mappings != 0 || active_local_updates.num_mappings != 0 || pending_shared_updates.num_mappings != 0 || pending_local_updates.num_mappings != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE a transaction that modified relation mapping")));
  }
}

   
                         
   
                                                                             
                                                                       
                                                                          
                                                                         
                                                                           
                                                                             
                                                        
   
void
CheckPointRelationMap(void)
{
  LWLockAcquire(RelationMappingLock, LW_SHARED);
  LWLockRelease(RelationMappingLock);
}

   
                              
   
                                                                     
                                                                      
                                   
   
void
RelationMapFinishBootstrap(void)
{
  Assert(IsBootstrapProcessingMode());

                                           
  Assert(active_shared_updates.num_mappings == 0);
  Assert(active_local_updates.num_mappings == 0);
  Assert(pending_shared_updates.num_mappings == 0);
  Assert(pending_local_updates.num_mappings == 0);

                                                
  write_relmap_file(true, &shared_map, false, false, false, InvalidOid, GLOBALTABLESPACE_OID, NULL);
  write_relmap_file(false, &local_map, false, false, false, MyDatabaseId, MyDatabaseTableSpace, DatabasePath);
}

   
                         
   
                                                                               
                                                       
   
void
RelationMapInitialize(void)
{
                                                                           
  shared_map.magic = 0;                         
  local_map.magic = 0;
  shared_map.num_mappings = 0;
  local_map.num_mappings = 0;
  active_shared_updates.num_mappings = 0;
  active_local_updates.num_mappings = 0;
  pending_shared_updates.num_mappings = 0;
  pending_local_updates.num_mappings = 0;
}

   
                               
   
                                                                       
                                                      
   
void
RelationMapInitializePhase2(void)
{
     
                                                                     
     
  if (IsBootstrapProcessingMode())
  {
    return;
  }

     
                                             
     
  load_relmap_file(true, false);
}

   
                               
   
                                                                        
                                                                              
   
void
RelationMapInitializePhase3(void)
{
     
                                                                     
     
  if (IsBootstrapProcessingMode())
  {
    return;
  }

     
                                            
     
  load_relmap_file(false, false);
}

   
                            
   
                                                                             
            
   
Size
EstimateRelationMapSpace(void)
{
  return sizeof(SerializedActiveRelMaps);
}

   
                        
   
                                                                        
   
void
SerializeRelationMap(Size maxSize, char *startAddress)
{
  SerializedActiveRelMaps *relmaps;

  Assert(maxSize >= EstimateRelationMapSpace());

  relmaps = (SerializedActiveRelMaps *)startAddress;
  relmaps->active_shared_updates = active_shared_updates;
  relmaps->active_local_updates = active_local_updates;
}

   
                      
   
                                                                          
   
void
RestoreRelationMap(char *startAddress)
{
  SerializedActiveRelMaps *relmaps;

  if (active_shared_updates.num_mappings != 0 || active_local_updates.num_mappings != 0 || pending_shared_updates.num_mappings != 0 || pending_local_updates.num_mappings != 0)
  {
    elog(ERROR, "parallel worker has existing mappings");
  }

  relmaps = (SerializedActiveRelMaps *)startAddress;
  active_shared_updates = relmaps->active_shared_updates;
  active_local_updates = relmaps->active_local_updates;
}

   
                                                                   
   
                                                                         
                                        
   
                                                                
   
static void
load_relmap_file(bool shared, bool lock_held)
{
  RelMapFile *map;
  char mapfilename[MAXPGPATH];
  pg_crc32c crc;
  int fd;
  int r;

  if (shared)
  {
    snprintf(mapfilename, sizeof(mapfilename), "global/%s", RELMAPPER_FILENAME);
    map = &shared_map;
  }
  else
  {
    snprintf(mapfilename, sizeof(mapfilename), "%s/%s", DatabasePath, RELMAPPER_FILENAME);
    map = &local_map;
  }

                     
  fd = OpenTransientFile(mapfilename, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", mapfilename)));
  }

     
                                                                            
                                                                            
                                                                        
                                                                             
                 
     
  if (!lock_held)
  {
    LWLockAcquire(RelationMappingLock, LW_SHARED);
  }

  pgstat_report_wait_start(WAIT_EVENT_RELATION_MAP_READ);
  r = read(fd, map, sizeof(RelMapFile));
  if (r != sizeof(RelMapFile))
  {
    if (r < 0)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", mapfilename)));
    }
    else
    {
      ereport(FATAL, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", mapfilename, r, sizeof(RelMapFile))));
    }
  }
  pgstat_report_wait_end();

  if (!lock_held)
  {
    LWLockRelease(RelationMappingLock);
  }

  if (CloseTransientFile(fd))
  {
    ereport(FATAL, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", mapfilename)));
  }

                                           
  if (map->magic != RELMAPPER_FILEMAGIC || map->num_mappings < 0 || map->num_mappings > MAX_MAPPINGS)
  {
    ereport(FATAL, (errmsg("relation mapping file \"%s\" contains invalid data", mapfilename)));
  }

                      
  INIT_CRC32C(crc);
  COMP_CRC32C(crc, (char *)map, offsetof(RelMapFile, crc));
  FIN_CRC32C(crc);

  if (!EQ_CRC32C(crc, map->crc))
  {
    ereport(FATAL, (errmsg("relation mapping file \"%s\" contains incorrect checksum", mapfilename)));
  }
}

   
                                                                     
   
                                                                      
                                                                           
   
                                                                    
                                                          
   
                                                                  
                                                   
   
                                                                       
                                       
   
                                                                   
                                                                             
                                                                        
                                  
   
static void
write_relmap_file(bool shared, RelMapFile *newmap, bool write_wal, bool send_sinval, bool preserve_files, Oid dbid, Oid tsid, const char *dbpath)
{
  int fd;
  RelMapFile *realmap;
  char mapfilename[MAXPGPATH];

     
                                                 
     
  newmap->magic = RELMAPPER_FILEMAGIC;
  if (newmap->num_mappings < 0 || newmap->num_mappings > MAX_MAPPINGS)
  {
    elog(ERROR, "attempt to write bogus relation mapping");
  }

  INIT_CRC32C(newmap->crc);
  COMP_CRC32C(newmap->crc, (char *)newmap, offsetof(RelMapFile, crc));
  FIN_CRC32C(newmap->crc);

     
                                                                     
                                                                       
     
  if (shared)
  {
    snprintf(mapfilename, sizeof(mapfilename), "global/%s", RELMAPPER_FILENAME);
    realmap = &shared_map;
  }
  else
  {
    snprintf(mapfilename, sizeof(mapfilename), "%s/%s", dbpath, RELMAPPER_FILENAME);
    realmap = &local_map;
  }

  fd = OpenTransientFile(mapfilename, O_WRONLY | O_CREAT | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", mapfilename)));
  }

  if (write_wal)
  {
    xl_relmap_update xlrec;
    XLogRecPtr lsn;

                                  
    START_CRIT_SECTION();

    xlrec.dbid = dbid;
    xlrec.tsid = tsid;
    xlrec.nbytes = sizeof(RelMapFile);

    XLogBeginInsert();
    XLogRegisterData((char *)(&xlrec), MinSizeOfRelmapUpdate);
    XLogRegisterData((char *)newmap, sizeof(RelMapFile));

    lsn = XLogInsert(RM_RELMAP_ID, XLOG_RELMAP_UPDATE);

                                                                      
    XLogFlush(lsn);
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_RELATION_MAP_WRITE);
  if (write(fd, newmap, sizeof(RelMapFile)) != sizeof(RelMapFile))
  {
                                                                    
    if (errno == 0)
    {
      errno = ENOSPC;
    }
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write file \"%s\": %m", mapfilename)));
  }
  pgstat_report_wait_end();

     
                                                                           
                                                                            
                                                                    
                            
     
  pgstat_report_wait_start(WAIT_EVENT_RELATION_MAP_SYNC);
  if (pg_fsync(fd) != 0)
  {
    ereport(data_sync_elevel(ERROR), (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", mapfilename)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", mapfilename)));
  }

     
                                                                           
                                                                       
                                                                         
                                                                           
                                                                           
                                                            
     
  if (send_sinval)
  {
    CacheInvalidateRelmap(dbid);
  }

     
                                                                             
                                                                         
                                                                             
                                                                             
                         
     
                                                                          
                                                                   
     
  if (preserve_files)
  {
    int32 i;

    for (i = 0; i < newmap->num_mappings; i++)
    {
      RelFileNode rnode;

      rnode.spcNode = tsid;
      rnode.dbNode = dbid;
      rnode.relNode = newmap->mappings[i].mapfilenode;
      RelationPreserveStorage(rnode, false);
    }
  }

     
                                                                            
                                                                            
                                            
     
  if (realmap != newmap)
  {
    memcpy(realmap, newmap, sizeof(RelMapFile));
  }
  else
  {
    Assert(!send_sinval);                            
  }

                             
  if (write_wal)
  {
    END_CRIT_SECTION();
  }
}

   
                                                                
                                                                         
                                              
   
static void
perform_relmap_update(bool shared, const RelMapFile *updates)
{
  RelMapFile newmap;

     
                                                                             
                                                                             
                                                                             
                                                                            
                                                                         
                                                                        
                                           
     
                                                                          
                                                                      
              
     
  LWLockAcquire(RelationMappingLock, LW_EXCLUSIVE);

                                                     
  load_relmap_file(shared, true);

                                                
  if (shared)
  {
    memcpy(&newmap, &shared_map, sizeof(RelMapFile));
  }
  else
  {
    memcpy(&newmap, &local_map, sizeof(RelMapFile));
  }

     
                                                                         
                                                    
     
  merge_map_updates(&newmap, updates, allowSystemTableMods);

                                                              
  write_relmap_file(shared, &newmap, true, true, true, (shared ? InvalidOid : MyDatabaseId), (shared ? GLOBALTABLESPACE_OID : MyDatabaseTableSpace), DatabasePath);

                                   
  LWLockRelease(RelationMappingLock);
}

   
                                      
   
void
relmap_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                    
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_RELMAP_UPDATE)
  {
    xl_relmap_update *xlrec = (xl_relmap_update *)XLogRecGetData(record);
    RelMapFile newmap;
    char *dbpath;

    if (xlrec->nbytes != sizeof(RelMapFile))
    {
      elog(PANIC, "relmap_redo: wrong size %u in relmap update record", xlrec->nbytes);
    }
    memcpy(&newmap, xlrec->data, sizeof(newmap));

                                                             
    dbpath = GetDatabasePath(xlrec->dbid, xlrec->tsid);

       
                                                                          
                                                                     
                               
       
                                                                          
                                                                  
       
    LWLockAcquire(RelationMappingLock, LW_EXCLUSIVE);
    write_relmap_file((xlrec->dbid == InvalidOid), &newmap, false, true, false, xlrec->dbid, xlrec->tsid, dbpath);
    LWLockRelease(RelationMappingLock);

    pfree(dbpath);
  }
  else
  {
    elog(PANIC, "relmap_redo: unknown op code %u", info);
  }
}
