                                                                            
   
          
                                    
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include "access/tuptoaster.h"
#include "catalog/pg_language.h"
#include "catalog/pg_proc.h"
#include "executor/functions.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "pgstat.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgrtab.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                            
   
PGDLLIMPORT needs_fmgr_hook_type needs_fmgr_hook = NULL;
PGDLLIMPORT fmgr_hook_type fmgr_hook = NULL;

   
                                                     
   
typedef struct
{
                                                    
  Oid fn_oid;                                               
  TransactionId fn_xmin;                                  
  ItemPointerData fn_tid;
  PGFunction user_fn;                                         
  const Pg_finfo_record *inforec;                                 
} CFuncHashTabEntry;

static HTAB *CFuncHash = NULL;

static void
fmgr_info_cxt_security(Oid functionId, FmgrInfo *finfo, MemoryContext mcxt, bool ignore_security);
static void
fmgr_info_C_lang(Oid functionId, FmgrInfo *finfo, HeapTuple procedureTuple);
static void
fmgr_info_other_lang(Oid functionId, FmgrInfo *finfo, HeapTuple procedureTuple);
static CFuncHashTabEntry *
lookup_C_func(HeapTuple procedureTuple);
static void
record_C_func(HeapTuple procedureTuple, PGFunction user_fn, const Pg_finfo_record *inforec);

                                     
extern Datum fmgr_security_definer(PG_FUNCTION_ARGS);

   
                                                                            
                                              
   

static const FmgrBuiltin *
fmgr_isbuiltin(Oid id)
{
  uint16 index;

                                                                
  if (id > fmgr_last_builtin_oid)
  {
    return NULL;
  }

     
                                                                         
                                                                            
     
  index = fmgr_builtin_oid_index[id];
  if (index == InvalidOidBuiltinMapping)
  {
    return NULL;
  }

  return &fmgr_builtins[index];
}

   
                                                                       
                                                                       
            
   
static const FmgrBuiltin *
fmgr_lookupByName(const char *name)
{
  int i;

  for (i = 0; i < fmgr_nbuiltins; i++)
  {
    if (strcmp(name, fmgr_builtins[i].funcName) == 0)
    {
      return fmgr_builtins + i;
    }
  }
  return NULL;
}

   
                                                       
                                 
   
                                                                        
                                                                           
                                                                        
                                                                        
                                                                         
                                                                           
                                                                        
                                                                   
   
void
fmgr_info(Oid functionId, FmgrInfo *finfo)
{
  fmgr_info_cxt_security(functionId, finfo, CurrentMemoryContext, false);
}

   
                                                                    
                              
   
void
fmgr_info_cxt(Oid functionId, FmgrInfo *finfo, MemoryContext mcxt)
{
  fmgr_info_cxt_security(functionId, finfo, mcxt, false);
}

   
                                                                       
                                                       
   
