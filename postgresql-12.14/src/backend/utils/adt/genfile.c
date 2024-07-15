                                                                            
   
             
                                         
   
   
                                                                
   
                                                     
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "access/htup_details.h"
#include "access/xlog_internal.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace_d.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

   
                                                                             
   
                                                                          
                                                       
   
                                                                           
                                                                              
                                                                           
                                                                          
                                                                         
   
static char *
convert_and_check_filename(text *arg)
{
  char *filename;

  filename = text_to_cstring(arg);
  canonicalize_path(filename);                                      

     
                                                                          
                                                                             
           
     
  if (is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_SERVER_FILES))
  {
    return filename;
  }

                                                                           
  if (is_absolute_path(filename))
  {
                                 
    if (path_contains_parent_reference(filename))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("reference to parent directory (\"..\") not allowed"))));
    }

       
                                                                     
                                                      
       
    if (!path_is_prefix_of_path(DataDir, filename) && (!is_absolute_path(Log_directory) || !path_is_prefix_of_path(Log_directory, filename)))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("absolute path not allowed"))));
    }
  }
  else if (!path_is_relative_and_below_cwd(filename))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("path must be in or below the current directory"))));
  }

  return filename;
}

   
                                                   
   
                                                       
   
                                                                 
   
static bytea *
read_binary_file(const char *filename, int64 seek_offset, int64 bytes_to_read, bool missing_ok)
{
  bytea *buf;
  size_t nbytes = 0;
  FILE *file;

                                                          
  if (bytes_to_read > (int64)(MaxAllocSize - VARHDRSZ))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("requested length too large")));
  }

  if ((file = AllocateFile(filename, PG_BINARY_R)) == NULL)
  {
    if (missing_ok && errno == ENOENT)
    {
      return NULL;
    }
    else
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\" for reading: %m", filename)));
    }
  }

  if (fseeko(file, (off_t)seek_offset, (seek_offset >= 0) ? SEEK_SET : SEEK_END) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in file \"%s\": %m", filename)));
  }

  if (bytes_to_read >= 0)
  {
                                                 
    buf = (bytea *)palloc((Size)bytes_to_read + VARHDRSZ);

    nbytes = fread(VARDATA(buf), 1, (size_t)bytes_to_read, file);
  }
  else
  {
                                               
    StringInfoData sbuf;

    initStringInfo(&sbuf);
                                                              
    sbuf.len += VARHDRSZ;
    Assert(sbuf.len < sbuf.maxlen);

    while (!(feof(file) || ferror(file)))
    {
      size_t rbytes;

                                            
#define MIN_READ_SIZE 4096

         
                                                         
                                                                 
                                                                 
                                                                  
                                                                   
                             
         
      if (sbuf.len == MaxAllocSize - 1)
      {
        char rbuf[1];

        if (fread(rbuf, 1, 1, file) != 0 || !feof(file))
        {
          ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("file length too large")));
        }
        else
        {
          break;
        }
      }

                                                              
      enlargeStringInfo(&sbuf, MIN_READ_SIZE);

         
                                                                       
                                                                       
                                                                    
         
      rbytes = fread(sbuf.data + sbuf.len, 1, (size_t)(sbuf.maxlen - sbuf.len - 1), file);
      sbuf.len += rbytes;
      nbytes += rbytes;
    }

                                                                     
    buf = (bytea *)sbuf.data;
  }

  if (ferror(file))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", filename)));
  }

  SET_VARSIZE(buf, nbytes + VARHDRSZ);

  FreeFile(file);

  return buf;
}

   
                                                                          
                             
   
static text *
read_text_file(const char *filename, int64 seek_offset, int64 bytes_to_read, bool missing_ok)
{
  bytea *buf;

  buf = read_binary_file(filename, seek_offset, bytes_to_read, missing_ok);

  if (buf != NULL)
  {
                                      
    pg_verifymbstr(VARDATA(buf), VARSIZE(buf) - VARHDRSZ, false);

                                           
    return (text *)buf;
  }
  else
  {
    return NULL;
  }
}

   
                                                  
   
                                                   
   
Datum
pg_read_file(PG_FUNCTION_ARGS)
{
  text *filename_t = PG_GETARG_TEXT_PP(0);
  int64 seek_offset = 0;
  int64 bytes_to_read = -1;
  bool missing_ok = false;
  char *filename;
  text *result;

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser to read files with adminpack 1.0"),
                                                                                                            
                                                                 errhint("Consider using %s, which is part of core, instead.", "pg_read_file()"))));
  }

                                 
  if (PG_NARGS() >= 3)
  {
    seek_offset = PG_GETARG_INT64(1);
    bytes_to_read = PG_GETARG_INT64(2);

    if (bytes_to_read < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("requested length cannot be negative")));
    }
  }
  if (PG_NARGS() >= 4)
  {
    missing_ok = PG_GETARG_BOOL(3);
  }

  filename = convert_and_check_filename(filename_t);

  result = read_text_file(filename, seek_offset, bytes_to_read, missing_ok);
  if (result)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                  
   
                                                                       
                 
   
