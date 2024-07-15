                                                                            
   
          
                                                               
   
   
                                                                         
                                                                        
   
   
                  
                              
   
                                                                            
   
#include "postgres.h"

#include "nodes/pg_list.h"

   
                                                                       
                                                           
   
#define IsPointerList(l) ((l) == NIL || IsA((l), List))
#define IsIntegerList(l) ((l) == NIL || IsA((l), IntList))
#define IsOidList(l) ((l) == NIL || IsA((l), OidList))

#ifdef USE_ASSERT_CHECKING
   
                                                                   
   
static void
check_list_invariants(const List *list)
{
  if (list == NIL)
  {
    return;
  }

  Assert(list->length > 0);
  Assert(list->head != NULL);
  Assert(list->tail != NULL);

  Assert(list->type == T_List || list->type == T_IntList || list->type == T_OidList);

  if (list->length == 1)
  {
    Assert(list->head == list->tail);
  }
  if (list->length == 2)
  {
    Assert(list->head->next == list->tail);
  }
  Assert(list->tail->next == NULL);
}
#else
#define check_list_invariants(l)
#endif                          

   
                                                                  
                                                                     
                                                          
   
static List *
new_list(NodeTag type)
{
  List *new_list;
  ListCell *new_head;

  new_head = (ListCell *)palloc(sizeof(*new_head));
  new_head->next = NULL;
                                         

  new_list = (List *)palloc(sizeof(*new_list));
  new_list->type = type;
  new_list->length = 1;
  new_list->head = new_head;
  new_list->tail = new_head;

  return new_list;
}

   
                                                             
                                                   
   
                                                                    
                      
   
static void
new_head_cell(List *list)
{
  ListCell *new_head;

  new_head = (ListCell *)palloc(sizeof(*new_head));
  new_head->next = list->head;

  list->head = new_head;
  list->length++;
}

   
                                                             
                                                   
   
                                                                    
                      
   
static void
new_tail_cell(List *list)
{
  ListCell *new_tail;

  new_tail = (ListCell *)palloc(sizeof(*new_tail));
  new_tail->next = NULL;

  list->tail->next = new_tail;
  list->tail = new_tail;
  list->length++;
}

   
                                                                   
                                                                  
                                                                     
                                                                  
                   
   
List *
lappend(List *list, void *datum)
{
  Assert(IsPointerList(list));

  if (list == NIL)
  {
    list = new_list(T_List);
  }
  else
  {
    new_tail_cell(list);
  }

  lfirst(list->tail) = datum;
  check_list_invariants(list);
  return list;
}

   
                                                          
   
List *
lappend_int(List *list, int datum)
{
  Assert(IsIntegerList(list));

  if (list == NIL)
  {
    list = new_list(T_IntList);
  }
  else
  {
    new_tail_cell(list);
  }

  lfirst_int(list->tail) = datum;
  check_list_invariants(list);
  return list;
}

   
                                                      
   
List *
lappend_oid(List *list, Oid datum)
{
  Assert(IsOidList(list));

  if (list == NIL)
  {
    list = new_list(T_OidList);
  }
  else
  {
    new_tail_cell(list);
  }

  lfirst_oid(list->tail) = datum;
  check_list_invariants(list);
  return list;
}

   
                                                                      
                                                                    
                                                                       
                                          
   
static ListCell *
add_new_cell(List *list, ListCell *prev_cell)
{
  ListCell *new_cell;

  new_cell = (ListCell *)palloc(sizeof(*new_cell));
                                         
  new_cell->next = prev_cell->next;
  prev_cell->next = new_cell;

  if (list->tail == prev_cell)
  {
    list->tail = new_cell;
  }

  list->length++;

  return new_cell;
}

   
                                                                 
                                                               
                                                                     
                                                       
   
ListCell *
lappend_cell(List *list, ListCell *prev, void *datum)
{
  ListCell *new_cell;

  Assert(IsPointerList(list));

  new_cell = add_new_cell(list, prev);
  lfirst(new_cell) = datum;
  check_list_invariants(list);
  return new_cell;
}

ListCell *
lappend_cell_int(List *list, ListCell *prev, int datum)
{
  ListCell *new_cell;

  Assert(IsIntegerList(list));

  new_cell = add_new_cell(list, prev);
  lfirst_int(new_cell) = datum;
  check_list_invariants(list);
  return new_cell;
}

