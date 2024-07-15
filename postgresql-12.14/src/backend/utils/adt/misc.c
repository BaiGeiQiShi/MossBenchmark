                                                                            
   
          
   
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/tablespace.h"
#include "common/keywords.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "parser/scansup.h"
#include "postmaster/syslogger.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "utils/lsyscache.h"
#include "utils/ruleutils.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

   
                                                         
                                                                     
                                                               
                                    
   
static bool
count_nulls(FunctionCallInfo fcinfo, int32 *nargs, int32 *nulls)
{
  int32 count = 0;
  int i;

                                                                    
  if (get_fn_expr_variadic(fcinfo->flinfo))
  {
    ArrayType *arr;
    int ndims, nitems, *dims;
    bits8 *bitmap;

    Assert(PG_NARGS() == 1);

       
                                                                          
                                                                           
                                                                          
       
    if (PG_ARGISNULL(0))
    {
      return false;
    }

       
                                                                          
                                                                         
                                                                           
                                                                       
                                         
       
    Assert(OidIsValid(get_base_element_type(get_fn_expr_argtype(fcinfo->flinfo, 0))));

                                           
    arr = PG_GETARG_ARRAYTYPE_P(0);

                                  
    ndims = ARR_NDIM(arr);
    dims = ARR_DIMS(arr);
    nitems = ArrayGetNItems(ndims, dims);

                                   
    bitmap = ARR_NULLBITMAP(arr);
    if (bitmap)
    {
      int bitmask = 1;

      for (i = 0; i < nitems; i++)
      {
        if ((*bitmap & bitmask) == 0)
        {
          count++;
        }

        bitmask <<= 1;
        if (bitmask == 0x100)
        {
          bitmap++;
          bitmask = 1;
        }
      }
    }

    *nargs = nitems;
    *nulls = count;
  }
  else
  {
                                               
    for (i = 0; i < PG_NARGS(); i++)
    {
      if (PG_ARGISNULL(i))
      {
        count++;
      }
    }

    *nargs = PG_NARGS();
    *nulls = count;
  }

  return true;
}

   
               
                                      
   
Datum
pg_num_nulls(PG_FUNCTION_ARGS)
{
  int32 nargs, nulls;

  if (!count_nulls(fcinfo, &nargs, &nulls))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT32(nulls);
}

   
                  
                                          
   
Datum
pg_num_nonnulls(PG_FUNCTION_ARGS)
{
  int32 nargs, nulls;

  if (!count_nulls(fcinfo, &nargs, &nulls))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT32(nargs - nulls);
}

   
                      
                                           
   
Datum
current_database(PG_FUNCTION_ARGS)
{
  Name db;

  db = (Name)palloc(NAMEDATALEN);

  namestrcpy(db, get_database_name(MyDatabaseId));
  PG_RETURN_NAME(db);
}

   
                   
                                                                      
                                                          
   
Datum
current_query(PG_FUNCTION_ARGS)
{
                                                                      
  if (debug_query_string)
  {
    PG_RETURN_TEXT_P(cstring_to_text(debug_query_string));
  }
  else
  {
    PG_RETURN_NULL();
  }
}

                                                                   

