                                                                            
   
               
                                              
   
                                                                    
                                                                     
                                                                           
                                                                       
                                                                    
                                                                         
                                                                         
                                                    
   
                                                                         
                                                                         
                                                                       
                                                                           
                                                                      
                                                     
   
                                                                         
                                                                      
                                                                     
                                                                           
                                                                         
                                                                          
                                                                        
                         
   
   
                                                                
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "nodes/bitmapset.h"
#include "nodes/tidbitmap.h"
#include "storage/lwlock.h"
#include "utils/dsa.h"
#include "utils/hashutils.h"

   
                                                                          
                                                                           
                                                                        
            
   
#define MAX_TUPLES_PER_PAGE MaxHeapTuplesPerPage

   
                                                                         
                                                                     
                                                                           
                                                                            
                                    
   
                                                                        
                                                                        
                                                                          
                                                                           
                                                                          
                                                                          
                                                                           
   
#define PAGES_PER_CHUNK (BLCKSZ / 32)

                                                                              

#define WORDNUM(x) ((x) / BITS_PER_BITMAPWORD)
#define BITNUM(x) ((x) % BITS_PER_BITMAPWORD)

                                               
#define WORDS_PER_PAGE ((MAX_TUPLES_PER_PAGE - 1) / BITS_PER_BITMAPWORD + 1)
                                               
#define WORDS_PER_CHUNK ((PAGES_PER_CHUNK - 1) / BITS_PER_BITMAPWORD + 1)

   
                                                                      
                                                                     
                                                                         
                                                                      
                                                                     
                                                                    
                                                                   
                                               
   
                                                                      
                                                                         
                                                               
   
typedef struct PagetableEntry
{
  BlockNumber blockno;                                  
  char status;                                
  bool ischunk;                                          
  bool recheck;                                             
  bitmapword words[Max(WORDS_PER_PAGE, WORDS_PER_CHUNK)];
} PagetableEntry;

   
                                     
   
typedef struct PTEntryArray
{
  pg_atomic_uint32 refcount;                               
  PagetableEntry ptentry[FLEXIBLE_ARRAY_MEMBER];
} PTEntryArray;

   
                                                                     
                                                                             
                                                                             
                                                                          
                                                                           
                                                                       
                                                                            
                                                                            
                                  
   
typedef enum
{
  TBM_EMPTY,                                     
  TBM_ONE_PAGE,                                       
  TBM_HASH                                             
} TBMStatus;

   
                                       
   
typedef enum
{
  TBM_NOT_ITERATING,                                                    
  TBM_ITERATING_PRIVATE,                                              
  TBM_ITERATING_SHARED                                                 
} TBMIteratingState;

   
                                                     
   
struct TIDBitmap
{
  NodeTag type;                                                  
  MemoryContext mcxt;                                                 
  TBMStatus status;                                      
  struct pagetable_hash *pagetable;                                     
  int nentries;                                                         
  int maxentries;                                                       
  int npages;                                                                 
  int nchunks;                                                                
  TBMIteratingState iterating;                                     
  uint32 lossify_start;                                                          
  PagetableEntry entry1;                                                  
                                               
  PagetableEntry **spages;                                          
  PagetableEntry **schunks;                                          
  dsa_pointer dsapagetable;                                          
  dsa_pointer dsapagetableold;                                           
  dsa_pointer ptpages;                                            
  dsa_pointer ptchunks;                                            
  dsa_area *dsa;                                                    
};

   
                                                                          
                                                                         
                                                                           
                            
   
struct TBMIterator
{
  TIDBitmap *tbm;                                              
  int spageptr;                                   
  int schunkptr;                                   
  int schunkbit;                                                    
  TBMIterateResult output;                                           
};

   
                                                                       
                        
   
typedef struct TBMSharedIteratorState
{
  int nentries;                                              
  int maxentries;                                            
  int npages;                                                      
  int nchunks;                                                     
  dsa_pointer pagetable;                                             
  dsa_pointer spages;                                   
  dsa_pointer schunks;                                   
  LWLock lock;                                              
  int spageptr;                                 
  int schunkptr;                                 
  int schunkbit;                                                  
} TBMSharedIteratorState;

   
                              
   
typedef struct PTIterationArray
{
  pg_atomic_uint32 refcount;                                      
  int index[FLEXIBLE_ARRAY_MEMBER];                  
} PTIterationArray;

   
                                                                           
                                               
   
