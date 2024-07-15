                                                                            
                  
                                            
   
                                                                   
                                         
   
                                                                         
   
                  
                               
   
                                                                            
   
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/unicode_norm.h"
#include "common/unicode_norm_table.h"

#ifndef FRONTEND
#define ALLOC(size) palloc(size)
#define FREE(size) pfree(size)
#else
#define ALLOC(size) malloc(size)
#define FREE(size) free(size)
#endif

                                                       
#define SBASE 0xAC00             
#define LBASE 0x1100             
#define VBASE 0x1161             
#define TBASE 0x11A7             
#define LCOUNT 19
#define VCOUNT 21
#define TCOUNT 28
#define NCOUNT VCOUNT *TCOUNT
#define SCOUNT LCOUNT *NCOUNT

                                                                     
static int
conv_compare(const void *p1, const void *p2)
{
  uint32 v1, v2;

  v1 = *(const uint32 *)p1;
  v2 = ((const pg_unicode_decomposition *)p2)->codepoint;
  return (v1 > v2) ? 1 : ((v1 == v2) ? 0 : -1);
}

   
                                                                          
   
static pg_unicode_decomposition *
get_code_entry(pg_wchar code)
{
  return bsearch(&(code), UnicodeDecompMain, lengthof(UnicodeDecompMain), sizeof(pg_unicode_decomposition), conv_compare);
}

   
                                                                     
               
   
                                                                            
                                                   
   
static const pg_wchar *
get_code_decomposition(pg_unicode_decomposition *entry, int *dec_size)
{
  static pg_wchar x;

  if (DECOMPOSITION_IS_INLINE(entry))
  {
    Assert(DECOMPOSITION_SIZE(entry) == 1);
    x = (pg_wchar)entry->dec_index;
    *dec_size = 1;
    return &x;
  }
  else
  {
    *dec_size = DECOMPOSITION_SIZE(entry);
    return &UnicodeDecomp_codepoints[entry->dec_index];
  }
}

   
                                                                      
   
                                                                           
                               
   
static int
get_decomposed_size(pg_wchar code)
{
  pg_unicode_decomposition *entry;
  int size = 0;
  int i;
  const uint32 *decomp;
  int dec_size;

     
                                                                            
                                       
                                                                           
                 
     
  if (code >= SBASE && code < SBASE + SCOUNT)
  {
    uint32 tindex, sindex;

    sindex = code - SBASE;
    tindex = sindex % TCOUNT;

    if (tindex != 0)
    {
      return 3;
    }
    return 2;
  }

  entry = get_code_entry(code);

     
                                                                          
                                                                   
     
  if (entry == NULL || DECOMPOSITION_SIZE(entry) == 0)
  {
    return 1;
  }

     
                                                                             
                                                            
     
  decomp = get_code_decomposition(entry, &dec_size);
  for (i = 0; i < dec_size; i++)
  {
    uint32 lcode = decomp[i];

    size += get_decomposed_size(lcode);
  }

  return size;
}

   
                                                                         
                                                                      
                                                                        
                    
   
