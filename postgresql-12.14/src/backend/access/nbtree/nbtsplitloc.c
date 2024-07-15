                                                                            
   
                 
                                                                
   
                                                                         
                                                                        
   
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/nbtree.h"
#include "storage/lmgr.h"

                                                      
#define MAX_LEAF_INTERVAL 9
#define MAX_INTERNAL_INTERVAL 18

typedef enum
{
                                                                        
  SPLIT_DEFAULT,                                             
  SPLIT_MANY_DUPLICATES,                                          
  SPLIT_SINGLE_VALUE                                      
} FindSplitStrat;

typedef struct
{
                                           
  int16 curdelta;                                        
  int16 leftfree;                                          
  int16 rightfree;                                          

                                                                     
  OffsetNumber firstoldonright;                                   
  bool newitemonleft;                                                 

} SplitPoint;

typedef struct
{
                                        
  Relation rel;                                
  Page page;                                          
  IndexTuple newitem;                                          
  Size newitemsz;                                                       
  bool is_leaf;                                            
  bool is_rightmost;                                                   
  OffsetNumber newitemoff;                                           
  int leftspace;                                                       
  int rightspace;                                                       
  int olddataitemstotal;                                 
  Size minfirstrightsz;                                             

                                  
  int maxsplits;                                    
  int nsplits;                                      
  SplitPoint *splits;                                          
  int interval;                                                     
} FindSplitData;

