                                                                            
   
               
                                           
   
                                                                        
                                                                        
                                                                       
                                                                       
                                                                   
                                                      
   
   
                                                                
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include "nodes/bitmapset.h"
#include "nodes/pg_list.h"
#include "port/pg_bitutils.h"
#include "utils/hashutils.h"

#define WORDNUM(x) ((x) / BITS_PER_BITMAPWORD)
#define BITNUM(x) ((x) % BITS_PER_BITMAPWORD)

#define BITMAPSET_SIZE(nwords) (offsetof(Bitmapset, words) + (nwords) * sizeof(bitmapword))

             
                                                                       
                                                                     
                                                                          
                       
                  
                                                                            
                                                              
                  
                                                                           
                  
                                                 
                  
                                                                          
                
             
   
#define RIGHTMOST_ONE(x) ((signedbitmapword)(x) & -((signedbitmapword)(x)))

#define HAS_MULTIPLE_ONES(x) ((bitmapword)RIGHTMOST_ONE(x) != (x))

                                                                     
#if BITS_PER_BITMAPWORD == 32
#define bmw_leftmost_one_pos(w) pg_leftmost_one_pos32(w)
#define bmw_rightmost_one_pos(w) pg_rightmost_one_pos32(w)
#define bmw_popcount(w) pg_popcount32(w)
#elif BITS_PER_BITMAPWORD == 64
#define bmw_leftmost_one_pos(w) pg_leftmost_one_pos64(w)
#define bmw_rightmost_one_pos(w) pg_rightmost_one_pos64(w)
#define bmw_popcount(w) pg_popcount64(w)
#else
#error "invalid BITS_PER_BITMAPWORD"
#endif

   
                                                  
   
Bitmapset *
bms_copy(const Bitmapset *a)
{
  Bitmapset *result;
  size_t size;

  if (a == NULL)
  {
    return NULL;
  }
  size = BITMAPSET_SIZE(a->nwords);
  result = (Bitmapset *)palloc(size);
  memcpy(result, a, size);
  return result;
}

   
                                         
   
                                                                             
                                                                   
   
bool
bms_equal(const Bitmapset *a, const Bitmapset *b)
{
  const Bitmapset *shorter;
  const Bitmapset *longer;
  int shortlen;
  int longlen;
  int i;

                                               
  if (a == NULL)
  {
    if (b == NULL)
    {
      return true;
    }
    return bms_is_empty(b);
  }
  else if (b == NULL)
  {
    return bms_is_empty(a);
  }
                                         
  if (a->nwords <= b->nwords)
  {
    shorter = a;
    longer = b;
  }
  else
  {
    shorter = b;
    longer = a;
  }
                   
  shortlen = shorter->nwords;
  for (i = 0; i < shortlen; i++)
  {
    if (shorter->words[i] != longer->words[i])
    {
      return false;
    }
  }
  longlen = longer->nwords;
  for (; i < longlen; i++)
  {
    if (longer->words[i] != 0)
    {
      return false;
    }
  }
  return true;
}

   
                                                       
   
                                                                              
                                                                            
                                                                             
                                                    
   
