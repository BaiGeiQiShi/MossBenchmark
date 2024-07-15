#include "c.h"
#include "pgtar.h"
#include <sys/stat.h>

   
                                                                            
                                               
   
                                                                            
                                                                             
                
   
                                                                              
                                                                            
                                                                             
                                                                            
                                   
   
void
print_tar_number(char *s, int len, uint64 val)
{
  if (val < (((uint64)1) << ((len - 1) * 3)))
  {
                                       
    s[--len] = ' ';
    while (len)
    {
      s[--len] = (val & 7) + '0';
      val >>= 3;
    }
  }
  else
  {
                                        
    s[0] = '\200';
    while (len > 1)
    {
      s[--len] = (val & 255);
      val >>= 8;
    }
  }
}

   
                                                                           
               
   
                                                                           
                                                                            
                                                                         
                                                                              
                                                             
   
uint64
read_tar_number(const char *s, int len)
{
  uint64 result = 0;

  if (*s == '\200')
  {
                  
    while (--len)
    {
      result <<= 8;
      result |= (unsigned char)(*++s);
    }
  }
  else
  {
               
    while (len-- && *s >= '0' && *s <= '7')
    {
      result <<= 3;
      result |= (*s - '0');
      s++;
    }
  }
  return result;
}

   
                                                                            
                                       
   
int
tarChecksum(char *header)
{
  int i, sum;

     
                                                                           
                                                                         
                                                  
     
  sum = 8 * ' ';                                        
  for (i = 0; i < 512; i++)
  {
    if (i < 148 || i >= 156)
    {
      sum += 0xFF & header[i];
    }
  }
  return sum;
}

   
                                                                            
                                                                        
                   
   
enum tarError
tarCreateHeader(char *h, const char *filename, const char *linktarget, pgoff_t size, mode_t mode, uid_t uid, gid_t gid, time_t mtime)
{
  if (strlen(filename) > 99)
  {
    return TAR_NAME_TOO_LONG;
  }

  if (linktarget && strlen(linktarget) > 99)
  {
    return TAR_SYMLINK_TOO_LONG;
  }

  memset(h, 0, 512);                             

                
  strlcpy(&h[0], filename, 100);
  if (linktarget != NULL || S_ISDIR(mode))
  {
       
                                                                  
                                                                       
                                                  
       
    int flen = strlen(filename);

    flen = Min(flen, 99);
    h[flen] = '/';
    h[flen + 1] = '\0';
  }

                                                                  
  print_tar_number(&h[100], 8, (mode & 07777));

                 
  print_tar_number(&h[108], 8, uid);

               
  print_tar_number(&h[116], 8, gid);

                    
  if (linktarget != NULL || S_ISDIR(mode))
  {
                                                  
    print_tar_number(&h[124], 12, 0);
  }
  else
  {
    print_tar_number(&h[124], 12, size);
  }

                   
  print_tar_number(&h[136], 12, mtime);

                                                                           

  if (linktarget != NULL)
  {
                              
    h[156] = '2';
                       
    strlcpy(&h[157], linktarget, 100);
  }
  else if (S_ISDIR(mode))
  {
                          
    h[156] = '5';
  }
  else
  {
                             
    h[156] = '0';
  }

               
  strcpy(&h[257], "ustar");

                 
  memcpy(&h[263], "00", 2);

               
                                                               
  strlcpy(&h[265], "postgres", 32);

                
                                                                 
  strlcpy(&h[297], "postgres", 32);

                   
  print_tar_number(&h[329], 8, 0);

                   
  print_tar_number(&h[337], 8, 0);

                                             

                                                
  print_tar_number(&h[148], 8, tarChecksum(h));

  return TAR_OK;
}
