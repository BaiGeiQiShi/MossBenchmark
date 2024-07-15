   
                                              
   
                                                                
   
                              
   
#include "postgres_fe.h"

#include <ctype.h>

#include "common.h"
#include "stringutils.h"

#define PQmblenBounded(s, e) strnlen(s, PQmblen(s, e))

   
                                                     
   
                                                                        
                                                  
   
                                                                      
               
   
                                                                   
                                                                  
                                                                 
                                                                    
                                                         
                                                             
                                                                           
                                        
                                                
   
                                                                       
                                         
   
                                                                             
                                                                         
                                                             
   
                                                                          
                                                                             
   
                                                                       
   
                                                                       
                                                                         
                              
   
char *
strtokx(const char *s, const char *whitespace, const char *delim, const char *quote, char escape, bool e_strings, bool del_quotes, int encoding)
{
  static char *storage = NULL;                                      
                                                
  static char *string = NULL;                                               
                                              

                                   
  unsigned int offset;
  char *start;
  char *p;

  if (s)
  {
    free(storage);

       
                                                                      
                                                                        
                                                           
       
    storage = pg_malloc(2 * strlen(s) + 1);
    strcpy(storage, s);
    string = storage;
  }

  if (!storage)
  {
    return NULL;
  }

                               
  offset = strspn(string, whitespace);
  start = &string[offset];

                              
  if (*start == '\0')
  {
                                                                
    free(storage);
    storage = NULL;
    string = NULL;
    return NULL;
  }

                                   
  if (delim && strchr(delim, *start))
  {
       
                                                                          
                                                                       
                                                                       
                                                                         
                     
       
    p = start + 1;
    if (*p != '\0')
    {
      if (!strchr(whitespace, *p))
      {
        memmove(p + 1, p, strlen(p) + 1);
      }
      *p = '\0';
      string = p + 1;
    }
    else
    {
                                              
      string = p;
    }

    return start;
  }

                          
  p = start;
  if (e_strings && (*p == 'E' || *p == 'e') && p[1] == '\'')
  {
    quote = "'";
    escape = '\\';                                          
    p++;
  }

                                 
  if (quote && strchr(quote, *p))
  {
                                                               
    char thisquote = *p++;

    for (; *p; p += PQmblenBounded(p, encoding))
    {
      if (*p == escape && p[1] != '\0')
      {
        p++;                               
      }
      else if (*p == thisquote && p[1] == thisquote)
      {
        p++;                            
      }
      else if (*p == thisquote)
      {
        p++;                          
        break;
      }
    }

       
                                                                          
                                         
       
    if (*p != '\0')
    {
      if (!strchr(whitespace, *p))
      {
        memmove(p + 1, p, strlen(p) + 1);
      }
      *p = '\0';
      string = p + 1;
    }
    else
    {
                                              
      string = p;
    }

                                                 
    if (del_quotes)
    {
      strip_quotes(start, thisquote, escape, encoding);
    }

    return start;
  }

     
                                                                           
                                                                   
                                                                             
     
  offset = strcspn(start, whitespace);

  if (delim)
  {
    unsigned int offset2 = strcspn(start, delim);

    if (offset > offset2)
    {
      offset = offset2;
    }
  }

  if (quote)
  {
    unsigned int offset2 = strcspn(start, quote);

    if (offset > offset2)
    {
      offset = offset2;
    }
  }

  p = start + offset;

     
                                                                        
                                       
     
  if (*p != '\0')
  {
    if (!strchr(whitespace, *p))
    {
      memmove(p + 1, p, strlen(p) + 1);
    }
    *p = '\0';
    string = p + 1;
  }
  else
  {
                                            
    string = p;
  }

  return start;
}

   
                
   
                                                                               
                                                                              
                                                                             
                                   
   
                                                        
   
void
strip_quotes(char *source, char quote, char escape, int encoding)
{
  char *src;
  char *dst;

  Assert(source != NULL);
  Assert(quote != '\0');

  src = dst = source;

  if (*src && *src == quote)
  {
    src++;                         
  }

  while (*src)
  {
    char c = *src;
    int i;

    if (c == quote && src[1] == '\0')
    {
      break;                          
    }
    else if (c == quote && src[1] == quote)
    {
      src++;                            
    }
    else if (c == escape && src[1] != '\0')
    {
      src++;                                
    }

    i = PQmblenBounded(src, encoding);
    while (i--)
    {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
}

   
                   
   
                                                                             
                                                                               
                                 
   
                              
                                                            
                                                         
                                    
                                                 
   
                                                                         
                                                                  
   
char *
quote_if_needed(const char *source, const char *entails_quote, char quote, char escape, int encoding)
{
  const char *src;
  char *ret;
  char *dst;
  bool need_quotes = false;

  Assert(source != NULL);
  Assert(quote != '\0');

  src = source;
  dst = ret = pg_malloc(2 * strlen(src) + 3);             

  *dst++ = quote;

  while (*src)
  {
    char c = *src;
    int i;

    if (c == quote)
    {
      need_quotes = true;
      *dst++ = quote;
    }
    else if (c == escape)
    {
      need_quotes = true;
      *dst++ = escape;
    }
    else if (strchr(entails_quote, c))
    {
      need_quotes = true;
    }

    i = PQmblenBounded(src, encoding);
    while (i--)
    {
      *dst++ = *src++;
    }
  }

  *dst++ = quote;
  *dst = '\0';

  if (!need_quotes)
  {
    free(ret);
    ret = NULL;
  }

  return ret;
}
