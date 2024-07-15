                                                                            
   
                      
                                                       
   
                                                                          
                                                             
   
                                                                         
                                                                     
                                              
   
                                                                
   
                  
                                 
   
                                                                            
   

#include "c.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef USE_OPENSSL
#include <openssl/rand.h>
#endif
#ifdef WIN32
#include <wincrypt.h>
#endif

#ifdef WIN32
   
                                                                        
                                                         
   
static HCRYPTPROV hProvider = 0;
#endif

#if defined(USE_DEV_URANDOM)
   
                                    
   
static bool
random_from_file(const char *filename, void *buf, size_t len)
{
  int f;
  char *p = buf;
  ssize_t res;

  f = open(filename, O_RDONLY, 0);
  if (f == -1)
  {
    return false;
  }

  while (len)
  {
    res = read(f, p, len);
    if (res <= 0)
    {
      if (errno == EINTR)
      {
        continue;                                        
      }

      close(f);
      return false;
    }

    p += res;
    len -= res;
  }

  close(f);
  return true;
}
#endif

   
                    
   
                                                                     
                                                                      
   
                                                                     
                                   
   
                             
                                         
                   
   
                                                              
                                    
   
                                                             
                                                                  
                                                                    
                                                       
   
bool
pg_strong_random(void *buf, size_t len)
{
     
                                                                 
     
#if defined(USE_OPENSSL_RANDOM)
  int i;

     
                                                                          
                                                                        
                                                                         
     
#define NUM_RAND_POLL_RETRIES 8

  for (i = 0; i < NUM_RAND_POLL_RETRIES; i++)
  {
    if (RAND_status() == 1)
    {
                                             
      break;
    }

    if (RAND_poll() == 0)
    {
         
                                                                        
                                                                      
                                                                      
              
         
      break;
    }
  }

  if (RAND_bytes(buf, len) == 1)
  {
    return true;
  }
  return false;

     
                                                             
     
#elif defined(USE_WIN32_RANDOM)
  if (hProvider == 0)
  {
    if (!CryptAcquireContext(&hProvider, NULL, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
         
                                                                         
                   
         
      hProvider = 0;
    }
  }
                                                       
  if (hProvider != 0)
  {
    if (CryptGenRandom(hProvider, len, buf))
    {
      return true;
    }
  }
  return false;

     
                                  
     
#elif defined(USE_DEV_URANDOM)
  if (random_from_file("/dev/urandom", buf, len))
  {
    return true;
  }
  return false;

#else
                                                        
#error no source of random numbers configured
#endif
}
