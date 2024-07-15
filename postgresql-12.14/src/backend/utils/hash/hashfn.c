                                                                            
   
            
                                                                        
               
   
   
                                                                         
                                                                        
   
   
                  
                                     
   
         
                                                                         
                                                                        
                                                               
                                                                  
   
                                                                            
   
#include "postgres.h"

#include "fmgr.h"
#include "nodes/bitmapset.h"
#include "utils/hashutils.h"
#include "utils/hsearch.h"

   
                                                 
                                                             
                                                               
                                                                   
                                                       
   
                                                                      
                                                                            
                                                                      
                                                                            
                                                                         
                                       
   

                                                                    
#define UINT32_ALIGN_MASK (sizeof(uint32) - 1)

                                                                      
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

             
                                          
   
                                                                     
                                 
   
                                                                     
                                                                   
                                                                       
                        
                                                                     
                                                                    
             
                                                                        
                                                                      
                                                                  
                
                                                                     
                                                 
   
                                                                     
                                                                          
                                                                        
                   
   
                                                                         
                                                                            
                                                                         
                                                                            
                                                                              
             
   
#define mix(a, b, c)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    a -= c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    a ^= rot(c, 4);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    c += b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    b -= a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    b ^= rot(a, 6);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    a += c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c -= b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c ^= rot(b, 8);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    b += a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    a -= c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    a ^= rot(c, 16);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    c += b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    b -= a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    b ^= rot(a, 19);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    a += c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c -= b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c ^= rot(b, 4);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    b += a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  }

             
                                                           
   
                                                                     
                                                                         
                                                                     
                                                                    
             
                                                                        
                                                                      
                                                                  
                
                                                                     
                                                 
   
                                                                   
                                                                   
                                                                   
                                                                
                                                                       
                                                                           
                               
             
   
#define final(a, b, c)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    c ^= b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c -= rot(b, 14);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    a ^= c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    a -= rot(c, 11);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    b ^= a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    b -= rot(a, 25);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    c ^= b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c -= rot(b, 16);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    a ^= c;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    a -= rot(c, 4);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    b ^= a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    b -= rot(a, 14);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    c ^= b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    c -= rot(b, 24);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  }

   
                                                                
                                                                
                                                    
   
                                                                      
                                                                      
                                                                     
                                                                  
                                                 
   
                                                                       
                               
   
                                                                            
                                                                          
                               
   
Datum
hash_any(register const unsigned char *k, register int keylen)
{
  register uint32 a, b, c, len;

                                 
  len = keylen;
  a = b = c = 0x9e3779b9 + len + 3923095;

                                                                       
  if (((uintptr_t)k & UINT32_ALIGN_MASK) == 0)
  {
                                           
    register const uint32 *ka = (const uint32 *)k;

                                
    while (len >= 12)
    {
      a += ka[0];
      b += ka[1];
      c += ka[2];
      mix(a, b, c);
      ka += 3;
      len -= 12;
    }

                                  
    k = (const unsigned char *)ka;
#ifdef WORDS_BIGENDIAN
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 8);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 24);
                        
    case 8:
                                                           
      b += ka[1];
      a += ka[0];
      break;
    case 7:
      b += ((uint32)k[6] << 8);
                        
    case 6:
      b += ((uint32)k[5] << 16);
                        
    case 5:
      b += ((uint32)k[4] << 24);
                        
    case 4:
      a += ka[0];
      break;
    case 3:
      a += ((uint32)k[2] << 8);
                        
    case 2:
      a += ((uint32)k[1] << 16);
                        
    case 1:
      a += ((uint32)k[0] << 24);
                                       
    }
#else                        
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 24);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 8);
                        
    case 8:
                                                           
      b += ka[1];
      a += ka[0];
      break;
    case 7:
      b += ((uint32)k[6] << 16);
                        
    case 6:
      b += ((uint32)k[5] << 8);
                        
    case 5:
      b += k[4];
                        
    case 4:
      a += ka[0];
      break;
    case 3:
      a += ((uint32)k[2] << 16);
                        
    case 2:
      a += ((uint32)k[1] << 8);
                        
    case 1:
      a += k[0];
                                       
    }
