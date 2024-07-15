                                                                            
   
            
                                                            
   
   
                                                                         
                                                                        
   
   
                  
                                   
   
               
                                                                   
                                                             
                                                                 
                                                                        
                                         
   
         
                                                                        
                       
                                         
                   
           
                       
                
   
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <math.h>

#include "catalog/namespace.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "parser/scansup.h"
#include "utils/builtins.h"

   
                                                                    
   
char *
defGetString(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a parameter", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_Integer:
    return psprintf("%ld", (long)intVal(def->arg));
  case T_Float:

       
                                                                  
                                                 
       
    return strVal(def->arg);
  case T_String:
    return strVal(def->arg);
  case T_TypeName:
    return TypeNameToString((TypeName *)def->arg);
  case T_List:
    return NameListToString((List *)def->arg);
  case T_A_Star:
    return pstrdup("*");
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(def->arg));
  }
  return NULL;                          
}

   
                                                             
   
double
defGetNumeric(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a numeric value", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_Integer:
    return (double)intVal(def->arg);
  case T_Float:
    return floatVal(def->arg);
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a numeric value", def->defname)));
  }
  return 0;                          
}

   
                                           
   
bool
defGetBoolean(DefElem *def)
{
     
                                                    
     
  if (def->arg == NULL)
  {
    return true;
  }

     
                                              
     
  switch (nodeTag(def->arg))
  {
  case T_Integer:
    switch (intVal(def->arg))
    {
    case 0:
      return false;
    case 1:
      return true;
    default:
                                      
      break;
    }
    break;
  default:
  {
    char *sval = defGetString(def);

       
                                                                 
                                         
       
    if (pg_strcasecmp(sval, "true") == 0)
    {
      return true;
    }
    if (pg_strcasecmp(sval, "false") == 0)
    {
      return false;
    }
    if (pg_strcasecmp(sval, "on") == 0)
    {
      return true;
    }
    if (pg_strcasecmp(sval, "off") == 0)
    {
      return false;
    }
  }
  break;
  }
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a Boolean value", def->defname)));
  return false;                          
}

   
                                          
   
int32
defGetInt32(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires an integer value", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_Integer:
    return (int32)intVal(def->arg);
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires an integer value", def->defname)));
  }
  return 0;                          
}

   
                                          
   
int64
defGetInt64(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a numeric value", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_Integer:
    return (int64)intVal(def->arg);
  case T_Float:

       
                                                              
                                                                    
                
       
    return DatumGetInt64(DirectFunctionCall1(int8in, CStringGetDatum(strVal(def->arg))));
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a numeric value", def->defname)));
  }
  return 0;                          
}

   
                                                                            
   
List *
defGetQualifiedName(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a parameter", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_TypeName:
    return ((TypeName *)def->arg)->names;
  case T_List:
    return (List *)def->arg;
  case T_String:
                                                       
    return list_make1(def->arg);
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("argument of %s must be a name", def->defname)));
  }
  return NIL;                          
}

   
                                      
   
                                                                        
                                                                 
   
TypeName *
defGetTypeName(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a parameter", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_TypeName:
    return (TypeName *)def->arg;
  case T_String:
                                                           
    return makeTypeNameFromNameList(list_make1(def->arg));
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("argument of %s must be a type name", def->defname)));
  }
  return NULL;                          
}

   
                                                              
                                      
   
int
defGetTypeLength(DefElem *def)
{
  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a parameter", def->defname)));
  }
  switch (nodeTag(def->arg))
  {
  case T_Integer:
    return intVal(def->arg);
  case T_Float:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires an integer value", def->defname)));
    break;
  case T_String:
    if (pg_strcasecmp(strVal(def->arg), "variable") == 0)
    {
      return -1;                      
    }
    break;
  case T_TypeName:
                                                                     
    if (pg_strcasecmp(TypeNameToString((TypeName *)def->arg), "variable") == 0)
    {
      return -1;                      
    }
    break;
  case T_List:
                                  
    break;
  default:
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(def->arg));
  }
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid argument for %s: \"%s\"", def->defname, defGetString(def))));
  return 0;                          
}

   
                                                                             
   
List *
defGetStringList(DefElem *def)
{
  ListCell *cell;

  if (def->arg == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s requires a parameter", def->defname)));
  }
  if (nodeTag(def->arg) != T_List)
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(def->arg));
  }

  foreach (cell, (List *)def->arg)
  {
    Node *str = (Node *)lfirst(cell);

    if (!IsA(str, String))
    {
      elog(ERROR, "unexpected node type in name list: %d", (int)nodeTag(str));
    }
  }

  return (List *)def->arg;
}
