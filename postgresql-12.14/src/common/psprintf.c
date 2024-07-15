                                                                            
   
              
                                               
   
   
                                                                         
                                                                        
   
   
                  
                           
   
                                                                            
   

#ifndef FRONTEND

#include "postgres.h"

#include "utils/memutils.h"

#else

#include "postgres_fe.h"

                                                                            
#define MaxAllocSize ((Size)0x3fffffff)                     

#endif

   
            
   
                                                                              
                                                                            
                                                                        
                                                                         
   
                                                                           
                                                                      
                                                               
   
char *
psprintf(const char *fmt, ...)
{
  int save_errno = errno;
  size_t len = 128;                                           

  for (;;)
  {
    char *result;
    va_list args;
    size_t newlen;

       
                                                                          
                           
       
    result = (char *)palloc(len);

                                 
    errno = save_errno;
    va_start(args, fmt);
    newlen = pvsnprintf(result, len, fmt, args);
    va_end(args);

    if (newlen < len)
    {
      return result;              
    }

                                                                      
    pfree(result);
    len = newlen;
  }
}

   
              
   
                                                                          
                                                                 
   
                                                                       
                                                                    
   
                                                                             
                                                                           
                           
   
                                                                        
                                               
   
                                                                    
                                                
   
                                                                      
                                                                            
                                                                          
                                                                          
                                                                     
                                                                          
                                                                           
   
size_t
pvsnprintf(char *buf, size_t len, const char *fmt, va_list args)
{
  int nprinted;

  nprinted = vsnprintf(buf, len, fmt, args);

                                                                          
  if (unlikely(nprinted < 0))
  {
#ifndef FRONTEND
    elog(ERROR, "vsnprintf failed: %m with format string \"%s\"", fmt);
#else
    fprintf(stderr, "vsnprintf failed: %s with format string \"%s\"\n", strerror(errno), fmt);
    exit(EXIT_FAILURE);
#endif
  }

  if ((size_t)nprinted < len)
  {
                                                                 
    return (size_t)nprinted;
  }

     
                                                                         
                                                                             
                                                             
     
                                                                         
                                                                
     
  if (unlikely((size_t)nprinted > MaxAllocSize - 1))
  {
#ifndef FRONTEND
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of memory")));
#else
    fprintf(stderr, _("out of memory\n"));
    exit(EXIT_FAILURE);
#endif
  }

  return nprinted + 1;
}
