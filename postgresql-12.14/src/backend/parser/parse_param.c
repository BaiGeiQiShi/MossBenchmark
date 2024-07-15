                                                                            
   
                 
                                 
   
                                                                     
                                                  
                                                                  
                                 
                                                                              
   
                                                                          
                                
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_param.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

typedef struct FixedParamState
{
  Oid *paramTypes;                                   
  int numParams;                                
} FixedParamState;

   
                                                                        
                                                                               
                                                                            
                              
   
typedef struct VarParamState
{
  Oid **paramTypes;                                   
  int *numParams;                                
} VarParamState;

static Node *
fixed_paramref_hook(ParseState *pstate, ParamRef *pref);
static Node *
variable_paramref_hook(ParseState *pstate, ParamRef *pref);
static Node *
variable_coerce_param_hook(ParseState *pstate, Param *param, Oid targetTypeId, int32 targetTypeMod, int location);
static bool
check_parameter_resolution_walker(Node *node, ParseState *pstate);
static bool
query_contains_extern_params_walker(Node *node, void *context);

   
                                                                        
   
void
parse_fixed_parameters(ParseState *pstate, Oid *paramTypes, int numParams)
{
  FixedParamState *parstate = palloc(sizeof(FixedParamState));

  parstate->paramTypes = paramTypes;
  parstate->numParams = numParams;
  pstate->p_ref_hook_state = (void *)parstate;
  pstate->p_paramref_hook = fixed_paramref_hook;
                                          
}

   
                                                                           
   
void
parse_variable_parameters(ParseState *pstate, Oid **paramTypes, int *numParams)
{
  VarParamState *parstate = palloc(sizeof(VarParamState));

  parstate->paramTypes = paramTypes;
  parstate->numParams = numParams;
  pstate->p_ref_hook_state = (void *)parstate;
  pstate->p_paramref_hook = variable_paramref_hook;
  pstate->p_coerce_param_hook = variable_coerce_param_hook;
}

   
                                                     
   
static Node *
fixed_paramref_hook(ParseState *pstate, ParamRef *pref)
{
  FixedParamState *parstate = (FixedParamState *)pstate->p_ref_hook_state;
  int paramno = pref->number;
  Param *param;

                                       
  if (paramno <= 0 || paramno > parstate->numParams || !OidIsValid(parstate->paramTypes[paramno - 1]))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("there is no parameter $%d", paramno), parser_errposition(pstate, pref->location)));
  }

  param = makeNode(Param);
  param->paramkind = PARAM_EXTERN;
  param->paramid = paramno;
  param->paramtype = parstate->paramTypes[paramno - 1];
  param->paramtypmod = -1;
  param->paramcollid = get_typcollation(param->paramtype);
  param->location = pref->location;

  return (Node *)param;
}

   
                                                        
   
                                                                        
              
   
