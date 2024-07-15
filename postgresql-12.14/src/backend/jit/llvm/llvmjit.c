                                                                            
   
             
                                         
   
                                                                
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

#include "jit/llvmjit.h"
#include "jit/llvmjit_emit.h"

#include "miscadmin.h"

#include "utils/memutils.h"
#include "utils/resowner_private.h"
#include "portability/instr_time.h"
#include "storage/ipc.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/BitReader.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#if LLVM_VERSION_MAJOR > 11
#include <llvm-c/Orc.h>
#include <llvm-c/OrcEE.h>
#include <llvm-c/LLJIT.h>
#else
#include <llvm-c/OrcBindings.h>
#endif
#include <llvm-c/Support.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Transforms/Scalar.h>
#if LLVM_VERSION_MAJOR > 6
#include <llvm-c/Transforms/Utils.h>
#endif

                                            
typedef struct LLVMJitHandle
{
#if LLVM_VERSION_MAJOR > 11
  LLVMOrcLLJITRef lljit;
  LLVMOrcResourceTrackerRef resource_tracker;
#else
  LLVMOrcJITStackRef stack;
  LLVMOrcModuleHandle orc_handle;
#endif
} LLVMJitHandle;

                                                  
LLVMTypeRef TypeSizeT;
LLVMTypeRef TypeParamBool;
LLVMTypeRef TypeStorageBool;
LLVMTypeRef TypePGFunction;
LLVMTypeRef StructNullableDatum;
LLVMTypeRef StructHeapTupleFieldsField3;
LLVMTypeRef StructHeapTupleFields;
LLVMTypeRef StructHeapTupleHeaderData;
LLVMTypeRef StructHeapTupleDataChoice;
LLVMTypeRef StructHeapTupleData;
LLVMTypeRef StructMinimalTupleData;
LLVMTypeRef StructItemPointerData;
LLVMTypeRef StructBlockId;
LLVMTypeRef StructFormPgAttribute;
LLVMTypeRef StructTupleConstr;
LLVMTypeRef StructTupleDescData;
LLVMTypeRef StructTupleTableSlot;
LLVMTypeRef StructHeapTupleTableSlot;
LLVMTypeRef StructMinimalTupleTableSlot;
LLVMTypeRef StructMemoryContextData;
LLVMTypeRef StructPGFinfoRecord;
LLVMTypeRef StructFmgrInfo;
LLVMTypeRef StructFunctionCallInfoData;
LLVMTypeRef StructExprContext;
LLVMTypeRef StructExprEvalStep;
LLVMTypeRef StructExprState;
LLVMTypeRef StructAggState;
LLVMTypeRef StructAggStatePerGroupData;
LLVMTypeRef StructAggStatePerTransData;

LLVMValueRef AttributeTemplate;
LLVMValueRef FuncStrlen;
LLVMValueRef FuncVarsizeAny;
LLVMValueRef FuncSlotGetsomeattrsInt;
LLVMValueRef FuncSlotGetmissingattrs;
LLVMValueRef FuncMakeExpandedObjectReadOnlyInternal;
LLVMValueRef FuncExecEvalSubscriptingRef;
LLVMValueRef FuncExecEvalSysVar;
LLVMValueRef FuncExecAggTransReparent;
LLVMValueRef FuncExecAggInitGroup;

static bool llvm_session_initialized = false;
static size_t llvm_generation = 0;
static const char *llvm_triple = NULL;
static const char *llvm_layout = NULL;

static LLVMTargetRef llvm_targetref;
#if LLVM_VERSION_MAJOR > 11
static LLVMOrcThreadSafeContextRef llvm_ts_context;
static LLVMOrcLLJITRef llvm_opt0_orc;
static LLVMOrcLLJITRef llvm_opt3_orc;
#else                               
static LLVMOrcJITStackRef llvm_opt0_orc;
static LLVMOrcJITStackRef llvm_opt3_orc;
#endif                              

static void
llvm_release_context(JitContext *context);
static void
llvm_session_initialize(void);
static void
llvm_shutdown(int code, Datum arg);
static void
llvm_compile_module(LLVMJitContext *context);
static void
llvm_optimize_module(LLVMJitContext *context, LLVMModuleRef module);

static void
llvm_create_types(void);
static uint64_t
llvm_resolve_symbol(const char *name, void *ctx);

#if LLVM_VERSION_MAJOR > 11
static LLVMOrcLLJITRef
llvm_create_jit_instance(LLVMTargetMachineRef tm);
static char *
llvm_error_message(LLVMErrorRef error);
#endif                              

