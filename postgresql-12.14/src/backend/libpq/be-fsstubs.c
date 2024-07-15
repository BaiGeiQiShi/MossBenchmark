                                                                            
   
                
                                                                             
   
                                                                         
                                                                        
   
   
                  
                                    
   
         
                                                                   
                                 
   
                                                                               
                                                                            
                                                                               
                                                                              
                                                                           
                                                                          
                                                                             
                                                                        
                                                                          
                                                                          
                                                                         
                  
   
                                                                          
                                                                       
                                                                              
                                                                          
                 
   
                                                                            
   

#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access/xact.h"
#include "libpq/be-fsstubs.h"
#include "libpq/libpq-fs.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "storage/large_object.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

                                         
                    
                                                  
#define BUFSIZE 8192

   
                                                
   
                                                                       
                                                                        
                                                                         
                                                                   
   
static LargeObjectDesc **cookies = NULL;
static int cookies_size = 0;

static bool lo_cleanup_needed = false;
static MemoryContext fscxt = NULL;

static int
newLOfd(void);
static void
closeLOfd(int fd);
static Oid
lo_import_internal(text *filename, Oid lobjOid);

                                                                               
                                     
                                                                               

Datum
be_lo_open(PG_FUNCTION_ARGS)
{
  Oid lobjId = PG_GETARG_OID(0);
  int32 mode = PG_GETARG_INT32(1);
  LargeObjectDesc *lobjDesc;
  int fd;

#if FSDB
  elog(DEBUG4, "lo_open(%u,%d)", lobjId, mode);
#endif

     
                                                                      
                                                                 
     
  fd = newLOfd();

  lobjDesc = inv_open(lobjId, mode, fscxt);
  lobjDesc->subid = GetCurrentSubTransactionId();

     
                                                                           
                                                                             
                 
     
  if (lobjDesc->snapshot)
  {
    lobjDesc->snapshot = RegisterSnapshotOnOwner(lobjDesc->snapshot, TopTransactionResourceOwner);
  }

  Assert(cookies[fd] == NULL);
  cookies[fd] = lobjDesc;

  PG_RETURN_INT32(fd);
}

Datum
be_lo_close(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }

#if FSDB
  elog(DEBUG4, "lo_close(%d)", fd);
#endif

  closeLOfd(fd);

  PG_RETURN_INT32(0);
}

                                                                               
                                                               
   
                                                                        
                            
   
                                                                               

int
lo_read(int fd, char *buf, int len)
{
  int status;
  LargeObjectDesc *lobj;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }
  lobj = cookies[fd];

     
                                                                           
                                                                             
                                                                        
     
  if ((lobj->flags & IFS_RDLOCK) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("large object descriptor %d was not opened for reading", fd)));
  }

  status = inv_read(lobj, buf, len);

  return status;
}

int
lo_write(int fd, const char *buf, int len)
{
  int status;
  LargeObjectDesc *lobj;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }
  lobj = cookies[fd];

                                
  if ((lobj->flags & IFS_WRLOCK) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("large object descriptor %d was not opened for writing", fd)));
  }

  status = inv_write(lobj, buf, len);

  return status;
}

Datum
be_lo_lseek(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int32 offset = PG_GETARG_INT32(1);
  int32 whence = PG_GETARG_INT32(2);
  int64 status;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }

  status = inv_seek(cookies[fd], offset, whence);

                                     
  if (status != (int32)status)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("lo_lseek result out of range for large-object descriptor %d", fd)));
  }

  PG_RETURN_INT32((int32)status);
}

Datum
be_lo_lseek64(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int64 offset = PG_GETARG_INT64(1);
  int32 whence = PG_GETARG_INT32(2);
  int64 status;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }

  status = inv_seek(cookies[fd], offset, whence);

  PG_RETURN_INT64(status);
}

Datum
be_lo_creat(PG_FUNCTION_ARGS)
{
  Oid lobjId;

  lo_cleanup_needed = true;
  lobjId = inv_create(InvalidOid);

  PG_RETURN_OID(lobjId);
}

Datum
be_lo_create(PG_FUNCTION_ARGS)
{
  Oid lobjId = PG_GETARG_OID(0);

  lo_cleanup_needed = true;
  lobjId = inv_create(lobjId);

  PG_RETURN_OID(lobjId);
}

