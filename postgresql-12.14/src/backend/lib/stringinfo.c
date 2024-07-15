                                                                            
   
                
   
                                                                    
                                                                             
                                                                      
   
                                                                         
                                                                        
   
                                  
   
                                                                            
   
#include "postgres.h"

#include "lib/stringinfo.h"
#include "utils/memutils.h"

   
                  
   
                                                              
   
StringInfo
makeStringInfo(void)
{
  StringInfo res;

  res = (StringInfo)palloc(sizeof(StringInfoData));

  initStringInfo(res);

  return res;
}

   
                  
   
                                                                           
                                
   
void
initStringInfo(StringInfo str)
{
  int size = 1024;                                  

  str->data = (char *)palloc(size);
  str->maxlen = size;
  resetStringInfo(str);
}

   
                   
   
                                                                
                                         
   
void
resetStringInfo(StringInfo str)
{
  str->data[0] = '\0';
  str->len = 0;
  str->cursor = 0;
}

   
                    
   
                                                                              
                                                                         
                                                                           
           
   
void
appendStringInfo(StringInfo str, const char *fmt, ...)
{
  int save_errno = errno;

  for (;;)
  {
    va_list args;
    int needed;

                                 
    errno = save_errno;
    va_start(args, fmt);
    needed = appendStringInfoVA(str, fmt, args);
    va_end(args);

    if (needed == 0)
    {
      break;              
    }

                                                 
    enlargeStringInfo(str, needed);
  }
}

   
                      
   
                                                                          
                                                                              
                                                                              
                                                                            
                                                                         
                                                
   
                                                                    
                                                
   
                                                                           
                                                                              
                                                                             
                   
   
int
appendStringInfoVA(StringInfo str, const char *fmt, va_list args)
{
  int avail;
  size_t nprinted;

  Assert(str != NULL);

     
                                                                             
                                                                       
                                                        
     
  avail = str->maxlen - str->len;
  if (avail < 16)
  {
    return 32;
  }

  nprinted = pvsnprintf(str->data + str->len, (size_t)avail, fmt, args);

  if (nprinted < (size_t)avail)
  {
                                                                 
    str->len += (int)nprinted;
    return 0;
  }

                                                            
  str->data[str->len] = '\0';

     
                                                                          
                                                                         
                         
     
  return (int)nprinted;
}

   
                          
   
                                           
                                                   
   
void
appendStringInfoString(StringInfo str, const char *s)
{
  appendBinaryStringInfo(str, s, strlen(s));
}

   
                        
   
                                
                                                         
   
void
appendStringInfoChar(StringInfo str, char ch)
{
                                
  if (str->len + 1 >= str->maxlen)
  {
    enlargeStringInfo(str, 1);
  }

                                
  str->data[str->len] = ch;
  str->len++;
  str->data[str->len] = '\0';
}

   
                          
   
                                                      
   
void
appendStringInfoSpaces(StringInfo str, int count)
{
  if (count > 0)
  {
                                  
    enlargeStringInfo(str, count);

                               
    while (--count >= 0)
    {
      str->data[str->len++] = ' ';
    }
    str->data[str->len] = '\0';
  }
}

   
                          
   
                                                                       
                                                               
   
void
appendBinaryStringInfo(StringInfo str, const char *data, int datalen)
{
  Assert(str != NULL);

                                
  enlargeStringInfo(str, datalen);

                           
  memcpy(str->data + str->len, data, datalen);
  str->len += datalen;

     
                                                                          
                                                                             
                                         
     
  str->data[str->len] = '\0';
}

   
                            
   
                                                                       
                                                              
   
void
appendBinaryStringInfoNT(StringInfo str, const char *data, int datalen)
{
  Assert(str != NULL);

                                
  enlargeStringInfo(str, datalen);

                           
  memcpy(str->data + str->len, data, datalen);
  str->len += datalen;
}

   
                     
   
                                                           
                                                     
   
                                                                         
                                                                        
                                                                    
                                                                         
                        
   
                                                                          
                                                                          
                                                                      
                                                     
   
void
enlargeStringInfo(StringInfo str, int needed)
{
  int newlen;

     
                                                                           
                                                    
     
  if (needed < 0)                        
  {
    elog(ERROR, "invalid string enlargement request size: %d", needed);
  }
  if (((Size)needed) >= (MaxAllocSize - (Size)str->len))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("out of memory"), errdetail("Cannot enlarge string buffer containing %d bytes by %d more bytes.", str->len, needed)));
  }

  needed += str->len + 1;                               

                                                                     

  if (needed <= str->maxlen)
  {
    return;                               
  }

     
                                                                          
                                                                    
                                                                          
     
  newlen = 2 * str->maxlen;
  while (needed > newlen)
  {
    newlen = 2 * newlen;
  }

     
                                                                          
                                                                    
                                                     
     
  if (newlen > (int)MaxAllocSize)
  {
    newlen = (int)MaxAllocSize;
  }

  str->data = (char *)repalloc(str->data, newlen);

  str->maxlen = newlen;
}