struct TBMSharedIterator
{
  TBMSharedIteratorState *state;                   
  PTEntryArray *ptbase;                                       
  PTIterationArray *ptpages;                                       
  PTIterationArray *ptchunks;                                      
  TBMIterateResult output;                                                 
};

                               
static void
tbm_union_page(TIDBitmap *a, const PagetableEntry *bpage);
static bool
tbm_intersect_page(TIDBitmap *a, PagetableEntry *apage, const TIDBitmap *b);
static const PagetableEntry *
tbm_find_pageentry(const TIDBitmap *tbm, BlockNumber pageno);
static PagetableEntry *
tbm_get_pageentry(TIDBitmap *tbm, BlockNumber pageno);
static bool
tbm_page_is_lossy(const TIDBitmap *tbm, BlockNumber pageno);
static void
tbm_mark_page_lossy(TIDBitmap *tbm, BlockNumber pageno);
static void
tbm_lossify(TIDBitmap *tbm);
static int
tbm_comparator(const void *left, const void *right);
static int
tbm_shared_comparator(const void *left, const void *right, void *arg);

                                                                
#define SH_USE_NONDEFAULT_ALLOCATOR
#define SH_PREFIX pagetable
#define SH_ELEMENT_TYPE PagetableEntry
#define SH_KEY_TYPE BlockNumber
#define SH_KEY blockno
#define SH_HASH_KEY(tb, key) murmurhash32(key)
#define SH_EQUAL(tb, a, b) a == b
#define SH_SCOPE static inline
#define SH_DEFINE
#define SH_DECLARE
#include "lib/simplehash.h"

   
                                                 
   
                                                                           
                                                                             
                                                                             
                                                                          
                              
   
TIDBitmap *
tbm_create(long maxbytes, dsa_area *dsa)
{
  TIDBitmap *tbm;

                                                           
  tbm = makeNode(TIDBitmap);

  tbm->mcxt = CurrentMemoryContext;
  tbm->status = TBM_EMPTY;

  tbm->maxentries = (int)tbm_calculate_entries(maxbytes);
  tbm->lossify_start = 0;
  tbm->dsa = dsa;
  tbm->dsapagetable = InvalidDsaPointer;
  tbm->dsapagetableold = InvalidDsaPointer;
  tbm->ptpages = InvalidDsaPointer;
  tbm->ptchunks = InvalidDsaPointer;

  return tbm;
}

   
                                                                        
                                                 
   
static void
tbm_create_pagetable(TIDBitmap *tbm)
{
  Assert(tbm->status != TBM_HASH);
  Assert(tbm->pagetable == NULL);

  tbm->pagetable = pagetable_create(tbm->mcxt, 128, tbm);

                                                      
  if (tbm->status == TBM_ONE_PAGE)
  {
    PagetableEntry *page;
    bool found;
    char oldstatus;

    page = pagetable_insert(tbm->pagetable, tbm->entry1.blockno, &found);
    Assert(!found);
    oldstatus = page->status;
    memcpy(page, &tbm->entry1, sizeof(PagetableEntry));
    page->status = oldstatus;
  }

  tbm->status = TBM_HASH;
}

   
                               
   
void
tbm_free(TIDBitmap *tbm)
{
  if (tbm->pagetable)
  {
    pagetable_destroy(tbm->pagetable);
  }
  if (tbm->spages)
  {
    pfree(tbm->spages);
  }
  if (tbm->schunks)
  {
    pfree(tbm->schunks);
  }
  pfree(tbm);
}

   
                                            
   
                                                                              
                                                                             
                 
   
void
tbm_free_shared_area(dsa_area *dsa, dsa_pointer dp)
{
  TBMSharedIteratorState *istate = dsa_get_address(dsa, dp);
  PTEntryArray *ptbase;
  PTIterationArray *ptpages;
  PTIterationArray *ptchunks;

  if (DsaPointerIsValid(istate->pagetable))
  {
    ptbase = dsa_get_address(dsa, istate->pagetable);
    if (pg_atomic_sub_fetch_u32(&ptbase->refcount, 1) == 0)
    {
      dsa_free(dsa, istate->pagetable);
    }
  }
  if (DsaPointerIsValid(istate->spages))
  {
    ptpages = dsa_get_address(dsa, istate->spages);
    if (pg_atomic_sub_fetch_u32(&ptpages->refcount, 1) == 0)
    {
      dsa_free(dsa, istate->spages);
    }
  }
  if (DsaPointerIsValid(istate->schunks))
  {
    ptchunks = dsa_get_address(dsa, istate->schunks);
    if (pg_atomic_sub_fetch_u32(&ptchunks->refcount, 1) == 0)
    {
      dsa_free(dsa, istate->schunks);
    }
  }

  dsa_free(dsa, dp);
}

   
                                                      
   
                                                                
                                                               
   
