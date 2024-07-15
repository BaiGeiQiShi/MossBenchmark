                                                                            
   
                
                                                     
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "utils/builtins.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/varbit.h"

static void
pcb_error_callback(void *arg);

   
                   
                                              
   
                                                                          
   
ParseState *
make_parsestate(ParseState *parentParseState)
{
  ParseState *pstate;

  pstate = palloc0(sizeof(ParseState));

  pstate->parentParseState = parentParseState;

                                                          
  pstate->p_next_resno = 1;
  pstate->p_resolve_unknowns = true;

  if (parentParseState)
  {
    pstate->p_sourcetext = parentParseState->p_sourcetext;
                                          
    pstate->p_pre_columnref_hook = parentParseState->p_pre_columnref_hook;
    pstate->p_post_columnref_hook = parentParseState->p_post_columnref_hook;
    pstate->p_paramref_hook = parentParseState->p_paramref_hook;
    pstate->p_coerce_param_hook = parentParseState->p_coerce_param_hook;
    pstate->p_ref_hook_state = parentParseState->p_ref_hook_state;
                                                                         
    pstate->p_queryEnv = parentParseState->p_queryEnv;
  }

  return pstate;
}

   
                   
                                                       
   
void
free_parsestate(ParseState *pstate)
{
     
                                                                         
                                                                         
                                                                 
     
  if (pstate->p_next_resno - 1 > MaxTupleAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("target lists can have at most %d entries", MaxTupleAttributeNumber)));
  }

  if (pstate->p_target_relation != NULL)
  {
    table_close(pstate->p_target_relation, NoLock);
  }

  pfree(pstate);
}

   
                      
                                                               
   
                                                                           
                                   
   
                                                                           
                                                                               
                                                                            
                                                                         
                                          
   
int
parser_errposition(ParseState *pstate, int location)
{
  int pos;

                                          
  if (location < 0)
  {
    return 0;
  }
                                                         
  if (pstate == NULL || pstate->p_sourcetext == NULL)
  {
    return 0;
  }
                                          
  pos = pg_mbstrlen_with_len(pstate->p_sourcetext, location) + 1;
                                            
  return errposition(pos);
}

   
                                     
                                                              
   
                                                                       
                                                                       
                                                                       
                                                                       
                                                    
   
                                                           
        
                                                                    
                                          
                                                   
   
void
setup_parser_errposition_callback(ParseCallbackState *pcbstate, ParseState *pstate, int location)
{
                                                   
  pcbstate->pstate = pstate;
  pcbstate->location = location;
  pcbstate->errcallback.callback = pcb_error_callback;
  pcbstate->errcallback.arg = (void *)pcbstate;
  pcbstate->errcallback.previous = error_context_stack;
  error_context_stack = &pcbstate->errcallback;
}

   
                                                    
   
void
cancel_parser_errposition_callback(ParseCallbackState *pcbstate)
{
                                   
  error_context_stack = pcbstate->errcallback.previous;
}

   
                                                               
   
                                                                     
                                                                           
                                                                           
   
static void
pcb_error_callback(void *arg)
{
  ParseCallbackState *pcbstate = (ParseCallbackState *)arg;

  if (geterrcode() != ERRCODE_QUERY_CANCELED)
  {
    (void)parser_errposition(pcbstate->pstate, pcbstate->location);
  }
}

   
            
                                                                   
   
