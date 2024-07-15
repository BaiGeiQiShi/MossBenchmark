                                                                            
   
              
                                                            
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "storage/bufmgr.h"
#include "storage/sinval.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/partcache.h"
#include "utils/syscache.h"

typedef struct PartitionDirectoryData
{
  MemoryContext pdir_mcxt;
  HTAB *pdir_hash;
} PartitionDirectoryData;

typedef struct PartitionDirectoryEntry
{
  Oid reloid;
  Relation rel;
  PartitionDesc pd;
} PartitionDirectoryEntry;

   
                              
                                                                 
   
                                                           
                                                          
                                                                   
                                                                 
                               
   
void
RelationBuildPartitionDesc(Relation rel)
{
  PartitionDesc partdesc;
  PartitionBoundInfo boundinfo = NULL;
  List *inhoids;
  PartitionBoundSpec **boundspecs = NULL;
  Oid *oids = NULL;
  ListCell *cell;
  int i, nparts;
  PartitionKey key = RelationGetPartitionKey(rel);
  MemoryContext oldcxt;
  int *mapping;

     
                                                                          
                                                                             
                                                                         
                                      
     
  inhoids = find_inheritance_children(RelationGetRelid(rel), NoLock);
  nparts = list_length(inhoids);

                                                
  if (nparts > 0)
  {
    oids = palloc(nparts * sizeof(Oid));
    boundspecs = palloc(nparts * sizeof(PartitionBoundSpec *));
  }

                                                    
  i = 0;
  foreach (cell, inhoids)
  {
    Oid inhrelid = lfirst_oid(cell);
    HeapTuple tuple;
    PartitionBoundSpec *boundspec = NULL;

                                                              
    tuple = SearchSysCache1(RELOID, inhrelid);
    if (HeapTupleIsValid(tuple))
    {
      Datum datum;
      bool isnull;

      datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relpartbound, &isnull);
      if (!isnull)
      {
        boundspec = stringToNode(TextDatumGetCString(datum));
      }
      ReleaseSysCache(tuple);
    }

       
                                                                           
                                                                          
                                                                           
                                                                  
                                                                         
                                                                       
                                                                        
       
                                                                          
                                                                    
                                                                       
                                                                      
       
    if (boundspec == NULL)
    {
      Relation pg_class;
      SysScanDesc scan;
      ScanKeyData key[1];
      Datum datum;
      bool isnull;

      pg_class = table_open(RelationRelationId, AccessShareLock);
      ScanKeyInit(&key[0], Anum_pg_class_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(inhrelid));
      scan = systable_beginscan(pg_class, ClassOidIndexId, true, NULL, 1, key);
      tuple = systable_getnext(scan);
      datum = heap_getattr(tuple, Anum_pg_class_relpartbound, RelationGetDescr(pg_class), &isnull);
      if (!isnull)
      {
        boundspec = stringToNode(TextDatumGetCString(datum));
      }
      systable_endscan(scan);
      table_close(pg_class, AccessShareLock);
    }

                        
    if (!boundspec)
    {
      elog(ERROR, "missing relpartbound for relation %u", inhrelid);
    }
    if (!IsA(boundspec, PartitionBoundSpec))
    {
      elog(ERROR, "invalid relpartbound for relation %u", inhrelid);
    }

       
                                                                         
                                                                    
                           
       
    if (boundspec->is_default)
    {
      Oid partdefid;

      partdefid = get_default_partition_oid(RelationGetRelid(rel));
      if (partdefid != inhrelid)
      {
        elog(ERROR, "expected partdefid %u, but got %u", inhrelid, partdefid);
      }
    }

                       
    oids[i] = inhrelid;
    boundspecs[i] = boundspec;
    ++i;
  }

                                                             
  Assert(rel->rd_pdcxt == NULL);
  Assert(rel->rd_partdesc == NULL);

     
                                                                        
                                                                      
                                                                            
                                                                            
                                                                     
                                                                       
                                                       
     
  rel->rd_pdcxt = AllocSetContextCreate(CacheMemoryContext, "partition descriptor", ALLOCSET_SMALL_SIZES);
  MemoryContextCopyAndSetIdentifier(rel->rd_pdcxt, RelationGetRelationName(rel));

  partdesc = (PartitionDescData *)MemoryContextAllocZero(rel->rd_pdcxt, sizeof(PartitionDescData));
  partdesc->nparts = nparts;
                                                                          
  if (nparts > 0)
  {
                                                                
    boundinfo = partition_bounds_create(boundspecs, nparts, key, &mapping);

                                                     
    oldcxt = MemoryContextSwitchTo(rel->rd_pdcxt);
    partdesc->boundinfo = partition_bounds_copy(boundinfo, key);
    partdesc->oids = (Oid *)palloc(nparts * sizeof(Oid));
    partdesc->is_leaf = (bool *)palloc(nparts * sizeof(bool));
    MemoryContextSwitchTo(oldcxt);

       
                                                                      
                                                                        
                                                                       
                                                                        
       
                                                                 
                                                                        
                             
       
    for (i = 0; i < nparts; i++)
    {
      int index = mapping[i];

      partdesc->oids[index] = oids[i];
      partdesc->is_leaf[index] = (get_rel_relkind(oids[i]) != RELKIND_PARTITIONED_TABLE);
    }
  }

  rel->rd_partdesc = partdesc;
}

   
                            
                                             
   