void
tbm_add_tuples(TIDBitmap *tbm, const ItemPointer tids, int ntids, bool recheck)
{
  BlockNumber currblk = InvalidBlockNumber;
  PagetableEntry *page = NULL;                                       
  int i;

  Assert(tbm->iterating == TBM_NOT_ITERATING);
  for (i = 0; i < ntids; i++)
  {
    BlockNumber blk = ItemPointerGetBlockNumber(tids + i);
    OffsetNumber off = ItemPointerGetOffsetNumber(tids + i);
    int wordnum, bitnum;

                                                                  
    if (off < 1 || off > MAX_TUPLES_PER_PAGE)
    {
      elog(ERROR, "tuple offset out of range: %u", off);
    }

       
                                                                          
                                                                        
                                                    
       
    if (blk != currblk)
    {
      if (tbm_page_is_lossy(tbm, blk))
      {
        page = NULL;                             
      }
      else
      {
        page = tbm_get_pageentry(tbm, blk);
      }
      currblk = blk;
    }

    if (page == NULL)
    {
      continue;                                   
    }

    if (page->ischunk)
    {
                                                                
      wordnum = bitnum = 0;
    }
    else
    {
                                                          
      wordnum = WORDNUM(off - 1);
      bitnum = BITNUM(off - 1);
    }
    page->words[wordnum] |= ((bitmapword)1 << bitnum);
    page->recheck |= recheck;

    if (tbm->nentries > tbm->maxentries)
    {
      tbm_lossify(tbm);
                                                                        
      currblk = InvalidBlockNumber;
    }
  }
}

   
                                                  
   
                                                                     
                                  
   