Datum
be_lo_tell(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int64 offset;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }

  offset = inv_tell(cookies[fd]);

                                     
  if (offset != (int32)offset)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("lo_tell result out of range for large-object descriptor %d", fd)));
  }

  PG_RETURN_INT32((int32)offset);
}

Datum
be_lo_tell64(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int64 offset;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }

  offset = inv_tell(cookies[fd]);

  PG_RETURN_INT64(offset);
}

Datum
be_lo_unlink(PG_FUNCTION_ARGS)
{
  Oid lobjId = PG_GETARG_OID(0);

     
                                                                           
                                                                            
                   
     
  if (!lo_compat_privileges && !pg_largeobject_ownercheck(lobjId, GetUserId()))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be owner of large object %u", lobjId)));
  }

     
                                                                  
     
  if (fscxt != NULL)
  {
    int i;

    for (i = 0; i < cookies_size; i++)
    {
      if (cookies[i] != NULL && cookies[i]->id == lobjId)
      {
        closeLOfd(i);
      }
    }
  }

     
                                                                        
                                                   
     
  PG_RETURN_INT32(inv_drop(lobjId));
}

                                                                               
                          
                                                                               

Datum
be_loread(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int32 len = PG_GETARG_INT32(1);
  bytea *retval;
  int totalread;

  if (len < 0)
  {
    len = 0;
  }

  retval = (bytea *)palloc(VARHDRSZ + len);
  totalread = lo_read(fd, VARDATA(retval), len);
  SET_VARSIZE(retval, totalread + VARHDRSZ);

  PG_RETURN_BYTEA_P(retval);
}

Datum
be_lowrite(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  bytea *wbuf = PG_GETARG_BYTEA_PP(1);
  int bytestowrite;
  int totalwritten;

  bytestowrite = VARSIZE_ANY_EXHDR(wbuf);
  totalwritten = lo_write(fd, VARDATA_ANY(wbuf), bytestowrite);
  PG_RETURN_INT32(totalwritten);
}

                                                                               
                                  
                                                                               

   
               
                                                    
   
Datum
be_lo_import(PG_FUNCTION_ARGS)
{
  text *filename = PG_GETARG_TEXT_PP(0);

  PG_RETURN_OID(lo_import_internal(filename, InvalidOid));
}

   
                        
                                                                   
   
Datum
be_lo_import_with_oid(PG_FUNCTION_ARGS)
{
  text *filename = PG_GETARG_TEXT_PP(0);
  Oid oid = PG_GETARG_OID(1);

  PG_RETURN_OID(lo_import_internal(filename, oid));
}

static Oid
lo_import_internal(text *filename, Oid lobjOid)
{
  int fd;
  int nbytes, tmp PG_USED_FOR_ASSERTS_ONLY;
  char buf[BUFSIZE];
  char fnamebuf[MAXPGPATH];
  LargeObjectDesc *lobj;
  Oid oid;

     
                                 
     
  text_to_cstring_buffer(filename, fnamebuf, sizeof(fnamebuf));
  fd = OpenTransientFile(fnamebuf, O_RDONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open server file \"%s\": %m", fnamebuf)));
  }

     
                                
     
  lo_cleanup_needed = true;
  oid = inv_create(lobjOid);

     
                                                                   
     
  lobj = inv_open(oid, INV_WRITE, CurrentMemoryContext);

  while ((nbytes = read(fd, buf, BUFSIZE)) > 0)
  {
    tmp = inv_write(lobj, buf, nbytes);
    Assert(tmp == nbytes);
  }

  if (nbytes < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read server file \"%s\": %m", fnamebuf)));
  }

  inv_close(lobj);

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", fnamebuf)));
  }

  return oid;
}

   
               
                                          
   
