                                                                            
   
                
                                    
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

                                                      
typedef enum
{
  FUNCLOOKUP_NOSUCHFUNC,
  FUNCLOOKUP_AMBIGUOUS
} FuncLookupError;

static void
unify_hypothetical_args(ParseState *pstate, List *fargs, int numAggregatedArgs, Oid *actual_arg_types, Oid *declared_arg_types);
static Oid
FuncNameAsType(List *funcname);
static Node *
ParseComplexProjection(ParseState *pstate, const char *funcname, Node *first_arg, int location);
static Oid
LookupFuncNameInternal(List *funcname, int nargs, const Oid *argtypes, bool missing_ok, FuncLookupError *lookupError);

   
                         
   
                                                                         
                                                                         
                                                                        
                                                                          
                                                                          
                                                                             
   
                                                                        
                                                           
   
                                                                           
                                                                    
                                                                          
                                      
   
                                                                
                                       
   
                                                                  
                                                           
   
                                                                       
                                                                        
                                                                   
   
                                                                         
                                                             
   
Node *
ParseFuncOrColumn(ParseState *pstate, List *funcname, List *fargs, Node *last_srf, FuncCall *fn, bool proc_call, int location)
{
  bool is_column = (fn == NULL);
  List *agg_order = (fn ? fn->agg_order : NIL);
  Expr *agg_filter = NULL;
  bool agg_within_group = (fn ? fn->agg_within_group : false);
  bool agg_star = (fn ? fn->agg_star : false);
  bool agg_distinct = (fn ? fn->agg_distinct : false);
  bool func_variadic = (fn ? fn->func_variadic : false);
  WindowDef *over = (fn ? fn->over : NULL);
  bool could_be_projection;
  Oid rettype;
  Oid funcid;
  ListCell *l;
  ListCell *nextl;
  Node *first_arg = NULL;
  int nargs;
  int nargsplusdefs;
  Oid actual_arg_types[FUNC_MAX_ARGS];
  Oid *declared_arg_types;
  List *argnames;
  List *argdefaults;
  Node *retval;
  bool retset;
  int nvargs;
  Oid vatype;
  FuncDetailCode fdresult;
  char aggkind = 0;
  ParseCallbackState pcbstate;

     
                                                                             
     
  if (fn && fn->agg_filter != NULL)
  {
    agg_filter = (Expr *)transformWhereClause(pstate, fn->agg_filter, EXPR_KIND_FILTER, "FILTER");
  }

     
                                                                            
                                                                          
                                                                          
                                
     
  if (list_length(fargs) > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("cannot pass more than %d argument to a function", "cannot pass more than %d arguments to a function", FUNC_MAX_ARGS, FUNC_MAX_ARGS), parser_errposition(pstate, location)));
  }

     
                                                               
     
                                                                           
                                                                             
                                                                         
                                                                             
                                                                            
                                                                           
                         
     
  nargs = 0;
  for (l = list_head(fargs); l != NULL; l = nextl)
  {
    Node *arg = lfirst(l);
    Oid argtype = exprType(arg);

    nextl = lnext(l);

    if (argtype == VOIDOID && IsA(arg, Param) && !is_column && !agg_within_group)
    {
      fargs = list_delete_ptr(fargs, arg);
      continue;
    }

    actual_arg_types[nargs++] = argtype;
  }

     
                                                                         
     
                                                                          
                                                                        
                                                                             
                                     
     
  argnames = NIL;
  foreach (l, fargs)
  {
    Node *arg = lfirst(l);

    if (IsA(arg, NamedArgExpr))
    {
      NamedArgExpr *na = (NamedArgExpr *)arg;
      ListCell *lc;

                                      
      foreach (lc, argnames)
      {
        if (strcmp(na->name, (char *)lfirst(lc)) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("argument name \"%s\" used more than once", na->name), parser_errposition(pstate, na->location)));
        }
      }
      argnames = lappend(argnames, na->name);
    }
    else
    {
      if (argnames != NIL)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("positional argument cannot follow named argument"), parser_errposition(pstate, exprLocation(arg))));
      }
    }
  }

  if (fargs)
  {
    first_arg = linitial(fargs);
    Assert(first_arg != NULL);
  }

     
                                                                             
                                                                         
                                                                            
                                                                      
                                                            
     
  could_be_projection = (nargs == 1 && !proc_call && agg_order == NIL && agg_filter == NULL && !agg_star && !agg_distinct && over == NULL && !func_variadic && argnames == NIL && list_length(funcname) == 1 && (actual_arg_types[0] == RECORDOID || ISCOMPLEX(actual_arg_types[0])));

     
                                                                    
     
  if (could_be_projection && is_column)
  {
    retval = ParseComplexProjection(pstate, strVal(linitial(funcname)), first_arg, location);
    if (retval)
    {
      return retval;
    }

       
                                                                       
                      
       
  }

     
                                                                 
                                                                        
                                                                    
                                                                          
                   
     
                                                                        
                                                                            
                                                                          
                                                                        
                                                                            
                             
     

  setup_parser_errposition_callback(&pcbstate, pstate, location);

  fdresult = func_get_detail(funcname, fargs, argnames, nargs, actual_arg_types, !func_variadic, true, &funcid, &rettype, &retset, &nvargs, &vatype, &declared_arg_types, &argdefaults);

  cancel_parser_errposition_callback(&pcbstate);

     
                                                    
     

                                                               
  if (proc_call && (fdresult == FUNCDETAIL_NORMAL || fdresult == FUNCDETAIL_AGGREGATE || fdresult == FUNCDETAIL_WINDOWFUNC || fdresult == FUNCDETAIL_COERCION))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s is not a procedure", func_signature_string(funcname, nargs, argnames, actual_arg_types)), errhint("To call a function, use SELECT."), parser_errposition(pstate, location)));
  }
                                                    
  if (fdresult == FUNCDETAIL_PROCEDURE && !proc_call)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s is a procedure", func_signature_string(funcname, nargs, argnames, actual_arg_types)), errhint("To call a procedure, use CALL."), parser_errposition(pstate, location)));
  }

  if (fdresult == FUNCDETAIL_NORMAL || fdresult == FUNCDETAIL_PROCEDURE || fdresult == FUNCDETAIL_COERCION)
  {
       
                                                                         
                                           
       
    if (agg_star)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s(*) specified, but %s is not an aggregate function", NameListToString(funcname), NameListToString(funcname)), parser_errposition(pstate, location)));
    }
    if (agg_distinct)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("DISTINCT specified, but %s is not an aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
    if (agg_within_group)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("WITHIN GROUP specified, but %s is not an aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
    if (agg_order != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("ORDER BY specified, but %s is not an aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
    if (agg_filter)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("FILTER specified, but %s is not an aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
    if (over)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("OVER specified, but %s is not a window function nor an aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
  }

     
                                                                   
     
  if (fdresult == FUNCDETAIL_NORMAL || fdresult == FUNCDETAIL_PROCEDURE)
  {
                                                
  }
  else if (fdresult == FUNCDETAIL_AGGREGATE)
  {
       
                                                                         
       
    HeapTuple tup;
    Form_pg_aggregate classForm;
    int catDirectArgs;

    tup = SearchSysCache1(AGGFNOID, ObjectIdGetDatum(funcid));
    if (!HeapTupleIsValid(tup))                        
    {
      elog(ERROR, "cache lookup failed for aggregate %u", funcid);
    }
    classForm = (Form_pg_aggregate)GETSTRUCT(tup);
    aggkind = classForm->aggkind;
    catDirectArgs = classForm->aggnumdirectargs;
    ReleaseSysCache(tup);

                                             
    if (AGGKIND_IS_ORDERED_SET(aggkind))
    {
      int numAggregatedArgs;
      int numDirectArgs;

      if (!agg_within_group)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("WITHIN GROUP is required for ordered-set aggregate %s", NameListToString(funcname)), parser_errposition(pstate, location)));
      }
      if (over)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("OVER is not supported for ordered-set aggregate %s", NameListToString(funcname)), parser_errposition(pstate, location)));
      }
                                                  
      Assert(!agg_distinct);
                                                  
      Assert(!func_variadic);

         
                                                                         
                                                                        
                                                                         
                                                                    
                                                                         
                                                                         
                                      
         
      numAggregatedArgs = list_length(agg_order);
      numDirectArgs = nargs - numAggregatedArgs;
      Assert(numDirectArgs >= 0);

      if (!OidIsValid(vatype))
      {
                                                        
        if (numDirectArgs != catDirectArgs)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)), errhint("There is an ordered-set aggregate %s, but it requires %d direct arguments, not %d.", NameListToString(funcname), catDirectArgs, numDirectArgs), parser_errposition(pstate, location)));
        }
      }
      else
      {
           
                                                                    
                                                                       
                                                                  
                                                                   
                                                               
           
        int pronargs;

        pronargs = nargs;
        if (nvargs > 1)
        {
          pronargs -= nvargs - 1;
        }
        if (catDirectArgs < pronargs)
        {
                                                                 
          if (numDirectArgs != catDirectArgs)
          {
            ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)), errhint("There is an ordered-set aggregate %s, but it requires %d direct arguments, not %d.", NameListToString(funcname), catDirectArgs, numDirectArgs), parser_errposition(pstate, location)));
          }
        }
        else
        {
             
                                                                     
                                                                     
                                                         
                                                            
                                                               
                                                                    
                                                
             
          if (aggkind == AGGKIND_HYPOTHETICAL)
          {
            if (nvargs != 2 * numAggregatedArgs)
            {
              ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)), errhint("To use the hypothetical-set aggregate %s, the number of hypothetical direct arguments (here %d) must match the number of ordering columns (here %d).", NameListToString(funcname), nvargs - numAggregatedArgs, numAggregatedArgs), parser_errposition(pstate, location)));
            }
          }
          else
          {
            if (nvargs <= numAggregatedArgs)
            {
              ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)), errhint("There is an ordered-set aggregate %s, but it requires at least %d direct arguments.", NameListToString(funcname), catDirectArgs), parser_errposition(pstate, location)));
            }
          }
        }
      }

                                                         
      if (aggkind == AGGKIND_HYPOTHETICAL)
      {
        unify_hypothetical_args(pstate, fargs, numAggregatedArgs, actual_arg_types, declared_arg_types);
      }
    }
    else
    {
                                                           
      if (agg_within_group)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s is not an ordered-set aggregate, so it cannot have WITHIN GROUP", NameListToString(funcname)), parser_errposition(pstate, location)));
      }
    }
  }
  else if (fdresult == FUNCDETAIL_WINDOWFUNC)
  {
       
                                                                      
       
    if (!over)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("window function %s requires an OVER clause", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
                                                   
    if (agg_within_group)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("window function %s cannot have WITHIN GROUP", NameListToString(funcname)), parser_errposition(pstate, location)));
    }
  }
  else if (fdresult == FUNCDETAIL_COERCION)
  {
       
                                                                          
                                       
       
    return coerce_type(pstate, linitial(fargs), actual_arg_types[0], rettype, -1, COERCION_EXPLICIT, COERCE_EXPLICIT_CALL, location);
  }
  else if (fdresult == FUNCDETAIL_MULTIPLE)
  {
       
                                                                         
                                                                          
                                                                      
                                                                    
                                                                 
       
    if (is_column)
    {
      return NULL;
    }

    if (proc_call)
    {
      ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("procedure %s is not unique", func_signature_string(funcname, nargs, argnames, actual_arg_types)),
                         errhint("Could not choose a best candidate procedure. "
                                 "You might need to add explicit type casts."),
                         parser_errposition(pstate, location)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("function %s is not unique", func_signature_string(funcname, nargs, argnames, actual_arg_types)),
                         errhint("Could not choose a best candidate function. "
                                 "You might need to add explicit type casts."),
                         parser_errposition(pstate, location)));
    }
  }
  else
  {
       
                                                                  
                                                                    
                                                         
       
    if (is_column)
    {
      return NULL;
    }

       
                                                                           
       
    if (could_be_projection)
    {
      retval = ParseComplexProjection(pstate, strVal(linitial(funcname)), first_arg, location);
      if (retval)
      {
        return retval;
      }
    }

       
                                                                    
                                                            
       
    if (list_length(agg_order) > 1 && !agg_within_group)
    {
                                                                    
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)),
                         errhint("No aggregate function matches the given name and argument types. "
                                 "Perhaps you misplaced ORDER BY; ORDER BY must appear "
                                 "after all regular arguments of the aggregate."),
                         parser_errposition(pstate, location)));
    }
    else if (proc_call)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("procedure %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)),
                         errhint("No procedure matches the given name and argument types. "
                                 "You might need to add explicit type casts."),
                         parser_errposition(pstate, location)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, argnames, actual_arg_types)),
                         errhint("No function matches the given name and argument types. "
                                 "You might need to add explicit type casts."),
                         parser_errposition(pstate, location)));
    }
  }

     
                                                                       
                                                                            
                                                                        
                                                                      
                                                               
     
  nargsplusdefs = nargs;
  foreach (l, argdefaults)
  {
    Node *expr = (Node *)lfirst(l);

                                       
    if (nargsplusdefs >= FUNC_MAX_ARGS)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("cannot pass more than %d argument to a function", "cannot pass more than %d arguments to a function", FUNC_MAX_ARGS, FUNC_MAX_ARGS), parser_errposition(pstate, location)));
    }

    actual_arg_types[nargsplusdefs++] = exprType(expr);
  }

     
                                                                     
                                                                         
                                                        
     
  rettype = enforce_generic_type_consistency(actual_arg_types, declared_arg_types, nargsplusdefs, rettype, false);

                                                      
  make_fn_arguments(pstate, fargs, actual_arg_types, declared_arg_types);

     
                                                                             
                                                                  
                                                       
     
  if (!OidIsValid(vatype))
  {
    Assert(nvargs == 0);
    func_variadic = false;
  }

     
                                                                           
                                                      
     
  if (nvargs > 0 && vatype != ANYOID)
  {
    ArrayExpr *newa = makeNode(ArrayExpr);
    int non_var_args = nargs - nvargs;
    List *vargs;

    Assert(non_var_args >= 0);
    vargs = list_copy_tail(fargs, non_var_args);
    fargs = list_truncate(fargs, non_var_args);

    newa->elements = vargs;
                                                                         
    newa->element_typeid = exprType((Node *)linitial(vargs));
    newa->array_typeid = get_array_type(newa->element_typeid);
    if (!OidIsValid(newa->array_typeid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("could not find array type for data type %s", format_type_be(newa->element_typeid)), parser_errposition(pstate, exprLocation((Node *)vargs))));
    }
                                                     
    newa->multidims = false;
    newa->location = exprLocation((Node *)vargs);

    fargs = lappend(fargs, newa);

                                                           
    Assert(!func_variadic);
                                           
    func_variadic = true;
  }

     
                                                                           
                                                        
     
  if (nargs > 0 && vatype == ANYOID && func_variadic)
  {
    Oid va_arr_typid = actual_arg_types[nargs - 1];

    if (!OidIsValid(get_base_element_type(va_arr_typid)))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("VARIADIC argument must be an array"), parser_errposition(pstate, exprLocation((Node *)llast(fargs)))));
    }
  }

                                            
  if (retset)
  {
    check_srf_call_placement(pstate, last_srf, location);
  }

                                              
  if (fdresult == FUNCDETAIL_NORMAL || fdresult == FUNCDETAIL_PROCEDURE)
  {
    FuncExpr *funcexpr = makeNode(FuncExpr);

    funcexpr->funcid = funcid;
    funcexpr->funcresulttype = rettype;
    funcexpr->funcretset = retset;
    funcexpr->funcvariadic = func_variadic;
    funcexpr->funcformat = COERCE_EXPLICIT_CALL;
                                                                   
    funcexpr->args = fargs;
    funcexpr->location = location;

    retval = (Node *)funcexpr;
  }
  else if (fdresult == FUNCDETAIL_AGGREGATE && !over)
  {
                            
    Aggref *aggref = makeNode(Aggref);

    aggref->aggfnoid = funcid;
    aggref->aggtype = rettype;
                                                                  
    aggref->aggtranstype = InvalidOid;                             
                                                           
                                                                      
                                                                        
    aggref->aggfilter = agg_filter;
    aggref->aggstar = agg_star;
    aggref->aggvariadic = func_variadic;
    aggref->aggkind = aggkind;
                                                           
    aggref->aggsplit = AGGSPLIT_SIMPLE;                                
    aggref->location = location;

       
                                                                    
                                                                  
       
    if (fargs == NIL && !agg_star && !agg_within_group)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s(*) must be used to call a parameterless aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }

    if (retset)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("aggregates cannot return sets"), parser_errposition(pstate, location)));
    }

       
                                                                           
                                                                           
                                                                        
                                                                          
                                                                         
                                                                        
                                            
       
    if (argnames != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("aggregates cannot use named arguments"), parser_errposition(pstate, location)));
    }

                                                                   
    transformAggregateCall(pstate, aggref, fargs, agg_order, agg_distinct);

    retval = (Node *)aggref;
  }
  else
  {
                         
    WindowFunc *wfunc = makeNode(WindowFunc);

    Assert(over);                                                  
    Assert(!agg_within_group);                         

    wfunc->winfnoid = funcid;
    wfunc->wintype = rettype;
                                                                  
    wfunc->args = fargs;
                                                       
    wfunc->winstar = agg_star;
    wfunc->winagg = (fdresult == FUNCDETAIL_AGGREGATE);
    wfunc->aggfilter = agg_filter;
    wfunc->location = location;

       
                                                                      
       
    if (agg_distinct)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DISTINCT is not implemented for window functions"), parser_errposition(pstate, location)));
    }

       
                                                                    
                                                                  
       
    if (wfunc->winagg && fargs == NIL && !agg_star)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s(*) must be used to call a parameterless aggregate function", NameListToString(funcname)), parser_errposition(pstate, location)));
    }

       
                                               
       
    if (agg_order != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("aggregate ORDER BY is not implemented for window functions"), parser_errposition(pstate, location)));
    }

       
                                                              
       
    if (!wfunc->winagg && agg_filter)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("FILTER is not implemented for non-aggregate window functions"), parser_errposition(pstate, location)));
    }

       
                                                         
       
    if (pstate->p_last_srf != last_srf)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("window function calls cannot contain set-returning function calls"), errhint("You might be able to move the set-returning function into a LATERAL FROM item."), parser_errposition(pstate, exprLocation(pstate->p_last_srf))));
    }

    if (retset)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("window functions cannot return sets"), parser_errposition(pstate, location)));
    }

                                                                     
    transformWindowFuncCall(pstate, wfunc, over);

    retval = (Node *)wfunc;
  }

                                                                          
  if (retset)
  {
    pstate->p_last_srf = retval;
  }

  return retval;
}

                         
   
                                                                         
                                                                             
                                                                             
                                                              
   
                                                                              
                                                                 
   
                                                                        
                                                                    
   
int
func_match_argtypes(int nargs, Oid *input_typeids, FuncCandidateList raw_candidates, FuncCandidateList *candidates)                   
{
  FuncCandidateList current_candidate;
  FuncCandidateList next_candidate;
  int ncandidates = 0;

  *candidates = NULL;

  for (current_candidate = raw_candidates; current_candidate != NULL; current_candidate = next_candidate)
  {
    next_candidate = current_candidate->next;
    if (can_coerce_type(nargs, input_typeids, current_candidate->args, COERCION_IMPLICIT))
    {
      current_candidate->next = *candidates;
      *candidates = current_candidate;
      ncandidates++;
    }
  }

  return ncandidates;
}                            

                           
                                                              
                                                       
   
                                                                   
                           
   
                                                                          
                                                                             
                                                                  
   
                                                                            
                                                                         
                                                                             
                                       
   
                 
   
                                                                      
                                                                     
                                                                         
                                                                
                                                                      
                                                                     
                                                                  
                       
   
                                                                    
                                                         
                       
   
                                                                         
                                                                 
             
   
                                                                          
                                                 
                                                                
   
                                                                     
                                                                       
                                                       
   
                                                                   
                                                                    
                                                                       
                                                                
                                                                   
                                                                       
                                                                       
                                                              
                                                                       
                                                                
             
   
FuncCandidateList
func_select_candidate(int nargs, Oid *input_typeids, FuncCandidateList candidates)
{
  FuncCandidateList current_candidate, first_candidate, last_candidate;
  Oid *current_typeids;
  Oid current_type;
  int i;
  int ncandidates;
  int nbestMatch, nmatch, nunknowns;
  Oid input_base_typeids[FUNC_MAX_ARGS];
  TYPCATEGORY slot_category[FUNC_MAX_ARGS], current_category;
  bool current_is_preferred;
  bool slot_has_preferred_type[FUNC_MAX_ARGS];
  bool resolved_unknowns;

                                       
  if (nargs > FUNC_MAX_ARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("cannot pass more than %d argument to a function", "cannot pass more than %d arguments to a function", FUNC_MAX_ARGS, FUNC_MAX_ARGS)));
  }

     
                                                                           
                                                                           
                                                                            
                                                                        
                                                                          
                                                                           
                                                                     
                                                                  
     
                                                                           
            
     
  nunknowns = 0;
  for (i = 0; i < nargs; i++)
  {
    if (input_typeids[i] != UNKNOWNOID)
    {
      input_base_typeids[i] = getBaseType(input_typeids[i]);
    }
    else
    {
                                                     
      input_base_typeids[i] = UNKNOWNOID;
      nunknowns++;
    }
  }

     
                                                                        
                                                     
     
  ncandidates = 0;
  nbestMatch = 0;
  last_candidate = NULL;
  for (current_candidate = candidates; current_candidate != NULL; current_candidate = current_candidate->next)
  {
    current_typeids = current_candidate->args;
    nmatch = 0;
    for (i = 0; i < nargs; i++)
    {
      if (input_base_typeids[i] != UNKNOWNOID && current_typeids[i] == input_base_typeids[i])
      {
        nmatch++;
      }
    }

                                                  
    if ((nmatch > nbestMatch) || (last_candidate == NULL))
    {
      nbestMatch = nmatch;
      candidates = current_candidate;
      last_candidate = current_candidate;
      ncandidates = 1;
    }
                                                              
    else if (nmatch == nbestMatch)
    {
      last_candidate->next = current_candidate;
      last_candidate = current_candidate;
      ncandidates++;
    }
                                                     
  }

  if (last_candidate)                             
  {
    last_candidate->next = NULL;
  }

  if (ncandidates == 1)
  {
    return candidates;
  }

     
                                                                          
                                                                    
                                                                         
                                                                  
                                                                          
     
  for (i = 0; i < nargs; i++)                             
  {
    slot_category[i] = TypeCategory(input_base_typeids[i]);
  }
  ncandidates = 0;
  nbestMatch = 0;
  last_candidate = NULL;
  for (current_candidate = candidates; current_candidate != NULL; current_candidate = current_candidate->next)
  {
    current_typeids = current_candidate->args;
    nmatch = 0;
    for (i = 0; i < nargs; i++)
    {
      if (input_base_typeids[i] != UNKNOWNOID)
      {
        if (current_typeids[i] == input_base_typeids[i] || IsPreferredType(slot_category[i], current_typeids[i]))
        {
          nmatch++;
        }
      }
    }

    if ((nmatch > nbestMatch) || (last_candidate == NULL))
    {
      nbestMatch = nmatch;
      candidates = current_candidate;
      last_candidate = current_candidate;
      ncandidates = 1;
    }
    else if (nmatch == nbestMatch)
    {
      last_candidate->next = current_candidate;
      last_candidate = current_candidate;
      ncandidates++;
    }
  }

  if (last_candidate)                             
  {
    last_candidate->next = NULL;
  }

  if (ncandidates == 1)
  {
    return candidates;
  }

     
                                                                             
     
                                                                            
                    
     
  if (nunknowns == 0)
  {
    return NULL;                                        
  }

     
                                                                            
                                                                        
                                                                         
                                                                           
                                                                         
                                                                       
                                  
     
                                                                             
                                                                    
     
                                                                          
                                                                    
                                                                          
                                                                             
                                                                          
                            
     
  resolved_unknowns = false;
  for (i = 0; i < nargs; i++)
  {
    bool have_conflict;

    if (input_base_typeids[i] != UNKNOWNOID)
    {
      continue;
    }
    resolved_unknowns = true;                          
    slot_category[i] = TYPCATEGORY_INVALID;
    slot_has_preferred_type[i] = false;
    have_conflict = false;
    for (current_candidate = candidates; current_candidate != NULL; current_candidate = current_candidate->next)
    {
      current_typeids = current_candidate->args;
      current_type = current_typeids[i];
      get_type_category_preferred(current_type, &current_category, &current_is_preferred);
      if (slot_category[i] == TYPCATEGORY_INVALID)
      {
                             
        slot_category[i] = current_category;
        slot_has_preferred_type[i] = current_is_preferred;
      }
      else if (current_category == slot_category[i])
      {
                                              
        slot_has_preferred_type[i] |= current_is_preferred;
      }
      else
      {
                                
        if (current_category == TYPCATEGORY_STRING)
        {
                                               
          slot_category[i] = current_category;
          slot_has_preferred_type[i] = current_is_preferred;
        }
        else
        {
             
                                                                   
             
          have_conflict = true;
        }
      }
    }
    if (have_conflict && slot_category[i] != TYPCATEGORY_STRING)
    {
                                                                
      resolved_unknowns = false;
      break;
    }
  }

  if (resolved_unknowns)
  {
                                       
    ncandidates = 0;
    first_candidate = candidates;
    last_candidate = NULL;
    for (current_candidate = candidates; current_candidate != NULL; current_candidate = current_candidate->next)
    {
      bool keepit = true;

      current_typeids = current_candidate->args;
      for (i = 0; i < nargs; i++)
      {
        if (input_base_typeids[i] != UNKNOWNOID)
        {
          continue;
        }
        current_type = current_typeids[i];
        get_type_category_preferred(current_type, &current_category, &current_is_preferred);
        if (current_category != slot_category[i])
        {
          keepit = false;
          break;
        }
        if (slot_has_preferred_type[i] && !current_is_preferred)
        {
          keepit = false;
          break;
        }
      }
      if (keepit)
      {
                                 
        last_candidate = current_candidate;
        ncandidates++;
      }
      else
      {
                                   
        if (last_candidate)
        {
          last_candidate->next = current_candidate->next;
        }
        else
        {
          first_candidate = current_candidate->next;
        }
      }
    }

                                                                  
    if (last_candidate)
    {
      candidates = first_candidate;
                                  
      last_candidate->next = NULL;
    }

    if (ncandidates == 1)
    {
      return candidates;
    }
  }

     
                                                                          
                                                                           
                                                                            
     
                                                                             
                                                                            
                                                                             
                                                         
     
  if (nunknowns < nargs)
  {
    Oid known_type = UNKNOWNOID;

    for (i = 0; i < nargs; i++)
    {
      if (input_base_typeids[i] == UNKNOWNOID)
      {
        continue;
      }
      if (known_type == UNKNOWNOID)                       
      {
        known_type = input_base_typeids[i];
      }
      else if (known_type != input_base_typeids[i])
      {
                                 
        known_type = UNKNOWNOID;
        break;
      }
    }

    if (known_type != UNKNOWNOID)
    {
                                                          
      for (i = 0; i < nargs; i++)
      {
        input_base_typeids[i] = known_type;
      }
      ncandidates = 0;
      last_candidate = NULL;
      for (current_candidate = candidates; current_candidate != NULL; current_candidate = current_candidate->next)
      {
        current_typeids = current_candidate->args;
        if (can_coerce_type(nargs, input_base_typeids, current_typeids, COERCION_IMPLICIT))
        {
          if (++ncandidates > 1)
          {
            break;                          
          }
          last_candidate = current_candidate;
        }
      }
      if (ncandidates == 1)
      {
                                                    
        last_candidate->next = NULL;
        return last_candidate;
      }
    }
  }

  return NULL;                                        
}                              

                     
   
                                                   
   
                                                                  
                                                                         
                            
   
                                  
                                                                   
                                                    
   
                                                                                
                                                                          
                                                                      
   
                                                                              
                                                                          
                                                                            
                                                                 
                                                                                
                                                                           
                                                
   
                                                                              
                                                                           
                                                                               
                                                                              
                                                                              
                                                           
   
FuncDetailCode
func_get_detail(List *funcname, List *fargs, List *fargnames, int nargs, Oid *argtypes, bool expand_variadic, bool expand_defaults, Oid *funcid,                   
    Oid *rettype,                                                                                                                                                  
    bool *retset,                                                                                                                                                  
    int *nvargs,                                                                                                                                                   
    Oid *vatype,                                                                                                                                                   
    Oid **true_typeids,                                                                                                                                            
    List **argdefaults)                                                                                                                                                     
{
  FuncCandidateList raw_candidates;
  FuncCandidateList best_candidate;

                                                      
  Assert(argtypes);

                                                                
  *funcid = InvalidOid;
  *rettype = InvalidOid;
  *retset = false;
  *nvargs = 0;
  *vatype = InvalidOid;
  *true_typeids = NULL;
  if (argdefaults)
  {
    *argdefaults = NIL;
  }

                                                             
  raw_candidates = FuncnameGetCandidates(funcname, nargs, fargnames, expand_variadic, expand_defaults, false);

     
                                                                            
                      
     
  for (best_candidate = raw_candidates; best_candidate != NULL; best_candidate = best_candidate->next)
  {
    if (memcmp(argtypes, best_candidate->args, nargs * sizeof(Oid)) == 0)
    {
      break;
    }
  }

  if (best_candidate == NULL)
  {
       
                                                                       
                                                                      
                                                                         
                                                                         
                                                    
       
                                                                  
                                                                        
                                                                          
                                                                           
                                                                         
                             
       
                                                                     
                                             
       
                                                                          
                                                                       
                                                                        
       
                                                                         
                                                                      
                                                                         
                                                                       
                                          
       
                                                                       
                                                                           
                                                                           
                                                                          
                                                       
       
                                                                          
                                                                      
                                                                           
                                                                        
                                    
       
    if (nargs == 1 && fargs != NIL && fargnames == NIL)
    {
      Oid targetType = FuncNameAsType(funcname);

      if (OidIsValid(targetType))
      {
        Oid sourceType = argtypes[0];
        Node *arg1 = linitial(fargs);
        bool iscoercion;

        if (sourceType == UNKNOWNOID && IsA(arg1, Const))
        {
                                                            
          iscoercion = true;
        }
        else
        {
          CoercionPathType cpathtype;
          Oid cfuncid;

          cpathtype = find_coercion_pathway(targetType, sourceType, COERCION_EXPLICIT, &cfuncid);
          switch (cpathtype)
          {
          case COERCION_PATH_RELABELTYPE:
            iscoercion = true;
            break;
          case COERCION_PATH_COERCEVIAIO:
            if ((sourceType == RECORDOID || ISCOMPLEX(sourceType)) && TypeCategory(targetType) == TYPCATEGORY_STRING)
            {
              iscoercion = false;
            }
            else
            {
              iscoercion = true;
            }
            break;
          default:
            iscoercion = false;
            break;
          }
        }

        if (iscoercion)
        {
                                           
          *funcid = InvalidOid;
          *rettype = targetType;
          *retset = false;
          *nvargs = 0;
          *vatype = InvalidOid;
          *true_typeids = argtypes;
          return FUNCDETAIL_COERCION;
        }
      }
    }

       
                                                                        
       
    if (raw_candidates != NULL)
    {
      FuncCandidateList current_candidates;
      int ncandidates;

      ncandidates = func_match_argtypes(nargs, argtypes, raw_candidates, &current_candidates);

                                               
      if (ncandidates == 1)
      {
        best_candidate = current_candidates;
      }

         
                                                                      
         
      else if (ncandidates > 1)
      {
        best_candidate = func_select_candidate(nargs, argtypes, current_candidates);

           
                                                                   
                                               
           
        if (!best_candidate)
        {
          return FUNCDETAIL_MULTIPLE;
        }
      }
    }
  }

  if (best_candidate)
  {
    HeapTuple ftup;
    Form_pg_proc pform;
    FuncDetailCode result;

       
                                                                        
                                                                   
                                                
       
    if (!OidIsValid(best_candidate->oid))
    {
      return FUNCDETAIL_MULTIPLE;
    }

       
                                                                          
                                                                      
                                                                           
       
    if (fargnames != NIL && !expand_variadic && nargs > 0 && best_candidate->argnumbers[nargs - 1] != nargs - 1)
    {
      return FUNCDETAIL_NOTFOUND;
    }

    *funcid = best_candidate->oid;
    *nvargs = best_candidate->nvargs;
    *true_typeids = best_candidate->args;

       
                                                                       
                                                                         
                                                             
       
    if (best_candidate->argnumbers != NULL)
    {
      int i = 0;
      ListCell *lc;

      foreach (lc, fargs)
      {
        NamedArgExpr *na = (NamedArgExpr *)lfirst(lc);

        if (IsA(na, NamedArgExpr))
        {
          na->argnumber = best_candidate->argnumbers[i];
        }
        i++;
      }
    }

    ftup = SearchSysCache1(PROCOID, ObjectIdGetDatum(best_candidate->oid));
    if (!HeapTupleIsValid(ftup))                        
    {
      elog(ERROR, "cache lookup failed for function %u", best_candidate->oid);
    }
    pform = (Form_pg_proc)GETSTRUCT(ftup);
    *rettype = pform->prorettype;
    *retset = pform->proretset;
    *vatype = pform->provariadic;
                                                
    if (argdefaults && best_candidate->ndargs > 0)
    {
      Datum proargdefaults;
      bool isnull;
      char *str;
      List *defaults;

                                                             
      if (best_candidate->ndargs > pform->pronargdefaults)
      {
        elog(ERROR, "not enough default arguments");
      }

      proargdefaults = SysCacheGetAttr(PROCOID, ftup, Anum_pg_proc_proargdefaults, &isnull);
      Assert(!isnull);
      str = TextDatumGetCString(proargdefaults);
      defaults = castNode(List, stringToNode(str));
      pfree(str);

                                                             
      if (best_candidate->argnumbers != NULL)
      {
           
                                                                      
                                                                   
                                                                     
                                                                    
                                                                     
                                                          
           
        Bitmapset *defargnumbers;
        int *firstdefarg;
        List *newdefaults;
        ListCell *lc;
        int i;

        defargnumbers = NULL;
        firstdefarg = &best_candidate->argnumbers[best_candidate->nargs - best_candidate->ndargs];
        for (i = 0; i < best_candidate->ndargs; i++)
        {
          defargnumbers = bms_add_member(defargnumbers, firstdefarg[i]);
        }
        newdefaults = NIL;
        i = pform->pronargs - pform->pronargdefaults;
        foreach (lc, defaults)
        {
          if (bms_is_member(i, defargnumbers))
          {
            newdefaults = lappend(newdefaults, lfirst(lc));
          }
          i++;
        }
        Assert(list_length(newdefaults) == best_candidate->ndargs);
        bms_free(defargnumbers);
        *argdefaults = newdefaults;
      }
      else
      {
           
                                                                  
                                                    
           
        int ndelete;

        ndelete = list_length(defaults) - best_candidate->ndargs;
        while (ndelete-- > 0)
        {
          defaults = list_delete_first(defaults);
        }
        *argdefaults = defaults;
      }
    }

    switch (pform->prokind)
    {
    case PROKIND_AGGREGATE:
      result = FUNCDETAIL_AGGREGATE;
      break;
    case PROKIND_FUNCTION:
      result = FUNCDETAIL_NORMAL;
      break;
    case PROKIND_PROCEDURE:
      result = FUNCDETAIL_PROCEDURE;
      break;
    case PROKIND_WINDOW:
      result = FUNCDETAIL_WINDOWFUNC;
      break;
    default:
      elog(ERROR, "unrecognized prokind: %c", pform->prokind);
      result = FUNCDETAIL_NORMAL;                          
      break;
    }

    ReleaseSysCache(ftup);
    return result;
  }

  return FUNCDETAIL_NOTFOUND;
}

   
                             
   
                                                                       
                                                                         
                                                                      
                       
   
                                                                          
                                                                             
                                                                            
                                                                           
                                                                       
   
