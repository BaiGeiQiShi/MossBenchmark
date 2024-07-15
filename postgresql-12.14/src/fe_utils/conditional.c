                                                                            
                                                              
   
                                                                
   
                              
   
                                                                            
   
#include "postgres_fe.h"

#include "fe_utils/conditional.h"

   
                
   
ConditionalStack
conditional_stack_create(void)
{
  ConditionalStack cstack = pg_malloc(sizeof(ConditionalStackData));

  cstack->head = NULL;
  return cstack;
}

   
                 
   
void
conditional_stack_destroy(ConditionalStack cstack)
{
  while (conditional_stack_pop(cstack))
  {
    continue;
  }
  free(cstack);
}

   
                                    
   
void
conditional_stack_push(ConditionalStack cstack, ifState new_state)
{
  IfStackElem *p = (IfStackElem *)pg_malloc(sizeof(IfStackElem));

  p->if_state = new_state;
  p->query_len = -1;
  p->paren_depth = -1;
  p->next = cstack->head;
  cstack->head = p;
}

   
                                           
                                                
   
bool
conditional_stack_pop(ConditionalStack cstack)
{
  IfStackElem *p = cstack->head;

  if (!p)
  {
    return false;
  }
  cstack->head = cstack->head->next;
  free(p);
  return true;
}

   
                                                        
   
int
conditional_stack_depth(ConditionalStack cstack)
{
  if (cstack == NULL)
  {
    return -1;
  }
  else
  {
    IfStackElem *p = cstack->head;
    int depth = 0;

    while (p != NULL)
    {
      depth++;
      p = p->next;
    }
    return depth;
  }
}

   
                                                    
   
ifState
conditional_stack_peek(ConditionalStack cstack)
{
  if (conditional_stack_empty(cstack))
  {
    return IFSTATE_NONE;
  }
  return cstack->head->if_state;
}

   
                                           
                                                      
   
bool
conditional_stack_poke(ConditionalStack cstack, ifState new_state)
{
  if (conditional_stack_empty(cstack))
  {
    return false;
  }
  cstack->head->if_state = new_state;
  return true;
}

   
                                           
   
bool
conditional_stack_empty(ConditionalStack cstack)
{
  return cstack->head == NULL;
}

   
                                                                     
                                                                
   
bool
conditional_active(ConditionalStack cstack)
{
  ifState s = conditional_stack_peek(cstack);

  return s == IFSTATE_NONE || s == IFSTATE_TRUE || s == IFSTATE_ELSE_TRUE;
}

   
                                                            
   
void
conditional_stack_set_query_len(ConditionalStack cstack, int len)
{
  Assert(!conditional_stack_empty(cstack));
  cstack->head->query_len = len;
}

   
                                                                     
                                                     
   
int
conditional_stack_get_query_len(ConditionalStack cstack)
{
  if (conditional_stack_empty(cstack))
  {
    return -1;
  }
  return cstack->head->query_len;
}

   
                                                                  
   
void
conditional_stack_set_paren_depth(ConditionalStack cstack, int depth)
{
  Assert(!conditional_stack_empty(cstack));
  cstack->head->paren_depth = depth;
}

   
                                                                           
                                                     
   
int
conditional_stack_get_paren_depth(ConditionalStack cstack)
{
  if (conditional_stack_empty(cstack))
  {
    return -1;
  }
  return cstack->head->paren_depth;
}