ListCell *
lappend_cell_oid(List *list, ListCell *prev, Oid datum)
{
  ListCell *new_cell;

  Assert(IsOidList(list));

  new_cell = add_new_cell(list, prev);
  lfirst_oid(new_cell) = datum;
  check_list_invariants(list);
  return new_cell;
}

   
                                                                     
                                                                     
                                                                     
                                                                  
                    
   
                                                                      
                                                                           
             
   
List *
lcons(void *datum, List *list)
{
  Assert(IsPointerList(list));

  if (list == NIL)
  {
    list = new_list(T_List);
  }
  else
  {
    new_head_cell(list);
  }

  lfirst(list->head) = datum;
  check_list_invariants(list);
  return list;
}

   
                                               
   
List *
lcons_int(int datum, List *list)
{
  Assert(IsIntegerList(list));

  if (list == NIL)
  {
    list = new_list(T_IntList);
  }
  else
  {
    new_head_cell(list);
  }

  lfirst_int(list->head) = datum;
  check_list_invariants(list);
  return list;
}

   
                                           
   
List *
lcons_oid(Oid datum, List *list)
{
  Assert(IsOidList(list));

  if (list == NIL)
  {
    list = new_list(T_OidList);
  }
  else
  {
    new_head_cell(list);
  }

  lfirst_oid(list->head) = datum;
  check_list_invariants(list);
  return list;
}

   
                                                                     
                                                                   
                                                                  
                                                                     
   
                                                                       
                                                                       
                                                                
                                  
   
List *
list_concat(List *list1, List *list2)
{
  if (list1 == NIL)
  {
    return list2;
  }
  if (list2 == NIL)
  {
    return list1;
  }
  if (list1 == list2)
  {
    elog(ERROR, "cannot list_concat() a list to itself");
  }

  Assert(list1->type == list2->type);

  list1->length += list2->length;
  list1->tail->next = list2->head;
  list1->tail = list2->tail;

  check_list_invariants(list1);
  return list1;
}

   
                                                                     
                                                                    
                                                                     
                                                                 
           
   
                                                                   
   
List *
list_truncate(List *list, int new_size)
{
  ListCell *cell;
  int n;

  if (new_size <= 0)
  {
    return NIL;                              
  }

                                                           
  if (new_size >= list_length(list))
  {
    return list;
  }

  n = 1;
  foreach (cell, list)
  {
    if (n == new_size)
    {
      cell->next = NULL;
      list->tail = cell;
      list->length = new_size;
      check_list_invariants(list);
      return list;
    }
    n++;
  }

                                              
  Assert(false);
  return list;
}

   
                                                                           
                                     
   
ListCell *
list_nth_cell(const List *list, int n)
{
  ListCell *match;

  Assert(list != NIL);
  Assert(n >= 0);
  Assert(n < list->length);
  check_list_invariants(list);

                                                        
  if (n == list->length - 1)
  {
    return list->tail;
  }

  for (match = list->head; n-- > 0; match = match->next)
    ;

  return match;
}

   
                                                              
                                               
   
void *
list_nth(const List *list, int n)
{
  Assert(IsPointerList(list));
  return lfirst(list_nth_cell(list, n));
}

   
                                                                 
                   
   
int
list_nth_int(const List *list, int n)
{
  Assert(IsIntegerList(list));
  return lfirst_int(list_nth_cell(list, n));
}

   
                                                                       
         
   
Oid
list_nth_oid(const List *list, int n)
{
  Assert(IsOidList(list));
  return lfirst_oid(list_nth_cell(list, n));
}

   
                                                                
                                                                     
                    
   
bool
list_member(const List *list, const void *datum)
{
  const ListCell *cell;

  Assert(IsPointerList(list));
  check_list_invariants(list);

  foreach (cell, list)
  {
    if (equal(lfirst(cell), datum))
    {
      return true;
    }
  }

  return false;
}

   
                                                                
                                                  
   
bool
list_member_ptr(const List *list, const void *datum)
{
  const ListCell *cell;

  Assert(IsPointerList(list));
  check_list_invariants(list);

  foreach (cell, list)
  {
    if (lfirst(cell) == datum)
    {
      return true;
    }
  }

  return false;
}

   
                                                                
   
bool
list_member_int(const List *list, int datum)
{
  const ListCell *cell;

  Assert(IsIntegerList(list));
  check_list_invariants(list);

  foreach (cell, list)
  {
    if (lfirst_int(cell) == datum)
    {
      return true;
    }
  }

  return false;
}

   
                                                            
   
bool
list_member_oid(const List *list, Oid datum)
{
  const ListCell *cell;

  Assert(IsOidList(list));
  check_list_invariants(list);

  foreach (cell, list)
  {
    if (lfirst_oid(cell) == datum)
    {
      return true;
    }
  }

  return false;
}

   
                                                                       
                                                                
   
                                                                           
   