Datum
be_lo_export(PG_FUNCTION_ARGS)
{
  Oid lobjId = PG_GETARG_OID(0);
  text *filename = PG_GETARG_TEXT_PP(1);
  int fd;
  int nbytes, tmp;
  char buf[BUFSIZE];
  char fnamebuf[MAXPGPATH];
  LargeObjectDesc *lobj;
  mode_t oumask;

     
                                                             
     
  lo_cleanup_needed = true;
  lobj = inv_open(lobjId, INV_READ, CurrentMemoryContext);

     
                                    
     
                                                                           
                                                                   
                                                    
     
  text_to_cstring_buffer(filename, fnamebuf, sizeof(fnamebuf));
  oumask = umask(S_IWGRP | S_IWOTH);
  PG_TRY();
  {
    fd = OpenTransientFilePerm(fnamebuf, O_CREAT | O_WRONLY | O_TRUNC | PG_BINARY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  }
  PG_CATCH();
  {
    umask(oumask);
    PG_RE_THROW();
  }
  PG_END_TRY();
  umask(oumask);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create server file \"%s\": %m", fnamebuf)));
  }

     
                                                                 
     
  while ((nbytes = inv_read(lobj, buf, BUFSIZE)) > 0)
  {
    tmp = write(fd, buf, nbytes);
    if (tmp != nbytes)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not write server file \"%s\": %m", fnamebuf)));
    }
  }

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", fnamebuf)));
  }

  inv_close(lobj);

  PG_RETURN_INT32(1);
}

   
                 
                                                   
   
static void
lo_truncate_internal(int32 fd, int64 len)
{
  LargeObjectDesc *lobj;

  if (fd < 0 || fd >= cookies_size || cookies[fd] == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("invalid large-object descriptor: %d", fd)));
  }
  lobj = cookies[fd];

                                
  if ((lobj->flags & IFS_WRLOCK) == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("large object descriptor %d was not opened for writing", fd)));
  }

  inv_truncate(lobj, len);
}

Datum
be_lo_truncate(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int32 len = PG_GETARG_INT32(1);

  lo_truncate_internal(fd, len);
  PG_RETURN_INT32(0);
}

Datum
be_lo_truncate64(PG_FUNCTION_ARGS)
{
  int32 fd = PG_GETARG_INT32(0);
  int64 len = PG_GETARG_INT64(1);

  lo_truncate_internal(fd, len);
  PG_RETURN_INT32(0);
}

   
                          
                                                   
   
void
AtEOXact_LargeObject(bool isCommit)
{
  int i;

  if (!lo_cleanup_needed)
  {
    return;                                    
  }

     
                                                                             
                                                                          
                                                                            
                                                                            
                
     
  if (isCommit)
  {
    for (i = 0; i < cookies_size; i++)
    {
      if (cookies[i] != NULL)
      {
        closeLOfd(i);
      }
    }
  }

                                                               
  cookies = NULL;
  cookies_size = 0;

                                                                        
  if (fscxt)
  {
    MemoryContextDelete(fscxt);
  }
  fscxt = NULL;

                                                
  close_lo_relation(isCommit);

  lo_cleanup_needed = false;
}

   
                           
                                                              
   
                                                                  
                                                             
   
void
AtEOSubXact_LargeObject(bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid)
{
  int i;

  if (fscxt == NULL)                                    
  {
    return;
  }

  for (i = 0; i < cookies_size; i++)
  {
    LargeObjectDesc *lo = cookies[i];

    if (lo != NULL && lo->subid == mySubid)
    {
      if (isCommit)
      {
        lo->subid = parentSubid;
      }
      else
      {
        closeLOfd(i);
      }
    }
  }
}

                                                                               
                                  
                                                                               

static int
newLOfd(void)
{
  int i, newsize;

  lo_cleanup_needed = true;
  if (fscxt == NULL)
  {
    fscxt = AllocSetContextCreate(TopMemoryContext, "Filesystem", ALLOCSET_DEFAULT_SIZES);
  }

                               
  for (i = 0; i < cookies_size; i++)
  {
    if (cookies[i] == NULL)
    {
      return i;
    }
  }

                                              
  if (cookies_size <= 0)
  {
                                                               
    i = 0;
    newsize = 64;
    cookies = (LargeObjectDesc **)MemoryContextAllocZero(fscxt, newsize * sizeof(LargeObjectDesc *));
    cookies_size = newsize;
  }
  else
  {
                              
    i = cookies_size;
    newsize = cookies_size * 2;
    cookies = (LargeObjectDesc **)repalloc(cookies, newsize * sizeof(LargeObjectDesc *));
    MemSet(cookies + cookies_size, 0, (newsize - cookies_size) * sizeof(LargeObjectDesc *));
    cookies_size = newsize;
  }

  return i;
}

