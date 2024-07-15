                                                                            
   
            
                            
   
   
                                                                         
                                                                        
   
   
                  
                         
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/string.h"

   
                                                           
   
bool
pg_str_endswith(const char *str, const char *end)
{
  size_t slen = strlen(str);
  size_t elen = strlen(end);

                                    
  if (elen > slen)
  {
    return false;
  }

                                      
  str += slen - elen;
  return strcmp(str, end) == 0;
}

   
                                                           
   
int
strtoint(const char *pg_restrict str, char **pg_restrict endptr, int base)
{
  long val;

  val = strtol(str, endptr, base);
  if (val != (int)val)
  {
    errno = ERANGE;
  }
  return (int)val;
}

   
                                                                 
   
                                                                
   
                                                                
                                                                               
                                                                             
                                                                                   
                                                                            
                                            
   
                                                                                 
                                                                      
   
                                                                               
                                                                                 
                                                                                  
            
   
void
pg_clean_ascii(char *str)
{
                                                  
  char *p;

  for (p = str; *p != '\0'; p++)
  {
    if (*p < 32 || *p > 126)
    {
      *p = '?';
    }
  }
}