static void
fmgr_info_cxt_security(Oid functionId, FmgrInfo *finfo, MemoryContext mcxt, bool ignore_security)
{
  const FmgrBuiltin *fbp;
  HeapTuple procedureTuple;
  Form_pg_proc procedureStruct;
  Datum prosrcdatum;
  bool isnull;
  char *prosrc;

     
                                                                           
                                                                          
            
     
  finfo->fn_oid = InvalidOid;
  finfo->fn_extra = NULL;
  finfo->fn_mcxt = mcxt;
  finfo->fn_expr = NULL;                                

  if ((fbp = fmgr_isbuiltin(functionId)) != NULL)
  {
       
                                                                        
       
    finfo->fn_nargs = fbp->nargs;
    finfo->fn_strict = fbp->strict;
    finfo->fn_retset = fbp->retset;
    finfo->fn_stats = TRACK_FUNC_ALL;                      
    finfo->fn_addr = fbp->func;
    finfo->fn_oid = functionId;
    return;
  }

                                           
  procedureTuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(functionId));
  if (!HeapTupleIsValid(procedureTuple))
  {
    elog(ERROR, "cache lookup failed for function %u", functionId);
  }
  procedureStruct = (Form_pg_proc)GETSTRUCT(procedureTuple);

  finfo->fn_nargs = procedureStruct->pronargs;
  finfo->fn_strict = procedureStruct->proisstrict;
  finfo->fn_retset = procedureStruct->proretset;

     
                                                                          
                                                                          
                                                                  
                           
     
                                                                         
                                                                          
                                                                       
                                                                         
                                                                      
                                                                           
                                                            
     
  if (!ignore_security && (procedureStruct->prosecdef || !heap_attisnull(procedureTuple, Anum_pg_proc_proconfig, NULL) || FmgrHookIsNeeded(functionId)))
  {
    finfo->fn_addr = fmgr_security_definer;
    finfo->fn_stats = TRACK_FUNC_ALL;                      
    finfo->fn_oid = functionId;
    ReleaseSysCache(procedureTuple);
    return;
  }

  switch (procedureStruct->prolang)
  {
  case INTERNALlanguageId:

       
                                                                  
                                                                 
                                                                    
                                                                       
                                                                 
                                                                    
                                           
       
    prosrcdatum = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc");
    }
    prosrc = TextDatumGetCString(prosrcdatum);
    fbp = fmgr_lookupByName(prosrc);
    if (fbp == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("internal function \"%s\" is not in internal lookup table", prosrc)));
    }
    pfree(prosrc);
                                                                     
    finfo->fn_addr = fbp->func;
                                                             
    finfo->fn_stats = TRACK_FUNC_ALL;                      
    break;

  case ClanguageId:
    fmgr_info_C_lang(functionId, finfo, procedureTuple);
    finfo->fn_stats = TRACK_FUNC_PL;                       
    break;

  case SQLlanguageId:
    finfo->fn_addr = fmgr_sql;
    finfo->fn_stats = TRACK_FUNC_PL;                       
    break;

  default:
    fmgr_info_other_lang(functionId, finfo, procedureTuple);
    finfo->fn_stats = TRACK_FUNC_OFF;                           
    break;
  }

  finfo->fn_oid = functionId;
  ReleaseSysCache(procedureTuple);
}

   
                                                                             
   
                                                                      
             
   
                                                                               
                    
   
                                                                              
                  
   
                                                                          
                   
   
void
fmgr_symbol(Oid functionId, char **mod, char **fn)
{
  HeapTuple procedureTuple;
  Form_pg_proc procedureStruct;
  bool isnull;
  Datum prosrcattr;
  Datum probinattr;

                                           
  procedureTuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(functionId));
  if (!HeapTupleIsValid(procedureTuple))
  {
    elog(ERROR, "cache lookup failed for function %u", functionId);
  }
  procedureStruct = (Form_pg_proc)GETSTRUCT(procedureTuple);

     
     
  if (procedureStruct->prosecdef || !heap_attisnull(procedureTuple, Anum_pg_proc_proconfig, NULL) || FmgrHookIsNeeded(functionId))
  {
    *mod = NULL;                  
    *fn = pstrdup("fmgr_security_definer");
    ReleaseSysCache(procedureTuple);
    return;
  }

                                                           
  switch (procedureStruct->prolang)
  {
  case INTERNALlanguageId:
    prosrcattr = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc");
    }

    *mod = NULL;                  
    *fn = TextDatumGetCString(prosrcattr);
    break;

  case ClanguageId:
    prosrcattr = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc for C function %u", functionId);
    }

    probinattr = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_probin, &isnull);
    if (isnull)
    {
      elog(ERROR, "null probin for C function %u", functionId);
    }

       
                                                                    
                                          
       
    *mod = TextDatumGetCString(probinattr);
    *fn = TextDatumGetCString(prosrcattr);
    break;

  case SQLlanguageId:
    *mod = NULL;                  
    *fn = pstrdup("fmgr_sql");
    break;

  default:
    *mod = NULL;
    *fn = NULL;                            
    break;
  }

  ReleaseSysCache(procedureTuple);
}

   
                                                                     
                                   
   
static void
fmgr_info_C_lang(Oid functionId, FmgrInfo *finfo, HeapTuple procedureTuple)
{
  CFuncHashTabEntry *hashentry;
  PGFunction user_fn;
  const Pg_finfo_record *inforec;
  bool isnull;

     
                                                        
     
  hashentry = lookup_C_func(procedureTuple);
  if (hashentry)
  {
    user_fn = hashentry->user_fn;
    inforec = hashentry->inforec;
  }
  else
  {
    Datum prosrcattr, probinattr;
    char *prosrcstring, *probinstring;
    void *libraryhandle;

       
                                                                         
                                                                        
                                 
       
    prosrcattr = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc for C function %u", functionId);
    }
    prosrcstring = TextDatumGetCString(prosrcattr);

    probinattr = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_probin, &isnull);
    if (isnull)
    {
      elog(ERROR, "null probin for C function %u", functionId);
    }
    probinstring = TextDatumGetCString(probinattr);

                                     
    user_fn = load_external_function(probinstring, prosrcstring, true, &libraryhandle);

                                                               
    inforec = fetch_finfo_record(libraryhandle, prosrcstring);

                                             
    record_C_func(procedureTuple, user_fn, inforec);

    pfree(prosrcstring);
    pfree(probinstring);
  }

  switch (inforec->api_version)
  {
  case 1:
                                  
    finfo->fn_addr = user_fn;
    break;
  default:
                                                              
    elog(ERROR, "unrecognized function API version: %d", inforec->api_version);
    break;
  }
}

   
                                                                    
                                        
   
