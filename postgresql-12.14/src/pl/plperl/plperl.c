                                                                        
                                                           
   
                            
   
                                                                        

#include "postgres.h"

                  
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

                      
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_language.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/event_trigger.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "storage/ipc.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/hsearch.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

                                             
#undef TEXTDOMAIN
#define TEXTDOMAIN PG_TEXTDOMAIN("plperl")

                
#include "plperl.h"
#include "plperl_helpers.h"

                                                        
#include "perlchunks.h"
                               
#include "plperl_opmask.h"

EXTERN_C void
boot_DynaLoader(pTHX_ CV *cv);
EXTERN_C void
boot_PostgreSQL__InServer__Util(pTHX_ CV *cv);
EXTERN_C void
boot_PostgreSQL__InServer__SPI(pTHX_ CV *cv);

PG_MODULE_MAGIC;

                                                                        
                                                                            
                                                                             
                                                                             
                                                                              
                                                                    
   
                                                                            
                                                                        
                                                                     
   
                                                                      
                                                                      
                                                                    
                                                                       
                                                                            
                                                                          
                                                                 
   
                                                                           
                                                                              
                                                                              
                                                                             
                                       
                                                                        
typedef struct plperl_interp_desc
{
  Oid user_id;                                            
  PerlInterpreter *interp;                      
  HTAB *query_hash;                                        
} plperl_interp_desc;

                                                                        
                                                    
   
                                                                           
                                                                              
                                                                             
                                                                         
                                                                    
                                                                        
typedef struct plperl_proc_desc
{
  char *proname;                                         
  MemoryContext fn_cxt;                                             
  unsigned long fn_refcount;                                  
  TransactionId fn_xmin;                                                
  ItemPointerData fn_tid;
  SV *reference;                                               
  plperl_interp_desc *interp;                                  
  bool fn_readonly;                                                     
  Oid lang_oid;
  List *trftypes;
  bool lanpltrusted;                                          
  bool fn_retistuple;                                      
  bool fn_retisset;                                      
  bool fn_retisarray;                                     
                                                   
  Oid result_oid;                                  
  FmgrInfo result_in_func;                                           
  Oid result_typioparam;
                                                        
  int nargs;
  FmgrInfo *arg_out_func;                               
  bool *arg_is_rowtype;                               
  Oid *arg_arraytype;                                     
} plperl_proc_desc;

#define increment_prodesc_refcount(prodesc) ((prodesc)->fn_refcount++)
#define decrement_prodesc_refcount(prodesc)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    Assert((prodesc)->fn_refcount > 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    if (--((prodesc)->fn_refcount) == 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      free_plperl_function(prodesc);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

                                                                        
                                                            
                                                                        
                                                                          
                                                                             
   
                                                                              
                                                                               
                                                                             
                                                                            
                                                                 
                                                                    
                                                                        
typedef struct plperl_proc_key
{
  Oid proc_id;                   

     
                                                                           
                         
     
  Oid is_trigger;                                
  Oid user_id;                                         
} plperl_proc_key;

typedef struct plperl_proc_ptr
{
  plperl_proc_key proc_key;                                
  plperl_proc_desc *proc_ptr;
} plperl_proc_ptr;

   
                                                                   
             
   
typedef struct plperl_call_data
{
  plperl_proc_desc *prodesc;
  FunctionCallInfo fcinfo;
                                                                   
  Tuplestorestate *tuple_store;
  TupleDesc ret_tdesc;
  Oid cdomain_oid;                                               
  void *cdomain_info;
  MemoryContext tmp_cxt;
} plperl_call_data;

                                                                        
                                                           
                                                                        
typedef struct plperl_query_desc
{
  char qname[24];
  MemoryContext plan_cxt;                                  
  SPIPlanPtr plan;
  int nargs;
  Oid *argtypes;
  FmgrInfo *arginfuncs;
  Oid *argtypioparams;
} plperl_query_desc;

                                     

typedef struct plperl_query_entry
{
  char query_name[NAMEDATALEN];
  plperl_query_desc *query_data;
} plperl_query_entry;

                                                                        
                                                       
                                                                        
typedef struct plperl_array_info
{
  int ndims;
  bool elem_is_rowtype;                                       
  Datum *elements;
  bool *nulls;
  int *nelems;
  FmgrInfo proc;
  FmgrInfo transform_proc;
} plperl_array_info;

                                                                        
               
                                                                        

static HTAB *plperl_interp_hash = NULL;
static HTAB *plperl_proc_hash = NULL;
static plperl_interp_desc *plperl_active_interp = NULL;

                                                                   
static PerlInterpreter *plperl_held_interp = NULL;

                   
static bool plperl_use_strict = false;
static char *plperl_on_init = NULL;
static char *plperl_on_plperl_init = NULL;
static char *plperl_on_plperlu_init = NULL;

static bool plperl_ending = false;
static OP *(*pp_require_orig)(pTHX) = NULL;
static char plperl_opmask[MAXO];

                                                       
static plperl_call_data *current_call_data = NULL;

                                                                        
                        
                                                                        
void
_PG_init(void);

static PerlInterpreter *
plperl_init_interp(void);
static void
plperl_destroy_interp(PerlInterpreter **);
static void
plperl_fini(int code, Datum arg);
static void
set_interp_require(bool trusted);

static Datum plperl_func_handler(PG_FUNCTION_ARGS);
static Datum plperl_trigger_handler(PG_FUNCTION_ARGS);
static void plperl_event_trigger_handler(PG_FUNCTION_ARGS);

static void
free_plperl_function(plperl_proc_desc *prodesc);

static plperl_proc_desc *
compile_plperl_function(Oid fn_oid, bool is_trigger, bool is_event_trigger);

static SV *
plperl_hash_from_tuple(HeapTuple tuple, TupleDesc tupdesc, bool include_generated);
static SV *
plperl_hash_from_datum(Datum attr);
static void
check_spi_usage_allowed(void);
static SV *
plperl_ref_from_pg_array(Datum arg, Oid typid);
static SV *
split_array(plperl_array_info *info, int first, int last, int nest);
static SV *
make_array_ref(plperl_array_info *info, int first, int last);
static SV *
get_perl_array_ref(SV *sv);
static Datum
plperl_sv_to_datum(SV *sv, Oid typid, int32 typmod, FunctionCallInfo fcinfo, FmgrInfo *finfo, Oid typioparam, bool *isnull);
static void
_sv_to_datum_finfo(Oid typid, FmgrInfo *finfo, Oid *typioparam);
static Datum
plperl_array_to_datum(SV *src, Oid typid, int32 typmod);
static void
array_to_datum_internal(AV *av, ArrayBuildState *astate, int *ndims, int *dims, int cur_depth, Oid arraytypid, Oid elemtypid, int32 typmod, FmgrInfo *finfo, Oid typioparam);
static Datum
plperl_hash_to_datum(SV *src, TupleDesc td);

static void plperl_init_shared_libs(pTHX);
static void
plperl_trusted_init(void);
static void
plperl_untrusted_init(void);
static HV *
plperl_spi_execute_fetch_result(SPITupleTable *, uint64, int);
static void
plperl_return_next_internal(SV *sv);
static char *
hek2cstr(HE *he);
static SV **
hv_store_string(HV *hv, const char *key, SV *val);
static SV **
hv_fetch_string(HV *hv, const char *key);
static void
plperl_create_sub(plperl_proc_desc *desc, const char *s, Oid fn_oid);
static SV *
plperl_call_perl_func(plperl_proc_desc *desc, FunctionCallInfo fcinfo);
static void
plperl_compile_callback(void *arg);
static void
plperl_exec_callback(void *arg);
static void
plperl_inline_callback(void *arg);
static char *
strip_trailing_ws(const char *msg);
static OP *pp_require_safe(pTHX);
static void
activate_interpreter(plperl_interp_desc *interp_desc);

#if defined(WIN32) && PERL_VERSION_LT(5, 28, 0)
static char *
setlocale_perl(int category, char *locale);
#else
#define setlocale_perl(a, b) Perl_setlocale(a, b)
#endif                                                  

   
                                                                             
   
                                                                           
                                                                
   