static void
unify_hypothetical_args(ParseState *pstate, List *fargs, int numAggregatedArgs, Oid *actual_arg_types, Oid *declared_arg_types)
{
  Node *args[FUNC_MAX_ARGS];
  int numDirectArgs, numNonHypotheticalArgs;
  int i;
  ListCell *lc;

  numDirectArgs = list_length(fargs) - numAggregatedArgs;
  numNonHypotheticalArgs = numDirectArgs - numAggregatedArgs;
                                                                 
  if (numNonHypotheticalArgs < 0)
  {
    elog(ERROR, "incorrect number of arguments to hypothetical-set aggregate");
  }

                                                                
  i = 0;
  foreach (lc, fargs)
  {
    args[i++] = (Node *)lfirst(lc);
  }

                                                                    
  for (i = numNonHypotheticalArgs; i < numDirectArgs; i++)
  {
    int aargpos = numDirectArgs + (i - numNonHypotheticalArgs);
    Oid commontype;

                                                                    
    if (declared_arg_types[i] != declared_arg_types[aargpos])
    {
      elog(ERROR, "hypothetical-set aggregate has inconsistent declared argument types");
    }

                                                           
    if (declared_arg_types[i] != ANYOID)
    {
      continue;
    }

       
                                                                          
                                                                         
                               
       
    commontype = select_common_type(pstate, list_make2(args[aargpos], args[i]), "WITHIN GROUP", NULL);

       
                                                                          
                                                           
       
    args[i] = coerce_type(pstate, args[i], actual_arg_types[i], commontype, -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
    actual_arg_types[i] = commontype;
    args[aargpos] = coerce_type(pstate, args[aargpos], actual_arg_types[aargpos], commontype, -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
    actual_arg_types[aargpos] = commontype;
  }

                                    
  i = 0;
  foreach (lc, fargs)
  {
    lfirst(lc) = args[i++];
  }
}

   
                       
   
                                                                         
                                                                      
                                                                         
            
   
                                                      
   
                                                                       
                         
   
void
make_fn_arguments(ParseState *pstate, List *fargs, Oid *actual_arg_types, Oid *declared_arg_types)
{
  ListCell *current_fargs;
  int i = 0;

  foreach (current_fargs, fargs)
  {
                                                                         
    if (actual_arg_types[i] != declared_arg_types[i])
    {
      Node *node = (Node *)lfirst(current_fargs);

         
                                                                        
                                                                     
         
      if (IsA(node, NamedArgExpr))
      {
        NamedArgExpr *na = (NamedArgExpr *)node;

        node = coerce_type(pstate, (Node *)na->arg, actual_arg_types[i], declared_arg_types[i], -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
        na->arg = (Expr *)node;
      }
      else
      {
        node = coerce_type(pstate, node, actual_arg_types[i], declared_arg_types[i], -1, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1);
        lfirst(current_fargs) = node;
      }
    }
    i++;
  }
}

   
                    
                                                                       
   
                                                                           
                                  
   
static Oid
FuncNameAsType(List *funcname)
{
  Oid result;
  Type typtup;

     
                                                                            
                                                             
     
  typtup = LookupTypeNameExtended(NULL, makeTypeNameFromNameList(funcname), NULL, false, false);
  if (typtup == NULL)
  {
    return InvalidOid;
  }

  if (((Form_pg_type)GETSTRUCT(typtup))->typisdefined && !OidIsValid(typeTypeRelid(typtup)))
  {
    result = typeTypeId(typtup);
  }
  else
  {
    result = InvalidOid;
  }

  ReleaseSysCache(typtup);
  return result;
}

   
                            
                                                                            
                                                                             
                                                        
   
static Node *
ParseComplexProjection(ParseState *pstate, const char *funcname, Node *first_arg, int location)
{
  TupleDesc tupdesc;
  int i;

     
                                                                             
                                                                         
                                                                       
                                                                          
            
     
                                                                       
                                                
     
  if (IsA(first_arg, Var) && ((Var *)first_arg)->varattno == InvalidAttrNumber)
  {
    RangeTblEntry *rte;

    rte = GetRTEByRangeTablePosn(pstate, ((Var *)first_arg)->varno, ((Var *)first_arg)->varlevelsup);
                                                              
    return scanRTEForColumn(pstate, rte, funcname, location, 0, NULL);
  }

     
                                                             
     
                                                                           
                                                                            
                                                     
     
  if (IsA(first_arg, Var) && ((Var *)first_arg)->vartype == RECORDOID)
  {
    tupdesc = expandRecordVariable(pstate, (Var *)first_arg, 0);
  }
  else
  {
    tupdesc = get_expr_result_tupdesc(first_arg, true);
  }
  if (!tupdesc)
  {
    return NULL;                               
  }

  for (i = 0; i < tupdesc->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

    if (strcmp(funcname, NameStr(att->attname)) == 0 && !att->attisdropped)
    {
                                                         
      FieldSelect *fselect = makeNode(FieldSelect);

      fselect->arg = (Expr *)first_arg;
      fselect->fieldnum = i + 1;
      fselect->resulttype = att->atttypid;
      fselect->resulttypmod = att->atttypmod;
                                                          
      fselect->resultcollid = att->attcollation;
      return (Node *)fselect;
    }
  }

  return NULL;                                         
}

   
                             
                                                                      
                                                 
   
                                                                            
                                                                            
                                                                            
   
                                                                          
             
   
const char *
funcname_signature_string(const char *funcname, int nargs, List *argnames, const Oid *argtypes)
{
  StringInfoData argbuf;
  int numposargs;
  ListCell *lc;
  int i;

  initStringInfo(&argbuf);

  appendStringInfo(&argbuf, "%s(", funcname);

  numposargs = nargs - list_length(argnames);
  lc = list_head(argnames);

  for (i = 0; i < nargs; i++)
  {
    if (i)
    {
      appendStringInfoString(&argbuf, ", ");
    }
    if (i >= numposargs)
    {
      appendStringInfo(&argbuf, "%s => ", (char *)lfirst(lc));
      lc = lnext(lc);
    }
    appendStringInfoString(&argbuf, format_type_be(argtypes[i]));
  }

  appendStringInfoChar(&argbuf, ')');

  return argbuf.data;                                    
}

   
                         
                                                                    
   
const char *
func_signature_string(List *funcname, int nargs, List *argnames, const Oid *argtypes)
{
  return funcname_signature_string(NameListToString(funcname), nargs, argnames, argtypes);
}

   
                          
                                                    
   
                                                                       
                                                                
   
                    
                                                                 
                                                                         
   
static Oid
LookupFuncNameInternal(List *funcname, int nargs, const Oid *argtypes, bool missing_ok, FuncLookupError *lookupError)
{
  FuncCandidateList clist;

                                                      
  Assert(argtypes);

                                                                             
  *lookupError = FUNCLOOKUP_NOSUCHFUNC;

  clist = FuncnameGetCandidates(funcname, nargs, NIL, false, false, missing_ok);

     
                                                                             
     
  if (nargs < 0)
  {
    if (clist)
    {
                                                          
      if (clist->next)
      {
        *lookupError = FUNCLOOKUP_AMBIGUOUS;
        return InvalidOid;
      }
                                      
      return clist->oid;
    }
    else
    {
      return InvalidOid;
    }
  }

     
                                                                          
                                                                      
     
  while (clist)
  {
    if (memcmp(argtypes, clist->args, nargs * sizeof(Oid)) == 0)
    {
      return clist->oid;
    }
    clist = clist->next;
  }

  return InvalidOid;
}

   
                  
   
                                                                             
                                                                              
                                                                       
                                            
   
                                                                             
                          
   
                                                                             
                        
   
                                                                               
                                                                               
           
   
Oid
LookupFuncName(List *funcname, int nargs, const Oid *argtypes, bool missing_ok)
{
  Oid funcoid;
  FuncLookupError lookupError;

  funcoid = LookupFuncNameInternal(funcname, nargs, argtypes, missing_ok, &lookupError);

  if (OidIsValid(funcoid))
  {
    return funcoid;
  }

  switch (lookupError)
  {
  case FUNCLOOKUP_NOSUCHFUNC:
                                                             
    if (missing_ok)
    {
      return InvalidOid;
    }

    if (nargs < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not find a function named \"%s\"", NameListToString(funcname))));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(funcname, nargs, NIL, argtypes))));
    }
    break;

  case FUNCLOOKUP_AMBIGUOUS:
                                                 
    ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("function name \"%s\" is not unique", NameListToString(funcname)), errhint("Specify the argument list to select the function unambiguously.")));
    break;
  }

  return InvalidOid;                          
}

   
                      
   
                                                                   
                                                                               
                                                                           
                                         
   
                                                                        
             
   
                                                                               
                                                                             
                               
   
Oid
LookupFuncWithArgs(ObjectType objtype, ObjectWithArgs *func, bool missing_ok)
{
  Oid argoids[FUNC_MAX_ARGS];
  int argcount;
  int nargs;
  int i;
  ListCell *args_item;
  Oid oid;
  FuncLookupError lookupError;

  Assert(objtype == OBJECT_AGGREGATE || objtype == OBJECT_FUNCTION || objtype == OBJECT_PROCEDURE || objtype == OBJECT_ROUTINE);

  argcount = list_length(func->objargs);
  if (argcount > FUNC_MAX_ARGS)
  {
    if (objtype == OBJECT_PROCEDURE)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("procedures cannot have more than %d argument", "procedures cannot have more than %d arguments", FUNC_MAX_ARGS, FUNC_MAX_ARGS)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_ARGUMENTS), errmsg_plural("functions cannot have more than %d argument", "functions cannot have more than %d arguments", FUNC_MAX_ARGS, FUNC_MAX_ARGS)));
    }
  }

  i = 0;
  foreach (args_item, func->objargs)
  {
    TypeName *t = (TypeName *)lfirst(args_item);

    argoids[i] = LookupTypeNameOid(NULL, t, missing_ok);
    if (!OidIsValid(argoids[i]))
    {
      return InvalidOid;                              
    }
    i++;
  }

     
                                                                         
                     
     
  nargs = func->args_unspecified ? -1 : argcount;

  oid = LookupFuncNameInternal(func->objname, nargs, argoids, missing_ok, &lookupError);

  if (OidIsValid(oid))
  {
       
                                                                          
                                                                          
                                                                         
                                                                           
                                                                         
       
    switch (objtype)
    {
    case OBJECT_FUNCTION:
                                              
      if (get_func_prokind(oid) == PROKIND_PROCEDURE)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s is not a function", func_signature_string(func->objname, argcount, NIL, argoids))));
      }
      break;

    case OBJECT_PROCEDURE:
                                                      
      if (get_func_prokind(oid) != PROKIND_PROCEDURE)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s is not a procedure", func_signature_string(func->objname, argcount, NIL, argoids))));
      }
      break;

    case OBJECT_AGGREGATE:
                                                       
      if (get_func_prokind(oid) != PROKIND_AGGREGATE)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("function %s is not an aggregate", func_signature_string(func->objname, argcount, NIL, argoids))));
      }
      break;

    default:
                                            
      break;
    }

    return oid;               
  }
  else
  {
                                                 
    switch (lookupError)
    {
    case FUNCLOOKUP_NOSUCHFUNC:
                                                                
      if (missing_ok)
      {
        break;
      }

      switch (objtype)
      {
      case OBJECT_PROCEDURE:
        if (func->args_unspecified)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not find a procedure named \"%s\"", NameListToString(func->objname))));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("procedure %s does not exist", func_signature_string(func->objname, argcount, NIL, argoids))));
        }
        break;

      case OBJECT_AGGREGATE:
        if (func->args_unspecified)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not find an aggregate named \"%s\"", NameListToString(func->objname))));
        }
        else if (argcount == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("aggregate %s(*) does not exist", NameListToString(func->objname))));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("aggregate %s does not exist", func_signature_string(func->objname, argcount, NIL, argoids))));
        }
        break;

      default:
                                  
        if (func->args_unspecified)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not find a function named \"%s\"", NameListToString(func->objname))));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function %s does not exist", func_signature_string(func->objname, argcount, NIL, argoids))));
        }
        break;
      }
      break;

    case FUNCLOOKUP_AMBIGUOUS:
      switch (objtype)
      {
      case OBJECT_FUNCTION:
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("function name \"%s\" is not unique", NameListToString(func->objname)), errhint("Specify the argument list to select the function unambiguously.")));
        break;
      case OBJECT_PROCEDURE:
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("procedure name \"%s\" is not unique", NameListToString(func->objname)), errhint("Specify the argument list to select the procedure unambiguously.")));
        break;
      case OBJECT_AGGREGATE:
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("aggregate name \"%s\" is not unique", NameListToString(func->objname)), errhint("Specify the argument list to select the aggregate unambiguously.")));
        break;
      case OBJECT_ROUTINE:
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_FUNCTION), errmsg("routine name \"%s\" is not unique", NameListToString(func->objname)), errhint("Specify the argument list to select the routine unambiguously.")));
        break;

      default:
        Assert(false);                                 
        break;
      }
      break;
    }

    return InvalidOid;
  }
}

   
                            
                                                                     
                                   
   
                                                                        
   
                                                                       
                                                                         
                                                    
   
void
check_srf_call_placement(ParseState *pstate, Node *last_srf, int location)
{
  const char *err;
  bool errkind;

     
                                                                       
                                                                          
                                                                            
                            
     
                                                                         
                                                                           
                                                                             
                                                                          
                                                      
     
  err = NULL;
  errkind = false;
  switch (pstate->p_expr_kind)
  {
  case EXPR_KIND_NONE:
    Assert(false);                   
    break;
  case EXPR_KIND_OTHER:
                                                            
    break;
  case EXPR_KIND_JOIN_ON:
  case EXPR_KIND_JOIN_USING:
    err = _("set-returning functions are not allowed in JOIN conditions");
    break;
  case EXPR_KIND_FROM_SUBSELECT:
                                                          
    errkind = true;
    break;
  case EXPR_KIND_FROM_FUNCTION:
                                                   
                                                            
                                                   
    if (pstate->p_last_srf != last_srf)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-returning functions must appear at top level of FROM"), parser_errposition(pstate, exprLocation(pstate->p_last_srf))));
    }
    break;
  case EXPR_KIND_WHERE:
    errkind = true;
    break;
  case EXPR_KIND_POLICY:
    err = _("set-returning functions are not allowed in policy expressions");
    break;
  case EXPR_KIND_HAVING:
    errkind = true;
    break;
  case EXPR_KIND_FILTER:
    errkind = true;
    break;
  case EXPR_KIND_WINDOW_PARTITION:
  case EXPR_KIND_WINDOW_ORDER:
                                                       
    pstate->p_hasTargetSRFs = true;
    break;
  case EXPR_KIND_WINDOW_FRAME_RANGE:
  case EXPR_KIND_WINDOW_FRAME_ROWS:
  case EXPR_KIND_WINDOW_FRAME_GROUPS:
    err = _("set-returning functions are not allowed in window definitions");
    break;
  case EXPR_KIND_SELECT_TARGET:
  case EXPR_KIND_INSERT_TARGET:
              
    pstate->p_hasTargetSRFs = true;
    break;
  case EXPR_KIND_UPDATE_SOURCE:
  case EXPR_KIND_UPDATE_TARGET:
                                                             
    errkind = true;
    break;
  case EXPR_KIND_GROUP_BY:
  case EXPR_KIND_ORDER_BY:
              
    pstate->p_hasTargetSRFs = true;
    break;
  case EXPR_KIND_DISTINCT_ON:
              
    pstate->p_hasTargetSRFs = true;
    break;
  case EXPR_KIND_LIMIT:
  case EXPR_KIND_OFFSET:
    errkind = true;
    break;
  case EXPR_KIND_RETURNING:
    errkind = true;
    break;
  case EXPR_KIND_VALUES:
                                                              
    errkind = true;
    break;
  case EXPR_KIND_VALUES_SINGLE:
                                                         
    pstate->p_hasTargetSRFs = true;
    break;
  case EXPR_KIND_CHECK_CONSTRAINT:
  case EXPR_KIND_DOMAIN_CHECK:
    err = _("set-returning functions are not allowed in check constraints");
    break;
  case EXPR_KIND_COLUMN_DEFAULT:
  case EXPR_KIND_FUNCTION_DEFAULT:
    err = _("set-returning functions are not allowed in DEFAULT expressions");
    break;
  case EXPR_KIND_INDEX_EXPRESSION:
    err = _("set-returning functions are not allowed in index expressions");
    break;
  case EXPR_KIND_INDEX_PREDICATE:
    err = _("set-returning functions are not allowed in index predicates");
    break;
  case EXPR_KIND_ALTER_COL_TRANSFORM:
    err = _("set-returning functions are not allowed in transform expressions");
    break;
  case EXPR_KIND_EXECUTE_PARAMETER:
    err = _("set-returning functions are not allowed in EXECUTE parameters");
    break;
  case EXPR_KIND_TRIGGER_WHEN:
    err = _("set-returning functions are not allowed in trigger WHEN conditions");
    break;
  case EXPR_KIND_PARTITION_BOUND:
    err = _("set-returning functions are not allowed in partition bound");
    break;
  case EXPR_KIND_PARTITION_EXPRESSION:
    err = _("set-returning functions are not allowed in partition key expressions");
    break;
  case EXPR_KIND_CALL_ARGUMENT:
    err = _("set-returning functions are not allowed in CALL arguments");
    break;
  case EXPR_KIND_COPY_WHERE:
    err = _("set-returning functions are not allowed in COPY FROM WHERE conditions");
    break;
  case EXPR_KIND_GENERATED_COLUMN:
    err = _("set-returning functions are not allowed in column generation expressions");
    break;

       
                                                                 
                                                                
                                                                     
                                                                      
                             
       
  }
  if (err)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg_internal("%s", err), parser_errposition(pstate, location)));
  }
  if (errkind)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                                   
                       errmsg("set-returning functions are not allowed in %s", ParseExprKindName(pstate->p_expr_kind)), parser_errposition(pstate, location)));
  }
}
