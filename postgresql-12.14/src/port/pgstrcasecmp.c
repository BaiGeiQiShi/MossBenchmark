                                                                            
   
                  
                                                                      
   
                                                                        
                                                                       
                                                                         
                                                                       
                                                                         
                                                                
               
   
                                                                           
   
                                                                          
                                                                          
                                   
   
   
                                                                         
   
                           
   
                                                                            
   
#include "c.h"

#include <ctype.h>

   
                                                               
   
int
pg_strcasecmp(const char *s1, const char *s2)
{
  for (;;)
  {
    unsigned char ch1 = (unsigned char)*s1++;
    unsigned char ch2 = (unsigned char)*s2++;

    if (ch1 != ch2)
    {
      if (ch1 >= 'A' && ch1 <= 'Z')
      {
        ch1 += 'a' - 'A';
      }
      else if (IS_HIGHBIT_SET(ch1) && isupper(ch1))
      {
        ch1 = tolower(ch1);
      }

      if (ch2 >= 'A' && ch2 <= 'Z')
      {
        ch2 += 'a' - 'A';
      }
      else if (IS_HIGHBIT_SET(ch2) && isupper(ch2))
      {
        ch2 = tolower(ch2);
      }

      if (ch1 != ch2)
      {
        return (int)ch1 - (int)ch2;
      }
    }
    if (ch1 == 0)
    {
      break;
    }
  }
  return 0;
}

   
                                                                               
                                                      
   
int
pg_strncasecmp(const char *s1, const char *s2, size_t n)
{
  while (n-- > 0)
  {
    unsigned char ch1 = (unsigned char)*s1++;
    unsigned char ch2 = (unsigned char)*s2++;

    if (ch1 != ch2)
    {
      if (ch1 >= 'A' && ch1 <= 'Z')
      {
        ch1 += 'a' - 'A';
      }
      else if (IS_HIGHBIT_SET(ch1) && isupper(ch1))
      {
        ch1 = tolower(ch1);
      }

      if (ch2 >= 'A' && ch2 <= 'Z')
      {
        ch2 += 'a' - 'A';
      }
      else if (IS_HIGHBIT_SET(ch2) && isupper(ch2))
      {
        ch2 = tolower(ch2);
      }

      if (ch1 != ch2)
      {
        return (int)ch1 - (int)ch2;
      }
    }
    if (ch1 == 0)
    {
      break;
    }
  }
  return 0;
}

   
                                   
   
                                                                          
                                                                         
                                             
   
unsigned char
pg_toupper(unsigned char ch)
{
  if (ch >= 'a' && ch <= 'z')
  {
    ch += 'A' - 'a';
  }
  else if (IS_HIGHBIT_SET(ch) && islower(ch))
  {
    ch = toupper(ch);
  }
  return ch;
}

   
                                   
   
                                                                          
                                                                         
                                             
   
unsigned char
pg_tolower(unsigned char ch)
{
  if (ch >= 'A' && ch <= 'Z')
  {
    ch += 'a' - 'A';
  }
  else if (IS_HIGHBIT_SET(ch) && isupper(ch))
  {
    ch = tolower(ch);
  }
  return ch;
}

   
                                                                   
   
unsigned char
pg_ascii_toupper(unsigned char ch)
{
  if (ch >= 'a' && ch <= 'z')
  {
    ch += 'A' - 'a';
  }
  return ch;
}

   
                                                                   
   
unsigned char
pg_ascii_tolower(unsigned char ch)
{
  if (ch >= 'A' && ch <= 'Z')
  {
    ch += 'a' - 'A';
  }
  return ch;
}