static void
fmgr_info_other_lang(Oid functionId, FmgrInfo *finfo, HeapTuple procedureTuple)
{
  Form_pg_proc procedureStruct = (Form_pg_proc)GETSTRUCT(procedureTuple);
  Oid language = procedureStruct->prolang;
  HeapTuple languageTuple;
  Form_pg_language languageStruct;
  FmgrInfo plfinfo;

  languageTuple = SearchSysCache1(LANGOID, ObjectIdGetDatum(language));
  if (!HeapTupleIsValid(languageTuple))
  {
    elog(ERROR, "cache lookup failed for language %u", language);
  }
  languageStruct = (Form_pg_language)GETSTRUCT(languageTuple);

     
                                                                           
                                                                            
                                                                   
     
  fmgr_info_cxt_security(languageStruct->lanplcallfoid, &plfinfo, CurrentMemoryContext, true);
  finfo->fn_addr = plfinfo.fn_addr;

  ReleaseSysCache(languageTuple);
}

   
                                                                              
                                                                    
                                                                        
   
                                                                     
   
                                                                            
                                                                           
            
   
const Pg_finfo_record *
fetch_finfo_record(void *filehandle, const char *funcname)
{
  char *infofuncname;
  PGFInfoFunction infofunc;
  const Pg_finfo_record *inforec;

  infofuncname = psprintf("pg_finfo_%s", funcname);

                                        
  infofunc = (PGFInfoFunction)lookup_external_function(filehandle, infofuncname);
  if (infofunc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not find function information for function \"%s\"", funcname), errhint("SQL-callable functions need an accompanying PG_FUNCTION_INFO_V1(funcname).")));
    return NULL;                       
  }

                         
  inforec = (*infofunc)();

                                      
  if (inforec == NULL)
  {
    elog(ERROR, "null result from info function \"%s\"", infofuncname);
  }
  switch (inforec->api_version)
  {
  case 1:
                                              
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized API version %d reported by info function \"%s\"", inforec->api_version, infofuncname)));
    break;
  }

  pfree(infofuncname);
  return inforec;
}

                                                                            
                                                                      
   
                                                                           
                                                                               
                                            
                                                                            
   

   
                                                             
   
                                                                     
   
static CFuncHashTabEntry *
lookup_C_func(HeapTuple procedureTuple)
{
  Oid fn_oid = ((Form_pg_proc)GETSTRUCT(procedureTuple))->oid;
  CFuncHashTabEntry *entry;

  if (CFuncHash == NULL)
  {
    return NULL;                   
  }
  entry = (CFuncHashTabEntry *)hash_search(CFuncHash, &fn_oid, HASH_FIND, NULL);
  if (entry == NULL)
  {
    return NULL;                    
  }
  if (entry->fn_xmin == HeapTupleHeaderGetRawXmin(procedureTuple->t_data) && ItemPointerEquals(&entry->fn_tid, &procedureTuple->t_self))
  {
    return entry;         
  }
  return NULL;                           
}

   
                                                                              
   
static void
record_C_func(HeapTuple procedureTuple, PGFunction user_fn, const Pg_finfo_record *inforec)
{
  Oid fn_oid = ((Form_pg_proc)GETSTRUCT(procedureTuple))->oid;
  CFuncHashTabEntry *entry;
  bool found;

                                                     
  if (CFuncHash == NULL)
  {
    HASHCTL hash_ctl;

    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(Oid);
    hash_ctl.entrysize = sizeof(CFuncHashTabEntry);
    CFuncHash = hash_create("CFuncHash", 100, &hash_ctl, HASH_ELEM | HASH_BLOBS);
  }

  entry = (CFuncHashTabEntry *)hash_search(CFuncHash, &fn_oid, HASH_ENTER, &found);
                                
  entry->fn_xmin = HeapTupleHeaderGetRawXmin(procedureTuple->t_data);
  entry->fn_tid = procedureTuple->t_self;
  entry->user_fn = user_fn;
  entry->inforec = inforec;
}

   
                                                                           
   
                                                                            
                                                                           
   
