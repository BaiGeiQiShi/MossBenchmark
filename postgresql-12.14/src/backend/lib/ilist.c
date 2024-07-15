                                                                            
   
           
                                                                    
   
                                                                         
                                                                        
   
   
                  
                             
   
         
                                                                         
                                                         
   
                                                                            
   
#include "postgres.h"

#include "lib/ilist.h"

   
                            
   
                                                                        
   
                                                                         
   
void
slist_delete(slist_head *head, slist_node *node)
{
  slist_node *last = &head->head;
  slist_node *cur;
  bool found PG_USED_FOR_ASSERTS_ONLY = false;

  while ((cur = last->next) != NULL)
  {
    if (cur == node)
    {
      last->next = cur->next;
#ifdef USE_ASSERT_CHECKING
      found = true;
#endif
      break;
    }
    last = cur;
  }
  Assert(found);

  slist_check(head);
}

#ifdef ILIST_DEBUG
   
                                            
   
void
dlist_check(dlist_head *head)
{
  dlist_node *cur;

  if (head == NULL)
  {
    elog(ERROR, "doubly linked list head address is NULL");
  }

  if (head->head.next == NULL && head->head.prev == NULL)
  {
    return;                                
  }

                                    
  for (cur = head->head.next; cur != &head->head; cur = cur->next)
  {
    if (cur == NULL || cur->next == NULL || cur->prev == NULL || cur->prev->next != cur || cur->next->prev != cur)
    {
      elog(ERROR, "doubly linked list is corrupted");
    }
  }

                                     
  for (cur = head->head.prev; cur != &head->head; cur = cur->prev)
  {
    if (cur == NULL || cur->next == NULL || cur->prev == NULL || cur->prev->next != cur || cur->next->prev != cur)
    {
      elog(ERROR, "doubly linked list is corrupted");
    }
  }
}

   
                                            
   
void
slist_check(slist_head *head)
{
  slist_node *cur;

  if (head == NULL)
  {
    elog(ERROR, "singly linked list head address is NULL");
  }

     
                                                                         
                                                                       
     
  for (cur = head->head.next; cur != NULL; cur = cur->next)
    ;
}

#endif                  
