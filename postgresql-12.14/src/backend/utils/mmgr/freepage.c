                                                                            
   
              
                                      
   
                                                                      
                                                                         
                                                                  
                                                                          
                                                                        
                                                                      
                                                           
   
                                                                            
                                                                        
                                                                          
                                                                  
                                                                       
   
                                                                          
                                                                      
                                                                           
                                                                          
                                                                        
                                                                     
                                                                        
            
   
                                                                         
                                                                          
                                                                   
                                                                       
                                                                     
                                                                       
                                                                         
                                                                       
                                                                
   
                                                                        
                                                                     
                                                                      
                                                                      
                                                          
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   

#include "postgres.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"

#include "utils/freepage.h"
#include "utils/relptr.h"

                                                  
#define FREE_PAGE_SPAN_LEADER_MAGIC 0xea4020f0
#define FREE_PAGE_LEAF_MAGIC 0x98eae728
#define FREE_PAGE_INTERNAL_MAGIC 0x19aa32c9

                                                                              
struct FreePageSpanLeader
{
  int magic;                                           
  Size npages;                              
  RelptrFreePageSpanLeader prev;
  RelptrFreePageSpanLeader next;
};

                                                      
typedef struct FreePageBtreeHeader
{
  int magic;                                             
                                                            
  Size nused;                                           
  RelptrFreePageBtree parent;             
} FreePageBtreeHeader;

                                                  
typedef struct FreePageBtreeInternalKey
{
  Size first_page;                                                 
  RelptrFreePageBtree child;               
} FreePageBtreeInternalKey;

                                
typedef struct FreePageBtreeLeafKey
{
  Size first_page;                         
  Size npages;                                  
} FreePageBtreeLeafKey;

                                                
#define FPM_ITEMS_PER_INTERNAL_PAGE ((FPM_PAGE_SIZE - sizeof(FreePageBtreeHeader)) / sizeof(FreePageBtreeInternalKey))
#define FPM_ITEMS_PER_LEAF_PAGE ((FPM_PAGE_SIZE - sizeof(FreePageBtreeHeader)) / sizeof(FreePageBtreeLeafKey))

                                 
struct FreePageBtree
{
  FreePageBtreeHeader hdr;
  union
  {
    FreePageBtreeInternalKey internal_key[FPM_ITEMS_PER_INTERNAL_PAGE];
    FreePageBtreeLeafKey leaf_key[FPM_ITEMS_PER_LEAF_PAGE];
  } u;
};

                               
typedef struct FreePageBtreeSearchResult
{
  FreePageBtree *page;
  Size index;
  bool found;
  unsigned split_pages;
} FreePageBtreeSearchResult;

                      
static void
FreePageBtreeAdjustAncestorKeys(FreePageManager *fpm, FreePageBtree *btp);
static Size
FreePageBtreeCleanup(FreePageManager *fpm);
static FreePageBtree *
FreePageBtreeFindLeftSibling(char *base, FreePageBtree *btp);
static FreePageBtree *
FreePageBtreeFindRightSibling(char *base, FreePageBtree *btp);
static Size
FreePageBtreeFirstKey(FreePageBtree *btp);
static FreePageBtree *
FreePageBtreeGetRecycled(FreePageManager *fpm);
static void
FreePageBtreeInsertInternal(char *base, FreePageBtree *btp, Size index, Size first_page, FreePageBtree *child);
static void
FreePageBtreeInsertLeaf(FreePageBtree *btp, Size index, Size first_page, Size npages);
static void
FreePageBtreeRecycle(FreePageManager *fpm, Size pageno);
static void
FreePageBtreeRemove(FreePageManager *fpm, FreePageBtree *btp, Size index);
static void
FreePageBtreeRemovePage(FreePageManager *fpm, FreePageBtree *btp);
static void
FreePageBtreeSearch(FreePageManager *fpm, Size first_page, FreePageBtreeSearchResult *result);
static Size
FreePageBtreeSearchInternal(FreePageBtree *btp, Size first_page);
static Size
FreePageBtreeSearchLeaf(FreePageBtree *btp, Size first_page);
static FreePageBtree *
FreePageBtreeSplitPage(FreePageManager *fpm, FreePageBtree *btp);
static void
FreePageBtreeUpdateParentPointers(char *base, FreePageBtree *btp);
static void
FreePageManagerDumpBtree(FreePageManager *fpm, FreePageBtree *btp, FreePageBtree *parent, int level, StringInfo buf);
static void
FreePageManagerDumpSpans(FreePageManager *fpm, FreePageSpanLeader *span, Size expected_pages, StringInfo buf);
static bool
FreePageManagerGetInternal(FreePageManager *fpm, Size npages, Size *first_page);
static Size
FreePageManagerPutInternal(FreePageManager *fpm, Size first_page, Size npages, bool soft);
static void
FreePagePopSpanLeader(FreePageManager *fpm, Size pageno);
static void
FreePagePushSpanLeader(FreePageManager *fpm, Size first_page, Size npages);
static Size
FreePageManagerLargestContiguous(FreePageManager *fpm);
static void
FreePageManagerUpdateLargest(FreePageManager *fpm);

#if FPM_EXTRA_ASSERTS
static Size
sum_free_pages(FreePageManager *fpm);
#endif

   
                                              
   
                                                                           
                                               
   
                                                                            
                                                                          
                                                                             
                                                                               
   
void
FreePageManagerInitialize(FreePageManager *fpm, char *base)
{
  Size f;

  relptr_store(base, fpm->self, fpm);
  relptr_store(base, fpm->btree_root, (FreePageBtree *)NULL);
  relptr_store(base, fpm->btree_recycle, (FreePageSpanLeader *)NULL);
  fpm->btree_depth = 0;
  fpm->btree_recycle_count = 0;
  fpm->singleton_first_page = 0;
  fpm->singleton_npages = 0;
  fpm->contiguous_pages = 0;
  fpm->contiguous_pages_dirty = true;
#ifdef FPM_EXTRA_ASSERTS
  fpm->free_pages = 0;
#endif

  for (f = 0; f < FPM_NUM_FREELISTS; f++)
  {
    relptr_store(base, fpm->freelist[f], (FreePageSpanLeader *)NULL);
  }
}

   
                                                                           
                                                                           
                                                                       
   
bool
FreePageManagerGet(FreePageManager *fpm, Size npages, Size *first_page)
{
  bool result;
  Size contiguous_pages;

  result = FreePageManagerGetInternal(fpm, npages, first_page);

     
                                                                           
                                                                           
                                                                         
                                                                             
                                                                       
                                                                      
                                                                          
                                                                          
                                                 
     
  contiguous_pages = FreePageBtreeCleanup(fpm);
  if (fpm->contiguous_pages < contiguous_pages)
  {
    fpm->contiguous_pages = contiguous_pages;
  }

     
                                                                     
                                       
     
  FreePageManagerUpdateLargest(fpm);

#ifdef FPM_EXTRA_ASSERTS
  if (result)
  {
    Assert(fpm->free_pages >= npages);
    fpm->free_pages -= npages;
  }
  Assert(fpm->free_pages == sum_free_pages(fpm));
  Assert(fpm->contiguous_pages == FreePageManagerLargestContiguous(fpm));
#endif
  return result;
}

