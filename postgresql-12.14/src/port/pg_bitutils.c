                                                                            
   
                 
                                                      
   
                                                           
   
                  
                            
   
                                                                            
   
#include "c.h"

#ifdef HAVE__GET_CPUID
#include <cpuid.h>
#endif
#ifdef HAVE__CPUID
#include <intrin.h>
#endif

#include "port/pg_bitutils.h"

   
                                                                        
                                                                         
                                                                          
   
                                                                 
                                                                   
                                                                      
   
const uint8 pg_leftmost_one_pos[256] = {0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

   
                                                                         
                                                                         
                                                                          
   
                                                                 
                                                                   
                                                                      
   
const uint8 pg_rightmost_one_pos[256] = {0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

   
                                                                  
   
                                                                   
                                                         
   
const uint8 pg_number_of_ones[256] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

   
                                                                        
                                                                     
   
                                                                           
                                           
   
#ifdef HAVE_X86_64_POPCNTQ
#if defined(HAVE__GET_CPUID) || defined(HAVE__CPUID)
#define USE_POPCNT_ASM 1
#endif
#endif

static int
pg_popcount32_slow(uint32 word);
static int
pg_popcount64_slow(uint64 word);

#ifdef USE_POPCNT_ASM
static bool
pg_popcount_available(void);
static int
pg_popcount32_choose(uint32 word);
static int
pg_popcount64_choose(uint64 word);
static int
pg_popcount32_asm(uint32 word);
static int
pg_popcount64_asm(uint64 word);

int (*pg_popcount32)(uint32 word) = pg_popcount32_choose;
int (*pg_popcount64)(uint64 word) = pg_popcount64_choose;
#else
int (*pg_popcount32)(uint32 word) = pg_popcount32_slow;
int (*pg_popcount64)(uint64 word) = pg_popcount64_slow;
#endif                     

#ifdef USE_POPCNT_ASM

   
                                                                            
   
static bool
pg_popcount_available(void)
{
  unsigned int exx[4] = {0, 0, 0, 0};

#if defined(HAVE__GET_CPUID)
  __get_cpuid(1, &exx[0], &exx[1], &exx[2], &exx[3]);
#elif defined(HAVE__CPUID)
  __cpuid(exx, 1);
#else
#error cpuid instruction not available
#endif

  return (exx[2] & (1 << 23)) != 0;             
}

   
                                                                      
                                                                       
                                                                         
                              
   
static int
pg_popcount32_choose(uint32 word)
{
  if (pg_popcount_available())
  {
    pg_popcount32 = pg_popcount32_asm;
    pg_popcount64 = pg_popcount64_asm;
  }
  else
  {
    pg_popcount32 = pg_popcount32_slow;
    pg_popcount64 = pg_popcount64_slow;
  }

  return pg_popcount32(word);
}

static int
pg_popcount64_choose(uint64 word)
{
  if (pg_popcount_available())
  {
    pg_popcount32 = pg_popcount32_asm;
    pg_popcount64 = pg_popcount64_asm;
  }
  else
  {
    pg_popcount32 = pg_popcount32_slow;
    pg_popcount64 = pg_popcount64_slow;
  }

  return pg_popcount64(word);
}

   
                     
                                            
   
static int
pg_popcount32_asm(uint32 word)
{
  uint32 res;

  __asm__ __volatile__(" popcntl %1,%0\n" : "=q"(res) : "rm"(word) : "cc");
  return (int)res;
}

   
                     
                                            
   
static int
pg_popcount64_asm(uint64 word)
{
  uint64 res;

  __asm__ __volatile__(" popcntq %1,%0\n" : "=q"(res) : "rm"(word) : "cc");
  return (int)res;
}

#endif                     

   
                      
                                            
   
static int
pg_popcount32_slow(uint32 word)
{
#ifdef HAVE__BUILTIN_POPCOUNT
  return __builtin_popcount(word);
#else                               
  int result = 0;

  while (word != 0)
  {
    result += pg_number_of_ones[word & 255];
    word >>= 8;
  }

  return result;
#endif                             
}

   
                      
                                            
   
static int
pg_popcount64_slow(uint64 word)
{
#ifdef HAVE__BUILTIN_POPCOUNT
#if defined(HAVE_LONG_INT_64)
  return __builtin_popcountl(word);
#elif defined(HAVE_LONG_LONG_INT_64)
  return __builtin_popcountll(word);
#else
#error must have a working 64-bit integer datatype
#endif
#else                               
  int result = 0;

  while (word != 0)
  {
    result += pg_number_of_ones[word & 255];
    word >>= 8;
  }

  return result;
#endif                             
}

   
               
                                        
   
uint64
pg_popcount(const char *buf, int bytes)
{
  uint64 popcnt = 0;

#if SIZEOF_VOID_P >= 8
                                                          
  if (buf == (const char *)TYPEALIGN(8, buf))
  {
    const uint64 *words = (const uint64 *)buf;

    while (bytes >= 8)
    {
      popcnt += pg_popcount64(*words++);
      bytes -= 8;
    }

    buf = (const char *)words;
  }
#else
                                                          
  if (buf == (const char *)TYPEALIGN(4, buf))
  {
    const uint32 *words = (const uint32 *)buf;

    while (bytes >= 4)
    {
      popcnt += pg_popcount32(*words++);
      bytes -= 4;
    }

    buf = (const char *)words;
  }
#endif

                                   
  while (bytes--)
  {
    popcnt += pg_number_of_ones[(unsigned char)*buf++];
  }

  return popcnt;
}