static inline void
SvREFCNT_dec_current(SV *sv)
{
  dTHX;

  SvREFCNT_dec(sv);
}

   
                                                                            
   
static char *
hek2cstr(HE *he)
{
  dTHX;
  char *ret;
  SV *sv;

     
                                                                          
                                                
     
  ENTER;
  SAVETMPS;

                              
                                                                            
                                                                           
                                                                             
               
     
                                
                       
                         
                             
                         
                                                          
     
                                                                             
                                                                         
                                                                            
                                                                          
               
     
                                                                             
                                                                            
                 
                              
     

  sv = HeSVKEY_force(he);
  if (HeUTF8(he))
  {
    SvUTF8_on(sv);
  }
  ret = sv2cstr(sv);

               
  FREETMPS;
  LEAVE;

  return ret;
}

   
                                                   
   
                                                
   
void
_PG_init(void)
{
     
                                             
     
                                                                            
                                                                           
                                                                 
                         
     
  static bool inited = false;
  HASHCTL hash_ctl;

  if (inited)
  {
    return;
  }

     
                                 
     
  pg_bindtextdomain(TEXTDOMAIN);

     
                               
     
  DefineCustomBoolVariable("plperl.use_strict", gettext_noop("If true, trusted and untrusted Perl code will be compiled in strict mode."), NULL, &plperl_use_strict, false, PGC_USERSET, 0, NULL, NULL, NULL);

     
                                                                           
                                                                            
                                                                         
             
     
  DefineCustomStringVariable("plperl.on_init", gettext_noop("Perl initialization code to execute when a Perl interpreter is initialized."), NULL, &plperl_on_init, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

     
                                                                         
                                                                         
                                                                            
                                                                            
                                                                         
                                                                           
                         
     
                                                                            
                                                                            
                                                                         
                           
     
  DefineCustomStringVariable("plperl.on_plperl_init", gettext_noop("Perl initialization code to execute once when plperl is first used."), NULL, &plperl_on_plperl_init, NULL, PGC_SUSET, 0, NULL, NULL, NULL);

  DefineCustomStringVariable("plperl.on_plperlu_init", gettext_noop("Perl initialization code to execute once when plperlu is first used."), NULL, &plperl_on_plperlu_init, NULL, PGC_SUSET, 0, NULL, NULL, NULL);

  EmitWarningsOnPlaceholders("plperl");

     
                         
     
  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(Oid);
  hash_ctl.entrysize = sizeof(plperl_interp_desc);
  plperl_interp_hash = hash_create("PL/Perl interpreters", 8, &hash_ctl, HASH_ELEM | HASH_BLOBS);

  memset(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(plperl_proc_key);
  hash_ctl.entrysize = sizeof(plperl_proc_ptr);
  plperl_proc_hash = hash_create("PL/Perl procedures", 32, &hash_ctl, HASH_ELEM | HASH_BLOBS);

     
                              
     
  PLPERL_SET_OPMASK(plperl_opmask);

     
                                                                          
     
  plperl_held_interp = plperl_init_interp();

  inited = true;
}

static void
set_interp_require(bool trusted)
{
  if (trusted)
  {
    PL_ppaddr[OP_REQUIRE] = pp_require_safe;
    PL_ppaddr[OP_DOFILE] = pp_require_safe;
  }
  else
  {
    PL_ppaddr[OP_REQUIRE] = pp_require_orig;
    PL_ppaddr[OP_DOFILE] = pp_require_orig;
  }
}

   
                                                            
                                                                             
   
static void
plperl_fini(int code, Datum arg)
{
  HASH_SEQ_STATUS hash_seq;
  plperl_interp_desc *interp_desc;

  elog(DEBUG3, "plperl_fini");

     
                                                                             
                                                                       
                                                       
                                                                       
     
  plperl_ending = true;

                                                          
  if (code)
  {
    elog(DEBUG3, "plperl_fini: skipped");
    return;
  }

                                                       
  plperl_destroy_interp(&plperl_held_interp);

                                              
  hash_seq_init(&hash_seq, plperl_interp_hash);
  while ((interp_desc = hash_seq_search(&hash_seq)) != NULL)
  {
    if (interp_desc->interp)
    {
      activate_interpreter(interp_desc);
      plperl_destroy_interp(&interp_desc->interp);
    }
  }

  elog(DEBUG3, "plperl_fini: done");
}

   
                                                        
   
static void
select_perl_context(bool trusted)
{
  Oid user_id;
  plperl_interp_desc *interp_desc;
  bool found;
  PerlInterpreter *interp = NULL;

                                                                      
  if (trusted)
  {
    user_id = GetUserId();
  }
  else
  {
    user_id = InvalidOid;
  }

  interp_desc = hash_search(plperl_interp_hash, &user_id, HASH_ENTER, &found);
  if (!found)
  {
                                                  
    interp_desc->interp = NULL;
    interp_desc->query_hash = NULL;
  }

                                                           
  if (interp_desc->query_hash == NULL)
  {
    HASHCTL hash_ctl;

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = NAMEDATALEN;
    hash_ctl.entrysize = sizeof(plperl_query_entry);
    interp_desc->query_hash = hash_create("PL/Perl queries", 32, &hash_ctl, HASH_ELEM);
  }

     
                                               
     
  if (interp_desc->interp)
  {
    activate_interpreter(interp_desc);
    return;
  }

     
                                                                
     
  if (plperl_held_interp != NULL)
  {
                                                
    interp = plperl_held_interp;

       
                                                                          
                                                                         
       
    plperl_held_interp = NULL;

    if (trusted)
    {
      plperl_trusted_init();
    }
    else
    {
      plperl_untrusted_init();
    }

                                                          
    on_proc_exit(plperl_fini, 0);
  }
  else
  {
#ifdef MULTIPLICITY

       
                                                                
                                                                           
                                                                         
                                                                 
                  
       
    plperl_active_interp = NULL;

                                       
    interp = plperl_init_interp();

    if (trusted)
    {
      plperl_trusted_init();
    }
    else
    {
      plperl_untrusted_init();
    }
#else
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot allocate multiple Perl interpreters on this platform")));
#endif
  }

  set_interp_require(trusted);

     
                                                                      
                                                                           
                                                                           
                                                       
                                                                       
     
  {
    dTHX;

    newXS("PostgreSQL::InServer::SPI::bootstrap", boot_PostgreSQL__InServer__SPI, __FILE__);

    eval_pv("PostgreSQL::InServer::SPI::bootstrap()", FALSE);
    if (SvTRUE(ERRSV))
    {
      ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while executing PostgreSQL::InServer::SPI::bootstrap")));
    }
  }

                                                            
  interp_desc->interp = interp;

                                               
  plperl_active_interp = interp_desc;
}

   
                                                 
   
                                                                               
                                                                           
   
static void
activate_interpreter(plperl_interp_desc *interp_desc)
{
  if (interp_desc && plperl_active_interp != interp_desc)
  {
    Assert(interp_desc->interp);
    PERL_SET_CONTEXT(interp_desc->interp);
                                              
    set_interp_require(OidIsValid(interp_desc->user_id));
    plperl_active_interp = interp_desc;
  }
}

   
                                  
   
                                                                          
                                                                         
                                                                             
                                                                           
   
static PerlInterpreter *
plperl_init_interp(void)
{
  PerlInterpreter *plperl;

  static char *embedding[3 + 2] = {"", "-e", PLC_PERLBOOT};
  int nargs = 3;

#ifdef WIN32

     
                                                                
                                                                            
                                                                    
                                                                          
                                                                          
                                                  
     
               
                                                                        
     
                                                                         
                                                                    
               
     
                                                                         
                                                                
     
     

  char *loc;
  char *save_collate, *save_ctype, *save_monetary, *save_numeric, *save_time;

  loc = setlocale(LC_COLLATE, NULL);
  save_collate = loc ? pstrdup(loc) : NULL;
  loc = setlocale(LC_CTYPE, NULL);
  save_ctype = loc ? pstrdup(loc) : NULL;
  loc = setlocale(LC_MONETARY, NULL);
  save_monetary = loc ? pstrdup(loc) : NULL;
  loc = setlocale(LC_NUMERIC, NULL);
  save_numeric = loc ? pstrdup(loc) : NULL;
  loc = setlocale(LC_TIME, NULL);
  save_time = loc ? pstrdup(loc) : NULL;

#define PLPERL_RESTORE_LOCALE(name, saved)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  STMT_START                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (saved != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      setlocale_perl(name, saved);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
      pfree(saved);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  STMT_END
#endif            

  if (plperl_on_init && *plperl_on_init)
  {
    embedding[nargs++] = "-e";
    embedding[nargs++] = plperl_on_init;
  }

     
                                                                         
                                                                             
                                                                             
                                                                           
                                                                          
                                                                             
                      
     
#if defined(PERL_SYS_INIT3) && !defined(MYMALLOC)
  {
    static int perl_sys_init_done;

                                                                          
    if (!perl_sys_init_done)
    {
      char *dummy_env[1] = {NULL};

      PERL_SYS_INIT3(&nargs, (char ***)&embedding, (char ***)&dummy_env);

         
                                                                        
                                                                        
                                                                    
                                                                       
                                                                       
                                                                       
                                                     
         
      pqsignal(SIGFPE, FloatExceptionHandler);

      perl_sys_init_done = 1;
                                                                          
      dummy_env[0] = NULL;
    }
  }
#endif

  plperl = perl_alloc();
  if (!plperl)
  {
    elog(ERROR, "could not allocate Perl interpreter");
  }

  PERL_SET_CONTEXT(plperl);
  perl_construct(plperl);

     
                                                                          
                                                                           
                                                         
     
  {
    dTHX;

    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

       
                                                                   
                                                                         
                             
       
    if (!pp_require_orig)
    {
      pp_require_orig = PL_ppaddr[OP_REQUIRE];
    }
    else
    {
      PL_ppaddr[OP_REQUIRE] = pp_require_orig;
      PL_ppaddr[OP_DOFILE] = pp_require_orig;
    }

#ifdef PLPERL_ENABLE_OPMASK_EARLY

       
                                                                 
                                                                        
                                                                       
                                                                     
                
       
    PL_op_mask = plperl_opmask;
#endif

    if (perl_parse(plperl, plperl_init_shared_libs, nargs, embedding, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while parsing Perl initialization")));
    }

    if (perl_run(plperl) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while running Perl initialization")));
    }

#ifdef PLPERL_RESTORE_LOCALE
    PLPERL_RESTORE_LOCALE(LC_COLLATE, save_collate);
    PLPERL_RESTORE_LOCALE(LC_CTYPE, save_ctype);
    PLPERL_RESTORE_LOCALE(LC_MONETARY, save_monetary);
    PLPERL_RESTORE_LOCALE(LC_NUMERIC, save_numeric);
    PLPERL_RESTORE_LOCALE(LC_TIME, save_time);
#endif
  }

  return plperl;
}

   
                                                  
                                                                 
                                                                           
                      
                                                                
   
static OP *
pp_require_safe(pTHX)
{
  dVAR;
  dSP;
  SV *sv, **svp;
  char *name;
  STRLEN len;

  sv = POPs;
  name = SvPV(sv, len);
  if (!(name && len > 0 && *name))
  {
    RETPUSHNO;
  }

  svp = hv_fetch(GvHVn(PL_incgv), name, len, 0);
  if (svp && *svp != &PL_sv_undef)
  {
    RETPUSHYES;
  }

  DIE(aTHX_ "Unable to load %s into plperl", name);

     
                                                                             
                                                                       
                                                                             
                                                                         
                                                                            
                               
     
  return NULL;
}

   
                                                                     
   
                                                                
   
static void
plperl_destroy_interp(PerlInterpreter **interp)
{
  if (interp && *interp)
  {
       
                                                                     
               
       
                                                                        
                                                                   
                                                                       
                                                                         
                                          
       
    dTHX;

                                                          
    if (PL_exit_flags & PERL_EXIT_DESTRUCT_END)
    {
      dJMPENV;
      int x = 0;

      JMPENV_PUSH(x);
      PERL_UNUSED_VAR(x);
      if (PL_endav && !PL_minus_c)
      {
        call_list(PL_scopestack_ix, PL_endav);
      }
      JMPENV_POP;
    }
    LEAVE;
    FREETMPS;

    *interp = NULL;
  }
}

   
                                                               
   
static void
plperl_trusted_init(void)
{
  dTHX;
  HV *stash;
  SV *sv;
  char *key;
  I32 klen;

                                            
  PL_ppaddr[OP_REQUIRE] = pp_require_orig;
  PL_ppaddr[OP_DOFILE] = pp_require_orig;

  eval_pv(PLC_TRUSTED, FALSE);
  if (SvTRUE(ERRSV))
  {
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while executing PLC_TRUSTED")));
  }

     
                                                                            
                                                           
                                                         
     
  eval_pv("my $a=chr(0x100); return $a =~ /\\xa9/i", FALSE);
  if (SvTRUE(ERRSV))
  {
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while executing utf8fix")));
  }

     
                               
     

                                                                
  PL_ppaddr[OP_REQUIRE] = pp_require_safe;
  PL_ppaddr[OP_DOFILE] = pp_require_safe;

     
                                                                        
                                                    
     
  PL_op_mask = plperl_opmask;

                                                                       
  stash = gv_stashpv("DynaLoader", GV_ADDWARN);
  hv_iterinit(stash);
  while ((sv = hv_iternextsv(stash, &key, &klen)))
  {
    if (!isGV_with_GP(sv) || !GvCV(sv))
    {
      continue;
    }
    SvREFCNT_dec(GvCV(sv));                  
    GvCV_set(sv, NULL);                              
  }
  hv_clear(stash);

                                  
  ++PL_sub_generation;
  hv_clear(PL_stashcache);

     
                                                                  
     
  if (plperl_on_plperl_init && *plperl_on_plperl_init)
  {
    eval_pv(plperl_on_plperl_init, FALSE);
                                                                   
    if (SvTRUE(ERRSV))
    {
      ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while executing plperl.on_plperl_init")));
    }
  }
}

   
                                                                  
   
static void
plperl_untrusted_init(void)
{
  dTHX;

     
                                                         
     
  if (plperl_on_plperlu_init && *plperl_on_plperlu_init)
  {
    eval_pv(plperl_on_plperlu_init, FALSE);
    if (SvTRUE(ERRSV))
    {
      ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV))), errcontext("while executing plperl.on_plperlu_init")));
    }
  }
}

   
                                                                       
   
static char *
strip_trailing_ws(const char *msg)
{
  char *res = pstrdup(msg);
  int len = strlen(res);

  while (len > 0 && isspace((unsigned char)res[len - 1]))
  {
    res[--len] = '\0';
  }
  return res;
}

                                

static HeapTuple
plperl_build_tuple_result(HV *perlhash, TupleDesc td)
{
  dTHX;
  Datum *values;
  bool *nulls;
  HE *he;
  HeapTuple tup;

  values = palloc0(sizeof(Datum) * td->natts);
  nulls = palloc(sizeof(bool) * td->natts);
  memset(nulls, true, sizeof(bool) * td->natts);

  hv_iterinit(perlhash);
  while ((he = hv_iternext(perlhash)))
  {
    SV *val = HeVAL(he);
    char *key = hek2cstr(he);
    int attn = SPI_fnumber(td, key);
    Form_pg_attribute attr = TupleDescAttr(td, attn - 1);

    if (attn == SPI_ERROR_NOATTRIBUTE)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("Perl hash contains nonexistent column \"%s\"", key)));
    }
    if (attn <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot set system attribute \"%s\"", key)));
    }

    values[attn - 1] = plperl_sv_to_datum(val, attr->atttypid, attr->atttypmod, NULL, NULL, InvalidOid, &nulls[attn - 1]);

    pfree(key);
  }
  hv_iterinit(perlhash);

  tup = heap_form_tuple(td, values, nulls);
  pfree(values);
  pfree(nulls);
  return tup;
}

                                         
static Datum
plperl_hash_to_datum(SV *src, TupleDesc td)
{
  HeapTuple tup = plperl_build_tuple_result((HV *)SvRV(src), td);

  return HeapTupleGetDatum(tup);
}

   
                                                                              
                                                                              
   
