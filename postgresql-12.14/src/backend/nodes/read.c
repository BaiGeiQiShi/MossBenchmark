                                                                            
   
          
                                                                            
              
   
                                                                         
                                                                        
   
   
                  
                              
   
           
                                 
                                            
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "common/string.h"
#include "nodes/pg_list.h"
#include "nodes/readfuncs.h"
#include "nodes/value.h"

                                
static const char *pg_strtok_ptr = NULL;

                                                                             
#ifdef WRITE_READ_PARSE_PLAN_TREES
bool restore_location_fields = false;
#endif

   
                  
                                                                       
   
                                                                        
                                                                        
                                                                      
   
static void *
stringToNodeInternal(const char *str, bool restore_loc_fields)
{
  void *retval;
  const char *save_strtok;
#ifdef WRITE_READ_PARSE_PLAN_TREES
  bool save_restore_location_fields;
#endif

     
                                                                             
                                                                             
                                                                       
                                                      
     
  save_strtok = pg_strtok_ptr;

  pg_strtok_ptr = str;                                            

     
                                                                         
     
#ifdef WRITE_READ_PARSE_PLAN_TREES
  save_restore_location_fields = restore_location_fields;
  restore_location_fields = restore_loc_fields;
#endif

  retval = nodeRead(NULL, 0);                     

  pg_strtok_ptr = save_strtok;

#ifdef WRITE_READ_PARSE_PLAN_TREES
  restore_location_fields = save_restore_location_fields;
#endif

  return retval;
}

   
                                   
   
void *
stringToNode(const char *str)
{
  return stringToNodeInternal(str, false);
}

#ifdef WRITE_READ_PARSE_PLAN_TREES

void *
stringToNodeWithLocations(const char *str)
{
  return stringToNodeInternal(str, true);
}

#endif

                                                                               
   
                         
   
                                                                               

   
                                                      
   
                                                                        
                                                                      
                               
                                                                          
                                                          
   
                                                                           
   
                             
                                                               
                                                                   
                                         
                                                                        
                                                          
                                                                        
                                                                           
                                                                      
                                                                       
                                                                        
                                                                         
                                                       
   
                                                                           
                                                                         
                                            
   
                                                                        
                                     
   
                                                                             
                                                                          
                                                                            
                                                                          
                                                                             
                                                                            
                      
   
const char *
pg_strtok(int *length)
{
  const char *local_str;                                
  const char *ret_str;                                 

  local_str = pg_strtok_ptr;

  while (*local_str == ' ' || *local_str == '\n' || *local_str == '\t')
  {
    local_str++;
  }

  if (*local_str == '\0')
  {
    *length = 0;
    pg_strtok_ptr = local_str;
    return NULL;                     
  }

     
                                          
     
  ret_str = local_str;

  if (*local_str == '(' || *local_str == ')' || *local_str == '{' || *local_str == '}')
  {
                                   
    local_str++;
  }
  else
  {
                                                       
    while (*local_str != '\0' && *local_str != ' ' && *local_str != '\n' && *local_str != '\t' && *local_str != '(' && *local_str != ')' && *local_str != '{' && *local_str != '}')
    {
      if (*local_str == '\\' && local_str[1] != '\0')
      {
        local_str += 2;
      }
      else
      {
        local_str++;
      }
    }
  }

  *length = local_str - ret_str;

                                                
  if (*length == 2 && ret_str[0] == '<' && ret_str[1] == '>')
  {
    *length = 0;
  }

  pg_strtok_ptr = local_str;

  return ret_str;
}

   
                 
                                                       
                                                          
   
char *
debackslash(const char *token, int length)
{
  char *result = palloc(length + 1);
  char *ptr = result;

  while (length > 0)
  {
    if (*token == '\\' && length > 1)
    {
      token++, length--;
    }
    *ptr++ = *token++;
    length--;
  }
  *ptr = '\0';
  return result;
}

#define RIGHT_PAREN (1000000 + 1)
#define LEFT_PAREN (1000000 + 2)
#define LEFT_BRACE (1000000 + 3)
#define OTHER_TOKEN (1000000 + 4)

   
                   
                                                            
                                                     
                                              
                          
                                                     
   
                                                   
   