Var *
make_var(ParseState *pstate, RangeTblEntry *rte, int attrno, int location)
{
  Var *result;
  int vnum, sublevels_up;
  Oid vartypeid;
  int32 type_mod;
  Oid varcollid;

  vnum = RTERangeTablePosn(pstate, rte, &sublevels_up);
  get_rte_attribute_type(rte, attrno, &vartypeid, &type_mod, &varcollid);
  result = makeVar(vnum, attrno, vartypeid, type_mod, varcollid, sublevels_up);
  result->location = location;
  return result;
}

   
                            
                                                                          
   
   
                                                                                
                                                                            
                                                                       
                                                                                
                  
   
Oid
transformContainerType(Oid *containerType, int32 *containerTypmod)
{
  Oid origContainerType = *containerType;
  Oid elementType;
  HeapTuple type_tuple_container;
  Form_pg_type type_struct_container;

     
                                                                          
                                                                        
                                                                          
                                                                             
                                                                             
     
  *containerType = getBaseTypeAndTypmod(*containerType, containerTypmod);

     
                                                                          
                                                                             
                                                                  
                                                                             
                                                                           
     
  if (*containerType == INT2VECTOROID)
  {
    *containerType = INT2ARRAYOID;
  }
  else if (*containerType == OIDVECTOROID)
  {
    *containerType = OIDARRAYOID;
  }

                                            
  type_tuple_container = SearchSysCache1(TYPEOID, ObjectIdGetDatum(*containerType));
  if (!HeapTupleIsValid(type_tuple_container))
  {
    elog(ERROR, "cache lookup failed for type %u", *containerType);
  }
  type_struct_container = (Form_pg_type)GETSTRUCT(type_tuple_container);

                                                              

  elementType = type_struct_container->typelem;
  if (elementType == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("cannot subscript type %s because it is not an array", format_type_be(origContainerType))));
  }

  ReleaseSysCache(type_tuple_container);

  return elementType;
}

   
                                  
                                                                          
                                              
   
                                                                              
                                                                             
                                 
   
                                                                                
                                                                              
                                                                               
                                                                       
   
                                                                           
                                                                          
                                                                        
                                                                     
   
                        
                                                                             
                                                                   
                                                             
                    
                                                            
                                                                 
                                                                          
                 
                                                                   
                                                                         
               
   
SubscriptingRef *
transformContainerSubscripts(ParseState *pstate, Node *containerBase, Oid containerType, Oid elementType, int32 containerTypMod, List *indirection, Node *assignFrom)
{
  bool isSlice = false;
  List *upperIndexpr = NIL;
  List *lowerIndexpr = NIL;
  ListCell *idx;
  SubscriptingRef *sbsref;

     
                                                                         
                                                                            
                                                                        
     
  if (!OidIsValid(elementType))
  {
    elementType = transformContainerType(&containerType, &containerTypMod);
  }

     
                                                                           
                                                                            
                                                                          
                                                                           
                                                                             
                                                                      
     
  foreach (idx, indirection)
  {
    A_Indices *ai = (A_Indices *)lfirst(idx);

    if (ai->is_slice)
    {
      isSlice = true;
      break;
    }
  }

     
                                          
     
  foreach (idx, indirection)
  {
    A_Indices *ai = lfirst_node(A_Indices, idx);
    Node *subexpr;

    if (isSlice)
    {
      if (ai->lidx)
      {
        subexpr = transformExpr(pstate, ai->lidx, pstate->p_expr_kind);
                                                     
        subexpr = coerce_to_target_type(pstate, subexpr, exprType(subexpr), INT4OID, -1, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
        if (subexpr == NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("array subscript must have type integer"), parser_errposition(pstate, exprLocation(ai->lidx))));
        }
      }
      else if (!ai->is_slice)
      {
                               
        subexpr = (Node *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), Int32GetDatum(1), false, true);                    
      }
      else
      {
                                                                    
        subexpr = NULL;
      }
      lowerIndexpr = lappend(lowerIndexpr, subexpr);
    }
    else
    {
      Assert(ai->lidx == NULL && !ai->is_slice);
    }

    if (ai->uidx)
    {
      subexpr = transformExpr(pstate, ai->uidx, pstate->p_expr_kind);
                                                   
      subexpr = coerce_to_target_type(pstate, subexpr, exprType(subexpr), INT4OID, -1, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
      if (subexpr == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("array subscript must have type integer"), parser_errposition(pstate, exprLocation(ai->uidx))));
      }
    }
    else
    {
                                                                  
      Assert(isSlice && ai->is_slice);
      subexpr = NULL;
    }
    upperIndexpr = lappend(upperIndexpr, subexpr);
  }

     
                                                                         
                                                                          
     
  if (assignFrom != NULL)
  {
    Oid typesource = exprType(assignFrom);
    Oid typeneeded = isSlice ? containerType : elementType;
    Node *newFrom;

    newFrom = coerce_to_target_type(pstate, assignFrom, typesource, typeneeded, containerTypMod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (newFrom == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                         errmsg("array assignment requires type %s"
                                " but expression is of type %s",
                             format_type_be(typeneeded), format_type_be(typesource)),
                         errhint("You will need to rewrite or cast the expression."), parser_errposition(pstate, exprLocation(assignFrom))));
    }
    assignFrom = newFrom;
  }

     
                                              
     
  sbsref = (SubscriptingRef *)makeNode(SubscriptingRef);
  if (assignFrom != NULL)
  {
    sbsref->refassgnexpr = (Expr *)assignFrom;
  }

  sbsref->refcontainertype = containerType;
  sbsref->refelemtype = elementType;
  sbsref->reftypmod = containerTypMod;
                                                
  sbsref->refupperindexpr = upperIndexpr;
  sbsref->reflowerindexpr = lowerIndexpr;
  sbsref->refexpr = (Expr *)containerBase;
  sbsref->refassgnexpr = (Expr *)assignFrom;

  return sbsref;
}

   
              
   
                                                                     
                                                                      
                                                                    
                                      
   
                                                                        
                                                                         
                                                                         
                                                        
   
                                                                       
                                                                    
                                                                     
                                          
   