int
bms_compare(const Bitmapset *a, const Bitmapset *b)
{
  int shortlen;
  int i;

                                               
  if (a == NULL)
  {
    return bms_is_empty(b) ? 0 : -1;
  }
  else if (b == NULL)
  {
    return bms_is_empty(a) ? 0 : +1;
  }
                                                             
  shortlen = Min(a->nwords, b->nwords);
  for (i = shortlen; i < a->nwords; i++)
  {
    if (a->words[i] != 0)
    {
      return +1;
    }
  }
  for (i = shortlen; i < b->nwords; i++)
  {
    if (b->words[i] != 0)
    {
      return -1;
    }
  }
                               
  i = shortlen;
  while (--i >= 0)
  {
    bitmapword aw = a->words[i];
    bitmapword bw = b->words[i];

    if (aw != bw)
    {
      return (aw > bw) ? +1 : -1;
    }
  }
  return 0;
}

   
                                                                     
   
Bitmapset *
bms_make_singleton(int x)
{
  Bitmapset *result;
  int wordnum, bitnum;

  if (x < 0)
  {
    elog(ERROR, "negative bitmapset member not allowed");
  }
  wordnum = WORDNUM(x);
  bitnum = BITNUM(x);
  result = (Bitmapset *)palloc0(BITMAPSET_SIZE(wordnum + 1));
  result->nwords = wordnum + 1;
  result->words[wordnum] = ((bitmapword)1 << bitnum);
  return result;
}

   
                               
   
                                                
   
void
bms_free(Bitmapset *a)
{
  if (a)
  {
    pfree(a);
  }
}

   
                                                        
                                  
   

   
                         
   
Bitmapset *
bms_union(const Bitmapset *a, const Bitmapset *b)
{
  Bitmapset *result;
  const Bitmapset *other;
  int otherlen;
  int i;

                                               
  if (a == NULL)
  {
    return bms_copy(b);
  }
  if (b == NULL)
  {
    return bms_copy(a);
  }
                                                              
  if (a->nwords <= b->nwords)
  {
    result = bms_copy(b);
    other = a;
  }
  else
  {
    result = bms_copy(a);
    other = b;
  }
                                                   
  otherlen = other->nwords;
  for (i = 0; i < otherlen; i++)
  {
    result->words[i] |= other->words[i];
  }
  return result;
}

   
                                    
   
Bitmapset *
bms_intersect(const Bitmapset *a, const Bitmapset *b)
{
  Bitmapset *result;
  const Bitmapset *other;
  int resultlen;
  int i;

                                               
  if (a == NULL || b == NULL)
  {
    return NULL;
  }
                                                               
  if (a->nwords <= b->nwords)
  {
    result = bms_copy(a);
    other = b;
  }
  else
  {
    result = bms_copy(b);
    other = a;
  }
                                                      
  resultlen = result->nwords;
  for (i = 0; i < resultlen; i++)
  {
    result->words[i] &= other->words[i];
  }
  return result;
}

   
                                                                
   
Bitmapset *
bms_difference(const Bitmapset *a, const Bitmapset *b)
{
  Bitmapset *result;
  int shortlen;
  int i;

                                               
  if (a == NULL)
  {
    return NULL;
  }
  if (b == NULL)
  {
    return bms_copy(a);
  }
                           
  result = bms_copy(a);
                                       
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    result->words[i] &= ~b->words[i];
  }
  return result;
}

   
                                       
   
bool
bms_is_subset(const Bitmapset *a, const Bitmapset *b)
{
  int shortlen;
  int longlen;
  int i;

                                               
  if (a == NULL)
  {
    return true;                                        
  }
  if (b == NULL)
  {
    return bms_is_empty(a);
  }
                          
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    if ((a->words[i] & ~b->words[i]) != 0)
    {
      return false;
    }
  }
                         
  if (a->nwords > b->nwords)
  {
    longlen = a->nwords;
    for (; i < longlen; i++)
    {
      if (a->words[i] != 0)
      {
        return false;
      }
    }
  }
  return true;
}

   
                                                                          
   
                                                                         
   
BMS_Comparison
bms_subset_compare(const Bitmapset *a, const Bitmapset *b)
{
  BMS_Comparison result;
  int shortlen;
  int longlen;
  int i;

                                               
  if (a == NULL)
  {
    if (b == NULL)
    {
      return BMS_EQUAL;
    }
    return bms_is_empty(b) ? BMS_EQUAL : BMS_SUBSET1;
  }
  if (b == NULL)
  {
    return bms_is_empty(a) ? BMS_EQUAL : BMS_SUBSET2;
  }
                          
  result = BMS_EQUAL;                    
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    bitmapword aword = a->words[i];
    bitmapword bword = b->words[i];

    if ((aword & ~bword) != 0)
    {
                                  
      if (result == BMS_SUBSET1)
      {
        return BMS_DIFFERENT;
      }
      result = BMS_SUBSET2;
    }
    if ((bword & ~aword) != 0)
    {
                                  
      if (result == BMS_SUBSET2)
      {
        return BMS_DIFFERENT;
      }
      result = BMS_SUBSET1;
    }
  }
                         
  if (a->nwords > b->nwords)
  {
    longlen = a->nwords;
    for (; i < longlen; i++)
    {
      if (a->words[i] != 0)
      {
                                    
        if (result == BMS_SUBSET1)
        {
          return BMS_DIFFERENT;
        }
        result = BMS_SUBSET2;
      }
    }
  }
  else if (a->nwords < b->nwords)
  {
    longlen = b->nwords;
    for (; i < longlen; i++)
    {
      if (b->words[i] != 0)
      {
                                    
        if (result == BMS_SUBSET2)
        {
          return BMS_DIFFERENT;
        }
        result = BMS_SUBSET1;
      }
    }
  }
  return result;
}

   
                                       
   
bool
bms_is_member(int x, const Bitmapset *a)
{
  int wordnum, bitnum;

                                                 
  if (x < 0)
  {
    elog(ERROR, "negative bitmapset member not allowed");
  }
  if (a == NULL)
  {
    return false;
  }
  wordnum = WORDNUM(x);
  bitnum = BITNUM(x);
  if (wordnum >= a->nwords)
  {
    return false;
  }
  if ((a->words[wordnum] & ((bitmapword)1 << bitnum)) != 0)
  {
    return true;
  }
  return false;
}

   
                    
                                                      
   
                                        
   
