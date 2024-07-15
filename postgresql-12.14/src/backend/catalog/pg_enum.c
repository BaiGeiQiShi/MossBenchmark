                                                                            
   
             
                                                              
   
                                                                
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_type.h"
#include "storage/lmgr.h"
#include "miscadmin.h"
#include "nodes/value.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

                                                     
Oid binary_upgrade_next_pg_enum_oid = InvalidOid;

   
                                                                           
                                                                          
                                                                          
                                                                           
                                                                          
                                                                              
                                                                        
   
static HTAB *enum_blacklist = NULL;

static void
RenumberEnumType(Relation pg_enum, HeapTuple *existing, int nelems);
static int
sort_order_cmp(const void *p1, const void *p2);

   
                    
                                                                     
   
                                    
   
void
EnumValuesCreate(Oid enumTypeOid, List *vals)
{
  Relation pg_enum;
  NameData enumlabel;
  Oid *oids;
  int elemno, num_elems;
  Datum values[Natts_pg_enum];
  bool nulls[Natts_pg_enum];
  ListCell *lc;
  HeapTuple tup;

  num_elems = list_length(vals);

     
                                                                            
                                                                             
                                       
     

  pg_enum = table_open(EnumRelationId, RowExclusiveLock);

     
                                           
     
                                                                         
                                                                             
                                                                             
                                                                
     
  oids = (Oid *)palloc(num_elems * sizeof(Oid));

  for (elemno = 0; elemno < num_elems; elemno++)
  {
       
                                                                      
                                                                       
                                           
       
    Oid new_oid;

    do
    {
      new_oid = GetNewOidWithIndex(pg_enum, EnumOidIndexId, Anum_pg_enum_oid);
    } while (new_oid & 1);
    oids[elemno] = new_oid;
  }

                                                                    
  qsort(oids, num_elems, sizeof(Oid), oid_cmp);

                            
  memset(nulls, false, sizeof(nulls));

  elemno = 0;
  foreach (lc, vals)
  {
    char *lab = strVal(lfirst(lc));

       
                                                                         
                                                        
       
    if (strlen(lab) > (NAMEDATALEN - 1))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid enum label \"%s\"", lab), errdetail("Labels must be %d characters or less.", NAMEDATALEN - 1)));
    }

    values[Anum_pg_enum_oid - 1] = ObjectIdGetDatum(oids[elemno]);
    values[Anum_pg_enum_enumtypid - 1] = ObjectIdGetDatum(enumTypeOid);
    values[Anum_pg_enum_enumsortorder - 1] = Float4GetDatum(elemno + 1);
    namestrcpy(&enumlabel, lab);
    values[Anum_pg_enum_enumlabel - 1] = NameGetDatum(&enumlabel);

    tup = heap_form_tuple(RelationGetDescr(pg_enum), values, nulls);

    CatalogTupleInsert(pg_enum, tup);
    heap_freetuple(tup);

    elemno++;
  }

                
  pfree(oids);
  table_close(pg_enum, RowExclusiveLock);
}

   
                    
                                                                
   
