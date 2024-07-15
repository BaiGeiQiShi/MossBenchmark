                                                                            
   
                    
                                               
   
                                                                            
                                                                             
                              
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include <llvm-c/Core.h>

#include "access/htup_details.h"
#include "access/tupdesc_details.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"
#include "executor/tuptable.h"
#include "jit/llvmjit.h"
#include "jit/llvmjit_emit.h"

   
                                                                     
                                                                          
                                                                         
                                                      
   
#define ATTNOTNULL(att) ((att)->attnotnull && !(((att)->attrelid == SubscriptionRelationId && (att)->attnum == Anum_pg_subscription_subslotname) || ((att)->attrelid == SubscriptionRelRelationId && (att)->attnum == Anum_pg_subscription_rel_srsublsn)))

   
                                                                            
   
LLVMValueRef
slot_compile_deform(LLVMJitContext *context, TupleDesc desc, const TupleTableSlotOps *ops, int natts)
{
  char *funcname;

  LLVMModuleRef mod;
  LLVMBuilderRef b;

  LLVMTypeRef deform_sig;
  LLVMValueRef v_deform_fn;

  LLVMBasicBlockRef b_entry;
  LLVMBasicBlockRef b_adjust_unavail_cols;
  LLVMBasicBlockRef b_find_start;

  LLVMBasicBlockRef b_out;
  LLVMBasicBlockRef b_dead;
  LLVMBasicBlockRef *attcheckattnoblocks;
  LLVMBasicBlockRef *attstartblocks;
  LLVMBasicBlockRef *attisnullblocks;
  LLVMBasicBlockRef *attcheckalignblocks;
  LLVMBasicBlockRef *attalignblocks;
  LLVMBasicBlockRef *attstoreblocks;

  LLVMValueRef v_offp;

  LLVMValueRef v_tupdata_base;
  LLVMValueRef v_tts_values;
  LLVMValueRef v_tts_nulls;
  LLVMValueRef v_slotoffp;
  LLVMValueRef v_flagsp;
  LLVMValueRef v_nvalidp;
  LLVMValueRef v_nvalid;
  LLVMValueRef v_maxatt;

  LLVMValueRef v_slot;

  LLVMValueRef v_tupleheaderp;
  LLVMValueRef v_tuplep;
  LLVMValueRef v_infomask1;
  LLVMValueRef v_infomask2;
  LLVMValueRef v_bits;

  LLVMValueRef v_hoff;

  LLVMValueRef v_hasnulls;

                                                   
  int guaranteed_column_number = -1;

                               
  int known_alignment = 0;

                                                                    
  bool attguaranteedalign = true;

  int attnum;

                                                                   
  if (ops == &TTSOpsVirtual)
  {
    return NULL;
  }

                                                             
  if (ops != &TTSOpsHeapTuple && ops != &TTSOpsBufferHeapTuple && ops != &TTSOpsMinimalTuple)
  {
    return NULL;
  }

  mod = llvm_mutable_module(context);

  funcname = llvm_expand_funcname(context, "deform");

     
                                                                            
                          
     
  for (attnum = 0; attnum < desc->natts; attnum++)
  {
    Form_pg_attribute att = TupleDescAttr(desc, attnum);

       
                                                                           
                                                                    
                                                                           
                                                                        
                                   
       
                                                                 
                                                                       
              
       
    if (ATTNOTNULL(att) && !att->atthasmissing && !att->attisdropped)
    {
      guaranteed_column_number = attnum;
    }
  }

                                         
  {
    LLVMTypeRef param_types[1];

    param_types[0] = l_ptr(StructTupleTableSlot);

    deform_sig = LLVMFunctionType(LLVMVoidType(), param_types, lengthof(param_types), 0);
  }
  v_deform_fn = LLVMAddFunction(mod, funcname, deform_sig);
  LLVMSetLinkage(v_deform_fn, LLVMInternalLinkage);
  LLVMSetParamAlignment(LLVMGetParam(v_deform_fn, 0), MAXIMUM_ALIGNOF);
  llvm_copy_attributes(AttributeTemplate, v_deform_fn);

  b_entry = LLVMAppendBasicBlock(v_deform_fn, "entry");
  b_adjust_unavail_cols = LLVMAppendBasicBlock(v_deform_fn, "adjust_unavail_cols");
  b_find_start = LLVMAppendBasicBlock(v_deform_fn, "find_startblock");
  b_out = LLVMAppendBasicBlock(v_deform_fn, "outblock");
  b_dead = LLVMAppendBasicBlock(v_deform_fn, "deadblock");

  b = LLVMCreateBuilder();

  attcheckattnoblocks = palloc(sizeof(LLVMBasicBlockRef) * natts);
  attstartblocks = palloc(sizeof(LLVMBasicBlockRef) * natts);
  attisnullblocks = palloc(sizeof(LLVMBasicBlockRef) * natts);
  attcheckalignblocks = palloc(sizeof(LLVMBasicBlockRef) * natts);
  attalignblocks = palloc(sizeof(LLVMBasicBlockRef) * natts);
  attstoreblocks = palloc(sizeof(LLVMBasicBlockRef) * natts);

  known_alignment = 0;

  LLVMPositionBuilderAtEnd(b, b_entry);

                                                                    
  v_offp = LLVMBuildAlloca(b, TypeSizeT, "v_offp");

  v_slot = LLVMGetParam(v_deform_fn, 0);

  v_tts_values = l_load_struct_gep(b, v_slot, FIELDNO_TUPLETABLESLOT_VALUES, "tts_values");
  v_tts_nulls = l_load_struct_gep(b, v_slot, FIELDNO_TUPLETABLESLOT_ISNULL, "tts_ISNULL");
  v_flagsp = LLVMBuildStructGEP(b, v_slot, FIELDNO_TUPLETABLESLOT_FLAGS, "");
  v_nvalidp = LLVMBuildStructGEP(b, v_slot, FIELDNO_TUPLETABLESLOT_NVALID, "");

  if (ops == &TTSOpsHeapTuple || ops == &TTSOpsBufferHeapTuple)
  {
    LLVMValueRef v_heapslot;

    v_heapslot = LLVMBuildBitCast(b, v_slot, l_ptr(StructHeapTupleTableSlot), "heapslot");
    v_slotoffp = LLVMBuildStructGEP(b, v_heapslot, FIELDNO_HEAPTUPLETABLESLOT_OFF, "");
    v_tupleheaderp = l_load_struct_gep(b, v_heapslot, FIELDNO_HEAPTUPLETABLESLOT_TUPLE, "tupleheader");
  }
  else if (ops == &TTSOpsMinimalTuple)
  {
    LLVMValueRef v_minimalslot;

    v_minimalslot = LLVMBuildBitCast(b, v_slot, l_ptr(StructMinimalTupleTableSlot), "minimalslot");
    v_slotoffp = LLVMBuildStructGEP(b, v_minimalslot, FIELDNO_MINIMALTUPLETABLESLOT_OFF, "");
    v_tupleheaderp = l_load_struct_gep(b, v_minimalslot, FIELDNO_MINIMALTUPLETABLESLOT_TUPLE, "tupleheader");
  }
  else
  {
                                                         
    pg_unreachable();
  }

  v_tuplep = l_load_struct_gep(b, v_tupleheaderp, FIELDNO_HEAPTUPLEDATA_DATA, "tuple");
  v_bits = LLVMBuildBitCast(b, LLVMBuildStructGEP(b, v_tuplep, FIELDNO_HEAPTUPLEHEADERDATA_BITS, ""), l_ptr(LLVMInt8Type()), "t_bits");
  v_infomask1 = l_load_struct_gep(b, v_tuplep, FIELDNO_HEAPTUPLEHEADERDATA_INFOMASK, "infomask1");
  v_infomask2 = l_load_struct_gep(b, v_tuplep, FIELDNO_HEAPTUPLEHEADERDATA_INFOMASK2, "infomask2");

                                 
  v_hasnulls = LLVMBuildICmp(b, LLVMIntNE, LLVMBuildAnd(b, l_int16_const(HEAP_HASNULL), v_infomask1, ""), l_int16_const(0), "hasnulls");

                                     
  v_maxatt = LLVMBuildAnd(b, l_int16_const(HEAP_NATTS_MASK), v_infomask2, "maxatt");

     
                                                                           
                                                                
     
  v_hoff = LLVMBuildZExt(b, l_load_struct_gep(b, v_tuplep, FIELDNO_HEAPTUPLEHEADERDATA_HOFF, ""), LLVMInt32Type(), "t_hoff");

  v_tupdata_base = LLVMBuildGEP(b, LLVMBuildBitCast(b, v_tuplep, l_ptr(LLVMInt8Type()), ""), &v_hoff, 1, "v_tupdata_base");

     
                                                                            
                                           
     
  {
    LLVMValueRef v_off_start;

    v_off_start = LLVMBuildLoad(b, v_slotoffp, "v_slot_off");
    v_off_start = LLVMBuildZExt(b, v_off_start, TypeSizeT, "");
    LLVMBuildStore(b, v_off_start, v_offp);
  }

                                                                          
  for (attnum = 0; attnum < natts; attnum++)
  {
    attcheckattnoblocks[attnum] = l_bb_append_v(v_deform_fn, "block.attr.%d.attcheckattno", attnum);
    attstartblocks[attnum] = l_bb_append_v(v_deform_fn, "block.attr.%d.start", attnum);
    attisnullblocks[attnum] = l_bb_append_v(v_deform_fn, "block.attr.%d.attisnull", attnum);
    attcheckalignblocks[attnum] = l_bb_append_v(v_deform_fn, "block.attr.%d.attcheckalign", attnum);
    attalignblocks[attnum] = l_bb_append_v(v_deform_fn, "block.attr.%d.align", attnum);
    attstoreblocks[attnum] = l_bb_append_v(v_deform_fn, "block.attr.%d.store", attnum);
  }

     
                                                                             
                                                                        
                                                                           
                                                                             
                                                                      
                                            
     
  if ((natts - 1) <= guaranteed_column_number)
  {
                                              
    LLVMBuildBr(b, b_adjust_unavail_cols);
    LLVMPositionBuilderAtEnd(b, b_adjust_unavail_cols);
    LLVMBuildBr(b, b_find_start);
  }
  else
  {
    LLVMValueRef v_params[3];

                                             
    LLVMBuildCondBr(b, LLVMBuildICmp(b, LLVMIntULT, v_maxatt, l_int16_const(natts), ""), b_adjust_unavail_cols, b_find_start);

                                                            
    LLVMPositionBuilderAtEnd(b, b_adjust_unavail_cols);

    v_params[0] = v_slot;
    v_params[1] = LLVMBuildZExt(b, v_maxatt, LLVMInt32Type(), "");
    v_params[2] = l_int32_const(natts);
    LLVMBuildCall(b, llvm_get_decl(mod, FuncSlotGetmissingattrs), v_params, lengthof(v_params), "");
    LLVMBuildBr(b, b_find_start);
  }

  LLVMPositionBuilderAtEnd(b, b_find_start);

  v_nvalid = LLVMBuildLoad(b, v_nvalidp, "");

     
                                                                      
                                                                             
                                                                           
             
     
  if (true)
  {
    LLVMValueRef v_switch = LLVMBuildSwitch(b, v_nvalid, b_dead, natts);

    for (attnum = 0; attnum < natts; attnum++)
    {
      LLVMValueRef v_attno = l_int16_const(attnum);

      LLVMAddCase(v_switch, v_attno, attcheckattnoblocks[attnum]);
    }
  }
  else
  {
                                              
    LLVMBuildBr(b, attcheckattnoblocks[0]);
  }

  LLVMPositionBuilderAtEnd(b, b_dead);
  LLVMBuildUnreachable(b);

     
                                                                          
                
     
  for (attnum = 0; attnum < natts; attnum++)
  {
    Form_pg_attribute att = TupleDescAttr(desc, attnum);
    LLVMValueRef v_incby;
    int alignto;
    LLVMValueRef l_attno = l_int16_const(attnum);
    LLVMValueRef v_attdatap;
    LLVMValueRef v_resultp;

                                                                          
    LLVMPositionBuilderAtEnd(b, attcheckattnoblocks[attnum]);

       
                                                                         
                                                                    
       
    if (attnum == 0)
    {
      LLVMBuildStore(b, l_sizet_const(0), v_offp);
    }

       
                                                                           
                                                                     
                                             
       
    if (attnum <= guaranteed_column_number)
    {
      LLVMBuildBr(b, attstartblocks[attnum]);
    }
    else
    {
      LLVMValueRef v_islast;

      v_islast = LLVMBuildICmp(b, LLVMIntUGE, l_attno, v_maxatt, "heap_natts");
      LLVMBuildCondBr(b, v_islast, b_out, attstartblocks[attnum]);
    }
    LLVMPositionBuilderAtEnd(b, attstartblocks[attnum]);

       
                                                                        
                                                                      
                                                                     
       
    if (!ATTNOTNULL(att))
    {
      LLVMBasicBlockRef b_ifnotnull;
      LLVMBasicBlockRef b_ifnull;
      LLVMBasicBlockRef b_next;
      LLVMValueRef v_attisnull;
      LLVMValueRef v_nullbyteno;
      LLVMValueRef v_nullbytemask;
      LLVMValueRef v_nullbyte;
      LLVMValueRef v_nullbit;

      b_ifnotnull = attcheckalignblocks[attnum];
      b_ifnull = attisnullblocks[attnum];

      if (attnum + 1 == natts)
      {
        b_next = b_out;
      }
      else
      {
        b_next = attcheckattnoblocks[attnum + 1];
      }

      v_nullbyteno = l_int32_const(attnum >> 3);
      v_nullbytemask = l_int8_const(1 << ((attnum) & 0x07));
      v_nullbyte = l_load_gep1(b, v_bits, v_nullbyteno, "attnullbyte");

      v_nullbit = LLVMBuildICmp(b, LLVMIntEQ, LLVMBuildAnd(b, v_nullbyte, v_nullbytemask, ""), l_int8_const(0), "attisnull");

      v_attisnull = LLVMBuildAnd(b, v_hasnulls, v_nullbit, "");

      LLVMBuildCondBr(b, v_attisnull, b_ifnull, b_ifnotnull);

      LLVMPositionBuilderAtEnd(b, b_ifnull);

                           
      LLVMBuildStore(b, l_int8_const(1), LLVMBuildGEP(b, v_tts_nulls, &l_attno, 1, ""));
                            
      LLVMBuildStore(b, l_sizet_const(0), LLVMBuildGEP(b, v_tts_values, &l_attno, 1, ""));

      LLVMBuildBr(b, b_next);
      attguaranteedalign = false;
    }
    else
    {
                         
      LLVMBuildBr(b, attcheckalignblocks[attnum]);
      LLVMPositionBuilderAtEnd(b, attisnullblocks[attnum]);
      LLVMBuildBr(b, attcheckalignblocks[attnum]);
    }
    LLVMPositionBuilderAtEnd(b, attcheckalignblocks[attnum]);

                                      
    if (att->attalign == 'i')
    {
      alignto = ALIGNOF_INT;
    }
    else if (att->attalign == 'c')
    {
      alignto = 1;
    }
    else if (att->attalign == 'd')
    {
      alignto = ALIGNOF_DOUBLE;
    }
    else if (att->attalign == 's')
    {
      alignto = ALIGNOF_SHORT;
    }
    else
    {
      elog(ERROR, "unknown alignment");
      alignto = 0;
    }

              
                                                                       
                    
                                                  
                                                                   
                                                                           
                                            
              
       
    if (alignto > 1 && (known_alignment < 0 || known_alignment != TYPEALIGN(alignto, known_alignment)))
    {
         
                                                                        
                                                                        
                                                                     
                                                                         
                                                                       
                                                                      
                                                         
         
      if (att->attlen == -1)
      {
        LLVMValueRef v_possible_padbyte;
        LLVMValueRef v_ispad;
        LLVMValueRef v_off;

                                                
        attguaranteedalign = false;

        v_off = LLVMBuildLoad(b, v_offp, "");

        v_possible_padbyte = l_load_gep1(b, v_tupdata_base, v_off, "padbyte");
        v_ispad = LLVMBuildICmp(b, LLVMIntEQ, v_possible_padbyte, l_int8_const(0), "ispadbyte");
        LLVMBuildCondBr(b, v_ispad, attalignblocks[attnum], attstoreblocks[attnum]);
      }
      else
      {
        LLVMBuildBr(b, attalignblocks[attnum]);
      }

      LLVMPositionBuilderAtEnd(b, attalignblocks[attnum]);

                                                          
      {
        LLVMValueRef v_off_aligned;
        LLVMValueRef v_off = LLVMBuildLoad(b, v_offp, "");

                              
        LLVMValueRef v_alignval = l_sizet_const(alignto - 1);

                                                    
        LLVMValueRef v_lh = LLVMBuildAdd(b, v_off, v_alignval, "");

                                             
        LLVMValueRef v_rh = l_sizet_const(~(alignto - 1));

        v_off_aligned = LLVMBuildAnd(b, v_lh, v_rh, "aligned_offset");

        LLVMBuildStore(b, v_off_aligned, v_offp);
      }

         
                                                                       
                                                                        
                                                            
         
      if (known_alignment >= 0)
      {
        Assert(known_alignment != 0);
        known_alignment = TYPEALIGN(alignto, known_alignment);
      }

      LLVMBuildBr(b, attstoreblocks[attnum]);
      LLVMPositionBuilderAtEnd(b, attstoreblocks[attnum]);
    }
    else
    {
      LLVMPositionBuilderAtEnd(b, attcheckalignblocks[attnum]);
      LLVMBuildBr(b, attalignblocks[attnum]);
      LLVMPositionBuilderAtEnd(b, attalignblocks[attnum]);
      LLVMBuildBr(b, attstoreblocks[attnum]);
    }
    LLVMPositionBuilderAtEnd(b, attstoreblocks[attnum]);

       
                                                                          
                                                                        
                                                                    
                        
       
    if (attguaranteedalign)
    {
      Assert(known_alignment >= 0);
      LLVMBuildStore(b, l_sizet_const(known_alignment), v_offp);
    }

                                                       
    if (att->attlen < 0)
    {
                                                                     
      known_alignment = -1;
      attguaranteedalign = false;
    }
    else if (ATTNOTNULL(att) && attguaranteedalign && known_alignment >= 0)
    {
         
                                                                        
                                                                  
                                               
         
      Assert(att->attlen > 0);
      known_alignment += att->attlen;
    }
    else if (ATTNOTNULL(att) && (att->attlen % alignto) == 0)
    {
         
                                                                     
                                                                      
                                                                       
         
      Assert(att->attlen > 0);
      known_alignment = alignto;
      Assert(known_alignment > 0);
      attguaranteedalign = false;
    }
    else
    {
      known_alignment = -1;
      attguaranteedalign = false;
    }

                                           
    {
      LLVMValueRef v_off = LLVMBuildLoad(b, v_offp, "");

      v_attdatap = LLVMBuildGEP(b, v_tupdata_base, &v_off, 1, "");
    }

                                           
    v_resultp = LLVMBuildGEP(b, v_tts_values, &l_attno, 1, "");

                                 
    LLVMBuildStore(b, l_int8_const(0), LLVMBuildGEP(b, v_tts_nulls, &l_attno, 1, ""));

       
                                                                        
                                                                 
       
    if (att->attbyval)
    {
      LLVMValueRef v_tmp_loaddata;
      LLVMTypeRef vartypep = LLVMPointerType(LLVMIntType(att->attlen * 8), 0);

      v_tmp_loaddata = LLVMBuildPointerCast(b, v_attdatap, vartypep, "");
      v_tmp_loaddata = LLVMBuildLoad(b, v_tmp_loaddata, "attr_byval");
      v_tmp_loaddata = LLVMBuildZExt(b, v_tmp_loaddata, TypeSizeT, "");

      LLVMBuildStore(b, v_tmp_loaddata, v_resultp);
    }
    else
    {
      LLVMValueRef v_tmp_loaddata;

                         
      v_tmp_loaddata = LLVMBuildPtrToInt(b, v_attdatap, TypeSizeT, "attr_ptr");
      LLVMBuildStore(b, v_tmp_loaddata, v_resultp);
    }

                                
    if (att->attlen > 0)
    {
      v_incby = l_sizet_const(att->attlen);
    }
    else if (att->attlen == -1)
    {
      v_incby = LLVMBuildCall(b, llvm_get_decl(mod, FuncVarsizeAny), &v_attdatap, 1, "varsize_any");
      l_callsite_ro(v_incby);
      l_callsite_alwaysinline(v_incby);
    }
    else if (att->attlen == -2)
    {
      v_incby = LLVMBuildCall(b, llvm_get_decl(mod, FuncStrlen), &v_attdatap, 1, "strlen");

      l_callsite_ro(v_incby);

                              
      v_incby = LLVMBuildAdd(b, v_incby, l_sizet_const(1), "");
    }
    else
    {
      Assert(false);
      v_incby = NULL;                       
    }

    if (attguaranteedalign)
    {
      Assert(known_alignment >= 0);
      LLVMBuildStore(b, l_sizet_const(known_alignment), v_offp);
    }
    else
    {
      LLVMValueRef v_off = LLVMBuildLoad(b, v_offp, "");

      v_off = LLVMBuildAdd(b, v_off, v_incby, "increment_offset");
      LLVMBuildStore(b, v_off, v_offp);
    }

       
                                                                       
                                                 
       
    if (attnum + 1 == natts)
    {
                    
      LLVMBuildBr(b, b_out);
    }
    else
    {
      LLVMBuildBr(b, attcheckattnoblocks[attnum + 1]);
    }
  }

                                
  LLVMPositionBuilderAtEnd(b, b_out);

  {
    LLVMValueRef v_off = LLVMBuildLoad(b, v_offp, "");
    LLVMValueRef v_flags;

    LLVMBuildStore(b, l_int16_const(natts), v_nvalidp);
    v_off = LLVMBuildTrunc(b, v_off, LLVMInt32Type(), "");
    LLVMBuildStore(b, v_off, v_slotoffp);
    v_flags = LLVMBuildLoad(b, v_flagsp, "tts_flags");
    v_flags = LLVMBuildOr(b, v_flags, l_int16_const(TTS_FLAG_SLOW), "");
    LLVMBuildStore(b, v_flags, v_flagsp);
    LLVMBuildRetVoid(b);
  }

  LLVMDisposeBuilder(b);

  return v_deform_fn;
}