PG_MODULE_MAGIC;

   
                                 
   
void
_PG_jit_provider_init(JitProviderCallbacks *cb)
{
  cb->reset_after_error = llvm_reset_after_error;
  cb->release_context = llvm_release_context;
  cb->compile_expr = llvm_compile_expr;
}

   
                                     
   
                                                                               
                                                               
                                                                         
   
LLVMJitContext *
llvm_create_context(int jitFlags)
{
  LLVMJitContext *context;

  llvm_assert_in_fatal_section();

  llvm_session_initialize();

  ResourceOwnerEnlargeJIT(CurrentResourceOwner);

  context = MemoryContextAllocZero(TopMemoryContext, sizeof(LLVMJitContext));
  context->base.flags = jitFlags;

                      
  context->base.resowner = CurrentResourceOwner;
  ResourceOwnerRememberJIT(CurrentResourceOwner, PointerGetDatum(context));

  return context;
}

   
                                                   
   
static void
llvm_release_context(JitContext *context)
{
  LLVMJitContext *llvm_context = (LLVMJitContext *)context;

     
                                                                          
                                                                            
                                                               
     
  if (proc_exit_inprogress)
  {
    return;
  }

  llvm_enter_fatal_on_oom();

  if (llvm_context->module)
  {
    LLVMDisposeModule(llvm_context->module);
    llvm_context->module = NULL;
  }

  while (llvm_context->handles != NIL)
  {
    LLVMJitHandle *jit_handle;

    jit_handle = (LLVMJitHandle *)linitial(llvm_context->handles);
    llvm_context->handles = list_delete_first(llvm_context->handles);

#if LLVM_VERSION_MAJOR > 11
    {
      LLVMOrcExecutionSessionRef ee;
      LLVMOrcSymbolStringPoolRef sp;

      LLVMOrcResourceTrackerRemove(jit_handle->resource_tracker);
      LLVMOrcReleaseResourceTracker(jit_handle->resource_tracker);

         
                                                                  
                                                                      
                                                                       
                
         
      ee = LLVMOrcLLJITGetExecutionSession(jit_handle->lljit);
      sp = LLVMOrcExecutionSessionGetSymbolStringPool(ee);
      LLVMOrcSymbolStringPoolClearDeadEntries(sp);
    }
#else                               
    {
      LLVMOrcRemoveModule(jit_handle->stack, jit_handle->orc_handle);
    }
#endif                              

    pfree(jit_handle);
  }
}

   
                                                                        
   
LLVMModuleRef
llvm_mutable_module(LLVMJitContext *context)
{
  llvm_assert_in_fatal_section();

     
                                                         
     
  if (!context->module)
  {
    context->compiled = false;
    context->module_generation = llvm_generation++;
    context->module = LLVMModuleCreateWithName("pg");
    LLVMSetTarget(context->module, llvm_triple);
    LLVMSetDataLayout(context->module, llvm_layout);
  }

  return context->module;
}

   
                                                                           
                                                                               
             
   
char *
llvm_expand_funcname(struct LLVMJitContext *context, const char *basename)
{
  Assert(context->module != NULL);

  context->base.instr.created_functions++;

     
                                                                         
                                             
     
  return psprintf("%s_%zu_%d", basename, context->module_generation, context->counter++);
}

   
                                                                               
                                                  
   
void *
llvm_get_function(LLVMJitContext *context, const char *funcname)
{
#if LLVM_VERSION_MAJOR > 11 || defined(HAVE_DECL_LLVMORCGETSYMBOLADDRESSIN) && HAVE_DECL_LLVMORCGETSYMBOLADDRESSIN
  ListCell *lc;
#endif

  llvm_assert_in_fatal_section();

     
                                                                       
                                                         
     
  if (!context->compiled)
  {
    llvm_compile_module(context);
  }

     
                                                                           
                     
     

#if LLVM_VERSION_MAJOR > 11
  foreach (lc, context->handles)
  {
    LLVMJitHandle *handle = (LLVMJitHandle *)lfirst(lc);
    instr_time starttime;
    instr_time endtime;
    LLVMErrorRef error;
    LLVMOrcJITTargetAddress addr;

    INSTR_TIME_SET_CURRENT(starttime);

    addr = 0;
    error = LLVMOrcLLJITLookup(handle->lljit, &addr, funcname);
    if (error)
    {
      elog(ERROR, "failed to look up symbol \"%s\": %s", funcname, llvm_error_message(error));
    }

       
                                                                 
                                                                          
                                                                      
               
       
    INSTR_TIME_SET_CURRENT(endtime);
    INSTR_TIME_ACCUM_DIFF(context->base.instr.emission_counter, endtime, starttime);

    if (addr)
    {
      return (void *)(uintptr_t)addr;
    }
  }
#elif defined(HAVE_DECL_LLVMORCGETSYMBOLADDRESSIN) && HAVE_DECL_LLVMORCGETSYMBOLADDRESSIN
  foreach (lc, context->handles)
  {
    LLVMOrcTargetAddress addr;
    LLVMJitHandle *handle = (LLVMJitHandle *)lfirst(lc);

    addr = 0;
    if (LLVMOrcGetSymbolAddressIn(handle->stack, &addr, handle->orc_handle, funcname))
    {
      elog(ERROR, "failed to look up symbol \"%s\"", funcname);
    }
    if (addr)
    {
      return (void *)(uintptr_t)addr;
    }
  }
#elif LLVM_VERSION_MAJOR < 5
  {
    LLVMOrcTargetAddress addr;

    if ((addr = LLVMOrcGetSymbolAddress(llvm_opt0_orc, funcname)))
    {
      return (void *)(uintptr_t)addr;
    }
    if ((addr = LLVMOrcGetSymbolAddress(llvm_opt3_orc, funcname)))
    {
      return (void *)(uintptr_t)addr;
    }
  }
#else
  {
    LLVMOrcTargetAddress addr;

    if (LLVMOrcGetSymbolAddress(llvm_opt0_orc, &addr, funcname))
    {
      elog(ERROR, "failed to look up symbol \"%s\"", funcname);
    }
    if (addr)
    {
      return (void *)(uintptr_t)addr;
    }
    if (LLVMOrcGetSymbolAddress(llvm_opt3_orc, &addr, funcname))
    {
      elog(ERROR, "failed to look up symbol \"%s\"", funcname);
    }
    if (addr)
    {
      return (void *)(uintptr_t)addr;
    }
  }
#endif

  elog(ERROR, "failed to JIT: %s", funcname);

  return NULL;
}

   
                                                                      
              
   
                                                                               
                                            
   
LLVMValueRef
llvm_get_decl(LLVMModuleRef mod, LLVMValueRef v_src)
{
  LLVMValueRef v_fn;

                                     
  v_fn = LLVMGetNamedFunction(mod, LLVMGetValueName(v_src));
  if (v_fn)
  {
    return v_fn;
  }

  v_fn = LLVMAddFunction(mod, LLVMGetValueName(v_src), LLVMGetElementType(LLVMTypeOf(v_src)));
  llvm_copy_attributes(v_src, v_fn);

  return v_fn;
}

   
                                                                          
                                                                         
   
