                                                                        
                                            
                                 
   
                        
   
                                                                        

#include "postgres.h"

#include <tcl.h>

#include <unistd.h>
#include <fcntl.h>

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/event_trigger.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "pgstat.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/regproc.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

PG_MODULE_MAGIC;

#define HAVE_TCL_VERSION(maj, min) ((TCL_MAJOR_VERSION > maj) || (TCL_MAJOR_VERSION == maj && TCL_MINOR_VERSION >= min))

                          
#if !HAVE_TCL_VERSION(8, 4)
#error PostgreSQL only supports Tcl 8.4 or later.
#endif

                                                                            
#ifndef CONST86
#define CONST86
#endif

                                             
#undef TEXTDOMAIN
#define TEXTDOMAIN PG_TEXTDOMAIN("pltcl")

   
                                                                             
                                                       
   
                                                                        
                                                                             
                                                                            
                                                 
   

static inline char *
utf_u2e(const char *src)
{
  return pg_any_to_server(src, strlen(src), PG_UTF8);
}

static inline char *
utf_e2u(const char *src)
{
  return pg_server_to_any(src, strlen(src), PG_UTF8);
}

#define UTF_BEGIN                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    const char *_pltcl_utf_src = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  char *_pltcl_utf_dst = NULL

#define UTF_END                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  if (_pltcl_utf_src != (const char *)_pltcl_utf_dst)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    pfree(_pltcl_utf_dst);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  while (0)

#define UTF_U2E(x) (_pltcl_utf_dst = utf_u2e(_pltcl_utf_src = (x)))

#define UTF_E2U(x) (_pltcl_utf_dst = utf_e2u(_pltcl_utf_src = (x)))

                                                                        
                                                                           
                                                                           
                                                                             
                                                                             
                                                                    
   
                                                                           
                                                                        
                                                                        
typedef struct pltcl_interp_desc
{
  Oid user_id;                                             
  Tcl_Interp *interp;                            
  Tcl_HashTable query_hash;                               
} pltcl_interp_desc;

                                                                        
                                                    
   
                                                                      
                                                                   
                                                                          
                                                                       
                                                                         
                                                                          
                                                                          
                                                                      
   
                                                                        
                                                                        
                                                                        
typedef struct pltcl_proc_desc
{
  char *user_proname;                                                     
  char *internal_proname;                                               
  MemoryContext fn_cxt;                                                  
  unsigned long fn_refcount;                                       
  TransactionId fn_xmin;                                   
  ItemPointerData fn_tid;                                 
  bool fn_readonly;                                          
  bool lanpltrusted;                                             
  pltcl_interp_desc *interp_desc;                         
  Oid result_typid;                                            
  FmgrInfo result_in_func;                                                 
  Oid result_typioparam;                                     
  bool fn_retisset;                                                   
  bool fn_retistuple;                                                     
  bool fn_retisdomain;                                                 
  void *domain_info;                                                  
  int nargs;                                               
                                        
  FmgrInfo *arg_out_func;                               
  bool *arg_is_rowtype;                               
} pltcl_proc_desc;

                                                                        
                                                           
                                                                        
typedef struct pltcl_query_desc
{
  char qname[20];
  SPIPlanPtr plan;
  int nargs;
  Oid *argtypes;
  FmgrInfo *arginfuncs;
  Oid *argtypioparams;
} pltcl_query_desc;

                                                                        
                                                            
                                                                       
                                                                         
                                                                            
   
                                                                              
                                                                              
                                                                           
                                  
                                                                        
typedef struct pltcl_proc_key
{
  Oid proc_id;                   

     
                                                                           
                         
     
  Oid is_trigger;                                
  Oid user_id;                                         
} pltcl_proc_key;

typedef struct pltcl_proc_ptr
{
  pltcl_proc_key proc_key;                                
  pltcl_proc_desc *proc_ptr;
} pltcl_proc_ptr;

                                                                        
                  
                                                                        
typedef struct pltcl_call_state
{
                                              
  FunctionCallInfo fcinfo;

                                                                         
  TriggerData *trigdata;

                                                             
  pltcl_proc_desc *prodesc;

     
                                                                   
                                                                     
                                                                        
     
  TupleDesc ret_tupdesc;                                                   
  AttInMetadata *attinmeta;                                                

  ReturnSetInfo *rsi;                                                 
  Tuplestorestate *tuple_store;                                   
  MemoryContext tuple_store_cxt;                                          
  ResourceOwner tuple_store_owner;
} pltcl_call_state;

                                                                        
               
                                                                        
static char *pltcl_start_proc = NULL;
static char *pltclu_start_proc = NULL;
static bool pltcl_pm_init_done = false;
static Tcl_Interp *pltcl_hold_interp = NULL;
static HTAB *pltcl_interp_htab = NULL;
static HTAB *pltcl_proc_htab = NULL;

                                                 
static pltcl_call_state *pltcl_current_call_state = NULL;

                                                                        
                                             
                                                                        
typedef struct
{
  const char *label;
  int sqlerrstate;
} TclExceptionNameMap;

static const TclExceptionNameMap exception_name_map[] = {
#include "pltclerrcodes.h"                         
    {NULL, 0}};

                                                                        
                        
                                                                        
void
_PG_init(void);

static void
pltcl_init_interp(pltcl_interp_desc *interp_desc, Oid prolang, bool pltrusted);
static pltcl_interp_desc *
pltcl_fetch_interp(Oid prolang, bool pltrusted);
static void
call_pltcl_start_proc(Oid prolang, bool pltrusted);
static void
start_proc_error_callback(void *arg);

static Datum
pltcl_handler(PG_FUNCTION_ARGS, bool pltrusted);

static Datum
pltcl_func_handler(PG_FUNCTION_ARGS, pltcl_call_state *call_state, bool pltrusted);
static HeapTuple
pltcl_trigger_handler(PG_FUNCTION_ARGS, pltcl_call_state *call_state, bool pltrusted);
static void
pltcl_event_trigger_handler(PG_FUNCTION_ARGS, pltcl_call_state *call_state, bool pltrusted);

static void
throw_tcl_error(Tcl_Interp *interp, const char *proname);

static pltcl_proc_desc *
compile_pltcl_function(Oid fn_oid, Oid tgreloid, bool is_event_trigger, bool pltrusted);