static void
_bt_recsplitloc(FindSplitData *state, OffsetNumber firstoldonright, bool newitemonleft, int olddataitemstoleft, Size firstoldonrightsz);
static void
_bt_deltasortsplits(FindSplitData *state, double fillfactormult, bool usemult);
static int
_bt_splitcmp(const void *arg1, const void *arg2);
static bool
_bt_afternewitemoff(FindSplitData *state, OffsetNumber maxoff, int leaffillfactor, bool *usemult);
static bool
_bt_adjacenthtid(ItemPointer lowhtid, ItemPointer highhtid);
static OffsetNumber
_bt_bestsplitloc(FindSplitData *state, int perfectpenalty, bool *newitemonleft, FindSplitStrat strategy);
static int
_bt_strategy(FindSplitData *state, SplitPoint *leftpage, SplitPoint *rightpage, FindSplitStrat *strategy);
static void
_bt_interval_edges(FindSplitData *state, SplitPoint **leftinterval, SplitPoint **rightinterval);
static inline int
_bt_split_penalty(FindSplitData *state, SplitPoint *split);
static inline IndexTuple
_bt_split_lastleft(FindSplitData *state, SplitPoint *split);
static inline IndexTuple
_bt_split_firstright(FindSplitData *state, SplitPoint *split);

   
                                                                    
   
                                                                         
                                                                          
                                                                            
                              
   
                                                                             
                                                                            
                                                                           
                                                                           
                                                                           
                                                                          
                                                                          
                                                                       
                                
   
                                                                             
                                                                       
                                                                             
                                                                         
                                                                      
                                                                         
                                                                             
                                                                          
                                                                            
                                                                             
                                                                     
                                                                          
                      
   
                                                                         
                                                                           
                                                                           
                                   
   
OffsetNumber
_bt_findsplitloc(Relation rel, Page page, OffsetNumber newitemoff, Size newitemsz, IndexTuple newitem, bool *newitemonleft)
{
  BTPageOpaque opaque;
  int leftspace, rightspace, olddataitemstotal, olddataitemstoleft, perfectpenalty, leaffillfactor;
  FindSplitData state;
  FindSplitStrat strategy;
  ItemId itemid;
  OffsetNumber offnum, maxoff, foundfirstright;
  double fillfactormult;
  bool usemult;
  SplitPoint leftpage, rightpage;

  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  maxoff = PageGetMaxOffsetNumber(page);

                                                                        
  leftspace = rightspace = PageGetPageSize(page) - SizeOfPageHeaderData - MAXALIGN(sizeof(BTPageOpaqueData));

                                                                  
  if (!P_RIGHTMOST(opaque))
  {
    itemid = PageGetItemId(page, P_HIKEY);
    rightspace -= (int)(MAXALIGN(ItemIdGetLength(itemid)) + sizeof(ItemIdData));
  }

                                                                       
  olddataitemstotal = rightspace - (int)PageGetExactFreeSpace(page);
  leaffillfactor = RelationGetFillFactor(rel, BTREE_DEFAULT_FILLFACTOR);

                                                                           
  newitemsz += sizeof(ItemIdData);
  state.rel = rel;
  state.page = page;
  state.newitem = newitem;
  state.newitemsz = newitemsz;
  state.is_leaf = P_ISLEAF(opaque);
  state.is_rightmost = P_RIGHTMOST(opaque);
  state.leftspace = leftspace;
  state.rightspace = rightspace;
  state.olddataitemstotal = olddataitemstotal;
  state.minfirstrightsz = SIZE_MAX;
  state.newitemoff = newitemoff;

     
                                                                           
                                                                            
                                                                        
                                                                         
                                    
     
  state.maxsplits = maxoff;
  state.splits = palloc(sizeof(SplitPoint) * state.maxsplits);
  state.nsplits = 0;

     
                                                                          
                            
     
  olddataitemstoleft = 0;

  for (offnum = P_FIRSTDATAKEY(opaque); offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    Size itemsz;

    itemid = PageGetItemId(page, offnum);
    itemsz = MAXALIGN(ItemIdGetLength(itemid)) + sizeof(ItemIdData);

       
                                                                      
                                                                          
                                                                          
                                                                        
                                          
       
    if (offnum < newitemoff)
    {
      _bt_recsplitloc(&state, offnum, false, olddataitemstoleft, itemsz);
    }
    else if (offnum > newitemoff)
    {
      _bt_recsplitloc(&state, offnum, true, olddataitemstoleft, itemsz);
    }
    else
    {
         
                                                                      
                                        
         
      _bt_recsplitloc(&state, offnum, false, olddataitemstoleft, itemsz);

         
                                                                 
                                                           
         
      _bt_recsplitloc(&state, offnum, true, olddataitemstoleft, itemsz);
    }

    olddataitemstoleft += itemsz;
  }

     
                                                                            
                                                                           
                  
     
  Assert(olddataitemstoleft == olddataitemstotal);
  if (newitemoff > maxoff)
  {
    _bt_recsplitloc(&state, newitemoff, false, olddataitemstotal, 0);
  }

     
                                                                             
                 
     
  if (state.nsplits == 0)
  {
    elog(ERROR, "could not find a feasible split point for index \"%s\"", RelationGetRelationName(rel));
  }

     
                                                                            
                                                                           
                                                                          
                                                                         
                                                                           
                                                                         
                  
     
                                                                           
                                                                             
                                                                          
                                                                         
                                                                         
                                                                     
                 
     
  if (!state.is_leaf)
  {
                                                    
    usemult = state.is_rightmost;
    fillfactormult = BTREE_NONLEAF_FILLFACTOR / 100.0;
  }
  else if (state.is_rightmost)
  {
                                                            
    usemult = true;
    fillfactormult = leaffillfactor / 100.0;
  }
  else if (_bt_afternewitemoff(&state, maxoff, leaffillfactor, &usemult))
  {
       
                                                                          
                                                                           
                                                                           
                                                                           
                
       
    if (usemult)
    {
                                                                 
      fillfactormult = leaffillfactor / 100.0;
    }
    else
    {
                                                     
      for (int i = 0; i < state.nsplits; i++)
      {
        SplitPoint *split = state.splits + i;

        if (split->newitemonleft && newitemoff == split->firstoldonright)
        {
          pfree(state.splits);
          *newitemonleft = true;
          return newitemoff;
        }
      }

         
                                                                   
                                                                      
                                             
         
      fillfactormult = 0.50;
    }
  }
  else
  {
                                             
    usemult = false;
                                              
    fillfactormult = 0.50;
  }

     
                                                                          
                                                                          
                                                                         
                                                                           
                                                                          
                                                                    
                                            
     
  state.interval = Min(Max(1, state.nsplits * 0.05), state.is_leaf ? MAX_LEAF_INTERVAL : MAX_INTERNAL_INTERVAL);

     
                                                                         
                                                     
     
  leftpage = state.splits[0];
  rightpage = state.splits[state.nsplits - 1];

                                                                         
  _bt_deltasortsplits(&state, fillfactormult, usemult);

     
                                                                 
                                                                           
                                                                      
                                                                 
                                                                        
                            
     
                                                                            
                                                                           
                                                                            
                                                                        
                           
     
  perfectpenalty = _bt_strategy(&state, &leftpage, &rightpage, &strategy);

  if (strategy == SPLIT_DEFAULT)
  {
       
                                                                          
                                             
       
  }

     
                                                                         
                                                                         
     
                                                                        
                                                                            
                                                                          
                                                                       
                                                                        
                                                                      
                                                                      
                                                                     
                                                                     
                          
     
                                                                            
                                                                      
                                                                     
                                                                            
                                                                            
                                                                  
                                                      
     
  else if (strategy == SPLIT_MANY_DUPLICATES)
  {
    Assert(state.is_leaf);
                                                              
    Assert(perfectpenalty == IndexRelationGetNumberOfKeyAttributes(state.rel));
                                                                        
    state.interval = state.nsplits;
  }
  else if (strategy == SPLIT_SINGLE_VALUE)
  {
    Assert(state.is_leaf);
                                        
    usemult = true;
    fillfactormult = BTREE_SINGLEVAL_FILLFACTOR / 100.0;
                                            
    _bt_deltasortsplits(&state, fillfactormult, usemult);
                                                                       
    state.interval = 1;
  }

     
                                                                           
                                                                         
                                                    
     
  foundfirstright = _bt_bestsplitloc(&state, perfectpenalty, newitemonleft, strategy);
  pfree(state.splits);

  return foundfirstright;
}

   
                                                                            
                                                                      
                                                                            
                                                                       
   
                                                                             
                                                                            
                                                                              
                                                                           
                                        
   
                                                                            
                                                                     
                                          
   