PartitionDirectory
CreatePartitionDirectory(MemoryContext mcxt)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(mcxt);
  PartitionDirectory pdir;
  HASHCTL ctl;

  MemSet(&ctl, 0, sizeof(HASHCTL));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(PartitionDirectoryEntry);
  ctl.hcxt = mcxt;

  pdir = palloc(sizeof(PartitionDirectoryData));
  pdir->pdir_mcxt = mcxt;
  pdir->pdir_hash = hash_create("partition directory", 256, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

  MemoryContextSwitchTo(oldcontext);
  return pdir;
}

   
                            
                                                                      
   
                                                                  
                                                                     
                                                                         
                                                                       
                                                                  
                               
   
PartitionDesc
PartitionDirectoryLookup(PartitionDirectory pdir, Relation rel)
{
  PartitionDirectoryEntry *pde;
  Oid relid = RelationGetRelid(rel);
  bool found;

  pde = hash_search(pdir->pdir_hash, &relid, HASH_ENTER, &found);
  if (!found)
  {
       
                                                                  
                                                                   
       
    RelationIncrementReferenceCount(rel);
    pde->rel = rel;
    pde->pd = RelationGetPartitionDesc(rel);
    Assert(pde->pd != NULL);
  }
  return pde->pd;
}

   
                             
                                   
   
                                               
   
void
DestroyPartitionDirectory(PartitionDirectory pdir)
{
  HASH_SEQ_STATUS status;
  PartitionDirectoryEntry *pde;

  hash_seq_init(&status, pdir->pdir_hash);
  while ((pde = hash_seq_search(&status)) != NULL)
  {
    RelationDecrementReferenceCount(pde->rel);
  }
}

   
                       
                                                           
   
bool
equalPartitionDescs(PartitionKey key, PartitionDesc partdesc1, PartitionDesc partdesc2)
{
  int i;

  if (partdesc1 != NULL)
  {
    if (partdesc2 == NULL)
    {
      return false;
    }
    if (partdesc1->nparts != partdesc2->nparts)
    {
      return false;
    }

    Assert(key != NULL || partdesc1->nparts == 0);

       
                                                                         
                                                                           
                                              
       
    for (i = 0; i < partdesc1->nparts; i++)
    {
      if (partdesc1->oids[i] != partdesc2->oids[i])
      {
        return false;
      }
    }

       
                                                                           
                                                  
       
    if (partdesc1->boundinfo != NULL)
    {
      if (partdesc2->boundinfo == NULL)
      {
        return false;
      }

      if (!partition_bounds_equal(key->partnatts, key->parttyplen, key->parttypbyval, partdesc1->boundinfo, partdesc2->boundinfo))
      {
        return false;
      }
    }
    else if (partdesc2->boundinfo != NULL)
    {
      return false;
    }
  }
  else if (partdesc2 != NULL)
  {
    return false;
  }

  return true;
}

   
                                 
   
                                                                             
                                        
   
Oid
get_default_oid_from_partdesc(PartitionDesc partdesc)
{
  if (partdesc && partdesc->boundinfo && partition_bound_has_default(partdesc->boundinfo))
  {
    return partdesc->oids[partdesc->boundinfo->default_index];
  }

  return InvalidOid;
}