static void
llvm_copy_attributes_at_index(LLVMValueRef v_from, LLVMValueRef v_to, uint32 index)
{
  int num_attributes;
  LLVMAttributeRef *attrs;

  num_attributes = LLVMGetAttributeCountAtIndexPG(v_from, index);

     
                                                       
                                                                          
     
  if (num_attributes == 0)
  {
    return;
  }

  attrs = palloc(sizeof(LLVMAttributeRef) * num_attributes);
  LLVMGetAttributesAtIndex(v_from, index, attrs);

  for (int attno = 0; attno < num_attributes; attno++)
  {
    LLVMAddAttributeAtIndex(v_to, index, attrs[attno]);
  }

  pfree(attrs);
}

   
                                                                               
                              
   
void
llvm_copy_attributes(LLVMValueRef v_from, LLVMValueRef v_to)
{
  uint32 param_count;

                                
  llvm_copy_attributes_at_index(v_from, v_to, LLVMAttributeFunctionIndex);

                                       
  llvm_copy_attributes_at_index(v_from, v_to, LLVMAttributeReturnIndex);

                                               
  param_count = LLVMCountParams(v_from);

  for (int paramidx = 1; paramidx <= param_count; paramidx++)
  {
    llvm_copy_attributes_at_index(v_from, v_to, paramidx);
  }
}

   
                                              
   
LLVMValueRef
llvm_function_reference(LLVMJitContext *context, LLVMBuilderRef builder, LLVMModuleRef mod, FunctionCallInfo fcinfo)
{
  char *modname;
  char *basename;
  char *funcname;

  LLVMValueRef v_fn;

  fmgr_symbol(fcinfo->flinfo->fn_oid, &modname, &basename);

  if (modname != NULL && basename != NULL)
  {
                                               
    funcname = psprintf("pgextern.%s.%s", modname, basename);
  }
  else if (basename != NULL)
  {
                           
    funcname = psprintf("%s", basename);
  }
  else
  {
       
                                                                     
                                                                        
                               
       
    LLVMValueRef v_fn_addr;

    funcname = psprintf("pgoidextern.%u", fcinfo->flinfo->fn_oid);
    v_fn = LLVMGetNamedGlobal(mod, funcname);
    if (v_fn != 0)
    {
      return LLVMBuildLoad(builder, v_fn, "");
    }

    v_fn_addr = l_ptr_const(fcinfo->flinfo->fn_addr, TypePGFunction);

    v_fn = LLVMAddGlobal(mod, TypePGFunction, funcname);
    LLVMSetInitializer(v_fn, v_fn_addr);
    LLVMSetGlobalConstant(v_fn, true);
    LLVMSetLinkage(v_fn, LLVMPrivateLinkage);
    LLVMSetUnnamedAddr(v_fn, true);

    return LLVMBuildLoad(builder, v_fn, "");
  }

                                                
  v_fn = LLVMGetNamedFunction(mod, funcname);
  if (v_fn != 0)
  {
    return v_fn;
  }

  v_fn = LLVMAddFunction(mod, funcname, LLVMGetElementType(TypePGFunction));

  return v_fn;
}

   
                                                           
   
static void
llvm_optimize_module(LLVMJitContext *context, LLVMModuleRef module)
{
  LLVMPassManagerBuilderRef llvm_pmb;
  LLVMPassManagerRef llvm_mpm;
  LLVMPassManagerRef llvm_fpm;
  LLVMValueRef func;
  int compile_optlevel;

  if (context->base.flags & PGJIT_OPT3)
  {
    compile_optlevel = 3;
  }
  else
  {
    compile_optlevel = 0;
  }

     
                                                                          
                                                                             
                                       
     
  llvm_pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(llvm_pmb, compile_optlevel);
  llvm_fpm = LLVMCreateFunctionPassManagerForModule(module);

  if (context->base.flags & PGJIT_OPT3)
  {
                                                     
    LLVMPassManagerBuilderUseInlinerWithThreshold(llvm_pmb, 512);
  }
  else
  {
                                                                 
    LLVMAddPromoteMemoryToRegisterPass(llvm_fpm);
  }

  LLVMPassManagerBuilderPopulateFunctionPassManager(llvm_pmb, llvm_fpm);

     
                                                                            
                                                          
     
  LLVMInitializeFunctionPassManager(llvm_fpm);
  for (func = LLVMGetFirstFunction(context->module); func != NULL; func = LLVMGetNextFunction(func))
  {
    LLVMRunFunctionPassManager(llvm_fpm, func);
  }
  LLVMFinalizeFunctionPassManager(llvm_fpm);
  LLVMDisposePassManager(llvm_fpm);

     
                                                                           
                                                                          
     
  llvm_mpm = LLVMCreatePassManager();
  LLVMPassManagerBuilderPopulateModulePassManager(llvm_pmb, llvm_mpm);
                                      
  if (!(context->base.flags & PGJIT_OPT3))
  {
    LLVMAddAlwaysInlinerPass(llvm_mpm);
  }
                                                                           
  if (context->base.flags & PGJIT_INLINE && !(context->base.flags & PGJIT_OPT3))
  {
    LLVMAddFunctionInliningPass(llvm_mpm);
  }
  LLVMRunPassManager(llvm_mpm, context->module);
  LLVMDisposePassManager(llvm_mpm);

  LLVMPassManagerBuilderDispose(llvm_pmb);
}

   
                                               
   
static void
llvm_compile_module(LLVMJitContext *context)
{
  LLVMJitHandle *handle;
  MemoryContext oldcontext;
  instr_time starttime;
  instr_time endtime;
#if LLVM_VERSION_MAJOR > 11
  LLVMOrcLLJITRef compile_orc;
#else
  LLVMOrcJITStackRef compile_orc;
#endif

  if (context->base.flags & PGJIT_OPT3)
  {
    compile_orc = llvm_opt3_orc;
  }
  else
  {
    compile_orc = llvm_opt0_orc;
  }

                        
  if (context->base.flags & PGJIT_INLINE)
  {
    INSTR_TIME_SET_CURRENT(starttime);
    llvm_inline(context->module);
    INSTR_TIME_SET_CURRENT(endtime);
    INSTR_TIME_ACCUM_DIFF(context->base.instr.inlining_counter, endtime, starttime);
  }

  if (jit_dump_bitcode)
  {
    char *filename;

    filename = psprintf("%u.%zu.bc", MyProcPid, context->module_generation);
    LLVMWriteBitcodeToFile(context->module, filename);
    pfree(filename);
  }

                                                              
  INSTR_TIME_SET_CURRENT(starttime);
  llvm_optimize_module(context, context->module);
  INSTR_TIME_SET_CURRENT(endtime);
  INSTR_TIME_ACCUM_DIFF(context->base.instr.optimization_counter, endtime, starttime);

  if (jit_dump_bitcode)
  {
    char *filename;

    filename = psprintf("%u.%zu.optimized.bc", MyProcPid, context->module_generation);
    LLVMWriteBitcodeToFile(context->module, filename);
    pfree(filename);
  }

  handle = (LLVMJitHandle *)MemoryContextAlloc(TopMemoryContext, sizeof(LLVMJitHandle));

     
                                                                      
                                                                             
                                                                        
                                                     
     
  INSTR_TIME_SET_CURRENT(starttime);
#if LLVM_VERSION_MAJOR > 11
  {
    LLVMOrcThreadSafeModuleRef ts_module;
    LLVMErrorRef error;
    LLVMOrcJITDylibRef jd = LLVMOrcLLJITGetMainJITDylib(compile_orc);

    ts_module = LLVMOrcCreateNewThreadSafeModule(context->module, llvm_ts_context);

    handle->lljit = compile_orc;
    handle->resource_tracker = LLVMOrcJITDylibCreateResourceTracker(jd);

       
                                                                          
                                                                     
                                                            
       

    context->module = NULL;                             
    error = LLVMOrcLLJITAddLLVMIRModuleWithRT(compile_orc, handle->resource_tracker, ts_module);

    if (error)
    {
      elog(ERROR, "failed to JIT module: %s", llvm_error_message(error));
    }

    handle->lljit = compile_orc;

                                                                         
  }
#elif LLVM_VERSION_MAJOR > 6
  {
    handle->stack = compile_orc;
    if (LLVMOrcAddEagerlyCompiledIR(compile_orc, &handle->orc_handle, context->module, llvm_resolve_symbol, NULL))
    {
      elog(ERROR, "failed to JIT module");
    }

                                                                   
  }
#elif LLVM_VERSION_MAJOR > 4
  {
    LLVMSharedModuleRef smod;

    smod = LLVMOrcMakeSharedModule(context->module);
    handle->stack = compile_orc;
    if (LLVMOrcAddEagerlyCompiledIR(compile_orc, &handle->orc_handle, smod, llvm_resolve_symbol, NULL))
    {
      elog(ERROR, "failed to JIT module");
    }

    LLVMOrcDisposeSharedModuleRef(smod);
  }
#else                       
  {
    handle->stack = compile_orc;
    handle->orc_handle = LLVMOrcAddEagerlyCompiledIR(compile_orc, context->module, llvm_resolve_symbol, NULL);

    LLVMDisposeModule(context->module);
  }
#endif

  INSTR_TIME_SET_CURRENT(endtime);
  INSTR_TIME_ACCUM_DIFF(context->base.instr.emission_counter, endtime, starttime);

  context->module = NULL;
  context->compiled = true;

                                                     
  oldcontext = MemoryContextSwitchTo(TopMemoryContext);
  context->handles = lappend(context->handles, handle);
  MemoryContextSwitchTo(oldcontext);

  ereport(DEBUG1, (errmsg("time to inline: %.3fs, opt: %.3fs, emit: %.3fs", INSTR_TIME_GET_DOUBLE(context->base.instr.inlining_counter), INSTR_TIME_GET_DOUBLE(context->base.instr.optimization_counter), INSTR_TIME_GET_DOUBLE(context->base.instr.emission_counter)), errhidestmt(true), errhidecontext(true)));
}

   
                               
   
static void
llvm_session_initialize(void)
{
  MemoryContext oldcontext;
  char *error = NULL;
  char *cpu = NULL;
  char *features = NULL;
  LLVMTargetMachineRef opt0_tm;
  LLVMTargetMachineRef opt3_tm;

  if (llvm_session_initialized)
  {
    return;
  }

  oldcontext = MemoryContextSwitchTo(TopMemoryContext);

  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

     
                                                                    
                                                                            
                                                                              
                                                      
     
#if LLVM_VERSION_MAJOR > 14
  LLVMContextSetOpaquePointers(LLVMGetGlobalContext(), false);
#endif

     
                                                                         
             
     
  llvm_create_types();

  if (LLVMGetTargetFromTriple(llvm_triple, &llvm_targetref, &error) != 0)
  {
    elog(FATAL, "failed to query triple %s", error);
  }

     
                                                                         
                                                                          
                                                                         
                                              
     
  cpu = LLVMGetHostCPUName();
  features = LLVMGetHostCPUFeatures();
  elog(DEBUG2, "LLVMJIT detected CPU \"%s\", with features \"%s\"", cpu, features);

  opt0_tm = LLVMCreateTargetMachine(llvm_targetref, llvm_triple, cpu, features, LLVMCodeGenLevelNone, LLVMRelocDefault, LLVMCodeModelJITDefault);
  opt3_tm = LLVMCreateTargetMachine(llvm_targetref, llvm_triple, cpu, features, LLVMCodeGenLevelAggressive, LLVMRelocDefault, LLVMCodeModelJITDefault);

  LLVMDisposeMessage(cpu);
  cpu = NULL;
  LLVMDisposeMessage(features);
  features = NULL;

                                                 
  LLVMLoadLibraryPermanently(NULL);

#if LLVM_VERSION_MAJOR > 11
  {
    llvm_ts_context = LLVMOrcCreateNewThreadSafeContext();

    llvm_opt0_orc = llvm_create_jit_instance(opt0_tm);
    opt0_tm = 0;

    llvm_opt3_orc = llvm_create_jit_instance(opt3_tm);
    opt3_tm = 0;
  }
#else                              
  {
    llvm_opt0_orc = LLVMOrcCreateInstance(opt0_tm);
    llvm_opt3_orc = LLVMOrcCreateInstance(opt3_tm);

#if defined(HAVE_DECL_LLVMCREATEGDBREGISTRATIONLISTENER) && HAVE_DECL_LLVMCREATEGDBREGISTRATIONLISTENER
    if (jit_debugging_support)
    {
      LLVMJITEventListenerRef l = LLVMCreateGDBRegistrationListener();

      LLVMOrcRegisterJITEventListener(llvm_opt0_orc, l);
      LLVMOrcRegisterJITEventListener(llvm_opt3_orc, l);
    }
#endif
#if defined(HAVE_DECL_LLVMCREATEPERFJITEVENTLISTENER) && HAVE_DECL_LLVMCREATEPERFJITEVENTLISTENER
    if (jit_profiling_support)
    {
      LLVMJITEventListenerRef l = LLVMCreatePerfJITEventListener();

      LLVMOrcRegisterJITEventListener(llvm_opt0_orc, l);
      LLVMOrcRegisterJITEventListener(llvm_opt3_orc, l);
    }
#endif
  }
#endif                              

  before_shmem_exit(llvm_shutdown, 0);

  llvm_session_initialized = true;

  MemoryContextSwitchTo(oldcontext);
}