static int
pltcl_elog(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static void
pltcl_construct_errorCode(Tcl_Interp *interp, ErrorData *edata);
static const char *
pltcl_get_condition_name(int sqlstate);
static int
pltcl_quote(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_argisnull(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_returnnull(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_returnnext(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_SPI_execute(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_process_SPI_result(Tcl_Interp *interp, const char *arrayname, Tcl_Obj *loop_body, int spi_rc, SPITupleTable *tuptable, uint64 ntuples);
static int
pltcl_SPI_prepare(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_SPI_execute_plan(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_subtransaction(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_commit(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int
pltcl_rollback(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

static void
pltcl_subtrans_begin(MemoryContext oldcontext, ResourceOwner oldowner);
static void
pltcl_subtrans_commit(MemoryContext oldcontext, ResourceOwner oldowner);
static void
pltcl_subtrans_abort(Tcl_Interp *interp, MemoryContext oldcontext, ResourceOwner oldowner);

static void
pltcl_set_tuple_values(Tcl_Interp *interp, const char *arrayname, uint64 tupno, HeapTuple tuple, TupleDesc tupdesc);
static Tcl_Obj *
pltcl_build_tuple_argument(HeapTuple tuple, TupleDesc tupdesc, bool include_generated);
static HeapTuple
pltcl_build_tuple_result(Tcl_Interp *interp, Tcl_Obj **kvObjv, int kvObjc, pltcl_call_state *call_state);
static void
pltcl_init_tuple_store(pltcl_call_state *call_state);

   
                                                                         
                                                                          
                                                                              
                                                                           
                                                        
                                                                               
                                                                          
                                                                          
                                                                         
   
static ClientData
pltcl_InitNotifier(void)
{
  static int fakeThreadKey;                                           

  return (ClientData) & (fakeThreadKey);
}

static void
pltcl_FinalizeNotifier(ClientData clientData)
{
}

static void
pltcl_SetTimer(CONST86 Tcl_Time *timePtr)
{
}

static void
pltcl_AlertNotifier(ClientData clientData)
{
}

static void
pltcl_CreateFileHandler(int fd, int mask, Tcl_FileProc *proc, ClientData clientData)
{
}

static void
pltcl_DeleteFileHandler(int fd)
{
}

static void
pltcl_ServiceModeHook(int mode)
{
}

static int
pltcl_WaitForEvent(CONST86 Tcl_Time *timePtr)
{
  return 0;
}

   
                                                   
   
                                                
   
                                                                    
                                                             
   
void
_PG_init(void)
{
  Tcl_NotifierProcs notifier;
  HASHCTL hash_ctl;

                                                                        
  if (pltcl_pm_init_done)
  {
    return;
  }

  pg_bindtextdomain(TEXTDOMAIN);

#ifdef WIN32
                                                           
  Tcl_FindExecutable("");
#endif

     
                                                                            
     
  notifier.setTimerProc = pltcl_SetTimer;
  notifier.waitForEventProc = pltcl_WaitForEvent;
  notifier.createFileHandlerProc = pltcl_CreateFileHandler;
  notifier.deleteFileHandlerProc = pltcl_DeleteFileHandler;
  notifier.initNotifierProc = pltcl_InitNotifier;
  notifier.finalizeNotifierProc = pltcl_FinalizeNotifier;
  notifier.alertNotifierProc = pltcl_AlertNotifier;
  notifier.serviceModeHookProc = pltcl_ServiceModeHook;
  Tcl_SetNotifier(&notifier);

                                                                
                                                           
                                       
                                                                
  if ((pltcl_hold_interp = Tcl_CreateInterp()) == NULL)
  {
    elog(ERROR, "could not create master Tcl interpreter");
  }
  if (Tcl_Init(pltcl_hold_interp) == TCL_ERROR)
  {
    elog(ERROR, "could not initialize master Tcl interpreter");
  }

                                                                
                                                    
                                                                
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(pltcl_interp_desc);
  pltcl_interp_htab = hash_create("PL/Tcl interpreters", 8, &hash_ctl, HASH_ELEM | HASH_BLOBS);

                                                                
                                               
                                                                
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(pltcl_proc_key);
  hash_ctl.entrysize = sizeof(pltcl_proc_ptr);
  pltcl_proc_htab = hash_create("PL/Tcl functions", 100, &hash_ctl, HASH_ELEM | HASH_BLOBS);

                                                                
                                 
                                                                
  DefineCustomStringVariable("pltcl.start_proc", gettext_noop("PL/Tcl function to call once when pltcl is first used."), NULL, &pltcl_start_proc, NULL, PGC_SUSET, 0, NULL, NULL, NULL);
  DefineCustomStringVariable("pltclu.start_proc", gettext_noop("PL/TclU function to call once when pltclu is first used."), NULL, &pltclu_start_proc, NULL, PGC_SUSET, 0, NULL, NULL, NULL);

  pltcl_pm_init_done = true;
}

                                                                        
                                                          
                                                                        
static void
pltcl_init_interp(pltcl_interp_desc *interp_desc, Oid prolang, bool pltrusted)
{
  Tcl_Interp *interp;
  char interpname[32];

                                                                
                                                                 
                                                                  
                                              
                                                                
  snprintf(interpname, sizeof(interpname), "slave_%u", interp_desc->user_id);
  if ((interp = Tcl_CreateSlave(pltcl_hold_interp, interpname, pltrusted ? 1 : 0)) == NULL)
  {
    elog(ERROR, "could not create slave Tcl interpreter");
  }

                                                                
                                                                 
                                                                
  Tcl_InitHashTable(&interp_desc->query_hash, TCL_STRING_KEYS);

                                                                
                                                             
                                                                
  Tcl_CreateObjCommand(interp, "elog", pltcl_elog, NULL, NULL);
  Tcl_CreateObjCommand(interp, "quote", pltcl_quote, NULL, NULL);
  Tcl_CreateObjCommand(interp, "argisnull", pltcl_argisnull, NULL, NULL);
  Tcl_CreateObjCommand(interp, "return_null", pltcl_returnnull, NULL, NULL);
  Tcl_CreateObjCommand(interp, "return_next", pltcl_returnnext, NULL, NULL);
  Tcl_CreateObjCommand(interp, "spi_exec", pltcl_SPI_execute, NULL, NULL);
  Tcl_CreateObjCommand(interp, "spi_prepare", pltcl_SPI_prepare, NULL, NULL);
  Tcl_CreateObjCommand(interp, "spi_execp", pltcl_SPI_execute_plan, NULL, NULL);
  Tcl_CreateObjCommand(interp, "subtransaction", pltcl_subtransaction, NULL, NULL);
  Tcl_CreateObjCommand(interp, "commit", pltcl_commit, NULL, NULL);
  Tcl_CreateObjCommand(interp, "rollback", pltcl_rollback, NULL, NULL);

                                                                
                                                       
     
                                                                          
                                                                   
                                                                  
                                                                
  PG_TRY();
  {
    interp_desc->interp = interp;
    call_pltcl_start_proc(prolang, pltrusted);
  }
  PG_CATCH();
  {
    interp_desc->interp = NULL;
    Tcl_DeleteInterp(interp);
    PG_RE_THROW();
  }
  PG_END_TRY();
}

                                                                        
                                                                          
   
                                                                     
                                                                        
static pltcl_interp_desc *
pltcl_fetch_interp(Oid prolang, bool pltrusted)
{
  Oid user_id;
  pltcl_interp_desc *interp_desc;
  bool found;

                                                                      
  if (pltrusted)
  {
    user_id = GetUserId();
  }
  else
  {
    user_id = InvalidOid;
  }

  interp_desc = hash_search(pltcl_interp_htab, &user_id, HASH_ENTER, &found);
  if (!found)
  {
    interp_desc->interp = NULL;
  }

                                                                          
  if (!interp_desc->interp)
  {
    pltcl_init_interp(interp_desc, prolang, pltrusted);
  }

  return interp_desc;
}

                                                                        
                                                                            
                                                                        
static void
call_pltcl_start_proc(Oid prolang, bool pltrusted)
{
  LOCAL_FCINFO(fcinfo, 0);
  char *start_proc;
  const char *gucname;
  ErrorContextCallback errcallback;
  List *namelist;
  Oid fargtypes[1];            
  Oid procOid;
  HeapTuple procTup;
  Form_pg_proc procStruct;
  AclResult aclresult;
  FmgrInfo finfo;
  PgStat_FunctionCallUsage fcusage;

                              
  start_proc = pltrusted ? pltcl_start_proc : pltclu_start_proc;
  gucname = pltrusted ? "pltcl.start_proc" : "pltclu.start_proc";

                                            
  if (start_proc == NULL || start_proc[0] == '\0')
  {
    return;
  }

                                                              
  errcallback.callback = start_proc_error_callback;
  errcallback.arg = unconstify(char *, gucname);
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                                                                    
  namelist = stringToQualifiedNameList(start_proc);
  procOid = LookupFuncName(namelist, 0, fargtypes, false);

                                                          
  aclresult = pg_proc_aclcheck(procOid, GetUserId(), ACL_EXECUTE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_FUNCTION, start_proc);
  }

                                        
  procTup = SearchSysCache1(PROCOID, ObjectIdGetDatum(procOid));
  if (!HeapTupleIsValid(procTup))
  {
    elog(ERROR, "cache lookup failed for function %u", procOid);
  }
  procStruct = (Form_pg_proc)GETSTRUCT(procTup);

                                                                        
  if (procStruct->prolang != prolang)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("function \"%s\" is in the wrong language", start_proc)));
  }

     
                                                                      
                                                                             
                                                    
     
  if (procStruct->prosecdef)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("function \"%s\" must not be SECURITY DEFINER", start_proc)));
  }

            
  ReleaseSysCache(procTup);

     
                                                                         
                                                                            
                                                                       
                            
     
  InvokeFunctionExecuteHook(procOid);
  fmgr_info(procOid, &finfo);
  InitFunctionCallInfoData(*fcinfo, &finfo, 0, InvalidOid, NULL, NULL);
  pgstat_init_function_usage(fcinfo, &fcusage);
  (void)FunctionCallInvoke(fcinfo);
  pgstat_end_function_usage(&fcusage, true);

                                   
  error_context_stack = errcallback.previous;
}

   
                                                                             
   
static void
start_proc_error_callback(void *arg)
{
  const char *gucname = (const char *)arg;

                                                                   
  errcontext("processing %s parameter", gucname);
}

                                                                        
                                                           
                                              
                                             
                                            
                           
                                                                        
PG_FUNCTION_INFO_V1(pltcl_call_handler);

                     
Datum
pltcl_call_handler(PG_FUNCTION_ARGS)
{
  return pltcl_handler(fcinfo, true);
}

   
                                            
   
PG_FUNCTION_INFO_V1(pltclu_call_handler);

                     
Datum
pltclu_call_handler(PG_FUNCTION_ARGS)
{
  return pltcl_handler(fcinfo, false);
}

                                                                        
                                                                  
                                                   
                                                                        
static Datum
pltcl_handler(PG_FUNCTION_ARGS, bool pltrusted)
{
  Datum retval;
  pltcl_call_state current_call_state;
  pltcl_call_state *save_call_state;

     
                                                                           
                                                                     
                                                                          
                                                                             
                                                                          
                                                                          
                              
     
  memset(&current_call_state, 0, sizeof(current_call_state));

     
                                                           
     
  save_call_state = pltcl_current_call_state;
  pltcl_current_call_state = &current_call_state;

  PG_TRY();
  {
       
                                                                       
                  
       
    if (CALLED_AS_TRIGGER(fcinfo))
    {
                                      
      retval = PointerGetDatum(pltcl_trigger_handler(fcinfo, &current_call_state, pltrusted));
    }
    else if (CALLED_AS_EVENT_TRIGGER(fcinfo))
    {
                                            
      pltcl_event_trigger_handler(fcinfo, &current_call_state, pltrusted);
      retval = (Datum)0;
    }
    else
    {
                                               
      current_call_state.fcinfo = fcinfo;
      retval = pltcl_func_handler(fcinfo, &current_call_state, pltrusted);
    }
  }
  PG_CATCH();
  {
                                                                           
    pltcl_current_call_state = save_call_state;
    if (current_call_state.prodesc != NULL)
    {
      Assert(current_call_state.prodesc->fn_refcount > 0);
      if (--current_call_state.prodesc->fn_refcount == 0)
      {
        MemoryContextDelete(current_call_state.prodesc->fn_cxt);
      }
    }
    PG_RE_THROW();
  }
  PG_END_TRY();

                                                                         
                                                                             
  pltcl_current_call_state = save_call_state;
  if (current_call_state.prodesc != NULL)
  {
    Assert(current_call_state.prodesc->fn_refcount > 0);
    if (--current_call_state.prodesc->fn_refcount == 0)
    {
      MemoryContextDelete(current_call_state.prodesc->fn_cxt);
    }
  }

  return retval;
}

                                                                        
                                                              
                                                                        
static Datum
pltcl_func_handler(PG_FUNCTION_ARGS, pltcl_call_state *call_state, bool pltrusted)
{
  bool nonatomic;
  pltcl_proc_desc *prodesc;
  Tcl_Interp *volatile interp;
  Tcl_Obj *tcl_cmd;
  int i;
  int tcl_rc;
  Datum retval;

  nonatomic = fcinfo->context && IsA(fcinfo->context, CallContext) && !castNode(CallContext, fcinfo->context)->atomic;

                              
  if (SPI_connect_ext(nonatomic ? SPI_OPT_NONATOMIC : 0) != SPI_OK_CONNECT)
  {
    elog(ERROR, "could not connect to SPI manager");
  }

                                    
  prodesc = compile_pltcl_function(fcinfo->flinfo->fn_oid, InvalidOid, false, pltrusted);

  call_state->prodesc = prodesc;
  prodesc->fn_refcount++;

  interp = prodesc->interp_desc->interp;

     
                                                                        
                                                                      
                                                                             
                              
     
  if (prodesc->fn_retisset)
  {
    ReturnSetInfo *rsi = (ReturnSetInfo *)fcinfo->resultinfo;

    if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
    }

    call_state->rsi = rsi;
    call_state->tuple_store_cxt = rsi->econtext->ecxt_per_query_memory;
    call_state->tuple_store_owner = CurrentResourceOwner;
  }

                                                                
                                                 
                                 
                                                                
  tcl_cmd = Tcl_NewObj();
  Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(prodesc->internal_proname, -1));

                                                                     
  Tcl_IncrRefCount(tcl_cmd);

                                                                
                                           
                                                                
  PG_TRY();
  {
    for (i = 0; i < prodesc->nargs; i++)
    {
      if (prodesc->arg_is_rowtype[i])
      {
                                                            
                                                            
                                                            
        if (fcinfo->args[i].isnull)
        {
          Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewObj());
        }
        else
        {
          HeapTupleHeader td;
          Oid tupType;
          int32 tupTypmod;
          TupleDesc tupdesc;
          HeapTupleData tmptup;
          Tcl_Obj *list_tmp;

          td = DatumGetHeapTupleHeader(fcinfo->args[i].value);
                                                       
          tupType = HeapTupleHeaderGetTypeId(td);
          tupTypmod = HeapTupleHeaderGetTypMod(td);
          tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
                                                             
          tmptup.t_len = HeapTupleHeaderGetDatumLength(td);
          tmptup.t_data = td;

          list_tmp = pltcl_build_tuple_argument(&tmptup, tupdesc, true);
          Tcl_ListObjAppendElement(NULL, tcl_cmd, list_tmp);

          ReleaseTupleDesc(tupdesc);
        }
      }
      else
      {
                                                            
                                                     
                                            
                                                            
        if (fcinfo->args[i].isnull)
        {
          Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewObj());
        }
        else
        {
          char *tmp;

          tmp = OutputFunctionCall(&prodesc->arg_out_func[i], fcinfo->args[i].value);
          UTF_BEGIN;
          Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(UTF_E2U(tmp), -1));
          UTF_END;
          pfree(tmp);
        }
      }
    }
  }
  PG_CATCH();
  {
                                          
    Tcl_DecrRefCount(tcl_cmd);
    PG_RE_THROW();
  }
  PG_END_TRY();

                                                                
                           
     
                                                                  
                                                                
  tcl_rc = Tcl_EvalObjEx(interp, tcl_cmd, (TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL));

                                                                     
  Tcl_DecrRefCount(tcl_cmd);

                                                                
                                       
                                                                
  if (tcl_rc != TCL_OK)
  {
    throw_tcl_error(interp, prodesc->user_proname);
  }

                                                                
                                                            
                                                             
                                                          
                                                               
                                                             
                                                                
                   
                                                                
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish() failed");
  }

  if (prodesc->fn_retisset)
  {
    ReturnSetInfo *rsi = call_state->rsi;

                                       
    rsi->returnMode = SFRM_Materialize;

                                                         
    if (call_state->tuple_store)
    {
      rsi->setResult = call_state->tuple_store;
      if (call_state->ret_tupdesc)
      {
        MemoryContext oldcxt;

        oldcxt = MemoryContextSwitchTo(call_state->tuple_store_cxt);
        rsi->setDesc = CreateTupleDescCopy(call_state->ret_tupdesc);
        MemoryContextSwitchTo(oldcxt);
      }
    }
    retval = (Datum)0;
    fcinfo->isnull = true;
  }
  else if (fcinfo->isnull)
  {
    retval = InputFunctionCall(&prodesc->result_in_func, NULL, prodesc->result_typioparam, -1);
  }
  else if (prodesc->fn_retistuple)
  {
    TupleDesc td;
    HeapTuple tup;
    Tcl_Obj *resultObj;
    Tcl_Obj **resultObjv;
    int resultObjc;

       
                                                                     
                                                                         
                                                                           
                                                                     
                                                                           
                                      
       
    switch (get_call_result_type(fcinfo, NULL, &td))
    {
    case TYPEFUNC_COMPOSITE:
                   
      break;
    case TYPEFUNC_COMPOSITE_DOMAIN:
      Assert(prodesc->fn_retisdomain);
      break;
    case TYPEFUNC_RECORD:
                                                     
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function returning record called in context "
                                                                     "that cannot accept type record")));
      break;
    default:
                                        
      elog(ERROR, "return type must be a row type");
      break;
    }

    Assert(!call_state->ret_tupdesc);
    Assert(!call_state->attinmeta);
    call_state->ret_tupdesc = td;
    call_state->attinmeta = TupleDescGetAttInMetadata(td);

                                          
    resultObj = Tcl_GetObjResult(interp);
    if (Tcl_ListObjGetElements(interp, resultObj, &resultObjc, &resultObjv) == TCL_ERROR)
    {
      throw_tcl_error(interp, prodesc->user_proname);
    }

    tup = pltcl_build_tuple_result(interp, resultObjv, resultObjc, call_state);
    retval = HeapTupleGetDatum(tup);
  }
  else
  {
    retval = InputFunctionCall(&prodesc->result_in_func, utf_u2e(Tcl_GetStringResult(interp)), prodesc->result_typioparam, -1);
  }

  return retval;
}

                                                                        
                                                       
                                                                        
static HeapTuple
pltcl_trigger_handler(PG_FUNCTION_ARGS, pltcl_call_state *call_state, bool pltrusted)
{
  pltcl_proc_desc *prodesc;
  Tcl_Interp *volatile interp;
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  char *stroid;
  TupleDesc tupdesc;
  volatile HeapTuple rettup;
  Tcl_Obj *tcl_cmd;
  Tcl_Obj *tcl_trigtup;
  int tcl_rc;
  int i;
  const char *result;
  int result_Objc;
  Tcl_Obj **result_Objv;
  int rc PG_USED_FOR_ASSERTS_ONLY;

  call_state->trigdata = trigdata;

                              
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "could not connect to SPI manager");
  }

                                                             
  rc = SPI_register_trigger_data(trigdata);
  Assert(rc >= 0);

                                    
  prodesc = compile_pltcl_function(fcinfo->flinfo->fn_oid, RelationGetRelid(trigdata->tg_relation), false,                           
      pltrusted);

  call_state->prodesc = prodesc;
  prodesc->fn_refcount++;

  interp = prodesc->interp_desc->interp;

  tupdesc = RelationGetDescr(trigdata->tg_relation);

                                                                
                                                 
                             
                                                                
  tcl_cmd = Tcl_NewObj();
  Tcl_IncrRefCount(tcl_cmd);

  PG_TRY();
  {
                                                                    
    Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(prodesc->internal_proname, -1));

                                               
    Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(utf_e2u(trigdata->tg_trigger->tgname), -1));

                                                               
                                                                   
    stroid = DatumGetCString(DirectFunctionCall1(oidout, ObjectIdGetDatum(trigdata->tg_relation->rd_id)));
    Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(stroid, -1));
    pfree(stroid);

                                                                       
    stroid = SPI_getrelname(trigdata->tg_relation);
    Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(utf_e2u(stroid), -1));
    pfree(stroid);

                                                                           
    stroid = SPI_getnspname(trigdata->tg_relation);
    Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(utf_e2u(stroid), -1));
    pfree(stroid);

                                                           
    tcl_trigtup = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, tcl_trigtup, Tcl_NewObj());
    for (i = 0; i < tupdesc->natts; i++)
    {
      Form_pg_attribute att = TupleDescAttr(tupdesc, i);

      if (att->attisdropped)
      {
        Tcl_ListObjAppendElement(NULL, tcl_trigtup, Tcl_NewObj());
      }
      else
      {
        Tcl_ListObjAppendElement(NULL, tcl_trigtup, Tcl_NewStringObj(utf_e2u(NameStr(att->attname)), -1));
      }
    }
    Tcl_ListObjAppendElement(NULL, tcl_cmd, tcl_trigtup);

                                                
    if (TRIGGER_FIRED_BEFORE(trigdata->tg_event))
    {
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("BEFORE", -1));
    }
    else if (TRIGGER_FIRED_AFTER(trigdata->tg_event))
    {
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("AFTER", -1));
    }
    else if (TRIGGER_FIRED_INSTEAD(trigdata->tg_event))
    {
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("INSTEAD OF", -1));
    }
    else
    {
      elog(ERROR, "unrecognized WHEN tg_event: %u", trigdata->tg_event);
    }

                                                  
    if (TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
    {
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("ROW", -1));

         
                                                                      
                 
         
                                                                   
                                                                 
         
      if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("INSERT", -1));

        Tcl_ListObjAppendElement(NULL, tcl_cmd, pltcl_build_tuple_argument(trigdata->tg_trigtuple, tupdesc, !TRIGGER_FIRED_BEFORE(trigdata->tg_event)));
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewObj());

        rettup = trigdata->tg_trigtuple;
      }
      else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("DELETE", -1));

        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewObj());
        Tcl_ListObjAppendElement(NULL, tcl_cmd, pltcl_build_tuple_argument(trigdata->tg_trigtuple, tupdesc, true));

        rettup = trigdata->tg_trigtuple;
      }
      else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("UPDATE", -1));

        Tcl_ListObjAppendElement(NULL, tcl_cmd, pltcl_build_tuple_argument(trigdata->tg_newtuple, tupdesc, !TRIGGER_FIRED_BEFORE(trigdata->tg_event)));
        Tcl_ListObjAppendElement(NULL, tcl_cmd, pltcl_build_tuple_argument(trigdata->tg_trigtuple, tupdesc, true));

        rettup = trigdata->tg_newtuple;
      }
      else
      {
        elog(ERROR, "unrecognized OP tg_event: %u", trigdata->tg_event);
      }
    }
    else if (TRIGGER_FIRED_FOR_STATEMENT(trigdata->tg_event))
    {
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("STATEMENT", -1));

      if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("INSERT", -1));
      }
      else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("DELETE", -1));
      }
      else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("UPDATE", -1));
      }
      else if (TRIGGER_FIRED_BY_TRUNCATE(trigdata->tg_event))
      {
        Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj("TRUNCATE", -1));
      }
      else
      {
        elog(ERROR, "unrecognized OP tg_event: %u", trigdata->tg_event);
      }

      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewObj());
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewObj());

      rettup = (HeapTuple)NULL;
    }
    else
    {
      elog(ERROR, "unrecognized LEVEL tg_event: %u", trigdata->tg_event);
    }

                                                          
    for (i = 0; i < trigdata->tg_trigger->tgnargs; i++)
    {
      Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(utf_e2u(trigdata->tg_trigger->tgargs[i]), -1));
    }
  }
  PG_CATCH();
  {
    Tcl_DecrRefCount(tcl_cmd);
    PG_RE_THROW();
  }
  PG_END_TRY();

                                                                
                           
     
                                                                  
                                                                
  tcl_rc = Tcl_EvalObjEx(interp, tcl_cmd, (TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL));

                                                                     
  Tcl_DecrRefCount(tcl_cmd);

                                                                
                                       
                                                                
  if (tcl_rc != TCL_OK)
  {
    throw_tcl_error(interp, prodesc->user_proname);
  }

                                                                
                           
                                                                
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish() failed");
  }

                                                                
                                                         
                                                             
                                                                  
                                                                
  result = Tcl_GetStringResult(interp);

  if (strcmp(result, "OK") == 0)
  {
    return rettup;
  }
  if (strcmp(result, "SKIP") == 0)
  {
    return (HeapTuple)NULL;
  }

                                                                
                                                                    
                                              
                                                                
  if (Tcl_ListObjGetElements(interp, Tcl_GetObjResult(interp), &result_Objc, &result_Objv) != TCL_OK)
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("could not split return value from trigger: %s", utf_u2e(Tcl_GetStringResult(interp)))));
  }

                                        
  rettup = pltcl_build_tuple_result(interp, result_Objv, result_Objc, call_state);

  return rettup;
}

                                                                        
                                                                   
                                                                        
static void
pltcl_event_trigger_handler(PG_FUNCTION_ARGS, pltcl_call_state *call_state, bool pltrusted)
{
  pltcl_proc_desc *prodesc;
  Tcl_Interp *volatile interp;
  EventTriggerData *tdata = (EventTriggerData *)fcinfo->context;
  Tcl_Obj *tcl_cmd;
  int tcl_rc;

                              
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "could not connect to SPI manager");
  }

                                    
  prodesc = compile_pltcl_function(fcinfo->flinfo->fn_oid, InvalidOid, true, pltrusted);

  call_state->prodesc = prodesc;
  prodesc->fn_refcount++;

  interp = prodesc->interp_desc->interp;

                                                         
  tcl_cmd = Tcl_NewObj();
  Tcl_IncrRefCount(tcl_cmd);
  Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(prodesc->internal_proname, -1));
  Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(utf_e2u(tdata->event), -1));
  Tcl_ListObjAppendElement(NULL, tcl_cmd, Tcl_NewStringObj(utf_e2u(tdata->tag), -1));

  tcl_rc = Tcl_EvalObjEx(interp, tcl_cmd, (TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL));

                                                                     
  Tcl_DecrRefCount(tcl_cmd);

                                         
  if (tcl_rc != TCL_OK)
  {
    throw_tcl_error(interp, prodesc->user_proname);
  }

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish() failed");
  }
}

                                                                        
                                                                        
                                                                        
static void
throw_tcl_error(Tcl_Interp *interp, const char *proname)
{
     
                                                                   
                                                                          
                                                                             
                                                                         
                 
     
  char *emsg;
  char *econtext;

  emsg = pstrdup(utf_u2e(Tcl_GetStringResult(interp)));
  econtext = utf_u2e(Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
  ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", emsg), errcontext("%s\nin PL/Tcl function \"%s\"", econtext, proname)));
}

                                                                        
                                                                         
   
                                                                         
                                                 
                                                                        