void
EnumValuesDelete(Oid enumTypeOid)
{
  Relation pg_enum;
  ScanKeyData key[1];
  SysScanDesc scan;
  HeapTuple tup;

  pg_enum = table_open(EnumRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_enum_enumtypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(enumTypeOid));

  scan = systable_beginscan(pg_enum, EnumTypIdLabelIndexId, true, NULL, 1, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    CatalogTupleDelete(pg_enum, &tup->t_self);
  }

  systable_endscan(scan);

  table_close(pg_enum, RowExclusiveLock);
}

   
                                                       
   
static void
init_enum_blacklist(void)
{
  HASHCTL hash_ctl;

  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(Oid);
  hash_ctl.hcxt = TopTransactionContext;
  enum_blacklist = hash_create("Enum value blacklist", 32, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

   
                
                                                           
                                                           
                                   
   
void
AddEnumLabel(Oid enumTypeOid, const char *newVal, const char *neighbor, bool newValIsAfter, bool skipIfExists)
{
  Relation pg_enum;
  Oid newOid;
  Datum values[Natts_pg_enum];
  bool nulls[Natts_pg_enum];
  NameData enumlabel;
  HeapTuple enum_tup;
  float4 newelemorder;
  HeapTuple *existing;
  CatCList *list;
  int nelems;
  int i;

                                       
  if (strlen(newVal) > (NAMEDATALEN - 1))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid enum label \"%s\"", newVal), errdetail("Labels must be %d characters or less.", NAMEDATALEN - 1)));
  }

     
                                                                           
                                                                           
                                                                            
                                                                          
                                                               
                       
     
  LockDatabaseObject(TypeRelationId, enumTypeOid, 0, ExclusiveLock);

     
                                                                          
                                                                      
                                                       
     
  enum_tup = SearchSysCache2(ENUMTYPOIDNAME, ObjectIdGetDatum(enumTypeOid), CStringGetDatum(newVal));
  if (HeapTupleIsValid(enum_tup))
  {
    ReleaseSysCache(enum_tup);
    if (skipIfExists)
    {
      ereport(NOTICE, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("enum label \"%s\" already exists, skipping", newVal)));
      return;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("enum label \"%s\" already exists", newVal)));
    }
  }

  pg_enum = table_open(EnumRelationId, RowExclusiveLock);

                                                                         
restart:

                                                    
  list = SearchSysCacheList1(ENUMTYPOIDNAME, ObjectIdGetDatum(enumTypeOid));
  nelems = list->n_members;

                                                  
  existing = (HeapTuple *)palloc(nelems * sizeof(HeapTuple));
  for (i = 0; i < nelems; i++)
  {
    existing[i] = &(list->members[i]->tuple);
  }

  qsort(existing, nelems, sizeof(HeapTuple), sort_order_cmp);

  if (neighbor == NULL)
  {
       
                                                                       
                           
       
    if (nelems > 0)
    {
      Form_pg_enum en = (Form_pg_enum)GETSTRUCT(existing[nelems - 1]);

      newelemorder = en->enumsortorder + 1;
    }
    else
    {
      newelemorder = 1;
    }
  }
  else
  {
                                       
    int nbr_index;
    int other_nbr_index;
    Form_pg_enum nbr_en;
    Form_pg_enum other_nbr_en;

                                     
    for (nbr_index = 0; nbr_index < nelems; nbr_index++)
    {
      Form_pg_enum en = (Form_pg_enum)GETSTRUCT(existing[nbr_index]);

      if (strcmp(NameStr(en->enumlabel), neighbor) == 0)
      {
        break;
      }
    }
    if (nbr_index >= nelems)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is not an existing enum label", neighbor)));
    }
    nbr_en = (Form_pg_enum)GETSTRUCT(existing[nbr_index]);

       
                                                                           
                                                                         
                                     
       
                                                                         
                                                                          
                                                                        
                      
       
    if (newValIsAfter)
    {
      other_nbr_index = nbr_index + 1;
    }
    else
    {
      other_nbr_index = nbr_index - 1;
    }

    if (other_nbr_index < 0)
    {
      newelemorder = nbr_en->enumsortorder - 1;
    }
    else if (other_nbr_index >= nelems)
    {
      newelemorder = nbr_en->enumsortorder + 1;
    }
    else
    {
         
                                                                      
                                                                       
                                                                        
                                                                       
                                      
         
      volatile float4 midpoint;

      other_nbr_en = (Form_pg_enum)GETSTRUCT(existing[other_nbr_index]);
      midpoint = (nbr_en->enumsortorder + other_nbr_en->enumsortorder) / 2;

      if (midpoint == nbr_en->enumsortorder || midpoint == other_nbr_en->enumsortorder)
      {
        RenumberEnumType(pg_enum, existing, nelems);
                                     
        pfree(existing);
        ReleaseCatCacheList(list);
        goto restart;
      }

      newelemorder = midpoint;
    }
  }

                                       
  if (IsBinaryUpgrade)
  {
    if (!OidIsValid(binary_upgrade_next_pg_enum_oid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_enum OID value not set when in binary upgrade mode")));
    }

       
                                                                        
                                                                      
                                    
       
    if (neighbor != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ALTER TYPE ADD BEFORE/AFTER is incompatible with binary upgrade")));
    }

    newOid = binary_upgrade_next_pg_enum_oid;
    binary_upgrade_next_pg_enum_oid = InvalidOid;
  }
  else
  {
       
                                                                 
       
                                                                          
                                                                      
                                                                          
       
    for (;;)
    {
      bool sorts_ok;

                                                                      
      newOid = GetNewOidWithIndex(pg_enum, EnumOidIndexId, Anum_pg_enum_oid);

         
                                                                
                                                                   
                                                                         
                                             
         
      sorts_ok = true;
      for (i = 0; i < nelems; i++)
      {
        HeapTuple exists_tup = existing[i];
        Form_pg_enum exists_en = (Form_pg_enum)GETSTRUCT(exists_tup);
        Oid exists_oid = exists_en->oid;

        if (exists_oid & 1)
        {
          continue;                      
        }

        if (exists_en->enumsortorder < newelemorder)
        {
                                  
          if (exists_oid >= newOid)
          {
            sorts_ok = false;
            break;
          }
        }
        else
        {
                                 
          if (exists_oid <= newOid)
          {
            sorts_ok = false;
            break;
          }
        }
      }

      if (sorts_ok)
      {
                                                    
        if ((newOid & 1) == 0)
        {
          break;
        }

           
                                                                       
                                                                       
                                                
           
      }
      else
      {
           
                                                                 
                                                             
                                                          
           
        if (newOid & 1)
        {
          break;
        }

           
                                                                       
                                                                     
           
      }
    }
  }

                                             
  pfree(existing);
  ReleaseCatCacheList(list);

                                    
  memset(nulls, false, sizeof(nulls));
  values[Anum_pg_enum_oid - 1] = ObjectIdGetDatum(newOid);
  values[Anum_pg_enum_enumtypid - 1] = ObjectIdGetDatum(enumTypeOid);
  values[Anum_pg_enum_enumsortorder - 1] = Float4GetDatum(newelemorder);
  namestrcpy(&enumlabel, newVal);
  values[Anum_pg_enum_enumlabel - 1] = NameGetDatum(&enumlabel);
  enum_tup = heap_form_tuple(RelationGetDescr(pg_enum), values, nulls);
  CatalogTupleInsert(pg_enum, enum_tup);
  heap_freetuple(enum_tup);

  table_close(pg_enum, RowExclusiveLock);

                                                                         
  if (enum_blacklist == NULL)
  {
    init_enum_blacklist();
  }

                                          
  (void)hash_search(enum_blacklist, &newOid, HASH_ENTER, NULL);
}

   
                   
                                   
   