static bool
recompose_code(uint32 start, uint32 code, uint32 *result)
{
     
                                                                     
     
                                                  
     
  if (start >= LBASE && start < LBASE + LCOUNT && code >= VBASE && code < VBASE + VCOUNT)
  {
                                  
    uint32 lindex = start - LBASE;
    uint32 vindex = code - VBASE;

    *result = SBASE + (lindex * VCOUNT + vindex) * TCOUNT;
    return true;
  }
                                                    
  else if (start >= SBASE && start < (SBASE + SCOUNT) && ((start - SBASE) % TCOUNT) == 0 && code >= TBASE && code < (TBASE + TCOUNT))
  {
                                   
    uint32 tindex = code - TBASE;

    *result = start + tindex;
    return true;
  }
  else
  {
    int i;

       
                                                                           
                                                                       
                                                                           
                             
       
    for (i = 0; i < lengthof(UnicodeDecompMain); i++)
    {
      const pg_unicode_decomposition *entry = &UnicodeDecompMain[i];

      if (DECOMPOSITION_SIZE(entry) != 2)
      {
        continue;
      }

      if (DECOMPOSITION_NO_COMPOSE(entry))
      {
        continue;
      }

      if (start == UnicodeDecomp_codepoints[entry->dec_index] && code == UnicodeDecomp_codepoints[entry->dec_index + 1])
      {
        *result = entry->codepoint;
        return true;
      }
    }
  }

  return false;
}

   
                                                                
                                                                    
                                                                       
                                                                      
                        
   
static void
decompose_code(pg_wchar code, pg_wchar **result, int *current)
{
  pg_unicode_decomposition *entry;
  int i;
  const uint32 *decomp;
  int dec_size;

     
                                                                            
                                       
                                                                           
                 
     
  if (code >= SBASE && code < SBASE + SCOUNT)
  {
    uint32 l, v, tindex, sindex;
    pg_wchar *res = *result;

    sindex = code - SBASE;
    l = LBASE + sindex / (VCOUNT * TCOUNT);
    v = VBASE + (sindex % (VCOUNT * TCOUNT)) / TCOUNT;
    tindex = sindex % TCOUNT;

    res[*current] = l;
    (*current)++;
    res[*current] = v;
    (*current)++;

    if (tindex != 0)
    {
      res[*current] = TBASE + tindex;
      (*current)++;
    }

    return;
  }

  entry = get_code_entry(code);

     
                                                                 
                                                                         
                                                                         
                
     
  if (entry == NULL || DECOMPOSITION_SIZE(entry) == 0)
  {
    pg_wchar *res = *result;

    res[*current] = code;
    (*current)++;
    return;
  }

     
                                                                       
     
  decomp = get_code_decomposition(entry, &dec_size);
  for (i = 0; i < dec_size; i++)
  {
    pg_wchar lcode = (pg_wchar)decomp[i];

                                         
    decompose_code(lcode, result, current);
  }
}

   
                                                                   
   
                                                    
   
                                                                           
                                                                     
                                                                   
   
pg_wchar *
unicode_normalize_kc(const pg_wchar *input)
{
  pg_wchar *decomp_chars;
  pg_wchar *recomp_chars;
  int decomp_size, current_size;
  int count;
  const pg_wchar *p;

                                   
  int last_class;
  int starter_pos;
  int target_pos;
  uint32 starter_ch;

                                         

     
                                                                        
     
  decomp_size = 0;
  for (p = input; *p; p++)
  {
    decomp_size += get_decomposed_size(*p);
  }

  decomp_chars = (pg_wchar *)ALLOC((decomp_size + 1) * sizeof(pg_wchar));
  if (decomp_chars == NULL)
  {
    return NULL;
  }

     
                                                                         
                          
     
  current_size = 0;
  for (p = input; *p; p++)
  {
    decompose_code(*p, &decomp_chars, &current_size);
  }
  decomp_chars[decomp_size] = '\0';
  Assert(decomp_size == current_size);

                                              
  if (decomp_size == 0)
  {
    return decomp_chars;
  }

     
                                   
     
  for (count = 1; count < decomp_size; count++)
  {
    pg_wchar prev = decomp_chars[count - 1];
    pg_wchar next = decomp_chars[count];
    pg_wchar tmp;
    pg_unicode_decomposition *prevEntry = get_code_entry(prev);
    pg_unicode_decomposition *nextEntry = get_code_entry(next);

       
                                                                       
                                                                         
                               
       
    if (prevEntry == NULL || nextEntry == NULL)
    {
      continue;
    }

       
                                                                           
                                                               
                                                                  
                                                                       
                                                                           
                                                           
       
    if (nextEntry->comb_class == 0x0 || prevEntry->comb_class == 0x0)
    {
      continue;
    }

    if (prevEntry->comb_class <= nextEntry->comb_class)
    {
      continue;
    }

                             
    tmp = decomp_chars[count - 1];
    decomp_chars[count - 1] = decomp_chars[count];
    decomp_chars[count] = tmp;

                                  
    if (count > 1)
    {
      count -= 2;
    }
  }

     
                                                                          
                                                                            
                                                                          
                               
     
  recomp_chars = (pg_wchar *)ALLOC((decomp_size + 1) * sizeof(pg_wchar));
  if (!recomp_chars)
  {
    FREE(decomp_chars);
    return NULL;
  }

  last_class = -1;                                      
  starter_pos = 0;
  target_pos = 1;
  starter_ch = recomp_chars[0] = decomp_chars[0];

  for (count = 1; count < decomp_size; count++)
  {
    pg_wchar ch = decomp_chars[count];
    pg_unicode_decomposition *ch_entry = get_code_entry(ch);
    int ch_class = (ch_entry == NULL) ? 0 : ch_entry->comb_class;
    pg_wchar composite;

    if (last_class < ch_class && recompose_code(starter_ch, ch, &composite))
    {
      recomp_chars[starter_pos] = composite;
      starter_ch = composite;
    }
    else if (ch_class == 0)
    {
      starter_pos = target_pos;
      starter_ch = ch;
      last_class = -1;
      recomp_chars[target_pos++] = ch;
    }
    else
    {
      last_class = ch_class;
      recomp_chars[target_pos++] = ch;
    }
  }
  recomp_chars[target_pos] = (pg_wchar)'\0';

  FREE(decomp_chars);

  return recomp_chars;
}