static SV *
get_perl_array_ref(SV *sv)
{
  dTHX;

  if (SvOK(sv) && SvROK(sv))
  {
    if (SvTYPE(SvRV(sv)) == SVt_PVAV)
    {
      return sv;
    }
    else if (sv_isa(sv, "PostgreSQL::InServer::ARRAY"))
    {
      HV *hv = (HV *)SvRV(sv);
      SV **sav = hv_fetch_string(hv, "array");

      if (*sav && SvOK(*sav) && SvROK(*sav) && SvTYPE(SvRV(*sav)) == SVt_PVAV)
      {
        return *sav;
      }

      elog(ERROR, "could not get array reference from PostgreSQL::InServer::ARRAY object");
    }
  }
  return NULL;
}

   
                                                                          
   
static void
array_to_datum_internal(AV *av, ArrayBuildState *astate, int *ndims, int *dims, int cur_depth, Oid arraytypid, Oid elemtypid, int32 typmod, FmgrInfo *finfo, Oid typioparam)
{
  dTHX;
  int i;
  int len = av_len(av) + 1;

  for (i = 0; i < len; i++)
  {
                                 
    SV **svp = av_fetch(av, i, FALSE);

                                                         
    SV *sav = svp ? get_perl_array_ref(*svp) : NULL;

                                  
    if (sav)
    {
      AV *nav = (AV *)SvRV(sav);

                                 
      if (cur_depth + 1 > MAXDIM)
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("number of array dimensions (%d) exceeds the maximum allowed (%d)", cur_depth + 1, MAXDIM)));
      }

                                                                      
      if (i == 0 && *ndims == cur_depth)
      {
        dims[*ndims] = av_len(nav) + 1;
        (*ndims)++;
      }
      else if (av_len(nav) + 1 != dims[cur_depth])
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("multidimensional arrays must have array expressions with matching dimensions")));
      }

                                                       
      array_to_datum_internal(nav, astate, ndims, dims, cur_depth + 1, arraytypid, elemtypid, typmod, finfo, typioparam);
    }
    else
    {
      Datum dat;
      bool isnull;

                                                       
      if (*ndims != cur_depth)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("multidimensional arrays must have array expressions with matching dimensions")));
      }

      dat = plperl_sv_to_datum(svp ? *svp : NULL, elemtypid, typmod, NULL, finfo, typioparam, &isnull);

      (void)accumArrayResult(astate, dat, isnull, elemtypid, CurrentMemoryContext);
    }
  }
}

   
                                     
   
static Datum
plperl_array_to_datum(SV *src, Oid typid, int32 typmod)
{
  dTHX;
  ArrayBuildState *astate;
  Oid elemtypid;
  FmgrInfo finfo;
  Oid typioparam;
  int dims[MAXDIM];
  int lbs[MAXDIM];
  int ndims = 1;
  int i;

  elemtypid = get_element_type(typid);
  if (!elemtypid)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot convert Perl array to non-array type %s", format_type_be(typid))));
  }

  astate = initArrayResult(elemtypid, CurrentMemoryContext, true);

  _sv_to_datum_finfo(elemtypid, &finfo, &typioparam);

  memset(dims, 0, sizeof(dims));
  dims[0] = av_len((AV *)SvRV(src)) + 1;

  array_to_datum_internal((AV *)SvRV(src), astate, &ndims, dims, 1, typid, elemtypid, typmod, &finfo, typioparam);

                                                                      
  if (dims[0] <= 0)
  {
    ndims = 0;
  }

  for (i = 0; i < ndims; i++)
  {
    lbs[i] = 1;
  }

  return makeMdArrayResult(astate, ndims, dims, lbs, CurrentMemoryContext, true);
}

                                                                         
static void
_sv_to_datum_finfo(Oid typid, FmgrInfo *finfo, Oid *typioparam)
{
  Oid typinput;

                                                  
  getTypeInputInfo(typid, &typinput, typioparam);
  fmgr_info(typinput, finfo);
}

   
                                                            
   
                                                                        
                                                                              
                                                                  
   
                                                                            
                                                                        
   
                                   
   
static Datum
plperl_sv_to_datum(SV *sv, Oid typid, int32 typmod, FunctionCallInfo fcinfo, FmgrInfo *finfo, Oid typioparam, bool *isnull)
{
  FmgrInfo tmp;
  Oid funcid;

                        
  check_stack_depth();

  *isnull = false;

     
                                                                         
                                                                            
                                                                        
     
  if (!sv || !SvOK(sv) || typid == VOIDOID)
  {
                                                   
    if (!finfo)
    {
      _sv_to_datum_finfo(typid, &tmp, &typioparam);
      finfo = &tmp;
    }
    *isnull = true;
                                                            
    return InputFunctionCall(finfo, NULL, typioparam, typmod);
  }
  else if ((funcid = get_transform_tosql(typid, current_call_data->prodesc->lang_oid, current_call_data->prodesc->trftypes)))
  {
    return OidFunctionCall1(funcid, PointerGetDatum(sv));
  }
  else if (SvROK(sv))
  {
                           
    SV *sav = get_perl_array_ref(sv);

    if (sav)
    {
                              
      return plperl_array_to_datum(sav, typid, typmod);
    }
    else if (SvTYPE(SvRV(sv)) == SVt_PVHV)
    {
                            
      Datum ret;
      TupleDesc td;
      bool isdomain;

      if (!type_is_rowtype(typid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot convert Perl hash to non-composite type %s", format_type_be(typid))));
      }

      td = lookup_rowtype_tupdesc_domain(typid, typmod, true);
      if (td != NULL)
      {
                                           
        isdomain = (typid != td->tdtypeid);
      }
      else
      {
                                                               
        TypeFuncClass funcclass;

        if (fcinfo)
        {
          funcclass = get_call_result_type(fcinfo, &typid, &td);
        }
        else
        {
          funcclass = TYPEFUNC_OTHER;
        }
        if (funcclass != TYPEFUNC_COMPOSITE && funcclass != TYPEFUNC_COMPOSITE_DOMAIN)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function returning record called in context "
                                                                         "that cannot accept type record")));
        }
        Assert(td);
        isdomain = (funcclass == TYPEFUNC_COMPOSITE_DOMAIN);
      }

      ret = plperl_hash_to_datum(sv, td);

      if (isdomain)
      {
        domain_check(ret, false, typid, NULL, NULL);
      }

                                                                     
      ReleaseTupleDesc(td);

      return ret;
    }

       
                                                                     
                                               
       
    return plperl_sv_to_datum(SvRV(sv), typid, typmod, fcinfo, finfo, typioparam, isnull);
  }
  else
  {
                                
    Datum ret;
    char *str = sv2cstr(sv);

                                                  
    if (!finfo)
    {
      _sv_to_datum_finfo(typid, &tmp, &typioparam);
      finfo = &tmp;
    }

    ret = InputFunctionCall(finfo, str, typioparam, typmod);
    pfree(str);

    return ret;
  }
}

                                                                          
char *
plperl_sv_to_literal(SV *sv, char *fqtypename)
{
  Oid typid;
  Oid typoutput;
  Datum datum;
  bool typisvarlena, isnull;

  check_spi_usage_allowed();

  typid = DirectFunctionCall1(regtypein, CStringGetDatum(fqtypename));
  if (!OidIsValid(typid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("lookup failed for type %s", fqtypename)));
  }

  datum = plperl_sv_to_datum(sv, typid, -1, NULL, NULL, InvalidOid, &isnull);

  if (isnull)
  {
    return NULL;
  }

  getTypeOutputInfo(typid, &typoutput, &typisvarlena);

  return OidOutputFunctionCall(typoutput, datum);
}

   
                                                             
   
                                                    
   
static SV *
plperl_ref_from_pg_array(Datum arg, Oid typid)
{
  dTHX;
  ArrayType *ar = DatumGetArrayTypeP(arg);
  Oid elementtype = ARR_ELEMTYPE(ar);
  int16 typlen;
  bool typbyval;
  char typalign, typdelim;
  Oid typioparam;
  Oid typoutputfunc;
  Oid transform_funcid;
  int i, nitems, *dims;
  plperl_array_info *info;
  SV *av;
  HV *hv;

     
                                                                            
                   
     
  info = palloc0(sizeof(plperl_array_info));

                                                                          
  get_type_io_data(elementtype, IOFunc_output, &typlen, &typbyval, &typalign, &typdelim, &typioparam, &typoutputfunc);

                                      
  transform_funcid = get_transform_fromsql(elementtype, current_call_data->prodesc->lang_oid, current_call_data->prodesc->trftypes);

                                                           
  if (OidIsValid(transform_funcid))
  {
    fmgr_info(transform_funcid, &info->transform_proc);
  }
  else
  {
    fmgr_info(typoutputfunc, &info->proc);
  }

  info->elem_is_rowtype = type_is_rowtype(elementtype);

                                                     
  info->ndims = ARR_NDIM(ar);
  dims = ARR_DIMS(ar);

                                            
  if (info->ndims == 0)
  {
    av = newRV_noinc((SV *)newAV());
  }
  else
  {
    deconstruct_array(ar, elementtype, typlen, typbyval, typalign, &info->elements, &info->nulls, &nitems);

                                                        
    info->nelems = palloc(sizeof(int) * info->ndims);
    info->nelems[0] = nitems;
    for (i = 1; i < info->ndims; i++)
    {
      info->nelems[i] = info->nelems[i - 1] / dims[i - 1];
    }

    av = split_array(info, 0, nitems, 0);
  }

  hv = newHV();
  (void)hv_store(hv, "array", 5, av, 0);
  (void)hv_store(hv, "typeoid", 7, newSVuv(typid), 0);

  return sv_bless(newRV_noinc((SV *)hv), gv_stashpv("PostgreSQL::InServer::ARRAY", 0));
}

   
                                                                       
   
static SV *
split_array(plperl_array_info *info, int first, int last, int nest)
{
  dTHX;
  int i;
  AV *result;

                                                                
  Assert(info->ndims > 0);

                                                                          
  check_stack_depth();

     
                                                                 
     
  if (nest >= info->ndims - 1)
  {
    return make_array_ref(info, first, last);
  }

  result = newAV();
  for (i = first; i < last; i += info->nelems[nest + 1])
  {
                                                                   
    SV *ref = split_array(info, i, i + info->nelems[nest + 1], nest + 1);

    av_push(result, ref);
  }
  return newRV_noinc((SV *)result);
}

   
                                                                      
                                               
   