void
RenameEnumLabel(Oid enumTypeOid, const char *oldVal, const char *newVal)
{
  Relation pg_enum;
  HeapTuple enum_tup;
  Form_pg_enum en;
  CatCList *list;
  int nelems;
  HeapTuple old_tup;
  bool found_new;
  int i;

                                       
  if (strlen(newVal) > (NAMEDATALEN - 1))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("invalid enum label \"%s\"", newVal), errdetail("Labels must be %d characters or less.", NAMEDATALEN - 1)));
  }

     
                                                                           
                                                                           
                                                                          
                                                                          
                          
     
  LockDatabaseObject(TypeRelationId, enumTypeOid, 0, ExclusiveLock);

  pg_enum = table_open(EnumRelationId, RowExclusiveLock);

                                                    
  list = SearchSysCacheList1(ENUMTYPOIDNAME, ObjectIdGetDatum(enumTypeOid));
  nelems = list->n_members;

     
                                                                           
                                                                        
                                         
     
  old_tup = NULL;
  found_new = false;
  for (i = 0; i < nelems; i++)
  {
    enum_tup = &(list->members[i]->tuple);
    en = (Form_pg_enum)GETSTRUCT(enum_tup);
    if (strcmp(NameStr(en->enumlabel), oldVal) == 0)
    {
      old_tup = enum_tup;
    }
    if (strcmp(NameStr(en->enumlabel), newVal) == 0)
    {
      found_new = true;
    }
  }
  if (!old_tup)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is not an existing enum label", oldVal)));
  }
  if (found_new)
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("enum label \"%s\" already exists", newVal)));
  }

                                             
  enum_tup = heap_copytuple(old_tup);
  en = (Form_pg_enum)GETSTRUCT(enum_tup);

  ReleaseCatCacheList(list);

                                
  namestrcpy(&en->enumlabel, newVal);
  CatalogTupleUpdate(pg_enum, &enum_tup->t_self, enum_tup);
  heap_freetuple(enum_tup);

  table_close(pg_enum, RowExclusiveLock);
}

   
                                                    
   
bool
EnumBlacklisted(Oid enum_id)
{
  bool found;

                                                             
  if (enum_blacklist == NULL)
  {
    return false;
  }

                                 
  (void)hash_search(enum_blacklist, &enum_id, HASH_FIND, &found);
  return found;
}

   
                                                           
   
void
AtEOXact_Enum(void)
{
     
                                                                          
                                                                         
                                                  
     
  enum_blacklist = NULL;
}

   
                    
                                                                 
   
                                                                          
                                                                       
                                                                            
                                                                       
                                                                           
                                                                        
                                                
   
                                            
   
                                                                        
                                                                         
                                                                    
                   
   
                                                                  
                                              
   
