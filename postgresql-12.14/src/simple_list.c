                                                                            
   
                                            
   
                                                                          
                                                                          
                                     
   
   
                                                                         
                                                                        
   
                              
   
                                                                            
   
#include "postgres_fe.h"

#include "fe_utils/simple_list.h"

   
                              
   
void
simple_oid_list_append(SimpleOidList *list, Oid val)
{
  SimpleOidListCell *cell;

  cell = (SimpleOidListCell *)pg_malloc(sizeof(SimpleOidListCell));
  cell->next = NULL;
  cell->val = val;

  if (list->tail)
  {
    list->tail->next = cell;
  }
  else
  {
    list->head = cell;
  }
  list->tail = cell;
}

   
                               
   
bool
simple_oid_list_member(SimpleOidList *list, Oid val)
{
  SimpleOidListCell *cell;

  for (cell = list->head; cell; cell = cell->next)
  {
    if (cell->val == val)
    {
      return true;
    }
  }
  return false;
}

   
                                
   
                                                                     
   
void
simple_string_list_append(SimpleStringList *list, const char *val)
{
  SimpleStringListCell *cell;

  cell = (SimpleStringListCell *)pg_malloc(offsetof(SimpleStringListCell, val) + strlen(val) + 1);

  cell->next = NULL;
  cell->touched = false;
  strcpy(cell->val, val);

  if (list->tail)
  {
    list->tail->next = cell;
  }
  else
  {
    list->head = cell;
  }
  list->tail = cell;
}

   
                                  
   
                                                                 
   
bool
simple_string_list_member(SimpleStringList *list, const char *val)
{
  SimpleStringListCell *cell;

  for (cell = list->head; cell; cell = cell->next)
  {
    if (strcmp(cell->val, val) == 0)
    {
      cell->touched = true;
      return true;
    }
  }
  return false;
}

   
                                                       
   
const char *
simple_string_list_not_touched(SimpleStringList *list)
{
  SimpleStringListCell *cell;

  for (cell = list->head; cell; cell = cell->next)
  {
    if (!cell->touched)
    {
      return cell->val;
    }
  }
  return NULL;
}

   
                                 
   
                                                      
   
void
simple_ptr_list_append(SimplePtrList *list, void *ptr)
{
  SimplePtrListCell *cell;

  cell = (SimplePtrListCell *)pg_malloc(sizeof(SimplePtrListCell));
  cell->next = NULL;
  cell->ptr = ptr;

  if (list->tail)
  {
    list->tail->next = cell;
  }
  else
  {
    list->head = cell;
  }
  list->tail = cell;
}