Datum
pg_read_file_v2(PG_FUNCTION_ARGS)
{
  text *filename_t = PG_GETARG_TEXT_PP(0);
  int64 seek_offset = 0;
  int64 bytes_to_read = -1;
  bool missing_ok = false;
  char *filename;
  text *result;

                                 
  if (PG_NARGS() >= 3)
  {
    seek_offset = PG_GETARG_INT64(1);
    bytes_to_read = PG_GETARG_INT64(2);

    if (bytes_to_read < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("requested length cannot be negative")));
    }
  }
  if (PG_NARGS() >= 4)
  {
    missing_ok = PG_GETARG_BOOL(3);
  }

  filename = convert_and_check_filename(filename_t);

  result = read_text_file(filename, seek_offset, bytes_to_read, missing_ok);
  if (result)
  {
    PG_RETURN_TEXT_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                   
   
Datum
pg_read_binary_file(PG_FUNCTION_ARGS)
{
  text *filename_t = PG_GETARG_TEXT_PP(0);
  int64 seek_offset = 0;
  int64 bytes_to_read = -1;
  bool missing_ok = false;
  char *filename;
  bytea *result;

                                 
  if (PG_NARGS() >= 3)
  {
    seek_offset = PG_GETARG_INT64(1);
    bytes_to_read = PG_GETARG_INT64(2);

    if (bytes_to_read < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("requested length cannot be negative")));
    }
  }
  if (PG_NARGS() >= 4)
  {
    missing_ok = PG_GETARG_BOOL(3);
  }

  filename = convert_and_check_filename(filename_t);

  result = read_binary_file(filename, seek_offset, bytes_to_read, missing_ok);
  if (result)
  {
    PG_RETURN_BYTEA_P(result);
  }
  else
  {
    PG_RETURN_NULL();
  }
}

   
                                                                            
                              
   
                                                                            
                                                                           
                                 
   
Datum
pg_read_file_off_len(PG_FUNCTION_ARGS)
{
  return pg_read_file_v2(fcinfo);
}

Datum
pg_read_file_all(PG_FUNCTION_ARGS)
{
  return pg_read_file_v2(fcinfo);
}

Datum
pg_read_binary_file_off_len(PG_FUNCTION_ARGS)
{
  return pg_read_binary_file(fcinfo);
}

Datum
pg_read_binary_file_all(PG_FUNCTION_ARGS)
{
  return pg_read_binary_file(fcinfo);
}

   
               
   
Datum
pg_stat_file(PG_FUNCTION_ARGS)
{
  text *filename_t = PG_GETARG_TEXT_PP(0);
  char *filename;
  struct stat fst;
  Datum values[6];
  bool isnull[6];
  HeapTuple tuple;
  TupleDesc tupdesc;
  bool missing_ok = false;

                                   
  if (PG_NARGS() == 2)
  {
    missing_ok = PG_GETARG_BOOL(1);
  }

  filename = convert_and_check_filename(filename_t);

  if (stat(filename, &fst) < 0)
  {
    if (missing_ok && errno == ENOENT)
    {
      PG_RETURN_NULL();
    }
    else
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", filename)));
    }
  }

     
                                                                             
                   
     
  tupdesc = CreateTemplateTupleDesc(6);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "size", INT8OID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "access", TIMESTAMPTZOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)3, "modification", TIMESTAMPTZOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)4, "change", TIMESTAMPTZOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)5, "creation", TIMESTAMPTZOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)6, "isdir", BOOLOID, -1, 0);
  BlessTupleDesc(tupdesc);

  memset(isnull, false, sizeof(isnull));

  values[0] = Int64GetDatum((int64)fst.st_size);
  values[1] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_atime));
  values[2] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_mtime));
                                                                       
#if !defined(WIN32) && !defined(__CYGWIN__)
  values[3] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_ctime));
  isnull[4] = true;
#else
  isnull[3] = true;
  values[4] = TimestampTzGetDatum(time_t_to_timestamptz(fst.st_ctime));
#endif
  values[5] = BoolGetDatum(S_ISDIR(fst.st_mode));

  tuple = heap_form_tuple(tupdesc, values, isnull);

  pfree(filename);

  PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

   
                                    
   
                                                                           
                                                                          
                                              
   