static void
llvm_shutdown(int code, Datum arg)
{
     
                                                                            
                                                                          
                                                        
     
                                                                    
                                               
     
  if (llvm_in_fatal_on_oom())
  {
    Assert(proc_exit_inprogress);
    return;
  }

#if LLVM_VERSION_MAJOR > 11
  {
    if (llvm_opt3_orc)
    {
      LLVMOrcDisposeLLJIT(llvm_opt3_orc);
      llvm_opt3_orc = NULL;
    }
    if (llvm_opt0_orc)
    {
      LLVMOrcDisposeLLJIT(llvm_opt0_orc);
      llvm_opt0_orc = NULL;
    }
    if (llvm_ts_context)
    {
      LLVMOrcDisposeThreadSafeContext(llvm_ts_context);
      llvm_ts_context = NULL;
    }
  }
#else                              
  {
                                                                        

    if (llvm_opt3_orc)
    {
#if defined(HAVE_DECL_LLVMORCREGISTERPERF) && HAVE_DECL_LLVMORCREGISTERPERF
      if (jit_profiling_support)
      {
        LLVMOrcUnregisterPerf(llvm_opt3_orc);
      }
#endif
      LLVMOrcDisposeInstance(llvm_opt3_orc);
      llvm_opt3_orc = NULL;
    }

    if (llvm_opt0_orc)
    {
#if defined(HAVE_DECL_LLVMORCREGISTERPERF) && HAVE_DECL_LLVMORCREGISTERPERF
      if (jit_profiling_support)
      {
        LLVMOrcUnregisterPerf(llvm_opt0_orc);
      }
#endif
      LLVMOrcDisposeInstance(llvm_opt0_orc);
      llvm_opt0_orc = NULL;
    }
  }
#endif                              
}

                                                                 
static LLVMTypeRef
load_type(LLVMModuleRef mod, const char *name)
{
  LLVMValueRef value;
  LLVMTypeRef typ;

                                                
  value = LLVMGetNamedGlobal(mod, name);
  if (!value)
  {
    elog(ERROR, "type %s is unknown", name);
  }

                                                            
  typ = LLVMTypeOf(value);
  Assert(typ != NULL);
  typ = LLVMGetElementType(typ);
  Assert(typ != NULL);
  return typ;
}

                                                                      
static LLVMTypeRef
load_return_type(LLVMModuleRef mod, const char *name)
{
  LLVMValueRef value;
  LLVMTypeRef typ;

                                                  
  value = LLVMGetNamedFunction(mod, name);
  if (!value)
  {
    elog(ERROR, "function %s is unknown", name);
  }

                                    
  typ = LLVMTypeOf(value);
  Assert(typ != NULL);
                           
  typ = LLVMGetElementType(typ);
  Assert(typ != NULL);
                               
  typ = LLVMGetReturnType(typ);
  Assert(typ != NULL);

  return typ;
}

   
                                                                              
                                                
   
                                                             
   