Datum
pg_tablespace_databases(PG_FUNCTION_ARGS)
{
  Oid tablespaceOid = PG_GETARG_OID(0);
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  char *location;
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

  tupdesc = CreateTemplateTupleDesc(1);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "pg_tablespace_databases", OIDOID, -1, 0);

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);

  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  if (tablespaceOid == GLOBALTABLESPACE_OID)
  {
    ereport(WARNING, (errmsg("global tablespace never has databases")));
                                 
    return (Datum)0;
  }

  if (tablespaceOid == DEFAULTTABLESPACE_OID)
  {
    location = psprintf("base");
  }
  else
  {
    location = psprintf("pg_tblspc/%u/%s", tablespaceOid, TABLESPACE_VERSION_DIRECTORY);
  }

  dirdesc = AllocateDir(location);

  if (!dirdesc)
  {
                                           
    if (errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", location)));
    }
    ereport(WARNING, (errmsg("%u is not a tablespace OID", tablespaceOid)));
                                 
    return (Datum)0;
  }

  while ((de = ReadDir(dirdesc, location)) != NULL)
  {
    Oid datOid = atooid(de->d_name);
    char *subdir;
    bool isempty;
    Datum values[1];
    bool nulls[1];

                                                       
    if (!datOid)
    {
      continue;
    }

                                                                      

    subdir = psprintf("%s/%s", location, de->d_name);
    isempty = directory_is_empty(subdir);
    pfree(subdir);

    if (isempty)
    {
      continue;                            
    }

    values[0] = ObjectIdGetDatum(datOid);
    nulls[0] = false;

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  FreeDir(dirdesc);
  return (Datum)0;
}

   
                                                          
   
Datum
pg_tablespace_location(PG_FUNCTION_ARGS)
{
  Oid tablespaceOid = PG_GETARG_OID(0);
  char sourcepath[MAXPGPATH];
  char targetpath[MAXPGPATH];
  int rllen;
#ifndef WIN32
  struct stat st;
#endif

     
                                                                           
                                                                      
                                                                           
     
  if (tablespaceOid == InvalidOid)
  {
    tablespaceOid = MyDatabaseTableSpace;
  }

     
                                                               
     
  if (tablespaceOid == DEFAULTTABLESPACE_OID || tablespaceOid == GLOBALTABLESPACE_OID)
  {
    PG_RETURN_TEXT_P(cstring_to_text(""));
  }

#if defined(HAVE_READLINK) || defined(WIN32)

     
                                                                           
                            
     
  snprintf(sourcepath, sizeof(sourcepath), "pg_tblspc/%u", tablespaceOid);

     
                                                                      
                                                                         
                                                                         
                                                               
     
#ifdef WIN32
  if (!pgwin32_is_junction(sourcepath))
  {
    PG_RETURN_TEXT_P(cstring_to_text(sourcepath));
  }
#else
  if (lstat(sourcepath, &st) < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", sourcepath)));
  }

  if (!S_ISLNK(st.st_mode))
  {
    PG_RETURN_TEXT_P(cstring_to_text(sourcepath));
  }
#endif

     
                                                                             
     
  rllen = readlink(sourcepath, targetpath, sizeof(targetpath));
  if (rllen < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read symbolic link \"%s\": %m", sourcepath)));
  }
  if (rllen >= sizeof(targetpath))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("symbolic link \"%s\" target is too long", sourcepath)));
  }
  targetpath[rllen] = '\0';

  PG_RETURN_TEXT_P(cstring_to_text(targetpath));
#else
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("tablespaces are not supported on this platform")));
  PG_RETURN_NULL();
#endif
}

   
                                  
   
Datum
pg_sleep(PG_FUNCTION_ARGS)
{
  float8 secs = PG_GETARG_FLOAT8(0);
  float8 endtime;

     
                                                                           
                                                                    
                                                                            
                                                                            
           
     
                                                                             
                                                                           
                                                                          
                                                
     
#define GetNowFloat() ((float8)GetCurrentTimestamp() / 1000000.0)

  endtime = GetNowFloat() + secs;

  for (;;)
  {
    float8 delay;
    long delay_ms;

    CHECK_FOR_INTERRUPTS();

    delay = endtime - GetNowFloat();
    if (delay >= 600.0)
    {
      delay_ms = 600000;
    }
    else if (delay > 0.0)
    {
      delay_ms = (long)ceil(delay * 1000.0);
    }
    else
    {
      break;
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, delay_ms, WAIT_EVENT_PG_SLEEP);
    ResetLatch(MyLatch);
  }

  PG_RETURN_VOID();
}

                                                     
Datum
pg_get_keywords(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;

  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcontext;
    TupleDesc tupdesc;

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    tupdesc = CreateTemplateTupleDesc(3);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "word", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "catcode", CHAROID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "catdesc", TEXTOID, -1, 0);

    funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();

  if (funcctx->call_cntr < ScanKeywords.num_keywords)
  {
    char *values[3];
    HeapTuple tuple;

                                                                     
    values[0] = unconstify(char *, GetScanKeyword(funcctx->call_cntr, &ScanKeywords));

    switch (ScanKeywordCategories[funcctx->call_cntr])
    {
    case UNRESERVED_KEYWORD:
      values[1] = "U";
      values[2] = _("unreserved");
      break;
    case COL_NAME_KEYWORD:
      values[1] = "C";
      values[2] = _("unreserved (cannot be function or type name)");
      break;
    case TYPE_FUNC_NAME_KEYWORD:
      values[1] = "T";
      values[2] = _("reserved (can be function or type name)");
      break;
    case RESERVED_KEYWORD:
      values[1] = "R";
      values[2] = _("reserved");
      break;
    default:                            
      values[1] = NULL;
      values[2] = NULL;
      break;
    }

    tuple = BuildTupleFromCStrings(funcctx->attinmeta, values);

    SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
  }

  SRF_RETURN_DONE(funcctx);
}

   
                                    
   
Datum
pg_typeof(PG_FUNCTION_ARGS)
{
  PG_RETURN_OID(get_fn_expr_argtype(fcinfo->flinfo, 0));
}

   
                                                                       
                    
   