static pltcl_proc_desc *
compile_pltcl_function(Oid fn_oid, Oid tgreloid, bool is_event_trigger, bool pltrusted)
{
  HeapTuple procTup;
  Form_pg_proc procStruct;
  pltcl_proc_key proc_key;
  pltcl_proc_ptr *proc_ptr;
  bool found;
  pltcl_proc_desc *prodesc;
  pltcl_proc_desc *old_prodesc;
  volatile MemoryContext proc_cxt = NULL;
  Tcl_DString proc_internal_def;
  Tcl_DString proc_internal_body;

                                                   
  procTup = SearchSysCache1(PROCOID, ObjectIdGetDatum(fn_oid));
  if (!HeapTupleIsValid(procTup))
  {
    elog(ERROR, "cache lookup failed for function %u", fn_oid);
  }
  procStruct = (Form_pg_proc)GETSTRUCT(procTup);

     
                                                                             
                                           
     
  proc_key.proc_id = fn_oid;
  proc_key.is_trigger = OidIsValid(tgreloid);
  proc_key.user_id = pltrusted ? GetUserId() : InvalidOid;

  proc_ptr = hash_search(pltcl_proc_htab, &proc_key, HASH_ENTER, &found);
  if (!found)
  {
    proc_ptr->proc_ptr = NULL;
  }

  prodesc = proc_ptr->proc_ptr;

                                                                
                                                                
                                                                      
                                                        
                                                                
  if (prodesc != NULL && prodesc->fn_xmin == HeapTupleHeaderGetRawXmin(procTup->t_data) && ItemPointerEquals(&prodesc->fn_tid, &procTup->t_self))
  {
                                                 
    ReleaseSysCache(procTup);
    return prodesc;
  }

                                                                
                                                         
                                                      
                                                           
                                   
     
                                                          
                                                                
  Tcl_DStringInit(&proc_internal_def);
  Tcl_DStringInit(&proc_internal_body);
  PG_TRY();
  {
    bool is_trigger = OidIsValid(tgreloid);
    char internal_proname[128];
    HeapTuple typeTup;
    Form_pg_type typeStruct;
    char proc_internal_args[33 * FUNC_MAX_ARGS];
    Datum prosrcdatum;
    bool isnull;
    char *proc_source;
    char buf[48];
    Tcl_Interp *interp;
    int i;
    int tcl_rc;
    MemoryContext oldcontext;

                                                                  
                                                                     
                                                                    
                                                              
                                                                  
    if (is_event_trigger)
    {
      snprintf(internal_proname, sizeof(internal_proname), "__PLTcl_proc_%u_evttrigger", fn_oid);
    }
    else if (is_trigger)
    {
      snprintf(internal_proname, sizeof(internal_proname), "__PLTcl_proc_%u_trigger", fn_oid);
    }
    else
    {
      snprintf(internal_proname, sizeof(internal_proname), "__PLTcl_proc_%u", fn_oid);
    }

                                                                  
                                                                        
                                                                  
    proc_cxt = AllocSetContextCreate(TopMemoryContext, "PL/Tcl function", ALLOCSET_SMALL_SIZES);

                                                                  
                                                            
                                                                     
                                                                  
    oldcontext = MemoryContextSwitchTo(proc_cxt);
    prodesc = (pltcl_proc_desc *)palloc0(sizeof(pltcl_proc_desc));
    prodesc->user_proname = pstrdup(NameStr(procStruct->proname));
    MemoryContextSetIdentifier(proc_cxt, prodesc->user_proname);
    prodesc->internal_proname = pstrdup(internal_proname);
    prodesc->fn_cxt = proc_cxt;
    prodesc->fn_refcount = 0;
    prodesc->fn_xmin = HeapTupleHeaderGetRawXmin(procTup->t_data);
    prodesc->fn_tid = procTup->t_self;
    prodesc->nargs = procStruct->pronargs;
    prodesc->arg_out_func = (FmgrInfo *)palloc0(prodesc->nargs * sizeof(FmgrInfo));
    prodesc->arg_is_rowtype = (bool *)palloc0(prodesc->nargs * sizeof(bool));
    MemoryContextSwitchTo(oldcontext);

                                                  
    prodesc->fn_readonly = (procStruct->provolatile != PROVOLATILE_VOLATILE);
                                   
    prodesc->lanpltrusted = pltrusted;

                                                                  
                                                        
                                                                  
    prodesc->interp_desc = pltcl_fetch_interp(procStruct->prolang, prodesc->lanpltrusted);
    interp = prodesc->interp_desc->interp;

                                                                  
                                                                
                     
                                                                  
    if (!is_trigger && !is_event_trigger)
    {
      Oid rettype = procStruct->prorettype;

      typeTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(rettype));
      if (!HeapTupleIsValid(typeTup))
      {
        elog(ERROR, "cache lookup failed for type %u", rettype);
      }
      typeStruct = (Form_pg_type)GETSTRUCT(typeTup);

                                                              
      if (typeStruct->typtype == TYPTYPE_PSEUDO)
      {
        if (rettype == VOIDOID || rettype == RECORDOID)
                    ;
        else if (rettype == TRIGGEROID || rettype == EVTTRIGGEROID)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("trigger functions can only be called as triggers")));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/Tcl functions cannot return type %s", format_type_be(rettype))));
        }
      }

      prodesc->result_typid = rettype;
      fmgr_info_cxt(typeStruct->typinput, &(prodesc->result_in_func), proc_cxt);
      prodesc->result_typioparam = getTypeIOParam(typeTup);

      prodesc->fn_retisset = procStruct->proretset;
      prodesc->fn_retistuple = type_is_rowtype(rettype);
      prodesc->fn_retisdomain = (typeStruct->typtype == TYPTYPE_DOMAIN);
      prodesc->domain_info = NULL;

      ReleaseSysCache(typeTup);
    }

                                                                  
                                                          
                                                                    
                                                                  
    if (!is_trigger && !is_event_trigger)
    {
      proc_internal_args[0] = '\0';
      for (i = 0; i < prodesc->nargs; i++)
      {
        Oid argtype = procStruct->proargtypes.values[i];

        typeTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(argtype));
        if (!HeapTupleIsValid(typeTup))
        {
          elog(ERROR, "cache lookup failed for type %u", argtype);
        }
        typeStruct = (Form_pg_type)GETSTRUCT(typeTup);

                                                         
        if (typeStruct->typtype == TYPTYPE_PSEUDO && argtype != RECORDOID)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/Tcl functions cannot accept type %s", format_type_be(argtype))));
        }

        if (type_is_rowtype(argtype))
        {
          prodesc->arg_is_rowtype[i] = true;
          snprintf(buf, sizeof(buf), "__PLTcl_Tup_%d", i + 1);
        }
        else
        {
          prodesc->arg_is_rowtype[i] = false;
          fmgr_info_cxt(typeStruct->typoutput, &(prodesc->arg_out_func[i]), proc_cxt);
          snprintf(buf, sizeof(buf), "%d", i + 1);
        }

        if (i > 0)
        {
          strcat(proc_internal_args, " ");
        }
        strcat(proc_internal_args, buf);

        ReleaseSysCache(typeTup);
      }
    }
    else if (is_trigger)
    {
                                            
      strcpy(proc_internal_args, "TG_name TG_relid TG_table_name TG_table_schema TG_relatts TG_when TG_level TG_op __PLTcl_Tup_NEW __PLTcl_Tup_OLD args");
    }
    else if (is_event_trigger)
    {
                                                  
      strcpy(proc_internal_args, "TG_event TG_tag");
    }

                                                                  
                                                     
                 
       
                                                                      
                                                                       
                                                          
                                                                  
    Tcl_DStringAppendElement(&proc_internal_def, "proc");
    Tcl_DStringAppendElement(&proc_internal_def, internal_proname);
    Tcl_DStringAppendElement(&proc_internal_def, proc_internal_args);

                                                                  
                                  
                                       
                                                 
                                                                  
    Tcl_DStringAppend(&proc_internal_body, "upvar #0 ", -1);
    Tcl_DStringAppend(&proc_internal_body, internal_proname, -1);
    Tcl_DStringAppend(&proc_internal_body, " GD\n", -1);
    if (is_trigger)
    {
      Tcl_DStringAppend(&proc_internal_body, "array set NEW $__PLTcl_Tup_NEW\n", -1);
      Tcl_DStringAppend(&proc_internal_body, "array set OLD $__PLTcl_Tup_OLD\n", -1);
      Tcl_DStringAppend(&proc_internal_body,
          "set i 0\n"
          "set v 0\n"
          "foreach v $args {\n"
          "  incr i\n"
          "  set $i $v\n"
          "}\n"
          "unset i v\n\n",
          -1);
    }
    else if (is_event_trigger)
    {
                                                  
    }
    else
    {
      for (i = 0; i < prodesc->nargs; i++)
      {
        if (prodesc->arg_is_rowtype[i])
        {
          snprintf(buf, sizeof(buf), "array set %d $__PLTcl_Tup_%d\n", i + 1, i + 1);
          Tcl_DStringAppend(&proc_internal_body, buf, -1);
        }
      }
    }

                                                                  
                                                   
                                                                  
    prosrcdatum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc");
    }
    proc_source = TextDatumGetCString(prosrcdatum);
    UTF_BEGIN;
    Tcl_DStringAppend(&proc_internal_body, UTF_E2U(proc_source), -1);
    UTF_END;
    pfree(proc_source);
    Tcl_DStringAppendElement(&proc_internal_def, Tcl_DStringValue(&proc_internal_body));

                                                                  
                                               
                                                                  
    tcl_rc = Tcl_EvalEx(interp, Tcl_DStringValue(&proc_internal_def), Tcl_DStringLength(&proc_internal_def), TCL_EVAL_GLOBAL);
    if (tcl_rc != TCL_OK)
    {
      ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("could not create internal procedure \"%s\": %s", internal_proname, utf_u2e(Tcl_GetStringResult(interp)))));
    }
  }
  PG_CATCH();
  {
       
                                                                         
                                                               
       
    if (proc_cxt)
    {
      MemoryContextDelete(proc_cxt);
    }
    Tcl_DStringFree(&proc_internal_def);
    Tcl_DStringFree(&proc_internal_body);
    PG_RE_THROW();
  }
  PG_END_TRY();

     
                                                                           
                                                                        
                                                                           
                                                                    
                                                                      
                                                                            
                                                                        
                                                                   
     
  old_prodesc = proc_ptr->proc_ptr;

  proc_ptr->proc_ptr = prodesc;
  prodesc->fn_refcount++;

  if (old_prodesc != NULL)
  {
    Assert(old_prodesc->fn_refcount > 0);
    if (--old_prodesc->fn_refcount == 0)
    {
      MemoryContextDelete(old_prodesc->fn_cxt);
    }
  }

  Tcl_DStringFree(&proc_internal_def);
  Tcl_DStringFree(&proc_internal_body);

  ReleaseSysCache(procTup);

  return prodesc;
}

                                                                        
                                            
                                                                        
static int
pltcl_elog(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  volatile int level;
  MemoryContext oldcontext;
  int priIndex;

  static const char *logpriorities[] = {"DEBUG", "LOG", "INFO", "NOTICE", "WARNING", "ERROR", "FATAL", (const char *)NULL};

  static const int loglevels[] = {DEBUG2, LOG, INFO, NOTICE, WARNING, ERROR, FATAL};

  if (objc != 3)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "level msg");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], logpriorities, "priority", TCL_EXACT, &priIndex) != TCL_OK)
  {
    return TCL_ERROR;
  }

  level = loglevels[priIndex];

  if (level == ERROR)
  {
       
                                                                      
                                                                     
                
       
    Tcl_SetObjResult(interp, objv[2]);
    return TCL_ERROR;
  }

     
                                                                           
                                                                         
                                                                            
                                                                          
     
                                                                         
                        
     
  oldcontext = CurrentMemoryContext;
  PG_TRY();
  {
    UTF_BEGIN;
    ereport(level, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", UTF_U2E(Tcl_GetString(objv[2])))));
    UTF_END;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                                   
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                    
    pltcl_construct_errorCode(interp, edata);
    UTF_BEGIN;
    Tcl_SetObjResult(interp, Tcl_NewStringObj(UTF_E2U(edata->message), -1));
    UTF_END;
    FreeErrorData(edata);

    return TCL_ERROR;
  }
  PG_END_TRY();

  return TCL_OK;
}

                                                                        
                                                            
                                                              
                                                                        