List *
list_delete_cell(List *list, ListCell *cell, ListCell *prev)
{
  check_list_invariants(list);
  Assert(prev != NULL ? lnext(prev) == cell : list_head(list) == cell);

     
                                                                          
                                                                            
                         
     
  if (list->length == 1)
  {
    list_free(list);
    return NIL;
  }

     
                                                                           
                                                                   
     
  list->length--;

  if (prev)
  {
    prev->next = cell->next;
  }
  else
  {
    list->head = cell->next;
  }

  if (list->tail == cell)
  {
    list->tail = prev;
  }

  pfree(cell);
  return list;
}

   
                                                             
                                       
   
List *
list_delete(List *list, void *datum)
{
  ListCell *cell;
  ListCell *prev;

  Assert(IsPointerList(list));
  check_list_invariants(list);

  prev = NULL;
  foreach (cell, list)
  {
    if (equal(lfirst(cell), datum))
    {
      return list_delete_cell(list, cell, prev);
    }

    prev = cell;
  }

                                                       
  return list;
}

                                               
List *
list_delete_ptr(List *list, void *datum)
{
  ListCell *cell;
  ListCell *prev;

  Assert(IsPointerList(list));
  check_list_invariants(list);

  prev = NULL;
  foreach (cell, list)
  {
    if (lfirst(cell) == datum)
    {
      return list_delete_cell(list, cell, prev);
    }

    prev = cell;
  }

                                                       
  return list;
}

                                
List *
list_delete_int(List *list, int datum)
{
  ListCell *cell;
  ListCell *prev;

  Assert(IsIntegerList(list));
  check_list_invariants(list);

  prev = NULL;
  foreach (cell, list)
  {
    if (lfirst_int(cell) == datum)
    {
      return list_delete_cell(list, cell, prev);
    }

    prev = cell;
  }

                                                       
  return list;
}

                            
List *
list_delete_oid(List *list, Oid datum)
{
  ListCell *cell;
  ListCell *prev;

  Assert(IsOidList(list));
  check_list_invariants(list);

  prev = NULL;
  foreach (cell, list)
  {
    if (lfirst_oid(cell) == datum)
    {
      return list_delete_cell(list, cell, prev);
    }

    prev = cell;
  }

                                                       
  return list;
}

   
                                         
   
                                                                            
                                                                       
                                                                            
                                                                   
   
List *
list_delete_first(List *list)
{
  check_list_invariants(list);

  if (list == NIL)
  {
    return NIL;                                
  }

  return list_delete_cell(list, list_head(list), NULL);
}

   
                                                                  
                                                                     
                                 
   
                                                                    
                
   
                                                                     
                                                                   
   
                                                                     
                                                                       
                                                                        
                                                                          
                                     
   
                                                                       
                           
   
