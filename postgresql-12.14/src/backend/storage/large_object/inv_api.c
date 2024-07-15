                                                                            
   
             
                                                                     
                                                                          
   
   
                                                                       
                                                                              
                                                                          
                                                                           
                                                                            
                                                                               
   
                                                                               
                                                                            
                                                                           
                                                                         
                                                                   
   
   
                                                                         
                                                                        
   
   
                  
                                                
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/genam.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "libpq/libpq-fs.h"
#include "miscadmin.h"
#include "storage/large_object.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

   
                                                                      
   
bool lo_compat_privileges;

   
                                                                              
                                                                             
                                                                   
                                                                               
                                                          
   
static Relation lo_heap_r = NULL;
static Relation lo_index_r = NULL;

   
                                                                          
   
static void
open_lo_relation(void)
{
  ResourceOwner currentOwner;

  if (lo_heap_r && lo_index_r)
  {
    return;                                   
  }

                                                                 
  currentOwner = CurrentResourceOwner;
  CurrentResourceOwner = TopTransactionResourceOwner;

                                                                
  if (lo_heap_r == NULL)
  {
    lo_heap_r = table_open(LargeObjectRelationId, RowExclusiveLock);
  }
  if (lo_index_r == NULL)
  {
    lo_index_r = index_open(LargeObjectLOidPNIndexId, RowExclusiveLock);
  }

  CurrentResourceOwner = currentOwner;
}

   
                                    
   
void
close_lo_relation(bool isCommit)
{
  if (lo_heap_r || lo_index_r)
  {
       
                                                                          
          
       
    if (isCommit)
    {
      ResourceOwner currentOwner;

      currentOwner = CurrentResourceOwner;
      CurrentResourceOwner = TopTransactionResourceOwner;

      if (lo_index_r)
      {
        index_close(lo_index_r, NoLock);
      }
      if (lo_heap_r)
      {
        table_close(lo_heap_r, NoLock);
      }

      CurrentResourceOwner = currentOwner;
    }
    lo_heap_r = NULL;
    lo_index_r = NULL;
  }
}

   
                                                                      
                               
   
static bool
myLargeObjectExists(Oid loid, Snapshot snapshot)
{
  Relation pg_lo_meta;
  ScanKeyData skey[1];
  SysScanDesc sd;
  HeapTuple tuple;
  bool retval = false;

  ScanKeyInit(&skey[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(loid));

  pg_lo_meta = table_open(LargeObjectMetadataRelationId, AccessShareLock);

  sd = systable_beginscan(pg_lo_meta, LargeObjectMetadataOidIndexId, true, snapshot, 1, skey);

  tuple = systable_getnext(sd);
  if (HeapTupleIsValid(tuple))
  {
    retval = true;
  }

  systable_endscan(sd);

  table_close(pg_lo_meta, AccessShareLock);

  return retval;
}

   
                                                                        
                                                                             
                                                                        
   
static void
getdatafield(Form_pg_largeobject tuple, bytea **pdatafield, int *plen, bool *pfreeit)
{
  bytea *datafield;
  int len;
  bool freeit;

  datafield = &(tuple->data);                              
  freeit = false;
  if (VARATT_IS_EXTENDED(datafield))
  {
    datafield = (bytea *)heap_tuple_untoast_attr((struct varlena *)datafield);
    freeit = true;
  }
  len = VARSIZE(datafield) - VARHDRSZ;
  if (len < 0 || len > LOBLKSIZE)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("pg_largeobject entry for OID %u, page %d has invalid data field size %d", tuple->loid, tuple->pageno, len)));
  }
  *pdatafield = datafield;
  *plen = len;
  *pfreeit = freeit;
}

   
                                           
   
              
                                                                         
   
            
                       
   
                                                                           
           
   
Oid
inv_create(Oid lobjId)
{
  Oid lobjId_new;

     
                                                    
     
  lobjId_new = LargeObjectCreate(lobjId);

     
                                            
     
                                                            
                                                                             
                                                                            
                                                                 
                                                                             
     
  recordDependencyOnOwner(LargeObjectRelationId, lobjId_new, GetUserId());

                                               
  InvokeObjectPostCreateHook(LargeObjectRelationId, lobjId_new, 0);

     
                                                                            
     
  CommandCounterIncrement();

  return lobjId_new;
}

   
                                                
   
                                                               
                                                                     
                                                                      
                                                                   
                                                                   
                                           
   