static Node *
variable_paramref_hook(ParseState *pstate, ParamRef *pref)
{
  VarParamState *parstate = (VarParamState *)pstate->p_ref_hook_state;
  int paramno = pref->number;
  Oid *pptype;
  Param *param;

                                          
  if (paramno <= 0 || paramno > INT_MAX / sizeof(Oid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("there is no parameter $%d", paramno), parser_errposition(pstate, pref->location)));
  }
  if (paramno > *parstate->numParams)
  {
                                     
    if (*parstate->paramTypes)
    {
      *parstate->paramTypes = (Oid *)repalloc(*parstate->paramTypes, paramno * sizeof(Oid));
    }
    else
    {
      *parstate->paramTypes = (Oid *)palloc(paramno * sizeof(Oid));
    }
                                                    
    MemSet(*parstate->paramTypes + *parstate->numParams, 0, (paramno - *parstate->numParams) * sizeof(Oid));
    *parstate->numParams = paramno;
  }

                                    
  pptype = &(*parstate->paramTypes)[paramno - 1];

                                                      
  if (*pptype == InvalidOid)
  {
    *pptype = UNKNOWNOID;
  }

  param = makeNode(Param);
  param->paramkind = PARAM_EXTERN;
  param->paramid = paramno;
  param->paramtype = *pptype;
  param->paramtypmod = -1;
  param->paramcollid = get_typcollation(param->paramtype);
  param->location = pref->location;

  return (Node *)param;
}

   
                                                                        
   
static Node *
variable_coerce_param_hook(ParseState *pstate, Param *param, Oid targetTypeId, int32 targetTypeMod, int location)
{
  if (param->paramkind == PARAM_EXTERN && param->paramtype == UNKNOWNOID)
  {
       
                                                                        
                                                 
       
    VarParamState *parstate = (VarParamState *)pstate->p_ref_hook_state;
    Oid *paramTypes = *parstate->paramTypes;
    int paramno = param->paramid;

    if (paramno <= 0 ||                               
        paramno > *parstate->numParams)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("there is no parameter $%d", paramno), parser_errposition(pstate, param->location)));
    }

    if (paramTypes[paramno - 1] == UNKNOWNOID)
    {
                                                
      paramTypes[paramno - 1] = targetTypeId;
    }
    else if (paramTypes[paramno - 1] == targetTypeId)
    {
                                                           
    }
    else
    {
                
      ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_PARAMETER), errmsg("inconsistent types deduced for parameter $%d", paramno), errdetail("%s versus %s", format_type_be(paramTypes[paramno - 1]), format_type_be(targetTypeId)), parser_errposition(pstate, param->location)));
    }

    param->paramtype = targetTypeId;

       
                                                                   
                                                                     
                                                                         
                                                                       
                                                            
       
    param->paramtypmod = -1;

       
                                                                         
                                                                           
                                               
       
    param->paramcollid = get_typcollation(param->paramtype);

                                                                  
    if (location >= 0 && (param->location < 0 || location < param->location))
    {
      param->location = location;
    }

    return (Node *)param;
  }

                                                   
  return NULL;
}

   
                                                                           
                                              
   
                                                                             
                                                                             
                                          
   
void
check_variable_parameters(ParseState *pstate, Query *query)
{
  VarParamState *parstate = (VarParamState *)pstate->p_ref_hook_state;

                                                                      
  if (*parstate->numParams > 0)
  {
    (void)query_tree_walker(query, check_parameter_resolution_walker, (void *)pstate, 0);
  }
}

   
                                                                   
                                                                    
                                                                 
                                                                 
   
static bool
check_parameter_resolution_walker(Node *node, ParseState *pstate)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_EXTERN)
    {
      VarParamState *parstate = (VarParamState *)pstate->p_ref_hook_state;
      int paramno = param->paramid;

      if (paramno <= 0 ||                               
          paramno > *parstate->numParams)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PARAMETER), errmsg("there is no parameter $%d", paramno), parser_errposition(pstate, param->location)));
      }

      if (param->paramtype != (*parstate->paramTypes)[paramno - 1])
      {
        ereport(ERROR, (errcode(ERRCODE_AMBIGUOUS_PARAMETER), errmsg("could not determine data type of parameter $%d", paramno), parser_errposition(pstate, param->location)));
      }
    }
    return false;
  }
  if (IsA(node, Query))
  {
                                                                       
    return query_tree_walker((Query *)node, check_parameter_resolution_walker, (void *)pstate, 0);
  }
  return expression_tree_walker(node, check_parameter_resolution_walker, (void *)pstate);
}

   
                                                                               
   
bool
query_contains_extern_params(Query *query)
{
  return query_tree_walker(query, query_contains_extern_params_walker, NULL, 0);
}

static bool
query_contains_extern_params_walker(Node *node, void *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_EXTERN)
    {
      return true;
    }
    return false;
  }
  if (IsA(node, Query))
  {
                                                                       
    return query_tree_walker((Query *)node, query_contains_extern_params_walker, context, 0);
  }
  return expression_tree_walker(node, query_contains_extern_params_walker, context);
}