static void
llvm_create_types(void)
{
  char path[MAXPGPATH];
  LLVMMemoryBufferRef buf;
  char *msg;
  LLVMModuleRef mod = NULL;

  snprintf(path, MAXPGPATH, "%s/%s", pkglib_path, "llvmjit_types.bc");

                 
  if (LLVMCreateMemoryBufferWithContentsOfFile(path, &buf, &msg))
  {
    elog(ERROR, "LLVMCreateMemoryBufferWithContentsOfFile(%s) failed: %s", path, msg);
  }

                                                   
  if (LLVMParseBitcode2(buf, &mod))
  {
    elog(ERROR, "LLVMParseBitcode2 of %s failed", path);
  }
  LLVMDisposeMemoryBuffer(buf);

     
                                                                            
                 
     
  llvm_triple = pstrdup(LLVMGetTarget(mod));
  llvm_layout = pstrdup(LLVMGetDataLayoutStr(mod));

  TypeSizeT = load_type(mod, "TypeSizeT");
  TypeParamBool = load_return_type(mod, "FunctionReturningBool");
  TypeStorageBool = load_type(mod, "TypeStorageBool");
  TypePGFunction = load_type(mod, "TypePGFunction");
  StructNullableDatum = load_type(mod, "StructNullableDatum");
  StructExprContext = load_type(mod, "StructExprContext");
  StructExprEvalStep = load_type(mod, "StructExprEvalStep");
  StructExprState = load_type(mod, "StructExprState");
  StructFunctionCallInfoData = load_type(mod, "StructFunctionCallInfoData");
  StructMemoryContextData = load_type(mod, "StructMemoryContextData");
  StructTupleTableSlot = load_type(mod, "StructTupleTableSlot");
  StructHeapTupleTableSlot = load_type(mod, "StructHeapTupleTableSlot");
  StructMinimalTupleTableSlot = load_type(mod, "StructMinimalTupleTableSlot");
  StructHeapTupleData = load_type(mod, "StructHeapTupleData");
  StructTupleDescData = load_type(mod, "StructTupleDescData");
  StructAggState = load_type(mod, "StructAggState");
  StructAggStatePerGroupData = load_type(mod, "StructAggStatePerGroupData");
  StructAggStatePerTransData = load_type(mod, "StructAggStatePerTransData");

  AttributeTemplate = LLVMGetNamedFunction(mod, "AttributeTemplate");
  FuncStrlen = LLVMGetNamedFunction(mod, "strlen");
  FuncVarsizeAny = LLVMGetNamedFunction(mod, "varsize_any");
  FuncSlotGetsomeattrsInt = LLVMGetNamedFunction(mod, "slot_getsomeattrs_int");
  FuncSlotGetmissingattrs = LLVMGetNamedFunction(mod, "slot_getmissingattrs");
  FuncMakeExpandedObjectReadOnlyInternal = LLVMGetNamedFunction(mod, "MakeExpandedObjectReadOnlyInternal");
  FuncExecEvalSubscriptingRef = LLVMGetNamedFunction(mod, "ExecEvalSubscriptingRef");
  FuncExecEvalSysVar = LLVMGetNamedFunction(mod, "ExecEvalSysVar");
  FuncExecAggTransReparent = LLVMGetNamedFunction(mod, "ExecAggTransReparent");
  FuncExecAggInitGroup = LLVMGetNamedFunction(mod, "ExecAggInitGroup");

     
                                                                       
               
     

  return;
}

   
                                                                           
                                                               
   