LargeObjectDesc *
inv_open(Oid lobjId, int flags, MemoryContext mcxt)
{
  LargeObjectDesc *retval;
  Snapshot snapshot = NULL;
  int descflags = 0;

     
                                                                            
                                                                    
                                
     
  if (flags & INV_WRITE)
  {
    descflags |= IFS_WRLOCK | IFS_RDLOCK;
  }
  if (flags & INV_READ)
  {
    descflags |= IFS_RDLOCK;
  }

  if (descflags == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid flags for opening a large object: %d", flags)));
  }

                                                                            
  if (descflags & IFS_WRLOCK)
  {
    snapshot = NULL;
  }
  else
  {
    snapshot = GetActiveSnapshot();
  }

                                                                            
  if (!myLargeObjectExists(lobjId, snapshot))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("large object %u does not exist", lobjId)));
  }

                                                          
  if ((descflags & IFS_RDLOCK) != 0)
  {
    if (!lo_compat_privileges && pg_largeobject_aclcheck_snapshot(lobjId, GetUserId(), ACL_SELECT, snapshot) != ACLCHECK_OK)
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for large object %u", lobjId)));
    }
  }
  if ((descflags & IFS_WRLOCK) != 0)
  {
    if (!lo_compat_privileges && pg_largeobject_aclcheck_snapshot(lobjId, GetUserId(), ACL_UPDATE, snapshot) != ACLCHECK_OK)
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for large object %u", lobjId)));
    }
  }

                                 
  retval = (LargeObjectDesc *)MemoryContextAlloc(mcxt, sizeof(LargeObjectDesc));
  retval->id = lobjId;
  retval->offset = 0;
  retval->flags = descflags;

                                                                     
  retval->subid = InvalidSubTransactionId;

     
                                                                       
                                                                
     
  retval->snapshot = snapshot;

  return retval;
}

   
                                                                       
                                             
   
void
inv_close(LargeObjectDesc *obj_desc)
{
  Assert(PointerIsValid(obj_desc));
  pfree(obj_desc);
}

   
                                                                             
   
                                                                      
   
int
inv_drop(Oid lobjId)
{
  ObjectAddress object;

     
                                                              
     
  object.classId = LargeObjectRelationId;
  object.objectId = lobjId;
  object.objectSubId = 0;
  performDeletion(&object, DROP_CASCADE, 0);

     
                                                                         
                                                  
     
  CommandCounterIncrement();

                                                              
  return 1;
}

   
                                    
   
                                                                         
                                    
   
static uint64
inv_getsize(LargeObjectDesc *obj_desc)
{
  uint64 lastbyte = 0;
  ScanKeyData skey[1];
  SysScanDesc sd;
  HeapTuple tuple;

  Assert(PointerIsValid(obj_desc));

  open_lo_relation();

  ScanKeyInit(&skey[0], Anum_pg_largeobject_loid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(obj_desc->id));

  sd = systable_beginscan_ordered(lo_heap_r, lo_index_r, obj_desc->snapshot, 1, skey);

     
                                                                         
                                                                         
                                                                           
                                                 
     
  tuple = systable_getnext_ordered(sd, BackwardScanDirection);
  if (HeapTupleIsValid(tuple))
  {
    Form_pg_largeobject data;
    bytea *datafield;
    int len;
    bool pfreeit;

    if (HeapTupleHasNulls(tuple))               
    {
      elog(ERROR, "null field found in pg_largeobject");
    }
    data = (Form_pg_largeobject)GETSTRUCT(tuple);
    getdatafield(data, &datafield, &len, &pfreeit);
    lastbyte = (uint64)data->pageno * LOBLKSIZE + len;
    if (pfreeit)
    {
      pfree(datafield);
    }
  }

  systable_endscan_ordered(sd);

  return lastbyte;
}

int64
inv_seek(LargeObjectDesc *obj_desc, int64 offset, int whence)
{
  int64 newoffset;

  Assert(PointerIsValid(obj_desc));

     
                                                                           
                                       
     

     
                                                                           
                                                              
     
  switch (whence)
  {
  case SEEK_SET:
    newoffset = offset;
    break;
  case SEEK_CUR:
    newoffset = obj_desc->offset + offset;
    break;
  case SEEK_END:
    newoffset = inv_getsize(obj_desc) + offset;
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid whence setting: %d", whence)));
    newoffset = 0;                          
    break;
  }

     
                                                                           
                                                                    
     
  if (newoffset < 0 || newoffset > MAX_LARGE_OBJECT_SIZE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg_internal("invalid large object seek target: " INT64_FORMAT, newoffset)));
  }

  obj_desc->offset = newoffset;
  return newoffset;
}