static void
_bt_recsplitloc(FindSplitData *state, OffsetNumber firstoldonright, bool newitemonleft, int olddataitemstoleft, Size firstoldonrightsz)
{
  int16 leftfree, rightfree;
  Size firstrightitemsz;
  bool newitemisfirstonright;

                                                                     
  newitemisfirstonright = (firstoldonright == state->newitemoff && !newitemonleft);

  if (newitemisfirstonright)
  {
    firstrightitemsz = state->newitemsz;
  }
  else
  {
    firstrightitemsz = firstoldonrightsz;
  }

                                      
  leftfree = state->leftspace - olddataitemstoleft;
  rightfree = state->rightspace - (state->olddataitemstotal - olddataitemstoleft);

     
                                                                             
                                                                       
                                                                           
                                                                            
                                                                            
                                                                    
                                                                             
                                                                      
                                                                       
                                                                            
         
     
                                                                             
                                                                             
                                                                         
                                                                      
     
  if (state->is_leaf)
  {
    leftfree -= (int16)(firstrightitemsz + MAXALIGN(sizeof(ItemPointerData)));
  }
  else
  {
    leftfree -= (int16)firstrightitemsz;
  }

                                
  if (newitemonleft)
  {
    leftfree -= (int16)state->newitemsz;
  }
  else
  {
    rightfree -= (int16)state->newitemsz;
  }

     
                                                                         
                                                               
     
  if (!state->is_leaf)
  {
    rightfree += (int16)firstrightitemsz - (int16)(MAXALIGN(sizeof(IndexTupleData)) + sizeof(ItemIdData));
  }

                             
  if (leftfree >= 0 && rightfree >= 0)
  {
    Assert(state->nsplits < state->maxsplits);

                                                         
    state->minfirstrightsz = Min(state->minfirstrightsz, firstrightitemsz);

    state->splits[state->nsplits].curdelta = 0;
    state->splits[state->nsplits].leftfree = leftfree;
    state->splits[state->nsplits].rightfree = rightfree;
    state->splits[state->nsplits].firstoldonright = firstoldonright;
    state->splits[state->nsplits].newitemonleft = newitemonleft;
    state->nsplits++;
  }
}

   
                                                                              
                                                                               
   