static NodeTag
nodeTokenType(const char *token, int length)
{
  NodeTag retval;
  const char *numptr;
  int numlen;

     
                                    
     
  numptr = token;
  numlen = length;
  if (*numptr == '+' || *numptr == '-')
  {
    numptr++, numlen--;
  }
  if ((numlen > 0 && isdigit((unsigned char)*numptr)) || (numlen > 1 && *numptr == '.' && isdigit((unsigned char)numptr[1])))
  {
       
                                                                       
                                                                         
                                                                        
                                                        
       
    char *endptr;

    errno = 0;
    (void)strtoint(token, &endptr, 10);
    if (endptr != token + length || errno == ERANGE)
    {
      return T_Float;
    }
    return T_Integer;
  }

     
                                                                         
                                             
     
  else if (*token == '(')
  {
    retval = LEFT_PAREN;
  }
  else if (*token == ')')
  {
    retval = RIGHT_PAREN;
  }
  else if (*token == '{')
  {
    retval = LEFT_BRACE;
  }
  else if (*token == '"' && length > 1 && token[length - 1] == '"')
  {
    retval = T_String;
  }
  else if (*token == 'b')
  {
    retval = T_BitString;
  }
  else
  {
    retval = OTHER_TOKEN;
  }
  return retval;
}

   
              
                                   
   
                                                                     
                                                
                                                       
                                                             
                         
                                
                                                                       
                                                                           
   
                                                                             
                                                                             
                                                       
   
                                                                           
                                                                      
   
void *
nodeRead(const char *token, int tok_len)
{
  Node *result;
  NodeTag type;

  if (token == NULL)                            
  {
    token = pg_strtok(&tok_len);

    if (token == NULL)                   
    {
      return NULL;
    }
  }

  type = nodeTokenType(token, tok_len);

  switch ((int)type)
  {
  case LEFT_BRACE:
    result = parseNodeString();
    token = pg_strtok(&tok_len);
    if (token == NULL || token[0] != '}')
    {
      elog(ERROR, "did not find '}' at end of input node");
    }
    break;
  case LEFT_PAREN:
  {
    List *l = NIL;

                 
                                                 
                                          
                                                  
                 
       
    token = pg_strtok(&tok_len);
    if (token == NULL)
    {
      elog(ERROR, "unterminated List structure");
    }
    if (tok_len == 1 && token[0] == 'i')
    {
                            
      for (;;)
      {
        int val;
        char *endptr;

        token = pg_strtok(&tok_len);
        if (token == NULL)
        {
          elog(ERROR, "unterminated List structure");
        }
        if (token[0] == ')')
        {
          break;
        }
        val = (int)strtol(token, &endptr, 10);
        if (endptr != token + tok_len)
        {
          elog(ERROR, "unrecognized integer: \"%.*s\"", tok_len, token);
        }
        l = lappend_int(l, val);
      }
    }
    else if (tok_len == 1 && token[0] == 'o')
    {
                        
      for (;;)
      {
        Oid val;
        char *endptr;

        token = pg_strtok(&tok_len);
        if (token == NULL)
        {
          elog(ERROR, "unterminated List structure");
        }
        if (token[0] == ')')
        {
          break;
        }
        val = (Oid)strtoul(token, &endptr, 10);
        if (endptr != token + tok_len)
        {
          elog(ERROR, "unrecognized OID: \"%.*s\"", tok_len, token);
        }
        l = lappend_oid(l, val);
      }
    }
    else
    {
                                    
      for (;;)
      {
                                                   
        if (token[0] == ')')
        {
          break;
        }
        l = lappend(l, nodeRead(token, tok_len));
        token = pg_strtok(&tok_len);
        if (token == NULL)
        {
          elog(ERROR, "unterminated List structure");
        }
      }
    }
    result = (Node *)l;
    break;
  }
  case RIGHT_PAREN:
    elog(ERROR, "unexpected right parenthesis");
    result = NULL;                          
    break;
  case OTHER_TOKEN:
    if (tok_len == 0)
    {
                                                      
      result = NULL;
    }
    else
    {
      elog(ERROR, "unrecognized token: \"%.*s\"", tok_len, token);
      result = NULL;                          
    }
    break;
  case T_Integer:

       
                                                                     
       
    result = (Node *)makeInteger(atoi(token));
    break;
  case T_Float:
  {
    char *fval = (char *)palloc(tok_len + 1);

    memcpy(fval, token, tok_len);
    fval[tok_len] = '\0';
    result = (Node *)makeFloat(fval);
  }
  break;
  case T_String:
                                                                     
    result = (Node *)makeString(debackslash(token + 1, tok_len - 2));
    break;
  case T_BitString:
  {
    char *val = palloc(tok_len);

                          
    memcpy(val, token + 1, tok_len - 1);
    val[tok_len - 1] = '\0';
    result = (Node *)makeBitString(val);
    break;
  }
  default:
    elog(ERROR, "unrecognized node type: %d", (int)type);
    result = NULL;                          
    break;
  }

  return (void *)result;
}