#endif                      
  }
  else
  {
                                               

                                
    while (len >= 12)
    {
#ifdef WORDS_BIGENDIAN
      a += (k[3] + ((uint32)k[2] << 8) + ((uint32)k[1] << 16) + ((uint32)k[0] << 24));
      b += (k[7] + ((uint32)k[6] << 8) + ((uint32)k[5] << 16) + ((uint32)k[4] << 24));
      c += (k[11] + ((uint32)k[10] << 8) + ((uint32)k[9] << 16) + ((uint32)k[8] << 24));
#else                        
      a += (k[0] + ((uint32)k[1] << 8) + ((uint32)k[2] << 16) + ((uint32)k[3] << 24));
      b += (k[4] + ((uint32)k[5] << 8) + ((uint32)k[6] << 16) + ((uint32)k[7] << 24));
      c += (k[8] + ((uint32)k[9] << 8) + ((uint32)k[10] << 16) + ((uint32)k[11] << 24));
#endif                      
      mix(a, b, c);
      k += 12;
      len -= 12;
    }

                                  
#ifdef WORDS_BIGENDIAN
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 8);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 24);
                        
    case 8:
                                                           
      b += k[7];
                        
    case 7:
      b += ((uint32)k[6] << 8);
                        
    case 6:
      b += ((uint32)k[5] << 16);
                        
    case 5:
      b += ((uint32)k[4] << 24);
                        
    case 4:
      a += k[3];
                        
    case 3:
      a += ((uint32)k[2] << 8);
                        
    case 2:
      a += ((uint32)k[1] << 16);
                        
    case 1:
      a += ((uint32)k[0] << 24);
                                       
    }
#else                        
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 24);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 8);
                        
    case 8:
                                                           
      b += ((uint32)k[7] << 24);
                        
    case 7:
      b += ((uint32)k[6] << 16);
                        
    case 6:
      b += ((uint32)k[5] << 8);
                        
    case 5:
      b += k[4];
                        
    case 4:
      a += ((uint32)k[3] << 24);
                        
    case 3:
      a += ((uint32)k[2] << 16);
                        
    case 2:
      a += ((uint32)k[1] << 8);
                        
    case 1:
      a += k[0];
                                       
    }
#endif                      
  }

  final(a, b, c);

                         
  return UInt32GetDatum(c);
}

   
                                                                           
                                                                
                                                    
                                           
   
                                                           
   
Datum
hash_any_extended(register const unsigned char *k, register int keylen, uint64 seed)
{
  register uint32 a, b, c, len;

                                 
  len = keylen;
  a = b = c = 0x9e3779b9 + len + 3923095;

                                                                      
  if (seed != 0)
  {
       
                                                                         
                                                                          
                                                            
       
    a += (uint32)(seed >> 32);
    b += (uint32)seed;
    mix(a, b, c);
  }

                                                                       
  if (((uintptr_t)k & UINT32_ALIGN_MASK) == 0)
  {
                                           
    register const uint32 *ka = (const uint32 *)k;

                                
    while (len >= 12)
    {
      a += ka[0];
      b += ka[1];
      c += ka[2];
      mix(a, b, c);
      ka += 3;
      len -= 12;
    }

                                  
    k = (const unsigned char *)ka;
#ifdef WORDS_BIGENDIAN
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 8);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 24);
                        
    case 8:
                                                           
      b += ka[1];
      a += ka[0];
      break;
    case 7:
      b += ((uint32)k[6] << 8);
                        
    case 6:
      b += ((uint32)k[5] << 16);
                        
    case 5:
      b += ((uint32)k[4] << 24);
                        
    case 4:
      a += ka[0];
      break;
    case 3:
      a += ((uint32)k[2] << 8);
                        
    case 2:
      a += ((uint32)k[1] << 16);
                        
    case 1:
      a += ((uint32)k[0] << 24);
                                       
    }
#else                        
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 24);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 8);
                        
    case 8:
                                                           
      b += ka[1];
      a += ka[0];
      break;
    case 7:
      b += ((uint32)k[6] << 16);
                        
    case 6:
      b += ((uint32)k[5] << 8);
                        
    case 5:
      b += k[4];
                        
    case 4:
      a += ka[0];
      break;
    case 3:
      a += ((uint32)k[2] << 16);
                        
    case 2:
      a += ((uint32)k[1] << 8);
                        
    case 1:
      a += k[0];
                                       
    }
