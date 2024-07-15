                                                                            
   
             
                                               
   
   
                                                                         
                                                                        
   
                  
                                      
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "access/gin_private.h"
#include "utils/datum.h"
#include "utils/memutils.h"

#define DEF_NENTRY 2048                                             
#define DEF_NPTR 5                                                  

                                    
static void
ginCombineData(RBTNode *existing, const RBTNode *newdata, void *arg)
{
  GinEntryAccumulator *eo = (GinEntryAccumulator *)existing;
  const GinEntryAccumulator *en = (const GinEntryAccumulator *)newdata;
  BuildAccumulator *accum = (BuildAccumulator *)arg;

     
                                                                        
     
  if (eo->count >= eo->maxcount)
  {
    if (eo->maxcount > INT_MAX)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("posting list is too long"), errhint("Reduce maintenance_work_mem.")));
    }

    accum->allocatedMemory -= GetMemoryChunkSpace(eo->list);
    eo->maxcount *= 2;
    eo->list = (ItemPointerData *)repalloc_huge(eo->list, sizeof(ItemPointerData) * eo->maxcount);
    accum->allocatedMemory += GetMemoryChunkSpace(eo->list);
  }

                                                                           
  if (eo->shouldSort == false)
  {
    int res;

    res = ginCompareItemPointers(eo->list + eo->count - 1, en->list);
    Assert(res != 0);

    if (res > 0)
    {
      eo->shouldSort = true;
    }
  }

  eo->list[eo->count] = en->list[0];
  eo->count++;
}

                                      
static int
cmpEntryAccumulator(const RBTNode *a, const RBTNode *b, void *arg)
{
  const GinEntryAccumulator *ea = (const GinEntryAccumulator *)a;
  const GinEntryAccumulator *eb = (const GinEntryAccumulator *)b;
  BuildAccumulator *accum = (BuildAccumulator *)arg;

  return ginCompareAttEntries(accum->ginstate, ea->attnum, ea->key, ea->category, eb->attnum, eb->key, eb->category);
}

                                     
static RBTNode *
ginAllocEntryAccumulator(void *arg)
{
  BuildAccumulator *accum = (BuildAccumulator *)arg;
  GinEntryAccumulator *ea;

     
                                                                            
                                                                   
     
  if (accum->entryallocator == NULL || accum->eas_used >= DEF_NENTRY)
  {
    accum->entryallocator = palloc(sizeof(GinEntryAccumulator) * DEF_NENTRY);
    accum->allocatedMemory += GetMemoryChunkSpace(accum->entryallocator);
    accum->eas_used = 0;
  }

                                               
  ea = accum->entryallocator + accum->eas_used;
  accum->eas_used++;

  return (RBTNode *)ea;
}

void
ginInitBA(BuildAccumulator *accum)
{
                                                     
  accum->allocatedMemory = 0;
  accum->entryallocator = NULL;
  accum->eas_used = 0;
  accum->tree = rbt_create(sizeof(GinEntryAccumulator), cmpEntryAccumulator, ginCombineData, ginAllocEntryAccumulator, NULL,                         
      (void *)accum);
}

   
                                                                    
                                             
   
static Datum
getDatumCopy(BuildAccumulator *accum, OffsetNumber attnum, Datum value)
{
  Form_pg_attribute att;
  Datum res;

  att = TupleDescAttr(accum->ginstate->origTupdesc, attnum - 1);
  if (att->attbyval)
  {
    res = value;
  }
  else
  {
    res = datumCopy(value, false, att->attlen);
    accum->allocatedMemory += GetMemoryChunkSpace(DatumGetPointer(res));
  }
  return res;
}

   
                                            
   
static void
ginInsertBAEntry(BuildAccumulator *accum, ItemPointer heapptr, OffsetNumber attnum, Datum key, GinNullCategory category)
{
  GinEntryAccumulator eatmp;
  GinEntryAccumulator *ea;
  bool isNew;

     
                                                                             
                                            
     
  eatmp.attnum = attnum;
  eatmp.key = key;
  eatmp.category = category;
                                                        
  eatmp.list = heapptr;

  ea = (GinEntryAccumulator *)rbt_insert(accum->tree, (RBTNode *)&eatmp, &isNew);

  if (isNew)
  {
       
                                                                      
                                                               
       
    if (category == GIN_CAT_NORM_KEY)
    {
      ea->key = getDatumCopy(accum, attnum, key);
    }
    ea->maxcount = DEF_NPTR;
    ea->count = 1;
    ea->shouldSort = false;
    ea->list = (ItemPointerData *)palloc(sizeof(ItemPointerData) * DEF_NPTR);
    ea->list[0] = *heapptr;
    accum->allocatedMemory += GetMemoryChunkSpace(ea->list);
  }
  else
  {
       
                                             
       
  }
}

   
                                            
   
                                                                         
                                                                              
                                                                      
                                                                              
                                                                               
                      
   
                                                                              
                                                                          
                                                                           
                                                                             
                             
   
void
ginInsertBAEntries(BuildAccumulator *accum, ItemPointer heapptr, OffsetNumber attnum, Datum *entries, GinNullCategory *categories, int32 nentries)
{
  uint32 step = nentries;

  if (nentries <= 0)
  {
    return;
  }

  Assert(ItemPointerIsValid(heapptr) && attnum >= FirstOffsetNumber);

     
                                                          
     
  step |= (step >> 1);
  step |= (step >> 2);
  step |= (step >> 4);
  step |= (step >> 8);
  step |= (step >> 16);
  step >>= 1;
  step++;

  while (step > 0)
  {
    int i;

    for (i = step - 1; i < nentries && i >= 0; i += step << 1         )
    {
      ginInsertBAEntry(accum, heapptr, attnum, entries[i], categories[i]);
    }

    step >>= 1;         
  }
}

static int
qsortCompareItemPointers(const void *a, const void *b)
{
  int res = ginCompareItemPointers((ItemPointer)a, (ItemPointer)b);

                                                                 
  Assert(res != 0);
  return res;
}

                                                                 
void
ginBeginBAScan(BuildAccumulator *accum)
{
  rbt_begin_iterate(accum->tree, LeftRightWalk, &accum->tree_walk);
}

   
                                                                      
                                                                         
                                                                         
   
ItemPointerData *
ginGetBAEntry(BuildAccumulator *accum, OffsetNumber *attnum, Datum *key, GinNullCategory *category, uint32 *n)
{
  GinEntryAccumulator *entry;
  ItemPointerData *list;

  entry = (GinEntryAccumulator *)rbt_iterate(&accum->tree_walk);

  if (entry == NULL)
  {
    return NULL;                      
  }

  *attnum = entry->attnum;
  *key = entry->key;
  *category = entry->category;
  list = entry->list;
  *n = entry->count;

  Assert(list != NULL && entry->count > 0);

  if (entry->shouldSort && entry->count > 1)
  {
    qsort(list, entry->count, sizeof(ItemPointerData), qsortCompareItemPointers);
  }

  return list;
}