void
clear_external_function_hash(void *filehandle)
{
  if (CFuncHash)
  {
    hash_destroy(CFuncHash);
  }
  CFuncHash = NULL;
}

   
                           
   
                                                                       
                                                                      
                                                                     
   
void
fmgr_info_copy(FmgrInfo *dstinfo, FmgrInfo *srcinfo, MemoryContext destcxt)
{
  memcpy(dstinfo, srcinfo, sizeof(FmgrInfo));
  dstinfo->fn_mcxt = destcxt;
  dstinfo->fn_extra = NULL;
}

   
                                                                             
                                                                 
                                                     
   
Oid
fmgr_internal_function(const char *proname)
{
  const FmgrBuiltin *fbp = fmgr_lookupByName(proname);

  if (fbp == NULL)
  {
    return InvalidOid;
  }
  return fbp->foid;
}

   
                                                                           
                                                                        
                                                                            
                                                       
   
struct fmgr_security_definer_cache
{
  FmgrInfo flinfo;                                           
  Oid userid;                                             
  ArrayType *proconfig;                                 
  Datum arg;                                                         
};

   
                                                                            
                                                                         
                                                                             
                                                                    
                                                                        
                                                                         
                                                                         
                                                       
   
extern Datum
fmgr_security_definer(PG_FUNCTION_ARGS)
{
  Datum result;
  struct fmgr_security_definer_cache *volatile fcache;
  FmgrInfo *save_flinfo;
  Oid save_userid;
  int save_sec_context;
  volatile int save_nestlevel;
  PgStat_FunctionCallUsage fcusage;

  if (!fcinfo->flinfo->fn_extra)
  {
    HeapTuple tuple;
    Form_pg_proc procedureStruct;
    Datum datum;
    bool isnull;
    MemoryContext oldcxt;

    fcache = MemoryContextAllocZero(fcinfo->flinfo->fn_mcxt, sizeof(*fcache));

    fmgr_info_cxt_security(fcinfo->flinfo->fn_oid, &fcache->flinfo, fcinfo->flinfo->fn_mcxt, true);
    fcache->flinfo.fn_expr = fcinfo->flinfo->fn_expr;

    tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(fcinfo->flinfo->fn_oid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", fcinfo->flinfo->fn_oid);
    }
    procedureStruct = (Form_pg_proc)GETSTRUCT(tuple);

    if (procedureStruct->prosecdef)
    {
      fcache->userid = procedureStruct->proowner;
    }

    datum = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_proconfig, &isnull);
    if (!isnull)
    {
      oldcxt = MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);
      fcache->proconfig = DatumGetArrayTypePCopy(datum);
      MemoryContextSwitchTo(oldcxt);
    }

    ReleaseSysCache(tuple);

    fcinfo->flinfo->fn_extra = fcache;
  }
  else
  {
    fcache = fcinfo->flinfo->fn_extra;
  }

                                                                            
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  if (fcache->proconfig)                                   
  {
    save_nestlevel = NewGUCNestLevel();
  }
  else
  {
    save_nestlevel = 0;                          
  }

  if (OidIsValid(fcache->userid))
  {
    SetUserIdAndSecContext(fcache->userid, save_sec_context | SECURITY_LOCAL_USERID_CHANGE);
  }

  if (fcache->proconfig)
  {
    ProcessGUCArray(fcache->proconfig, (superuser() ? PGC_SUSET : PGC_USERSET), PGC_S_SESSION, GUC_ACTION_SAVE);
  }

                             
  if (fmgr_hook)
  {
    (*fmgr_hook)(FHET_START, &fcache->flinfo, &fcache->arg);
  }

     
                                                                           
                                                                           
                                         
     
  save_flinfo = fcinfo->flinfo;

  PG_TRY();
  {
    fcinfo->flinfo = &fcache->flinfo;

                                             
    pgstat_init_function_usage(fcinfo, &fcusage);

    result = FunctionCallInvoke(fcinfo);

       
                                                                         
                                                            
       
    pgstat_end_function_usage(&fcusage, (fcinfo->resultinfo == NULL || !IsA(fcinfo->resultinfo, ReturnSetInfo) || ((ReturnSetInfo *)fcinfo->resultinfo)->isDone != ExprMultipleResult));
  }
  PG_CATCH();
  {
    fcinfo->flinfo = save_flinfo;
    if (fmgr_hook)
    {
      (*fmgr_hook)(FHET_ABORT, &fcache->flinfo, &fcache->arg);
    }
    PG_RE_THROW();
  }
  PG_END_TRY();

  fcinfo->flinfo = save_flinfo;

  if (fcache->proconfig)
  {
    AtEOXact_GUC(true, save_nestlevel);
  }
  if (OidIsValid(fcache->userid))
  {
    SetUserIdAndSecContext(save_userid, save_sec_context);
  }
  if (fmgr_hook)
  {
    (*fmgr_hook)(FHET_END, &fcache->flinfo, &fcache->arg);
  }

  return result;
}

                                                                            
                                                              
                                                                            
   

   
                                                                    
                                                                             
                                                                           
                                               
   