Const *
make_const(ParseState *pstate, Value *value, int location)
{
  Const *con;
  Datum val;
  int64 val64;
  Oid typeid;
  int typelen;
  bool typebyval;
  ParseCallbackState pcbstate;

  switch (nodeTag(value))
  {
  case T_Integer:
    val = Int32GetDatum(intVal(value));

    typeid = INT4OID;
    typelen = sizeof(int32);
    typebyval = true;
    break;

  case T_Float:
                                                             
    if (scanint8(strVal(value), true, &val64))
    {
         
                                                                   
                                                                   
         
      int32 val32 = (int32)val64;

      if (val64 == (int64)val32)
      {
        val = Int32GetDatum(val32);

        typeid = INT4OID;
        typelen = sizeof(int32);
        typebyval = true;
      }
      else
      {
        val = Int64GetDatum(val64);

        typeid = INT8OID;
        typelen = sizeof(int64);
        typebyval = FLOAT8PASSBYVAL;                            
      }
    }
    else
    {
                                                            
      setup_parser_errposition_callback(&pcbstate, pstate, location);
      val = DirectFunctionCall3(numeric_in, CStringGetDatum(strVal(value)), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
      cancel_parser_errposition_callback(&pcbstate);

      typeid = NUMERICOID;
      typelen = -1;                   
      typebyval = false;
    }
    break;

  case T_String:

       
                                                                    
                       
       
    val = CStringGetDatum(strVal(value));

    typeid = UNKNOWNOID;                            
    typelen = -2;                                         
    typebyval = false;
    break;

  case T_BitString:
                                                      
    setup_parser_errposition_callback(&pcbstate, pstate, location);
    val = DirectFunctionCall3(bit_in, CStringGetDatum(strVal(value)), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
    cancel_parser_errposition_callback(&pcbstate);
    typeid = BITOID;
    typelen = -1;
    typebyval = false;
    break;

  case T_Null:
                             
    con = makeConst(UNKNOWNOID, -1, InvalidOid, -2, (Datum)0, true, false);
    con->location = location;
    return con;

  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(value));
    return NULL;                          
  }

  con = makeConst(typeid, -1,                                    
      InvalidOid,                                                   
      typelen, val, false, typebyval);
  con->location = location;

  return con;
}