int64
inv_tell(LargeObjectDesc *obj_desc)
{
  Assert(PointerIsValid(obj_desc));

     
                                                                           
                                       
     

  return obj_desc->offset;
}

int
inv_read(LargeObjectDesc *obj_desc, char *buf, int nbytes)
{
  int nread = 0;
  int64 n;
  int64 off;
  int len;
  int32 pageno = (int32)(obj_desc->offset / LOBLKSIZE);
  uint64 pageoff;
  ScanKeyData skey[2];
  SysScanDesc sd;
  HeapTuple tuple;

  Assert(PointerIsValid(obj_desc));
  Assert(buf != NULL);

  if ((obj_desc->flags & IFS_RDLOCK) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for large object %u", obj_desc->id)));
  }

  if (nbytes <= 0)
  {
    return 0;
  }

  open_lo_relation();

  ScanKeyInit(&skey[0], Anum_pg_largeobject_loid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(obj_desc->id));

  ScanKeyInit(&skey[1], Anum_pg_largeobject_pageno, BTGreaterEqualStrategyNumber, F_INT4GE, Int32GetDatum(pageno));

  sd = systable_beginscan_ordered(lo_heap_r, lo_index_r, obj_desc->snapshot, 2, skey);

  while ((tuple = systable_getnext_ordered(sd, ForwardScanDirection)) != NULL)
  {
    Form_pg_largeobject data;
    bytea *datafield;
    bool pfreeit;

    if (HeapTupleHasNulls(tuple))               
    {
      elog(ERROR, "null field found in pg_largeobject");
    }
    data = (Form_pg_largeobject)GETSTRUCT(tuple);

       
                                                                      
                                                                           
                                                    
       
    pageoff = ((uint64)data->pageno) * LOBLKSIZE;
    if (pageoff > obj_desc->offset)
    {
      n = pageoff - obj_desc->offset;
      n = (n <= (nbytes - nread)) ? n : (nbytes - nread);
      MemSet(buf + nread, 0, n);
      nread += n;
      obj_desc->offset += n;
    }

    if (nread < nbytes)
    {
      Assert(obj_desc->offset >= pageoff);
      off = (int)(obj_desc->offset - pageoff);
      Assert(off >= 0 && off < LOBLKSIZE);

      getdatafield(data, &datafield, &len, &pfreeit);
      if (len > off)
      {
        n = len - off;
        n = (n <= (nbytes - nread)) ? n : (nbytes - nread);
        memcpy(buf + nread, VARDATA(datafield) + off, n);
        nread += n;
        obj_desc->offset += n;
      }
      if (pfreeit)
      {
        pfree(datafield);
      }
    }

    if (nread >= nbytes)
    {
      break;
    }
  }

  systable_endscan_ordered(sd);

  return nread;
}