#ifdef FPM_EXTRA_ASSERTS
static void
sum_free_pages_recurse(FreePageManager *fpm, FreePageBtree *btp, Size *sum)
{
  char *base = fpm_segment_base(fpm);

  Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC || btp->hdr.magic == FREE_PAGE_LEAF_MAGIC);
  ++*sum;
  if (btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC)
  {
    Size index;

    for (index = 0; index < btp->hdr.nused; ++index)
    {
      FreePageBtree *child;

      child = relptr_access(base, btp->u.internal_key[index].child);
      sum_free_pages_recurse(fpm, child, sum);
    }
  }
}
static Size
sum_free_pages(FreePageManager *fpm)
{
  FreePageSpanLeader *recycle;
  char *base = fpm_segment_base(fpm);
  Size sum = 0;
  int list;

                                                  
  for (list = 0; list < FPM_NUM_FREELISTS; ++list)
  {

    if (!relptr_is_null(fpm->freelist[list]))
    {
      FreePageSpanLeader *candidate = relptr_access(base, fpm->freelist[list]);

      do
      {
        sum += candidate->npages;
        candidate = relptr_access(base, candidate->next);
      } while (candidate != NULL);
    }
  }

                                   
  if (fpm->btree_depth > 0)
  {
    FreePageBtree *root = relptr_access(base, fpm->btree_root);

    sum_free_pages_recurse(fpm, root, &sum);
  }

                               
  for (recycle = relptr_access(base, fpm->btree_recycle); recycle != NULL; recycle = relptr_access(base, recycle->next))
  {
    Assert(recycle->npages == 1);
    ++sum;
  }

  return sum;
}
#endif

   
                                                                    
                     
   
static Size
FreePageManagerLargestContiguous(FreePageManager *fpm)
{
  char *base;
  Size largest;

  base = fpm_segment_base(fpm);
  largest = 0;
  if (!relptr_is_null(fpm->freelist[FPM_NUM_FREELISTS - 1]))
  {
    FreePageSpanLeader *candidate;

    candidate = relptr_access(base, fpm->freelist[FPM_NUM_FREELISTS - 1]);
    do
    {
      if (candidate->npages > largest)
      {
        largest = candidate->npages;
      }
      candidate = relptr_access(base, candidate->next);
    } while (candidate != NULL);
  }
  else
  {
    Size f = FPM_NUM_FREELISTS - 1;

    do
    {
      --f;
      if (!relptr_is_null(fpm->freelist[f]))
      {
        largest = f + 1;
        break;
      }
    } while (f > 0);
  }

  return largest;
}

   
                                                                      
                                                  
   
static void
FreePageManagerUpdateLargest(FreePageManager *fpm)
{
  if (!fpm->contiguous_pages_dirty)
  {
    return;
  }

  fpm->contiguous_pages = FreePageManagerLargestContiguous(fpm);
  fpm->contiguous_pages_dirty = false;
}

   
                                                     
   
void
FreePageManagerPut(FreePageManager *fpm, Size first_page, Size npages)
{
  Size contiguous_pages;

  Assert(npages > 0);

                             
  contiguous_pages = FreePageManagerPutInternal(fpm, first_page, npages, false);

     
                                                                            
                                                                     
     
  if (contiguous_pages > npages)
  {
    Size cleanup_contiguous_pages;

    cleanup_contiguous_pages = FreePageBtreeCleanup(fpm);
    if (cleanup_contiguous_pages > contiguous_pages)
    {
      contiguous_pages = cleanup_contiguous_pages;
    }
  }

                                               
  if (fpm->contiguous_pages < contiguous_pages)
  {
    fpm->contiguous_pages = contiguous_pages;
  }

     
                                                                 
                                                                        
                                              
     
  FreePageManagerUpdateLargest(fpm);

#ifdef FPM_EXTRA_ASSERTS
  fpm->free_pages += npages;
  Assert(fpm->free_pages == sum_free_pages(fpm));
  Assert(fpm->contiguous_pages == FreePageManagerLargestContiguous(fpm));
#endif
}

   
                                                                 
   
char *
FreePageManagerDump(FreePageManager *fpm)
{
  char *base = fpm_segment_base(fpm);
  StringInfoData buf;
  FreePageSpanLeader *recycle;
  bool dumped_any_freelist = false;
  Size f;

                                 
  initStringInfo(&buf);

                           
  appendStringInfo(&buf, "metadata: self %zu max contiguous pages = %zu\n", fpm->self.relptr_off, fpm->contiguous_pages);

                   
  if (fpm->btree_depth > 0)
  {
    FreePageBtree *root;

    appendStringInfo(&buf, "btree depth %u:\n", fpm->btree_depth);
    root = relptr_access(base, fpm->btree_root);
    FreePageManagerDumpBtree(fpm, root, NULL, 0, &buf);
  }
  else if (fpm->singleton_npages > 0)
  {
    appendStringInfo(&buf, "singleton: %zu(%zu)\n", fpm->singleton_first_page, fpm->singleton_npages);
  }

                                
  recycle = relptr_access(base, fpm->btree_recycle);
  if (recycle != NULL)
  {
    appendStringInfoString(&buf, "btree recycle:");
    FreePageManagerDumpSpans(fpm, recycle, 1, &buf);
  }

                        
  for (f = 0; f < FPM_NUM_FREELISTS; ++f)
  {
    FreePageSpanLeader *span;

    if (relptr_is_null(fpm->freelist[f]))
    {
      continue;
    }
    if (!dumped_any_freelist)
    {
      appendStringInfoString(&buf, "freelists:\n");
      dumped_any_freelist = true;
    }
    appendStringInfo(&buf, "  %zu:", f + 1);
    span = relptr_access(base, fpm->freelist[f]);
    FreePageManagerDumpSpans(fpm, span, f + 1, &buf);
  }

                                    
  return buf.data;
}

   
                                                                             
                                                                               
                                                                                
                                                                              
                                                                          
                                                                           
                            
   
                                                                           
                                                                         
                                                                           
                                                                               
                                                                       
                                             
   
