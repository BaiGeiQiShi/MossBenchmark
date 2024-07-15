                                                                            
   
                
                                      
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

   
                                                                              
                                                                          
                                                                           
   
                                                                        
                                                                       
                                                                          
   
                                                                          
                                                                           
                             
   

                                                                  
#define MAX_CACHED_PATH_LEN 16

typedef struct OprCacheKey
{
  char oprname[NAMEDATALEN];
  Oid left_arg;                                         
  Oid right_arg;                                          
  Oid search_path[MAX_CACHED_PATH_LEN];
} OprCacheKey;

typedef struct OprCacheEntry
{
                                         
  OprCacheKey key;

  Oid opr_oid;                                   
} OprCacheEntry;

static Oid
binary_oper_exact(List *opname, Oid arg1, Oid arg2);
static FuncDetailCode
oper_select_candidate(int nargs, Oid *input_typeids, FuncCandidateList candidates, Oid *operOid);
static const char *
op_signature_string(List *op, char oprkind, Oid arg1, Oid arg2);
static void
op_error(ParseState *pstate, List *op, char oprkind, Oid arg1, Oid arg2, FuncDetailCode fdresult, int location);
static bool
make_oper_cache_key(ParseState *pstate, OprCacheKey *key, List *opname, Oid ltypeId, Oid rtypeId, int location);
static Oid
find_oper_cache_entry(OprCacheKey *key);
static void
make_oper_cache_entry(OprCacheKey *key, Oid opr_oid);
static void
InvalidateOprCacheCallBack(Datum arg, int cacheid, uint32 hashvalue);

   
                  
                                                                        
                          
   
                                                                        
                 
   
                                                                             
                          
   
                                                                          
                                                                         
                                                  
   
Oid
LookupOperName(ParseState *pstate, List *opername, Oid oprleft, Oid oprright, bool noError, int location)
{
  Oid result;

  result = OpernameGetOprid(opername, oprleft, oprright);
  if (OidIsValid(result))
  {
    return result;
  }

                                                                        
  if (!noError)
  {
    char oprkind;

    if (!OidIsValid(oprleft))
    {
      oprkind = 'l';
    }
    else if (!OidIsValid(oprright))
    {
      oprkind = 'r';
    }
    else
    {
      oprkind = 'b';
    }

    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator does not exist: %s", op_signature_string(opername, oprkind, oprleft, oprright)), parser_errposition(pstate, location)));
  }

  return InvalidOid;
}

   
                      
                                                                 
                          
   
Oid
LookupOperWithArgs(ObjectWithArgs *oper, bool noError)
{
  TypeName *oprleft, *oprright;
  Oid leftoid, rightoid;

  Assert(list_length(oper->objargs) == 2);
  oprleft = linitial(oper->objargs);
  oprright = lsecond(oper->objargs);

  if (oprleft == NULL)
  {
    leftoid = InvalidOid;
  }
  else
  {
    leftoid = LookupTypeNameOid(NULL, oprleft, noError);
  }

  if (oprright == NULL)
  {
    rightoid = InvalidOid;
  }
  else
  {
    rightoid = LookupTypeNameOid(NULL, oprright, noError);
  }

  return LookupOperName(NULL, oper->objname, leftoid, rightoid, noError, -1);
}

   
                                                                              
   
                                                                         
                                                                            
                                                                       
                                                                          
                                                                          
   
                                                                        
                                                           
   
                                                                           
                                         
   
                                                                          
   
                                                                              
                                                                             
                   
   
void
get_sort_group_operators(Oid argtype, bool needLT, bool needEQ, bool needGT, Oid *ltOpr, Oid *eqOpr, Oid *gtOpr, bool *isHashable)
{
  TypeCacheEntry *typentry;
  int cache_flags;
  Oid lt_opr;
  Oid eq_opr;
  Oid gt_opr;
  bool hashable;

     
                                                 
     
                                                                            
                                                     
     
  if (isHashable != NULL)
  {
    cache_flags = TYPECACHE_LT_OPR | TYPECACHE_EQ_OPR | TYPECACHE_GT_OPR | TYPECACHE_HASH_PROC;
  }
  else
  {
    cache_flags = TYPECACHE_LT_OPR | TYPECACHE_EQ_OPR | TYPECACHE_GT_OPR;
  }

  typentry = lookup_type_cache(argtype, cache_flags);
  lt_opr = typentry->lt_opr;
  eq_opr = typentry->eq_opr;
  gt_opr = typentry->gt_opr;
  hashable = OidIsValid(typentry->hash_proc);

                               
  if ((needLT && !OidIsValid(lt_opr)) || (needGT && !OidIsValid(gt_opr)))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify an ordering operator for type %s", format_type_be(argtype)), errhint("Use an explicit ordering operator or modify the query.")));
  }
  if (needEQ && !OidIsValid(eq_opr))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify an equality operator for type %s", format_type_be(argtype))));
  }

                                
  if (ltOpr)
  {
    *ltOpr = lt_opr;
  }
  if (eqOpr)
  {
    *eqOpr = eq_opr;
  }
  if (gtOpr)
  {
    *gtOpr = gt_opr;
  }
  if (isHashable)
  {
    *isHashable = hashable;
  }
}

                                                   
Oid
oprid(Operator op)
{
  return ((Form_pg_operator)GETSTRUCT(op))->oid;
}

                                                                
Oid
oprfuncid(Operator op)
{
  Form_pg_operator pgopform = (Form_pg_operator)GETSTRUCT(op);

  return pgopform->oprcode;
}

                       
                                                              
   
                                                                         
                                                                        
                                                                         
                                                         
   
