                                                                            
   
                 
   
                                                                     
                                                                             
                                                                      
   
                                                                              
                                                                         
                                                                       
                         
   
                                                                             
                                                                          
                         
   
                                                                         
                                                                        
   
                                      
   
                                                                            
   

#include "postgres_fe.h"

#include <limits.h>

#include "pqexpbuffer.h"

#ifdef WIN32
#include "win32.h"
#endif

                                                     
static const char oom_buffer[1] = "";

                                                  
static const char *oom_buffer_ptr = oom_buffer;

static bool
appendPQExpBufferVA(PQExpBuffer str, const char *fmt, va_list args) pg_attribute_printf(2, 0);

   
                         
   
                                                            
   
static void
markPQExpBufferBroken(PQExpBuffer str)
{
  if (str->data != oom_buffer)
  {
    free(str->data);
  }

     
                                                                           
                                                                             
                                                                         
                                                          
     
  str->data = unconstify(char *, oom_buffer_ptr);
  str->len = 0;
  str->maxlen = 0;
}

   
                     
   
                                                               
   
PQExpBuffer
createPQExpBuffer(void)
{
  PQExpBuffer res;

  res = (PQExpBuffer)malloc(sizeof(PQExpBufferData));
  if (res != NULL)
  {
    initPQExpBuffer(res);
  }

  return res;
}

   
                   
   
                                                                            
                                
   
void
initPQExpBuffer(PQExpBuffer str)
{
  str->data = (char *)malloc(INITIAL_EXPBUFFER_SIZE);
  if (str->data == NULL)
  {
    str->data = unconstify(char *, oom_buffer_ptr);                        
    str->maxlen = 0;
    str->len = 0;
  }
  else
  {
    str->maxlen = INITIAL_EXPBUFFER_SIZE;
    str->len = 0;
    str->data[0] = '\0';
  }
}

   
                            
   
                                                          
                                                
   
void
destroyPQExpBuffer(PQExpBuffer str)
{
  if (str)
  {
    termPQExpBuffer(str);
    free(str);
  }
}

   
                        
                                                                
                                              
   
void
termPQExpBuffer(PQExpBuffer str)
{
  if (str->data != oom_buffer)
  {
    free(str->data);
  }
                                                     
  str->data = unconstify(char *, oom_buffer_ptr);                        
  str->maxlen = 0;
  str->len = 0;
}

   
                    
                                 
   
                                                                    
   
void
resetPQExpBuffer(PQExpBuffer str)
{
  if (str)
  {
    if (str->data != oom_buffer)
    {
      str->len = 0;
      str->data[0] = '\0';
    }
    else
    {
                                              
      initPQExpBuffer(str);
    }
  }
}

   
                      
                                                                         
                                                     
   
                                                                        
                                          
   
int
enlargePQExpBuffer(PQExpBuffer str, size_t needed)
{
  size_t newlen;
  char *newdata;

  if (PQExpBufferBroken(str))
  {
    return 0;                     
  }

     
                                                                            
                                                                           
                    
     
  if (needed >= ((size_t)INT_MAX - str->len))
  {
    markPQExpBufferBroken(str);
    return 0;
  }

  needed += str->len + 1;                               

                                                                

  if (needed <= str->maxlen)
  {
    return 1;                               
  }

     
                                                                          
                                                                    
                                                                          
     
  newlen = (str->maxlen > 0) ? (2 * str->maxlen) : 64;
  while (needed > newlen)
  {
    newlen = 2 * newlen;
  }

     
                                                                          
                                                                         
                                       
     
  if (newlen > (size_t)INT_MAX)
  {
    newlen = (size_t)INT_MAX;
  }

  newdata = (char *)realloc(str->data, newlen);
  if (newdata != NULL)
  {
    str->data = newdata;
    str->maxlen = newlen;
    return 1;
  }

  markPQExpBufferBroken(str);
  return 0;
}

   
                     
                                                                             
                                                                         
                                                             
                                                       
   
void
printfPQExpBuffer(PQExpBuffer str, const char *fmt, ...)
{
  int save_errno = errno;
  va_list args;
  bool done;

  resetPQExpBuffer(str);

  if (PQExpBufferBroken(str))
  {
    return;                     
  }

                                                                 
  do
  {
    errno = save_errno;
    va_start(args, fmt);
    done = appendPQExpBufferVA(str, fmt, args);
    va_end(args);
  } while (!done);
}

   
                     
   
                                                                             
                                                                         
                                                                           
           
   
void
appendPQExpBuffer(PQExpBuffer str, const char *fmt, ...)
{
  int save_errno = errno;
  va_list args;
  bool done;

  if (PQExpBufferBroken(str))
  {
    return;                     
  }

                                                                 
  do
  {
    errno = save_errno;
    va_start(args, fmt);
    done = appendPQExpBufferVA(str, fmt, args);
    va_end(args);
  } while (!done);
}

   
                       
                                                       
                                                                      
                                                                
   
                                                                    
                                                
   
static bool
appendPQExpBufferVA(PQExpBuffer str, const char *fmt, va_list args)
{
  size_t avail;
  size_t needed;
  int nprinted;

     
                                                                             
                                                                           
     
  if (str->maxlen > str->len + 16)
  {
    avail = str->maxlen - str->len;

    nprinted = vsnprintf(str->data + str->len, avail, fmt, args);

       
                                                                         
                                                
       
    if (unlikely(nprinted < 0))
    {
      markPQExpBufferBroken(str);
      return true;
    }

    if ((size_t)nprinted < avail)
    {
                                                                   
      str->len += nprinted;
      return true;
    }

       
                                                                           
                                                                           
                                                                   
       
                                                                           
                                       
       
    if (unlikely(nprinted > INT_MAX - 1))
    {
      markPQExpBufferBroken(str);
      return true;
    }
    needed = nprinted + 1;
  }
  else
  {
       
                                                                         
                                                                      
                                                                          
                                                                         
                                               
       
    needed = 32;
  }

                                               
  if (!enlargePQExpBuffer(str, needed))
  {
    return true;                          
  }

  return false;
}

   
                        
                                                                   
                 
   
void
appendPQExpBufferStr(PQExpBuffer str, const char *data)
{
  appendBinaryPQExpBuffer(str, data, strlen(data));
}

   
                         
                                
                                                          
   
void
appendPQExpBufferChar(PQExpBuffer str, char ch)
{
                                
  if (!enlargePQExpBuffer(str, 1))
  {
    return;
  }

                                
  str->data[str->len] = ch;
  str->len++;
  str->data[str->len] = '\0';
}

   
                           
   
                                                                        
                 
   
void
appendBinaryPQExpBuffer(PQExpBuffer str, const char *data, size_t datalen)
{
                                
  if (!enlargePQExpBuffer(str, datalen))
  {
    return;
  }

                           
  memcpy(str->data + str->len, data, datalen);
  str->len += datalen;

     
                                                                          
                    
     
  str->data[str->len] = '\0';
}
