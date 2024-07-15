                                                                            
   
                 
                                                                  
   
                                                                          
                                                                               
                                                                       
                                                            
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_inherits.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

   
                                                                 
   
typedef struct SeenRelsEntry
{
  Oid rel_id;                                  
  ListCell *numparents_cell;                              
} SeenRelsEntry;

   
                             
   
                                                             
                                                                
   
                                                                              
                                                                            
                                                                         
                                              
   
List *
find_inheritance_children(Oid parentrelId, LOCKMODE lockmode)
{
  List *list = NIL;
  Relation relation;
  SysScanDesc scan;
  ScanKeyData key[1];
  HeapTuple inheritsTuple;
  Oid inhrelid;
  Oid *oidarr;
  int maxoids, numoids, i;

     
                                                                      
               
     
  if (!has_subclass(parentrelId))
  {
    return NIL;
  }

     
                                                                  
     
  maxoids = 32;
  oidarr = (Oid *)palloc(maxoids * sizeof(Oid));
  numoids = 0;

  relation = table_open(InheritsRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_inherits_inhparent, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(parentrelId));

  scan = systable_beginscan(relation, InheritsParentIndexId, true, NULL, 1, key);

  while ((inheritsTuple = systable_getnext(scan)) != NULL)
  {
    inhrelid = ((Form_pg_inherits)GETSTRUCT(inheritsTuple))->inhrelid;
    if (numoids >= maxoids)
    {
      maxoids *= 2;
      oidarr = (Oid *)repalloc(oidarr, maxoids * sizeof(Oid));
    }
    oidarr[numoids++] = inhrelid;
  }

  systable_endscan(scan);

  table_close(relation, AccessShareLock);

     
                                                                      
                                                                     
                                                                         
                                                                  
     
  if (numoids > 1)
  {
    qsort(oidarr, numoids, sizeof(Oid), oid_cmp);
  }

     
                                              
     
  for (i = 0; i < numoids; i++)
  {
    inhrelid = oidarr[i];

    if (lockmode != NoLock)
    {
                                                               
      LockRelationOid(inhrelid, lockmode);

         
                                                                        
                                                                       
                                                
         
      if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(inhrelid)))
      {
                                  
        UnlockRelationOid(inhrelid, lockmode);
                                      
        continue;
      }
    }

    list = lappend_oid(list, inhrelid);
  }

  pfree(oidarr);

  return list;
}

   
                         
                                                                 
                                                                
                                                                
                                                                 
               
   
                                                                              
                                                                            
                                                                         
                                              
   
List *
find_all_inheritors(Oid parentrelId, LOCKMODE lockmode, List **numparents)
{
                                                                 
  HTAB *seen_rels;
  HASHCTL ctl;
  List *rels_list, *rel_numparents;
  ListCell *l;

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(SeenRelsEntry);
  ctl.hcxt = CurrentMemoryContext;

  seen_rels = hash_create("find_all_inheritors temporary table", 32,                             
      &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

     
                                                                           
                                                                        
                                                                          
                                                                           
                                                                       
     
  rels_list = list_make1_oid(parentrelId);
  rel_numparents = list_make1_int(0);

  foreach (l, rels_list)
  {
    Oid currentrel = lfirst_oid(l);
    List *currentchildren;
    ListCell *lc;

                                             
    currentchildren = find_inheritance_children(currentrel, lockmode);

       
                                                                          
                                                                           
                                                                           
                                                                   
                                  
       
    foreach (lc, currentchildren)
    {
      Oid child_oid = lfirst_oid(lc);
      bool found;
      SeenRelsEntry *hash_entry;

      hash_entry = hash_search(seen_rels, &child_oid, HASH_ENTER, &found);
      if (found)
      {
                                                                         
        lfirst_int(hash_entry->numparents_cell)++;
      }
      else
      {
                                                                    
        rels_list = lappend_oid(rels_list, child_oid);
        rel_numparents = lappend_int(rel_numparents, 1);
        hash_entry->numparents_cell = rel_numparents->tail;
      }
    }
  }

  if (numparents)
  {
    *numparents = rel_numparents;
  }
  else
  {
    list_free(rel_numparents);
  }

  hash_destroy(seen_rels);

  return rels_list;
}

   
                                                        
   
                                                                 
                                                                    
                                                                     
                                                                         
                                                                          
   
                                                                     
                                                                       
                                                                        
   
                                                                         
                                                                          
   
bool
has_subclass(Oid relationId)
{
  HeapTuple tuple;
  bool result;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relationId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relationId);
  }

  result = ((Form_pg_class)GETSTRUCT(tuple))->relhassubclass;
  ReleaseSysCache(tuple);
  return result;
}

   
                                                             
   
                                                                          
                                                                         
                                                                            
   
bool
has_superclass(Oid relationId)
{
  Relation catalog;
  SysScanDesc scan;
  ScanKeyData skey;
  bool result;

  catalog = table_open(InheritsRelationId, AccessShareLock);
  ScanKeyInit(&skey, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relationId));
  scan = systable_beginscan(catalog, InheritsRelidSeqnoIndexId, true, NULL, 1, &skey);
  result = HeapTupleIsValid(systable_getnext(scan));
  systable_endscan(scan);
  table_close(catalog, AccessShareLock);

  return result;
}

   
                                                                      
                                               
   
                                                                              
                                                                            
                                                                             
                                           
   
bool
typeInheritsFrom(Oid subclassTypeId, Oid superclassTypeId)
{
  bool result = false;
  Oid subclassRelid;
  Oid superclassRelid;
  Relation inhrel;
  List *visited, *queue;
  ListCell *queue_item;

                                                         
  subclassRelid = typeOrDomainTypeRelid(subclassTypeId);
  if (subclassRelid == InvalidOid)
  {
    return false;                                            
  }
  superclassRelid = typeidTypeRelid(superclassTypeId);
  if (superclassRelid == InvalidOid)
  {
    return false;                         
  }

                                                                 
  if (!has_subclass(superclassRelid))
  {
    return false;
  }

     
                                                                             
     
  queue = list_make1_oid(subclassRelid);
  visited = NIL;

  inhrel = table_open(InheritsRelationId, AccessShareLock);

     
                                                                             
                                                                            
                                                                          
                                                              
     
  foreach (queue_item, queue)
  {
    Oid this_relid = lfirst_oid(queue_item);
    ScanKeyData skey;
    SysScanDesc inhscan;
    HeapTuple inhtup;

       
                                                                          
                                                                       
                                                                     
                                             
       
    if (list_member_oid(visited, this_relid))
    {
      continue;
    }

       
                                                                 
                                                                         
                                       
       
    visited = lappend_oid(visited, this_relid);

    ScanKeyInit(&skey, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(this_relid));

    inhscan = systable_beginscan(inhrel, InheritsRelidSeqnoIndexId, true, NULL, 1, &skey);

    while ((inhtup = systable_getnext(inhscan)) != NULL)
    {
      Form_pg_inherits inh = (Form_pg_inherits)GETSTRUCT(inhtup);
      Oid inhparent = inh->inhparent;

                                                        
      if (inhparent == superclassRelid)
      {
        result = true;
        break;
      }

                             
      queue = lappend_oid(queue, inhparent);
    }

    systable_endscan(inhscan);

    if (result)
    {
      break;
    }
  }

                    
  table_close(inhrel, AccessShareLock);

  list_free(visited);
  list_free(queue);

  return result;
}

   
                                                       
   
void
StoreSingleInheritance(Oid relationId, Oid parentOid, int32 seqNumber)
{
  Datum values[Natts_pg_inherits];
  bool nulls[Natts_pg_inherits];
  HeapTuple tuple;
  Relation inhRelation;

  inhRelation = table_open(InheritsRelationId, RowExclusiveLock);

     
                                
     
  values[Anum_pg_inherits_inhrelid - 1] = ObjectIdGetDatum(relationId);
  values[Anum_pg_inherits_inhparent - 1] = ObjectIdGetDatum(parentOid);
  values[Anum_pg_inherits_inhseqno - 1] = Int32GetDatum(seqNumber);

  memset(nulls, 0, sizeof(nulls));

  tuple = heap_form_tuple(RelationGetDescr(inhRelation), values, nulls);

  CatalogTupleInsert(inhRelation, tuple);

  heap_freetuple(tuple);

  table_close(inhRelation, RowExclusiveLock);
}

   
                       
   
                                                                              
                                                                          
                                                              
   
                                                 
   
bool
DeleteInheritsTuple(Oid inhrelid, Oid inhparent)
{
  bool found = false;
  Relation catalogRelation;
  ScanKeyData key;
  SysScanDesc scan;
  HeapTuple inheritsTuple;

     
                                           
     
  catalogRelation = table_open(InheritsRelationId, RowExclusiveLock);
  ScanKeyInit(&key, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(inhrelid));
  scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId, true, NULL, 1, &key);

  while (HeapTupleIsValid(inheritsTuple = systable_getnext(scan)))
  {
    Oid parent;

                                                                        
    parent = ((Form_pg_inherits)GETSTRUCT(inheritsTuple))->inhparent;
    if (!OidIsValid(inhparent) || parent == inhparent)
    {
      CatalogTupleDelete(catalogRelation, &inheritsTuple->t_self);
      found = true;
    }
  }

            
  systable_endscan(scan);
  table_close(catalogRelation, RowExclusiveLock);

  return found;
}