static Oid
binary_oper_exact(List *opname, Oid arg1, Oid arg2)
{
  Oid result;
  bool was_unknown = false;

                                                                     
  if ((arg1 == UNKNOWNOID) && (arg2 != InvalidOid))
  {
    arg1 = arg2;
    was_unknown = true;
  }
  else if ((arg2 == UNKNOWNOID) && (arg1 != InvalidOid))
  {
    arg2 = arg1;
    was_unknown = true;
  }

  result = OpernameGetOprid(opname, arg1, arg2);
  if (OidIsValid(result))
  {
    return result;
  }

  if (was_unknown)
  {
                                                                 
    Oid basetype = getBaseType(arg1);

    if (basetype != arg1)
    {
      result = OpernameGetOprid(opname, basetype, basetype);
      if (OidIsValid(result))
      {
        return result;
      }
    }
  }

  return InvalidOid;
}

                           
                                                             
                                                       
   
                                                                           
                                                                            
   
                                                                          
                                                                               
                         
   
static FuncDetailCode
oper_select_candidate(int nargs, Oid *input_typeids, FuncCandidateList candidates, Oid *operOid)                      
{
  int ncandidates;

     
                                                                       
                                             
     
  ncandidates = func_match_argtypes(nargs, input_typeids, candidates, &candidates);

                                                           
  if (ncandidates == 0)
  {
    *operOid = InvalidOid;
    return FUNCDETAIL_NOTFOUND;
  }
  if (ncandidates == 1)
  {
    *operOid = candidates->oid;
    return FUNCDETAIL_NORMAL;
  }

     
                                                                       
               
     
  candidates = func_select_candidate(nargs, input_typeids, candidates);

  if (candidates)
  {
    *operOid = candidates->oid;
    return FUNCDETAIL_NORMAL;
  }

  *operOid = InvalidOid;
  return FUNCDETAIL_MULTIPLE;                                        
}

                                          
                                                                    
   
                                                                    
                                                                     
                                                                       
   
                                                                  
                                                                               
                                                      
   
                                                                          
                                                       
   
