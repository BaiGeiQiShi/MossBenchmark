                                                                            
   
                 
                                           
   
                                                                            
                                                                           
                                                                               
                                                                                
                                      
   
                                                                              
                                                                               
                                                                            
                                                                              
         
   
                                                                              
                                                                             
                                                                        
                                                                             
                                                                          
                                                                             
                                                                                
                      
   
                                                                
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "lib/bloomfilter.h"
#include "port/pg_bitutils.h"
#include "utils/hashutils.h"

#define MAX_HASH_FUNCS 10

struct bloom_filter
{
                                                          
  int k_hash_funcs;
  uint64 seed;
                                                                    
  uint64 m;
  unsigned char bitset[FLEXIBLE_ARRAY_MEMBER];
};

static int
my_bloom_power(uint64 target_bitset_bits);
static int
optimal_k(uint64 bitset_bits, int64 total_elems);
static void
k_hashes(bloom_filter *filter, uint32 *hashes, unsigned char *elem, size_t len);
static inline uint32
mod_m(uint32 a, uint64 m);

   
                                                                                
                                                                           
                 
   
                                                                          
                                                                             
                                                                   
                                                                               
                     
   
                                                                                
                                                                                
                                                                           
                                                                    
                                                                            
                                                                               
                                                                               
                
   
                                                                             
                                                                           
                                                                            
                                                                               
                                                                           
                     
   
bloom_filter *
bloom_create(int64 total_elems, int bloom_work_mem, uint64 seed)
{
  bloom_filter *filter;
  int bloom_power;
  uint64 bitset_bytes;
  uint64 bitset_bits;

     
                                                                      
                                                                            
                                                                           
                                                                          
                                                                    
     
  bitset_bytes = Min(bloom_work_mem * UINT64CONST(1024), total_elems * 2);
  bitset_bytes = Max(1024 * 1024, bitset_bytes);

     
                                                                             
                                                           
     
  bloom_power = my_bloom_power(bitset_bytes * BITS_PER_BYTE);
  bitset_bits = UINT64CONST(1) << bloom_power;
  bitset_bytes = bitset_bits / BITS_PER_BYTE;

                                               
  filter = palloc0(offsetof(bloom_filter, bitset) + sizeof(unsigned char) * bitset_bytes);
  filter->k_hash_funcs = optimal_k(bitset_bits, total_elems);
  filter->seed = seed;
  filter->m = bitset_bits;

  return filter;
}

   
                     
   
void
bloom_free(bloom_filter *filter)
{
  pfree(filter);
}

   
                               
   
void
bloom_add_element(bloom_filter *filter, unsigned char *elem, size_t len)
{
  uint32 hashes[MAX_HASH_FUNCS];
  int i;

  k_hashes(filter, hashes, elem, len);

                                                                  
  for (i = 0; i < filter->k_hash_funcs; i++)
  {
    filter->bitset[hashes[i] >> 3] |= 1 << (hashes[i] & 7);
  }
}

   
                                                  
   
                                                                        
                                                                               
                                       
   
bool
bloom_lacks_element(bloom_filter *filter, unsigned char *elem, size_t len)
{
  uint32 hashes[MAX_HASH_FUNCS];
  int i;

  k_hashes(filter, hashes, elem, len);

                                                                  
  for (i = 0; i < filter->k_hash_funcs; i++)
  {
    if (!(filter->bitset[hashes[i] >> 3] & (1 << (hashes[i] & 7))))
    {
      return true;
    }
  }

  return false;
}

   
                                              
   
                                                                              
                                                                           
                                                                             
                                                               
   
                                                                             
                                                                               
                                                         
   
double
bloom_prop_bits_set(bloom_filter *filter)
{
  int bitset_bytes = filter->m / BITS_PER_BYTE;
  uint64 bits_set = pg_popcount((char *)filter->bitset, bitset_bytes);

  return bits_set / (double)filter->m;
}

   
                                                                           
                       
   
                                                                             
         
   
                                                                              
                                                                          
                                                                                
                                                                        
                                                                            
                          
   
static int
my_bloom_power(uint64 target_bitset_bits)
{
  int bloom_power = -1;

  while (target_bitset_bits > 0 && bloom_power < 32)
  {
    bloom_power++;
    target_bitset_bits >>= 1;
  }

  return bloom_power;
}

   
                                                                               
                                                                             
                                           
   
static int
optimal_k(uint64 bitset_bits, int64 total_elems)
{
  int k = rint(log(2.0) * bitset_bits / total_elems);

  return Max(1, Min(k, MAX_HASH_FUNCS));
}

   
                                       
   
                                                                               
                     
   
                                                                          
                                                                                
                                                                              
                                                                             
                                                                             
            
   
static void
k_hashes(bloom_filter *filter, uint32 *hashes, unsigned char *elem, size_t len)
{
  uint64 hash;
  uint32 x, y;
  uint64 m;
  int i;

                                                               
  hash = DatumGetUInt64(hash_any_extended(elem, len, filter->seed));
  x = (uint32)hash;
  y = (uint32)(hash >> 32);
  m = filter->m;

  x = mod_m(x, m);
  y = mod_m(y, m);

                         
  hashes[0] = x;
  for (i = 1; i < filter->k_hash_funcs; i++)
  {
    x = mod_m(x + y, m);
    y = mod_m(y + i, m);

    hashes[i] = x;
  }
}

   
                                        
   
                                                            
   
                                                                                
                                                                               
                                           
   
static inline uint32
mod_m(uint32 val, uint64 m)
{
  Assert(m <= PG_UINT32_MAX + UINT64CONST(1));
  Assert(((m - 1) & m) == 0);

  return val & (m - 1);
}