static void
RenumberEnumType(Relation pg_enum, HeapTuple *existing, int nelems)
{
  int i;

     
                                                                        
                                                                            
                                     
     
  for (i = nelems - 1; i >= 0; i--)
  {
    HeapTuple newtup;
    Form_pg_enum en;
    float4 newsortorder;

    newtup = heap_copytuple(existing[i]);
    en = (Form_pg_enum)GETSTRUCT(newtup);

    newsortorder = i + 1;
    if (en->enumsortorder != newsortorder)
    {
      en->enumsortorder = newsortorder;

      CatalogTupleUpdate(pg_enum, &newtup->t_self, newtup);
    }

    heap_freetuple(newtup);
  }

                                
  CommandCounterIncrement();
}

                                                        
static int
sort_order_cmp(const void *p1, const void *p2)
{
  HeapTuple v1 = *((const HeapTuple *)p1);
  HeapTuple v2 = *((const HeapTuple *)p2);
  Form_pg_enum en1 = (Form_pg_enum)GETSTRUCT(v1);
  Form_pg_enum en2 = (Form_pg_enum)GETSTRUCT(v2);

  if (en1->enumsortorder < en2->enumsortorder)
  {
    return -1;
  }
  else if (en1->enumsortorder > en2->enumsortorder)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

Size
EstimateEnumBlacklistSpace(void)
{
  size_t entries;

  if (enum_blacklist)
  {
    entries = hash_get_num_entries(enum_blacklist);
  }
  else
  {
    entries = 0;
  }

                                   
  return sizeof(Oid) * (entries + 1);
}

void
SerializeEnumBlacklist(void *space, Size size)
{
  Oid *serialized = (Oid *)space;

     
                                                                      
                         
     
  Assert(size == EstimateEnumBlacklistSpace());

                                                                      
  if (enum_blacklist)
  {
    HASH_SEQ_STATUS status;
    Oid *value;

    hash_seq_init(&status, enum_blacklist);
    while ((value = (Oid *)hash_seq_search(&status)))
    {
      *serialized++ = *value;
    }
  }

                                 
  *serialized = InvalidOid;

     
                                                                     
                
     
  Assert((char *)(serialized + 1) == ((char *)space) + size);
}

void
RestoreEnumBlacklist(void *space)
{
  Oid *serialized = (Oid *)space;

  Assert(!enum_blacklist);

     
                                                                       
                                                                           
                             
     
  if (!OidIsValid(*serialized))
  {
    return;
  }

                                                  
  init_enum_blacklist();
  do
  {
    hash_search(enum_blacklist, serialized++, HASH_ENTER, NULL);
  } while (OidIsValid(*serialized));
}