Datum
pg_stat_file_1arg(PG_FUNCTION_ARGS)
{
  return pg_stat_file(fcinfo);
}

   
                                                 
   
Datum
pg_ls_dir(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  char *location;
  bool missing_ok = false;
  bool include_dot_dirs = false;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  DIR *dirdesc;
  struct dirent *de;
  MemoryContext oldcontext;

  location = convert_and_check_filename(PG_GETARG_TEXT_PP(0));

                                    
  if (PG_NARGS() == 3)
  {
    if (!PG_ARGISNULL(1))
    {
      missing_ok = PG_GETARG_BOOL(1);
    }
    if (!PG_ARGISNULL(2))
    {
      include_dot_dirs = PG_GETARG_BOOL(2);
    }
  }

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("materialize mode required, but it is not allowed in this context")));
  }

                                                                           
  oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

  tupdesc = CreateTemplateTupleDesc(1);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "pg_ls_dir", TEXTOID, -1, 0);

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  dirdesc = AllocateDir(location);
  if (!dirdesc)
  {
                                                
    if (missing_ok && errno == ENOENT)
    {
      return (Datum)0;
    }
                                                         
  }

  while ((de = ReadDir(dirdesc, location)) != NULL)
  {
    Datum values[1];
    bool nulls[1];

    if (!include_dot_dirs && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0))
    {
      continue;
    }

    values[0] = CStringGetTextDatum(de->d_name);
    nulls[0] = false;

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  FreeDir(dirdesc);
  return (Datum)0;
}

   
                                         
   
                                                                           
                                                                          
                                               
   
Datum
pg_ls_dir_1arg(PG_FUNCTION_ARGS)
{
  return pg_ls_dir(fcinfo);
}

   
                                                            
   
                                                                             
                                                    
   
static Datum
pg_ls_dir_files(FunctionCallInfo fcinfo, const char *dir, bool missing_ok)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  DIR *dirdesc;
  struct dirent *de;
  MemoryContext oldcontext;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("materialize mode required, but it is not allowed in this context")));
  }

                                                                           
  oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

     
                                                                            
                                                                        
                                                     
     
  dirdesc = AllocateDir(dir);
  if (!dirdesc)
  {
                                                
    if (missing_ok && errno == ENOENT)
    {
      return (Datum)0;
    }
                                                         
  }

  while ((de = ReadDir(dirdesc, dir)) != NULL)
  {
    Datum values[3];
    bool nulls[3];
    char path[MAXPGPATH * 2];
    struct stat attrib;

                           
    if (de->d_name[0] == '.')
    {
      continue;
    }

                           
    snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
    if (stat(path, &attrib) < 0)
    {
                                                            
      if (errno == ENOENT)
      {
        continue;
      }
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", path)));
    }

                                           
    if (!S_ISREG(attrib.st_mode))
    {
      continue;
    }

    values[0] = CStringGetTextDatum(de->d_name);
    values[1] = Int64GetDatum((int64)attrib.st_size);
    values[2] = TimestampTzGetDatum(time_t_to_timestamptz(attrib.st_mtime));
    memset(nulls, 0, sizeof(nulls));

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  FreeDir(dirdesc);
  return (Datum)0;
}

                                                               
Datum
pg_ls_logdir(PG_FUNCTION_ARGS)
{
  return pg_ls_dir_files(fcinfo, Log_directory, false);
}

                                                               
Datum
pg_ls_waldir(PG_FUNCTION_ARGS)
{
  return pg_ls_dir_files(fcinfo, XLOGDIR, false);
}

   
                                                             
   
static Datum
pg_ls_tmpdir(FunctionCallInfo fcinfo, Oid tblspc)
{
  char path[MAXPGPATH];

  if (!SearchSysCacheExists1(TABLESPACEOID, ObjectIdGetDatum(tblspc)))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace with OID %u does not exist", tblspc)));
  }

  TempTablespacePath(path, tblspc);
  return pg_ls_dir_files(fcinfo, path, true);
}

   
                                                                                 
                       
   
Datum
pg_ls_tmpdir_noargs(PG_FUNCTION_ARGS)
{
  return pg_ls_tmpdir(fcinfo, DEFAULTTABLESPACE_OID);
}

   
                                                                                
                       
   
Datum
pg_ls_tmpdir_1arg(PG_FUNCTION_ARGS)
{
  return pg_ls_tmpdir(fcinfo, PG_GETARG_OID(0));
}

   
                                                                             
   
Datum
pg_ls_archive_statusdir(PG_FUNCTION_ARGS)
{
  return pg_ls_dir_files(fcinfo, XLOGDIR "/archive_status", true);
}
