   
                                   
   
   
                            
                                                                      
   
                                                             
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   
                                               
#include "postgres_fe.h"

#include "mb/pg_wchar.h"

int
pg_wchar_strncmp(const pg_wchar *s1, const pg_wchar *s2, size_t n)
{
  if (n == 0)
  {
    return 0;
  }
  do
  {
    if (*s1 != *s2++)
    {
      return (*s1 - *(s2 - 1));
    }
    if (*s1++ == 0)
    {
      break;
    }
  } while (--n != 0);
  return 0;
}

int
pg_char_and_wchar_strncmp(const char *s1, const pg_wchar *s2, size_t n)
{
  if (n == 0)
  {
    return 0;
  }
  do
  {
    if ((pg_wchar)((unsigned char)*s1) != *s2++)
    {
      return ((pg_wchar)((unsigned char)*s1) - *(s2 - 1));
    }
    if (*s1++ == 0)
    {
      break;
    }
  } while (--n != 0);
  return 0;
}

size_t
pg_wchar_strlen(const pg_wchar *str)
{
  const pg_wchar *s;

  for (s = str; *s; ++s)
    ;
  return (s - str);
}