Datum
DirectFunctionCall1Coll(PGFunction func, Oid collation, Datum arg1)
{
  LOCAL_FCINFO(fcinfo, 1);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 1, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall2Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2)
{
  LOCAL_FCINFO(fcinfo, 2);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 2, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall3Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3)
{
  LOCAL_FCINFO(fcinfo, 3);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 3, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall4Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4)
{
  LOCAL_FCINFO(fcinfo, 4);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 4, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall5Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5)
{
  LOCAL_FCINFO(fcinfo, 5);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 5, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall6Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6)
{
  LOCAL_FCINFO(fcinfo, 6);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 6, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall7Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7)
{
  LOCAL_FCINFO(fcinfo, 7);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 7, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;
  fcinfo->args[6].value = arg7;
  fcinfo->args[6].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall8Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7, Datum arg8)
{
  LOCAL_FCINFO(fcinfo, 8);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 8, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;
  fcinfo->args[6].value = arg7;
  fcinfo->args[6].isnull = false;
  fcinfo->args[7].value = arg8;
  fcinfo->args[7].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
DirectFunctionCall9Coll(PGFunction func, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7, Datum arg8, Datum arg9)
{
  LOCAL_FCINFO(fcinfo, 9);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, NULL, 9, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;
  fcinfo->args[6].value = arg7;
  fcinfo->args[6].isnull = false;
  fcinfo->args[7].value = arg8;
  fcinfo->args[7].isnull = false;
  fcinfo->args[8].value = arg9;
  fcinfo->args[8].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

   
                                                                          
                                                                        
                                                                      
                                                                        
                                                                     
                                                                              
   

Datum
CallerFInfoFunctionCall1(PGFunction func, FmgrInfo *flinfo, Oid collation, Datum arg1)
{
  LOCAL_FCINFO(fcinfo, 1);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 1, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

Datum
CallerFInfoFunctionCall2(PGFunction func, FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2)
{
  LOCAL_FCINFO(fcinfo, 2);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 2, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;

  result = (*func)(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %p returned NULL", (void *)func);
  }

  return result;
}

   
                                                                      
                                                                             
                           
   
Datum
FunctionCall0Coll(FmgrInfo *flinfo, Oid collation)
{
  LOCAL_FCINFO(fcinfo, 0);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 0, collation, NULL, NULL);

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall1Coll(FmgrInfo *flinfo, Oid collation, Datum arg1)
{
  LOCAL_FCINFO(fcinfo, 1);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 1, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall2Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2)
{
  LOCAL_FCINFO(fcinfo, 2);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 2, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall3Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3)
{
  LOCAL_FCINFO(fcinfo, 3);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 3, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall4Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4)
{
  LOCAL_FCINFO(fcinfo, 4);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 4, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall5Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5)
{
  LOCAL_FCINFO(fcinfo, 5);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 5, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall6Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6)
{
  LOCAL_FCINFO(fcinfo, 6);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 6, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall7Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7)
{
  LOCAL_FCINFO(fcinfo, 7);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 7, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;
  fcinfo->args[6].value = arg7;
  fcinfo->args[6].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall8Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7, Datum arg8)
{
  LOCAL_FCINFO(fcinfo, 8);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 8, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;
  fcinfo->args[6].value = arg7;
  fcinfo->args[6].isnull = false;
  fcinfo->args[7].value = arg8;
  fcinfo->args[7].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

Datum
FunctionCall9Coll(FmgrInfo *flinfo, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7, Datum arg8, Datum arg9)
{
  LOCAL_FCINFO(fcinfo, 9);
  Datum result;

  InitFunctionCallInfoData(*fcinfo, flinfo, 9, collation, NULL, NULL);

  fcinfo->args[0].value = arg1;
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = arg2;
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = arg3;
  fcinfo->args[2].isnull = false;
  fcinfo->args[3].value = arg4;
  fcinfo->args[3].isnull = false;
  fcinfo->args[4].value = arg5;
  fcinfo->args[4].isnull = false;
  fcinfo->args[5].value = arg6;
  fcinfo->args[5].isnull = false;
  fcinfo->args[6].value = arg7;
  fcinfo->args[6].isnull = false;
  fcinfo->args[7].value = arg8;
  fcinfo->args[7].isnull = false;
  fcinfo->args[8].value = arg9;
  fcinfo->args[8].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                                        
  if (fcinfo->isnull)
  {
    elog(ERROR, "function %u returned NULL", flinfo->fn_oid);
  }

  return result;
}

   
                                                                   
                                                                             
                                                                       
                                                                          
                                                         
   
Datum
OidFunctionCall0Coll(Oid functionId, Oid collation)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall0Coll(&flinfo, collation);
}

Datum
OidFunctionCall1Coll(Oid functionId, Oid collation, Datum arg1)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall1Coll(&flinfo, collation, arg1);
}

Datum
OidFunctionCall2Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall2Coll(&flinfo, collation, arg1, arg2);
}

Datum
OidFunctionCall3Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall3Coll(&flinfo, collation, arg1, arg2, arg3);
}