void
llvm_split_symbol_name(const char *name, char **modname, char **funcname)
{
  *modname = NULL;
  *funcname = NULL;

     
                                                          
     
  if (strncmp(name, "pgextern.", strlen("pgextern.")) == 0)
  {
       
                                                                        
                                         
       
    *funcname = rindex(name, '.');
    (*funcname)++;                  

    *modname = pnstrdup(name + strlen("pgextern."), *funcname - name - strlen("pgextern.") - 1);
    Assert(funcname);

    *funcname = pstrdup(*funcname);
  }
  else
  {
    *modname = NULL;
    *funcname = pstrdup(name);
  }
}

   
                                                                  
   
static uint64_t
llvm_resolve_symbol(const char *symname, void *ctx)
{
  uintptr_t addr;
  char *funcname;
  char *modname;

     
                                                                             
                                                    
     
#if defined(__darwin__)
  if (symname[0] != '_')
  {
    elog(ERROR, "expected prefixed symbol name, but got \"%s\"", symname);
  }
  symname++;
#endif

  llvm_split_symbol_name(symname, &modname, &funcname);

                                                                       
  Assert(funcname);

  if (modname)
  {
    addr = (uintptr_t)load_external_function(modname, funcname, true, NULL);
  }
  else
  {
    addr = (uintptr_t)LLVMSearchForAddressOfSymbol(symname);
  }

  pfree(funcname);
  if (modname)
  {
    pfree(modname);
  }

                                                     
  if (!addr)
  {
    elog(WARNING, "failed to resolve name %s", symname);
  }

  return (uint64_t)addr;
}