Operator
oper(ParseState *pstate, List *opname, Oid ltypeId, Oid rtypeId, bool noError, int location)
{
  Oid operOid;
  OprCacheKey key;
  bool key_ok;
  FuncDetailCode fdresult = FUNCDETAIL_NOTFOUND;
  HeapTuple tup = NULL;

     
                                                     
     
  key_ok = make_oper_cache_key(pstate, &key, opname, ltypeId, rtypeId, location);

  if (key_ok)
  {
    operOid = find_oper_cache_entry(&key);
    if (OidIsValid(operOid))
    {
      tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
      if (HeapTupleIsValid(tup))
      {
        return (Operator)tup;
      }
    }
  }

     
                                     
     
  operOid = binary_oper_exact(opname, ltypeId, rtypeId);
  if (!OidIsValid(operOid))
  {
       
                                                          
       
    FuncCandidateList clist;

                                            
    clist = OpernameGetCandidates(opname, 'b', false);

                                          
    if (clist != NULL)
    {
         
                                                                       
                                           
         
      Oid inputOids[2];

      if (rtypeId == InvalidOid)
      {
        rtypeId = ltypeId;
      }
      else if (ltypeId == InvalidOid)
      {
        ltypeId = rtypeId;
      }
      inputOids[0] = ltypeId;
      inputOids[1] = rtypeId;
      fdresult = oper_select_candidate(2, inputOids, clist, &operOid);
    }
  }

  if (OidIsValid(operOid))
  {
    tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
  }

  if (HeapTupleIsValid(tup))
  {
    if (key_ok)
    {
      make_oper_cache_entry(&key, operOid);
    }
  }
  else if (!noError)
  {
    op_error(pstate, opname, 'b', ltypeId, rtypeId, fdresult, location);
  }

  return (Operator)tup;
}

                     
                                                                          
   
                                                                           
                                                                             
                                                          
   
Operator
compatible_oper(ParseState *pstate, List *op, Oid arg1, Oid arg2, bool noError, int location)
{
  Operator optup;
  Form_pg_operator opform;

                                                 
  optup = oper(pstate, op, arg1, arg2, noError, location);
  if (optup == (Operator)NULL)
  {
    return (Operator)NULL;                           
  }

                              
  opform = (Form_pg_operator)GETSTRUCT(optup);
  if (IsBinaryCoercible(arg1, opform->oprleft) && IsBinaryCoercible(arg2, opform->oprright))
  {
    return optup;
  }

               
  ReleaseSysCache(optup);

  if (!noError)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator requires run-time type coercion: %s", op_signature_string(op, 'b', arg1, arg2)), parser_errposition(pstate, location)));
  }

  return (Operator)NULL;
}

                                                          
   
                                                                     
                                                                        
                                     
   
Oid
compatible_oper_opid(List *op, Oid arg1, Oid arg2, bool noError)
{
  Operator optup;
  Oid result;

  optup = compatible_oper(NULL, op, arg1, arg2, noError, -1);
  if (optup != NULL)
  {
    result = oprid(optup);
    ReleaseSysCache(optup);
    return result;
  }
  return InvalidOid;
}

                                                                        
                                                            
   
                                                                    
                                                                    
                                                  
   
                                                                  
                                                                               
                                                      
   
                                                                          
                                                       
   
Operator
right_oper(ParseState *pstate, List *op, Oid arg, bool noError, int location)
{
  Oid operOid;
  OprCacheKey key;
  bool key_ok;
  FuncDetailCode fdresult = FUNCDETAIL_NOTFOUND;
  HeapTuple tup = NULL;

     
                                                     
     
  key_ok = make_oper_cache_key(pstate, &key, op, arg, InvalidOid, location);

  if (key_ok)
  {
    operOid = find_oper_cache_entry(&key);
    if (OidIsValid(operOid))
    {
      tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
      if (HeapTupleIsValid(tup))
      {
        return (Operator)tup;
      }
    }
  }

     
                                     
     
  operOid = OpernameGetOprid(op, arg, InvalidOid);
  if (!OidIsValid(operOid))
  {
       
                                                          
       
    FuncCandidateList clist;

                                             
    clist = OpernameGetCandidates(op, 'r', false);

                                          
    if (clist != NULL)
    {
         
                                                                       
                                                                         
         
      fdresult = oper_select_candidate(1, &arg, clist, &operOid);
    }
  }

  if (OidIsValid(operOid))
  {
    tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
  }

  if (HeapTupleIsValid(tup))
  {
    if (key_ok)
    {
      make_oper_cache_entry(&key, operOid);
    }
  }
  else if (!noError)
  {
    op_error(pstate, op, 'r', arg, InvalidOid, fdresult, location);
  }

  return (Operator)tup;
}

                                                                     
                                                            
   
                                                                    
                                                                    
                                                  
   
                                                                  
                                                                               
                                                      
   
                                                                          
                                                       
   
