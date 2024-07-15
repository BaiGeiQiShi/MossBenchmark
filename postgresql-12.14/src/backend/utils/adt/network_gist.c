                                                                            
   
                  
                                     
   
                                                                        
                                                              
                                                                           
                                                                            
                                   
                                                         
                                                                           
                            
                                                                           
                                                                        
                                         
                                                                         
                                                                         
                                                              
   
                                                                             
                                                                             
                                                                         
                                                                         
                                                                         
                                                                      
   
                                                                             
                                                                             
                                                                             
                                                                         
                                                                            
                                                                         
                                                                            
                                                           
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <sys/socket.h>

#include "access/gist.h"
#include "access/stratnum.h"
#include "utils/builtins.h"
#include "utils/inet.h"

   
                                                               
   
#define INETSTRAT_OVERLAPS RTOverlapStrategyNumber
#define INETSTRAT_EQ RTEqualStrategyNumber
#define INETSTRAT_NE RTNotEqualStrategyNumber
#define INETSTRAT_LT RTLessStrategyNumber
#define INETSTRAT_LE RTLessEqualStrategyNumber
#define INETSTRAT_GT RTGreaterStrategyNumber
#define INETSTRAT_GE RTGreaterEqualStrategyNumber
#define INETSTRAT_SUB RTSubStrategyNumber
#define INETSTRAT_SUBEQ RTSubEqualStrategyNumber
#define INETSTRAT_SUP RTSuperStrategyNumber
#define INETSTRAT_SUPEQ RTSuperEqualStrategyNumber

   
                                                                           
                                                                               
                                                                         
                                                                               
                                                              
   
typedef struct GistInetKey
{
  uint8 va_header;                                                       
  unsigned char family;                                                 
  unsigned char minbits;                                           
  unsigned char commonbits;                                                
  unsigned char ipaddr[16];                                       
} GistInetKey;