Datum
OidFunctionCall4Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall4Coll(&flinfo, collation, arg1, arg2, arg3, arg4);
}

Datum
OidFunctionCall5Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall5Coll(&flinfo, collation, arg1, arg2, arg3, arg4, arg5);
}

Datum
OidFunctionCall6Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall6Coll(&flinfo, collation, arg1, arg2, arg3, arg4, arg5, arg6);
}

Datum
OidFunctionCall7Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall7Coll(&flinfo, collation, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

Datum
OidFunctionCall8Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7, Datum arg8)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall8Coll(&flinfo, collation, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

Datum
OidFunctionCall9Coll(Oid functionId, Oid collation, Datum arg1, Datum arg2, Datum arg3, Datum arg4, Datum arg5, Datum arg6, Datum arg7, Datum arg8, Datum arg9)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);

  return FunctionCall9Coll(&flinfo, collation, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

   
                                                                      
   

   
                                                        
   
                                                                      
                                                                         
                                                                        
                              
   
Datum
InputFunctionCall(FmgrInfo *flinfo, char *str, Oid typioparam, int32 typmod)
{
  LOCAL_FCINFO(fcinfo, 3);
  Datum result;

  if (str == NULL && flinfo->fn_strict)
  {
    return (Datum)0;                              
  }

  InitFunctionCallInfoData(*fcinfo, flinfo, 3, InvalidOid, NULL, NULL);

  fcinfo->args[0].value = CStringGetDatum(str);
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = ObjectIdGetDatum(typioparam);
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = Int32GetDatum(typmod);
  fcinfo->args[2].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                         
  if (str == NULL)
  {
    if (!fcinfo->isnull)
    {
      elog(ERROR, "input function %u returned non-NULL", flinfo->fn_oid);
    }
  }
  else
  {
    if (fcinfo->isnull)
    {
      elog(ERROR, "input function %u returned NULL", flinfo->fn_oid);
    }
  }

  return result;
}

   
                                                         
   
                                    
   
                                                                         
   
char *
OutputFunctionCall(FmgrInfo *flinfo, Datum val)
{
  return DatumGetCString(FunctionCall1(flinfo, val));
}

   
                                                               
   
                                                                      
                                                                           
                                                                        
                              
   
Datum
ReceiveFunctionCall(FmgrInfo *flinfo, StringInfo buf, Oid typioparam, int32 typmod)
{
  LOCAL_FCINFO(fcinfo, 3);
  Datum result;

  if (buf == NULL && flinfo->fn_strict)
  {
    return (Datum)0;                              
  }

  InitFunctionCallInfoData(*fcinfo, flinfo, 3, InvalidOid, NULL, NULL);

  fcinfo->args[0].value = PointerGetDatum(buf);
  fcinfo->args[0].isnull = false;
  fcinfo->args[1].value = ObjectIdGetDatum(typioparam);
  fcinfo->args[1].isnull = false;
  fcinfo->args[2].value = Int32GetDatum(typmod);
  fcinfo->args[2].isnull = false;

  result = FunctionCallInvoke(fcinfo);

                                                         
  if (buf == NULL)
  {
    if (!fcinfo->isnull)
    {
      elog(ERROR, "receive function %u returned non-NULL", flinfo->fn_oid);
    }
  }
  else
  {
    if (fcinfo->isnull)
    {
      elog(ERROR, "receive function %u returned NULL", flinfo->fn_oid);
    }
  }

  return result;
}

   
                                                                
   
                                    
   
                                                                           
                                                                          
                     
   
bytea *
SendFunctionCall(FmgrInfo *flinfo, Datum val)
{
  return DatumGetByteaP(FunctionCall1(flinfo, val));
}

   
                                                                             
                                                                           
   
Datum
OidInputFunctionCall(Oid functionId, char *str, Oid typioparam, int32 typmod)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);
  return InputFunctionCall(&flinfo, str, typioparam, typmod);
}