Operator
left_oper(ParseState *pstate, List *op, Oid arg, bool noError, int location)
{
  Oid operOid;
  OprCacheKey key;
  bool key_ok;
  FuncDetailCode fdresult = FUNCDETAIL_NOTFOUND;
  HeapTuple tup = NULL;

     
                                                     
     
  key_ok = make_oper_cache_key(pstate, &key, op, InvalidOid, arg, location);

  if (key_ok)
  {
    operOid = find_oper_cache_entry(&key);
    if (OidIsValid(operOid))
    {
      tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
      if (HeapTupleIsValid(tup))
      {
        return (Operator)tup;
      }
    }
  }

     
                                     
     
  operOid = OpernameGetOprid(op, InvalidOid, arg);
  if (!OidIsValid(operOid))
  {
       
                                                          
       
    FuncCandidateList clist;

                                            
    clist = OpernameGetCandidates(op, 'l', false);

                                          
    if (clist != NULL)
    {
         
                                                                        
                                                                        
                                                                    
         
      FuncCandidateList clisti;

      for (clisti = clist; clisti != NULL; clisti = clisti->next)
      {
        clisti->args[0] = clisti->args[1];
      }

         
                                                                       
                                                                         
         
      fdresult = oper_select_candidate(1, &arg, clist, &operOid);
    }
  }

  if (OidIsValid(operOid))
  {
    tup = SearchSysCache1(OPEROID, ObjectIdGetDatum(operOid));
  }

  if (HeapTupleIsValid(tup))
  {
    if (key_ok)
    {
      make_oper_cache_entry(&key, operOid);
    }
  }
  else if (!noError)
  {
    op_error(pstate, op, 'l', InvalidOid, arg, fdresult, location);
  }

  return (Operator)tup;
}

   
                       
                                                                         
                                                      
   
                                                                          
             
   
static const char *
op_signature_string(List *op, char oprkind, Oid arg1, Oid arg2)
{
  StringInfoData argbuf;

  initStringInfo(&argbuf);

  if (oprkind != 'l')
  {
    appendStringInfo(&argbuf, "%s ", format_type_be(arg1));
  }

  appendStringInfoString(&argbuf, NameListToString(op));

  if (oprkind != 'r')
  {
    appendStringInfo(&argbuf, " %s", format_type_be(arg2));
  }

  return argbuf.data;                                    
}

   
                                                                         
   
static void
op_error(ParseState *pstate, List *op, char oprkind, Oid arg1, Oid arg2, FuncDetailCode fdresult, int location)
{
  if (fdresult == FUNCDETAIL_MULTIPLE)
  {
    ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("operator is not unique: %s", op_signature_string(op, oprkind, arg1, arg2)),
                       errhint("Could not choose a best candidate operator. "
                               "You might need to add explicit type casts."),
                       parser_errposition(pstate, location)));
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator does not exist: %s", op_signature_string(op, oprkind, arg1, arg2)),
                       (!arg1 || !arg2) ? errhint("No operator matches the given name and argument type. "
                                                  "You might need to add an explicit type cast.")
                                        : errhint("No operator matches the given name and argument types. "
                                                  "You might need to add explicit type casts."),
                       parser_errposition(pstate, location)));
  }
}

   
             
                                      
   
                                                              
                                               
   
                                                                       
                                                                              
                                                                            
                                                                    
   
