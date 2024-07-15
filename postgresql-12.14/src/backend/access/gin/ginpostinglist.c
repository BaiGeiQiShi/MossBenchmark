                                                                            
   
                    
                                              
   
   
                                                                         
                                                                        
   
                  
                                             
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"

#ifdef USE_ASSERT_CHECKING
#define CHECK_ENCODING_ROUNDTRIP
#endif

   
                                                                           
                                                                          
                                                                         
                              
   
                                                                           
                                                                           
                                                                         
                                                                             
                                                                 
   
                                                                           
                                                                             
                                                                       
                                                                          
                                     
   
            
                     
                              
                                       
                                                
                                                         
                                                                  
   
                                   
                                  
                  
   
                                                   
   
                                                                             
                                                                             
   
                                                                             
                                                                             
                                                                              
                                                                                
                                                                               
                                                                              
                                                                            
                    
   
                                                                          
                                                                            
                                                                       
                                                                              
                                                   
   

   
                                                                               
                                                                             
                                                                            
                                                                               
                                                                       
   
#define MaxHeapTuplesPerPageBits 11

                                                                          
#define MaxBytesPerInteger 7

static inline uint64
itemptr_to_uint64(const ItemPointer iptr)
{
  uint64 val;

  Assert(ItemPointerIsValid(iptr));
  Assert(GinItemPointerGetOffsetNumber(iptr) < (1 << MaxHeapTuplesPerPageBits));

  val = GinItemPointerGetBlockNumber(iptr);
  val <<= MaxHeapTuplesPerPageBits;
  val |= GinItemPointerGetOffsetNumber(iptr);

  return val;
}

static inline void
uint64_to_itemptr(uint64 val, ItemPointer iptr)
{
  GinItemPointerSetOffsetNumber(iptr, val & ((1 << MaxHeapTuplesPerPageBits) - 1));
  val = val >> MaxHeapTuplesPerPageBits;
  GinItemPointerSetBlockNumber(iptr, val);

  Assert(ItemPointerIsValid(iptr));
}

   
                                                                        
   
static void
encode_varbyte(uint64 val, unsigned char **ptr)
{
  unsigned char *p = *ptr;

  while (val > 0x7F)
  {
    *(p++) = 0x80 | (val & 0x7F);
    val >>= 7;
  }
  *(p++) = (unsigned char)val;

  *ptr = p;
}

   
                                                                                
   
static uint64
decode_varbyte(unsigned char **ptr)
{
  uint64 val;
  unsigned char *p = *ptr;
  uint64 c;

                
  c = *(p++);
  val = c & 0x7F;
  if (c & 0x80)
  {
                  
    c = *(p++);
    val |= (c & 0x7F) << 7;
    if (c & 0x80)
    {
                    
      c = *(p++);
      val |= (c & 0x7F) << 14;
      if (c & 0x80)
      {
                      
        c = *(p++);
        val |= (c & 0x7F) << 21;
        if (c & 0x80)
        {
                        
          c = *(p++);
          val |= (c & 0x7F) << 28;
          if (c & 0x80)
          {
                          
            c = *(p++);
            val |= (c & 0x7F) << 35;
            if (c & 0x80)
            {
                                                              
              c = *(p++);
              val |= c << 42;
              Assert((c & 0x80) == 0);
            }
          }
        }
      }
    }
  }

  *ptr = p;

  return val;
}

   
                          
   
                                                                            
                                                                         
                                                                           
                                                            
   
                                                                               
                                     
   
GinPostingList *
ginCompressPostingList(const ItemPointer ipd, int nipd, int maxsize, int *nwritten)
{
  uint64 prev;
  int totalpacked = 0;
  int maxbytes;
  GinPostingList *result;
  unsigned char *ptr;
  unsigned char *endptr;

  maxsize = SHORTALIGN_DOWN(maxsize);

  result = palloc(maxsize);

  maxbytes = maxsize - offsetof(GinPostingList, bytes);
  Assert(maxbytes > 0);

                                    
  result->first = ipd[0];

  prev = itemptr_to_uint64(&result->first);

  ptr = result->bytes;
  endptr = result->bytes + maxbytes;
  for (totalpacked = 1; totalpacked < nipd; totalpacked++)
  {
    uint64 val = itemptr_to_uint64(&ipd[totalpacked]);
    uint64 delta = val - prev;

    Assert(val > prev);

    if (endptr - ptr >= MaxBytesPerInteger)
    {
      encode_varbyte(delta, &ptr);
    }
    else
    {
         
                                                                     
                                                        
         
      unsigned char buf[MaxBytesPerInteger];
      unsigned char *p = buf;

      encode_varbyte(delta, &p);
      if (p - buf > (endptr - ptr))
      {
        break;                     
      }

      memcpy(ptr, buf, p - buf);
      ptr += (p - buf);
    }
    prev = val;
  }
  result->nbytes = ptr - result->bytes;

     
                                                                          
          
     
  if (result->nbytes != SHORTALIGN(result->nbytes))
  {
    result->bytes[result->nbytes] = 0;
  }

  if (nwritten)
  {
    *nwritten = totalpacked;
  }

  Assert(SizeOfGinPostingList(result) <= maxsize);

     
                                                                        
     
#if defined(CHECK_ENCODING_ROUNDTRIP)
  {
    int ndecoded;
    ItemPointer tmp = ginPostingListDecode(result, &ndecoded);
    int i;

    Assert(ndecoded == totalpacked);
    for (i = 0; i < ndecoded; i++)
    {
      Assert(memcmp(&tmp[i], &ipd[i], sizeof(ItemPointerData)) == 0);
    }
    pfree(tmp);
  }
#endif

  return result;
}

   
                                                                    
                                                 
   
