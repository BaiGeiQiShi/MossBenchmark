                                                                            
   
                  
                              
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/tupconvert.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_type.h"
#include "executor/execdebug.h"
#include "executor/nodeAgg.h"
#include "executor/nodeSubplan.h"
#include "executor/execExpr.h"
#include "funcapi.h"
#include "jit/llvmjit.h"
#include "jit/llvmjit_emit.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parsetree.h"
#include "pgstat.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/fmgrtab.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"
#include "utils/xml.h"

typedef struct CompiledExprState
{
  LLVMJitContext *context;
  const char *funcname;
} CompiledExprState;

static Datum
ExecRunCompiledExpr(ExprState *state, ExprContext *econtext, bool *isNull);

static LLVMValueRef
BuildV1Call(LLVMJitContext *context, LLVMBuilderRef b, LLVMModuleRef mod, FunctionCallInfo fcinfo, LLVMValueRef *v_fcinfo_isnull);
static void
build_EvalXFunc(LLVMBuilderRef b, LLVMModuleRef mod, const char *funcname, LLVMValueRef v_state, LLVMValueRef v_econtext, ExprEvalStep *op);
static LLVMValueRef
create_LifetimeEnd(LLVMModuleRef mod);

   
                           
   
bool
llvm_compile_expr(ExprState *state)
{
  PlanState *parent = state->parent;
  int i;
  char *funcname;

  LLVMJitContext *context = NULL;

  LLVMBuilderRef b;
  LLVMModuleRef mod;
  LLVMTypeRef eval_sig;
  LLVMValueRef eval_fn;
  LLVMBasicBlockRef entry;
  LLVMBasicBlockRef *opblocks;

                    
  LLVMValueRef v_state;
  LLVMValueRef v_econtext;

                   
  LLVMValueRef v_isnullp;

                         
  LLVMValueRef v_tmpvaluep;
  LLVMValueRef v_tmpisnullp;

             
  LLVMValueRef v_innerslot;
  LLVMValueRef v_outerslot;
  LLVMValueRef v_scanslot;
  LLVMValueRef v_resultslot;

                             
  LLVMValueRef v_innervalues;
  LLVMValueRef v_innernulls;
  LLVMValueRef v_outervalues;
  LLVMValueRef v_outernulls;
  LLVMValueRef v_scanvalues;
  LLVMValueRef v_scannulls;
  LLVMValueRef v_resultvalues;
  LLVMValueRef v_resultnulls;

                         
  LLVMValueRef v_aggvalues;
  LLVMValueRef v_aggnulls;

  instr_time starttime;
  instr_time endtime;

  llvm_enter_fatal_on_oom();

                                 
  if (parent && parent->state->es_jit)
  {
    context = (LLVMJitContext *)parent->state->es_jit;
  }
  else
  {
    context = llvm_create_context(parent->state->es_jit_flags);

    if (parent)
    {
      parent->state->es_jit = &context->base;
    }
  }

  INSTR_TIME_SET_CURRENT(starttime);

  mod = llvm_mutable_module(context);

  b = LLVMCreateBuilder();

  funcname = llvm_expand_funcname(context, "evalexpr");

                                         
  {
    LLVMTypeRef param_types[3];

    param_types[0] = l_ptr(StructExprState);              
    param_types[1] = l_ptr(StructExprContext);               
    param_types[2] = l_ptr(TypeStorageBool);               

    eval_sig = LLVMFunctionType(TypeSizeT, param_types, lengthof(param_types), false);
  }
  eval_fn = LLVMAddFunction(mod, funcname, eval_sig);
  LLVMSetLinkage(eval_fn, LLVMExternalLinkage);
  LLVMSetVisibility(eval_fn, LLVMDefaultVisibility);
  llvm_copy_attributes(AttributeTemplate, eval_fn);

  entry = LLVMAppendBasicBlock(eval_fn, "entry");

                   
  v_state = LLVMGetParam(eval_fn, 0);
  v_econtext = LLVMGetParam(eval_fn, 1);
  v_isnullp = LLVMGetParam(eval_fn, 2);

  LLVMPositionBuilderAtEnd(b, entry);

  v_tmpvaluep = LLVMBuildStructGEP(b, v_state, FIELDNO_EXPRSTATE_RESVALUE, "v.state.resvalue");
  v_tmpisnullp = LLVMBuildStructGEP(b, v_state, FIELDNO_EXPRSTATE_RESNULL, "v.state.resnull");

                          
  v_scanslot = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_SCANTUPLE, "v_scanslot");
  v_innerslot = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_INNERTUPLE, "v_innerslot");
  v_outerslot = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_OUTERTUPLE, "v_outerslot");
  v_resultslot = l_load_struct_gep(b, v_state, FIELDNO_EXPRSTATE_RESULTSLOT, "v_resultslot");

                                           
  v_scanvalues = l_load_struct_gep(b, v_scanslot, FIELDNO_TUPLETABLESLOT_VALUES, "v_scanvalues");
  v_scannulls = l_load_struct_gep(b, v_scanslot, FIELDNO_TUPLETABLESLOT_ISNULL, "v_scannulls");
  v_innervalues = l_load_struct_gep(b, v_innerslot, FIELDNO_TUPLETABLESLOT_VALUES, "v_innervalues");
  v_innernulls = l_load_struct_gep(b, v_innerslot, FIELDNO_TUPLETABLESLOT_ISNULL, "v_innernulls");
  v_outervalues = l_load_struct_gep(b, v_outerslot, FIELDNO_TUPLETABLESLOT_VALUES, "v_outervalues");
  v_outernulls = l_load_struct_gep(b, v_outerslot, FIELDNO_TUPLETABLESLOT_ISNULL, "v_outernulls");
  v_resultvalues = l_load_struct_gep(b, v_resultslot, FIELDNO_TUPLETABLESLOT_VALUES, "v_resultvalues");
  v_resultnulls = l_load_struct_gep(b, v_resultslot, FIELDNO_TUPLETABLESLOT_ISNULL, "v_resultnulls");

                          
  v_aggvalues = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_AGGVALUES, "v.econtext.aggvalues");
  v_aggnulls = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_AGGNULLS, "v.econtext.aggnulls");

                                                                      
  opblocks = palloc(sizeof(LLVMBasicBlockRef) * state->steps_len);
  for (i = 0; i < state->steps_len; i++)
  {
    opblocks[i] = l_bb_append_v(eval_fn, "b.op.%d.start", i);
  }

                                      
  LLVMBuildBr(b, opblocks[0]);

  for (i = 0; i < state->steps_len; i++)
  {
    ExprEvalStep *op;
    ExprEvalOp opcode;
    LLVMValueRef v_resvaluep;
    LLVMValueRef v_resnullp;

    LLVMPositionBuilderAtEnd(b, opblocks[i]);

    op = &state->steps[i];
    opcode = ExecEvalStepOp(state, op);

    v_resvaluep = l_ptr_const(op->resvalue, l_ptr(TypeSizeT));
    v_resnullp = l_ptr_const(op->resnull, l_ptr(TypeStorageBool));

    switch (opcode)
    {
    case EEOP_DONE:
    {
      LLVMValueRef v_tmpisnull, v_tmpvalue;

      v_tmpvalue = LLVMBuildLoad(b, v_tmpvaluep, "");
      v_tmpisnull = LLVMBuildLoad(b, v_tmpisnullp, "");

      LLVMBuildStore(b, v_tmpisnull, v_isnullp);

      LLVMBuildRet(b, v_tmpvalue);
      break;
    }

    case EEOP_INNER_FETCHSOME:
    case EEOP_OUTER_FETCHSOME:
    case EEOP_SCAN_FETCHSOME:
    {
      TupleDesc desc = NULL;
      LLVMValueRef v_slot;
      LLVMBasicBlockRef b_fetch;
      LLVMValueRef v_nvalid;
      LLVMValueRef l_jit_deform = NULL;
      const TupleTableSlotOps *tts_ops = NULL;

      b_fetch = l_bb_before_v(opblocks[i + 1], "op.%d.fetch", i);

      if (op->d.fetch.known_desc)
      {
        desc = op->d.fetch.known_desc;
      }

      if (op->d.fetch.fixed)
      {
        tts_ops = op->d.fetch.kind;
      }

      if (opcode == EEOP_INNER_FETCHSOME)
      {
        v_slot = v_innerslot;
      }
      else if (opcode == EEOP_OUTER_FETCHSOME)
      {
        v_slot = v_outerslot;
      }
      else
      {
        v_slot = v_scanslot;
      }

         
                                                            
                                        
         
                                                               
                            
         
      v_nvalid = l_load_struct_gep(b, v_slot, FIELDNO_TUPLETABLESLOT_NVALID, "");
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntUGE, v_nvalid, l_int16_const(op->d.fetch.last_var), ""), opblocks[i + 1], b_fetch);

      LLVMPositionBuilderAtEnd(b, b_fetch);

         
                                                                
                                                          
                                                                
                                     
         
      if (tts_ops && desc && (context->base.flags & PGJIT_DEFORM))
      {
        l_jit_deform = slot_compile_deform(context, desc, tts_ops, op->d.fetch.last_var);
      }

      if (l_jit_deform)
      {
        LLVMValueRef params[1];

        params[0] = v_slot;

        LLVMBuildCall(b, l_jit_deform, params, lengthof(params), "");
      }
      else
      {
        LLVMValueRef params[2];

        params[0] = v_slot;
        params[1] = l_int32_const(op->d.fetch.last_var);

        LLVMBuildCall(b, llvm_get_decl(mod, FuncSlotGetsomeattrsInt), params, lengthof(params), "");
      }

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_INNER_VAR:
    case EEOP_OUTER_VAR:
    case EEOP_SCAN_VAR:
    {
      LLVMValueRef value, isnull;
      LLVMValueRef v_attnum;
      LLVMValueRef v_values;
      LLVMValueRef v_nulls;

      if (opcode == EEOP_INNER_VAR)
      {
        v_values = v_innervalues;
        v_nulls = v_innernulls;
      }
      else if (opcode == EEOP_OUTER_VAR)
      {
        v_values = v_outervalues;
        v_nulls = v_outernulls;
      }
      else
      {
        v_values = v_scanvalues;
        v_nulls = v_scannulls;
      }

      v_attnum = l_int32_const(op->d.var.attnum);
      value = l_load_gep1(b, v_values, v_attnum, "");
      isnull = l_load_gep1(b, v_nulls, v_attnum, "");
      LLVMBuildStore(b, value, v_resvaluep);
      LLVMBuildStore(b, isnull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_INNER_SYSVAR:
    case EEOP_OUTER_SYSVAR:
    case EEOP_SCAN_SYSVAR:
    {
      LLVMValueRef v_slot;
      LLVMValueRef v_params[4];

      if (opcode == EEOP_INNER_SYSVAR)
      {
        v_slot = v_innerslot;
      }
      else if (opcode == EEOP_OUTER_SYSVAR)
      {
        v_slot = v_outerslot;
      }
      else
      {
        v_slot = v_scanslot;
      }

      v_params[0] = v_state;
      v_params[1] = l_ptr_const(op, l_ptr(StructExprEvalStep));
      v_params[2] = v_econtext;
      v_params[3] = v_slot;

      LLVMBuildCall(b, llvm_get_decl(mod, FuncExecEvalSysVar), v_params, lengthof(v_params), "");

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_WHOLEROW:
      build_EvalXFunc(b, mod, "ExecEvalWholeRowVar", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_ASSIGN_INNER_VAR:
    case EEOP_ASSIGN_OUTER_VAR:
    case EEOP_ASSIGN_SCAN_VAR:
    {
      LLVMValueRef v_value, v_isnull;
      LLVMValueRef v_rvaluep, v_risnullp;
      LLVMValueRef v_attnum, v_resultnum;
      LLVMValueRef v_values;
      LLVMValueRef v_nulls;

      if (opcode == EEOP_ASSIGN_INNER_VAR)
      {
        v_values = v_innervalues;
        v_nulls = v_innernulls;
      }
      else if (opcode == EEOP_ASSIGN_OUTER_VAR)
      {
        v_values = v_outervalues;
        v_nulls = v_outernulls;
      }
      else
      {
        v_values = v_scanvalues;
        v_nulls = v_scannulls;
      }

                     
      v_attnum = l_int32_const(op->d.assign_var.attnum);
      v_value = l_load_gep1(b, v_values, v_attnum, "");
      v_isnull = l_load_gep1(b, v_nulls, v_attnum, "");

                                        
      v_resultnum = l_int32_const(op->d.assign_var.resultnum);
      v_rvaluep = LLVMBuildGEP(b, v_resultvalues, &v_resultnum, 1, "");
      v_risnullp = LLVMBuildGEP(b, v_resultnulls, &v_resultnum, 1, "");

                     
      LLVMBuildStore(b, v_value, v_rvaluep);
      LLVMBuildStore(b, v_isnull, v_risnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_ASSIGN_TMP:
    {
      LLVMValueRef v_value, v_isnull;
      LLVMValueRef v_rvaluep, v_risnullp;
      LLVMValueRef v_resultnum;
      size_t resultnum = op->d.assign_tmp.resultnum;

                     
      v_value = LLVMBuildLoad(b, v_tmpvaluep, "");
      v_isnull = LLVMBuildLoad(b, v_tmpisnullp, "");

                                        
      v_resultnum = l_int32_const(resultnum);
      v_rvaluep = LLVMBuildGEP(b, v_resultvalues, &v_resultnum, 1, "");
      v_risnullp = LLVMBuildGEP(b, v_resultnulls, &v_resultnum, 1, "");

                     
      LLVMBuildStore(b, v_value, v_rvaluep);
      LLVMBuildStore(b, v_isnull, v_risnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_ASSIGN_TMP_MAKE_RO:
    {
      LLVMBasicBlockRef b_notnull;
      LLVMValueRef v_params[1];
      LLVMValueRef v_ret;
      LLVMValueRef v_value, v_isnull;
      LLVMValueRef v_rvaluep, v_risnullp;
      LLVMValueRef v_resultnum;
      size_t resultnum = op->d.assign_tmp.resultnum;

      b_notnull = l_bb_before_v(opblocks[i + 1], "op.%d.assign_tmp.notnull", i);

                     
      v_value = LLVMBuildLoad(b, v_tmpvaluep, "");
      v_isnull = LLVMBuildLoad(b, v_tmpisnullp, "");

                                        
      v_resultnum = l_int32_const(resultnum);
      v_rvaluep = LLVMBuildGEP(b, v_resultvalues, &v_resultnum, 1, "");
      v_risnullp = LLVMBuildGEP(b, v_resultnulls, &v_resultnum, 1, "");

                          
      LLVMBuildStore(b, v_isnull, v_risnullp);

                                  
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_isnull, l_sbool_const(0), ""), b_notnull, opblocks[i + 1]);

                                                     
      LLVMPositionBuilderAtEnd(b, b_notnull);
      v_params[0] = v_value;
      v_ret = LLVMBuildCall(b, llvm_get_decl(mod, FuncMakeExpandedObjectReadOnlyInternal), v_params, lengthof(v_params), "");

                       
      LLVMBuildStore(b, v_ret, v_rvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_CONST:
    {
      LLVMValueRef v_constvalue, v_constnull;

      v_constvalue = l_sizet_const(op->d.constval.value);
      v_constnull = l_sbool_const(op->d.constval.isnull);

      LLVMBuildStore(b, v_constvalue, v_resvaluep);
      LLVMBuildStore(b, v_constnull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_FUNCEXPR_STRICT:
    {
      FunctionCallInfo fcinfo = op->d.func.fcinfo_data;
      LLVMBasicBlockRef b_nonull;
      int argno;
      LLVMValueRef v_fcinfo;
      LLVMBasicBlockRef *b_checkargnulls;

         
                                                         
                   
         
      b_nonull = l_bb_before_v(opblocks[i + 1], "b.%d.no-null-args", i);

                                                         
      if (op->d.func.nargs == 0)
      {
        elog(ERROR, "argumentless strict functions are pointless");
      }

      v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));

         
                                                          
                                
         
      LLVMBuildStore(b, l_sbool_const(1), v_resnullp);

                                                         
      b_checkargnulls = palloc(sizeof(LLVMBasicBlockRef *) * op->d.func.nargs);
      for (argno = 0; argno < op->d.func.nargs; argno++)
      {
        b_checkargnulls[argno] = l_bb_before_v(b_nonull, "b.%d.isnull.%d", i, argno);
      }

                                           
      LLVMBuildBr(b, b_checkargnulls[0]);

                                       
      for (argno = 0; argno < op->d.func.nargs; argno++)
      {
        LLVMValueRef v_argisnull;
        LLVMBasicBlockRef b_argnotnull;

        LLVMPositionBuilderAtEnd(b, b_checkargnulls[argno]);

                                                              
        if (argno + 1 == op->d.func.nargs)
        {
          b_argnotnull = b_nonull;
        }
        else
        {
          b_argnotnull = b_checkargnulls[argno + 1];
        }

                                                      
        v_argisnull = l_funcnull(b, v_fcinfo, argno);
        LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_argisnull, l_sbool_const(1), ""), opblocks[i + 1], b_argnotnull);
      }

      LLVMPositionBuilderAtEnd(b, b_nonull);
    }
                       

    case EEOP_FUNCEXPR:
    {
      FunctionCallInfo fcinfo = op->d.func.fcinfo_data;
      LLVMValueRef v_fcinfo_isnull;
      LLVMValueRef v_retval;

      v_retval = BuildV1Call(context, b, mod, fcinfo, &v_fcinfo_isnull);
      LLVMBuildStore(b, v_retval, v_resvaluep);
      LLVMBuildStore(b, v_fcinfo_isnull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_FUNCEXPR_FUSAGE:
      build_EvalXFunc(b, mod, "ExecEvalFuncExprFusage", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_FUNCEXPR_STRICT_FUSAGE:
      build_EvalXFunc(b, mod, "ExecEvalFuncExprStrictFusage", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_BOOL_AND_STEP_FIRST:
    {
      LLVMValueRef v_boolanynullp;

      v_boolanynullp = l_ptr_const(op->d.boolexpr.anynull, l_ptr(TypeStorageBool));
      LLVMBuildStore(b, l_sbool_const(0), v_boolanynullp);
    }
                       

         
                                                           
                                                                     
                 
         
    case EEOP_BOOL_AND_STEP_LAST:
    case EEOP_BOOL_AND_STEP:
    {
      LLVMValueRef v_boolvalue;
      LLVMValueRef v_boolnull;
      LLVMValueRef v_boolanynullp, v_boolanynull;
      LLVMBasicBlockRef b_boolisnull;
      LLVMBasicBlockRef b_boolcheckfalse;
      LLVMBasicBlockRef b_boolisfalse;
      LLVMBasicBlockRef b_boolcont;
      LLVMBasicBlockRef b_boolisanynull;

      b_boolisnull = l_bb_before_v(opblocks[i + 1], "b.%d.boolisnull", i);
      b_boolcheckfalse = l_bb_before_v(opblocks[i + 1], "b.%d.boolcheckfalse", i);
      b_boolisfalse = l_bb_before_v(opblocks[i + 1], "b.%d.boolisfalse", i);
      b_boolisanynull = l_bb_before_v(opblocks[i + 1], "b.%d.boolisanynull", i);
      b_boolcont = l_bb_before_v(opblocks[i + 1], "b.%d.boolcont", i);

      v_boolanynullp = l_ptr_const(op->d.boolexpr.anynull, l_ptr(TypeStorageBool));

      v_boolnull = LLVMBuildLoad(b, v_resnullp, "");
      v_boolvalue = LLVMBuildLoad(b, v_resvaluep, "");

                                   
      LLVMBuildStore(b, v_boolnull, v_resnullp);
                                    
      LLVMBuildStore(b, v_boolvalue, v_resvaluep);

                                          
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolnull, l_sbool_const(1), ""), b_boolisnull, b_boolcheckfalse);

                                         
      LLVMPositionBuilderAtEnd(b, b_boolisnull);
                                   
      LLVMBuildStore(b, l_sbool_const(1), v_boolanynullp);
                                  
      LLVMBuildBr(b, b_boolcont);

                                          
      LLVMPositionBuilderAtEnd(b, b_boolcheckfalse);
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolvalue, l_sizet_const(0), ""), b_boolisfalse, b_boolcont);

         
                                                              
                  
         
      LLVMPositionBuilderAtEnd(b, b_boolisfalse);
                                                              
                                                     
      LLVMBuildBr(b, opblocks[op->d.boolexpr.jumpdone]);

                                                       
      LLVMPositionBuilderAtEnd(b, b_boolcont);

      v_boolanynull = LLVMBuildLoad(b, v_boolanynullp, "");

                                                              
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolanynull, l_sbool_const(0), ""), opblocks[i + 1], b_boolisanynull);

      LLVMPositionBuilderAtEnd(b, b_boolisanynull);
                               
      LLVMBuildStore(b, l_sbool_const(1), v_resnullp);
                          
      LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }
    case EEOP_BOOL_OR_STEP_FIRST:
    {
      LLVMValueRef v_boolanynullp;

      v_boolanynullp = l_ptr_const(op->d.boolexpr.anynull, l_ptr(TypeStorageBool));
      LLVMBuildStore(b, l_sbool_const(0), v_boolanynullp);
    }
                       

         
                                                           
                                                                     
                 
         
    case EEOP_BOOL_OR_STEP_LAST:
    case EEOP_BOOL_OR_STEP:
    {
      LLVMValueRef v_boolvalue;
      LLVMValueRef v_boolnull;
      LLVMValueRef v_boolanynullp, v_boolanynull;

      LLVMBasicBlockRef b_boolisnull;
      LLVMBasicBlockRef b_boolchecktrue;
      LLVMBasicBlockRef b_boolistrue;
      LLVMBasicBlockRef b_boolcont;
      LLVMBasicBlockRef b_boolisanynull;

      b_boolisnull = l_bb_before_v(opblocks[i + 1], "b.%d.boolisnull", i);
      b_boolchecktrue = l_bb_before_v(opblocks[i + 1], "b.%d.boolchecktrue", i);
      b_boolistrue = l_bb_before_v(opblocks[i + 1], "b.%d.boolistrue", i);
      b_boolisanynull = l_bb_before_v(opblocks[i + 1], "b.%d.boolisanynull", i);
      b_boolcont = l_bb_before_v(opblocks[i + 1], "b.%d.boolcont", i);

      v_boolanynullp = l_ptr_const(op->d.boolexpr.anynull, l_ptr(TypeStorageBool));

      v_boolnull = LLVMBuildLoad(b, v_resnullp, "");
      v_boolvalue = LLVMBuildLoad(b, v_resvaluep, "");

                                   
      LLVMBuildStore(b, v_boolnull, v_resnullp);
                                    
      LLVMBuildStore(b, v_boolvalue, v_resvaluep);

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolnull, l_sbool_const(1), ""), b_boolisnull, b_boolchecktrue);

                                         
      LLVMPositionBuilderAtEnd(b, b_boolisnull);
                                   
      LLVMBuildStore(b, l_sbool_const(1), v_boolanynullp);
                                  
      LLVMBuildBr(b, b_boolcont);

                                         
      LLVMPositionBuilderAtEnd(b, b_boolchecktrue);
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolvalue, l_sizet_const(1), ""), b_boolistrue, b_boolcont);

         
                                                            
                  
         
      LLVMPositionBuilderAtEnd(b, b_boolistrue);
                                                             
                                                    
      LLVMBuildBr(b, opblocks[op->d.boolexpr.jumpdone]);

                                                       
      LLVMPositionBuilderAtEnd(b, b_boolcont);

      v_boolanynull = LLVMBuildLoad(b, v_boolanynullp, "");

                                                              
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolanynull, l_sbool_const(0), ""), opblocks[i + 1], b_boolisanynull);

      LLVMPositionBuilderAtEnd(b, b_boolisanynull);
                               
      LLVMBuildStore(b, l_sbool_const(1), v_resnullp);
                          
      LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_BOOL_NOT_STEP:
    {
      LLVMValueRef v_boolvalue;
      LLVMValueRef v_boolnull;
      LLVMValueRef v_negbool;

      v_boolnull = LLVMBuildLoad(b, v_resnullp, "");
      v_boolvalue = LLVMBuildLoad(b, v_resvaluep, "");

      v_negbool = LLVMBuildZExt(b, LLVMBuildICmp(b, LLVMIntEQ, v_boolvalue, l_sizet_const(0), ""), TypeSizeT, "");
                                   
      LLVMBuildStore(b, v_boolnull, v_resnullp);
                                     
      LLVMBuildStore(b, v_negbool, v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_QUAL:
    {
      LLVMValueRef v_resnull;
      LLVMValueRef v_resvalue;
      LLVMValueRef v_nullorfalse;
      LLVMBasicBlockRef b_qualfail;

      b_qualfail = l_bb_before_v(opblocks[i + 1], "op.%d.qualfail", i);

      v_resvalue = LLVMBuildLoad(b, v_resvaluep, "");
      v_resnull = LLVMBuildLoad(b, v_resnullp, "");

      v_nullorfalse = LLVMBuildOr(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), LLVMBuildICmp(b, LLVMIntEQ, v_resvalue, l_sizet_const(0), ""), "");

      LLVMBuildCondBr(b, v_nullorfalse, b_qualfail, opblocks[i + 1]);

                                              
      LLVMPositionBuilderAtEnd(b, b_qualfail);
                                
      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);
                                 
      LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);
                        
      LLVMBuildBr(b, opblocks[op->d.qualexpr.jumpdone]);
      break;
    }

    case EEOP_JUMP:
    {
      LLVMBuildBr(b, opblocks[op->d.jump.jumpdone]);
      break;
    }

    case EEOP_JUMP_IF_NULL:
    {
      LLVMValueRef v_resnull;

                                                      

      v_resnull = LLVMBuildLoad(b, v_resnullp, "");

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), opblocks[op->d.jump.jumpdone], opblocks[i + 1]);
      break;
    }

    case EEOP_JUMP_IF_NOT_NULL:
    {
      LLVMValueRef v_resnull;

                                                          

      v_resnull = LLVMBuildLoad(b, v_resnullp, "");

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(0), ""), opblocks[op->d.jump.jumpdone], opblocks[i + 1]);
      break;
    }

    case EEOP_JUMP_IF_NOT_TRUE:
    {
      LLVMValueRef v_resnull;
      LLVMValueRef v_resvalue;
      LLVMValueRef v_nullorfalse;

                                                               

      v_resvalue = LLVMBuildLoad(b, v_resvaluep, "");
      v_resnull = LLVMBuildLoad(b, v_resnullp, "");

      v_nullorfalse = LLVMBuildOr(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), LLVMBuildICmp(b, LLVMIntEQ, v_resvalue, l_sizet_const(0), ""), "");

      LLVMBuildCondBr(b, v_nullorfalse, opblocks[op->d.jump.jumpdone], opblocks[i + 1]);
      break;
    }

    case EEOP_NULLTEST_ISNULL:
    {
      LLVMValueRef v_resnull = LLVMBuildLoad(b, v_resnullp, "");
      LLVMValueRef v_resvalue;

      v_resvalue = LLVMBuildSelect(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), l_sizet_const(1), l_sizet_const(0), "");
      LLVMBuildStore(b, v_resvalue, v_resvaluep);
      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_NULLTEST_ISNOTNULL:
    {
      LLVMValueRef v_resnull = LLVMBuildLoad(b, v_resnullp, "");
      LLVMValueRef v_resvalue;

      v_resvalue = LLVMBuildSelect(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), l_sizet_const(0), l_sizet_const(1), "");
      LLVMBuildStore(b, v_resvalue, v_resvaluep);
      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_NULLTEST_ROWISNULL:
      build_EvalXFunc(b, mod, "ExecEvalRowNull", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_NULLTEST_ROWISNOTNULL:
      build_EvalXFunc(b, mod, "ExecEvalRowNotNull", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_BOOLTEST_IS_TRUE:
    case EEOP_BOOLTEST_IS_NOT_FALSE:
    case EEOP_BOOLTEST_IS_FALSE:
    case EEOP_BOOLTEST_IS_NOT_TRUE:
    {
      LLVMBasicBlockRef b_isnull, b_notnull;
      LLVMValueRef v_resnull = LLVMBuildLoad(b, v_resnullp, "");

      b_isnull = l_bb_before_v(opblocks[i + 1], "op.%d.isnull", i);
      b_notnull = l_bb_before_v(opblocks[i + 1], "op.%d.isnotnull", i);

                                  
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), b_isnull, b_notnull);

                                          
      LLVMPositionBuilderAtEnd(b, b_isnull);

                              
      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);

      if (opcode == EEOP_BOOLTEST_IS_TRUE || opcode == EEOP_BOOLTEST_IS_FALSE)
      {
        LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);
      }
      else
      {
        LLVMBuildStore(b, l_sizet_const(1), v_resvaluep);
      }

      LLVMBuildBr(b, opblocks[i + 1]);

      LLVMPositionBuilderAtEnd(b, b_notnull);

      if (opcode == EEOP_BOOLTEST_IS_TRUE || opcode == EEOP_BOOLTEST_IS_NOT_FALSE)
      {
           
                                                            
                
           
      }
      else
      {
        LLVMValueRef v_value = LLVMBuildLoad(b, v_resvaluep, "");

        v_value = LLVMBuildZExt(b, LLVMBuildICmp(b, LLVMIntEQ, v_value, l_sizet_const(0), ""), TypeSizeT, "");
        LLVMBuildStore(b, v_value, v_resvaluep);
      }
      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_PARAM_EXEC:
      build_EvalXFunc(b, mod, "ExecEvalParamExec", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_PARAM_EXTERN:
      build_EvalXFunc(b, mod, "ExecEvalParamExtern", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_PARAM_CALLBACK:
    {
      LLVMTypeRef param_types[3];
      LLVMValueRef v_params[3];
      LLVMTypeRef v_functype;
      LLVMValueRef v_func;

      param_types[0] = l_ptr(StructExprState);
      param_types[1] = l_ptr(TypeSizeT);
      param_types[2] = l_ptr(StructExprContext);

      v_functype = LLVMFunctionType(LLVMVoidType(), param_types, lengthof(param_types), false);
      v_func = l_ptr_const(op->d.cparam.paramfunc, l_ptr(v_functype));

      v_params[0] = v_state;
      v_params[1] = l_ptr_const(op, l_ptr(TypeSizeT));
      v_params[2] = v_econtext;
      LLVMBuildCall(b, v_func, v_params, lengthof(v_params), "");

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_SBSREF_OLD:
      build_EvalXFunc(b, mod, "ExecEvalSubscriptingRefOld", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_SBSREF_ASSIGN:
      build_EvalXFunc(b, mod, "ExecEvalSubscriptingRefAssign", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_SBSREF_FETCH:
      build_EvalXFunc(b, mod, "ExecEvalSubscriptingRefFetch", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_CASE_TESTVAL:
    {
      LLVMBasicBlockRef b_avail, b_notavail;
      LLVMValueRef v_casevaluep, v_casevalue;
      LLVMValueRef v_casenullp, v_casenull;
      LLVMValueRef v_casevaluenull;

      b_avail = l_bb_before_v(opblocks[i + 1], "op.%d.avail", i);
      b_notavail = l_bb_before_v(opblocks[i + 1], "op.%d.notavail", i);

      v_casevaluep = l_ptr_const(op->d.casetest.value, l_ptr(TypeSizeT));
      v_casenullp = l_ptr_const(op->d.casetest.isnull, l_ptr(TypeStorageBool));

      v_casevaluenull = LLVMBuildICmp(b, LLVMIntEQ, LLVMBuildPtrToInt(b, v_casevaluep, TypeSizeT, ""), l_sizet_const(0), "");
      LLVMBuildCondBr(b, v_casevaluenull, b_notavail, b_avail);

                               
      LLVMPositionBuilderAtEnd(b, b_avail);
      v_casevalue = LLVMBuildLoad(b, v_casevaluep, "");
      v_casenull = LLVMBuildLoad(b, v_casenullp, "");
      LLVMBuildStore(b, v_casevalue, v_resvaluep);
      LLVMBuildStore(b, v_casenull, v_resnullp);
      LLVMBuildBr(b, opblocks[i + 1]);

                               
      LLVMPositionBuilderAtEnd(b, b_notavail);
      v_casevalue = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_CASEDATUM, "");
      v_casenull = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_CASENULL, "");
      LLVMBuildStore(b, v_casevalue, v_resvaluep);
      LLVMBuildStore(b, v_casenull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_MAKE_READONLY:
    {
      LLVMBasicBlockRef b_notnull;
      LLVMValueRef v_params[1];
      LLVMValueRef v_ret;
      LLVMValueRef v_nullp;
      LLVMValueRef v_valuep;
      LLVMValueRef v_null;
      LLVMValueRef v_value;

      b_notnull = l_bb_before_v(opblocks[i + 1], "op.%d.readonly.notnull", i);

      v_nullp = l_ptr_const(op->d.make_readonly.isnull, l_ptr(TypeStorageBool));

      v_null = LLVMBuildLoad(b, v_nullp, "");

                                             
      LLVMBuildStore(b, v_null, v_resnullp);

                                  
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_null, l_sbool_const(1), ""), opblocks[i + 1], b_notnull);

                                                     
      LLVMPositionBuilderAtEnd(b, b_notnull);

      v_valuep = l_ptr_const(op->d.make_readonly.value, l_ptr(TypeSizeT));

      v_value = LLVMBuildLoad(b, v_valuep, "");

      v_params[0] = v_value;
      v_ret = LLVMBuildCall(b, llvm_get_decl(mod, FuncMakeExpandedObjectReadOnlyInternal), v_params, lengthof(v_params), "");
      LLVMBuildStore(b, v_ret, v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_IOCOERCE:
    {
      FunctionCallInfo fcinfo_out, fcinfo_in;
      LLVMValueRef v_fcinfo_out, v_fcinfo_in;
      LLVMValueRef v_fn_addr_out, v_fn_addr_in;
      LLVMValueRef v_fcinfo_in_isnullp;
      LLVMValueRef v_retval;
      LLVMValueRef v_resvalue;
      LLVMValueRef v_resnull;

      LLVMValueRef v_output_skip;
      LLVMValueRef v_output;

      LLVMBasicBlockRef b_skipoutput;
      LLVMBasicBlockRef b_calloutput;
      LLVMBasicBlockRef b_input;
      LLVMBasicBlockRef b_inputcall;

      fcinfo_out = op->d.iocoerce.fcinfo_data_out;
      fcinfo_in = op->d.iocoerce.fcinfo_data_in;

      b_skipoutput = l_bb_before_v(opblocks[i + 1], "op.%d.skipoutputnull", i);
      b_calloutput = l_bb_before_v(opblocks[i + 1], "op.%d.calloutput", i);
      b_input = l_bb_before_v(opblocks[i + 1], "op.%d.input", i);
      b_inputcall = l_bb_before_v(opblocks[i + 1], "op.%d.inputcall", i);

      v_fcinfo_out = l_ptr_const(fcinfo_out, l_ptr(StructFunctionCallInfoData));
      v_fcinfo_in = l_ptr_const(fcinfo_in, l_ptr(StructFunctionCallInfoData));
      v_fn_addr_out = l_ptr_const(fcinfo_out->flinfo->fn_addr, TypePGFunction);
      v_fn_addr_in = l_ptr_const(fcinfo_in->flinfo->fn_addr, TypePGFunction);

      v_fcinfo_in_isnullp = LLVMBuildStructGEP(b, v_fcinfo_in, FIELDNO_FUNCTIONCALLINFODATA_ISNULL, "v_fcinfo_in_isnull");

                                                    
      v_resnull = LLVMBuildLoad(b, v_resnullp, "");
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_resnull, l_sbool_const(1), ""), b_skipoutput, b_calloutput);

      LLVMPositionBuilderAtEnd(b, b_skipoutput);
      v_output_skip = l_sizet_const(0);
      LLVMBuildBr(b, b_input);

      LLVMPositionBuilderAtEnd(b, b_calloutput);
      v_resvalue = LLVMBuildLoad(b, v_resvaluep, "");

                      
      LLVMBuildStore(b, v_resvalue, l_funcvaluep(b, v_fcinfo_out, 0));
      LLVMBuildStore(b, l_sbool_const(0), l_funcnullp(b, v_fcinfo_out, 0));
                                                            
      v_output = LLVMBuildCall(b, v_fn_addr_out, &v_fcinfo_out, 1, "funccall_coerce_out");
      LLVMBuildBr(b, b_input);

                                                    
      LLVMPositionBuilderAtEnd(b, b_input);

                                                                 
      {
        LLVMValueRef incoming_values[2];
        LLVMBasicBlockRef incoming_blocks[2];

        incoming_values[0] = v_output_skip;
        incoming_blocks[0] = b_skipoutput;

        incoming_values[1] = v_output;
        incoming_blocks[1] = b_calloutput;

        v_output = LLVMBuildPhi(b, TypeSizeT, "output");
        LLVMAddIncoming(v_output, incoming_values, incoming_blocks, lengthof(incoming_blocks));
      }

         
                                                              
               
         
      if (op->d.iocoerce.finfo_in->fn_strict)
      {
        LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_output, l_sizet_const(0), ""), opblocks[i + 1], b_inputcall);
      }
      else
      {
        LLVMBuildBr(b, b_inputcall);
      }

      LLVMPositionBuilderAtEnd(b, b_inputcall);
                         
                        
      LLVMBuildStore(b, v_output, l_funcvaluep(b, v_fcinfo_in, 0));
      LLVMBuildStore(b, v_resnull, l_funcnullp(b, v_fcinfo_in, 0));

                                               
                                               

                                   
      LLVMBuildStore(b, l_sbool_const(0), v_fcinfo_in_isnullp);
                             
      v_retval = LLVMBuildCall(b, v_fn_addr_in, &v_fcinfo_in, 1, "funccall_iocoerce_in");

      LLVMBuildStore(b, v_retval, v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_DISTINCT:
    case EEOP_NOT_DISTINCT:
    {
      FunctionCallInfo fcinfo = op->d.func.fcinfo_data;

      LLVMValueRef v_fcinfo;
      LLVMValueRef v_fcinfo_isnull;

      LLVMValueRef v_argnull0, v_argisnull0;
      LLVMValueRef v_argnull1, v_argisnull1;

      LLVMValueRef v_anyargisnull;
      LLVMValueRef v_bothargisnull;

      LLVMValueRef v_result;

      LLVMBasicBlockRef b_noargnull;
      LLVMBasicBlockRef b_checkbothargnull;
      LLVMBasicBlockRef b_bothargnull;
      LLVMBasicBlockRef b_anyargnull;

      b_noargnull = l_bb_before_v(opblocks[i + 1], "op.%d.noargnull", i);
      b_checkbothargnull = l_bb_before_v(opblocks[i + 1], "op.%d.checkbothargnull", i);
      b_bothargnull = l_bb_before_v(opblocks[i + 1], "op.%d.bothargnull", i);
      b_anyargnull = l_bb_before_v(opblocks[i + 1], "op.%d.anyargnull", i);

      v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));

                                                    
      v_argnull0 = l_funcnull(b, v_fcinfo, 0);
      v_argisnull0 = LLVMBuildICmp(b, LLVMIntEQ, v_argnull0, l_sbool_const(1), "");
      v_argnull1 = l_funcnull(b, v_fcinfo, 1);
      v_argisnull1 = LLVMBuildICmp(b, LLVMIntEQ, v_argnull1, l_sbool_const(1), "");

      v_anyargisnull = LLVMBuildOr(b, v_argisnull0, v_argisnull1, "");
      v_bothargisnull = LLVMBuildAnd(b, v_argisnull0, v_argisnull1, "");

         
                                                             
                                                              
                     
         
      LLVMBuildCondBr(b, v_anyargisnull, b_checkbothargnull, b_noargnull);

         
                                                 
         
      LLVMPositionBuilderAtEnd(b, b_checkbothargnull);
      LLVMBuildCondBr(b, v_bothargisnull, b_bothargnull, b_anyargnull);

                                              
      LLVMPositionBuilderAtEnd(b, b_bothargnull);
      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);
      if (opcode == EEOP_NOT_DISTINCT)
      {
        LLVMBuildStore(b, l_sizet_const(1), v_resvaluep);
      }
      else
      {
        LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);
      }

      LLVMBuildBr(b, opblocks[i + 1]);

                                                 
      LLVMPositionBuilderAtEnd(b, b_anyargnull);
      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);
      if (opcode == EEOP_NOT_DISTINCT)
      {
        LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);
      }
      else
      {
        LLVMBuildStore(b, l_sizet_const(1), v_resvaluep);
      }
      LLVMBuildBr(b, opblocks[i + 1]);

                                             
      LLVMPositionBuilderAtEnd(b, b_noargnull);

      v_result = BuildV1Call(context, b, mod, fcinfo, &v_fcinfo_isnull);

      if (opcode == EEOP_DISTINCT)
      {
                                       
        v_result = LLVMBuildZExt(b, LLVMBuildICmp(b, LLVMIntEQ, v_result, l_sizet_const(0), ""), TypeSizeT, "");
      }

      LLVMBuildStore(b, v_fcinfo_isnull, v_resnullp);
      LLVMBuildStore(b, v_result, v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_NULLIF:
    {
      FunctionCallInfo fcinfo = op->d.func.fcinfo_data;

      LLVMValueRef v_fcinfo;
      LLVMValueRef v_fcinfo_isnull;
      LLVMValueRef v_argnull0;
      LLVMValueRef v_argnull1;
      LLVMValueRef v_anyargisnull;
      LLVMValueRef v_arg0;
      LLVMBasicBlockRef b_hasnull;
      LLVMBasicBlockRef b_nonull;
      LLVMBasicBlockRef b_argsequal;
      LLVMValueRef v_retval;
      LLVMValueRef v_argsequal;

      b_hasnull = l_bb_before_v(opblocks[i + 1], "b.%d.null-args", i);
      b_nonull = l_bb_before_v(opblocks[i + 1], "b.%d.no-null-args", i);
      b_argsequal = l_bb_before_v(opblocks[i + 1], "b.%d.argsequal", i);

      v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));

                                                          
      v_argnull0 = l_funcnull(b, v_fcinfo, 0);
      v_argnull1 = l_funcnull(b, v_fcinfo, 1);

      v_anyargisnull = LLVMBuildOr(b, LLVMBuildICmp(b, LLVMIntEQ, v_argnull0, l_sbool_const(1), ""), LLVMBuildICmp(b, LLVMIntEQ, v_argnull1, l_sbool_const(1), ""), "");

      LLVMBuildCondBr(b, v_anyargisnull, b_hasnull, b_nonull);

                                                                  
      LLVMPositionBuilderAtEnd(b, b_hasnull);
      v_arg0 = l_funcvalue(b, v_fcinfo, 0);
      LLVMBuildStore(b, v_argnull0, v_resnullp);
      LLVMBuildStore(b, v_arg0, v_resvaluep);
      LLVMBuildBr(b, opblocks[i + 1]);

                                                           
      LLVMPositionBuilderAtEnd(b, b_nonull);

      v_retval = BuildV1Call(context, b, mod, fcinfo, &v_fcinfo_isnull);

         
                                                                 
                                                            
                     
         
      v_argsequal = LLVMBuildAnd(b, LLVMBuildICmp(b, LLVMIntEQ, v_fcinfo_isnull, l_sbool_const(0), ""), LLVMBuildICmp(b, LLVMIntEQ, v_retval, l_sizet_const(1), ""), "");
      LLVMBuildCondBr(b, v_argsequal, b_argsequal, b_hasnull);

                                                                 
      LLVMPositionBuilderAtEnd(b, b_argsequal);
      LLVMBuildStore(b, l_sbool_const(1), v_resnullp);
      LLVMBuildStore(b, l_sizet_const(0), v_resvaluep);
      LLVMBuildStore(b, v_retval, v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_SQLVALUEFUNCTION:
      build_EvalXFunc(b, mod, "ExecEvalSQLValueFunction", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_CURRENTOFEXPR:
      build_EvalXFunc(b, mod, "ExecEvalCurrentOfExpr", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_NEXTVALUEEXPR:
      build_EvalXFunc(b, mod, "ExecEvalNextValueExpr", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_ARRAYEXPR:
      build_EvalXFunc(b, mod, "ExecEvalArrayExpr", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_ARRAYCOERCE:
      build_EvalXFunc(b, mod, "ExecEvalArrayCoerce", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_ROW:
      build_EvalXFunc(b, mod, "ExecEvalRow", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_ROWCOMPARE_STEP:
    {
      FunctionCallInfo fcinfo = op->d.rowcompare_step.fcinfo_data;
      LLVMValueRef v_fcinfo_isnull;
      LLVMBasicBlockRef b_null;
      LLVMBasicBlockRef b_compare;
      LLVMBasicBlockRef b_compare_result;

      LLVMValueRef v_retval;

      b_null = l_bb_before_v(opblocks[i + 1], "op.%d.row-null", i);
      b_compare = l_bb_before_v(opblocks[i + 1], "op.%d.row-compare", i);
      b_compare_result = l_bb_before_v(opblocks[i + 1], "op.%d.row-compare-result", i);

         
                                                              
               
         
      if (op->d.rowcompare_step.finfo->fn_strict)
      {
        LLVMValueRef v_fcinfo;
        LLVMValueRef v_argnull0;
        LLVMValueRef v_argnull1;
        LLVMValueRef v_anyargisnull;

        v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));

        v_argnull0 = l_funcnull(b, v_fcinfo, 0);
        v_argnull1 = l_funcnull(b, v_fcinfo, 1);

        v_anyargisnull = LLVMBuildOr(b, LLVMBuildICmp(b, LLVMIntEQ, v_argnull0, l_sbool_const(1), ""), LLVMBuildICmp(b, LLVMIntEQ, v_argnull1, l_sbool_const(1), ""), "");

        LLVMBuildCondBr(b, v_anyargisnull, b_null, b_compare);
      }
      else
      {
        LLVMBuildBr(b, b_compare);
      }

                                                    
      LLVMPositionBuilderAtEnd(b, b_compare);

                         
      v_retval = BuildV1Call(context, b, mod, fcinfo, &v_fcinfo_isnull);
      LLVMBuildStore(b, v_retval, v_resvaluep);

                                                            
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_fcinfo_isnull, l_sbool_const(0), ""), b_compare_result, b_null);

                                                             
      LLVMPositionBuilderAtEnd(b, b_compare_result);

                                                          
      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_retval, l_sizet_const(0), ""), opblocks[i + 1], opblocks[op->d.rowcompare_step.jumpdone]);

         
                                                            
                 
         
      LLVMPositionBuilderAtEnd(b, b_null);
      LLVMBuildStore(b, l_sbool_const(1), v_resnullp);
      LLVMBuildBr(b, opblocks[op->d.rowcompare_step.jumpnull]);

      break;
    }

    case EEOP_ROWCOMPARE_FINAL:
    {
      RowCompareType rctype = op->d.rowcompare_final.rctype;

      LLVMValueRef v_cmpresult;
      LLVMValueRef v_result;
      LLVMIntPredicate predicate;

         
                                                             
                                                         
                           
         
      v_cmpresult = LLVMBuildTrunc(b, LLVMBuildLoad(b, v_resvaluep, ""), LLVMInt32Type(), "");

      switch (rctype)
      {
      case ROWCOMPARE_LT:
        predicate = LLVMIntSLT;
        break;
      case ROWCOMPARE_LE:
        predicate = LLVMIntSLE;
        break;
      case ROWCOMPARE_GT:
        predicate = LLVMIntSGT;
        break;
      case ROWCOMPARE_GE:
        predicate = LLVMIntSGE;
        break;
      default:
                                                 
        Assert(false);
        predicate = 0;                               
        break;
      }

      v_result = LLVMBuildICmp(b, predicate, v_cmpresult, l_int32_const(0), "");
      v_result = LLVMBuildZExt(b, v_result, TypeSizeT, "");

      LLVMBuildStore(b, l_sbool_const(0), v_resnullp);
      LLVMBuildStore(b, v_result, v_resvaluep);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_MINMAX:
      build_EvalXFunc(b, mod, "ExecEvalMinMax", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_FIELDSELECT:
      build_EvalXFunc(b, mod, "ExecEvalFieldSelect", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_FIELDSTORE_DEFORM:
      build_EvalXFunc(b, mod, "ExecEvalFieldStoreDeForm", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_FIELDSTORE_FORM:
      build_EvalXFunc(b, mod, "ExecEvalFieldStoreForm", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_SBSREF_SUBSCRIPT:
    {
      LLVMValueRef v_fn;
      int jumpdone = op->d.sbsref_subscript.jumpdone;
      LLVMValueRef v_params[2];
      LLVMValueRef v_ret;

      v_fn = llvm_get_decl(mod, FuncExecEvalSubscriptingRef);

      v_params[0] = v_state;
      v_params[1] = l_ptr_const(op, l_ptr(StructExprEvalStep));
      v_ret = LLVMBuildCall(b, v_fn, v_params, lengthof(v_params), "");
      v_ret = LLVMBuildZExt(b, v_ret, TypeStorageBool, "");

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_ret, l_sbool_const(1), ""), opblocks[i + 1], opblocks[jumpdone]);
      break;
    }

    case EEOP_DOMAIN_TESTVAL:
    {
      LLVMBasicBlockRef b_avail, b_notavail;
      LLVMValueRef v_casevaluep, v_casevalue;
      LLVMValueRef v_casenullp, v_casenull;
      LLVMValueRef v_casevaluenull;

      b_avail = l_bb_before_v(opblocks[i + 1], "op.%d.avail", i);
      b_notavail = l_bb_before_v(opblocks[i + 1], "op.%d.notavail", i);

      v_casevaluep = l_ptr_const(op->d.casetest.value, l_ptr(TypeSizeT));
      v_casenullp = l_ptr_const(op->d.casetest.isnull, l_ptr(TypeStorageBool));

      v_casevaluenull = LLVMBuildICmp(b, LLVMIntEQ, LLVMBuildPtrToInt(b, v_casevaluep, TypeSizeT, ""), l_sizet_const(0), "");
      LLVMBuildCondBr(b, v_casevaluenull, b_notavail, b_avail);

                               
      LLVMPositionBuilderAtEnd(b, b_avail);
      v_casevalue = LLVMBuildLoad(b, v_casevaluep, "");
      v_casenull = LLVMBuildLoad(b, v_casenullp, "");
      LLVMBuildStore(b, v_casevalue, v_resvaluep);
      LLVMBuildStore(b, v_casenull, v_resnullp);
      LLVMBuildBr(b, opblocks[i + 1]);

                               
      LLVMPositionBuilderAtEnd(b, b_notavail);
      v_casevalue = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_DOMAINDATUM, "");
      v_casenull = l_load_struct_gep(b, v_econtext, FIELDNO_EXPRCONTEXT_DOMAINNULL, "");
      LLVMBuildStore(b, v_casevalue, v_resvaluep);
      LLVMBuildStore(b, v_casenull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_DOMAIN_NOTNULL:
      build_EvalXFunc(b, mod, "ExecEvalConstraintNotNull", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_DOMAIN_CHECK:
      build_EvalXFunc(b, mod, "ExecEvalConstraintCheck", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_CONVERT_ROWTYPE:
      build_EvalXFunc(b, mod, "ExecEvalConvertRowtype", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_SCALARARRAYOP:
      build_EvalXFunc(b, mod, "ExecEvalScalarArrayOp", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_XMLEXPR:
      build_EvalXFunc(b, mod, "ExecEvalXmlExpr", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_AGGREF:
    {
      AggrefExprState *aggref = op->d.aggref.astate;
      LLVMValueRef v_aggnop;
      LLVMValueRef v_aggno;
      LLVMValueRef value, isnull;

         
                                                                 
                                                                 
                                              
         
      v_aggnop = l_ptr_const(&aggref->aggno, l_ptr(LLVMInt32Type()));
      v_aggno = LLVMBuildLoad(b, v_aggnop, "v_aggno");

                                 
      value = l_load_gep1(b, v_aggvalues, v_aggno, "aggvalue");
      isnull = l_load_gep1(b, v_aggnulls, v_aggno, "aggnull");

                            
      LLVMBuildStore(b, value, v_resvaluep);
      LLVMBuildStore(b, isnull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_GROUPING_FUNC:
      build_EvalXFunc(b, mod, "ExecEvalGroupingFunc", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_WINDOW_FUNC:
    {
      WindowFuncExprState *wfunc = op->d.window_func.wfstate;
      LLVMValueRef v_wfuncnop;
      LLVMValueRef v_wfuncno;
      LLVMValueRef value, isnull;

         
                                                                
                                                          
                                                              
         
      v_wfuncnop = l_ptr_const(&wfunc->wfuncno, l_ptr(LLVMInt32Type()));
      v_wfuncno = LLVMBuildLoad(b, v_wfuncnop, "v_wfuncno");

                                         
      value = l_load_gep1(b, v_aggvalues, v_wfuncno, "windowvalue");
      isnull = l_load_gep1(b, v_aggnulls, v_wfuncno, "windownull");

      LLVMBuildStore(b, value, v_resvaluep);
      LLVMBuildStore(b, isnull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_SUBPLAN:
      build_EvalXFunc(b, mod, "ExecEvalSubPlan", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_ALTERNATIVE_SUBPLAN:
      build_EvalXFunc(b, mod, "ExecEvalAlternativeSubPlan", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_AGG_STRICT_DESERIALIZE:
    {
      FunctionCallInfo fcinfo = op->d.agg_deserialize.fcinfo_data;
      LLVMValueRef v_fcinfo;
      LLVMValueRef v_argnull0;
      LLVMBasicBlockRef b_deserialize;

      b_deserialize = l_bb_before_v(opblocks[i + 1], "op.%d.deserialize", i);

      v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));
      v_argnull0 = l_funcnull(b, v_fcinfo, 0);

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_argnull0, l_sbool_const(1), ""), opblocks[op->d.agg_deserialize.jumpnull], b_deserialize);
      LLVMPositionBuilderAtEnd(b, b_deserialize);
    }
                       

    case EEOP_AGG_DESERIALIZE:
    {
      AggState *aggstate;
      FunctionCallInfo fcinfo;

      LLVMValueRef v_retval;
      LLVMValueRef v_fcinfo_isnull;
      LLVMValueRef v_tmpcontext;
      LLVMValueRef v_oldcontext;

      aggstate = op->d.agg_deserialize.aggstate;
      fcinfo = op->d.agg_deserialize.fcinfo_data;

      v_tmpcontext = l_ptr_const(aggstate->tmpcontext->ecxt_per_tuple_memory, l_ptr(StructMemoryContextData));
      v_oldcontext = l_mcxt_switch(mod, b, v_tmpcontext);
      v_retval = BuildV1Call(context, b, mod, fcinfo, &v_fcinfo_isnull);
      l_mcxt_switch(mod, b, v_oldcontext);

      LLVMBuildStore(b, v_retval, v_resvaluep);
      LLVMBuildStore(b, v_fcinfo_isnull, v_resnullp);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_AGG_STRICT_INPUT_CHECK_NULLS:
    case EEOP_AGG_STRICT_INPUT_CHECK_ARGS:
    {
      int nargs = op->d.agg_strict_input_check.nargs;
      NullableDatum *args = op->d.agg_strict_input_check.args;
      bool *nulls = op->d.agg_strict_input_check.nulls;
      int jumpnull;
      int argno;

      LLVMValueRef v_argsp;
      LLVMValueRef v_nullsp;
      LLVMBasicBlockRef *b_checknulls;

      Assert(nargs > 0);

      jumpnull = op->d.agg_strict_input_check.jumpnull;
      v_argsp = l_ptr_const(args, l_ptr(StructNullableDatum));
      v_nullsp = l_ptr_const(nulls, l_ptr(TypeStorageBool));

                                           
      b_checknulls = palloc(sizeof(LLVMBasicBlockRef *) * nargs);
      for (argno = 0; argno < nargs; argno++)
      {
        b_checknulls[argno] = l_bb_before_v(opblocks[i + 1], "op.%d.check-null.%d", i, argno);
      }

      LLVMBuildBr(b, b_checknulls[0]);

                                                
      for (argno = 0; argno < nargs; argno++)
      {
        LLVMValueRef v_argno = l_int32_const(argno);
        LLVMValueRef v_argisnull;
        LLVMBasicBlockRef b_argnotnull;

        LLVMPositionBuilderAtEnd(b, b_checknulls[argno]);

        if (argno + 1 == nargs)
        {
          b_argnotnull = opblocks[i + 1];
        }
        else
        {
          b_argnotnull = b_checknulls[argno + 1];
        }

        if (opcode == EEOP_AGG_STRICT_INPUT_CHECK_NULLS)
        {
          v_argisnull = l_load_gep1(b, v_nullsp, v_argno, "");
        }
        else
        {
          LLVMValueRef v_argn;

          v_argn = LLVMBuildGEP(b, v_argsp, &v_argno, 1, "");
          v_argisnull = l_load_struct_gep(b, v_argn, FIELDNO_NULLABLE_DATUM_ISNULL, "");
        }

        LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_argisnull, l_sbool_const(1), ""), opblocks[jumpnull], b_argnotnull);
      }

      break;
    }

    case EEOP_AGG_INIT_TRANS:
    {
      AggState *aggstate;
      AggStatePerTrans pertrans;

      LLVMValueRef v_aggstatep;
      LLVMValueRef v_pertransp;

      LLVMValueRef v_allpergroupsp;

      LLVMValueRef v_pergroupp;

      LLVMValueRef v_setoff, v_transno;

      LLVMValueRef v_notransvalue;

      LLVMBasicBlockRef b_init;

      aggstate = op->d.agg_init_trans.aggstate;
      pertrans = op->d.agg_init_trans.pertrans;

      v_aggstatep = l_ptr_const(aggstate, l_ptr(StructAggState));
      v_pertransp = l_ptr_const(pertrans, l_ptr(StructAggStatePerTransData));

         
                                             
                                             
                                               
         
      v_allpergroupsp = l_load_struct_gep(b, v_aggstatep, FIELDNO_AGGSTATE_ALL_PERGROUPS, "aggstate.all_pergroups");
      v_setoff = l_int32_const(op->d.agg_init_trans.setoff);
      v_transno = l_int32_const(op->d.agg_init_trans.transno);
      v_pergroupp = LLVMBuildGEP(b, l_load_gep1(b, v_allpergroupsp, v_setoff, ""), &v_transno, 1, "");

      v_notransvalue = l_load_struct_gep(b, v_pergroupp, FIELDNO_AGGSTATEPERGROUPDATA_NOTRANSVALUE, "notransvalue");

      b_init = l_bb_before_v(opblocks[i + 1], "op.%d.inittrans", i);

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_notransvalue, l_sbool_const(1), ""), b_init, opblocks[i + 1]);

      LLVMPositionBuilderAtEnd(b, b_init);

      {
        LLVMValueRef params[3];
        LLVMValueRef v_curaggcontext;
        LLVMValueRef v_current_set;
        LLVMValueRef v_aggcontext;

        v_aggcontext = l_ptr_const(op->d.agg_init_trans.aggcontext, l_ptr(StructExprContext));

        v_current_set = LLVMBuildStructGEP(b, v_aggstatep, FIELDNO_AGGSTATE_CURRENT_SET, "aggstate.current_set");
        v_curaggcontext = LLVMBuildStructGEP(b, v_aggstatep, FIELDNO_AGGSTATE_CURAGGCONTEXT, "aggstate.curaggcontext");

        LLVMBuildStore(b, l_int32_const(op->d.agg_init_trans.setno), v_current_set);
        LLVMBuildStore(b, v_aggcontext, v_curaggcontext);

        params[0] = v_aggstatep;
        params[1] = v_pertransp;
        params[2] = v_pergroupp;

        LLVMBuildCall(b, llvm_get_decl(mod, FuncExecAggInitGroup), params, lengthof(params), "");
      }
      LLVMBuildBr(b, opblocks[op->d.agg_init_trans.jumpnull]);

      break;
    }

    case EEOP_AGG_STRICT_TRANS_CHECK:
    {
      AggState *aggstate;
      LLVMValueRef v_setoff, v_transno;

      LLVMValueRef v_aggstatep;
      LLVMValueRef v_allpergroupsp;

      LLVMValueRef v_transnull;
      LLVMValueRef v_pergroupp;

      int jumpnull = op->d.agg_strict_trans_check.jumpnull;

      aggstate = op->d.agg_strict_trans_check.aggstate;
      v_aggstatep = l_ptr_const(aggstate, l_ptr(StructAggState));

         
                                             
                                               
                                               
         
      v_allpergroupsp = l_load_struct_gep(b, v_aggstatep, FIELDNO_AGGSTATE_ALL_PERGROUPS, "aggstate.all_pergroups");
      v_setoff = l_int32_const(op->d.agg_strict_trans_check.setoff);
      v_transno = l_int32_const(op->d.agg_strict_trans_check.transno);
      v_pergroupp = LLVMBuildGEP(b, l_load_gep1(b, v_allpergroupsp, v_setoff, ""), &v_transno, 1, "");

      v_transnull = l_load_struct_gep(b, v_pergroupp, FIELDNO_AGGSTATEPERGROUPDATA_TRANSVALUEISNULL, "transnull");

      LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_transnull, l_sbool_const(1), ""), opblocks[jumpnull], opblocks[i + 1]);

      break;
    }

    case EEOP_AGG_PLAIN_TRANS_BYVAL:
    case EEOP_AGG_PLAIN_TRANS:
    {
      AggState *aggstate;
      AggStatePerTrans pertrans;
      FunctionCallInfo fcinfo;

      LLVMValueRef v_aggstatep;
      LLVMValueRef v_fcinfo;
      LLVMValueRef v_fcinfo_isnull;

      LLVMValueRef v_transvaluep;
      LLVMValueRef v_transnullp;

      LLVMValueRef v_setoff;
      LLVMValueRef v_transno;

      LLVMValueRef v_aggcontext;

      LLVMValueRef v_allpergroupsp;
      LLVMValueRef v_current_setp;
      LLVMValueRef v_current_pertransp;
      LLVMValueRef v_curaggcontext;

      LLVMValueRef v_pertransp;

      LLVMValueRef v_pergroupp;

      LLVMValueRef v_retval;

      LLVMValueRef v_tmpcontext;
      LLVMValueRef v_oldcontext;

      aggstate = op->d.agg_trans.aggstate;
      pertrans = op->d.agg_trans.pertrans;

      fcinfo = pertrans->transfn_fcinfo;

      v_aggstatep = l_ptr_const(aggstate, l_ptr(StructAggState));
      v_pertransp = l_ptr_const(pertrans, l_ptr(StructAggStatePerTransData));

         
                                             
                                               
                                               
         
      v_allpergroupsp = l_load_struct_gep(b, v_aggstatep, FIELDNO_AGGSTATE_ALL_PERGROUPS, "aggstate.all_pergroups");
      v_setoff = l_int32_const(op->d.agg_trans.setoff);
      v_transno = l_int32_const(op->d.agg_trans.transno);
      v_pergroupp = LLVMBuildGEP(b, l_load_gep1(b, v_allpergroupsp, v_setoff, ""), &v_transno, 1, "");

      v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));
      v_aggcontext = l_ptr_const(op->d.agg_trans.aggcontext, l_ptr(StructExprContext));

      v_current_setp = LLVMBuildStructGEP(b, v_aggstatep, FIELDNO_AGGSTATE_CURRENT_SET, "aggstate.current_set");
      v_curaggcontext = LLVMBuildStructGEP(b, v_aggstatep, FIELDNO_AGGSTATE_CURAGGCONTEXT, "aggstate.curaggcontext");
      v_current_pertransp = LLVMBuildStructGEP(b, v_aggstatep, FIELDNO_AGGSTATE_CURPERTRANS, "aggstate.curpertrans");

                                
      LLVMBuildStore(b, v_aggcontext, v_curaggcontext);
      LLVMBuildStore(b, l_int32_const(op->d.agg_trans.setno), v_current_setp);
      LLVMBuildStore(b, v_pertransp, v_current_pertransp);

                                                           
      v_tmpcontext = l_ptr_const(aggstate->tmpcontext->ecxt_per_tuple_memory, l_ptr(StructMemoryContextData));
      v_oldcontext = l_mcxt_switch(mod, b, v_tmpcontext);

                                               
      v_transvaluep = LLVMBuildStructGEP(b, v_pergroupp, FIELDNO_AGGSTATEPERGROUPDATA_TRANSVALUE, "transvalue");
      v_transnullp = LLVMBuildStructGEP(b, v_pergroupp, FIELDNO_AGGSTATEPERGROUPDATA_TRANSVALUEISNULL, "transnullp");
      LLVMBuildStore(b, LLVMBuildLoad(b, v_transvaluep, "transvalue"), l_funcvaluep(b, v_fcinfo, 0));
      LLVMBuildStore(b, LLVMBuildLoad(b, v_transnullp, "transnull"), l_funcnullp(b, v_fcinfo, 0));

                                          
      v_retval = BuildV1Call(context, b, mod, fcinfo, &v_fcinfo_isnull);

         
                                                                
                                                           
                                                                 
                                                           
                                                            
                                                                 
                             
         
      if (opcode == EEOP_AGG_PLAIN_TRANS)
      {
        LLVMBasicBlockRef b_call;
        LLVMBasicBlockRef b_nocall;
        LLVMValueRef v_fn;
        LLVMValueRef v_transvalue;
        LLVMValueRef v_transnull;
        LLVMValueRef v_newval;
        LLVMValueRef params[6];

        b_call = l_bb_before_v(opblocks[i + 1], "op.%d.transcall", i);
        b_nocall = l_bb_before_v(opblocks[i + 1], "op.%d.transnocall", i);

        v_transvalue = LLVMBuildLoad(b, v_transvaluep, "");
        v_transnull = LLVMBuildLoad(b, v_transnullp, "");

           
                                      
                                                  
           
        LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntEQ, v_transvalue, v_retval, ""), b_nocall, b_call);

                                                       
        LLVMPositionBuilderAtEnd(b, b_call);

        params[0] = v_aggstatep;
        params[1] = v_pertransp;
        params[2] = v_retval;
        params[3] = LLVMBuildTrunc(b, v_fcinfo_isnull, TypeParamBool, "");
        params[4] = v_transvalue;
        params[5] = LLVMBuildTrunc(b, v_transnull, TypeParamBool, "");

        v_fn = llvm_get_decl(mod, FuncExecAggTransReparent);
        v_newval = LLVMBuildCall(b, v_fn, params, lengthof(params), "");

                               
        LLVMBuildStore(b, v_newval, v_transvaluep);
        LLVMBuildStore(b, v_fcinfo_isnull, v_transnullp);

        l_mcxt_switch(mod, b, v_oldcontext);
        LLVMBuildBr(b, opblocks[i + 1]);

                                                              
        LLVMPositionBuilderAtEnd(b, b_nocall);
      }

                             
      LLVMBuildStore(b, v_retval, v_transvaluep);
      LLVMBuildStore(b, v_fcinfo_isnull, v_transnullp);

      l_mcxt_switch(mod, b, v_oldcontext);

      LLVMBuildBr(b, opblocks[i + 1]);
      break;
    }

    case EEOP_AGG_ORDERED_TRANS_DATUM:
      build_EvalXFunc(b, mod, "ExecEvalAggOrderedTransDatum", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_AGG_ORDERED_TRANS_TUPLE:
      build_EvalXFunc(b, mod, "ExecEvalAggOrderedTransTuple", v_state, v_econtext, op);
      LLVMBuildBr(b, opblocks[i + 1]);
      break;

    case EEOP_LAST:
      Assert(false);
      break;
    }
  }

  LLVMDisposeBuilder(b);

     
                                                                       
                                                                    
                                                                    
                         
     
  {

    CompiledExprState *cstate = palloc0(sizeof(CompiledExprState));

    cstate->context = context;
    cstate->funcname = funcname;

    state->evalfunc = ExecRunCompiledExpr;
    state->evalfunc_private = cstate;
  }

  llvm_leave_fatal_on_oom();

  INSTR_TIME_SET_CURRENT(endtime);
  INSTR_TIME_ACCUM_DIFF(context->base.instr.generation_counter, endtime, starttime);

  return true;
}

   
                            
   
                                                                            
                                                                              
                                                                         
                                                        
   
static Datum
ExecRunCompiledExpr(ExprState *state, ExprContext *econtext, bool *isNull)
{
  CompiledExprState *cstate = state->evalfunc_private;
  ExprStateEvalFunc func;

  CheckExprStillValid(state, econtext);

  llvm_enter_fatal_on_oom();
  func = (ExprStateEvalFunc)llvm_get_function(cstate->context, cstate->funcname);
  llvm_leave_fatal_on_oom();
  Assert(func);

                                                             
  state->evalfunc = func;

  return func(state, econtext, isNull);
}

static LLVMValueRef
BuildV1Call(LLVMJitContext *context, LLVMBuilderRef b, LLVMModuleRef mod, FunctionCallInfo fcinfo, LLVMValueRef *v_fcinfo_isnull)
{
  LLVMValueRef v_fn;
  LLVMValueRef v_fcinfo_isnullp;
  LLVMValueRef v_retval;
  LLVMValueRef v_fcinfo;

  v_fn = llvm_function_reference(context, b, mod, fcinfo);

  v_fcinfo = l_ptr_const(fcinfo, l_ptr(StructFunctionCallInfoData));
  v_fcinfo_isnullp = LLVMBuildStructGEP(b, v_fcinfo, FIELDNO_FUNCTIONCALLINFODATA_ISNULL, "v_fcinfo_isnull");
  LLVMBuildStore(b, l_sbool_const(0), v_fcinfo_isnullp);

  v_retval = LLVMBuildCall(b, v_fn, &v_fcinfo, 1, "funccall");

  if (v_fcinfo_isnull)
  {
    *v_fcinfo_isnull = LLVMBuildLoad(b, v_fcinfo_isnullp, "");
  }

     
                                                                         
                                                             
     
  {
    LLVMValueRef v_lifetime = create_LifetimeEnd(mod);
    LLVMValueRef params[2];

    params[0] = l_int64_const(sizeof(NullableDatum) * fcinfo->nargs);
    params[1] = l_ptr_const(fcinfo->args, l_ptr(LLVMInt8Type()));
    LLVMBuildCall(b, v_lifetime, params, lengthof(params), "");

    params[0] = l_int64_const(sizeof(fcinfo->isnull));
    params[1] = l_ptr_const(&fcinfo->isnull, l_ptr(LLVMInt8Type()));
    LLVMBuildCall(b, v_lifetime, params, lengthof(params), "");
  }

  return v_retval;
}

   
                                                                  
   
static void
build_EvalXFunc(LLVMBuilderRef b, LLVMModuleRef mod, const char *funcname, LLVMValueRef v_state, LLVMValueRef v_econtext, ExprEvalStep *op)
{
  LLVMTypeRef sig;
  LLVMValueRef v_fn;
  LLVMTypeRef param_types[3];
  LLVMValueRef params[3];

  v_fn = LLVMGetNamedFunction(mod, funcname);
  if (!v_fn)
  {
    param_types[0] = l_ptr(StructExprState);
    param_types[1] = l_ptr(StructExprEvalStep);
    param_types[2] = l_ptr(StructExprContext);

    sig = LLVMFunctionType(LLVMVoidType(), param_types, lengthof(param_types), false);
    v_fn = LLVMAddFunction(mod, funcname, sig);
  }

  params[0] = v_state;
  params[1] = l_ptr_const(op, l_ptr(StructExprEvalStep));
  params[2] = v_econtext;

  LLVMBuildCall(b, v_fn, params, lengthof(params), "");
}

static LLVMValueRef
create_LifetimeEnd(LLVMModuleRef mod)
{
  LLVMTypeRef sig;
  LLVMValueRef fn;
  LLVMTypeRef param_types[2];

                                               
#if LLVM_VERSION_MAJOR < 5
  const char *nm = "llvm.lifetime.end";
#else
  const char *nm = "llvm.lifetime.end.p0i8";
#endif

  fn = LLVMGetNamedFunction(mod, nm);
  if (fn)
  {
    return fn;
  }

  param_types[0] = LLVMInt64Type();
  param_types[1] = l_ptr(LLVMInt8Type());

  sig = LLVMFunctionType(LLVMVoidType(), param_types, lengthof(param_types), false);
  fn = LLVMAddFunction(mod, nm, sig);

  LLVMSetFunctionCallConv(fn, LLVMCCallConv);

  Assert(LLVMGetIntrinsicID(fn));

  return fn;
}