void
tbm_add_page(TIDBitmap *tbm, BlockNumber pageno)
{
                                                                         
  tbm_mark_page_lossy(tbm, pageno);
                                                                 
  if (tbm->nentries > tbm->maxentries)
  {
    tbm_lossify(tbm);
  }
}

   
                         
   
                                            
   
void
tbm_union(TIDBitmap *a, const TIDBitmap *b)
{
  Assert(!a->iterating);
                                   
  if (b->nentries == 0)
  {
    return;
  }
                                                        
  if (b->status == TBM_ONE_PAGE)
  {
    tbm_union_page(a, &b->entry1);
  }
  else
  {
    pagetable_iterator i;
    PagetableEntry *bpage;

    Assert(b->status == TBM_HASH);
    pagetable_start_iterate(b->pagetable, &i);
    while ((bpage = pagetable_iterate(b->pagetable, &i)) != NULL)
    {
      tbm_union_page(a, bpage);
    }
  }
}

                                             
static void
tbm_union_page(TIDBitmap *a, const PagetableEntry *bpage)
{
  PagetableEntry *apage;
  int wordnum;

  if (bpage->ischunk)
  {
                                                             
    for (wordnum = 0; wordnum < WORDS_PER_CHUNK; wordnum++)
    {
      bitmapword w = bpage->words[wordnum];

      if (w != 0)
      {
        BlockNumber pg;

        pg = bpage->blockno + (wordnum * BITS_PER_BITMAPWORD);
        while (w != 0)
        {
          if (w & 1)
          {
            tbm_mark_page_lossy(a, pg);
          }
          pg++;
          w >>= 1;
        }
      }
    }
  }
  else if (tbm_page_is_lossy(a, bpage->blockno))
  {
                                                   
    return;
  }
  else
  {
    apage = tbm_get_pageentry(a, bpage->blockno);
    if (apage->ischunk)
    {
                                                                
      apage->words[0] |= ((bitmapword)1 << 0);
    }
    else
    {
                                                        
      for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
      {
        apage->words[wordnum] |= bpage->words[wordnum];
      }
      apage->recheck |= bpage->recheck;
    }
  }

  if (a->nentries > a->maxentries)
  {
    tbm_lossify(a);
  }
}

   
                                    
   
                                            
   
void
tbm_intersect(TIDBitmap *a, const TIDBitmap *b)
{
  Assert(!a->iterating);
                                   
  if (a->nentries == 0)
  {
    return;
  }
                                                             
  if (a->status == TBM_ONE_PAGE)
  {
    if (tbm_intersect_page(a, &a->entry1, b))
    {
                                               
      Assert(!a->entry1.ischunk);
      a->npages--;
      a->nentries--;
      Assert(a->nentries == 0);
      a->status = TBM_EMPTY;
    }
  }
  else
  {
    pagetable_iterator i;
    PagetableEntry *apage;

    Assert(a->status == TBM_HASH);
    pagetable_start_iterate(a->pagetable, &i);
    while ((apage = pagetable_iterate(a->pagetable, &i)) != NULL)
    {
      if (tbm_intersect_page(a, apage, b))
      {
                                                          
        if (apage->ischunk)
        {
          a->nchunks--;
        }
        else
        {
          a->npages--;
        }
        a->nentries--;
        if (!pagetable_delete(a->pagetable, apage->blockno))
        {
          elog(ERROR, "hash table corrupted");
        }
      }
    }
  }
}

   
                                                   
   
                                                                   
   
static bool
tbm_intersect_page(TIDBitmap *a, PagetableEntry *apage, const TIDBitmap *b)
{
  const PagetableEntry *bpage;
  int wordnum;

  if (apage->ischunk)
  {
                                              
    bool candelete = true;

    for (wordnum = 0; wordnum < WORDS_PER_CHUNK; wordnum++)
    {
      bitmapword w = apage->words[wordnum];

      if (w != 0)
      {
        bitmapword neww = w;
        BlockNumber pg;
        int bitnum;

        pg = apage->blockno + (wordnum * BITS_PER_BITMAPWORD);
        bitnum = 0;
        while (w != 0)
        {
          if (w & 1)
          {
            if (!tbm_page_is_lossy(b, pg) && tbm_find_pageentry(b, pg) == NULL)
            {
                                                           
              neww &= ~((bitmapword)1 << bitnum);
            }
          }
          pg++;
          bitnum++;
          w >>= 1;
        }
        apage->words[wordnum] = neww;
        if (neww != 0)
        {
          candelete = false;
        }
      }
    }
    return candelete;
  }
  else if (tbm_page_is_lossy(b, apage->blockno))
  {
       
                                                                          
                                                                          
                                                                         
                                      
       
    apage->recheck = true;
    return false;
  }
  else
  {
    bool candelete = true;

    bpage = tbm_find_pageentry(b, apage->blockno);
    if (bpage != NULL)
    {
                                                        
      Assert(!bpage->ischunk);
      for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
      {
        apage->words[wordnum] &= bpage->words[wordnum];
        if (apage->words[wordnum] != 0)
        {
          candelete = false;
        }
      }
      apage->recheck |= bpage->recheck;
    }
                                                                       
    return candelete;
  }
}

   
                                                   
   
bool
tbm_is_empty(const TIDBitmap *tbm)
{
  return (tbm->nentries == 0);
}

   
                                                              
   
                                                                     
                                                                         
                                                                              
                                                                          
             
   
                                                                            
                                                                         
                                                  
   
TBMIterator *
tbm_begin_iterate(TIDBitmap *tbm)
{
  TBMIterator *iterator;

  Assert(tbm->iterating != TBM_ITERATING_SHARED);

     
                                                                            
                                               
     
  iterator = (TBMIterator *)palloc(sizeof(TBMIterator) + MAX_TUPLES_PER_PAGE * sizeof(OffsetNumber));
  iterator->tbm = tbm;

     
                                    
     
  iterator->spageptr = 0;
  iterator->schunkptr = 0;
  iterator->schunkbit = 0;

     
                                                                           
                                                                           
                                                                          
                        
     
  if (tbm->status == TBM_HASH && tbm->iterating == TBM_NOT_ITERATING)
  {
    pagetable_iterator i;
    PagetableEntry *page;
    int npages;
    int nchunks;

    if (!tbm->spages && tbm->npages > 0)
    {
      tbm->spages = (PagetableEntry **)MemoryContextAlloc(tbm->mcxt, tbm->npages * sizeof(PagetableEntry *));
    }
    if (!tbm->schunks && tbm->nchunks > 0)
    {
      tbm->schunks = (PagetableEntry **)MemoryContextAlloc(tbm->mcxt, tbm->nchunks * sizeof(PagetableEntry *));
    }

    npages = nchunks = 0;
    pagetable_start_iterate(tbm->pagetable, &i);
    while ((page = pagetable_iterate(tbm->pagetable, &i)) != NULL)
    {
      if (page->ischunk)
      {
        tbm->schunks[nchunks++] = page;
      }
      else
      {
        tbm->spages[npages++] = page;
      }
    }
    Assert(npages == tbm->npages);
    Assert(nchunks == tbm->nchunks);
    if (npages > 1)
    {
      qsort(tbm->spages, npages, sizeof(PagetableEntry *), tbm_comparator);
    }
    if (nchunks > 1)
    {
      qsort(tbm->schunks, nchunks, sizeof(PagetableEntry *), tbm_comparator);
    }
  }

  tbm->iterating = TBM_ITERATING_PRIVATE;

  return iterator;
}

   
                                                                                
   
                                                                       
                                                                                
   
                                                                               
                         
   
dsa_pointer
tbm_prepare_shared_iterate(TIDBitmap *tbm)
{
  dsa_pointer dp;
  TBMSharedIteratorState *istate;
  PTEntryArray *ptbase = NULL;
  PTIterationArray *ptpages = NULL;
  PTIterationArray *ptchunks = NULL;

  Assert(tbm->dsa != NULL);
  Assert(tbm->iterating != TBM_ITERATING_PRIVATE);

     
                                                                             
                                                                         
     
  dp = dsa_allocate0(tbm->dsa, sizeof(TBMSharedIteratorState));
  istate = dsa_get_address(tbm->dsa, dp);

     
                                                                            
                                                                            
                                  
     
  if (tbm->iterating == TBM_NOT_ITERATING)
  {
    pagetable_iterator i;
    PagetableEntry *page;
    int idx;
    int npages;
    int nchunks;

       
                                                                      
                                  
       
    if (tbm->npages)
    {
      tbm->ptpages = dsa_allocate(tbm->dsa, sizeof(PTIterationArray) + tbm->npages * sizeof(int));
      ptpages = dsa_get_address(tbm->dsa, tbm->ptpages);
      pg_atomic_init_u32(&ptpages->refcount, 0);
    }
    if (tbm->nchunks)
    {
      tbm->ptchunks = dsa_allocate(tbm->dsa, sizeof(PTIterationArray) + tbm->nchunks * sizeof(int));
      ptchunks = dsa_get_address(tbm->dsa, tbm->ptchunks);
      pg_atomic_init_u32(&ptchunks->refcount, 0);
    }

       
                                                                     
                                                                
                                                                        
                     
       
    npages = nchunks = 0;
    if (tbm->status == TBM_HASH)
    {
      ptbase = dsa_get_address(tbm->dsa, tbm->dsapagetable);

      pagetable_start_iterate(tbm->pagetable, &i);
      while ((page = pagetable_iterate(tbm->pagetable, &i)) != NULL)
      {
        idx = page - ptbase->ptentry;
        if (page->ischunk)
        {
          ptchunks->index[nchunks++] = idx;
        }
        else
        {
          ptpages->index[npages++] = idx;
        }
      }

      Assert(npages == tbm->npages);
      Assert(nchunks == tbm->nchunks);
    }
    else if (tbm->status == TBM_ONE_PAGE)
    {
         
                                                                      
                                                                     
                     
         
      tbm->dsapagetable = dsa_allocate(tbm->dsa, sizeof(PTEntryArray) + sizeof(PagetableEntry));
      ptbase = dsa_get_address(tbm->dsa, tbm->dsapagetable);
      memcpy(ptbase->ptentry, &tbm->entry1, sizeof(PagetableEntry));
      ptpages->index[0] = 0;
    }

    if (ptbase != NULL)
    {
      pg_atomic_init_u32(&ptbase->refcount, 0);
    }
    if (npages > 1)
    {
      qsort_arg((void *)(ptpages->index), npages, sizeof(int), tbm_shared_comparator, (void *)ptbase->ptentry);
    }
    if (nchunks > 1)
    {
      qsort_arg((void *)(ptchunks->index), nchunks, sizeof(int), tbm_shared_comparator, (void *)ptbase->ptentry);
    }
  }

     
                                                                         
                                
     
  istate->nentries = tbm->nentries;
  istate->maxentries = tbm->maxentries;
  istate->npages = tbm->npages;
  istate->nchunks = tbm->nchunks;
  istate->pagetable = tbm->dsapagetable;
  istate->spages = tbm->ptpages;
  istate->schunks = tbm->ptchunks;

  ptbase = dsa_get_address(tbm->dsa, tbm->dsapagetable);
  ptpages = dsa_get_address(tbm->dsa, tbm->ptpages);
  ptchunks = dsa_get_address(tbm->dsa, tbm->ptchunks);

     
                                                                           
                                                                             
                                                                           
     
  if (ptbase != NULL)
  {
    pg_atomic_add_fetch_u32(&ptbase->refcount, 1);
  }
  if (ptpages != NULL)
  {
    pg_atomic_add_fetch_u32(&ptpages->refcount, 1);
  }
  if (ptchunks != NULL)
  {
    pg_atomic_add_fetch_u32(&ptchunks->refcount, 1);
  }

                                    
  LWLockInitialize(&istate->lock, LWTRANCHE_TBM);

                                            
  istate->schunkbit = 0;
  istate->schunkptr = 0;
  istate->spageptr = 0;

  tbm->iterating = TBM_ITERATING_SHARED;

  return dp;
}

   
                                                                  
   
                                                           
   
static inline int
tbm_extract_page_tuple(PagetableEntry *page, TBMIterateResult *output)
{
  int wordnum;
  int ntuples = 0;

  for (wordnum = 0; wordnum < WORDS_PER_PAGE; wordnum++)
  {
    bitmapword w = page->words[wordnum];

    if (w != 0)
    {
      int off = wordnum * BITS_PER_BITMAPWORD + 1;

      while (w != 0)
      {
        if (w & 1)
        {
          output->offsets[ntuples++] = (OffsetNumber)off;
        }
        off++;
        w >>= 1;
      }
    }
  }

  return ntuples;
}

   
                                                 
   
static inline void
tbm_advance_schunkbit(PagetableEntry *chunk, int *schunkbitp)
{
  int schunkbit = *schunkbitp;

  while (schunkbit < PAGES_PER_CHUNK)
  {
    int wordnum = WORDNUM(schunkbit);
    int bitnum = BITNUM(schunkbit);

    if ((chunk->words[wordnum] & ((bitmapword)1 << bitnum)) != 0)
    {
      break;
    }
    schunkbit++;
  }

  *schunkbitp = schunkbit;
}

   
                                                       
   
                                                                          
                                                                             
                                                                            
                                                                         
                                                                      
                                                                          
                                                                          
                                                          
   
