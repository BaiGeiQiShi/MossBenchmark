                                                                            
   
             
                                                     
   
   
                                                                         
                                                                        
   
                  
                                             
   
          
   
                                                                         
                                                                           
                                                                 
                                                            
                                                                  
   
                                                                            
   
#include "postgres.h"

#include "storage/bufmgr.h"
#include "storage/fsm_internals.h"

                                                                     
#define leftchild(x) (2 * (x) + 1)
#define rightchild(x) (2 * (x) + 2)
#define parentof(x) (((x)-1) / 2)

   
                                                              
   
static int
rightneighbor(int x)
{
     
                                                                          
                     
     
  x++;

     
                                                                            
                                                                           
                                                          
                                       
     
  if (((x + 1) & x) == 0)
  {
    x = parentof(x);
  }

  return x;
}

   
                                                                            
   
                                                       
   
bool
fsm_set_avail(Page page, int slot, uint8 value)
{
  int nodeno = NonLeafNodesPerPage + slot;
  FSMPage fsmpage = (FSMPage)PageGetContents(page);
  uint8 oldvalue;

  Assert(slot < LeafNodesPerPage);

  oldvalue = fsmpage->fp_nodes[nodeno];

                                                                 
  if (oldvalue == value && value <= fsmpage->fp_nodes[0])
  {
    return false;
  }

  fsmpage->fp_nodes[nodeno] = value;

     
                                                                           
              
     
  do
  {
    uint8 newvalue = 0;
    int lchild;
    int rchild;

    nodeno = parentof(nodeno);
    lchild = leftchild(nodeno);
    rchild = lchild + 1;

    newvalue = fsmpage->fp_nodes[lchild];
    if (rchild < NodesPerPage)
    {
      newvalue = Max(newvalue, fsmpage->fp_nodes[rchild]);
    }

    oldvalue = fsmpage->fp_nodes[nodeno];
    if (oldvalue == newvalue)
    {
      break;
    }

    fsmpage->fp_nodes[nodeno] = newvalue;
  } while (nodeno > 0);

     
                                                                            
                                                
     
  if (value > fsmpage->fp_nodes[0])
  {
    fsm_rebuild_page(page);
  }

  return true;
}

   
                                            
   
                                                                            
                      
   
uint8
fsm_get_avail(Page page, int slot)
{
  FSMPage fsmpage = (FSMPage)PageGetContents(page);

  Assert(slot < LeafNodesPerPage);

  return fsmpage->fp_nodes[NonLeafNodesPerPage + slot];
}

   
                                            
   
                                                                            
                      
   
uint8
fsm_get_max_avail(Page page)
{
  FSMPage fsmpage = (FSMPage)PageGetContents(page);

  return fsmpage->fp_nodes[0];
}

   
                                                        
                                             
   
                                                                     
                                                                       
                                                                         
                                                                     
   
                                                                         
                                                                
   
int
fsm_search_avail(Buffer buf, uint8 minvalue, bool advancenext, bool exclusive_lock_held)
{
  Page page = BufferGetPage(buf);
  FSMPage fsmpage = (FSMPage)PageGetContents(page);
  int nodeno;
  int target;
  uint16 slot;

restart:

     
                                                                           
                
     
  if (fsmpage->fp_nodes[0] < minvalue)
  {
    return -1;
  }

     
                                                                            
                                                                            
                                 
     
  target = fsmpage->fp_next_slot;
  if (target < 0 || target >= LeafNodesPerPage)
  {
    target = 0;
  }
  target += NonLeafNodesPerPage;

               
                                                                     
                                                                         
                                                                          
             
     
                                                                         
                                                                        
                                                                          
                                                                           
                                                                          
                                                                        
                                                                         
                                                                       
                                                                         
                                                                          
                                          
     
                                                                           
                                                                            
                                                                          
                                                                    
     
                                      
     
           
               
                 
                     
          
     
                                                                        
                                                                          
                                                                            
                                                                           
                                                                            
                                                                             
                                                                            
                                                                
               
     
  nodeno = target;
  while (nodeno > 0)
  {
    if (fsmpage->fp_nodes[nodeno] >= minvalue)
    {
      break;
    }

       
                                                                           
                 
       
    nodeno = parentof(rightneighbor(nodeno));
  }

     
                                                                            
                                                                        
                                                         
     
  while (nodeno < NonLeafNodesPerPage)
  {
    int childnodeno = leftchild(nodeno);

    if (childnodeno < NodesPerPage && fsmpage->fp_nodes[childnodeno] >= minvalue)
    {
      nodeno = childnodeno;
      continue;
    }
    childnodeno++;                           
    if (childnodeno < NodesPerPage && fsmpage->fp_nodes[childnodeno] >= minvalue)
    {
      nodeno = childnodeno;
    }
    else
    {
         
                                                                        
                                                                        
                                                                        
                                                                      
         
                                         
         
      RelFileNode rnode;
      ForkNumber forknum;
      BlockNumber blknum;

      BufferGetTag(buf, &rnode, &forknum, &blknum);
      elog(DEBUG1, "fixing corrupt FSM block %u, relation %u/%u/%u", blknum, rnode.spcNode, rnode.dbNode, rnode.relNode);

                                               
      if (!exclusive_lock_held)
      {
        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
        LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
        exclusive_lock_held = true;
      }
      fsm_rebuild_page(page);
      MarkBufferDirtyHint(buf, false);
      goto restart;
    }
  }

                                                                   
  slot = nodeno - NonLeafNodesPerPage;

     
                                                                             
                                                                            
                                                                           
                                           
     
                                                               
     
  fsmpage->fp_next_slot = slot + (advancenext ? 1 : 0);

  return slot;
}

   
                                                                      
                                          
   
bool
fsm_truncate_avail(Page page, int nslots)
{
  FSMPage fsmpage = (FSMPage)PageGetContents(page);
  uint8 *ptr;
  bool changed = false;

  Assert(nslots >= 0 && nslots < LeafNodesPerPage);

                                      
  ptr = &fsmpage->fp_nodes[NonLeafNodesPerPage + nslots];
  for (; ptr < &fsmpage->fp_nodes[NodesPerPage]; ptr++)
  {
    if (*ptr != 0)
    {
      changed = true;
    }
    *ptr = 0;
  }

                        
  if (changed)
  {
    fsm_rebuild_page(page);
  }

  return changed;
}

   
                                                                     
                 
   
bool
fsm_rebuild_page(Page page)
{
  FSMPage fsmpage = (FSMPage)PageGetContents(page);
  bool changed = false;
  int nodeno;

     
                                                                         
                                                                          
     
  for (nodeno = NonLeafNodesPerPage - 1; nodeno >= 0; nodeno--)
  {
    int lchild = leftchild(nodeno);
    int rchild = lchild + 1;
    uint8 newvalue = 0;

                                                                      
    if (lchild < NodesPerPage)
    {
      newvalue = fsmpage->fp_nodes[lchild];
    }

    if (rchild < NodesPerPage)
    {
      newvalue = Max(newvalue, fsmpage->fp_nodes[rchild]);
    }

    if (fsmpage->fp_nodes[nodeno] != newvalue)
    {
      fsmpage->fp_nodes[nodeno] = newvalue;
      changed = true;
    }
  }

  return changed;
}