Expr *
make_op(ParseState *pstate, List *opname, Node *ltree, Node *rtree, Node *last_srf, int location)
{
  Oid ltypeId, rtypeId;
  Operator tup;
  Form_pg_operator opform;
  Oid actual_arg_types[2];
  Oid declared_arg_types[2];
  int nargs;
  List *args;
  Oid rettype;
  OpExpr *result;

                           
  if (rtree == NULL)
  {
                        
    ltypeId = exprType(ltree);
    rtypeId = InvalidOid;
    tup = right_oper(pstate, opname, ltypeId, false, location);
  }
  else if (ltree == NULL)
  {
                       
    rtypeId = exprType(rtree);
    ltypeId = InvalidOid;
    tup = left_oper(pstate, opname, rtypeId, false, location);
  }
  else
  {
                                    
    ltypeId = exprType(ltree);
    rtypeId = exprType(rtree);
    tup = oper(pstate, opname, ltypeId, rtypeId, false, location);
  }

  opform = (Form_pg_operator)GETSTRUCT(tup);

                              
  if (!RegProcedureIsValid(opform->oprcode))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator is only a shell: %s", op_signature_string(opname, opform->oprkind, opform->oprleft, opform->oprright)), parser_errposition(pstate, location)));
  }

                                                    
  if (rtree == NULL)
  {
                        
    args = list_make1(ltree);
    actual_arg_types[0] = ltypeId;
    declared_arg_types[0] = opform->oprleft;
    nargs = 1;
  }
  else if (ltree == NULL)
  {
                       
    args = list_make1(rtree);
    actual_arg_types[0] = rtypeId;
    declared_arg_types[0] = opform->oprright;
    nargs = 1;
  }
  else
  {
                                    
    args = list_make2(ltree, rtree);
    actual_arg_types[0] = ltypeId;
    actual_arg_types[1] = rtypeId;
    declared_arg_types[0] = opform->oprleft;
    declared_arg_types[1] = opform->oprright;
    nargs = 2;
  }

     
                                                                     
                                                                         
                                                        
     
  rettype = enforce_generic_type_consistency(actual_arg_types, declared_arg_types, nargs, opform->oprresult, false);

                                                      
  make_fn_arguments(pstate, args, actual_arg_types, declared_arg_types);

                                     
  result = makeNode(OpExpr);
  result->opno = oprid(tup);
  result->opfuncid = opform->oprcode;
  result->opresulttype = rettype;
  result->opretset = get_func_retset(opform->oprcode);
                                                               
  result->args = args;
  result->location = location;

                                            
  if (result->opretset)
  {
    check_srf_call_placement(pstate, last_srf, location);
                                                               
    pstate->p_last_srf = (Node *)result;
  }

  ReleaseSysCache(tup);

  return (Expr *)result;
}

   
                          
                                                                     
   
Expr *
make_scalar_array_op(ParseState *pstate, List *opname, bool useOr, Node *ltree, Node *rtree, int location)
{
  Oid ltypeId, rtypeId, atypeId, res_atypeId;
  Operator tup;
  Form_pg_operator opform;
  Oid actual_arg_types[2];
  Oid declared_arg_types[2];
  List *args;
  Oid rettype;
  ScalarArrayOpExpr *result;

  ltypeId = exprType(ltree);
  atypeId = exprType(rtree);

     
                                                                          
                                                                          
                                                                 
     
  if (atypeId == UNKNOWNOID)
  {
    rtypeId = UNKNOWNOID;
  }
  else
  {
    rtypeId = get_base_element_type(atypeId);
    if (!OidIsValid(rtypeId))
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("op ANY/ALL (array) requires array on right side"), parser_errposition(pstate, location)));
    }
  }

                                
  tup = oper(pstate, opname, ltypeId, rtypeId, false, location);
  opform = (Form_pg_operator)GETSTRUCT(tup);

                              
  if (!RegProcedureIsValid(opform->oprcode))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator is only a shell: %s", op_signature_string(opname, opform->oprkind, opform->oprleft, opform->oprright)), parser_errposition(pstate, location)));
  }

  args = list_make2(ltree, rtree);
  actual_arg_types[0] = ltypeId;
  actual_arg_types[1] = rtypeId;
  declared_arg_types[0] = opform->oprleft;
  declared_arg_types[1] = opform->oprright;

     
                                                                     
                                                                         
                                                        
     
  rettype = enforce_generic_type_consistency(actual_arg_types, declared_arg_types, 2, opform->oprresult, false);

     
                                           
     
  if (rettype != BOOLOID)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("op ANY/ALL (array) requires operator to yield boolean"), parser_errposition(pstate, location)));
  }
  if (get_func_retset(opform->oprcode))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("op ANY/ALL (array) requires operator not to return a set"), parser_errposition(pstate, location)));
  }

     
                                                                       
                                                                       
                                                                     
                                       
     
  if (IsPolymorphicType(declared_arg_types[1]))
  {
                                            
    res_atypeId = atypeId;
  }
  else
  {
    res_atypeId = get_array_type(declared_arg_types[1]);
    if (!OidIsValid(res_atypeId))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(declared_arg_types[1])), parser_errposition(pstate, location)));
    }
  }
  actual_arg_types[1] = atypeId;
  declared_arg_types[1] = res_atypeId;

                                                      
  make_fn_arguments(pstate, args, actual_arg_types, declared_arg_types);

                                     
  result = makeNode(ScalarArrayOpExpr);
  result->opno = oprid(tup);
  result->opfuncid = opform->oprcode;
  result->useOr = useOr;
                                                  
  result->args = args;
  result->location = location;

  ReleaseSysCache(tup);

  return (Expr *)result;
}

   
                                                                         
                                          
   
                                                                           
                                                                              
                                                                              
                                                                               
                                                                             
                                                                        
                                                                           
                                                                          
                                                                            
                                                                         
   
                                                                         
                                                                           
                                                                   
   
                                                                            
                                                                          
                                                                        
                                                                        
                                              
   

                                  
static HTAB *OprCacheHash = NULL;

   
                       
                                                                  
   
                                                                   
                                   
   
                                                                            
                     
   