int
inv_write(LargeObjectDesc *obj_desc, const char *buf, int nbytes)
{
  int nwritten = 0;
  int n;
  int off;
  int len;
  int32 pageno = (int32)(obj_desc->offset / LOBLKSIZE);
  ScanKeyData skey[2];
  SysScanDesc sd;
  HeapTuple oldtuple;
  Form_pg_largeobject olddata;
  bool neednextpage;
  bytea *datafield;
  bool pfreeit;
  union
  {
    bytea hdr;
                                                                   
    char data[LOBLKSIZE + VARHDRSZ];
                                              
    int32 align_it;
  } workbuf;
  char *workb = VARDATA(&workbuf.hdr);
  HeapTuple newtup;
  Datum values[Natts_pg_largeobject];
  bool nulls[Natts_pg_largeobject];
  bool replace[Natts_pg_largeobject];
  CatalogIndexState indstate;

  Assert(PointerIsValid(obj_desc));
  Assert(buf != NULL);

                                                                        
  if ((obj_desc->flags & IFS_WRLOCK) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for large object %u", obj_desc->id)));
  }

  if (nbytes <= 0)
  {
    return 0;
  }

                                                                 
  if ((nbytes + obj_desc->offset) > MAX_LARGE_OBJECT_SIZE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid large object write request size: %d", nbytes)));
  }

  open_lo_relation();

  indstate = CatalogOpenIndexes(lo_heap_r);

  ScanKeyInit(&skey[0], Anum_pg_largeobject_loid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(obj_desc->id));

  ScanKeyInit(&skey[1], Anum_pg_largeobject_pageno, BTGreaterEqualStrategyNumber, F_INT4GE, Int32GetDatum(pageno));

  sd = systable_beginscan_ordered(lo_heap_r, lo_index_r, obj_desc->snapshot, 2, skey);

  oldtuple = NULL;
  olddata = NULL;
  neednextpage = true;

  while (nwritten < nbytes)
  {
       
                                                                         
                                                                         
       
    if (neednextpage)
    {
      if ((oldtuple = systable_getnext_ordered(sd, ForwardScanDirection)) != NULL)
      {
        if (HeapTupleHasNulls(oldtuple))               
        {
          elog(ERROR, "null field found in pg_largeobject");
        }
        olddata = (Form_pg_largeobject)GETSTRUCT(oldtuple);
        Assert(olddata->pageno >= pageno);
      }
      neednextpage = false;
    }

       
                                                                        
                              
       
    if (olddata != NULL && olddata->pageno == pageno)
    {
         
                                                  
         
                                           
         
      getdatafield(olddata, &datafield, &len, &pfreeit);
      memcpy(workb, VARDATA(datafield), len);
      if (pfreeit)
      {
        pfree(datafield);
      }

         
                       
         
      off = (int)(obj_desc->offset % LOBLKSIZE);
      if (off > len)
      {
        MemSet(workb + len, 0, off - len);
      }

         
                                                
         
      n = LOBLKSIZE - off;
      n = (n <= (nbytes - nwritten)) ? n : (nbytes - nwritten);
      memcpy(workb + off, buf + nwritten, n);
      nwritten += n;
      obj_desc->offset += n;
      off += n;
                                            
      len = (len >= off) ? len : off;
      SET_VARSIZE(&workbuf.hdr, len + VARHDRSZ);

         
                                       
         
      memset(values, 0, sizeof(values));
      memset(nulls, false, sizeof(nulls));
      memset(replace, false, sizeof(replace));
      values[Anum_pg_largeobject_data - 1] = PointerGetDatum(&workbuf);
      replace[Anum_pg_largeobject_data - 1] = true;
      newtup = heap_modify_tuple(oldtuple, RelationGetDescr(lo_heap_r), values, nulls, replace);
      CatalogTupleUpdateWithInfo(lo_heap_r, &newtup->t_self, newtup, indstate);
      heap_freetuple(newtup);

         
                                        
         
      oldtuple = NULL;
      olddata = NULL;
      neednextpage = true;
    }
    else
    {
         
                                 
         
                              
         
      off = (int)(obj_desc->offset % LOBLKSIZE);
      if (off > 0)
      {
        MemSet(workb, 0, off);
      }

         
                                                
         
      n = LOBLKSIZE - off;
      n = (n <= (nbytes - nwritten)) ? n : (nbytes - nwritten);
      memcpy(workb + off, buf + nwritten, n);
      nwritten += n;
      obj_desc->offset += n;
                                            
      len = off + n;
      SET_VARSIZE(&workbuf.hdr, len + VARHDRSZ);

         
                                       
         
      memset(values, 0, sizeof(values));
      memset(nulls, false, sizeof(nulls));
      values[Anum_pg_largeobject_loid - 1] = ObjectIdGetDatum(obj_desc->id);
      values[Anum_pg_largeobject_pageno - 1] = Int32GetDatum(pageno);
      values[Anum_pg_largeobject_data - 1] = PointerGetDatum(&workbuf);
      newtup = heap_form_tuple(lo_heap_r->rd_att, values, nulls);
      CatalogTupleInsertWithInfo(lo_heap_r, newtup, indstate);
      heap_freetuple(newtup);
    }
    pageno++;
  }

  systable_endscan_ordered(sd);

  CatalogCloseIndexes(indstate);

     
                                                                            
                                                  
     
  CommandCounterIncrement();

  return nwritten;
}