Datum
pg_collation_for(PG_FUNCTION_ARGS)
{
  Oid typeid;
  Oid collid;

  typeid = get_fn_expr_argtype(fcinfo->flinfo, 0);
  if (!typeid)
  {
    PG_RETURN_NULL();
  }
  if (!type_is_collatable(typeid) && typeid != UNKNOWNOID)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be(typeid))));
  }

  collid = PG_GET_COLLATION();
  if (!collid)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_TEXT_P(cstring_to_text(generate_collation_name(collid)));
}

   
                                                                          
                      
   
                                                                         
                               
   
Datum
pg_relation_is_updatable(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  bool include_triggers = PG_GETARG_BOOL(1);

  PG_RETURN_INT32(relation_is_updatable(reloid, NIL, include_triggers, NULL));
}

   
                                                                    
   
                                                           
                                                                           
                                                                        
                                                                            
   
Datum
pg_column_is_updatable(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  AttrNumber attnum = PG_GETARG_INT16(1);
  AttrNumber col = attnum - FirstLowInvalidHeapAttributeNumber;
  bool include_triggers = PG_GETARG_BOOL(2);
  int events;

                                          
  if (attnum <= 0)
  {
    PG_RETURN_BOOL(false);
  }

  events = relation_is_updatable(reloid, NIL, include_triggers, bms_make_singleton(col));

                                                                     
#define REQ_EVENTS ((1 << CMD_UPDATE) | (1 << CMD_DELETE))

  PG_RETURN_BOOL((events & REQ_EVENTS) == REQ_EVENTS);
}

   
                                          
                                                      
   
static bool
is_ident_start(unsigned char c)
{
                                            
  if (c == '_')
  {
    return true;
  }
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
  {
    return true;
  }
                                                                            
  if (IS_HIGHBIT_SET(c))
  {
    return true;
  }
  return false;
}

   
                                                 
                                                     
   