static SV *
make_array_ref(plperl_array_info *info, int first, int last)
{
  dTHX;
  int i;
  AV *result = newAV();

  for (i = first; i < last; i++)
  {
    if (info->nulls[i])
    {
         
                                                                      
                              
         
      av_push(result, newSV(0));
    }
    else
    {
      Datum itemvalue = info->elements[i];

      if (info->transform_proc.fn_oid)
      {
        av_push(result, (SV *)DatumGetPointer(FunctionCall1(&info->transform_proc, itemvalue)));
      }
      else if (info->elem_is_rowtype)
      {
                                            
        av_push(result, plperl_hash_from_datum(itemvalue));
      }
      else
      {
        char *val = OutputFunctionCall(&info->proc, itemvalue);

        av_push(result, cstr2sv(val));
      }
    }
  }
  return newRV_noinc((SV *)result);
}

                                              
static SV *
plperl_trigger_build_args(FunctionCallInfo fcinfo)
{
  dTHX;
  TriggerData *tdata;
  TupleDesc tupdesc;
  int i;
  char *level;
  char *event;
  char *relid;
  char *when;
  HV *hv;

  hv = newHV();
  hv_ksplit(hv, 12);                        

  tdata = (TriggerData *)fcinfo->context;
  tupdesc = tdata->tg_relation->rd_att;

  relid = DatumGetCString(DirectFunctionCall1(oidout, ObjectIdGetDatum(tdata->tg_relation->rd_id)));

  hv_store_string(hv, "name", cstr2sv(tdata->tg_trigger->tgname));
  hv_store_string(hv, "relid", cstr2sv(relid));

     
                                                                             
                                               
     

  if (TRIGGER_FIRED_BY_INSERT(tdata->tg_event))
  {
    event = "INSERT";
    if (TRIGGER_FIRED_FOR_ROW(tdata->tg_event))
    {
      hv_store_string(hv, "new", plperl_hash_from_tuple(tdata->tg_trigtuple, tupdesc, !TRIGGER_FIRED_BEFORE(tdata->tg_event)));
    }
  }
  else if (TRIGGER_FIRED_BY_DELETE(tdata->tg_event))
  {
    event = "DELETE";
    if (TRIGGER_FIRED_FOR_ROW(tdata->tg_event))
    {
      hv_store_string(hv, "old", plperl_hash_from_tuple(tdata->tg_trigtuple, tupdesc, true));
    }
  }
  else if (TRIGGER_FIRED_BY_UPDATE(tdata->tg_event))
  {
    event = "UPDATE";
    if (TRIGGER_FIRED_FOR_ROW(tdata->tg_event))
    {
      hv_store_string(hv, "old", plperl_hash_from_tuple(tdata->tg_trigtuple, tupdesc, true));
      hv_store_string(hv, "new", plperl_hash_from_tuple(tdata->tg_newtuple, tupdesc, !TRIGGER_FIRED_BEFORE(tdata->tg_event)));
    }
  }
  else if (TRIGGER_FIRED_BY_TRUNCATE(tdata->tg_event))
  {
    event = "TRUNCATE";
  }
  else
  {
    event = "UNKNOWN";
  }

  hv_store_string(hv, "event", cstr2sv(event));
  hv_store_string(hv, "argc", newSViv(tdata->tg_trigger->tgnargs));

  if (tdata->tg_trigger->tgnargs > 0)
  {
    AV *av = newAV();

    av_extend(av, tdata->tg_trigger->tgnargs);
    for (i = 0; i < tdata->tg_trigger->tgnargs; i++)
    {
      av_push(av, cstr2sv(tdata->tg_trigger->tgargs[i]));
    }
    hv_store_string(hv, "args", newRV_noinc((SV *)av));
  }

  hv_store_string(hv, "relname", cstr2sv(SPI_getrelname(tdata->tg_relation)));

  hv_store_string(hv, "table_name", cstr2sv(SPI_getrelname(tdata->tg_relation)));

  hv_store_string(hv, "table_schema", cstr2sv(SPI_getnspname(tdata->tg_relation)));

  if (TRIGGER_FIRED_BEFORE(tdata->tg_event))
  {
    when = "BEFORE";
  }
  else if (TRIGGER_FIRED_AFTER(tdata->tg_event))
  {
    when = "AFTER";
  }
  else if (TRIGGER_FIRED_INSTEAD(tdata->tg_event))
  {
    when = "INSTEAD OF";
  }
  else
  {
    when = "UNKNOWN";
  }
  hv_store_string(hv, "when", cstr2sv(when));

  if (TRIGGER_FIRED_FOR_ROW(tdata->tg_event))
  {
    level = "ROW";
  }
  else if (TRIGGER_FIRED_FOR_STATEMENT(tdata->tg_event))
  {
    level = "STATEMENT";
  }
  else
  {
    level = "UNKNOWN";
  }
  hv_store_string(hv, "level", cstr2sv(level));

  return newRV_noinc((SV *)hv);
}

                                                     
static SV *
plperl_event_trigger_build_args(FunctionCallInfo fcinfo)
{
  dTHX;
  EventTriggerData *tdata;
  HV *hv;

  hv = newHV();

  tdata = (EventTriggerData *)fcinfo->context;

  hv_store_string(hv, "event", cstr2sv(tdata->event));
  hv_store_string(hv, "tag", cstr2sv(tdata->tag));

  return newRV_noinc((SV *)hv);
}

                                                                     
static HeapTuple
plperl_modify_tuple(HV *hvTD, TriggerData *tdata, HeapTuple otup)
{
  dTHX;
  SV **svp;
  HV *hvNew;
  HE *he;
  HeapTuple rtup;
  TupleDesc tupdesc;
  int natts;
  Datum *modvalues;
  bool *modnulls;
  bool *modrepls;

  svp = hv_fetch_string(hvTD, "new");
  if (!svp)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("$_TD->{new} does not exist")));
  }
  if (!SvOK(*svp) || !SvROK(*svp) || SvTYPE(SvRV(*svp)) != SVt_PVHV)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("$_TD->{new} is not a hash reference")));
  }
  hvNew = (HV *)SvRV(*svp);

  tupdesc = tdata->tg_relation->rd_att;
  natts = tupdesc->natts;

  modvalues = (Datum *)palloc0(natts * sizeof(Datum));
  modnulls = (bool *)palloc0(natts * sizeof(bool));
  modrepls = (bool *)palloc0(natts * sizeof(bool));

  hv_iterinit(hvNew);
  while ((he = hv_iternext(hvNew)))
  {
    char *key = hek2cstr(he);
    SV *val = HeVAL(he);
    int attn = SPI_fnumber(tupdesc, key);
    Form_pg_attribute attr = TupleDescAttr(tupdesc, attn - 1);

    if (attn == SPI_ERROR_NOATTRIBUTE)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("Perl hash contains nonexistent column \"%s\"", key)));
    }
    if (attn <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot set system attribute \"%s\"", key)));
    }
    if (attr->attgenerated)
    {
      ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("cannot set generated column \"%s\"", key)));
    }

    modvalues[attn - 1] = plperl_sv_to_datum(val, attr->atttypid, attr->atttypmod, NULL, NULL, InvalidOid, &modnulls[attn - 1]);
    modrepls[attn - 1] = true;

    pfree(key);
  }
  hv_iterinit(hvNew);

  rtup = heap_modify_tuple(otup, tupdesc, modvalues, modnulls, modrepls);

  pfree(modvalues);
  pfree(modnulls);
  pfree(modrepls);

  return rtup;
}

   
                                                                             
                                                
   

   
                                                                         
                                           
   
PG_FUNCTION_INFO_V1(plperl_call_handler);

Datum
plperl_call_handler(PG_FUNCTION_ARGS)
{
  Datum retval;
  plperl_call_data *volatile save_call_data = current_call_data;
  plperl_interp_desc *volatile oldinterp = plperl_active_interp;
  plperl_call_data this_call_data;

                                             
  MemSet(&this_call_data, 0, sizeof(this_call_data));
  this_call_data.fcinfo = fcinfo;

  PG_TRY();
  {
    current_call_data = &this_call_data;
    if (CALLED_AS_TRIGGER(fcinfo))
    {
      retval = PointerGetDatum(plperl_trigger_handler(fcinfo));
    }
    else if (CALLED_AS_EVENT_TRIGGER(fcinfo))
    {
      plperl_event_trigger_handler(fcinfo);
      retval = (Datum)0;
    }
    else
    {
      retval = plperl_func_handler(fcinfo);
    }
  }
  PG_CATCH();
  {
    current_call_data = save_call_data;
    activate_interpreter(oldinterp);
    if (this_call_data.prodesc)
    {
      decrement_prodesc_refcount(this_call_data.prodesc);
    }
    PG_RE_THROW();
  }
  PG_END_TRY();

  current_call_data = save_call_data;
  activate_interpreter(oldinterp);
  if (this_call_data.prodesc)
  {
    decrement_prodesc_refcount(this_call_data.prodesc);
  }
  return retval;
}

   
                                                              
   
PG_FUNCTION_INFO_V1(plperl_inline_handler);

Datum
plperl_inline_handler(PG_FUNCTION_ARGS)
{
  LOCAL_FCINFO(fake_fcinfo, 0);
  InlineCodeBlock *codeblock = (InlineCodeBlock *)PG_GETARG_POINTER(0);
  FmgrInfo flinfo;
  plperl_proc_desc desc;
  plperl_call_data *volatile save_call_data = current_call_data;
  plperl_interp_desc *volatile oldinterp = plperl_active_interp;
  plperl_call_data this_call_data;
  ErrorContextCallback pl_error_context;

                                             
  MemSet(&this_call_data, 0, sizeof(this_call_data));

                                             
  pl_error_context.callback = plperl_inline_callback;
  pl_error_context.previous = error_context_stack;
  pl_error_context.arg = NULL;
  error_context_stack = &pl_error_context;

     
                                                                          
                                                                           
                                                          
     
  MemSet(fake_fcinfo, 0, SizeForFunctionCallInfo(0));
  MemSet(&flinfo, 0, sizeof(flinfo));
  MemSet(&desc, 0, sizeof(desc));
  fake_fcinfo->flinfo = &flinfo;
  flinfo.fn_oid = InvalidOid;
  flinfo.fn_mcxt = CurrentMemoryContext;

  desc.proname = "inline_code_block";
  desc.fn_readonly = false;

  desc.lang_oid = codeblock->langOid;
  desc.trftypes = NIL;
  desc.lanpltrusted = codeblock->langIsTrusted;

  desc.fn_retistuple = false;
  desc.fn_retisset = false;
  desc.fn_retisarray = false;
  desc.result_oid = InvalidOid;
  desc.nargs = 0;
  desc.reference = NULL;

  this_call_data.fcinfo = fake_fcinfo;
  this_call_data.prodesc = &desc;
                                                          

  PG_TRY();
  {
    SV *perlret;

    current_call_data = &this_call_data;

    if (SPI_connect_ext(codeblock->atomic ? 0 : SPI_OPT_NONATOMIC) != SPI_OK_CONNECT)
    {
      elog(ERROR, "could not connect to SPI manager");
    }

    select_perl_context(desc.lanpltrusted);

    plperl_create_sub(&desc, codeblock->source_text, 0);

    if (!desc.reference)                       
    {
      elog(ERROR, "could not create internal procedure for anonymous code block");
    }

    perlret = plperl_call_perl_func(&desc, fake_fcinfo);

    SvREFCNT_dec_current(perlret);

    if (SPI_finish() != SPI_OK_FINISH)
    {
      elog(ERROR, "SPI_finish() failed");
    }
  }
  PG_CATCH();
  {
    if (desc.reference)
    {
      SvREFCNT_dec_current(desc.reference);
    }
    current_call_data = save_call_data;
    activate_interpreter(oldinterp);
    PG_RE_THROW();
  }
  PG_END_TRY();

  if (desc.reference)
  {
    SvREFCNT_dec_current(desc.reference);
  }

  current_call_data = save_call_data;
  activate_interpreter(oldinterp);

  error_context_stack = pl_error_context.previous;

  PG_RETURN_VOID();
}

   
                                                                           
                                                                        
                                              
   
PG_FUNCTION_INFO_V1(plperl_validator);

Datum
plperl_validator(PG_FUNCTION_ARGS)
{
  Oid funcoid = PG_GETARG_OID(0);
  HeapTuple tuple;
  Form_pg_proc proc;
  char functyptype;
  int numargs;
  Oid *argtypes;
  char **argnames;
  char *argmodes;
  bool is_trigger = false;
  bool is_event_trigger = false;
  int i;

  if (!CheckFunctionValidatorAccess(fcinfo->flinfo->fn_oid, funcoid))
  {
    PG_RETURN_VOID();
  }

                                            
  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcoid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for function %u", funcoid);
  }
  proc = (Form_pg_proc)GETSTRUCT(tuple);

  functyptype = get_typtype(proc->prorettype);

                                  
                                                       
  if (functyptype == TYPTYPE_PSEUDO)
  {
                                                            
    if (proc->prorettype == TRIGGEROID || (proc->prorettype == OPAQUEOID && proc->pronargs == 0))
    {
      is_trigger = true;
    }
    else if (proc->prorettype == EVTTRIGGEROID)
    {
      is_event_trigger = true;
    }
    else if (proc->prorettype != RECORDOID && proc->prorettype != VOIDOID)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/Perl functions cannot return type %s", format_type_be(proc->prorettype))));
    }
  }

                                                            
  numargs = get_func_arg_info(tuple, &argtypes, &argnames, &argmodes);
  for (i = 0; i < numargs; i++)
  {
    if (get_typtype(argtypes[i]) == TYPTYPE_PSEUDO && argtypes[i] != RECORDOID)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/Perl functions cannot accept type %s", format_type_be(argtypes[i]))));
    }
  }

  ReleaseSysCache(tuple);

                                                      
  if (check_function_bodies)
  {
    (void)compile_plperl_function(funcoid, is_trigger, is_event_trigger);
  }

                                            
  PG_RETURN_VOID();
}

   
                                                                 
                                                                        
                                                                    
                                                                     
                                                              
   