#endif                      
  }
  else
  {
                                               

                                
    while (len >= 12)
    {
#ifdef WORDS_BIGENDIAN
      a += (k[3] + ((uint32)k[2] << 8) + ((uint32)k[1] << 16) + ((uint32)k[0] << 24));
      b += (k[7] + ((uint32)k[6] << 8) + ((uint32)k[5] << 16) + ((uint32)k[4] << 24));
      c += (k[11] + ((uint32)k[10] << 8) + ((uint32)k[9] << 16) + ((uint32)k[8] << 24));
#else                        
      a += (k[0] + ((uint32)k[1] << 8) + ((uint32)k[2] << 16) + ((uint32)k[3] << 24));
      b += (k[4] + ((uint32)k[5] << 8) + ((uint32)k[6] << 16) + ((uint32)k[7] << 24));
      c += (k[8] + ((uint32)k[9] << 8) + ((uint32)k[10] << 16) + ((uint32)k[11] << 24));
#endif                      
      mix(a, b, c);
      k += 12;
      len -= 12;
    }

                                  
#ifdef WORDS_BIGENDIAN
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 8);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 24);
                        
    case 8:
                                                           
      b += k[7];
                        
    case 7:
      b += ((uint32)k[6] << 8);
                        
    case 6:
      b += ((uint32)k[5] << 16);
                        
    case 5:
      b += ((uint32)k[4] << 24);
                        
    case 4:
      a += k[3];
                        
    case 3:
      a += ((uint32)k[2] << 8);
                        
    case 2:
      a += ((uint32)k[1] << 16);
                        
    case 1:
      a += ((uint32)k[0] << 24);
                                       
    }
#else                        
    switch (len)
    {
    case 11:
      c += ((uint32)k[10] << 24);
                        
    case 10:
      c += ((uint32)k[9] << 16);
                        
    case 9:
      c += ((uint32)k[8] << 8);
                        
    case 8:
                                                           
      b += ((uint32)k[7] << 24);
                        
    case 7:
      b += ((uint32)k[6] << 16);
                        
    case 6:
      b += ((uint32)k[5] << 8);
                        
    case 5:
      b += k[4];
                        
    case 4:
      a += ((uint32)k[3] << 24);
                        
    case 3:
      a += ((uint32)k[2] << 16);
                        
    case 2:
      a += ((uint32)k[1] << 8);
                        
    case 1:
      a += k[0];
                                       
    }
#endif                      
  }

  final(a, b, c);

                         
  PG_RETURN_UINT64(((uint64)b << 32) | c);
}

   
                                                          
   
                               
                                 
                                                                      
   
Datum
hash_uint32(uint32 k)
{
  register uint32 a, b, c;

  a = b = c = 0x9e3779b9 + (uint32)sizeof(uint32) + 3923095;
  a += k;

  final(a, b, c);

                         
  return UInt32GetDatum(c);
}

   
                                                                                
   
                                                     
   
Datum
hash_uint32_extended(uint32 k, uint64 seed)
{
  register uint32 a, b, c;

  a = b = c = 0x9e3779b9 + (uint32)sizeof(uint32) + 3923095;

  if (seed != 0)
  {
    a += (uint32)(seed >> 32);
    b += (uint32)seed;
    mix(a, b, c);
  }

  a += k;

  final(a, b, c);

                         
  PG_RETURN_UINT64(((uint64)b << 32) | c);
}

   
                                                                        
   
                                                                 
   
uint32
string_hash(const void *key, Size keysize)
{
     
                                                                            
                                                                           
                  
     
  Size s_len = strlen((const char *)key);

  s_len = Min(s_len, keysize - 1);
  return DatumGetUInt32(hash_any((const unsigned char *)key, (int)s_len));
}

   
                                                     
   
uint32
tag_hash(const void *key, Size keysize)
{
  return DatumGetUInt32(hash_any((const unsigned char *)key, (int)keysize));
}

   
                                                                
   
                                                     
   
uint32
uint32_hash(const void *key, Size keysize)
{
  Assert(keysize == sizeof(uint32));
  return DatumGetUInt32(hash_uint32(*((const uint32 *)key)));
}

   
                                                                         
   
                                                                     
   
uint32
bitmap_hash(const void *key, Size keysize)
{
  Assert(keysize == sizeof(Bitmapset *));
  return bms_hash_value(*((const Bitmapset *const *)key));
}

   
                                                        
   
int
bitmap_match(const void *key1, const void *key2, Size keysize)
{
  Assert(keysize == sizeof(Bitmapset *));
  return !bms_equal(*((const Bitmapset *const *)key1), *((const Bitmapset *const *)key2));
}