static void
pltcl_construct_errorCode(Tcl_Interp *interp, ErrorData *edata)
{
  Tcl_Obj *obj = Tcl_NewObj();

  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("POSTGRES", -1));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(PG_VERSION, -1));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("SQLSTATE", -1));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(unpack_sql_state(edata->sqlerrcode), -1));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("condition", -1));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(pltcl_get_condition_name(edata->sqlerrcode), -1));
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("message", -1));
  UTF_BEGIN;
  Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->message), -1));
  UTF_END;
  if (edata->detail)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("detail", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->detail), -1));
    UTF_END;
  }
  if (edata->hint)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("hint", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->hint), -1));
    UTF_END;
  }
  if (edata->context)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("context", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->context), -1));
    UTF_END;
  }
  if (edata->schema_name)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("schema", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->schema_name), -1));
    UTF_END;
  }
  if (edata->table_name)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("table", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->table_name), -1));
    UTF_END;
  }
  if (edata->column_name)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("column", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->column_name), -1));
    UTF_END;
  }
  if (edata->datatype_name)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("datatype", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->datatype_name), -1));
    UTF_END;
  }
  if (edata->constraint_name)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("constraint", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->constraint_name), -1));
    UTF_END;
  }
                                                                      
  if (edata->internalquery)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("statement", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->internalquery), -1));
    UTF_END;
  }
  if (edata->internalpos > 0)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("cursor_position", -1));
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewIntObj(edata->internalpos));
  }
  if (edata->filename)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("filename", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->filename), -1));
    UTF_END;
  }
  if (edata->lineno > 0)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("lineno", -1));
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewIntObj(edata->lineno));
  }
  if (edata->funcname)
  {
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj("funcname", -1));
    UTF_BEGIN;
    Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(UTF_E2U(edata->funcname), -1));
    UTF_END;
  }

  Tcl_SetObjErrorCode(interp, obj);
}

                                                                        
                                                       
                                                                        
static const char *
pltcl_get_condition_name(int sqlstate)
{
  int i;

  for (i = 0; exception_name_map[i].label != NULL; i++)
  {
    if (exception_name_map[i].sqlerrstate == sqlstate)
    {
      return exception_name_map[i].label;
    }
  }
  return "unrecognized_sqlstate";
}

                                                                        
                                                     
                                            
                                                                        