#define DatumGetInetKeyP(X) ((GistInetKey *)DatumGetPointer(X))
#define InetKeyPGetDatum(X) PointerGetDatum(X)

   
                                                                       
                                                                              
                                               
   
#define gk_ip_family(gkptr) ((gkptr)->family)
#define gk_ip_minbits(gkptr) ((gkptr)->minbits)
#define gk_ip_commonbits(gkptr) ((gkptr)->commonbits)
#define gk_ip_addr(gkptr) ((gkptr)->ipaddr)
#define ip_family_maxbits(fam) ((fam) == PGSQL_AF_INET6 ? 128 : 32)

                                                       
#define gk_ip_addrsize(gkptr) (gk_ip_family(gkptr) == PGSQL_AF_INET6 ? 16 : 4)
#define gk_ip_maxbits(gkptr) ip_family_maxbits(gk_ip_family(gkptr))
#define SET_GK_VARSIZE(dst) SET_VARSIZE_SHORT(dst, offsetof(GistInetKey, ipaddr) + gk_ip_addrsize(dst))

   
                                    
   
Datum
inet_gist_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *ent = (GISTENTRY *)PG_GETARG_POINTER(0);
  inet *query = PG_GETARG_INET_PP(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                        
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  GistInetKey *key = DatumGetInetKeyP(ent->key);
  int minbits, order;

                                                        
  *recheck = false;

     
                                 
     
                                                                           
                                                             
     
  if (gk_ip_family(key) == 0)
  {
    Assert(!GIST_LEAF(ent));
    PG_RETURN_BOOL(true);
  }

     
                                 
     
                                                          
     
  if (gk_ip_family(key) != ip_family(query))
  {
    switch (strategy)
    {
    case INETSTRAT_LT:
    case INETSTRAT_LE:
      if (gk_ip_family(key) < ip_family(query))
      {
        PG_RETURN_BOOL(true);
      }
      break;

    case INETSTRAT_GE:
    case INETSTRAT_GT:
      if (gk_ip_family(key) > ip_family(query))
      {
        PG_RETURN_BOOL(true);
      }
      break;

    case INETSTRAT_NE:
      PG_RETURN_BOOL(true);
    }
                                                               
    PG_RETURN_BOOL(false);
  }

     
                                
     
                                                                           
                                                                          
                                                                           
                
     
  switch (strategy)
  {
  case INETSTRAT_SUB:
    if (GIST_LEAF(ent) && gk_ip_minbits(key) <= ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_SUBEQ:
    if (GIST_LEAF(ent) && gk_ip_minbits(key) < ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_SUPEQ:
  case INETSTRAT_EQ:
    if (gk_ip_minbits(key) > ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_SUP:
    if (gk_ip_minbits(key) >= ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;
  }

     
                                  
     
                                                                       
                                                                             
                                                                           
                                                                             
                                                               
     
                                                                       
                                  
     
  minbits = Min(gk_ip_commonbits(key), gk_ip_minbits(key));
  minbits = Min(minbits, ip_bits(query));

  order = bitncmp(gk_ip_addr(key), ip_addr(query), minbits);

  switch (strategy)
  {
  case INETSTRAT_SUB:
  case INETSTRAT_SUBEQ:
  case INETSTRAT_OVERLAPS:
  case INETSTRAT_SUPEQ:
  case INETSTRAT_SUP:
    PG_RETURN_BOOL(order == 0);

  case INETSTRAT_LT:
  case INETSTRAT_LE:
    if (order > 0)
    {
      PG_RETURN_BOOL(false);
    }
    if (order < 0 || !GIST_LEAF(ent))
    {
      PG_RETURN_BOOL(true);
    }
    break;

  case INETSTRAT_EQ:
    if (order != 0)
    {
      PG_RETURN_BOOL(false);
    }
    if (!GIST_LEAF(ent))
    {
      PG_RETURN_BOOL(true);
    }
    break;

  case INETSTRAT_GE:
  case INETSTRAT_GT:
    if (order < 0)
    {
      PG_RETURN_BOOL(false);
    }
    if (order > 0 || !GIST_LEAF(ent))
    {
      PG_RETURN_BOOL(true);
    }
    break;

  case INETSTRAT_NE:
    if (order != 0 || !GIST_LEAF(ent))
    {
      PG_RETURN_BOOL(true);
    }
    break;
  }

     
                                                                           
                                                                            
                                                                             
                                                           
     
  Assert(GIST_LEAF(ent));

     
                                
     
                                             
     
  switch (strategy)
  {
  case INETSTRAT_LT:
  case INETSTRAT_LE:
    if (gk_ip_minbits(key) < ip_bits(query))
    {
      PG_RETURN_BOOL(true);
    }
    if (gk_ip_minbits(key) > ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_EQ:
    if (gk_ip_minbits(key) != ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_GE:
  case INETSTRAT_GT:
    if (gk_ip_minbits(key) > ip_bits(query))
    {
      PG_RETURN_BOOL(true);
    }
    if (gk_ip_minbits(key) < ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_NE:
    if (gk_ip_minbits(key) != ip_bits(query))
    {
      PG_RETURN_BOOL(true);
    }
    break;
  }

     
                            
     
                                                                     
     
  order = bitncmp(gk_ip_addr(key), ip_addr(query), gk_ip_maxbits(key));

  switch (strategy)
  {
  case INETSTRAT_LT:
    PG_RETURN_BOOL(order < 0);

  case INETSTRAT_LE:
    PG_RETURN_BOOL(order <= 0);

  case INETSTRAT_EQ:
    PG_RETURN_BOOL(order == 0);

  case INETSTRAT_GE:
    PG_RETURN_BOOL(order >= 0);

  case INETSTRAT_GT:
    PG_RETURN_BOOL(order > 0);

  case INETSTRAT_NE:
    PG_RETURN_BOOL(order != 0);
  }

  elog(ERROR, "unknown strategy for inet GiST");
  PG_RETURN_BOOL(false);                          
}

   
                                                           
   
                                                                       
                                        
                                                   
                                                   
                                      
                                                                        
   
                                                                      
                   
   
static void
calc_inet_union_params(GISTENTRY *ent, int m, int n, int *minfamily_p, int *maxfamily_p, int *minbits_p, int *commonbits_p)
{
  int minfamily, maxfamily, minbits, commonbits;
  unsigned char *addr;
  GistInetKey *tmp;
  int i;

                                 
  Assert(m <= n);

                                                 
  tmp = DatumGetInetKeyP(ent[m].key);
  minfamily = maxfamily = gk_ip_family(tmp);
  minbits = gk_ip_minbits(tmp);
  commonbits = gk_ip_commonbits(tmp);
  addr = gk_ip_addr(tmp);

                            
  for (i = m + 1; i <= n; i++)
  {
    tmp = DatumGetInetKeyP(ent[i].key);

                                           
    if (minfamily > gk_ip_family(tmp))
    {
      minfamily = gk_ip_family(tmp);
    }
    if (maxfamily < gk_ip_family(tmp))
    {
      maxfamily = gk_ip_family(tmp);
    }

                              
    if (minbits > gk_ip_minbits(tmp))
    {
      minbits = gk_ip_minbits(tmp);
    }

                                               
    if (commonbits > gk_ip_commonbits(tmp))
    {
      commonbits = gk_ip_commonbits(tmp);
    }
    if (commonbits > 0)
    {
      commonbits = bitncommon(addr, gk_ip_addr(tmp), commonbits);
    }
  }

                                                                 
  if (minfamily != maxfamily)
  {
    minbits = commonbits = 0;
  }

  *minfamily_p = minfamily;
  *maxfamily_p = maxfamily;
  *minbits_p = minbits;
  *commonbits_p = commonbits;
}

   
                                                                       
                                          
   
static void
calc_inet_union_params_indexed(GISTENTRY *ent, OffsetNumber *offsets, int noffsets, int *minfamily_p, int *maxfamily_p, int *minbits_p, int *commonbits_p)
{
  int minfamily, maxfamily, minbits, commonbits;
  unsigned char *addr;
  GistInetKey *tmp;
  int i;

                                 
  Assert(noffsets > 0);

                                                 
  tmp = DatumGetInetKeyP(ent[offsets[0]].key);
  minfamily = maxfamily = gk_ip_family(tmp);
  minbits = gk_ip_minbits(tmp);
  commonbits = gk_ip_commonbits(tmp);
  addr = gk_ip_addr(tmp);

                            
  for (i = 1; i < noffsets; i++)
  {
    tmp = DatumGetInetKeyP(ent[offsets[i]].key);

                                           
    if (minfamily > gk_ip_family(tmp))
    {
      minfamily = gk_ip_family(tmp);
    }
    if (maxfamily < gk_ip_family(tmp))
    {
      maxfamily = gk_ip_family(tmp);
    }

                              
    if (minbits > gk_ip_minbits(tmp))
    {
      minbits = gk_ip_minbits(tmp);
    }

                                               
    if (commonbits > gk_ip_commonbits(tmp))
    {
      commonbits = gk_ip_commonbits(tmp);
    }
    if (commonbits > 0)
    {
      commonbits = bitncommon(addr, gk_ip_addr(tmp), commonbits);
    }
  }

                                                                 
  if (minfamily != maxfamily)
  {
    minbits = commonbits = 0;
  }

  *minfamily_p = minfamily;
  *maxfamily_p = maxfamily;
  *minbits_p = minbits;
  *commonbits_p = commonbits;
}

   
                                                       
   
                                                                             
                                                                             
                                                          
   
static GistInetKey *
build_inet_union_key(int family, int minbits, int commonbits, unsigned char *addr)
{
  GistInetKey *result;

                                             
  result = (GistInetKey *)palloc0(sizeof(GistInetKey));

  gk_ip_family(result) = family;
  gk_ip_minbits(result) = minbits;
  gk_ip_commonbits(result) = commonbits;

                                               
  if (commonbits > 0)
  {
    memcpy(gk_ip_addr(result), addr, (commonbits + 7) / 8);
  }

                                                         
  if (commonbits % 8 != 0)
  {
    gk_ip_addr(result)[commonbits / 8] &= ~(0xFF >> (commonbits % 8));
  }

                                     
  SET_GK_VARSIZE(result);

  return result;
}

   
                           
   
                                                                 
   
Datum
inet_gist_union(PG_FUNCTION_ARGS)
{
  GistEntryVector *entryvec = (GistEntryVector *)PG_GETARG_POINTER(0);
  GISTENTRY *ent = entryvec->vector;
  int minfamily, maxfamily, minbits, commonbits;
  unsigned char *addr;
  GistInetKey *tmp, *result;

                                          
  calc_inet_union_params(ent, 0, entryvec->n - 1, &minfamily, &maxfamily, &minbits, &commonbits);

                                                         
  if (minfamily != maxfamily)
  {
    minfamily = 0;
  }

                                               
  tmp = DatumGetInetKeyP(ent[0].key);
  addr = gk_ip_addr(tmp);

                                  
  result = build_inet_union_key(minfamily, minbits, commonbits, addr);

  PG_RETURN_POINTER(result);
}

   
                              
   
                                         
   
Datum
inet_gist_compress(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *retval;

  if (entry->leafkey)
  {
    retval = palloc(sizeof(GISTENTRY));
    if (DatumGetPointer(entry->key) != NULL)
    {
      inet *in = DatumGetInetPP(entry->key);
      GistInetKey *r;

      r = (GistInetKey *)palloc0(sizeof(GistInetKey));

      gk_ip_family(r) = ip_family(in);
      gk_ip_minbits(r) = ip_bits(in);
      gk_ip_commonbits(r) = gk_ip_maxbits(r);
      memcpy(gk_ip_addr(r), ip_addr(in), gk_ip_addrsize(r));
      SET_GK_VARSIZE(r);

      gistentryinit(*retval, PointerGetDatum(r), entry->rel, entry->page, entry->offset, false);
    }
    else
    {
      gistentryinit(*retval, (Datum)0, entry->rel, entry->page, entry->offset, false);
    }
  }
  else
  {
    retval = entry;
  }
  PG_RETURN_POINTER(retval);
}

   
                                                                     
                                                               
   

   
                           
   
                                                           
   
Datum
inet_gist_fetch(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GistInetKey *key = DatumGetInetKeyP(entry->key);
  GISTENTRY *retval;
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

  ip_family(dst) = gk_ip_family(key);
  ip_bits(dst) = gk_ip_minbits(key);
  memcpy(ip_addr(dst), gk_ip_addr(key), ip_addrsize(dst));
  SET_INET_VARSIZE(dst);

  retval = palloc(sizeof(GISTENTRY));
  gistentryinit(*retval, InetPGetDatum(dst), entry->rel, entry->page, entry->offset, false);

  PG_RETURN_POINTER(retval);
}

   
                                        
   
                                                                         
                                                                  
                                                                  
                                      
   
Datum
inet_gist_penalty(PG_FUNCTION_ARGS)
{
  GISTENTRY *origent = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *newent = (GISTENTRY *)PG_GETARG_POINTER(1);
  float *penalty = (float *)PG_GETARG_POINTER(2);
  GistInetKey *orig = DatumGetInetKeyP(origent->key), *new = DatumGetInetKeyP(newent->key);
  int commonbits;

  if (gk_ip_family(orig) == gk_ip_family(new))
  {
    if (gk_ip_minbits(orig) <= gk_ip_minbits(new))
    {
      commonbits = bitncommon(gk_ip_addr(orig), gk_ip_addr(new), Min(gk_ip_commonbits(orig), gk_ip_commonbits(new)));
      if (commonbits > 0)
      {
        *penalty = 1.0f / commonbits;
      }
      else
      {
        *penalty = 2;
      }
    }
    else
    {
      *penalty = 3;
    }
  }
  else
  {
    *penalty = 4;
  }

  PG_RETURN_POINTER(penalty);
}

   
                             
   
                                                                           
                                                          
   
                                                                             
                                                                              
                                                                              
                                                                           
          
   
Datum
inet_gist_picksplit(PG_FUNCTION_ARGS)
{
  GistEntryVector *entryvec = (GistEntryVector *)PG_GETARG_POINTER(0);
  GIST_SPLITVEC *splitvec = (GIST_SPLITVEC *)PG_GETARG_POINTER(1);
  GISTENTRY *ent = entryvec->vector;
  int minfamily, maxfamily, minbits, commonbits;
  unsigned char *addr;
  GistInetKey *tmp, *left_union, *right_union;
  int maxoff, nbytes;
  OffsetNumber i, *left, *right;

  maxoff = entryvec->n - 1;
  nbytes = (maxoff + 1) * sizeof(OffsetNumber);

  left = (OffsetNumber *)palloc(nbytes);
  right = (OffsetNumber *)palloc(nbytes);

  splitvec->spl_left = left;
  splitvec->spl_right = right;

  splitvec->spl_nleft = 0;
  splitvec->spl_nright = 0;

                                                            
  calc_inet_union_params(ent, FirstOffsetNumber, maxoff, &minfamily, &maxfamily, &minbits, &commonbits);

  if (minfamily != maxfamily)
  {
                                                
    for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
    {
         
                                                                        
                                                                        
                                                                   
         
      tmp = DatumGetInetKeyP(ent[i].key);
      if (gk_ip_family(tmp) != maxfamily)
      {
        left[splitvec->spl_nleft++] = i;
      }
      else
      {
        right[splitvec->spl_nright++] = i;
      }
    }
  }
  else
  {
       
                                                                      
                                                                           
                                                                       
       
    int maxbits = ip_family_maxbits(minfamily);

    while (commonbits < maxbits)
    {
                                                       
      int bitbyte = commonbits / 8;
      int bitmask = 0x80 >> (commonbits % 8);

      splitvec->spl_nleft = splitvec->spl_nright = 0;

      for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
      {
        tmp = DatumGetInetKeyP(ent[i].key);
        addr = gk_ip_addr(tmp);
        if ((addr[bitbyte] & bitmask) == 0)
        {
          left[splitvec->spl_nleft++] = i;
        }
        else
        {
          right[splitvec->spl_nright++] = i;
        }
      }

      if (splitvec->spl_nleft > 0 && splitvec->spl_nright > 0)
      {
        break;              
      }
      commonbits++;
    }

    if (commonbits >= maxbits)
    {
                                        
      splitvec->spl_nleft = splitvec->spl_nright = 0;

      for (i = FirstOffsetNumber; i <= maxoff / 2; i = OffsetNumberNext(i))
      {
        left[splitvec->spl_nleft++] = i;
      }
      for (; i <= maxoff; i = OffsetNumberNext(i))
      {
        right[splitvec->spl_nright++] = i;
      }
    }
  }

     
                                                                           
                                                                            
                                                                      
               
     
  calc_inet_union_params_indexed(ent, left, splitvec->spl_nleft, &minfamily, &maxfamily, &minbits, &commonbits);
  if (minfamily != maxfamily)
  {
    minfamily = 0;
  }
  tmp = DatumGetInetKeyP(ent[left[0]].key);
  addr = gk_ip_addr(tmp);
  left_union = build_inet_union_key(minfamily, minbits, commonbits, addr);
  splitvec->spl_ldatum = PointerGetDatum(left_union);

  calc_inet_union_params_indexed(ent, right, splitvec->spl_nright, &minfamily, &maxfamily, &minbits, &commonbits);
  if (minfamily != maxfamily)
  {
    minfamily = 0;
  }
  tmp = DatumGetInetKeyP(ent[right[0]].key);
  addr = gk_ip_addr(tmp);
  right_union = build_inet_union_key(minfamily, minbits, commonbits, addr);
  splitvec->spl_rdatum = PointerGetDatum(right_union);

  PG_RETURN_POINTER(splitvec);
}

   
                              
   
Datum
inet_gist_same(PG_FUNCTION_ARGS)
{
  GistInetKey *left = DatumGetInetKeyP(PG_GETARG_DATUM(0));
  GistInetKey *right = DatumGetInetKeyP(PG_GETARG_DATUM(1));
  bool *result = (bool *)PG_GETARG_POINTER(2);

  *result = (gk_ip_family(left) == gk_ip_family(right) && gk_ip_minbits(left) == gk_ip_minbits(right) && gk_ip_commonbits(left) == gk_ip_commonbits(right) && memcmp(gk_ip_addr(left), gk_ip_addr(right), gk_ip_addrsize(left)) == 0);

  PG_RETURN_POINTER(result);
}