static void
FreePageBtreeAdjustAncestorKeys(FreePageManager *fpm, FreePageBtree *btp)
{
  char *base = fpm_segment_base(fpm);
  Size first_page;
  FreePageBtree *parent;
  FreePageBtree *child;

                                                        
  Assert(btp->hdr.nused > 0);
  if (btp->hdr.magic == FREE_PAGE_LEAF_MAGIC)
  {
    Assert(btp->hdr.nused <= FPM_ITEMS_PER_LEAF_PAGE);
    first_page = btp->u.leaf_key[0].first_page;
  }
  else
  {
    Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
    Assert(btp->hdr.nused <= FPM_ITEMS_PER_INTERNAL_PAGE);
    first_page = btp->u.internal_key[0].first_page;
  }
  child = btp;

                                                                        
  for (;;)
  {
    Size s;

    parent = relptr_access(base, child->hdr.parent);
    if (parent == NULL)
    {
      break;
    }
    s = FreePageBtreeSearchInternal(parent, first_page);

                                                                  
    if (s >= parent->hdr.nused)
    {
      Assert(s == parent->hdr.nused);
      --s;
    }
    else
    {
      FreePageBtree *check;

      check = relptr_access(base, parent->u.internal_key[s].child);
      if (check != child)
      {
        Assert(s > 0);
        --s;
      }
    }

#ifdef USE_ASSERT_CHECKING
                                 
    {
      FreePageBtree *check;

      check = relptr_access(base, parent->u.internal_key[s].child);
      Assert(s < parent->hdr.nused);
      Assert(child == check);
    }
#endif

                                
    parent->u.internal_key[s].first_page = first_page;

       
                                                                         
             
       
    if (s > 0)
    {
      break;
    }
    child = parent;
  }
}

   
                                                                           
                                                                           
   
