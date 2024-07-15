                                                                            
   
             
                                            
   
                                                                         
   
   
                  
                        
   
                                                                              
                                                                          
                                                                        
                                                                  
                                                                            
   

#include "c.h"

#define _DIAGASSERT(x)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

                                                               

   
                            
                                                                      
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#if !HAVE_NBTOOL_CONFIG_H || !HAVE_MKSTEMP || !HAVE_MKDTEMP

#ifdef NOT_POSTGRESQL
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)mktemp.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: gettemp.c,v 1.17 2014/01/21 19:09:48 seanb Exp $");
#endif
#endif                             
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef NOT_POSTGRESQL
#if HAVE_NBTOOL_CONFIG_H
#define GETTEMP __nbcompat_gettemp
#else
#include "reentrant.h"
#include "local.h"
#define GETTEMP __gettemp
#endif
#endif

static int
GETTEMP(char *path, int *doopen, int domkdir)
{
  char *start, *trv;
  struct stat sbuf;
  u_int pid;

     
                                                                           
                                                                            
              
     
  static char xtra[2] = "aa";
  int xcnt = 0;

  _DIAGASSERT(path != NULL);
                          

  pid = getpid();

                                                   
  for (trv = path; *trv; ++trv)
  {
    if (*trv == 'X')
    {
      xcnt++;
    }
    else
    {
      xcnt = 0;
    }
  }

                                                              
  if (xcnt > 0)
  {
    *--trv = xtra[0];
    xcnt--;
  }
  if (xcnt > 5)
  {
    *--trv = xtra[1];
    xcnt--;
  }

                                                             
  for (; xcnt > 0; xcnt--)
  {
    *--trv = (pid % 10) + '0';
    pid /= 10;
  }

                                  
  if (xtra[0] != 'z')
  {
    xtra[0]++;
  }
  else
  {
    xtra[0] = 'a';
    if (xtra[1] != 'z')
    {
      xtra[1]++;
    }
    else
    {
      xtra[1] = 'a';
    }
  }

     
                                                                          
                                       
     
  for (start = trv + 1;; --trv)
  {
    if (trv <= path)
    {
      break;
    }
    if (*trv == '/')
    {
      int e;

      *trv = '\0';
      e = stat(path, &sbuf);
      *trv = '/';
      if (e == -1)
      {
        return doopen == NULL && !domkdir;
      }
      if (!S_ISDIR(sbuf.st_mode))
      {
        errno = ENOTDIR;
        return doopen == NULL && !domkdir;
      }
      break;
    }
  }

  for (;;)
  {
    if (doopen)
    {
      if ((*doopen = open(path, O_CREAT | O_EXCL | O_RDWR, 0600)) >= 0)
      {
        return 1;
      }
      if (errno != EEXIST)
      {
        return 0;
      }
    }
    else if (domkdir)
    {
      if (mkdir(path, 0700) >= 0)
      {
        return 1;
      }
      if (errno != EEXIST)
      {
        return 0;
      }
    }
    else if (lstat(path, &sbuf))
    {
      return errno == ENOENT ? 1 : 0;
    }

                                                            
    for (trv = start;;)
    {
      if (!*trv)
      {
        return 0;
      }
      if (*trv == 'z')
      {
        *trv++ = 'a';
      }
      else
      {
        if (isdigit((unsigned char)*trv))
        {
          *trv = 'a';
        }
        else
        {
          ++*trv;
        }
        break;
      }
    }
  }
                  
}

#endif                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
                          

                                                                  

   
                            
                                                                      
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#if !HAVE_NBTOOL_CONFIG_H || !HAVE_MKDTEMP

#ifdef NOT_POSTGRESQL

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)mktemp.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: mkdtemp.c,v 1.11 2012/03/15 18:22:30 christos Exp $");
#endif
#endif                             

#if HAVE_NBTOOL_CONFIG_H
#define GETTEMP __nbcompat_gettemp
#else
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "reentrant.h"
#include "local.h"
#define GETTEMP __gettemp
#endif

#endif

char *
mkdtemp(char *path)
{
  _DIAGASSERT(path != NULL);

  return GETTEMP(path, NULL, 1) ? path : NULL;
}

#endif                                             