List *
list_union(const List *list1, const List *list2)
{
  List *result;
  const ListCell *cell;

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  result = list_copy(list1);
  foreach (cell, list2)
  {
    if (!list_member(result, lfirst(cell)))
    {
      result = lappend(result, lfirst(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                 
                       
   
List *
list_union_ptr(const List *list1, const List *list2)
{
  List *result;
  const ListCell *cell;

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  result = list_copy(list1);
  foreach (cell, list2)
  {
    if (!list_member_ptr(result, lfirst(cell)))
    {
      result = lappend(result, lfirst(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                 
   
List *
list_union_int(const List *list1, const List *list2)
{
  List *result;
  const ListCell *cell;

  Assert(IsIntegerList(list1));
  Assert(IsIntegerList(list2));

  result = list_copy(list1);
  foreach (cell, list2)
  {
    if (!list_member_int(result, lfirst_int(cell)))
    {
      result = lappend_int(result, lfirst_int(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                             
   
List *
list_union_oid(const List *list1, const List *list2)
{
  List *result;
  const ListCell *cell;

  Assert(IsOidList(list1));
  Assert(IsOidList(list2));

  result = list_copy(list1);
  foreach (cell, list2)
  {
    if (!list_member_oid(result, lfirst_oid(cell)))
    {
      result = lappend_oid(result, lfirst_oid(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                        
                                                                        
                                                                  
                
   
                                                                          
                                                       
   
                                                                
                                                                       
                     
   
List *
list_intersection(const List *list1, const List *list2)
{
  List *result;
  const ListCell *cell;

  if (list1 == NIL || list2 == NIL)
  {
    return NIL;
  }

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  result = NIL;
  foreach (cell, list1)
  {
    if (list_member(list2, lfirst(cell)))
    {
      result = lappend(result, lfirst(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                           
   
List *
list_intersection_int(const List *list1, const List *list2)
{
  List *result;
  const ListCell *cell;

  if (list1 == NIL || list2 == NIL)
  {
    return NIL;
  }

  Assert(IsIntegerList(list1));
  Assert(IsIntegerList(list2));

  result = NIL;
  foreach (cell, list1)
  {
    if (list_member_int(list2, lfirst_int(cell)))
    {
      result = lappend_int(result, lfirst_int(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                      
                                                                       
                                                                  
                
   
                                                                
                          
   
List *
list_difference(const List *list1, const List *list2)
{
  const ListCell *cell;
  List *result = NIL;

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  if (list2 == NIL)
  {
    return list_copy(list1);
  }

  foreach (cell, list1)
  {
    if (!list_member(list2, lfirst(cell)))
    {
      result = lappend(result, lfirst(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                    
                            
   
List *
list_difference_ptr(const List *list1, const List *list2)
{
  const ListCell *cell;
  List *result = NIL;

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  if (list2 == NIL)
  {
    return list_copy(list1);
  }

  foreach (cell, list1)
  {
    if (!list_member_ptr(list2, lfirst(cell)))
    {
      result = lappend(result, lfirst(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                      
   
List *
list_difference_int(const List *list1, const List *list2)
{
  const ListCell *cell;
  List *result = NIL;

  Assert(IsIntegerList(list1));
  Assert(IsIntegerList(list2));

  if (list2 == NIL)
  {
    return list_copy(list1);
  }

  foreach (cell, list1)
  {
    if (!list_member_int(list2, lfirst_int(cell)))
    {
      result = lappend_int(result, lfirst_int(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                  
   
List *
list_difference_oid(const List *list1, const List *list2)
{
  const ListCell *cell;
  List *result = NIL;

  Assert(IsOidList(list1));
  Assert(IsOidList(list2));

  if (list2 == NIL)
  {
    return list_copy(list1);
  }

  foreach (cell, list1)
  {
    if (!list_member_oid(list2, lfirst_oid(cell)))
    {
      result = lappend_oid(result, lfirst_oid(cell));
    }
  }

  check_list_invariants(result);
  return result;
}

   
                                                                   
   
                                                                    
                
   
List *
list_append_unique(List *list, void *datum)
{
  if (list_member(list, datum))
  {
    return list;
  }
  else
  {
    return lappend(list, datum);
  }
}

   
                                                                       
                            
   
List *
list_append_unique_ptr(List *list, void *datum)
{
  if (list_member_ptr(list, datum))
  {
    return list;
  }
  else
  {
    return lappend(list, datum);
  }
}

   
                                                                         
   
List *
list_append_unique_int(List *list, int datum)
{
  if (list_member_int(list, datum))
  {
    return list;
  }
  else
  {
    return lappend_int(list, datum);
  }
}

   
                                                                     
   
List *
list_append_unique_oid(List *list, Oid datum)
{
  if (list_member_oid(list, datum))
  {
    return list;
  }
  else
  {
    return lappend_oid(list, datum);
  }
}

   
                                                                     
   
                                                                    
                
   
                                                                       
                                                                        
                                                                            
                                                                            
                                                                        
                                
   
List *
list_concat_unique(List *list1, List *list2)
{
  ListCell *cell;

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  foreach (cell, list2)
  {
    if (!list_member(list1, lfirst(cell)))
    {
      list1 = lappend(list1, lfirst(cell));
    }
  }

  check_list_invariants(list1);
  return list1;
}

   
                                                                       
                            
   
List *
list_concat_unique_ptr(List *list1, List *list2)
{
  ListCell *cell;

  Assert(IsPointerList(list1));
  Assert(IsPointerList(list2));

  foreach (cell, list2)
  {
    if (!list_member_ptr(list1, lfirst(cell)))
    {
      list1 = lappend(list1, lfirst(cell));
    }
  }

  check_list_invariants(list1);
  return list1;
}

   
                                                                         
   
List *
list_concat_unique_int(List *list1, List *list2)
{
  ListCell *cell;

  Assert(IsIntegerList(list1));
  Assert(IsIntegerList(list2));

  foreach (cell, list2)
  {
    if (!list_member_int(list1, lfirst_int(cell)))
    {
      list1 = lappend_int(list1, lfirst_int(cell));
    }
  }

  check_list_invariants(list1);
  return list1;
}

   
                                                                     
   
List *
list_concat_unique_oid(List *list1, List *list2)
{
  ListCell *cell;

  Assert(IsOidList(list1));
  Assert(IsOidList(list2));

  foreach (cell, list2)
  {
    if (!list_member_oid(list1, lfirst_oid(cell)))
    {
      list1 = lappend_oid(list1, lfirst_oid(cell));
    }
  }

  check_list_invariants(list1);
  return list1;
}

   
                                                                      
   
static void
list_free_private(List *list, bool deep)
{
  ListCell *cell;

  check_list_invariants(list);

  cell = list_head(list);
  while (cell != NULL)
  {
    ListCell *tmp = cell;

    cell = lnext(cell);
    if (deep)
    {
      pfree(lfirst(tmp));
    }
    pfree(tmp);
  }

  if (list)
  {
    pfree(list);
  }
}

   
                                                                   
                                                                
           
   
                                                                   
                                                            
   
void
list_free(List *list)
{
  list_free_private(list, false);
}

   
                                                                
                                                                    
                                                                  
   
                                                                   
                                                            
   
void
list_free_deep(List *list)
{
     
                                                                     
     
  Assert(IsPointerList(list));
  list_free_private(list, true);
}

   
                                                
   
List *
list_copy(const List *oldlist)
{
  List *newlist;
  ListCell *newlist_prev;
  ListCell *oldlist_cur;

  if (oldlist == NIL)
  {
    return NIL;
  }

  newlist = new_list(oldlist->type);
  newlist->length = oldlist->length;

     
                                                                            
                          
     
  newlist->head->data = oldlist->head->data;

  newlist_prev = newlist->head;
  oldlist_cur = oldlist->head->next;
  while (oldlist_cur)
  {
    ListCell *newlist_cur;

    newlist_cur = (ListCell *)palloc(sizeof(*newlist_cur));
    newlist_cur->data = oldlist_cur->data;
    newlist_prev->next = newlist_cur;

    newlist_prev = newlist_cur;
    oldlist_cur = oldlist_cur->next;
  }

  newlist_prev->next = NULL;
  newlist->tail = newlist_prev;

  check_list_invariants(newlist);
  return newlist;
}

   
                                                                              
   
List *
list_copy_tail(const List *oldlist, int nskip)
{
  List *newlist;
  ListCell *newlist_prev;
  ListCell *oldlist_cur;

  if (nskip < 0)
  {
    nskip = 0;                                  
  }

  if (oldlist == NIL || nskip >= oldlist->length)
  {
    return NIL;
  }

  newlist = new_list(oldlist->type);
  newlist->length = oldlist->length - nskip;

     
                                      
     
  oldlist_cur = oldlist->head;
  while (nskip-- > 0)
  {
    oldlist_cur = oldlist_cur->next;
  }

     
                                                                            
                                    
     
  newlist->head->data = oldlist_cur->data;

  newlist_prev = newlist->head;
  oldlist_cur = oldlist_cur->next;
  while (oldlist_cur)
  {
    ListCell *newlist_cur;

    newlist_cur = (ListCell *)palloc(sizeof(*newlist_cur));
    newlist_cur->data = oldlist_cur->data;
    newlist_prev->next = newlist_cur;

    newlist_prev = newlist_cur;
    oldlist_cur = oldlist_cur->next;
  }

  newlist_prev->next = NULL;
  newlist->tail = newlist_prev;

  check_list_invariants(newlist);
  return newlist;
}

   
                                   
   
                                                                        
                                        
   
                                                                   
   
List *
list_qsort(const List *list, list_qsort_comparator cmp)
{
  int len = list_length(list);
  ListCell **list_arr;
  List *newlist;
  ListCell *newlist_prev;
  ListCell *cell;
  int i;

                          
  if (len == 0)
  {
    return NIL;
  }

                                                             
  list_arr = (ListCell **)palloc(sizeof(ListCell *) * len);
  i = 0;
  foreach (cell, list)
  {
    list_arr[i++] = cell;
  }

  qsort(list_arr, len, sizeof(ListCell *), cmp);

                                                             
  newlist = new_list(list->type);
  newlist->length = len;

     
                                                                            
                          
     
  newlist->head->data = list_arr[0]->data;

  newlist_prev = newlist->head;
  for (i = 1; i < len; i++)
  {
    ListCell *newlist_cur;

    newlist_cur = (ListCell *)palloc(sizeof(*newlist_cur));
    newlist_cur->data = list_arr[i]->data;
    newlist_prev->next = newlist_cur;

    newlist_prev = newlist_cur;
  }

  newlist_prev->next = NULL;
  newlist->tail = newlist_prev;

                                              
  pfree(list_arr);

  check_list_invariants(newlist);
  return newlist;
}

   
                                     
   
                                                                      
                                                                       
                                                           
                                                 
   

   
                                                                   
                                                                     
                                                                      
                                                                    
         
   
int
length(const List *list);

int
length(const List *list)
{
  return list_length(list);
}