static int
pltcl_quote(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  char *tmp;
  const char *cp1;
  char *cp2;
  int length;

                                                                
                       
                                                                
  if (objc != 2)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "string");
    return TCL_ERROR;
  }

                                                                
                                                   
                                     
                                                                
  cp1 = Tcl_GetStringFromObj(objv[1], &length);
  tmp = palloc(length * 2 + 1);
  cp2 = tmp;

                                                                
                                                              
                                                                
  while (*cp1)
  {
    if (*cp1 == '\'')
    {
      *cp2++ = '\'';
    }
    else
    {
      if (*cp1 == '\\')
      {
        *cp2++ = '\\';
      }
    }
    *cp2++ = *cp1++;
  }

                                                                
                                               
                                                                
  *cp2 = '\0';
  Tcl_SetObjResult(interp, Tcl_NewStringObj(tmp, -1));
  pfree(tmp);
  return TCL_OK;
}

                                                                        
                                                                
                                                                        
static int
pltcl_argisnull(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  int argno;
  FunctionCallInfo fcinfo = pltcl_current_call_state->fcinfo;

                                                                
                       
                                                                
  if (objc != 2)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "argno");
    return TCL_ERROR;
  }

                                                                
                                                  
                                                                
  if (fcinfo == NULL)
  {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("argisnull cannot be used in triggers", -1));
    return TCL_ERROR;
  }

                                                                
                             
                                                                
  if (Tcl_GetIntFromObj(interp, objv[1], &argno) != TCL_OK)
  {
    return TCL_ERROR;
  }

                                                                
                                   
                                                                
  argno--;
  if (argno < 0 || argno >= fcinfo->nargs)
  {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("argno out of range", -1));
    return TCL_ERROR;
  }

                                                                
                                  
                                                                
  Tcl_SetObjResult(interp, Tcl_NewBooleanObj(PG_ARGISNULL(argno)));
  return TCL_OK;
}

                                                                        
                                                                      
                                                                        
static int
pltcl_returnnull(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  FunctionCallInfo fcinfo = pltcl_current_call_state->fcinfo;

                                                                
                       
                                                                
  if (objc != 1)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "");
    return TCL_ERROR;
  }

                                                                
                                                  
                                                                
  if (fcinfo == NULL)
  {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("return_null cannot be used in triggers", -1));
    return TCL_ERROR;
  }

                                                                
                                                               
                
                                                                
  fcinfo->isnull = true;

  return TCL_RETURN;
}

                                                                        
                                                                     
                                                                        
static int
pltcl_returnnext(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  pltcl_call_state *call_state = pltcl_current_call_state;
  FunctionCallInfo fcinfo = call_state->fcinfo;
  pltcl_proc_desc *prodesc = call_state->prodesc;
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;
  volatile int result = TCL_OK;

     
                                                         
     
  if (fcinfo == NULL)
  {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("return_next cannot be used in triggers", -1));
    return TCL_ERROR;
  }

  if (!prodesc->fn_retisset)
  {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("return_next cannot be used in non-set-returning functions", -1));
    return TCL_ERROR;
  }

     
                       
     
  if (objc != 2)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "result");
    return TCL_ERROR;
  }

     
                                                                        
     
                                                                       
                                                                            
                                                                         
                                                             
     
  BeginInternalSubTransaction(NULL);
  PG_TRY();
  {
                                                
    if (call_state->tuple_store == NULL)
    {
      pltcl_init_tuple_store(call_state);
    }

    if (prodesc->fn_retistuple)
    {
      Tcl_Obj **rowObjv;
      int rowObjc;

                                                     
      if (Tcl_ListObjGetElements(interp, objv[1], &rowObjc, &rowObjv) == TCL_ERROR)
      {
        result = TCL_ERROR;
      }
      else
      {
        HeapTuple tuple;

        tuple = pltcl_build_tuple_result(interp, rowObjv, rowObjc, call_state);
        tuplestore_puttuple(call_state->tuple_store, tuple);
      }
    }
    else
    {
      Datum retval;
      bool isNull = false;

                                                                          
      if (call_state->ret_tupdesc->natts != 1)
      {
        elog(ERROR, "wrong result type supplied in return_next");
      }

      retval = InputFunctionCall(&prodesc->result_in_func, utf_u2e((char *)Tcl_GetString(objv[1])), prodesc->result_typioparam, -1);
      tuplestore_putvalues(call_state->tuple_store, call_state->ret_tupdesc, &retval, &isNull);
    }

    pltcl_subtrans_commit(oldcontext, oldowner);
  }
  PG_CATCH();
  {
    pltcl_subtrans_abort(interp, oldcontext, oldowner);
    return TCL_ERROR;
  }
  PG_END_TRY();

  return result;
}

             
                                                             
   
                              
   
                                                    
                                                  
   
       
                                               
             
     
                        
                                                 
     
               
     
                                                        
                      
     
                 
                  
             
   