void
inv_truncate(LargeObjectDesc *obj_desc, int64 len)
{
  int32 pageno = (int32)(len / LOBLKSIZE);
  int32 off;
  ScanKeyData skey[2];
  SysScanDesc sd;
  HeapTuple oldtuple;
  Form_pg_largeobject olddata;
  union
  {
    bytea hdr;
                                                                   
    char data[LOBLKSIZE + VARHDRSZ];
                                              
    int32 align_it;
  } workbuf;
  char *workb = VARDATA(&workbuf.hdr);
  HeapTuple newtup;
  Datum values[Natts_pg_largeobject];
  bool nulls[Natts_pg_largeobject];
  bool replace[Natts_pg_largeobject];
  CatalogIndexState indstate;

  Assert(PointerIsValid(obj_desc));

                                                                        
  if ((obj_desc->flags & IFS_WRLOCK) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for large object %u", obj_desc->id)));
  }

     
                                                                           
                                                                    
     
  if (len < 0 || len > MAX_LARGE_OBJECT_SIZE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg_internal("invalid large object truncation target: " INT64_FORMAT, len)));
  }

  open_lo_relation();

  indstate = CatalogOpenIndexes(lo_heap_r);

     
                                                                     
     
  ScanKeyInit(&skey[0], Anum_pg_largeobject_loid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(obj_desc->id));

  ScanKeyInit(&skey[1], Anum_pg_largeobject_pageno, BTGreaterEqualStrategyNumber, F_INT4GE, Int32GetDatum(pageno));

  sd = systable_beginscan_ordered(lo_heap_r, lo_index_r, obj_desc->snapshot, 2, skey);

     
                                                                          
                                                         
     
  olddata = NULL;
  if ((oldtuple = systable_getnext_ordered(sd, ForwardScanDirection)) != NULL)
  {
    if (HeapTupleHasNulls(oldtuple))               
    {
      elog(ERROR, "null field found in pg_largeobject");
    }
    olddata = (Form_pg_largeobject)GETSTRUCT(oldtuple);
    Assert(olddata->pageno >= pageno);
  }

     
                                                                          
                                                                            
                           
     
  if (olddata != NULL && olddata->pageno == pageno)
  {
                                           
    bytea *datafield;
    int pagelen;
    bool pfreeit;

    getdatafield(olddata, &datafield, &pagelen, &pfreeit);
    memcpy(workb, VARDATA(datafield), pagelen);
    if (pfreeit)
    {
      pfree(datafield);
    }

       
                     
       
    off = len % LOBLKSIZE;
    if (off > pagelen)
    {
      MemSet(workb + pagelen, 0, off - pagelen);
    }

                                    
    SET_VARSIZE(&workbuf.hdr, off + VARHDRSZ);

       
                                     
       
    memset(values, 0, sizeof(values));
    memset(nulls, false, sizeof(nulls));
    memset(replace, false, sizeof(replace));
    values[Anum_pg_largeobject_data - 1] = PointerGetDatum(&workbuf);
    replace[Anum_pg_largeobject_data - 1] = true;
    newtup = heap_modify_tuple(oldtuple, RelationGetDescr(lo_heap_r), values, nulls, replace);
    CatalogTupleUpdateWithInfo(lo_heap_r, &newtup->t_self, newtup, indstate);
    heap_freetuple(newtup);
  }
  else
  {
       
                                                                           
                                                                    
                                                    
       
    if (olddata != NULL)
    {
      Assert(olddata->pageno > pageno);
      CatalogTupleDelete(lo_heap_r, &oldtuple->t_self);
    }

       
                               
       
                                                
       
    off = len % LOBLKSIZE;
    if (off > 0)
    {
      MemSet(workb, 0, off);
    }

                                    
    SET_VARSIZE(&workbuf.hdr, off + VARHDRSZ);

       
                                 
       
    memset(values, 0, sizeof(values));
    memset(nulls, false, sizeof(nulls));
    values[Anum_pg_largeobject_loid - 1] = ObjectIdGetDatum(obj_desc->id);
    values[Anum_pg_largeobject_pageno - 1] = Int32GetDatum(pageno);
    values[Anum_pg_largeobject_data - 1] = PointerGetDatum(&workbuf);
    newtup = heap_form_tuple(lo_heap_r->rd_att, values, nulls);
    CatalogTupleInsertWithInfo(lo_heap_r, newtup, indstate);
    heap_freetuple(newtup);
  }

     
                                                                         
                                                                    
     
  if (olddata != NULL)
  {
    while ((oldtuple = systable_getnext_ordered(sd, ForwardScanDirection)) != NULL)
    {
      CatalogTupleDelete(lo_heap_r, &oldtuple->t_self);
    }
  }

  systable_endscan_ordered(sd);

  CatalogCloseIndexes(indstate);

     
                                                                         
                                                  
     
  CommandCounterIncrement();
}