char *
OidOutputFunctionCall(Oid functionId, Datum val)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);
  return OutputFunctionCall(&flinfo, val);
}

Datum
OidReceiveFunctionCall(Oid functionId, StringInfo buf, Oid typioparam, int32 typmod)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);
  return ReceiveFunctionCall(&flinfo, buf, typioparam, typmod);
}

bytea *
OidSendFunctionCall(Oid functionId, Datum val)
{
  FmgrInfo flinfo;

  fmgr_info(functionId, &flinfo);
  return SendFunctionCall(&flinfo, val);
}

                                                                            
                                                                    
   
                                                                            
                                                                           
                                                     
   
                                                                           
                                                                           
                                                                    
                                                                            
   

#ifndef USE_FLOAT8_BYVAL                        

Datum
Int64GetDatum(int64 X)
{
  int64 *retval = (int64 *)palloc(sizeof(int64));

  *retval = X;
  return PointerGetDatum(retval);
}
#endif                       

#ifndef USE_FLOAT4_BYVAL

Datum
Float4GetDatum(float4 X)
{
  float4 *retval = (float4 *)palloc(sizeof(float4));

  *retval = X;
  return PointerGetDatum(retval);
}
#endif

#ifndef USE_FLOAT8_BYVAL

Datum
Float8GetDatum(float8 X)
{
  float8 *retval = (float8 *)palloc(sizeof(float8));

  *retval = X;
  return PointerGetDatum(retval);
}
#endif

                                                                            
                                             
                                                                            
   

struct varlena *
pg_detoast_datum(struct varlena *datum)
{
  if (VARATT_IS_EXTENDED(datum))
  {
    return heap_tuple_untoast_attr(datum);
  }
  else
  {
    return datum;
  }
}

struct varlena *
pg_detoast_datum_copy(struct varlena *datum)
{
  if (VARATT_IS_EXTENDED(datum))
  {
    return heap_tuple_untoast_attr(datum);
  }
  else
  {
                                                      
    Size len = VARSIZE(datum);
    struct varlena *result = (struct varlena *)palloc(len);

    memcpy(result, datum, len);
    return result;
  }
}

struct varlena *
pg_detoast_datum_slice(struct varlena *datum, int32 first, int32 count)
{
                                                         
  return heap_tuple_untoast_attr_slice(datum, first, count);
}

struct varlena *
pg_detoast_datum_packed(struct varlena *datum)
{
  if (VARATT_IS_COMPRESSED(datum) || VARATT_IS_EXTERNAL(datum))
  {
    return heap_tuple_untoast_attr(datum);
  }
  else
  {
    return datum;
  }
}

                                                                            
                                                                 
   
                                                                             
                                                                       
                                                                                
                                                                               
                                                                            
   

   
                                                       
   
                                                      
   
Oid
get_fn_expr_rettype(FmgrInfo *flinfo)
{
  Node *expr;

     
                                                                           
                                   
     
  if (!flinfo || !flinfo->fn_expr)
  {
    return InvalidOid;
  }

  expr = flinfo->fn_expr;

  return exprType(expr);
}

   
                                                                             
   
                                                      
   
Oid
get_fn_expr_argtype(FmgrInfo *flinfo, int argnum)
{
     
                                                                           
                                   
     
  if (!flinfo || !flinfo->fn_expr)
  {
    return InvalidOid;
  }

  return get_call_expr_argtype(flinfo->fn_expr, argnum);
}

   
                                                                              
                                                                    
   
                                                      
   
Oid
get_call_expr_argtype(Node *expr, int argnum)
{
  List *args;
  Oid argtype;

  if (expr == NULL)
  {
    return InvalidOid;
  }

  if (IsA(expr, FuncExpr))
  {
    args = ((FuncExpr *)expr)->args;
  }
  else if (IsA(expr, OpExpr))
  {
    args = ((OpExpr *)expr)->args;
  }
  else if (IsA(expr, DistinctExpr))
  {
    args = ((DistinctExpr *)expr)->args;
  }
  else if (IsA(expr, ScalarArrayOpExpr))
  {
    args = ((ScalarArrayOpExpr *)expr)->args;
  }
  else if (IsA(expr, NullIfExpr))
  {
    args = ((NullIfExpr *)expr)->args;
  }
  else if (IsA(expr, WindowFunc))
  {
    args = ((WindowFunc *)expr)->args;
  }
  else
  {
    return InvalidOid;
  }

  if (argnum < 0 || argnum >= list_length(args))
  {
    return InvalidOid;
  }

  argtype = exprType((Node *)list_nth(args, argnum));

     
                                                                           
                                                           
     
  if (IsA(expr, ScalarArrayOpExpr) && argnum == 1)
  {
    argtype = get_base_element_type(argtype);
  }

  return argtype;
}

   
                                                                     
                       
   
                                                 
   