PG_FUNCTION_INFO_V1(plperlu_call_handler);

Datum
plperlu_call_handler(PG_FUNCTION_ARGS)
{
  return plperl_call_handler(fcinfo);
}

PG_FUNCTION_INFO_V1(plperlu_inline_handler);

Datum
plperlu_inline_handler(PG_FUNCTION_ARGS)
{
  return plperl_inline_handler(fcinfo);
}

PG_FUNCTION_INFO_V1(plperlu_validator);

Datum
plperlu_validator(PG_FUNCTION_ARGS)
{
                                                                
  return plperl_validator(fcinfo);
}

   
                                                                     
                                                
   
static void
plperl_create_sub(plperl_proc_desc *prodesc, const char *s, Oid fn_oid)
{
  dTHX;
  dSP;
  char subname[NAMEDATALEN + 40];
  HV *pragma_hv = newHV();
  SV *subref = NULL;
  int count;

  sprintf(subname, "%s__%u", prodesc->proname, fn_oid);

  if (plperl_use_strict)
  {
    hv_store_string(pragma_hv, "strict", (SV *)newAV());
  }

  ENTER;
  SAVETMPS;
  PUSHMARK(SP);
  EXTEND(SP, 4);
  PUSHs(sv_2mortal(cstr2sv(subname)));
  PUSHs(sv_2mortal(newRV_noinc((SV *)pragma_hv)));

     
                                                                           
                                                                             
               
     
  PUSHs(&PL_sv_no);
  PUSHs(sv_2mortal(cstr2sv(s)));
  PUTBACK;

     
                                                                        
                                                                          
                        
     
  count = call_pv("PostgreSQL::InServer::mkfunc", G_SCALAR | G_EVAL | G_KEEPERR);
  SPAGAIN;

  if (count == 1)
  {
    SV *sub_rv = (SV *)POPs;

    if (sub_rv && SvROK(sub_rv) && SvTYPE(SvRV(sub_rv)) == SVt_PVCV)
    {
      subref = newRV_inc(SvRV(sub_rv));
    }
  }

  PUTBACK;
  FREETMPS;
  LEAVE;

  if (SvTRUE(ERRSV))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV)))));
  }

  if (!subref)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("didn't get a CODE reference from compiling function \"%s\"", prodesc->proname)));
  }

  prodesc->reference = subref;

  return;
}

                                                                        
                                
                                                                        

static void
plperl_init_shared_libs(pTHX)
{
  char *file = __FILE__;

  newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
  newXS("PostgreSQL::InServer::Util::bootstrap", boot_PostgreSQL__InServer__Util, file);
                                                                
}

static SV *
plperl_call_perl_func(plperl_proc_desc *desc, FunctionCallInfo fcinfo)
{
  dTHX;
  dSP;
  SV *retval;
  int i;
  int count;
  Oid *argtypes = NULL;
  int nargs = 0;

  ENTER;
  SAVETMPS;

  PUSHMARK(SP);
  EXTEND(sp, desc->nargs);

                                                                     
  if (fcinfo->flinfo->fn_oid)
  {
    get_func_signature(fcinfo->flinfo->fn_oid, &argtypes, &nargs);
  }
  Assert(nargs == desc->nargs);

  for (i = 0; i < desc->nargs; i++)
  {
    if (fcinfo->args[i].isnull)
    {
      PUSHs(&PL_sv_undef);
    }
    else if (desc->arg_is_rowtype[i])
    {
      SV *sv = plperl_hash_from_datum(fcinfo->args[i].value);

      PUSHs(sv_2mortal(sv));
    }
    else
    {
      SV *sv;
      Oid funcid;

      if (OidIsValid(desc->arg_arraytype[i]))
      {
        sv = plperl_ref_from_pg_array(fcinfo->args[i].value, desc->arg_arraytype[i]);
      }
      else if ((funcid = get_transform_fromsql(argtypes[i], current_call_data->prodesc->lang_oid, current_call_data->prodesc->trftypes)))
      {
        sv = (SV *)DatumGetPointer(OidFunctionCall1(funcid, fcinfo->args[i].value));
      }
      else
      {
        char *tmp;

        tmp = OutputFunctionCall(&(desc->arg_out_func[i]), fcinfo->args[i].value);
        sv = cstr2sv(tmp);
        pfree(tmp);
      }

      PUSHs(sv_2mortal(sv));
    }
  }
  PUTBACK;

                                 
  count = call_sv(desc->reference, G_SCALAR | G_EVAL);

  SPAGAIN;

  if (count != 1)
  {
    PUTBACK;
    FREETMPS;
    LEAVE;
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("didn't get a return item from function")));
  }

  if (SvTRUE(ERRSV))
  {
    (void)POPs;
    PUTBACK;
    FREETMPS;
    LEAVE;
                                                                   
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV)))));
  }

  retval = newSVsv(POPs);

  PUTBACK;
  FREETMPS;
  LEAVE;

  return retval;
}

static SV *
plperl_call_perl_trigger_func(plperl_proc_desc *desc, FunctionCallInfo fcinfo, SV *td)
{
  dTHX;
  dSP;
  SV *retval, *TDsv;
  int i, count;
  Trigger *tg_trigger = ((TriggerData *)fcinfo->context)->tg_trigger;

  ENTER;
  SAVETMPS;

  TDsv = get_sv("main::_TD", 0);
  if (!TDsv)
  {
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("couldn't fetch $_TD")));
  }

  save_item(TDsv);                 
  sv_setsv(TDsv, td);

  PUSHMARK(sp);
  EXTEND(sp, tg_trigger->tgnargs);

  for (i = 0; i < tg_trigger->tgnargs; i++)
  {
    PUSHs(sv_2mortal(cstr2sv(tg_trigger->tgargs[i])));
  }
  PUTBACK;

                                 
  count = call_sv(desc->reference, G_SCALAR | G_EVAL);

  SPAGAIN;

  if (count != 1)
  {
    PUTBACK;
    FREETMPS;
    LEAVE;
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("didn't get a return item from trigger function")));
  }

  if (SvTRUE(ERRSV))
  {
    (void)POPs;
    PUTBACK;
    FREETMPS;
    LEAVE;
                                                                   
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV)))));
  }

  retval = newSVsv(POPs);

  PUTBACK;
  FREETMPS;
  LEAVE;

  return retval;
}

static void
plperl_call_perl_event_trigger_func(plperl_proc_desc *desc, FunctionCallInfo fcinfo, SV *td)
{
  dTHX;
  dSP;
  SV *retval, *TDsv;
  int count;

  ENTER;
  SAVETMPS;

  TDsv = get_sv("main::_TD", 0);
  if (!TDsv)
  {
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("couldn't fetch $_TD")));
  }

  save_item(TDsv);                 
  sv_setsv(TDsv, td);

  PUSHMARK(sp);
  PUTBACK;

                                 
  count = call_sv(desc->reference, G_SCALAR | G_EVAL);

  SPAGAIN;

  if (count != 1)
  {
    PUTBACK;
    FREETMPS;
    LEAVE;
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("didn't get a return item from trigger function")));
  }

  if (SvTRUE(ERRSV))
  {
    (void)POPs;
    PUTBACK;
    FREETMPS;
    LEAVE;
                                                                   
    ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION), errmsg("%s", strip_trailing_ws(sv2cstr(ERRSV)))));
  }

  retval = newSVsv(POPs);
  (void)retval;                               

  PUTBACK;
  FREETMPS;
  LEAVE;

  return;
}

static Datum
plperl_func_handler(PG_FUNCTION_ARGS)
{
  bool nonatomic;
  plperl_proc_desc *prodesc;
  SV *perlret;
  Datum retval = 0;
  ReturnSetInfo *rsi;
  ErrorContextCallback pl_error_context;

  nonatomic = fcinfo->context && IsA(fcinfo->context, CallContext) && !castNode(CallContext, fcinfo->context)->atomic;

  if (SPI_connect_ext(nonatomic ? SPI_OPT_NONATOMIC : 0) != SPI_OK_CONNECT)
  {
    elog(ERROR, "could not connect to SPI manager");
  }

  prodesc = compile_plperl_function(fcinfo->flinfo->fn_oid, false, false);
  current_call_data->prodesc = prodesc;
  increment_prodesc_refcount(prodesc);

                                          
  pl_error_context.callback = plperl_exec_callback;
  pl_error_context.previous = error_context_stack;
  pl_error_context.arg = prodesc->proname;
  error_context_stack = &pl_error_context;

  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (prodesc->fn_retisset)
  {
                                                              
    if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_Materialize) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that "
                                                                     "cannot accept a set")));
    }
  }

  activate_interpreter(prodesc->interp);

  perlret = plperl_call_perl_func(prodesc, fcinfo);

                                                                
                                                            
                                                              
                                                          
                                        
                                                                
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish() failed");
  }

  if (prodesc->fn_retisset)
  {
    SV *sav;

       
                                                                     
                                                                         
                                                                           
                                                                        
       
    sav = get_perl_array_ref(perlret);
    if (sav)
    {
      dTHX;
      int i = 0;
      SV **svp = 0;
      AV *rav = (AV *)SvRV(sav);

      while ((svp = av_fetch(rav, i, FALSE)) != NULL)
      {
        plperl_return_next_internal(*svp);
        i++;
      }
    }
    else if (SvOK(perlret))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("set-returning PL/Perl function must return "
                                                                 "reference to array or use return_next")));
    }

    rsi->returnMode = SFRM_Materialize;
    if (current_call_data->tuple_store)
    {
      rsi->setResult = current_call_data->tuple_store;
      rsi->setDesc = current_call_data->ret_tdesc;
    }
    retval = (Datum)0;
  }
  else if (prodesc->result_oid)
  {
    retval = plperl_sv_to_datum(perlret, prodesc->result_oid, -1, fcinfo, &prodesc->result_in_func, prodesc->result_typioparam, &fcinfo->isnull);

    if (fcinfo->isnull && rsi && IsA(rsi, ReturnSetInfo))
    {
      rsi->isDone = ExprEndResult;
    }
  }

                                           
  error_context_stack = pl_error_context.previous;

  SvREFCNT_dec_current(perlret);

  return retval;
}