int
bms_member_index(Bitmapset *a, int x)
{
  int i;
  int bitnum;
  int wordnum;
  int result = 0;
  bitmapword mask;

                                               
  if (!bms_is_member(x, a))
  {
    return -1;
  }

  wordnum = WORDNUM(x);
  bitnum = BITNUM(x);

                                     
  for (i = 0; i < wordnum; i++)
  {
    bitmapword w = a->words[i];

                                                  
    if (w != 0)
    {
      result += bmw_popcount(w);
    }
  }

     
                                                                           
                                                                      
                                                                       
                               
     
  mask = ((bitmapword)1 << bitnum) - 1;
  result += bmw_popcount(a->words[wordnum] & mask);

  return result;
}

   
                                                                     
   
bool
bms_overlap(const Bitmapset *a, const Bitmapset *b)
{
  int shortlen;
  int i;

                                               
  if (a == NULL || b == NULL)
  {
    return false;
  }
                             
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    if ((a->words[i] & b->words[i]) != 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                          
   
bool
bms_overlap_list(const Bitmapset *a, const List *b)
{
  ListCell *lc;
  int wordnum, bitnum;

  if (a == NULL || b == NIL)
  {
    return false;
  }

  foreach (lc, b)
  {
    int x = lfirst_int(lc);

    if (x < 0)
    {
      elog(ERROR, "negative bitmapset member not allowed");
    }
    wordnum = WORDNUM(x);
    bitnum = BITNUM(x);
    if (wordnum < a->nwords)
    {
      if ((a->words[wordnum] & ((bitmapword)1 << bitnum)) != 0)
      {
        return true;
      }
    }
  }

  return false;
}

   
                                                                 
   
bool
bms_nonempty_difference(const Bitmapset *a, const Bitmapset *b)
{
  int shortlen;
  int i;

                                               
  if (a == NULL)
  {
    return false;
  }
  if (b == NULL)
  {
    return !bms_is_empty(a);
  }
                             
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    if ((a->words[i] & ~b->words[i]) != 0)
    {
      return true;
    }
  }
                              
  for (; i < a->nwords; i++)
  {
    if (a->words[i] != 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                                
   
                                 
   
int
bms_singleton_member(const Bitmapset *a)
{
  int result = -1;
  int nwords;
  int wordnum;

  if (a == NULL)
  {
    elog(ERROR, "bitmapset is empty");
  }
  nwords = a->nwords;
  for (wordnum = 0; wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

    if (w != 0)
    {
      if (result >= 0 || HAS_MULTIPLE_ONES(w))
      {
        elog(ERROR, "bitmapset has multiple members");
      }
      result = wordnum * BITS_PER_BITMAPWORD;
      result += bmw_rightmost_one_pos(w);
    }
  }
  if (result < 0)
  {
    elog(ERROR, "bitmapset is empty");
  }
  return result;
}

   
                            
   
                                              
                                                                        
                                                   
   
                                                                             
                                                                            
                              
   
bool
bms_get_singleton_member(const Bitmapset *a, int *member)
{
  int result = -1;
  int nwords;
  int wordnum;

  if (a == NULL)
  {
    return false;
  }
  nwords = a->nwords;
  for (wordnum = 0; wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

    if (w != 0)
    {
      if (result >= 0 || HAS_MULTIPLE_ONES(w))
      {
        return false;
      }
      result = wordnum * BITS_PER_BITMAPWORD;
      result += bmw_rightmost_one_pos(w);
    }
  }
  if (result < 0)
  {
    return false;
  }
  *member = result;
  return true;
}

   
                                          
   
int
bms_num_members(const Bitmapset *a)
{
  int result = 0;
  int nwords;
  int wordnum;

  if (a == NULL)
  {
    return 0;
  }
  nwords = a->nwords;
  for (wordnum = 0; wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

                                                  
    if (w != 0)
    {
      result += bmw_popcount(w);
    }
  }
  return result;
}

   
                                                                    
   
                                                                     
   
BMS_Membership
bms_membership(const Bitmapset *a)
{
  BMS_Membership result = BMS_EMPTY_SET;
  int nwords;
  int wordnum;

  if (a == NULL)
  {
    return BMS_EMPTY_SET;
  }
  nwords = a->nwords;
  for (wordnum = 0; wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

    if (w != 0)
    {
      if (result != BMS_EMPTY_SET || HAS_MULTIPLE_ONES(w))
      {
        return BMS_MULTIPLE;
      }
      result = BMS_SINGLETON;
    }
  }
  return result;
}

   
                                  
   
                                              
   
bool
bms_is_empty(const Bitmapset *a)
{
  int nwords;
  int wordnum;

  if (a == NULL)
  {
    return true;
  }
  nwords = a->nwords;
  for (wordnum = 0; wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

    if (w != 0)
    {
      return false;
    }
  }
  return true;
}

   
                                                                     
                                                                      
   
                                               
   
                                  
   

   
                                                  
   
                                      
   
Bitmapset *
bms_add_member(Bitmapset *a, int x)
{
  int wordnum, bitnum;

  if (x < 0)
  {
    elog(ERROR, "negative bitmapset member not allowed");
  }
  if (a == NULL)
  {
    return bms_make_singleton(x);
  }
  wordnum = WORDNUM(x);
  bitnum = BITNUM(x);

                                    
  if (wordnum >= a->nwords)
  {
    int oldnwords = a->nwords;
    int i;

    a = (Bitmapset *)repalloc(a, BITMAPSET_SIZE(wordnum + 1));
    a->nwords = wordnum + 1;
                                       
    for (i = oldnwords; i < a->nwords; i++)
    {
      a->words[i] = 0;
    }
  }

  a->words[wordnum] |= ((bitmapword)1 << bitnum);
  return a;
}

   
                                                       
   
                                                  
   
                                   
   
Bitmapset *
bms_del_member(Bitmapset *a, int x)
{
  int wordnum, bitnum;

  if (x < 0)
  {
    elog(ERROR, "negative bitmapset member not allowed");
  }
  if (a == NULL)
  {
    return NULL;
  }
  wordnum = WORDNUM(x);
  bitnum = BITNUM(x);
  if (wordnum < a->nwords)
  {
    a->words[wordnum] &= ~((bitmapword)1 << bitnum);
  }
  return a;
}

   
                                                                
   
Bitmapset *
bms_add_members(Bitmapset *a, const Bitmapset *b)
{
  Bitmapset *result;
  const Bitmapset *other;
  int otherlen;
  int i;

                                               
  if (a == NULL)
  {
    return bms_copy(b);
  }
  if (b == NULL)
  {
    return a;
  }
                                                                        
  if (a->nwords < b->nwords)
  {
    result = bms_copy(b);
    other = a;
  }
  else
  {
    result = a;
    other = b;
  }
                                                   
  otherlen = other->nwords;
  for (i = 0; i < otherlen; i++)
  {
    result->words[i] |= other->words[i];
  }
  if (result != a)
  {
    pfree(a);
  }
  return result;
}

   
                 
                                                               
   
                                                                              
                                                                            
                                                  
   
Bitmapset *
bms_add_range(Bitmapset *a, int lower, int upper)
{
  int lwordnum, lbitnum, uwordnum, ushiftbits, wordnum;

                                                                     
  if (upper < lower)
  {
    return a;
  }

  if (lower < 0)
  {
    elog(ERROR, "negative bitmapset member not allowed");
  }
  uwordnum = WORDNUM(upper);

  if (a == NULL)
  {
    a = (Bitmapset *)palloc0(BITMAPSET_SIZE(uwordnum + 1));
    a->nwords = uwordnum + 1;
  }
  else if (uwordnum >= a->nwords)
  {
    int oldnwords = a->nwords;
    int i;

                                                            
    a = (Bitmapset *)repalloc(a, BITMAPSET_SIZE(uwordnum + 1));
    a->nwords = uwordnum + 1;
                                       
    for (i = oldnwords; i < a->nwords; i++)
    {
      a->words[i] = 0;
    }
  }

  wordnum = lwordnum = WORDNUM(lower);

  lbitnum = BITNUM(lower);
  ushiftbits = BITS_PER_BITMAPWORD - (BITNUM(upper) + 1);

     
                                                                            
                                          
     
  if (lwordnum == uwordnum)
  {
    a->words[lwordnum] |= ~(bitmapword)(((bitmapword)1 << lbitnum) - 1) & (~(bitmapword)0) >> ushiftbits;
  }
  else
  {
                                                 
    a->words[wordnum++] |= ~(bitmapword)(((bitmapword)1 << lbitnum) - 1);

                                                     
    while (wordnum < uwordnum)
    {
      a->words[wordnum++] = ~(bitmapword)0;
    }

                                                       
    a->words[uwordnum] |= (~(bitmapword)0) >> ushiftbits;
  }

  return a;
}

   
                                                                    
   
Bitmapset *
bms_int_members(Bitmapset *a, const Bitmapset *b)
{
  int shortlen;
  int i;

                                               
  if (a == NULL)
  {
    return NULL;
  }
  if (b == NULL)
  {
    pfree(a);
    return NULL;
  }
                                              
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    a->words[i] &= b->words[i];
  }
  for (; i < a->nwords; i++)
  {
    a->words[i] = 0;
  }
  return a;
}

   
                                                                     
   
Bitmapset *
bms_del_members(Bitmapset *a, const Bitmapset *b)
{
  int shortlen;
  int i;

                                               
  if (a == NULL)
  {
    return NULL;
  }
  if (b == NULL)
  {
    return a;
  }
                                                  
  shortlen = Min(a->nwords, b->nwords);
  for (i = 0; i < shortlen; i++)
  {
    a->words[i] &= ~b->words[i];
  }
  return a;
}

   
                                                             
   
Bitmapset *
bms_join(Bitmapset *a, Bitmapset *b)
{
  Bitmapset *result;
  Bitmapset *other;
  int otherlen;
  int i;

                                               
  if (a == NULL)
  {
    return b;
  }
  if (b == NULL)
  {
    return a;
  }
                                                                   
  if (a->nwords < b->nwords)
  {
    result = b;
    other = a;
  }
  else
  {
    result = a;
    other = b;
  }
                                                   
  otherlen = other->nwords;
  for (i = 0; i < otherlen; i++)
  {
    result->words[i] |= other->words[i];
  }
  if (other != result)                    
  {
    pfree(other);
  }
  return result;
}

   
                                                            
   
                                                                   
   
                                                                           
                          
   
                                                   
                        
   
                                                                      
                                                 
   
int
bms_first_member(Bitmapset *a)
{
  int nwords;
  int wordnum;

  if (a == NULL)
  {
    return -1;
  }
  nwords = a->nwords;
  for (wordnum = 0; wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

    if (w != 0)
    {
      int result;

      w = RIGHTMOST_ONE(w);
      a->words[wordnum] &= ~w;

      result = wordnum * BITS_PER_BITMAPWORD;
      result += bmw_rightmost_one_pos(w);
      return result;
    }
  }
  return -1;
}

   
                                               
   
                                                                           
                                                                         
   
                                                                           
                          
   
             
                                                     
                        
   
                                                                           
                                                                        
                                                                             
                                                                            
                               
   
int
bms_next_member(const Bitmapset *a, int prevbit)
{
  int nwords;
  int wordnum;
  bitmapword mask;

  if (a == NULL)
  {
    return -2;
  }
  nwords = a->nwords;
  prevbit++;
  mask = (~(bitmapword)0) << BITNUM(prevbit);
  for (wordnum = WORDNUM(prevbit); wordnum < nwords; wordnum++)
  {
    bitmapword w = a->words[wordnum];

                                    
    w &= mask;

    if (w != 0)
    {
      int result;

      result = wordnum * BITS_PER_BITMAPWORD;
      result += bmw_rightmost_one_pos(w);
      return result;
    }

                                                
    mask = (~(bitmapword)0);
  }
  return -2;
}

   
                                               
   
                                                                       
                                                                               
                                                
   
                                                                         
                                                                           
                             
   
                                                                             
                                    
   
             
                                                     
                        
   
                                                                           
                                                                        
                                                                             
                                                                            
                               
   

int
bms_prev_member(const Bitmapset *a, int prevbit)
{
  int wordnum;
  int ushiftbits;
  bitmapword mask;

     
                                                                         
                    
     
  if (a == NULL || prevbit == 0)
  {
    return -2;
  }

                                                                  
  if (prevbit == -1)
  {
    prevbit = a->nwords * BITS_PER_BITMAPWORD - 1;
  }
  else
  {
    prevbit--;
  }

  ushiftbits = BITS_PER_BITMAPWORD - (BITNUM(prevbit) + 1);
  mask = (~(bitmapword)0) >> ushiftbits;
  for (wordnum = WORDNUM(prevbit); wordnum >= 0; wordnum--)
  {
    bitmapword w = a->words[wordnum];

                                       
    w &= mask;

    if (w != 0)
    {
      int result;

      result = wordnum * BITS_PER_BITMAPWORD;
      result += bmw_leftmost_one_pos(w);
      return result;
    }

                                                
    mask = (~(bitmapword)0);
  }
  return -2;
}

   
                                                       
   
                                                                          
                                                                         
                                                                           
               
   
uint32
bms_hash_value(const Bitmapset *a)
{
  int lastword;

  if (a == NULL)
  {
    return 0;                               
  }
  for (lastword = a->nwords; --lastword >= 0;)
  {
    if (a->words[lastword] != 0)
    {
      break;
    }
  }
  if (lastword < 0)
  {
    return 0;                               
  }
  return DatumGetUInt32(hash_any((const unsigned char *)a->words, (lastword + 1) * sizeof(bitmapword)));
}