ItemPointer
ginPostingListDecode(GinPostingList *plist, int *ndecoded)
{
  return ginPostingListDecodeAllSegments(plist, SizeOfGinPostingList(plist), ndecoded);
}

   
                                                                         
                                                                             
                                                      
   
ItemPointer
ginPostingListDecodeAllSegments(GinPostingList *segment, int len, int *ndecoded_out)
{
  ItemPointer result;
  int nallocated;
  uint64 val;
  char *endseg = ((char *)segment) + len;
  int ndecoded;
  unsigned char *ptr;
  unsigned char *endptr;

     
                                         
     
  nallocated = segment->nbytes * 2 + 1;
  result = palloc(nallocated * sizeof(ItemPointerData));

  ndecoded = 0;
  while ((char *)segment < endseg)
  {
                                        
    if (ndecoded >= nallocated)
    {
      nallocated *= 2;
      result = repalloc(result, nallocated * sizeof(ItemPointerData));
    }

                             
    Assert(OffsetNumberIsValid(ItemPointerGetOffsetNumber(&segment->first)));
    Assert(ndecoded == 0 || ginCompareItemPointers(&segment->first, &result[ndecoded - 1]) > 0);
    result[ndecoded] = segment->first;
    ndecoded++;

    val = itemptr_to_uint64(&segment->first);
    ptr = segment->bytes;
    endptr = segment->bytes + segment->nbytes;
    while (ptr < endptr)
    {
                                          
      if (ndecoded >= nallocated)
      {
        nallocated *= 2;
        result = repalloc(result, nallocated * sizeof(ItemPointerData));
      }

      val += decode_varbyte(&ptr);

      uint64_to_itemptr(val, &result[ndecoded]);
      ndecoded++;
    }
    segment = GinNextPostingListSegment(segment);
  }

  if (ndecoded_out)
  {
    *ndecoded_out = ndecoded;
  }
  return result;
}

   
                                                                       
   
int
ginPostingListDecodeAllSegmentsToTbm(GinPostingList *ptr, int len, TIDBitmap *tbm)
{
  int ndecoded;
  ItemPointer items;

  items = ginPostingListDecodeAllSegments(ptr, len, &ndecoded);
  tbm_add_tuples(tbm, items, ndecoded, false);
  pfree(items);

  return ndecoded;
}

   
                                                                         
   
                                                                           
                                             
   
ItemPointer
ginMergeItemPointers(ItemPointerData *a, uint32 na, ItemPointerData *b, uint32 nb, int *nmerged)
{
  ItemPointerData *dst;

  dst = (ItemPointer)palloc((na + nb) * sizeof(ItemPointerData));

     
                                                                           
            
     
  if (na == 0 || nb == 0 || ginCompareItemPointers(&a[na - 1], &b[0]) < 0)
  {
    memcpy(dst, a, na * sizeof(ItemPointerData));
    memcpy(&dst[na], b, nb * sizeof(ItemPointerData));
    *nmerged = na + nb;
  }
  else if (ginCompareItemPointers(&b[nb - 1], &a[0]) < 0)
  {
    memcpy(dst, b, nb * sizeof(ItemPointerData));
    memcpy(&dst[nb], a, na * sizeof(ItemPointerData));
    *nmerged = na + nb;
  }
  else
  {
    ItemPointerData *dptr = dst;
    ItemPointerData *aptr = a;
    ItemPointerData *bptr = b;

    while (aptr - a < na && bptr - b < nb)
    {
      int cmp = ginCompareItemPointers(aptr, bptr);

      if (cmp > 0)
      {
        *dptr++ = *bptr++;
      }
      else if (cmp == 0)
      {
                                                       
        *dptr++ = *bptr++;
        aptr++;
      }
      else
      {
        *dptr++ = *aptr++;
      }
    }

    while (aptr - a < na)
    {
      *dptr++ = *aptr++;
    }

    while (bptr - b < nb)
    {
      *dptr++ = *bptr++;
    }

    *nmerged = dptr - dst;
  }

  return dst;
}