static void
_bt_deltasortsplits(FindSplitData *state, double fillfactormult, bool usemult)
{
  for (int i = 0; i < state->nsplits; i++)
  {
    SplitPoint *split = state->splits + i;
    int16 delta;

    if (usemult)
    {
      delta = fillfactormult * split->leftfree - (1.0 - fillfactormult) * split->rightfree;
    }
    else
    {
      delta = split->leftfree - split->rightfree;
    }

    if (delta < 0)
    {
      delta = -delta;
    }

                    
    split->curdelta = delta;
  }

  qsort(state->splits, state->nsplits, sizeof(SplitPoint), _bt_splitcmp);
}

   
                                                        
   
static int
_bt_splitcmp(const void *arg1, const void *arg2)
{
  SplitPoint *split1 = (SplitPoint *)arg1;
  SplitPoint *split2 = (SplitPoint *)arg2;

  if (split1->curdelta > split2->curdelta)
  {
    return 1;
  }
  if (split1->curdelta < split2->curdelta)
  {
    return -1;
  }

  return 0;
}

   
                                                                              
                                                                     
                                                                               
                                                                        
                                                                             
                                                                      
                                                                          
                                                                           
                                                                           
                                                                              
         
   
                                                                            
                                                                         
                                                                     
                                                                            
                                                                               
        
   
                                                                               
                                                                         
                                                                             
                                                                         
                                                                               
                                  
   
static bool
_bt_afternewitemoff(FindSplitData *state, OffsetNumber maxoff, int leaffillfactor, bool *usemult)
{
  int16 nkeyatts;
  ItemId itemid;
  IndexTuple tup;
  int keepnatts;

  Assert(state->is_leaf && !state->is_rightmost);

  nkeyatts = IndexRelationGetNumberOfKeyAttributes(state->rel);

                                              
  if (nkeyatts == 1)
  {
    return false;
  }

                                                                         
  if (state->newitemoff == P_FIRSTKEY)
  {
    return false;
  }

     
                                                                           
                                                                     
                                                                          
                                                           
     
                                                                           
                                                                          
                                                                      
                                                           
                                                             
     
  if (state->newitemsz != state->minfirstrightsz)
  {
    return false;
  }
  if (state->newitemsz * (maxoff - 1) != state->olddataitemstotal)
  {
    return false;
  }

     
                                                                    
                                                                        
                            
     
  if (state->newitemsz > MAXALIGN(sizeof(IndexTupleData) + sizeof(int64) * 2) + sizeof(ItemIdData))
  {
    return false;
  }

     
                                                                             
                                                                          
                        
     
                                                                             
                                                                         
                                                                   
     
  if (state->newitemoff > maxoff)
  {
    itemid = PageGetItemId(state->page, maxoff);
    tup = (IndexTuple)PageGetItem(state->page, itemid);
    keepnatts = _bt_keep_natts_fast(state->rel, tup, state->newitem);

    if (keepnatts > 1 && keepnatts <= nkeyatts)
    {
      *usemult = true;
      return true;
    }

    return false;
  }

     
                                                                      
                                                                            
                                                                             
                                                                      
                                                                           
                                                                          
     
                                                                             
                                                                 
                                                                         
                                                       
     
  itemid = PageGetItemId(state->page, OffsetNumberPrev(state->newitemoff));
  tup = (IndexTuple)PageGetItem(state->page, itemid);
                             
  if (!_bt_adjacenthtid(&tup->t_tid, &state->newitem->t_tid))
  {
    return false;
  }
                                                         
  keepnatts = _bt_keep_natts_fast(state->rel, tup, state->newitem);

  if (keepnatts > 1 && keepnatts <= nkeyatts)
  {
    double interp = (double)state->newitemoff / ((double)maxoff + 1);
    double leaffillfactormult = (double)leaffillfactor / 100.0;

       
                                                                           
                                                                      
                                                                    
       
    *usemult = interp > leaffillfactormult;

    return true;
  }

  return false;
}

   
                                                               
   
                                                                              
                                                                            
                
   
static bool
_bt_adjacenthtid(ItemPointer lowhtid, ItemPointer highhtid)
{
  BlockNumber lowblk, highblk;

  lowblk = ItemPointerGetBlockNumber(lowhtid);
  highblk = ItemPointerGetBlockNumber(highhtid);

                                                                      
  if (lowblk == highblk)
  {
    return true;
  }

                                                                         
  if (lowblk + 1 == highblk && ItemPointerGetOffsetNumber(highhtid) == FirstOffsetNumber)
  {
    return true;
  }

  return false;
}

   
                                                                           
                                                                               
                                                                        
                                                                            
                                                                           
            
   
                                                                       
                                                                           
                                                                         
                                                                              
                                                                              
                                       
   
                                                                               
                                                                          
   