TBMIterateResult *
tbm_iterate(TBMIterator *iterator)
{
  TIDBitmap *tbm = iterator->tbm;
  TBMIterateResult *output = &(iterator->output);

  Assert(tbm->iterating == TBM_ITERATING_PRIVATE);

     
                                                                      
                                    
     
  while (iterator->schunkptr < tbm->nchunks)
  {
    PagetableEntry *chunk = tbm->schunks[iterator->schunkptr];
    int schunkbit = iterator->schunkbit;

    tbm_advance_schunkbit(chunk, &schunkbit);
    if (schunkbit < PAGES_PER_CHUNK)
    {
      iterator->schunkbit = schunkbit;
      break;
    }
                               
    iterator->schunkptr++;
    iterator->schunkbit = 0;
  }

     
                                                                         
                   
     
  if (iterator->schunkptr < tbm->nchunks)
  {
    PagetableEntry *chunk = tbm->schunks[iterator->schunkptr];
    BlockNumber chunk_blockno;

    chunk_blockno = chunk->blockno + iterator->schunkbit;
    if (iterator->spageptr >= tbm->npages || chunk_blockno < tbm->spages[iterator->spageptr]->blockno)
    {
                                                        
      output->blockno = chunk_blockno;
      output->ntuples = -1;
      output->recheck = true;
      iterator->schunkbit++;
      return output;
    }
  }

  if (iterator->spageptr < tbm->npages)
  {
    PagetableEntry *page;
    int ntuples;

                                                                
    if (tbm->status == TBM_ONE_PAGE)
    {
      page = &tbm->entry1;
    }
    else
    {
      page = tbm->spages[iterator->spageptr];
    }

                                                          
    ntuples = tbm_extract_page_tuple(page, output);
    output->blockno = page->blockno;
    output->ntuples = ntuples;
    output->recheck = page->recheck;
    iterator->spageptr++;
    return output;
  }

                                  
  return NULL;
}

   
                                                              
   
                                                                     
                                                                       
                                        
   
TBMIterateResult *
tbm_shared_iterate(TBMSharedIterator *iterator)
{
  TBMIterateResult *output = &iterator->output;
  TBMSharedIteratorState *istate = iterator->state;
  PagetableEntry *ptbase = NULL;
  int *idxpages = NULL;
  int *idxchunks = NULL;

  if (iterator->ptbase != NULL)
  {
    ptbase = iterator->ptbase->ptentry;
  }
  if (iterator->ptpages != NULL)
  {
    idxpages = iterator->ptpages->index;
  }
  if (iterator->ptchunks != NULL)
  {
    idxchunks = iterator->ptchunks->index;
  }

                                                              
  LWLockAcquire(&istate->lock, LW_EXCLUSIVE);

     
                                                                      
                                    
     
  while (istate->schunkptr < istate->nchunks)
  {
    PagetableEntry *chunk = &ptbase[idxchunks[istate->schunkptr]];
    int schunkbit = istate->schunkbit;

    tbm_advance_schunkbit(chunk, &schunkbit);
    if (schunkbit < PAGES_PER_CHUNK)
    {
      istate->schunkbit = schunkbit;
      break;
    }
                               
    istate->schunkptr++;
    istate->schunkbit = 0;
  }

     
                                                                         
                   
     
  if (istate->schunkptr < istate->nchunks)
  {
    PagetableEntry *chunk = &ptbase[idxchunks[istate->schunkptr]];
    BlockNumber chunk_blockno;

    chunk_blockno = chunk->blockno + istate->schunkbit;

    if (istate->spageptr >= istate->npages || chunk_blockno < ptbase[idxpages[istate->spageptr]].blockno)
    {
                                                        
      output->blockno = chunk_blockno;
      output->ntuples = -1;
      output->recheck = true;
      istate->schunkbit++;

      LWLockRelease(&istate->lock);
      return output;
    }
  }

  if (istate->spageptr < istate->npages)
  {
    PagetableEntry *page = &ptbase[idxpages[istate->spageptr]];
    int ntuples;

                                                          
    ntuples = tbm_extract_page_tuple(page, output);
    output->blockno = page->blockno;
    output->ntuples = ntuples;
    output->recheck = page->recheck;
    istate->spageptr++;

    LWLockRelease(&istate->lock);

    return output;
  }

  LWLockRelease(&istate->lock);

                                  
  return NULL;
}

   
                                                          
   
                                                                       
                                                                      
                                                                            
   
void
tbm_end_iterate(TBMIterator *iterator)
{
  pfree(iterator);
}

   
                                                                       
   
                                                                           
                                   
   