static bool
is_ident_cont(unsigned char c)
{
                                       
  if ((c >= '0' && c <= '9') || c == '$')
  {
    return true;
  }
                                            
  return is_ident_start(c);
}

   
                                                                             
                                                                       
                                       
   
Datum
parse_ident(PG_FUNCTION_ARGS)
{
  text *qualname = PG_GETARG_TEXT_PP(0);
  bool strict = PG_GETARG_BOOL(1);
  char *qualname_str = text_to_cstring(qualname);
  ArrayBuildState *astate = NULL;
  char *nextp;
  bool after_dot = false;

     
                                                                          
                                                                        
               
     
  nextp = qualname_str;

                               
  while (scanner_isspace(*nextp))
  {
    nextp++;
  }

  for (;;)
  {
    char *curname;
    bool missing_ident = true;

    if (*nextp == '"')
    {
      char *endp;

      curname = nextp + 1;
      for (;;)
      {
        endp = strchr(nextp + 1, '"');
        if (endp == NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("string is not a valid identifier: \"%s\"", text_to_cstring(qualname)), errdetail("String has unclosed double quotes.")));
        }
        if (endp[1] != '"')
        {
          break;
        }
        memmove(endp, endp + 1, strlen(endp));
        nextp = endp;
      }
      nextp = endp + 1;
      *endp = '\0';

      if (endp - curname == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("string is not a valid identifier: \"%s\"", text_to_cstring(qualname)), errdetail("Quoted identifier must not be empty.")));
      }

      astate = accumArrayResult(astate, CStringGetTextDatum(curname), false, TEXTOID, CurrentMemoryContext);
      missing_ident = false;
    }
    else if (is_ident_start((unsigned char)*nextp))
    {
      char *downname;
      int len;
      text *part;

      curname = nextp++;
      while (is_ident_cont((unsigned char)*nextp))
      {
        nextp++;
      }

      len = nextp - curname;

         
                                                                      
                                                                         
                                                                  
                                                          
         
      downname = downcase_identifier(curname, len, false, false);
      part = cstring_to_text_with_len(downname, len);
      astate = accumArrayResult(astate, PointerGetDatum(part), false, TEXTOID, CurrentMemoryContext);
      missing_ident = false;
    }

    if (missing_ident)
    {
                                                              
      if (*nextp == '.')
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("string is not a valid identifier: \"%s\"", text_to_cstring(qualname)), errdetail("No valid identifier before \".\".")));
      }
      else if (after_dot)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("string is not a valid identifier: \"%s\"", text_to_cstring(qualname)), errdetail("No valid identifier after \".\".")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("string is not a valid identifier: \"%s\"", text_to_cstring(qualname))));
      }
    }

    while (scanner_isspace(*nextp))
    {
      nextp++;
    }

    if (*nextp == '.')
    {
      after_dot = true;
      nextp++;
      while (scanner_isspace(*nextp))
      {
        nextp++;
      }
    }
    else if (*nextp == '\0')
    {
      break;
    }
    else
    {
      if (strict)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("string is not a valid identifier: \"%s\"", text_to_cstring(qualname))));
      }
      break;
    }
  }

  PG_RETURN_DATUM(makeArrayResult(astate, CurrentMemoryContext));
}

   
                      
   
                                                                               
   
Datum
pg_current_logfile(PG_FUNCTION_ARGS)
{
  FILE *fd;
  char lbuffer[MAXPGPATH];
  char *logfmt;

                                            
  if (PG_NARGS() == 0 || PG_ARGISNULL(0))
  {
    logfmt = NULL;
  }
  else
  {
    logfmt = text_to_cstring(PG_GETARG_TEXT_PP(0));

    if (strcmp(logfmt, "stderr") != 0 && strcmp(logfmt, "csvlog") != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("log format \"%s\" is not supported", logfmt), errhint("The supported log formats are \"stderr\" and \"csvlog\".")));
    }
  }

  fd = AllocateFile(LOG_METAINFO_DATAFILE, "r");
  if (fd == NULL)
  {
    if (errno != ENOENT)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", LOG_METAINFO_DATAFILE)));
    }
    PG_RETURN_NULL();
  }

#ifdef WIN32
                                                       
  _setmode(_fileno(fd), _O_TEXT);
#endif

     
                                                                       
                
     
  while (fgets(lbuffer, sizeof(lbuffer), fd) != NULL)
  {
    char *log_format;
    char *log_filepath;
    char *nlpos;

                                                             
    log_format = lbuffer;
    log_filepath = strchr(lbuffer, ' ');
    if (log_filepath == NULL)
    {
                                                                 
      elog(ERROR, "missing space character in \"%s\"", LOG_METAINFO_DATAFILE);
      break;
    }

    *log_filepath = '\0';
    log_filepath++;
    nlpos = strchr(log_filepath, '\n');
    if (nlpos == NULL)
    {
                                                                   
      elog(ERROR, "missing newline character in \"%s\"", LOG_METAINFO_DATAFILE);
      break;
    }
    *nlpos = '\0';

    if (logfmt == NULL || strcmp(logfmt, log_format) == 0)
    {
      FreeFile(fd);
      PG_RETURN_TEXT_P(cstring_to_text(log_filepath));
    }
  }

                                            
  FreeFile(fd);

  PG_RETURN_NULL();
}

   
                                                                      
   
                                                                           
                                                                          
                                              
   
Datum
pg_current_logfile_1arg(PG_FUNCTION_ARGS)
{
  return pg_current_logfile(fcinfo);
}

   
                                                 
   
Datum
pg_get_replica_identity_index(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  Oid idxoid;
  Relation rel;

  rel = table_open(reloid, AccessShareLock);
  idxoid = RelationGetReplicaIndex(rel);
  table_close(rel, AccessShareLock);

  if (OidIsValid(idxoid))
  {
    PG_RETURN_OID(idxoid);
  }
  else
  {
    PG_RETURN_NULL();
  }
}