static OffsetNumber
_bt_bestsplitloc(FindSplitData *state, int perfectpenalty, bool *newitemonleft, FindSplitStrat strategy)
{
  int bestpenalty, lowsplit;
  int highsplit = Min(state->interval, state->nsplits);
  SplitPoint *final;

  bestpenalty = INT_MAX;
  lowsplit = 0;
  for (int i = lowsplit; i < highsplit; i++)
  {
    int penalty;

    penalty = _bt_split_penalty(state, state->splits + i);

    if (penalty <= perfectpenalty)
    {
      bestpenalty = penalty;
      lowsplit = i;
      break;
    }

    if (penalty < bestpenalty)
    {
      bestpenalty = penalty;
      lowsplit = i;
    }
  }

  final = &state->splits[lowsplit];

     
                                                                            
                                                                           
                                                                             
                                                                        
                                  
     
                                                                             
                                                                         
                                                                         
                                                                          
                                                                           
                                                                         
                                                                     
                    
     
  if (strategy == SPLIT_MANY_DUPLICATES && !state->is_rightmost && !final->newitemonleft && final->firstoldonright >= state->newitemoff && final->firstoldonright < state->newitemoff + MAX_LEAF_INTERVAL)
  {
       
                                                                         
                                                                        
       
    final = &state->splits[0];
  }

  *newitemonleft = final->newitemonleft;
  return final->firstoldonright;
}

   
                                                                          
                                                                        
                                                                   
   
                                                                            
                                                                              
                                                                           
                                                                              
                                                               
   
static int
_bt_strategy(FindSplitData *state, SplitPoint *leftpage, SplitPoint *rightpage, FindSplitStrat *strategy)
{
  IndexTuple leftmost, rightmost;
  SplitPoint *leftinterval, *rightinterval;
  int perfectpenalty;
  int indnkeyatts = IndexRelationGetNumberOfKeyAttributes(state->rel);

                                                              
  *strategy = SPLIT_DEFAULT;

     
                                                                            
                                                                         
                                                                           
                                          
     
  if (!state->is_leaf)
  {
    return state->minfirstrightsz;
  }

     
                                                                             
                            
     
  _bt_interval_edges(state, &leftinterval, &rightinterval);
  leftmost = _bt_split_lastleft(state, leftinterval);
  rightmost = _bt_split_firstright(state, rightinterval);

     
                                                                            
                                                                           
                                                       
     
  perfectpenalty = _bt_keep_natts_fast(state->rel, leftmost, rightmost);
  if (perfectpenalty <= indnkeyatts)
  {
    return perfectpenalty;
  }

     
                                                                       
                                                                            
                                                                            
     
                                                                       
                                                      
     
  leftmost = _bt_split_lastleft(state, leftpage);
  rightmost = _bt_split_firstright(state, rightpage);

     
                                                                          
                                                                             
                                                                           
                        
     
  perfectpenalty = _bt_keep_natts_fast(state->rel, leftmost, rightmost);
  if (perfectpenalty <= indnkeyatts)
  {
    *strategy = SPLIT_MANY_DUPLICATES;

       
                                                                         
                                                                      
                                                                     
                                                                          
                                                                  
       
                                                                         
                                                                           
                                                                         
                                                                   
       
    return indnkeyatts;
  }

     
                                                                         
                                                                        
                                                                            
                                                                         
                                                                           
                                                                           
                                                       
     
  else if (state->is_rightmost)
  {
    *strategy = SPLIT_SINGLE_VALUE;
  }
  else
  {
    ItemId itemid;
    IndexTuple hikey;

    itemid = PageGetItemId(state->page, P_HIKEY);
    hikey = (IndexTuple)PageGetItem(state->page, itemid);
    perfectpenalty = _bt_keep_natts_fast(state->rel, hikey, state->newitem);
    if (perfectpenalty <= indnkeyatts)
    {
      *strategy = SPLIT_SINGLE_VALUE;
    }
    else
    {
         
                                                                     
                                                                        
                                       
         
    }
  }

  return perfectpenalty;
}

   
                                                                          
                                                                              
                      
   
static void
_bt_interval_edges(FindSplitData *state, SplitPoint **leftinterval, SplitPoint **rightinterval)
{
  int highsplit = Min(state->interval, state->nsplits);
  SplitPoint *deltaoptimal;

  deltaoptimal = state->splits;
  *leftinterval = NULL;
  *rightinterval = NULL;

     
                                                                       
                                                                          
           
     
  for (int i = highsplit - 1; i >= 0; i--)
  {
    SplitPoint *distant = state->splits + i;

    if (distant->firstoldonright < deltaoptimal->firstoldonright)
    {
      if (*leftinterval == NULL)
      {
        *leftinterval = distant;
      }
    }
    else if (distant->firstoldonright > deltaoptimal->firstoldonright)
    {
      if (*rightinterval == NULL)
      {
        *rightinterval = distant;
      }
    }
    else if (!distant->newitemonleft && deltaoptimal->newitemonleft)
    {
         
                                                                       
                                                                       
                         
         
      Assert(distant->firstoldonright == state->newitemoff);
      if (*leftinterval == NULL)
      {
        *leftinterval = distant;
      }
    }
    else if (distant->newitemonleft && !deltaoptimal->newitemonleft)
    {
         
                                                                        
                                                                       
                         
         
      Assert(distant->firstoldonright == state->newitemoff);
      if (*rightinterval == NULL)
      {
        *rightinterval = distant;
      }
    }
    else
    {
                                                                      
      Assert(distant == deltaoptimal);
      if (*leftinterval == NULL)
      {
        *leftinterval = distant;
      }
      if (*rightinterval == NULL)
      {
        *rightinterval = distant;
      }
    }

    if (*leftinterval && *rightinterval)
    {
      return;
    }
  }

  Assert(false);
}

   
                                                                  
   
                                                                               
                                                                              
                                                                              
                                                                      
   
                                                                          
                                                                               
                                              
   