static Size
FreePageBtreeCleanup(FreePageManager *fpm)
{
  char *base = fpm_segment_base(fpm);
  Size max_contiguous_pages = 0;

                                                 
  while (!relptr_is_null(fpm->btree_root))
  {
    FreePageBtree *root = relptr_access(base, fpm->btree_root);

                                                                 
    if (root->hdr.nused == 1)
    {
                                        
      Assert(fpm->btree_depth > 0);
      --fpm->btree_depth;
      if (root->hdr.magic == FREE_PAGE_LEAF_MAGIC)
      {
                                                                       
        relptr_store(base, fpm->btree_root, (FreePageBtree *)NULL);
        fpm->singleton_first_page = root->u.leaf_key[0].first_page;
        fpm->singleton_npages = root->u.leaf_key[0].npages;
      }
      else
      {
        FreePageBtree *newroot;

                                                                    
        Assert(root->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
        relptr_copy(fpm->btree_root, root->u.internal_key[0].child);
        newroot = relptr_access(base, fpm->btree_root);
        relptr_store(base, newroot->hdr.parent, (FreePageBtree *)NULL);
      }
      FreePageBtreeRecycle(fpm, fpm_pointer_to_page(base, root));
    }
    else if (root->hdr.nused == 2 && root->hdr.magic == FREE_PAGE_LEAF_MAGIC)
    {
      Size end_of_first;
      Size start_of_second;

      end_of_first = root->u.leaf_key[0].first_page + root->u.leaf_key[0].npages;
      start_of_second = root->u.leaf_key[1].first_page;

      if (end_of_first + 1 == start_of_second)
      {
        Size root_page = fpm_pointer_to_page(base, root);

        if (end_of_first == root_page)
        {
          FreePagePopSpanLeader(fpm, root->u.leaf_key[0].first_page);
          FreePagePopSpanLeader(fpm, root->u.leaf_key[1].first_page);
          fpm->singleton_first_page = root->u.leaf_key[0].first_page;
          fpm->singleton_npages = root->u.leaf_key[0].npages + root->u.leaf_key[1].npages + 1;
          fpm->btree_depth = 0;
          relptr_store(base, fpm->btree_root, (FreePageBtree *)NULL);
          FreePagePushSpanLeader(fpm, fpm->singleton_first_page, fpm->singleton_npages);
          Assert(max_contiguous_pages == 0);
          max_contiguous_pages = fpm->singleton_npages;
        }
      }

                                                        
      break;
    }
    else
    {
                                      
      break;
    }
  }

     
                                                                          
                                                                            
                                                                      
                        
     
                                                                          
                                                                           
                                     
     
  while (fpm->btree_recycle_count > 0)
  {
    FreePageBtree *btp;
    Size first_page;
    Size contiguous_pages;

    btp = FreePageBtreeGetRecycled(fpm);
    first_page = fpm_pointer_to_page(base, btp);
    contiguous_pages = FreePageManagerPutInternal(fpm, first_page, 1, true);
    if (contiguous_pages == 0)
    {
      FreePageBtreeRecycle(fpm, first_page);
      break;
    }
    else
    {
      if (contiguous_pages > max_contiguous_pages)
      {
        max_contiguous_pages = contiguous_pages;
      }
    }
  }

  return max_contiguous_pages;
}

   
                                                                         
                         
   
static void
FreePageBtreeConsolidate(FreePageManager *fpm, FreePageBtree *btp)
{
  char *base = fpm_segment_base(fpm);
  FreePageBtree *np;
  Size max;

     
                                                                          
                                                                         
                                                                             
                                                                            
                                                                             
                                                                             
                                  
     
  if (btp->hdr.magic == FREE_PAGE_LEAF_MAGIC)
  {
    max = FPM_ITEMS_PER_LEAF_PAGE;
  }
  else
  {
    Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
    max = FPM_ITEMS_PER_INTERNAL_PAGE;
  }
  if (btp->hdr.nused >= max / 3)
  {
    return;
  }

     
                                                                         
     
  np = FreePageBtreeFindRightSibling(base, btp);
  if (np != NULL && btp->hdr.nused + np->hdr.nused <= max)
  {
    if (btp->hdr.magic == FREE_PAGE_LEAF_MAGIC)
    {
      memcpy(&btp->u.leaf_key[btp->hdr.nused], &np->u.leaf_key[0], sizeof(FreePageBtreeLeafKey) * np->hdr.nused);
      btp->hdr.nused += np->hdr.nused;
    }
    else
    {
      memcpy(&btp->u.internal_key[btp->hdr.nused], &np->u.internal_key[0], sizeof(FreePageBtreeInternalKey) * np->hdr.nused);
      btp->hdr.nused += np->hdr.nused;
      FreePageBtreeUpdateParentPointers(base, btp);
    }
    FreePageBtreeRemovePage(fpm, np);
    return;
  }

     
                                                                          
                                                                       
                                                     
     
  np = FreePageBtreeFindLeftSibling(base, btp);
  if (np != NULL && btp->hdr.nused + np->hdr.nused <= max)
  {
    if (btp->hdr.magic == FREE_PAGE_LEAF_MAGIC)
    {
      memcpy(&np->u.leaf_key[np->hdr.nused], &btp->u.leaf_key[0], sizeof(FreePageBtreeLeafKey) * btp->hdr.nused);
      np->hdr.nused += btp->hdr.nused;
    }
    else
    {
      memcpy(&np->u.internal_key[np->hdr.nused], &btp->u.internal_key[0], sizeof(FreePageBtreeInternalKey) * btp->hdr.nused);
      np->hdr.nused += btp->hdr.nused;
      FreePageBtreeUpdateParentPointers(base, np);
    }
    FreePageBtreeRemovePage(fpm, btp);
    return;
  }
}

   
                                                                            
                                                         
   
static FreePageBtree *
FreePageBtreeFindLeftSibling(char *base, FreePageBtree *btp)
{
  FreePageBtree *p = btp;
  int levels = 0;

                                       
  for (;;)
  {
    Size first_page;
    Size index;

    first_page = FreePageBtreeFirstKey(p);
    p = relptr_access(base, p->hdr.parent);

    if (p == NULL)
    {
      return NULL;                                        
    }

    index = FreePageBtreeSearchInternal(p, first_page);
    if (index > 0)
    {
      Assert(p->u.internal_key[index].first_page == first_page);
      p = relptr_access(base, p->u.internal_key[index - 1].child);
      break;
    }
    Assert(index == 0);
    ++levels;
  }

                     
  while (levels > 0)
  {
    Assert(p->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
    p = relptr_access(base, p->u.internal_key[p->hdr.nused - 1].child);
    --levels;
  }
  Assert(p->hdr.magic == btp->hdr.magic);

  return p;
}

   
                                                                             
                                                        
   
static FreePageBtree *
FreePageBtreeFindRightSibling(char *base, FreePageBtree *btp)
{
  FreePageBtree *p = btp;
  int levels = 0;

                                        
  for (;;)
  {
    Size first_page;
    Size index;

    first_page = FreePageBtreeFirstKey(p);
    p = relptr_access(base, p->hdr.parent);

    if (p == NULL)
    {
      return NULL;                                        
    }

    index = FreePageBtreeSearchInternal(p, first_page);
    if (index < p->hdr.nused - 1)
    {
      Assert(p->u.internal_key[index].first_page == first_page);
      p = relptr_access(base, p->u.internal_key[index + 1].child);
      break;
    }
    Assert(index == p->hdr.nused - 1);
    ++levels;
  }

                     
  while (levels > 0)
  {
    Assert(p->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
    p = relptr_access(base, p->u.internal_key[0].child);
    --levels;
  }
  Assert(p->hdr.magic == btp->hdr.magic);

  return p;
}

   
                                      
   
static Size
FreePageBtreeFirstKey(FreePageBtree *btp)
{
  Assert(btp->hdr.nused > 0);

  if (btp->hdr.magic == FREE_PAGE_LEAF_MAGIC)
  {
    return btp->u.leaf_key[0].first_page;
  }
  else
  {
    Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
    return btp->u.internal_key[0].first_page;
  }
}

   
                                                                   
   
static FreePageBtree *
FreePageBtreeGetRecycled(FreePageManager *fpm)
{
  char *base = fpm_segment_base(fpm);
  FreePageSpanLeader *victim = relptr_access(base, fpm->btree_recycle);
  FreePageSpanLeader *newhead;

  Assert(victim != NULL);
  newhead = relptr_access(base, victim->next);
  if (newhead != NULL)
  {
    relptr_copy(newhead->prev, victim->prev);
  }
  relptr_store(base, fpm->btree_recycle, newhead);
  Assert(fpm_pointer_is_page_aligned(base, victim));
  fpm->btree_recycle_count--;
  return (FreePageBtree *)victim;
}

   
                                         
   
static void
FreePageBtreeInsertInternal(char *base, FreePageBtree *btp, Size index, Size first_page, FreePageBtree *child)
{
  Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
  Assert(btp->hdr.nused <= FPM_ITEMS_PER_INTERNAL_PAGE);
  Assert(index <= btp->hdr.nused);
  memmove(&btp->u.internal_key[index + 1], &btp->u.internal_key[index], sizeof(FreePageBtreeInternalKey) * (btp->hdr.nused - index));
  btp->u.internal_key[index].first_page = first_page;
  relptr_store(base, btp->u.internal_key[index].child, child);
  ++btp->hdr.nused;
}

   
                                    
   
static void
FreePageBtreeInsertLeaf(FreePageBtree *btp, Size index, Size first_page, Size npages)
{
  Assert(btp->hdr.magic == FREE_PAGE_LEAF_MAGIC);
  Assert(btp->hdr.nused <= FPM_ITEMS_PER_LEAF_PAGE);
  Assert(index <= btp->hdr.nused);
  memmove(&btp->u.leaf_key[index + 1], &btp->u.leaf_key[index], sizeof(FreePageBtreeLeafKey) * (btp->hdr.nused - index));
  btp->u.leaf_key[index].first_page = first_page;
  btp->u.leaf_key[index].npages = npages;
  ++btp->hdr.nused;
}

   
                                         
   
static void
FreePageBtreeRecycle(FreePageManager *fpm, Size pageno)
{
  char *base = fpm_segment_base(fpm);
  FreePageSpanLeader *head = relptr_access(base, fpm->btree_recycle);
  FreePageSpanLeader *span;

  span = (FreePageSpanLeader *)fpm_page_to_pointer(base, pageno);
  span->magic = FREE_PAGE_SPAN_LEADER_MAGIC;
  span->npages = 1;
  relptr_store(base, span->next, head);
  relptr_store(base, span->prev, (FreePageSpanLeader *)NULL);
  if (head != NULL)
  {
    relptr_store(base, head->prev, span);
  }
  relptr_store(base, fpm->btree_recycle, span);
  fpm->btree_recycle_count++;
}

   
                                                                          
   
static void
FreePageBtreeRemove(FreePageManager *fpm, FreePageBtree *btp, Size index)
{
  Assert(btp->hdr.magic == FREE_PAGE_LEAF_MAGIC);
  Assert(index < btp->hdr.nused);

                                                                    
  if (btp->hdr.nused == 1)
  {
    FreePageBtreeRemovePage(fpm, btp);
    return;
  }

                                                
  --btp->hdr.nused;
  if (index < btp->hdr.nused)
  {
    memmove(&btp->u.leaf_key[index], &btp->u.leaf_key[index + 1], sizeof(FreePageBtreeLeafKey) * (btp->hdr.nused - index));
  }

                                                               
  if (index == 0)
  {
    FreePageBtreeAdjustAncestorKeys(fpm, btp);
  }

                                                                 
  FreePageBtreeConsolidate(fpm, btp);
}

   
                                                                             
                                                                             
                  
   
static void
FreePageBtreeRemovePage(FreePageManager *fpm, FreePageBtree *btp)
{
  char *base = fpm_segment_base(fpm);
  FreePageBtree *parent;
  Size index;
  Size first_page;

  for (;;)
  {
                           
    parent = relptr_access(base, btp->hdr.parent);
    if (parent == NULL)
    {
                                          
      relptr_store(base, fpm->btree_root, (FreePageBtree *)NULL);
      fpm->btree_depth = 0;
      Assert(fpm->singleton_first_page == 0);
      Assert(fpm->singleton_npages == 0);
      return;
    }

       
                                                                           
       
    if (parent->hdr.nused > 1)
    {
      break;
    }
    FreePageBtreeRecycle(fpm, fpm_pointer_to_page(base, btp));
    btp = parent;
  }

                                     
  first_page = FreePageBtreeFirstKey(btp);
  if (parent->hdr.magic == FREE_PAGE_LEAF_MAGIC)
  {
    index = FreePageBtreeSearchLeaf(parent, first_page);
    Assert(index < parent->hdr.nused);
    if (index < parent->hdr.nused - 1)
    {
      memmove(&parent->u.leaf_key[index], &parent->u.leaf_key[index + 1], sizeof(FreePageBtreeLeafKey) * (parent->hdr.nused - index - 1));
    }
  }
  else
  {
    index = FreePageBtreeSearchInternal(parent, first_page);
    Assert(index < parent->hdr.nused);
    if (index < parent->hdr.nused - 1)
    {
      memmove(&parent->u.internal_key[index], &parent->u.internal_key[index + 1], sizeof(FreePageBtreeInternalKey) * (parent->hdr.nused - index - 1));
    }
  }
  parent->hdr.nused--;
  Assert(parent->hdr.nused > 0);

                         
  FreePageBtreeRecycle(fpm, fpm_pointer_to_page(base, btp));

                                       
  if (index == 0)
  {
    FreePageBtreeAdjustAncestorKeys(fpm, parent);
  }

                                                                  
  FreePageBtreeConsolidate(fpm, parent);
}

   
                                                                         
                                                                           
                                                                           
                                                                              
                                                                               
                                                                            
                                                                              
                        
   
static void
FreePageBtreeSearch(FreePageManager *fpm, Size first_page, FreePageBtreeSearchResult *result)
{
  char *base = fpm_segment_base(fpm);
  FreePageBtree *btp = relptr_access(base, fpm->btree_root);
  Size index;

  result->split_pages = 1;

                                                       
  if (btp == NULL)
  {
    result->page = NULL;
    result->found = false;
    return;
  }

                                    
  while (btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC)
  {
    FreePageBtree *child;
    bool found_exact;

    index = FreePageBtreeSearchInternal(btp, first_page);
    found_exact = index < btp->hdr.nused && btp->u.internal_key[index].first_page == first_page;

       
                                                                      
                                                                          
                                                     
       
    if (!found_exact && index > 0)
    {
      --index;
    }

                                                     
    if (btp->hdr.nused >= FPM_ITEMS_PER_INTERNAL_PAGE)
    {
      Assert(btp->hdr.nused == FPM_ITEMS_PER_INTERNAL_PAGE);
      result->split_pages++;
    }
    else
    {
      result->split_pages = 0;
    }

                                            
    Assert(index < btp->hdr.nused);
    child = relptr_access(base, btp->u.internal_key[index].child);
    Assert(relptr_access(base, child->hdr.parent) == btp);
    btp = child;
  }

                                                   
  if (btp->hdr.nused >= FPM_ITEMS_PER_LEAF_PAGE)
  {
    Assert(btp->hdr.nused == FPM_ITEMS_PER_INTERNAL_PAGE);
    result->split_pages++;
  }
  else
  {
    result->split_pages = 0;
  }

                         
  index = FreePageBtreeSearchLeaf(btp, first_page);

                         
  result->page = btp;
  result->index = index;
  result->found = index < btp->hdr.nused && first_page == btp->u.leaf_key[index].first_page;
}

   
                                                                              
                                                                               
                                
   
static Size
FreePageBtreeSearchInternal(FreePageBtree *btp, Size first_page)
{
  Size low = 0;
  Size high = btp->hdr.nused;

  Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
  Assert(high > 0 && high <= FPM_ITEMS_PER_INTERNAL_PAGE);

  while (low < high)
  {
    Size mid = (low + high) / 2;
    Size val = btp->u.internal_key[mid].first_page;

    if (first_page == val)
    {
      return mid;
    }
    else if (first_page < val)
    {
      high = mid;
    }
    else
    {
      low = mid + 1;
    }
  }

  return low;
}

   
                                                                         
                                                                               
                                
   
static Size
FreePageBtreeSearchLeaf(FreePageBtree *btp, Size first_page)
{
  Size low = 0;
  Size high = btp->hdr.nused;

  Assert(btp->hdr.magic == FREE_PAGE_LEAF_MAGIC);
  Assert(high > 0 && high <= FPM_ITEMS_PER_LEAF_PAGE);

  while (low < high)
  {
    Size mid = (low + high) / 2;
    Size val = btp->u.leaf_key[mid].first_page;

    if (first_page == val)
    {
      return mid;
    }
    else if (first_page < val)
    {
      high = mid;
    }
    else
    {
      low = mid + 1;
    }
  }

  return low;
}

   
                                                                           
                                                                          
                                                                               
                                        
   
static FreePageBtree *
FreePageBtreeSplitPage(FreePageManager *fpm, FreePageBtree *btp)
{
  FreePageBtree *newsibling;

  newsibling = FreePageBtreeGetRecycled(fpm);
  newsibling->hdr.magic = btp->hdr.magic;
  newsibling->hdr.nused = btp->hdr.nused / 2;
  relptr_copy(newsibling->hdr.parent, btp->hdr.parent);
  btp->hdr.nused -= newsibling->hdr.nused;

  if (btp->hdr.magic == FREE_PAGE_LEAF_MAGIC)
  {
    memcpy(&newsibling->u.leaf_key, &btp->u.leaf_key[btp->hdr.nused], sizeof(FreePageBtreeLeafKey) * newsibling->hdr.nused);
  }
  else
  {
    Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
    memcpy(&newsibling->u.internal_key, &btp->u.internal_key[btp->hdr.nused], sizeof(FreePageBtreeInternalKey) * newsibling->hdr.nused);
    FreePageBtreeUpdateParentPointers(fpm_segment_base(fpm), newsibling);
  }

  return newsibling;
}

   
                                                                         
                             
   
static void
FreePageBtreeUpdateParentPointers(char *base, FreePageBtree *btp)
{
  Size i;

  Assert(btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC);
  for (i = 0; i < btp->hdr.nused; ++i)
  {
    FreePageBtree *child;

    child = relptr_access(base, btp->u.internal_key[i].child);
    relptr_store(base, child->hdr.parent, btp);
  }
}

   
                                 
   
static void
FreePageManagerDumpBtree(FreePageManager *fpm, FreePageBtree *btp, FreePageBtree *parent, int level, StringInfo buf)
{
  char *base = fpm_segment_base(fpm);
  Size pageno = fpm_pointer_to_page(base, btp);
  Size index;
  FreePageBtree *check_parent;

  check_stack_depth();
  check_parent = relptr_access(base, btp->hdr.parent);
  appendStringInfo(buf, "  %zu@%d %c", pageno, level, btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC ? 'i' : 'l');
  if (parent != check_parent)
  {
    appendStringInfo(buf, " [actual parent %zu, expected %zu]", fpm_pointer_to_page(base, check_parent), fpm_pointer_to_page(base, parent));
  }
  appendStringInfoChar(buf, ':');
  for (index = 0; index < btp->hdr.nused; ++index)
  {
    if (btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC)
    {
      appendStringInfo(buf, " %zu->%zu", btp->u.internal_key[index].first_page, btp->u.internal_key[index].child.relptr_off / FPM_PAGE_SIZE);
    }
    else
    {
      appendStringInfo(buf, " %zu(%zu)", btp->u.leaf_key[index].first_page, btp->u.leaf_key[index].npages);
    }
  }
  appendStringInfoChar(buf, '\n');

  if (btp->hdr.magic == FREE_PAGE_INTERNAL_MAGIC)
  {
    for (index = 0; index < btp->hdr.nused; ++index)
    {
      FreePageBtree *child;

      child = relptr_access(base, btp->u.internal_key[index].child);
      FreePageManagerDumpBtree(fpm, child, btp, level + 1, buf);
    }
  }
}

   
                                     
   
static void
FreePageManagerDumpSpans(FreePageManager *fpm, FreePageSpanLeader *span, Size expected_pages, StringInfo buf)
{
  char *base = fpm_segment_base(fpm);

  while (span != NULL)
  {
    if (span->npages != expected_pages)
    {
      appendStringInfo(buf, " %zu(%zu)", fpm_pointer_to_page(base, span), span->npages);
    }
    else
    {
      appendStringInfo(buf, " %zu", fpm_pointer_to_page(base, span));
    }
    span = relptr_access(base, span->next);
  }

  appendStringInfoChar(buf, '\n');
}

   
                                                                            
                 
   
static bool
FreePageManagerGetInternal(FreePageManager *fpm, Size npages, Size *first_page)
{
  char *base = fpm_segment_base(fpm);
  FreePageSpanLeader *victim = NULL;
  FreePageSpanLeader *prev;
  FreePageSpanLeader *next;
  FreePageBtreeSearchResult result;
  Size victim_page = 0;                       
  Size f;

     
                             
     
                                                                            
                                                                         
                                                                        
                                                                        
                                                                             
                                                                          
               
     
  for (f = Min(npages, FPM_NUM_FREELISTS) - 1; f < FPM_NUM_FREELISTS; ++f)
  {
                               
    if (relptr_is_null(fpm->freelist[f]))
    {
      continue;
    }

       
                                                                        
                                                                       
                                                                          
                                
       
    if (f < FPM_NUM_FREELISTS - 1)
    {
      victim = relptr_access(base, fpm->freelist[f]);
    }
    else
    {
      FreePageSpanLeader *candidate;

      candidate = relptr_access(base, fpm->freelist[f]);
      do
      {
        if (candidate->npages >= npages && (victim == NULL || victim->npages > candidate->npages))
        {
          victim = candidate;
          if (victim->npages == npages)
          {
            break;
          }
        }
        candidate = relptr_access(base, candidate->next);
      } while (candidate != NULL);
    }
    break;
  }

                                                              
  if (victim == NULL)
  {
    return false;
  }

                                   
  Assert(victim->magic == FREE_PAGE_SPAN_LEADER_MAGIC);
  prev = relptr_access(base, victim->prev);
  next = relptr_access(base, victim->next);
  if (prev != NULL)
  {
    relptr_copy(prev->next, victim->next);
  }
  else
  {
    relptr_copy(fpm->freelist[f], victim->next);
  }
  if (next != NULL)
  {
    relptr_copy(next->prev, victim->prev);
  }
  victim_page = fpm_pointer_to_page(base, victim);

                                                                 
  if (f == FPM_NUM_FREELISTS - 1 && victim->npages == fpm->contiguous_pages)
  {
       
                                                                          
                                                                         
                                                                        
             
       
    fpm->contiguous_pages_dirty = true;
  }
  else if (f + 1 == fpm->contiguous_pages && relptr_is_null(fpm->freelist[f]))
  {
       
                                                                        
                                                                        
                                                            
                                                            
       
    fpm->contiguous_pages_dirty = true;
  }

     
                                                                            
                                                                           
                       
     
  if (relptr_is_null(fpm->btree_root))
  {
    Assert(victim_page == fpm->singleton_first_page);
    Assert(victim->npages == fpm->singleton_npages);
    Assert(victim->npages >= npages);
    fpm->singleton_first_page += npages;
    fpm->singleton_npages -= npages;
    if (fpm->singleton_npages > 0)
    {
      FreePagePushSpanLeader(fpm, fpm->singleton_first_page, fpm->singleton_npages);
    }
  }
  else
  {
       
                                                                          
                                                                           
                                                                          
                              
       
    FreePageBtreeSearch(fpm, victim_page, &result);
    Assert(result.found);
    if (victim->npages == npages)
    {
      FreePageBtreeRemove(fpm, result.page, result.index);
    }
    else
    {
      FreePageBtreeLeafKey *key;

                                                    
      Assert(victim->npages > npages);
      key = &result.page->u.leaf_key[result.index];
      Assert(key->npages == victim->npages);
      key->first_page += npages;
      key->npages -= npages;
      if (result.index == 0)
      {
        FreePageBtreeAdjustAncestorKeys(fpm, result.page);
      }

                                                                        
      FreePagePushSpanLeader(fpm, victim_page + npages, victim->npages - npages);
    }
  }

                                 
  *first_page = fpm_pointer_to_page(base, victim);
  return true;
}

   
                                                                            
                                                                        
                                                                             
                                                                         
                                                                         
                                                                           
                                          
   
static Size
FreePageManagerPutInternal(FreePageManager *fpm, Size first_page, Size npages, bool soft)
{
  char *base = fpm_segment_base(fpm);
  FreePageBtreeSearchResult result;
  FreePageBtreeLeafKey *prevkey = NULL;
  FreePageBtreeLeafKey *nextkey = NULL;
  FreePageBtree *np;
  Size nindex;

  Assert(npages > 0);

                                                                       
  if (fpm->btree_depth == 0)
  {
    if (fpm->singleton_npages == 0)
    {
                                                  
      fpm->singleton_first_page = first_page;
      fpm->singleton_npages = npages;
      FreePagePushSpanLeader(fpm, first_page, npages);
      return fpm->singleton_npages;
    }
    else if (fpm->singleton_first_page + fpm->singleton_npages == first_page)
    {
                                                            
      fpm->singleton_npages += npages;
      FreePagePopSpanLeader(fpm, fpm->singleton_first_page);
      FreePagePushSpanLeader(fpm, fpm->singleton_first_page, fpm->singleton_npages);
      return fpm->singleton_npages;
    }
    else if (first_page + npages == fpm->singleton_first_page)
    {
                                                             
      FreePagePopSpanLeader(fpm, fpm->singleton_first_page);
      fpm->singleton_first_page = first_page;
      fpm->singleton_npages += npages;
      FreePagePushSpanLeader(fpm, fpm->singleton_first_page, fpm->singleton_npages);
      return fpm->singleton_npages;
    }
    else
    {
                                                            
      Size root_page;
      FreePageBtree *root;

      if (!relptr_is_null(fpm->btree_recycle))
      {
        root = FreePageBtreeGetRecycled(fpm);
      }
      else if (soft)
      {
        return 0;                                   
      }
      else if (FreePageManagerGetInternal(fpm, 1, &root_page))
      {
        root = (FreePageBtree *)fpm_page_to_pointer(base, root_page);
      }
      else
      {
                                                                        
        elog(FATAL, "free page manager btree is corrupt");
      }

                                                                    
      root->hdr.magic = FREE_PAGE_LEAF_MAGIC;
      root->hdr.nused = 1;
      relptr_store(base, root->hdr.parent, (FreePageBtree *)NULL);
      root->u.leaf_key[0].first_page = fpm->singleton_first_page;
      root->u.leaf_key[0].npages = fpm->singleton_npages;
      relptr_store(base, fpm->btree_root, root);
      fpm->singleton_first_page = 0;
      fpm->singleton_npages = 0;
      fpm->btree_depth = 1;

         
                                                                       
                                                                      
                                                                        
                                       
         
      if (root->u.leaf_key[0].npages == 0)
      {
        root->u.leaf_key[0].first_page = first_page;
        root->u.leaf_key[0].npages = npages;
        FreePagePushSpanLeader(fpm, first_page, npages);
        return npages;
      }

                                               
    }
  }

                         
  FreePageBtreeSearch(fpm, first_page, &result);
  Assert(!result.found);
  if (result.index > 0)
  {
    prevkey = &result.page->u.leaf_key[result.index - 1];
  }
  if (result.index < result.page->hdr.nused)
  {
    np = result.page;
    nindex = result.index;
    nextkey = &result.page->u.leaf_key[result.index];
  }
  else
  {
    np = FreePageBtreeFindRightSibling(base, result.page);
    nindex = 0;
    if (np != NULL)
    {
      nextkey = &np->u.leaf_key[0];
    }
  }

                                                        
  if (prevkey != NULL && prevkey->first_page + prevkey->npages >= first_page)
  {
    bool remove_next = false;
    Size result;

    Assert(prevkey->first_page + prevkey->npages == first_page);
    prevkey->npages = (first_page - prevkey->first_page) + npages;

                                                                           
    if (nextkey != NULL && prevkey->first_page + prevkey->npages >= nextkey->first_page)
    {
      Assert(prevkey->first_page + prevkey->npages == nextkey->first_page);
      prevkey->npages = (nextkey->first_page - prevkey->first_page) + nextkey->npages;
      FreePagePopSpanLeader(fpm, nextkey->first_page);
      remove_next = true;
    }

                                                             
    FreePagePopSpanLeader(fpm, prevkey->first_page);
    FreePagePushSpanLeader(fpm, prevkey->first_page, prevkey->npages);
    result = prevkey->npages;

       
                                                                         
                                                                     
                                                                          
                                        
       
                                                                     
                                                                         
                                                                         
       
    if (remove_next)
    {
      FreePageBtreeRemove(fpm, np, nindex);
    }

    return result;
  }

                                                    
  if (nextkey != NULL && first_page + npages >= nextkey->first_page)
  {
    Size newpages;

                                    
    Assert(first_page + npages == nextkey->first_page);
    newpages = (nextkey->first_page - first_page) + nextkey->npages;

                                        
    FreePagePopSpanLeader(fpm, nextkey->first_page);
    FreePagePushSpanLeader(fpm, first_page, newpages);

                              
    nextkey->first_page = first_page;
    nextkey->npages = newpages;

                                                                         
    if (nindex == 0)
    {
      FreePageBtreeAdjustAncestorKeys(fpm, np);
    }

    return nextkey->npages;
  }

                                                                  
  if (result.split_pages > 0)
  {
       
                                                                       
                                                                         
                                                                      
                                                                          
                                                                       
                                                                          
                                                                        
                                     
       

                                                         
    if (soft)
    {
      return 0;
    }

                                                                      
    if (result.split_pages > fpm->btree_recycle_count)
    {
      Size pages_needed;
      Size recycle_page;
      Size i;

         
                                                                     
                                                                    
                                                                    
                                                                 
                                                                      
                                                                      
                                                                
         
      pages_needed = result.split_pages - fpm->btree_recycle_count;
      for (i = 0; i < pages_needed; ++i)
      {
        if (!FreePageManagerGetInternal(fpm, 1, &recycle_page))
        {
          elog(FATAL, "free page manager btree is corrupt");
        }
        FreePageBtreeRecycle(fpm, recycle_page);
      }

         
                                                                         
                                                                        
                                                                         
                                                                       
                                                                    
                             
         
      FreePageBtreeSearch(fpm, first_page, &result);

         
                                                                       
                                                                     
                                                                        
                                                                        
                                        
         
      Assert(result.split_pages <= fpm->btree_recycle_count);
    }

                                                     
    if (result.split_pages > 0)
    {
      FreePageBtree *split_target = result.page;
      FreePageBtree *child = NULL;
      Size key = first_page;

      for (;;)
      {
        FreePageBtree *newsibling;
        FreePageBtree *parent;

                                                                
        parent = relptr_access(base, split_target->hdr.parent);

                                                      
        newsibling = FreePageBtreeSplitPage(fpm, split_target);

           
                                                                      
                                                                    
                                                                     
                                                                     
                                                                       
                                                               
                            
           
        if (child == NULL)
        {
          Size index;
          FreePageBtree *insert_into;

          insert_into = key < newsibling->u.leaf_key[0].first_page ? split_target : newsibling;
          index = FreePageBtreeSearchLeaf(insert_into, key);
          FreePageBtreeInsertLeaf(insert_into, index, key, npages);
          if (index == 0 && insert_into == split_target)
          {
            FreePageBtreeAdjustAncestorKeys(fpm, split_target);
          }
        }
        else
        {
          Size index;
          FreePageBtree *insert_into;

          insert_into = key < newsibling->u.internal_key[0].first_page ? split_target : newsibling;
          index = FreePageBtreeSearchInternal(insert_into, key);
          FreePageBtreeInsertInternal(base, insert_into, index, key, child);
          relptr_store(base, child->hdr.parent, insert_into);
          if (index == 0 && insert_into == split_target)
          {
            FreePageBtreeAdjustAncestorKeys(fpm, split_target);
          }
        }

                                                                      
        if (parent == NULL)
        {
          FreePageBtree *newroot;

          newroot = FreePageBtreeGetRecycled(fpm);
          newroot->hdr.magic = FREE_PAGE_INTERNAL_MAGIC;
          newroot->hdr.nused = 2;
          relptr_store(base, newroot->hdr.parent, (FreePageBtree *)NULL);
          newroot->u.internal_key[0].first_page = FreePageBtreeFirstKey(split_target);
          relptr_store(base, newroot->u.internal_key[0].child, split_target);
          relptr_store(base, split_target->hdr.parent, newroot);
          newroot->u.internal_key[1].first_page = FreePageBtreeFirstKey(newsibling);
          relptr_store(base, newroot->u.internal_key[1].child, newsibling);
          relptr_store(base, newsibling->hdr.parent, newroot);
          relptr_store(base, fpm->btree_root, newroot);
          fpm->btree_depth++;

          break;
        }

                                                                 
        key = newsibling->u.internal_key[0].first_page;
        if (parent->hdr.nused < FPM_ITEMS_PER_INTERNAL_PAGE)
        {
          Size index;

          index = FreePageBtreeSearchInternal(parent, key);
          FreePageBtreeInsertInternal(base, parent, index, key, newsibling);
          relptr_store(base, newsibling->hdr.parent, parent);
          if (index == 0)
          {
            FreePageBtreeAdjustAncestorKeys(fpm, parent);
          }
          break;
        }

                                                                
        child = newsibling;
        split_target = parent;
      }

         
                                                                        
                               
         
      FreePagePushSpanLeader(fpm, first_page, npages);

      return npages;
    }
  }

                                           
  Assert(result.page->hdr.nused < FPM_ITEMS_PER_LEAF_PAGE);
  FreePageBtreeInsertLeaf(result.page, result.index, first_page, npages);

                                                                  
  if (result.index == 0)
  {
    FreePageBtreeAdjustAncestorKeys(fpm, result.page);
  }

                                
  FreePagePushSpanLeader(fpm, first_page, npages);

  return npages;
}

   
                                                                             
                                                                                
   
static void
FreePagePopSpanLeader(FreePageManager *fpm, Size pageno)
{
  char *base = fpm_segment_base(fpm);
  FreePageSpanLeader *span;
  FreePageSpanLeader *next;
  FreePageSpanLeader *prev;

  span = (FreePageSpanLeader *)fpm_page_to_pointer(base, pageno);

  next = relptr_access(base, span->next);
  prev = relptr_access(base, span->prev);
  if (next != NULL)
  {
    relptr_copy(next->prev, span->prev);
  }
  if (prev != NULL)
  {
    relptr_copy(prev->next, span->next);
  }
  else
  {
    Size f = Min(span->npages, FPM_NUM_FREELISTS) - 1;

    Assert(fpm->freelist[f].relptr_off == pageno * FPM_PAGE_SIZE);
    relptr_copy(fpm->freelist[f], span->next);
  }
}

   
                                                                                
   
static void
FreePagePushSpanLeader(FreePageManager *fpm, Size first_page, Size npages)
{
  char *base = fpm_segment_base(fpm);
  Size f = Min(npages, FPM_NUM_FREELISTS) - 1;
  FreePageSpanLeader *head = relptr_access(base, fpm->freelist[f]);
  FreePageSpanLeader *span;

  span = (FreePageSpanLeader *)fpm_page_to_pointer(base, first_page);
  span->magic = FREE_PAGE_SPAN_LEADER_MAGIC;
  span->npages = npages;
  relptr_store(base, span->next, head);
  relptr_store(base, span->prev, (FreePageSpanLeader *)NULL);
  if (head != NULL)
  {
    relptr_store(base, head->prev, span);
  }
  relptr_store(base, fpm->freelist[f], span);
}