void
tbm_end_shared_iterate(TBMSharedIterator *iterator)
{
  pfree(iterator);
}

   
                                                             
   
                                                               
   
static const PagetableEntry *
tbm_find_pageentry(const TIDBitmap *tbm, BlockNumber pageno)
{
  const PagetableEntry *page;

  if (tbm->nentries == 0)                                      
  {
    return NULL;
  }

  if (tbm->status == TBM_ONE_PAGE)
  {
    page = &tbm->entry1;
    if (page->blockno != pageno)
    {
      return NULL;
    }
    Assert(!page->ischunk);
    return page;
  }

  page = pagetable_lookup(tbm->pagetable, pageno);
  if (page == NULL)
  {
    return NULL;
  }
  if (page->ischunk)
  {
    return NULL;                                      
  }
  return page;
}

   
                                                                      
   
                                                              
   
                                                                      
                                                                        
   
static PagetableEntry *
tbm_get_pageentry(TIDBitmap *tbm, BlockNumber pageno)
{
  PagetableEntry *page;
  bool found;

  if (tbm->status == TBM_EMPTY)
  {
                            
    page = &tbm->entry1;
    found = false;
    tbm->status = TBM_ONE_PAGE;
  }
  else
  {
    if (tbm->status == TBM_ONE_PAGE)
    {
      page = &tbm->entry1;
      if (page->blockno == pageno)
      {
        return page;
      }
                                                       
      tbm_create_pagetable(tbm);
    }

                                    
    page = pagetable_insert(tbm->pagetable, pageno, &found);
  }

                                           
  if (!found)
  {
    char oldstatus = page->status;

    MemSet(page, 0, sizeof(PagetableEntry));
    page->status = oldstatus;
    page->blockno = pageno;
                           
    tbm->nentries++;
    tbm->npages++;
  }

  return page;
}

   
                                                             
   
static bool
tbm_page_is_lossy(const TIDBitmap *tbm, BlockNumber pageno)
{
  PagetableEntry *page;
  BlockNumber chunk_pageno;
  int bitno;

                                                           
  if (tbm->nchunks == 0)
  {
    return false;
  }
  Assert(tbm->status == TBM_HASH);

  bitno = pageno % PAGES_PER_CHUNK;
  chunk_pageno = pageno - bitno;

  page = pagetable_lookup(tbm->pagetable, chunk_pageno);

  if (page != NULL && page->ischunk)
  {
    int wordnum = WORDNUM(bitno);
    int bitnum = BITNUM(bitno);

    if ((page->words[wordnum] & ((bitmapword)1 << bitnum)) != 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                                
   
                                                                      
                                                                        
   
static void
tbm_mark_page_lossy(TIDBitmap *tbm, BlockNumber pageno)
{
  PagetableEntry *page;
  bool found;
  BlockNumber chunk_pageno;
  int bitno;
  int wordnum;
  int bitnum;

                                                                   
  if (tbm->status != TBM_HASH)
  {
    tbm_create_pagetable(tbm);
  }

  bitno = pageno % PAGES_PER_CHUNK;
  chunk_pageno = pageno - bitno;

     
                                                                             
                                                                    
     
  if (bitno != 0)
  {
    if (pagetable_delete(tbm->pagetable, pageno))
    {
                                            
      tbm->nentries--;
      tbm->npages--;                                         
    }
  }

                                                     
  page = pagetable_insert(tbm->pagetable, chunk_pageno, &found);

                                           
  if (!found)
  {
    char oldstatus = page->status;

    MemSet(page, 0, sizeof(PagetableEntry));
    page->status = oldstatus;
    page->blockno = chunk_pageno;
    page->ischunk = true;
                           
    tbm->nentries++;
    tbm->nchunks++;
  }
  else if (!page->ischunk)
  {
    char oldstatus = page->status;

                                                                 
    MemSet(page, 0, sizeof(PagetableEntry));
    page->status = oldstatus;
    page->blockno = chunk_pageno;
    page->ischunk = true;
                                                                  
    page->words[0] = ((bitmapword)1 << 0);
                       
    tbm->nchunks++;
    tbm->npages--;
  }

                                              
  wordnum = WORDNUM(bitno);
  bitnum = BITNUM(bitno);
  page->words[wordnum] |= ((bitmapword)1 << bitnum);
}

   
                                                                          
   
static void
tbm_lossify(TIDBitmap *tbm)
{
  pagetable_iterator i;
  PagetableEntry *page;

     
                                                                    
                                                                          
                                               
     
                                                                           
                                                                             
                                                                         
     
  Assert(tbm->iterating == TBM_NOT_ITERATING);
  Assert(tbm->status == TBM_HASH);

  pagetable_start_iterate_at(tbm->pagetable, &i, tbm->lossify_start);
  while ((page = pagetable_iterate(tbm->pagetable, &i)) != NULL)
  {
    if (page->ischunk)
    {
      continue;                             
    }

       
                                                                          
                                           
       
    if ((page->blockno % PAGES_PER_CHUNK) == 0)
    {
      continue;
    }

                                      
    tbm_mark_page_lossy(tbm, page->blockno);

    if (tbm->nentries <= tbm->maxentries / 2)
    {
         
                                                                      
                                                              
         
      tbm->lossify_start = i.cur;
      break;
    }

       
                                                                          
                                                                   
                                                                     
                                                                    
       
  }

     
                                                                            
                                                                      
                                                                         
                                                                          
                                                                       
                                                                             
                                                                             
                                                  
     
  if (tbm->nentries > tbm->maxentries / 2)
  {
    tbm->maxentries = Min(tbm->nentries, (INT_MAX - 1) / 2) * 2;
  }
}

   
                                                       
   
static int
tbm_comparator(const void *left, const void *right)
{
  BlockNumber l = (*((PagetableEntry *const *)left))->blockno;
  BlockNumber r = (*((PagetableEntry *const *)right))->blockno;

  if (l < r)
  {
    return -1;
  }
  else if (l > r)
  {
    return 1;
  }
  return 0;
}

   
                                                                            
                                                                              
            
   
static int
tbm_shared_comparator(const void *left, const void *right, void *arg)
{
  PagetableEntry *base = (PagetableEntry *)arg;
  PagetableEntry *lpage = &base[*(int *)left];
  PagetableEntry *rpage = &base[*(int *)right];

  if (lpage->blockno < rpage->blockno)
  {
    return -1;
  }
  else if (lpage->blockno > rpage->blockno)
  {
    return 1;
  }
  return 0;
}

   
                             
   
                                                                            
                                                         
   
                                                                           
                         
   
TBMSharedIterator *
tbm_attach_shared_iterate(dsa_area *dsa, dsa_pointer dp)
{
  TBMSharedIterator *iterator;
  TBMSharedIteratorState *istate;

     
                                                                        
                                                         
     
  iterator = (TBMSharedIterator *)palloc0(sizeof(TBMSharedIterator) + MAX_TUPLES_PER_PAGE * sizeof(OffsetNumber));

  istate = (TBMSharedIteratorState *)dsa_get_address(dsa, dp);

  iterator->state = istate;

  iterator->ptbase = dsa_get_address(dsa, istate->pagetable);

  if (istate->npages)
  {
    iterator->ptpages = dsa_get_address(dsa, istate->spages);
  }
  if (istate->nchunks)
  {
    iterator->ptchunks = dsa_get_address(dsa, istate->schunks);
  }

  return iterator;
}

   
                      
   
                                                                       
                                                                   
   
static inline void *
pagetable_allocate(pagetable_hash *pagetable, Size size)
{
  TIDBitmap *tbm = (TIDBitmap *)pagetable->private_data;
  PTEntryArray *ptbase;

  if (tbm->dsa == NULL)
  {
    return MemoryContextAllocExtended(pagetable->ctx, size, MCXT_ALLOC_HUGE | MCXT_ALLOC_ZERO);
  }

     
                                                                          
                                                               
     
  tbm->dsapagetableold = tbm->dsapagetable;
  tbm->dsapagetable = dsa_allocate_extended(tbm->dsa, sizeof(PTEntryArray) + size, DSA_ALLOC_HUGE | DSA_ALLOC_ZERO);
  ptbase = dsa_get_address(tbm->dsa, tbm->dsapagetable);

  return ptbase->ptentry;
}

   
                  
   
                                                      
   
static inline void
pagetable_free(pagetable_hash *pagetable, void *pointer)
{
  TIDBitmap *tbm = (TIDBitmap *)pagetable->private_data;

                                                       
  if (tbm->dsa == NULL)
  {
    pfree(pointer);
  }
  else if (DsaPointerIsValid(tbm->dsapagetableold))
  {
    dsa_free(tbm->dsa, tbm->dsapagetableold);
    tbm->dsapagetableold = InvalidDsaPointer;
  }
}

   
                         
   
                                                                     
   
long
tbm_calculate_entries(double maxbytes)
{
  long nbuckets;

     
                                                                            
                                                                             
                                                                            
                                       
     
  nbuckets = maxbytes / (sizeof(PagetableEntry) + sizeof(Pointer) + sizeof(Pointer));
  nbuckets = Min(nbuckets, INT_MAX - 1);                   
  nbuckets = Max(nbuckets, 16);                            

  return nbuckets;
}