static bool
make_oper_cache_key(ParseState *pstate, OprCacheKey *key, List *opname, Oid ltypeId, Oid rtypeId, int location)
{
  char *schemaname;
  char *opername;

                                 
  DeconstructQualifiedName(opname, &schemaname, &opername);

                                           
  MemSet(key, 0, sizeof(OprCacheKey));

                                                   
  strlcpy(key->oprname, opername, NAMEDATALEN);
  key->left_arg = ltypeId;
  key->right_arg = rtypeId;

  if (schemaname)
  {
    ParseCallbackState pcbstate;

                                           
    setup_parser_errposition_callback(&pcbstate, pstate, location);
    key->search_path[0] = LookupExplicitNamespace(schemaname, false);
    cancel_parser_errposition_callback(&pcbstate);
  }
  else
  {
                                    
    if (fetch_search_path_array(key->search_path, MAX_CACHED_PATH_LEN) > MAX_CACHED_PATH_LEN)
    {
      return false;                       
    }
  }

  return true;
}

   
                         
   
                                                                        
                                                   
   
static Oid
find_oper_cache_entry(OprCacheKey *key)
{
  OprCacheEntry *oprentry;

  if (OprCacheHash == NULL)
  {
                                                       
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(OprCacheKey);
    ctl.entrysize = sizeof(OprCacheEntry);
    OprCacheHash = hash_create("Operator lookup cache", 256, &ctl, HASH_ELEM | HASH_BLOBS);

                                                                   
    CacheRegisterSyscacheCallback(OPERNAMENSP, InvalidateOprCacheCallBack, (Datum)0);
    CacheRegisterSyscacheCallback(CASTSOURCETARGET, InvalidateOprCacheCallBack, (Datum)0);
  }

                                  
  oprentry = (OprCacheEntry *)hash_search(OprCacheHash, (void *)key, HASH_FIND, NULL);
  if (oprentry == NULL)
  {
    return InvalidOid;
  }

  return oprentry->opr_oid;
}

   
                         
   
                                           
   
static void
make_oper_cache_entry(OprCacheKey *key, Oid opr_oid)
{
  OprCacheEntry *oprentry;

  Assert(OprCacheHash != NULL);

  oprentry = (OprCacheEntry *)hash_search(OprCacheHash, (void *)key, HASH_ENTER, NULL);
  oprentry->opr_oid = opr_oid;
}

   
                                                     
   
static void
InvalidateOprCacheCallBack(Datum arg, int cacheid, uint32 hashvalue)
{
  HASH_SEQ_STATUS status;
  OprCacheEntry *hentry;

  Assert(OprCacheHash != NULL);

                                                                   
  hash_seq_init(&status, OprCacheHash);

  while ((hentry = (OprCacheEntry *)hash_seq_search(&status)) != NULL)
  {
    if (hash_search(OprCacheHash, (void *)&hentry->key, HASH_REMOVE, NULL) == NULL)
    {
      elog(ERROR, "hash table corrupted");
    }
  }
}