static void
pltcl_subtrans_begin(MemoryContext oldcontext, ResourceOwner oldowner)
{
  BeginInternalSubTransaction(NULL);

                                                    
  MemoryContextSwitchTo(oldcontext);
}

static void
pltcl_subtrans_commit(MemoryContext oldcontext, ResourceOwner oldowner)
{
                                                                  
  ReleaseCurrentSubTransaction();
  MemoryContextSwitchTo(oldcontext);
  CurrentResourceOwner = oldowner;
}

static void
pltcl_subtrans_abort(Tcl_Interp *interp, MemoryContext oldcontext, ResourceOwner oldowner)
{
  ErrorData *edata;

                       
  MemoryContextSwitchTo(oldcontext);
  edata = CopyErrorData();
  FlushErrorState();

                                   
  RollbackAndReleaseCurrentSubTransaction();
  MemoryContextSwitchTo(oldcontext);
  CurrentResourceOwner = oldowner;

                                  
  pltcl_construct_errorCode(interp, edata);
  UTF_BEGIN;
  Tcl_SetObjResult(interp, Tcl_NewStringObj(UTF_E2U(edata->message), -1));
  UTF_END;
  FreeErrorData(edata);
}

                                                                        
                                                          
                                
                                                                        
static int
pltcl_SPI_execute(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  int my_rc;
  int spi_rc;
  int query_idx;
  int i;
  int optIndex;
  int count = 0;
  const char *volatile arrayname = NULL;
  Tcl_Obj *volatile loop_body = NULL;
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

  enum options
  {
    OPT_ARRAY,
    OPT_COUNT
  };

  static const char *options[] = {"-array", "-count", (const char *)NULL};

                                                                
                                               
                                                                
  if (objc < 2)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "?-count n? ?-array name? query ?loop body?");
    return TCL_ERROR;
  }

  i = 1;
  while (i < objc)
  {
    if (Tcl_GetIndexFromObj(NULL, objv[i], options, NULL, TCL_EXACT, &optIndex) != TCL_OK)
    {
      break;
    }

    if (++i >= objc)
    {
      Tcl_SetObjResult(interp, Tcl_NewStringObj("missing argument to -count or -array", -1));
      return TCL_ERROR;
    }

    switch ((enum options)optIndex)
    {
    case OPT_ARRAY:
      arrayname = Tcl_GetString(objv[i++]);
      break;

    case OPT_COUNT:
      if (Tcl_GetIntFromObj(interp, objv[i++], &count) != TCL_OK)
      {
        return TCL_ERROR;
      }
      break;
    }
  }

  query_idx = i;
  if (query_idx >= objc || query_idx + 2 < objc)
  {
    Tcl_WrongNumArgs(interp, query_idx - 1, objv, "query ?loop body?");
    return TCL_ERROR;
  }

  if (query_idx + 1 < objc)
  {
    loop_body = objv[query_idx + 1];
  }

                                                                
                                                                     
                   
                                                                

  pltcl_subtrans_begin(oldcontext, oldowner);

  PG_TRY();
  {
    UTF_BEGIN;
    spi_rc = SPI_execute(UTF_U2E(Tcl_GetString(objv[query_idx])), pltcl_current_call_state->prodesc->fn_readonly, count);
    UTF_END;

    my_rc = pltcl_process_SPI_result(interp, arrayname, loop_body, spi_rc, SPI_tuptable, SPI_processed);

    pltcl_subtrans_commit(oldcontext, oldowner);
  }
  PG_CATCH();
  {
    pltcl_subtrans_abort(interp, oldcontext, oldowner);
    return TCL_ERROR;
  }
  PG_END_TRY();

  return my_rc;
}

   
                                                           
   
                                                                    
   
static int
pltcl_process_SPI_result(Tcl_Interp *interp, const char *arrayname, Tcl_Obj *loop_body, int spi_rc, SPITupleTable *tuptable, uint64 ntuples)
{
  int my_rc = TCL_OK;
  int loop_rc;
  HeapTuple *tuples;
  TupleDesc tupdesc;

  switch (spi_rc)
  {
  case SPI_OK_SELINTO:
  case SPI_OK_INSERT:
  case SPI_OK_DELETE:
  case SPI_OK_UPDATE:
    Tcl_SetObjResult(interp, Tcl_NewWideIntObj(ntuples));
    break;

  case SPI_OK_UTILITY:
  case SPI_OK_REWRITTEN:
    if (tuptable == NULL)
    {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
      break;
    }
                                                   
                     

  case SPI_OK_SELECT:
  case SPI_OK_INSERT_RETURNING:
  case SPI_OK_DELETE_RETURNING:
  case SPI_OK_UPDATE_RETURNING:

       
                                 
       
    tuples = tuptable->vals;
    tupdesc = tuptable->tupdesc;

    if (loop_body == NULL)
    {
         
                                                                     
                                  
         
      if (ntuples > 0)
      {
        pltcl_set_tuple_values(interp, arrayname, 0, tuples[0], tupdesc);
      }
    }
    else
    {
         
                                                                    
                      
         
      uint64 i;

      for (i = 0; i < ntuples; i++)
      {
        pltcl_set_tuple_values(interp, arrayname, i, tuples[i], tupdesc);

        loop_rc = Tcl_EvalObjEx(interp, loop_body, 0);

        if (loop_rc == TCL_OK)
        {
          continue;
        }
        if (loop_rc == TCL_CONTINUE)
        {
          continue;
        }
        if (loop_rc == TCL_RETURN)
        {
          my_rc = TCL_RETURN;
          break;
        }
        if (loop_rc == TCL_BREAK)
        {
          break;
        }
        my_rc = TCL_ERROR;
        break;
      }
    }

    if (my_rc == TCL_OK)
    {
      Tcl_SetObjResult(interp, Tcl_NewWideIntObj(ntuples));
    }
    break;

  default:
    Tcl_AppendResult(interp, "pltcl: SPI_execute failed: ", SPI_result_code_string(spi_rc), NULL);
    my_rc = TCL_ERROR;
    break;
  }

  SPI_freetuptable(tuptable);

  return my_rc;
}

                                                                        
                                                             
                                    
                                    
                                           
                                              
                                         
                                                                        