static void
closeLOfd(int fd)
{
  LargeObjectDesc *lobj;

     
                                                                       
                                          
     
  lobj = cookies[fd];
  cookies[fd] = NULL;

  if (lobj->snapshot)
  {
    UnregisterSnapshotFromOwner(lobj->snapshot, TopTransactionResourceOwner);
  }
  inv_close(lobj);
}

                                                                               
                                        
                                                                               

   
                                                                           
   
static bytea *
lo_get_fragment_internal(Oid loOid, int64 offset, int32 nbytes)
{
  LargeObjectDesc *loDesc;
  int64 loSize;
  int64 result_length;
  int total_read PG_USED_FOR_ASSERTS_ONLY;
  bytea *result = NULL;

  lo_cleanup_needed = true;
  loDesc = inv_open(loOid, INV_READ, CurrentMemoryContext);

     
                                                                             
                                         
     
  loSize = inv_seek(loDesc, 0, SEEK_END);
  if (loSize > offset)
  {
    if (nbytes >= 0 && nbytes <= loSize - offset)
    {
      result_length = nbytes;                                  
    }
    else
    {
      result_length = loSize - offset;                          
    }
  }
  else
  {
    result_length = 0;                                   
  }

     
                                                                            
                                                                            
     
  if (result_length > MaxAllocSize - VARHDRSZ)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("large object read request is too large")));
  }

  result = (bytea *)palloc(VARHDRSZ + result_length);

  inv_seek(loDesc, offset, SEEK_SET);
  total_read = inv_read(loDesc, VARDATA(result), result_length);
  Assert(total_read == result_length);
  SET_VARSIZE(result, result_length + VARHDRSZ);

  inv_close(loDesc);

  return result;
}

   
                  
   
Datum
be_lo_get(PG_FUNCTION_ARGS)
{
  Oid loOid = PG_GETARG_OID(0);
  bytea *result;

  result = lo_get_fragment_internal(loOid, 0, -1);

  PG_RETURN_BYTEA_P(result);
}

   
                        
   
Datum
be_lo_get_fragment(PG_FUNCTION_ARGS)
{
  Oid loOid = PG_GETARG_OID(0);
  int64 offset = PG_GETARG_INT64(1);
  int32 nbytes = PG_GETARG_INT32(2);
  bytea *result;

  if (nbytes < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("requested length cannot be negative")));
  }

  result = lo_get_fragment_internal(loOid, offset, nbytes);

  PG_RETURN_BYTEA_P(result);
}

   
                                                             
   
Datum
be_lo_from_bytea(PG_FUNCTION_ARGS)
{
  Oid loOid = PG_GETARG_OID(0);
  bytea *str = PG_GETARG_BYTEA_PP(1);
  LargeObjectDesc *loDesc;
  int written PG_USED_FOR_ASSERTS_ONLY;

  lo_cleanup_needed = true;
  loOid = inv_create(loOid);
  loDesc = inv_open(loOid, INV_WRITE, CurrentMemoryContext);
  written = inv_write(loDesc, VARDATA_ANY(str), VARSIZE_ANY_EXHDR(str));
  Assert(written == VARSIZE_ANY_EXHDR(str));
  inv_close(loDesc);

  PG_RETURN_OID(loOid);
}

   
                          
   
Datum
be_lo_put(PG_FUNCTION_ARGS)
{
  Oid loOid = PG_GETARG_OID(0);
  int64 offset = PG_GETARG_INT64(1);
  bytea *str = PG_GETARG_BYTEA_PP(2);
  LargeObjectDesc *loDesc;
  int written PG_USED_FOR_ASSERTS_ONLY;

  lo_cleanup_needed = true;
  loDesc = inv_open(loOid, INV_WRITE, CurrentMemoryContext);

                        
  if (!lo_compat_privileges && pg_largeobject_aclcheck_snapshot(loDesc->id, GetUserId(), ACL_UPDATE, loDesc->snapshot) != ACLCHECK_OK)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for large object %u", loDesc->id)));
  }

  inv_seek(loDesc, offset, SEEK_SET);
  written = inv_write(loDesc, VARDATA_ANY(str), VARSIZE_ANY_EXHDR(str));
  Assert(written == VARSIZE_ANY_EXHDR(str));
  inv_close(loDesc);

  PG_RETURN_VOID();
}
