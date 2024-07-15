                        
   
                                                                  
                                                                       
                                          
   
                                                                      
                                                                    
                                                                     
                                   
   

   
                                  
                                                                       
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                   

#include "c.h"

#include <netinet/in.h>
#include <ctype.h>

#include "port/pg_bswap.h"

   
                                                      
                                                           
                                                
                                                        
                                                                     
   
int
inet_aton(const char *cp, struct in_addr *addr)
{
  unsigned int val;
  int base, n;
  char c;
  u_int parts[4];
  u_int *pp = parts;

  for (;;)
  {
       
                                                                          
                               
       
    val = 0;
    base = 10;
    if (*cp == '0')
    {
      if (*++cp == 'x' || *cp == 'X')
      {
        base = 16, cp++;
      }
      else
      {
        base = 8;
      }
    }
    while ((c = *cp) != '\0')
    {
      if (isdigit((unsigned char)c))
      {
        val = (val * base) + (c - '0');
        cp++;
        continue;
      }
      if (base == 16 && isxdigit((unsigned char)c))
      {
        val = (val << 4) + (c + 10 - (islower((unsigned char)c) ? 'a' : 'A'));
        cp++;
        continue;
      }
      break;
    }
    if (*cp == '.')
    {
         
                                                                    
                                            
         
      if (pp >= parts + 3 || val > 0xff)
      {
        return 0;
      }
      *pp++ = val, cp++;
    }
    else
    {
      break;
    }
  }

     
                              
     
  while (*cp)
  {
    if (!isspace((unsigned char)*cp++))
    {
      return 0;
    }
  }

     
                                                                     
     
  n = pp - parts + 1;
  switch (n)
  {

  case 1:                   
    break;

  case 2:                       
    if (val > 0xffffff)
    {
      return 0;
    }
    val |= parts[0] << 24;
    break;

  case 3:                           
    if (val > 0xffff)
    {
      return 0;
    }
    val |= (parts[0] << 24) | (parts[1] << 16);
    break;

  case 4:                              
    if (val > 0xff)
    {
      return 0;
    }
    val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
    break;
  }
  if (addr)
  {
    addr->s_addr = pg_hton32(val);
  }
  return 1;
}