static int
pltcl_SPI_prepare(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  volatile MemoryContext plan_cxt = NULL;
  int nargs;
  Tcl_Obj **argsObj;
  pltcl_query_desc *qdesc;
  int i;
  Tcl_HashEntry *hashent;
  int hashnew;
  Tcl_HashTable *query_hash;
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

                                                                
                           
                                                                
  if (objc != 3)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "query argtypes");
    return TCL_ERROR;
  }

                                                                
                                  
                                                                
  if (Tcl_ListObjGetElements(interp, objv[2], &nargs, &argsObj) != TCL_OK)
  {
    return TCL_ERROR;
  }

                                                                
                                          
     
                                                                              
                                                                        
                            
                                                                
  plan_cxt = AllocSetContextCreate(TopMemoryContext, "PL/Tcl spi_prepare query", ALLOCSET_SMALL_SIZES);
  MemoryContextSwitchTo(plan_cxt);
  qdesc = (pltcl_query_desc *)palloc0(sizeof(pltcl_query_desc));
  snprintf(qdesc->qname, sizeof(qdesc->qname), "%p", qdesc);
  qdesc->nargs = nargs;
  qdesc->argtypes = (Oid *)palloc(nargs * sizeof(Oid));
  qdesc->arginfuncs = (FmgrInfo *)palloc(nargs * sizeof(FmgrInfo));
  qdesc->argtypioparams = (Oid *)palloc(nargs * sizeof(Oid));
  MemoryContextSwitchTo(oldcontext);

                                                                
                                                                       
                   
                                                                

  pltcl_subtrans_begin(oldcontext, oldowner);

  PG_TRY();
  {
                                                                  
                                                                
                                                                  
                             
                                                                  
    for (i = 0; i < nargs; i++)
    {
      Oid typId, typInput, typIOParam;
      int32 typmod;

      parseTypeString(Tcl_GetString(argsObj[i]), &typId, &typmod, false);

      getTypeInputInfo(typId, &typInput, &typIOParam);

      qdesc->argtypes[i] = typId;
      fmgr_info_cxt(typInput, &(qdesc->arginfuncs[i]), plan_cxt);
      qdesc->argtypioparams[i] = typIOParam;
    }

                                                                  
                                             
                                                                  
    UTF_BEGIN;
    qdesc->plan = SPI_prepare(UTF_U2E(Tcl_GetString(objv[1])), nargs, qdesc->argtypes);
    UTF_END;

    if (qdesc->plan == NULL)
    {
      elog(ERROR, "SPI_prepare() failed");
    }

                                                                  
                                                                  
                                                         
                                                                  
    if (SPI_keepplan(qdesc->plan))
    {
      elog(ERROR, "SPI_keepplan() failed");
    }

    pltcl_subtrans_commit(oldcontext, oldowner);
  }
  PG_CATCH();
  {
    pltcl_subtrans_abort(interp, oldcontext, oldowner);

    MemoryContextDelete(plan_cxt);

    return TCL_ERROR;
  }
  PG_END_TRY();

                                                                
                                                      
                           
                                                                
  query_hash = &pltcl_current_call_state->prodesc->interp_desc->query_hash;

  hashent = Tcl_CreateHashEntry(query_hash, qdesc->qname, &hashnew);
  Tcl_SetHashValue(hashent, (ClientData)qdesc);

                                                          
  Tcl_SetObjResult(interp, Tcl_NewStringObj(qdesc->qname, -1));
  return TCL_OK;
}

                                                                        
                                                       
                                                                        
static int
pltcl_SPI_execute_plan(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  int my_rc;
  int spi_rc;
  int i;
  int j;
  int optIndex;
  Tcl_HashEntry *hashent;
  pltcl_query_desc *qdesc;
  const char *nulls = NULL;
  const char *arrayname = NULL;
  Tcl_Obj *loop_body = NULL;
  int count = 0;
  int callObjc;
  Tcl_Obj **callObjv = NULL;
  Datum *argvalues;
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;
  Tcl_HashTable *query_hash;

  enum options
  {
    OPT_ARRAY,
    OPT_COUNT,
    OPT_NULLS
  };

  static const char *options[] = {"-array", "-count", "-nulls", (const char *)NULL};

                                                                
                                      
                                                                
  i = 1;
  while (i < objc)
  {
    if (Tcl_GetIndexFromObj(NULL, objv[i], options, NULL, TCL_EXACT, &optIndex) != TCL_OK)
    {
      break;
    }

    if (++i >= objc)
    {
      Tcl_SetObjResult(interp, Tcl_NewStringObj("missing argument to -array, -count or -nulls", -1));
      return TCL_ERROR;
    }

    switch ((enum options)optIndex)
    {
    case OPT_ARRAY:
      arrayname = Tcl_GetString(objv[i++]);
      break;

    case OPT_COUNT:
      if (Tcl_GetIntFromObj(interp, objv[i++], &count) != TCL_OK)
      {
        return TCL_ERROR;
      }
      break;

    case OPT_NULLS:
      nulls = Tcl_GetString(objv[i++]);
      break;
    }
  }

                                                                
                                                 
                                                                
  if (i >= objc)
  {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("missing argument to -count or -array", -1));
    return TCL_ERROR;
  }

  query_hash = &pltcl_current_call_state->prodesc->interp_desc->query_hash;

  hashent = Tcl_FindHashEntry(query_hash, Tcl_GetString(objv[i]));
  if (hashent == NULL)
  {
    Tcl_AppendResult(interp, "invalid queryid '", Tcl_GetString(objv[i]), "'", NULL);
    return TCL_ERROR;
  }
  qdesc = (pltcl_query_desc *)Tcl_GetHashValue(hashent);
  i++;

                                                                
                                                          
                                                                
  if (nulls != NULL)
  {
    if (strlen(nulls) != qdesc->nargs)
    {
      Tcl_SetObjResult(interp, Tcl_NewStringObj("length of nulls string doesn't match number of arguments", -1));
      return TCL_ERROR;
    }
  }

                                                                
                                                          
                                
                                                                
  if (qdesc->nargs > 0)
  {
    if (i >= objc)
    {
      Tcl_SetObjResult(interp, Tcl_NewStringObj("argument list length doesn't match number of arguments for query", -1));
      return TCL_ERROR;
    }

                                                                  
                                 
                                                                  
    if (Tcl_ListObjGetElements(interp, objv[i++], &callObjc, &callObjv) != TCL_OK)
    {
      return TCL_ERROR;
    }

                                                                  
                                                  
                                                                  
    if (callObjc != qdesc->nargs)
    {
      Tcl_SetObjResult(interp, Tcl_NewStringObj("argument list length doesn't match number of arguments for query", -1));
      return TCL_ERROR;
    }
  }
  else
  {
    callObjc = 0;
  }

                                                                
                              
                                                                
  if (i < objc)
  {
    loop_body = objv[i++];
  }

  if (i != objc)
  {
    Tcl_WrongNumArgs(interp, 1, objv,
        "?-count n? ?-array name? ?-nulls string? "
        "query ?args? ?loop body?");
    return TCL_ERROR;
  }

                                                                
                                                                    
                   
                                                                

  pltcl_subtrans_begin(oldcontext, oldowner);

  PG_TRY();
  {
                                                                  
                                                          
                                         
                                                                  
    argvalues = (Datum *)palloc(callObjc * sizeof(Datum));

    for (j = 0; j < callObjc; j++)
    {
      if (nulls && nulls[j] == 'n')
      {
        argvalues[j] = InputFunctionCall(&qdesc->arginfuncs[j], NULL, qdesc->argtypioparams[j], -1);
      }
      else
      {
        UTF_BEGIN;
        argvalues[j] = InputFunctionCall(&qdesc->arginfuncs[j], UTF_U2E(Tcl_GetString(callObjv[j])), qdesc->argtypioparams[j], -1);
        UTF_END;
      }
    }

                                                                  
                        
                                                                  
    spi_rc = SPI_execute_plan(qdesc->plan, argvalues, nulls, pltcl_current_call_state->prodesc->fn_readonly, count);

    my_rc = pltcl_process_SPI_result(interp, arrayname, loop_body, spi_rc, SPI_tuptable, SPI_processed);

    pltcl_subtrans_commit(oldcontext, oldowner);
  }
  PG_CATCH();
  {
    pltcl_subtrans_abort(interp, oldcontext, oldowner);
    return TCL_ERROR;
  }
  PG_END_TRY();

  return my_rc;
}

                                                                        
                                                                      
   
                                                                             
                                
                                                                        