#if LLVM_VERSION_MAJOR > 11

static LLVMErrorRef
llvm_resolve_symbols(LLVMOrcDefinitionGeneratorRef GeneratorObj, void *Ctx, LLVMOrcLookupStateRef *LookupState, LLVMOrcLookupKind Kind, LLVMOrcJITDylibRef JD, LLVMOrcJITDylibLookupFlags JDLookupFlags, LLVMOrcCLookupSet LookupSet, size_t LookupSetSize)
{
#if LLVM_VERSION_MAJOR > 14
  LLVMOrcCSymbolMapPairs symbols = palloc0(sizeof(LLVMOrcCSymbolMapPair) * LookupSetSize);
#else
  LLVMOrcCSymbolMapPairs symbols = palloc0(sizeof(LLVMJITCSymbolMapPair) * LookupSetSize);
#endif
  LLVMErrorRef error;
  LLVMOrcMaterializationUnitRef mu;

  for (int i = 0; i < LookupSetSize; i++)
  {
    const char *name = LLVMOrcSymbolStringPoolEntryStr(LookupSet[i].Name);

#if LLVM_VERSION_MAJOR > 12
    LLVMOrcRetainSymbolStringPoolEntry(LookupSet[i].Name);
#endif
    symbols[i].Name = LookupSet[i].Name;
    symbols[i].Sym.Address = llvm_resolve_symbol(name, NULL);
    symbols[i].Sym.Flags.GenericFlags = LLVMJITSymbolGenericFlagsExported;
  }

  mu = LLVMOrcAbsoluteSymbols(symbols, LookupSetSize);
  error = LLVMOrcJITDylibDefine(JD, mu);
  if (error != LLVMErrorSuccess)
  {
    LLVMOrcDisposeMaterializationUnit(mu);
  }

  pfree(symbols);

  return error;
}

   
                                                                              
                                                                             
                                                                       
                                       
   
                                                                         
                                                                    
   
static void
llvm_log_jit_error(void *ctx, LLVMErrorRef error)
{
  elog(WARNING, "error during JITing: %s", llvm_error_message(error));
}

   
                                                               
   
static LLVMOrcObjectLayerRef
llvm_create_object_layer(void *Ctx, LLVMOrcExecutionSessionRef ES, const char *Triple)
{
  LLVMOrcObjectLayerRef objlayer = LLVMOrcCreateRTDyldObjectLinkingLayerWithSectionMemoryManager(ES);

#if defined(HAVE_DECL_LLVMCREATEGDBREGISTRATIONLISTENER) && HAVE_DECL_LLVMCREATEGDBREGISTRATIONLISTENER
  if (jit_debugging_support)
  {
    LLVMJITEventListenerRef l = LLVMCreateGDBRegistrationListener();

    LLVMOrcRTDyldObjectLinkingLayerRegisterJITEventListener(objlayer, l);
  }
#endif

#if defined(HAVE_DECL_LLVMCREATEPERFJITEVENTLISTENER) && HAVE_DECL_LLVMCREATEPERFJITEVENTLISTENER
  if (jit_profiling_support)
  {
    LLVMJITEventListenerRef l = LLVMCreatePerfJITEventListener();

    LLVMOrcRTDyldObjectLinkingLayerRegisterJITEventListener(objlayer, l);
  }
#endif

  return objlayer;
}

   
                                                                            
                                                             
   