bool
get_fn_expr_arg_stable(FmgrInfo *flinfo, int argnum)
{
     
                                                                           
                                   
     
  if (!flinfo || !flinfo->fn_expr)
  {
    return false;
  }

  return get_call_expr_arg_stable(flinfo->fn_expr, argnum);
}

   
                                                                     
                                                                     
   
                                                 
   
bool
get_call_expr_arg_stable(Node *expr, int argnum)
{
  List *args;
  Node *arg;

  if (expr == NULL)
  {
    return false;
  }

  if (IsA(expr, FuncExpr))
  {
    args = ((FuncExpr *)expr)->args;
  }
  else if (IsA(expr, OpExpr))
  {
    args = ((OpExpr *)expr)->args;
  }
  else if (IsA(expr, DistinctExpr))
  {
    args = ((DistinctExpr *)expr)->args;
  }
  else if (IsA(expr, ScalarArrayOpExpr))
  {
    args = ((ScalarArrayOpExpr *)expr)->args;
  }
  else if (IsA(expr, NullIfExpr))
  {
    args = ((NullIfExpr *)expr)->args;
  }
  else if (IsA(expr, WindowFunc))
  {
    args = ((WindowFunc *)expr)->args;
  }
  else
  {
    return false;
  }

  if (argnum < 0 || argnum >= list_length(args))
  {
    return false;
  }

  arg = (Node *)list_nth(args, argnum);

     
                                                                             
                                                                           
                                           
     
  if (IsA(arg, Const))
  {
    return true;
  }
  if (IsA(arg, Param) && ((Param *)arg)->paramkind == PARAM_EXTERN)
  {
    return true;
  }

  return false;
}

   
                                                      
   
                                                                          
   
                                                                     
   
bool
get_fn_expr_variadic(FmgrInfo *flinfo)
{
  Node *expr;

     
                                                                           
                                   
     
  if (!flinfo || !flinfo->fn_expr)
  {
    return false;
  }

  expr = flinfo->fn_expr;

  if (IsA(expr, FuncExpr))
  {
    return ((FuncExpr *)expr)->funcvariadic;
  }
  else
  {
    return false;
  }
}

                                                                            
                                                             
                                                                            
   

   
                                                                         
                                                                             
                                                                        
                                                                               
                                                                            
                                        
   
                                                                             
                                                                             
                         
   
                                                                            
                                                                             
                                                                        
                                                                             
                                                                               
                                                                            
                                                                       
                                                                               
                                                                               
                                                                          
                                                                            
                                                          
   
bool
CheckFunctionValidatorAccess(Oid validatorOid, Oid functionOid)
{
  HeapTuple procTup;
  HeapTuple langTup;
  Form_pg_proc procStruct;
  Form_pg_language langStruct;
  AclResult aclresult;

     
                                                                          
                                                                     
     
  procTup = SearchSysCache1(PROCOID, ObjectIdGetDatum(functionOid));
  if (!HeapTupleIsValid(procTup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function with OID %u does not exist", functionOid)));
  }
  procStruct = (Form_pg_proc)GETSTRUCT(procTup);

     
                                                                       
                                      
     
  langTup = SearchSysCache1(LANGOID, ObjectIdGetDatum(procStruct->prolang));
  if (!HeapTupleIsValid(langTup))
  {
    elog(ERROR, "cache lookup failed for language %u", procStruct->prolang);
  }
  langStruct = (Form_pg_language)GETSTRUCT(langTup);

  if (langStruct->lanvalidator != validatorOid)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("language validation function %u called for language %u instead of %u", validatorOid, procStruct->prolang, langStruct->lanvalidator)));
  }

                                                                   
  aclresult = pg_language_aclcheck(procStruct->prolang, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_LANGUAGE, NameStr(langStruct->lanname));
  }

     
                                                                            
                                                            
                                                     
     
  aclresult = pg_proc_aclcheck(functionOid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_FUNCTION, NameStr(procStruct->proname));
  }

  ReleaseSysCache(procTup);
  ReleaseSysCache(langTup);

  return true;
}