static int
pltcl_subtransaction(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;
  int retcode;

  if (objc != 2)
  {
    Tcl_WrongNumArgs(interp, 1, objv, "command");
    return TCL_ERROR;
  }

     
                                                                         
                                                                           
                                                                    
     
  BeginInternalSubTransaction(NULL);
  MemoryContextSwitchTo(oldcontext);

  retcode = Tcl_EvalObjEx(interp, objv[1], 0);

  if (retcode == TCL_ERROR)
  {
                                     
    RollbackAndReleaseCurrentSubTransaction();
  }
  else
  {
                                   
    ReleaseCurrentSubTransaction();
  }

                                                                          
  MemoryContextSwitchTo(oldcontext);
  CurrentResourceOwner = oldowner;

  return retcode;
}

                                                                        
                  
   
                                               
                                                                        
static int
pltcl_commit(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  MemoryContext oldcontext = CurrentMemoryContext;

  PG_TRY();
  {
    SPI_commit();
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                    
    pltcl_construct_errorCode(interp, edata);
    UTF_BEGIN;
    Tcl_SetObjResult(interp, Tcl_NewStringObj(UTF_E2U(edata->message), -1));
    UTF_END;
    FreeErrorData(edata);

    return TCL_ERROR;
  }
  PG_END_TRY();

  return TCL_OK;
}

                                                                        
                    
   
                                              
                                                                        
static int
pltcl_rollback(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
  MemoryContext oldcontext = CurrentMemoryContext;

  PG_TRY();
  {
    SPI_rollback();
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                    
    pltcl_construct_errorCode(interp, edata);
    UTF_BEGIN;
    Tcl_SetObjResult(interp, Tcl_NewStringObj(UTF_E2U(edata->message), -1));
    UTF_END;
    FreeErrorData(edata);

    return TCL_ERROR;
  }
  PG_END_TRY();

  return TCL_OK;
}

                                                                        
                                                               
                         
   
                                                                    
                                                                        
static void
pltcl_set_tuple_values(Tcl_Interp *interp, const char *arrayname, uint64 tupno, HeapTuple tuple, TupleDesc tupdesc)
{
  int i;
  char *outputstr;
  Datum attr;
  bool isnull;
  const char *attname;
  Oid typoutput;
  bool typisvarlena;
  const char **arrptr;
  const char **nameptr;
  const char *nullname = NULL;

                                                                
                                              
                                                                
  if (arrayname == NULL)
  {
    arrptr = &attname;
    nameptr = &nullname;
  }
  else
  {
    arrptr = &arrayname;
    nameptr = &attname;

       
                                                                       
                                                                           
                                                      
       
    Tcl_SetVar2Ex(interp, arrayname, ".tupno", Tcl_NewWideIntObj(tupno), 0);
  }

  for (i = 0; i < tupdesc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

                                   
    if (att->attisdropped)
    {
      continue;
    }

                                                                  
                              
                                                                  
    UTF_BEGIN;
    attname = pstrdup(UTF_E2U(NameStr(att->attname)));
    UTF_END;

                                                                  
                                
                                                                  
    attr = heap_getattr(tuple, i + 1, tupdesc, &isnull);

                                                                  
                                             
                        
       
                                                      
                                                           
                        
                                                                  
    if (!isnull)
    {
      getTypeOutputInfo(att->atttypid, &typoutput, &typisvarlena);
      outputstr = OidOutputFunctionCall(typoutput, attr);
      UTF_BEGIN;
      Tcl_SetVar2Ex(interp, *arrptr, *nameptr, Tcl_NewStringObj(UTF_E2U(outputstr), -1), 0);
      UTF_END;
      pfree(outputstr);
    }
    else
    {
      Tcl_UnsetVar2(interp, *arrptr, *nameptr, 0);
    }

    pfree(unconstify(char *, attname));
  }
}

                                                                        
                                                                             
                                             
                                                                        
static Tcl_Obj *
pltcl_build_tuple_argument(HeapTuple tuple, TupleDesc tupdesc, bool include_generated)
{
  Tcl_Obj *retobj = Tcl_NewObj();
  int i;
  char *outputstr;
  Datum attr;
  bool isnull;
  char *attname;
  Oid typoutput;
  bool typisvarlena;

  for (i = 0; i < tupdesc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

                                   
    if (att->attisdropped)
    {
      continue;
    }

    if (att->attgenerated)
    {
                                          
      if (!include_generated)
      {
        continue;
      }
    }

                                                                  
                              
                                                                  
    attname = NameStr(att->attname);

                                                                  
                                
                                                                  
    attr = heap_getattr(tuple, i + 1, tupdesc, &isnull);

                                                                  
                                                              
                         
       
                                                      
                                                           
                        
                                                                  
    if (!isnull)
    {
      getTypeOutputInfo(att->atttypid, &typoutput, &typisvarlena);
      outputstr = OidOutputFunctionCall(typoutput, attr);
      UTF_BEGIN;
      Tcl_ListObjAppendElement(NULL, retobj, Tcl_NewStringObj(UTF_E2U(attname), -1));
      UTF_END;
      UTF_BEGIN;
      Tcl_ListObjAppendElement(NULL, retobj, Tcl_NewStringObj(UTF_E2U(outputstr), -1));
      UTF_END;
      pfree(outputstr);
    }
  }

  return retobj;
}

                                                                        
                                                                           
                                                   
   
                                                                           
   
                                                                          
                                                                         
                                                                         
                              
                                                                        
static HeapTuple
pltcl_build_tuple_result(Tcl_Interp *interp, Tcl_Obj **kvObjv, int kvObjc, pltcl_call_state *call_state)
{
  HeapTuple tuple;
  TupleDesc tupdesc;
  AttInMetadata *attinmeta;
  char **values;
  int i;

  if (call_state->ret_tupdesc)
  {
    tupdesc = call_state->ret_tupdesc;
    attinmeta = call_state->attinmeta;
  }
  else if (call_state->trigdata)
  {
    tupdesc = RelationGetDescr(call_state->trigdata->tg_relation);
    attinmeta = TupleDescGetAttInMetadata(tupdesc);
  }
  else
  {
    elog(ERROR, "PL/Tcl function does not return a tuple");
    tupdesc = NULL;                          
    attinmeta = NULL;
  }

  values = (char **)palloc0(tupdesc->natts * sizeof(char *));

  if (kvObjc % 2 != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("column name/value list must have even number of elements")));
  }

  for (i = 0; i < kvObjc; i += 2)
  {
    char *fieldName = utf_u2e(Tcl_GetString(kvObjv[i]));
    int attn = SPI_fnumber(tupdesc, fieldName);

       
                                                                          
                                                                          
                                 
       
    if (attn == SPI_ERROR_NOATTRIBUTE)
    {
      if (strcmp(fieldName, ".tupno") == 0)
      {
        continue;
      }
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column name/value list contains nonexistent column name \"%s\"", fieldName)));
    }

    if (attn <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot set system attribute \"%s\"", fieldName)));
    }

    if (TupleDescAttr(tupdesc, attn - 1)->attgenerated)
    {
      ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("cannot set generated column \"%s\"", fieldName)));
    }

    values[attn - 1] = utf_u2e(Tcl_GetString(kvObjv[i + 1]));
  }

  tuple = BuildTupleFromCStrings(attinmeta, values);

                                                                         
  if (call_state->prodesc->fn_retisdomain)
  {
    domain_check(HeapTupleGetDatum(tuple), false, call_state->prodesc->result_typid, &call_state->prodesc->domain_info, call_state->prodesc->fn_cxt);
  }

  return tuple;
}

                                                                        
                                                                         
                                                                        
static void
pltcl_init_tuple_store(pltcl_call_state *call_state)
{
  ReturnSetInfo *rsi = call_state->rsi;
  MemoryContext oldcxt;
  ResourceOwner oldowner;

                          
  Assert(rsi);
                                    
  Assert(!call_state->tuple_store);
  Assert(!call_state->attinmeta);

                                                                 
  Assert(rsi->expectedDesc);
  call_state->ret_tupdesc = rsi->expectedDesc;

     
                                                                           
                                                                          
                                                                             
                                                                          
                                      
     
  oldcxt = MemoryContextSwitchTo(call_state->tuple_store_cxt);
  oldowner = CurrentResourceOwner;
  CurrentResourceOwner = call_state->tuple_store_owner;

  call_state->tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

                                            
  call_state->attinmeta = TupleDescGetAttInMetadata(call_state->ret_tupdesc);

  CurrentResourceOwner = oldowner;
  MemoryContextSwitchTo(oldcxt);
}