static LLVMOrcLLJITRef
llvm_create_jit_instance(LLVMTargetMachineRef tm)
{
  LLVMOrcLLJITRef lljit;
  LLVMOrcJITTargetMachineBuilderRef tm_builder;
  LLVMOrcLLJITBuilderRef lljit_builder;
  LLVMErrorRef error;
  LLVMOrcDefinitionGeneratorRef main_gen;
  LLVMOrcDefinitionGeneratorRef ref_gen;

  lljit_builder = LLVMOrcCreateLLJITBuilder();
  tm_builder = LLVMOrcJITTargetMachineBuilderCreateFromTargetMachine(tm);
  LLVMOrcLLJITBuilderSetJITTargetMachineBuilder(lljit_builder, tm_builder);

  LLVMOrcLLJITBuilderSetObjectLinkingLayerCreator(lljit_builder, llvm_create_object_layer, NULL);

  error = LLVMOrcCreateLLJIT(&lljit, lljit_builder);
  if (error)
  {
    elog(ERROR, "failed to create lljit instance: %s", llvm_error_message(error));
  }

  LLVMOrcExecutionSessionSetErrorReporter(LLVMOrcLLJITGetExecutionSession(lljit), llvm_log_jit_error, NULL);

     
                                                                    
                               
     
  error = LLVMOrcCreateDynamicLibrarySearchGeneratorForProcess(&main_gen, LLVMOrcLLJITGetGlobalPrefix(lljit), 0, NULL);
  if (error)
  {
    elog(ERROR, "failed to create generator: %s", llvm_error_message(error));
  }
  LLVMOrcJITDylibAddGenerator(LLVMOrcLLJITGetMainJITDylib(lljit), main_gen);

     
                                                                            
                            
     
#if LLVM_VERSION_MAJOR > 14
  ref_gen = LLVMOrcCreateCustomCAPIDefinitionGenerator(llvm_resolve_symbols, NULL, NULL);
#else
  ref_gen = LLVMOrcCreateCustomCAPIDefinitionGenerator(llvm_resolve_symbols, NULL);
#endif
  LLVMOrcJITDylibAddGenerator(LLVMOrcLLJITGetMainJITDylib(lljit), ref_gen);

  return lljit;
}

static char *
llvm_error_message(LLVMErrorRef error)
{
  char *orig = LLVMGetErrorMessage(error);
  char *msg = pstrdup(orig);

  LLVMDisposeErrorMessage(orig);

  return msg;
}

#endif                              