static Datum
plperl_trigger_handler(PG_FUNCTION_ARGS)
{
  plperl_proc_desc *prodesc;
  SV *perlret;
  Datum retval;
  SV *svTD;
  HV *hvTD;
  ErrorContextCallback pl_error_context;
  TriggerData *tdata;
  int rc PG_USED_FOR_ASSERTS_ONLY;

                              
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "could not connect to SPI manager");
  }

                                                             
  tdata = (TriggerData *)fcinfo->context;
  rc = SPI_register_trigger_data(tdata);
  Assert(rc >= 0);

                                    
  prodesc = compile_plperl_function(fcinfo->flinfo->fn_oid, true, false);
  current_call_data->prodesc = prodesc;
  increment_prodesc_refcount(prodesc);

                                          
  pl_error_context.callback = plperl_exec_callback;
  pl_error_context.previous = error_context_stack;
  pl_error_context.arg = prodesc->proname;
  error_context_stack = &pl_error_context;

  activate_interpreter(prodesc->interp);

  svTD = plperl_trigger_build_args(fcinfo);
  perlret = plperl_call_perl_trigger_func(prodesc, fcinfo, svTD);
  hvTD = (HV *)SvRV(svTD);

                                                                
                                                            
                                                              
                                                          
                                        
                                                                
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish() failed");
  }

  if (perlret == NULL || !SvOK(perlret))
  {
                                                         
    TriggerData *trigdata = ((TriggerData *)fcinfo->context);

    if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
    {
      retval = (Datum)trigdata->tg_trigtuple;
    }
    else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
    {
      retval = (Datum)trigdata->tg_newtuple;
    }
    else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
    {
      retval = (Datum)trigdata->tg_trigtuple;
    }
    else if (TRIGGER_FIRED_BY_TRUNCATE(trigdata->tg_event))
    {
      retval = (Datum)trigdata->tg_trigtuple;
    }
    else
    {
      retval = (Datum)0;                       
    }
  }
  else
  {
    HeapTuple trv;
    char *tmp;

    tmp = sv2cstr(perlret);

    if (pg_strcasecmp(tmp, "SKIP") == 0)
    {
      trv = NULL;
    }
    else if (pg_strcasecmp(tmp, "MODIFY") == 0)
    {
      TriggerData *trigdata = (TriggerData *)fcinfo->context;

      if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
      {
        trv = plperl_modify_tuple(hvTD, trigdata, trigdata->tg_trigtuple);
      }
      else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
      {
        trv = plperl_modify_tuple(hvTD, trigdata, trigdata->tg_newtuple);
      }
      else
      {
        ereport(WARNING, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("ignoring modified row in DELETE trigger")));
        trv = NULL;
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("result of PL/Perl trigger function must be undef, "
                                                                                 "\"SKIP\", or \"MODIFY\"")));
      trv = NULL;
    }
    retval = PointerGetDatum(trv);
    pfree(tmp);
  }

                                           
  error_context_stack = pl_error_context.previous;

  SvREFCNT_dec_current(svTD);
  if (perlret)
  {
    SvREFCNT_dec_current(perlret);
  }

  return retval;
}

static void
plperl_event_trigger_handler(PG_FUNCTION_ARGS)
{
  plperl_proc_desc *prodesc;
  SV *svTD;
  ErrorContextCallback pl_error_context;

                              
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "could not connect to SPI manager");
  }

                                    
  prodesc = compile_plperl_function(fcinfo->flinfo->fn_oid, false, true);
  current_call_data->prodesc = prodesc;
  increment_prodesc_refcount(prodesc);

                                          
  pl_error_context.callback = plperl_exec_callback;
  pl_error_context.previous = error_context_stack;
  pl_error_context.arg = prodesc->proname;
  error_context_stack = &pl_error_context;

  activate_interpreter(prodesc->interp);

  svTD = plperl_event_trigger_build_args(fcinfo);
  plperl_call_perl_event_trigger_func(prodesc, fcinfo, svTD);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish() failed");
  }

                                           
  error_context_stack = pl_error_context.previous;

  SvREFCNT_dec_current(svTD);
}

static bool
validate_plperl_function(plperl_proc_ptr *proc_ptr, HeapTuple procTup)
{
  if (proc_ptr && proc_ptr->proc_ptr)
  {
    plperl_proc_desc *prodesc = proc_ptr->proc_ptr;
    bool uptodate;

                                                                  
                                                                  
                                                                        
                                                          
                                                                  
    uptodate = (prodesc->fn_xmin == HeapTupleHeaderGetRawXmin(procTup->t_data) && ItemPointerEquals(&prodesc->fn_tid, &procTup->t_self));

    if (uptodate)
    {
      return true;
    }

                                                                      
    proc_ptr->proc_ptr = NULL;
                                                                          
    decrement_prodesc_refcount(prodesc);
  }

  return false;
}

static void
free_plperl_function(plperl_proc_desc *prodesc)
{
  Assert(prodesc->fn_refcount == 0);
                                                                           
  if (prodesc->reference)
  {
    plperl_interp_desc *oldinterp = plperl_active_interp;

    activate_interpreter(prodesc->interp);
    SvREFCNT_dec_current(prodesc->reference);
    activate_interpreter(oldinterp);
  }
                                               
  MemoryContextDelete(prodesc->fn_cxt);
}

static plperl_proc_desc *
compile_plperl_function(Oid fn_oid, bool is_trigger, bool is_event_trigger)
{
  HeapTuple procTup;
  Form_pg_proc procStruct;
  plperl_proc_key proc_key;
  plperl_proc_ptr *proc_ptr;
  plperl_proc_desc *volatile prodesc = NULL;
  volatile MemoryContext proc_cxt = NULL;
  plperl_interp_desc *oldinterp = plperl_active_interp;
  ErrorContextCallback plperl_error_context;

                                                   
  procTup = SearchSysCache1(PROCOID, ObjectIdGetDatum(fn_oid));
  if (!HeapTupleIsValid(procTup))
  {
    elog(ERROR, "cache lookup failed for function %u", fn_oid);
  }
  procStruct = (Form_pg_proc)GETSTRUCT(procTup);

     
                                                                    
                                                                            
                                                                             
                  
     
  proc_key.proc_id = fn_oid;
  proc_key.is_trigger = is_trigger;
  proc_key.user_id = GetUserId();
  proc_ptr = hash_search(plperl_proc_hash, &proc_key, HASH_FIND, NULL);
  if (validate_plperl_function(proc_ptr, procTup))
  {
                                  
    ReleaseSysCache(procTup);
    return proc_ptr->proc_ptr;
  }

                                                    
  proc_key.user_id = InvalidOid;
  proc_ptr = hash_search(plperl_proc_hash, &proc_key, HASH_FIND, NULL);
  if (validate_plperl_function(proc_ptr, procTup))
  {
                                   
    ReleaseSysCache(procTup);
    return proc_ptr->proc_ptr;
  }

                                                                
                                                         
                                                        
                                                 
                                                           
                                                      
                                                                

                                                       
  plperl_error_context.callback = plperl_compile_callback;
  plperl_error_context.previous = error_context_stack;
  plperl_error_context.arg = NameStr(procStruct->proname);
  error_context_stack = &plperl_error_context;

  PG_TRY();
  {
    HeapTuple langTup;
    HeapTuple typeTup;
    Form_pg_language langStruct;
    Form_pg_type typeStruct;
    Datum protrftypes_datum;
    Datum prosrcdatum;
    bool isnull;
    char *proc_source;
    MemoryContext oldcontext;

                                                                  
                                                                        
                                                                  
    proc_cxt = AllocSetContextCreate(TopMemoryContext, "PL/Perl function", ALLOCSET_SMALL_SIZES);

                                                                  
                                                            
                                                                     
                                                                  
    oldcontext = MemoryContextSwitchTo(proc_cxt);
    prodesc = (plperl_proc_desc *)palloc0(sizeof(plperl_proc_desc));
    prodesc->proname = pstrdup(NameStr(procStruct->proname));
    MemoryContextSetIdentifier(proc_cxt, prodesc->proname);
    prodesc->fn_cxt = proc_cxt;
    prodesc->fn_refcount = 0;
    prodesc->fn_xmin = HeapTupleHeaderGetRawXmin(procTup->t_data);
    prodesc->fn_tid = procTup->t_self;
    prodesc->nargs = procStruct->pronargs;
    prodesc->arg_out_func = (FmgrInfo *)palloc0(prodesc->nargs * sizeof(FmgrInfo));
    prodesc->arg_is_rowtype = (bool *)palloc0(prodesc->nargs * sizeof(bool));
    prodesc->arg_arraytype = (Oid *)palloc0(prodesc->nargs * sizeof(Oid));
    MemoryContextSwitchTo(oldcontext);

                                                  
    prodesc->fn_readonly = (procStruct->provolatile != PROVOLATILE_VOLATILE);

                           
    protrftypes_datum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_protrftypes, &isnull);
    MemoryContextSwitchTo(proc_cxt);
    prodesc->trftypes = isnull ? NIL : oid_array_to_list(protrftypes_datum);
    MemoryContextSwitchTo(oldcontext);

                                                                  
                                           
                                                                  
    langTup = SearchSysCache1(LANGOID, ObjectIdGetDatum(procStruct->prolang));
    if (!HeapTupleIsValid(langTup))
    {
      elog(ERROR, "cache lookup failed for language %u", procStruct->prolang);
    }
    langStruct = (Form_pg_language)GETSTRUCT(langTup);
    prodesc->lang_oid = langStruct->oid;
    prodesc->lanpltrusted = langStruct->lanpltrusted;
    ReleaseSysCache(langTup);

                                                                  
                                                                
                     
                                                                  
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
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("trigger functions can only be called "
                                                                         "as triggers")));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/Perl functions cannot return type %s", format_type_be(rettype))));
        }
      }

      prodesc->result_oid = rettype;
      prodesc->fn_retisset = procStruct->proretset;
      prodesc->fn_retistuple = type_is_rowtype(rettype);

      prodesc->fn_retisarray = (typeStruct->typlen == -1 && typeStruct->typelem);

      fmgr_info_cxt(typeStruct->typinput, &(prodesc->result_in_func), proc_cxt);
      prodesc->result_typioparam = getTypeIOParam(typeTup);

      ReleaseSysCache(typeTup);
    }

                                                                  
                                                          
                                  
                                                                  
    if (!is_trigger && !is_event_trigger)
    {
      int i;

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
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("PL/Perl functions cannot accept type %s", format_type_be(argtype))));
        }

        if (type_is_rowtype(argtype))
        {
          prodesc->arg_is_rowtype[i] = true;
        }
        else
        {
          prodesc->arg_is_rowtype[i] = false;
          fmgr_info_cxt(typeStruct->typoutput, &(prodesc->arg_out_func[i]), proc_cxt);
        }

                                           
        if (typeStruct->typelem != 0 && typeStruct->typlen == -1)
        {
          prodesc->arg_arraytype[i] = argtype;
        }
        else
        {
          prodesc->arg_arraytype[i] = InvalidOid;
        }

        ReleaseSysCache(typeTup);
      }
    }

                                                                  
                                                    
                                                                     
                              
                                                                  
    prosrcdatum = SysCacheGetAttr(PROCOID, procTup, Anum_pg_proc_prosrc, &isnull);
    if (isnull)
    {
      elog(ERROR, "null prosrc");
    }
    proc_source = TextDatumGetCString(prosrcdatum);

                                                                  
                                                           
                                                                  

    select_perl_context(prodesc->lanpltrusted);

    prodesc->interp = plperl_active_interp;

    plperl_create_sub(prodesc, proc_source, fn_oid);

    activate_interpreter(oldinterp);

    pfree(proc_source);

    if (!prodesc->reference)                       
    {
      elog(ERROR, "could not create PL/Perl internal procedure");
    }

                                                                  
                                                                
                                                                         
                                                                         
                                                             
                                                                  
    proc_key.user_id = prodesc->lanpltrusted ? GetUserId() : InvalidOid;

    proc_ptr = hash_search(plperl_proc_hash, &proc_key, HASH_ENTER, NULL);
                                                         
    proc_ptr->proc_ptr = prodesc;
    increment_prodesc_refcount(prodesc);
  }
  PG_CATCH();
  {
       
                                                                          
                                                                         
                                                                       
       
    if (prodesc && prodesc->reference)
    {
      free_plperl_function(prodesc);
    }
    else if (proc_cxt)
    {
      MemoryContextDelete(proc_cxt);
    }

                                                                    
    activate_interpreter(oldinterp);

    PG_RE_THROW();
  }
  PG_END_TRY();

                                       
  error_context_stack = plperl_error_context.previous;

  ReleaseSysCache(procTup);

  return prodesc;
}

                                                   
static SV *
plperl_hash_from_datum(Datum attr)
{
  HeapTupleHeader td;
  Oid tupType;
  int32 tupTypmod;
  TupleDesc tupdesc;
  HeapTupleData tmptup;
  SV *sv;

  td = DatumGetHeapTupleHeader(attr);

                                               
  tupType = HeapTupleHeaderGetTypeId(td);
  tupTypmod = HeapTupleHeaderGetTypMod(td);
  tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

                                                     
  tmptup.t_len = HeapTupleHeaderGetDatumLength(td);
  tmptup.t_data = td;

  sv = plperl_hash_from_tuple(&tmptup, tupdesc, true);
  ReleaseTupleDesc(tupdesc);

  return sv;
}

                                                        
static SV *
plperl_hash_from_tuple(HeapTuple tuple, TupleDesc tupdesc, bool include_generated)
{
  dTHX;
  HV *hv;
  int i;

                                                                          
  check_stack_depth();

  hv = newHV();
  hv_ksplit(hv, tupdesc->natts);                        

  for (i = 0; i < tupdesc->natts; i++)
  {
    Datum attr;
    bool isnull, typisvarlena;
    char *attname;
    Oid typoutput;
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

    if (isnull)
    {
         
                                                                  
                                                                   
                                      
         
      hv_store_string(hv, attname, newSV(0));
      continue;
    }

    if (type_is_rowtype(att->atttypid))
    {
      SV *sv = plperl_hash_from_datum(attr);

      hv_store_string(hv, attname, sv);
    }
    else
    {
      SV *sv;
      Oid funcid;

      if (OidIsValid(get_base_element_type(att->atttypid)))
      {
        sv = plperl_ref_from_pg_array(attr, att->atttypid);
      }
      else if ((funcid = get_transform_fromsql(att->atttypid, current_call_data->prodesc->lang_oid, current_call_data->prodesc->trftypes)))
      {
        sv = (SV *)DatumGetPointer(OidFunctionCall1(funcid, attr));
      }
      else
      {
        char *outputstr;

                                                          
        getTypeOutputInfo(att->atttypid, &typoutput, &typisvarlena);

        outputstr = OidOutputFunctionCall(typoutput, attr);
        sv = cstr2sv(outputstr);
        pfree(outputstr);
      }

      hv_store_string(hv, attname, sv);
    }
  }
  return newRV_noinc((SV *)hv);
}