static inline int
_bt_split_penalty(FindSplitData *state, SplitPoint *split)
{
  IndexTuple lastleftuple;
  IndexTuple firstrighttuple;

  if (!state->is_leaf)
  {
    ItemId itemid;

    if (!split->newitemonleft && split->firstoldonright == state->newitemoff)
    {
      return state->newitemsz;
    }

    itemid = PageGetItemId(state->page, split->firstoldonright);

    return MAXALIGN(ItemIdGetLength(itemid)) + sizeof(ItemIdData);
  }

  lastleftuple = _bt_split_lastleft(state, split);
  firstrighttuple = _bt_split_firstright(state, split);

  Assert(lastleftuple != firstrighttuple);
  return _bt_keep_natts_fast(state->rel, lastleftuple, firstrighttuple);
}

   
                                                                      
   
static inline IndexTuple
_bt_split_lastleft(FindSplitData *state, SplitPoint *split)
{
  ItemId itemid;

  if (split->newitemonleft && split->firstoldonright == state->newitemoff)
  {
    return state->newitem;
  }

  itemid = PageGetItemId(state->page, OffsetNumberPrev(split->firstoldonright));
  return (IndexTuple)PageGetItem(state->page, itemid);
}

   
                                                                        
   
static inline IndexTuple
_bt_split_firstright(FindSplitData *state, SplitPoint *split)
{
  ItemId itemid;

  if (!split->newitemonleft && split->firstoldonright == state->newitemoff)
  {
    return state->newitem;
  }

  itemid = PageGetItemId(state->page, split->firstoldonright);
  return (IndexTuple)PageGetItem(state->page, itemid);
}
