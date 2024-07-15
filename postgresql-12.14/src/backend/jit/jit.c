                                                                            
   
         
                                              
   
                                                                               
                                                                          
                       
   
   
                                                                
   
                  
                           
   
                                                                            
   
#include "postgres.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fmgr.h"
#include "executor/execExpr.h"
#include "jit/jit.h"
#include "miscadmin.h"
#include "utils/resowner_private.h"
#include "utils/fmgrprotos.h"

          
bool jit_enabled = true;
char *jit_provider = NULL;
bool jit_debugging_support = false;
bool jit_dump_bitcode = false;
bool jit_expressions = true;
bool jit_profiling_support = false;
bool jit_tuple_deforming = true;
double jit_above_cost = 100000;
double jit_inline_above_cost = 500000;
double jit_optimize_above_cost = 500000;

static JitProviderCallbacks provider;
static bool provider_successfully_loaded = false;
static bool provider_failed_loading = false;

static bool
provider_init(void);
static bool
file_exists(const char *name);

   
                                                                        
                                                            
   
Datum
pg_jit_available(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(provider_init());
}

   
                                                                           
           
   
static bool
provider_init(void)
{
  char path[MAXPGPATH];
  JitProviderInit init;

                                             
  if (!jit_enabled)
  {
    return false;
  }

     
                                                                         
                  
     
  if (provider_failed_loading)
  {
    return false;
  }
  if (provider_successfully_loaded)
  {
    return true;
  }

     
                                                                           
                                                                           
                                                                 
     
  snprintf(path, MAXPGPATH, "%s/%s%s", pkglib_path, jit_provider, DLSUFFIX);
  elog(DEBUG1, "probing availability of JIT provider at %s", path);
  if (!file_exists(path))
  {
    elog(DEBUG1, "provider not available, disabling JIT for current session");
    provider_failed_loading = true;
    return false;
  }

     
                                                                  
                                                                         
                                                                         
                                                                       
                        
     
  provider_failed_loading = true;

                      
  init = (JitProviderInit)load_external_function(path, "_PG_jit_provider_init", true, NULL);
  init(&provider);

  provider_successfully_loaded = true;
  provider_failed_loading = false;

  elog(DEBUG1, "successfully loaded JIT provider in current session");

  return true;
}

   
                                                                             
                                                             
   
void
jit_reset_after_error(void)
{
  if (provider_successfully_loaded)
  {
    provider.reset_after_error();
  }
}

   
                                                  
   
void
jit_release_context(JitContext *context)
{
  if (provider_successfully_loaded)
  {
    provider.release_context(context);
  }

  ResourceOwnerForgetJIT(context->resowner, PointerGetDatum(context));
  pfree(context);
}

   
                                              
   
                                             
   
bool
jit_compile_expr(struct ExprState *state)
{
     
                                                                     
                                                                             
                                                                          
                                                                            
                                                                        
                                                                           
                                                                             
     
  if (!state->parent)
  {
    return false;
  }

                                                
  if (!(state->parent->state->es_jit_flags & PGJIT_PERFORM))
  {
    return false;
  }

                                      
  if (!(state->parent->state->es_jit_flags & PGJIT_EXPR))
  {
    return false;
  }

                                                 
  if (provider_init())
  {
    return provider.compile_expr(state);
  }

  return false;
}

                                               
void
InstrJitAgg(JitInstrumentation *dst, JitInstrumentation *add)
{
  dst->created_functions += add->created_functions;
  INSTR_TIME_ADD(dst->generation_counter, add->generation_counter);
  INSTR_TIME_ADD(dst->inlining_counter, add->inlining_counter);
  INSTR_TIME_ADD(dst->optimization_counter, add->optimization_counter);
  INSTR_TIME_ADD(dst->emission_counter, add->emission_counter);
}

static bool
file_exists(const char *name)
{
  struct stat st;

  AssertArg(name != NULL);

  if (stat(name, &st) == 0)
  {
    return S_ISDIR(st.st_mode) ? false : true;
  }
  else if (!(errno == ENOENT || errno == ENOTDIR))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not access file \"%s\": %m", name)));
  }

  return false;
}