static void
check_spi_usage_allowed(void)
{
                                    
  if (plperl_ending)
  {
                                                                  
    croak("SPI functions can not be used in END blocks");
  }

     
                                                                       
                                                                             
                                                                           
                                                                         
                                                                          
                                                                    
                                     
     
  if (current_call_data == NULL || current_call_data->prodesc == NULL)
  {
                                                                  
    croak("SPI functions can not be used during function compilation");
  }
}

HV *
plperl_spi_exec(char *query, int limit)
{
  HV *ret_hv;

     
                                                                            
            
     
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

  check_spi_usage_allowed();

  BeginInternalSubTransaction(NULL);
                                                    
  MemoryContextSwitchTo(oldcontext);

  PG_TRY();
  {
    int spi_rv;

    pg_verifymbstr(query, strlen(query), false);

    spi_rv = SPI_execute(query, current_call_data->prodesc->fn_readonly, limit);
    ret_hv = plperl_spi_execute_fetch_result(SPI_tuptable, SPI_processed, spi_rv);

                                                                    
    ReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                     
    RollbackAndReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;

                                
    croak_cstr(edata->message);

                                                 
    return NULL;
  }
  PG_END_TRY();

  return ret_hv;
}

static HV *
plperl_spi_execute_fetch_result(SPITupleTable *tuptable, uint64 processed, int status)
{
  dTHX;
  HV *result;

  check_spi_usage_allowed();

  result = newHV();

  hv_store_string(result, "status", cstr2sv(SPI_result_code_string(status)));
  hv_store_string(result, "processed", (processed > (uint64)UV_MAX) ? newSVnv((NV)processed) : newSVuv((UV)processed));

  if (status > 0 && tuptable)
  {
    AV *rows;
    SV *row;
    uint64 i;

                                                 
    if (processed > (uint64)AV_SIZE_MAX)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("query result has too many rows to fit in a Perl array")));
    }

    rows = newAV();
    av_extend(rows, processed);
    for (i = 0; i < processed; i++)
    {
      row = plperl_hash_from_tuple(tuptable->vals[i], tuptable->tupdesc, true);
      av_push(rows, row);
    }
    hv_store_string(result, "rows", newRV_noinc((SV *)rows));
  }

  SPI_freetuptable(tuptable);

  return result;
}

   
                                                                         
                                                                             
                                                             
   
void
plperl_return_next(SV *sv)
{
  MemoryContext oldcontext = CurrentMemoryContext;

  check_spi_usage_allowed();

  PG_TRY();
  {
    plperl_return_next_internal(sv);
  }
  PG_CATCH();
  {
    ErrorData *edata;

                                   
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                
    croak_cstr(edata->message);
  }
  PG_END_TRY();
}

   
                                                                      
                  
   
static void
plperl_return_next_internal(SV *sv)
{
  plperl_proc_desc *prodesc;
  FunctionCallInfo fcinfo;
  ReturnSetInfo *rsi;
  MemoryContext old_cxt;

  if (!sv)
  {
    return;
  }

  prodesc = current_call_data->prodesc;
  fcinfo = current_call_data->fcinfo;
  rsi = (ReturnSetInfo *)fcinfo->resultinfo;

  if (!prodesc->fn_retisset)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use return_next in a non-SETOF function")));
  }

  if (!current_call_data->ret_tdesc)
  {
    TupleDesc tupdesc;

    Assert(!current_call_data->tuple_store);

       
                                                                    
                                                                     
                                           
       
    if (prodesc->fn_retistuple)
    {
      TypeFuncClass funcclass;
      Oid typid;

      funcclass = get_call_result_type(fcinfo, &typid, &tupdesc);
      if (funcclass != TYPEFUNC_COMPOSITE && funcclass != TYPEFUNC_COMPOSITE_DOMAIN)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function returning record called in context "
                                                                       "that cannot accept type record")));
      }
                                                                    
      if (funcclass == TYPEFUNC_COMPOSITE_DOMAIN)
      {
        current_call_data->cdomain_oid = typid;
      }
    }
    else
    {
      tupdesc = rsi->expectedDesc;
                                                                      
      if (tupdesc == NULL || tupdesc->natts != 1)
      {
        elog(ERROR, "expected single-column result descriptor for non-composite SETOF result");
      }
    }

       
                                                                
                   
       
    old_cxt = MemoryContextSwitchTo(rsi->econtext->ecxt_per_query_memory);

    current_call_data->ret_tdesc = CreateTupleDescCopy(tupdesc);
    current_call_data->tuple_store = tuplestore_begin_heap(rsi->allowedModes & SFRM_Materialize_Random, false, work_mem);

    MemoryContextSwitchTo(old_cxt);
  }

     
                                                                     
                                                                           
                                                                         
                                                          
     
  if (!current_call_data->tmp_cxt)
  {
    current_call_data->tmp_cxt = AllocSetContextCreate(CurrentMemoryContext, "PL/Perl return_next temporary cxt", ALLOCSET_DEFAULT_SIZES);
  }

  old_cxt = MemoryContextSwitchTo(current_call_data->tmp_cxt);

  if (prodesc->fn_retistuple)
  {
    HeapTuple tuple;

    if (!(SvOK(sv) && SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVHV))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("SETOF-composite-returning PL/Perl function "
                                                                 "must call return_next with reference to hash")));
    }

    tuple = plperl_build_tuple_result((HV *)SvRV(sv), current_call_data->ret_tdesc);

    if (OidIsValid(current_call_data->cdomain_oid))
    {
      domain_check(HeapTupleGetDatum(tuple), false, current_call_data->cdomain_oid, &current_call_data->cdomain_info, rsi->econtext->ecxt_per_query_memory);
    }

    tuplestore_puttuple(current_call_data->tuple_store, tuple);
  }
  else if (prodesc->result_oid)
  {
    Datum ret[1];
    bool isNull[1];

    ret[0] = plperl_sv_to_datum(sv, prodesc->result_oid, -1, fcinfo, &prodesc->result_in_func, prodesc->result_typioparam, &isNull[0]);

    tuplestore_putvalues(current_call_data->tuple_store, current_call_data->ret_tdesc, ret, isNull);
  }

  MemoryContextSwitchTo(old_cxt);
  MemoryContextReset(current_call_data->tmp_cxt);
}

SV *
plperl_spi_query(char *query)
{
  SV *cursor;

     
                                                                            
            
     
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

  check_spi_usage_allowed();

  BeginInternalSubTransaction(NULL);
                                                    
  MemoryContextSwitchTo(oldcontext);

  PG_TRY();
  {
    SPIPlanPtr plan;
    Portal portal;

                                                
    pg_verifymbstr(query, strlen(query), false);

                                       
    plan = SPI_prepare(query, 0, NULL);
    if (plan == NULL)
    {
      elog(ERROR, "SPI_prepare() failed:%s", SPI_result_code_string(SPI_result));
    }

    portal = SPI_cursor_open(NULL, plan, NULL, NULL, false);
    SPI_freeplan(plan);
    if (portal == NULL)
    {
      elog(ERROR, "SPI_cursor_open() failed:%s", SPI_result_code_string(SPI_result));
    }
    cursor = cstr2sv(portal->name);

    PinPortal(portal);

                                                                    
    ReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                     
    RollbackAndReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;

                                
    croak_cstr(edata->message);

                                                 
    return NULL;
  }
  PG_END_TRY();

  return cursor;
}

SV *
plperl_spi_fetchrow(char *cursor)
{
  SV *row;

     
                                                                            
            
     
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

  check_spi_usage_allowed();

  BeginInternalSubTransaction(NULL);
                                                    
  MemoryContextSwitchTo(oldcontext);

  PG_TRY();
  {
    dTHX;
    Portal p = SPI_cursor_find(cursor);

    if (!p)
    {
      row = &PL_sv_undef;
    }
    else
    {
      SPI_cursor_fetch(p, true, 1);
      if (SPI_processed == 0)
      {
        UnpinPortal(p);
        SPI_cursor_close(p);
        row = &PL_sv_undef;
      }
      else
      {
        row = plperl_hash_from_tuple(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, true);
      }
      SPI_freetuptable(SPI_tuptable);
    }

                                                                    
    ReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                     
    RollbackAndReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;

                                
    croak_cstr(edata->message);

                                                 
    return NULL;
  }
  PG_END_TRY();

  return row;
}

void
plperl_spi_cursor_close(char *cursor)
{
  Portal p;

  check_spi_usage_allowed();

  p = SPI_cursor_find(cursor);

  if (p)
  {
    UnpinPortal(p);
    SPI_cursor_close(p);
  }
}

SV *
plperl_spi_prepare(char *query, int argc, SV **argv)
{
  volatile SPIPlanPtr plan = NULL;
  volatile MemoryContext plan_cxt = NULL;
  plperl_query_desc *volatile qdesc = NULL;
  plperl_query_entry *volatile hash_entry = NULL;
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;
  MemoryContext work_cxt;
  bool found;
  int i;

  check_spi_usage_allowed();

  BeginInternalSubTransaction(NULL);
  MemoryContextSwitchTo(oldcontext);

  PG_TRY();
  {
    CHECK_FOR_INTERRUPTS();

                                                                  
                                            
       
                                                                          
                                                      
                                                                  
    plan_cxt = AllocSetContextCreate(TopMemoryContext, "PL/Perl spi_prepare query", ALLOCSET_SMALL_SIZES);
    MemoryContextSwitchTo(plan_cxt);
    qdesc = (plperl_query_desc *)palloc0(sizeof(plperl_query_desc));
    snprintf(qdesc->qname, sizeof(qdesc->qname), "%p", qdesc);
    qdesc->plan_cxt = plan_cxt;
    qdesc->nargs = argc;
    qdesc->argtypes = (Oid *)palloc(argc * sizeof(Oid));
    qdesc->arginfuncs = (FmgrInfo *)palloc(argc * sizeof(FmgrInfo));
    qdesc->argtypioparams = (Oid *)palloc(argc * sizeof(Oid));
    MemoryContextSwitchTo(oldcontext);

                                                                  
                                                                       
                                                                        
                                                                  
    work_cxt = AllocSetContextCreate(CurrentMemoryContext, "PL/Perl spi_prepare workspace", ALLOCSET_DEFAULT_SIZES);
    MemoryContextSwitchTo(work_cxt);

                                                                  
                                                                
                                                                  
                             
                                                                  
    for (i = 0; i < argc; i++)
    {
      Oid typId, typInput, typIOParam;
      int32 typmod;
      char *typstr;

      typstr = sv2cstr(argv[i]);
      parseTypeString(typstr, &typId, &typmod, false);
      pfree(typstr);

      getTypeInputInfo(typId, &typInput, &typIOParam);

      qdesc->argtypes[i] = typId;
      fmgr_info_cxt(typInput, &(qdesc->arginfuncs[i]), plan_cxt);
      qdesc->argtypioparams[i] = typIOParam;
    }

                                                
    pg_verifymbstr(query, strlen(query), false);

                                                                  
                                             
                                                                  
    plan = SPI_prepare(query, argc, qdesc->argtypes);

    if (plan == NULL)
    {
      elog(ERROR, "SPI_prepare() failed:%s", SPI_result_code_string(SPI_result));
    }

                                                                  
                                                                  
                                                         
                                                                  
    if (SPI_keepplan(plan))
    {
      elog(ERROR, "SPI_keepplan() failed");
    }
    qdesc->plan = plan;

                                                                  
                                              
                                                                  
    hash_entry = hash_search(plperl_active_interp->query_hash, qdesc->qname, HASH_ENTER, &found);
    hash_entry->query_data = qdesc;

                              
    MemoryContextDelete(work_cxt);

                                                                    
    ReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                              
    if (hash_entry)
    {
      hash_search(plperl_active_interp->query_hash, qdesc->qname, HASH_REMOVE, NULL);
    }
    if (plan_cxt)
    {
      MemoryContextDelete(plan_cxt);
    }
    if (plan)
    {
      SPI_freeplan(plan);
    }

                                     
    RollbackAndReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;

                                
    croak_cstr(edata->message);

                                                 
    return NULL;
  }
  PG_END_TRY();

                                                                
                                                
                                                                
  return cstr2sv(qdesc->qname);
}

HV *
plperl_spi_exec_prepared(char *query, HV *attr, int argc, SV **argv)
{
  HV *ret_hv;
  SV **sv;
  int i, limit, spi_rv;
  char *nulls;
  Datum *argvalues;
  plperl_query_desc *qdesc;
  plperl_query_entry *hash_entry;

     
                                                                            
            
     
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

  check_spi_usage_allowed();

  BeginInternalSubTransaction(NULL);
                                                    
  MemoryContextSwitchTo(oldcontext);

  PG_TRY();
  {
    dTHX;

                                                                  
                                                         
                                                                  
    hash_entry = hash_search(plperl_active_interp->query_hash, query, HASH_FIND, NULL);
    if (hash_entry == NULL)
    {
      elog(ERROR, "spi_exec_prepared: Invalid prepared query passed");
    }

    qdesc = hash_entry->query_data;
    if (qdesc == NULL)
    {
      elog(ERROR, "spi_exec_prepared: plperl query_hash value vanished");
    }

    if (qdesc->nargs != argc)
    {
      elog(ERROR, "spi_exec_prepared: expected %d argument(s), %d passed", qdesc->nargs, argc);
    }

                                                                  
                                 
                                                                  
    limit = 0;
    if (attr != NULL)
    {
      sv = hv_fetch_string(attr, "limit");
      if (sv && *sv && SvIOK(*sv))
      {
        limit = SvIV(*sv);
      }
    }
                                                                  
                        
                                                                  
    if (argc > 0)
    {
      nulls = (char *)palloc(argc);
      argvalues = (Datum *)palloc(argc * sizeof(Datum));
    }
    else
    {
      nulls = NULL;
      argvalues = NULL;
    }

    for (i = 0; i < argc; i++)
    {
      bool isnull;

      argvalues[i] = plperl_sv_to_datum(argv[i], qdesc->argtypes[i], -1, NULL, &qdesc->arginfuncs[i], qdesc->argtypioparams[i], &isnull);
      nulls[i] = isnull ? 'n' : ' ';
    }

                                                                  
          
                                                                  
    spi_rv = SPI_execute_plan(qdesc->plan, argvalues, nulls, current_call_data->prodesc->fn_readonly, limit);
    ret_hv = plperl_spi_execute_fetch_result(SPI_tuptable, SPI_processed, spi_rv);
    if (argc > 0)
    {
      pfree(argvalues);
      pfree(nulls);
    }

                                                                    
    ReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                     
    RollbackAndReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;

                                
    croak_cstr(edata->message);

                                                 
    return NULL;
  }
  PG_END_TRY();

  return ret_hv;
}

SV *
plperl_spi_query_prepared(char *query, int argc, SV **argv)
{
  int i;
  char *nulls;
  Datum *argvalues;
  plperl_query_desc *qdesc;
  plperl_query_entry *hash_entry;
  SV *cursor;
  Portal portal = NULL;

     
                                                                            
            
     
  MemoryContext oldcontext = CurrentMemoryContext;
  ResourceOwner oldowner = CurrentResourceOwner;

  check_spi_usage_allowed();

  BeginInternalSubTransaction(NULL);
                                                    
  MemoryContextSwitchTo(oldcontext);

  PG_TRY();
  {
                                                                  
                                                         
                                                                  
    hash_entry = hash_search(plperl_active_interp->query_hash, query, HASH_FIND, NULL);
    if (hash_entry == NULL)
    {
      elog(ERROR, "spi_query_prepared: Invalid prepared query passed");
    }

    qdesc = hash_entry->query_data;
    if (qdesc == NULL)
    {
      elog(ERROR, "spi_query_prepared: plperl query_hash value vanished");
    }

    if (qdesc->nargs != argc)
    {
      elog(ERROR, "spi_query_prepared: expected %d argument(s), %d passed", qdesc->nargs, argc);
    }

                                                                  
                        
                                                                  
    if (argc > 0)
    {
      nulls = (char *)palloc(argc);
      argvalues = (Datum *)palloc(argc * sizeof(Datum));
    }
    else
    {
      nulls = NULL;
      argvalues = NULL;
    }

    for (i = 0; i < argc; i++)
    {
      bool isnull;

      argvalues[i] = plperl_sv_to_datum(argv[i], qdesc->argtypes[i], -1, NULL, &qdesc->arginfuncs[i], qdesc->argtypioparams[i], &isnull);
      nulls[i] = isnull ? 'n' : ' ';
    }

                                                                  
          
                                                                  
    portal = SPI_cursor_open(NULL, qdesc->plan, argvalues, nulls, current_call_data->prodesc->fn_readonly);
    if (argc > 0)
    {
      pfree(argvalues);
      pfree(nulls);
    }
    if (portal == NULL)
    {
      elog(ERROR, "SPI_cursor_open() failed:%s", SPI_result_code_string(SPI_result));
    }

    cursor = cstr2sv(portal->name);

    PinPortal(portal);

                                                                    
    ReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;
  }
  PG_CATCH();
  {
    ErrorData *edata;

                         
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

                                     
    RollbackAndReleaseCurrentSubTransaction();
    MemoryContextSwitchTo(oldcontext);
    CurrentResourceOwner = oldowner;

                                
    croak_cstr(edata->message);

                                                 
    return NULL;
  }
  PG_END_TRY();

  return cursor;
}

void
plperl_spi_freeplan(char *query)
{
  SPIPlanPtr plan;
  plperl_query_desc *qdesc;
  plperl_query_entry *hash_entry;

  check_spi_usage_allowed();

  hash_entry = hash_search(plperl_active_interp->query_hash, query, HASH_FIND, NULL);
  if (hash_entry == NULL)
  {
    elog(ERROR, "spi_freeplan: Invalid prepared query passed");
  }

  qdesc = hash_entry->query_data;
  if (qdesc == NULL)
  {
    elog(ERROR, "spi_freeplan: plperl query_hash value vanished");
  }
  plan = qdesc->plan;

     
                                                                         
               
     
  hash_search(plperl_active_interp->query_hash, query, HASH_REMOVE, NULL);

  MemoryContextDelete(qdesc->plan_cxt);

  SPI_freeplan(plan);
}

void
plperl_spi_commit(void)
{
  MemoryContext oldcontext = CurrentMemoryContext;

  check_spi_usage_allowed();

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

                                
    croak_cstr(edata->message);
  }
  PG_END_TRY();
}

void
plperl_spi_rollback(void)
{
  MemoryContext oldcontext = CurrentMemoryContext;

  check_spi_usage_allowed();

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

                                
    croak_cstr(edata->message);
  }
  PG_END_TRY();
}

   
                                              
   
                                                                          
                                                                      
                                                                          
                                                                            
   
                                                                            
                          
   
void
plperl_util_elog(int level, SV *msg)
{
  MemoryContext oldcontext = CurrentMemoryContext;
  char *volatile cmsg = NULL;

     
                                                                         
                                                                    
     

  PG_TRY();
  {
    cmsg = sv2cstr(msg);
    elog(level, "%s", cmsg);
    pfree(cmsg);
  }
  PG_CATCH();
  {
    ErrorData *edata;

                                   
    MemoryContextSwitchTo(oldcontext);
    edata = CopyErrorData();
    FlushErrorState();

    if (cmsg)
    {
      pfree(cmsg);
    }

                                
    croak_cstr(edata->message);
  }
  PG_END_TRY();
}

   
                                                                            
                                       
   
static SV **
hv_store_string(HV *hv, const char *key, SV *val)
{
  dTHX;
  int32 hlen;
  char *hkey;
  SV **ret;

  hkey = pg_server_to_any(key, strlen(key), PG_UTF8);

     
                                                                        
                  
     
  hlen = -(int)strlen(hkey);
  ret = hv_store(hv, hkey, hlen, val, 0);

  if (hkey != key)
  {
    pfree(hkey);
  }

  return ret;
}

   
                                                                            
                                       
   
static SV **
hv_fetch_string(HV *hv, const char *key)
{
  dTHX;
  int32 hlen;
  char *hkey;
  SV **ret;

  hkey = pg_server_to_any(key, strlen(key), PG_UTF8);

                                    
  hlen = -(int)strlen(hkey);
  ret = hv_fetch(hv, hkey, hlen, 0);

  if (hkey != key)
  {
    pfree(hkey);
  }

  return ret;
}

   
                                                      
   
static void
plperl_exec_callback(void *arg)
{
  char *procname = (char *)arg;

  if (procname)
  {
    errcontext("PL/Perl function \"%s\"", procname);
  }
}

   
                                                        
   
static void
plperl_compile_callback(void *arg)
{
  char *procname = (char *)arg;

  if (procname)
  {
    errcontext("compilation of PL/Perl function \"%s\"", procname);
  }
}

   
                                                
   
static void
plperl_inline_callback(void *arg)
{
  errcontext("PL/Perl anonymous code block");
}

   
                                                
                                            
   
                                                           
   
#if defined(WIN32) && PERL_VERSION_LT(5, 28, 0)
static char *
setlocale_perl(int category, char *locale)
{
  dTHX;
  char *RETVAL = setlocale(category, locale);

  if (RETVAL)
  {
#ifdef USE_LOCALE_CTYPE
    if (category == LC_CTYPE
#ifdef LC_ALL
        || category == LC_ALL
#endif
    )
    {
      char *newctype;

#ifdef LC_ALL
      if (category == LC_ALL)
      {
        newctype = setlocale(LC_CTYPE, NULL);
      }
      else
#endif
        newctype = RETVAL;
      new_ctype(newctype);
    }
#endif                       
#ifdef USE_LOCALE_COLLATE
    if (category == LC_COLLATE
#ifdef LC_ALL
        || category == LC_ALL
#endif
    )
    {
      char *newcoll;

#ifdef LC_ALL
      if (category == LC_ALL)
      {
        newcoll = setlocale(LC_COLLATE, NULL);
      }
      else
#endif
        newcoll = RETVAL;
      new_collate(newcoll);
    }
#endif                         

#ifdef USE_LOCALE_NUMERIC
    if (category == LC_NUMERIC
#ifdef LC_ALL
        || category == LC_ALL
#endif
    )
    {
      char *newnum;

#ifdef LC_ALL
      if (category == LC_ALL)
      {
        newnum = setlocale(LC_NUMERIC, NULL);
      }
      else
#endif
        newnum = RETVAL;
      new_numeric(newnum);
    }
#endif                         
  }

  return RETVAL;
}
#endif                                                  
