                       

   
                                  
                                                                      
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
   

#include "c.h"

#include "pg_getopt.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getopt.c	8.3 (Berkeley) 4/27/95";
#endif                             

   
                                                                              
                                                                              
                                                                         
                                                                           
                                                   
   
#ifndef HAVE_INT_OPTERR

int opterr = 1,                                         
    optind = 1,                                    
    optopt;                                         
char *optarg;                                        

#endif

#define BADCH (int)'?'
#define BADARG (int)':'
#define EMSG ""

   
          
                                    
   
                                                                          
                                                                              
                                                                             
                                                                           
                  
   
int
getopt(int nargc, char *const *nargv, const char *ostr)
{
  static char *place = EMSG;                               
  char *oli;                                               

  if (!*place)
  {                              
    if (optind >= nargc || *(place = nargv[optind]) != '-')
    {
      place = EMSG;
      return -1;
    }
    if (place[1] && *++place == '-' && place[1] == '\0')
    {                 
      ++optind;
      place = EMSG;
      return -1;
    }
  }                          
  if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr, optopt)))
  {
       
                                                                        
       
    if (optopt == (int)'-')
    {
      place = EMSG;
      return -1;
    }
    if (!*place)
    {
      ++optind;
    }
    if (opterr && *ostr != ':')
    {
      (void)fprintf(stderr, "illegal option -- %c\n", optopt);
    }
    return BADCH;
  }
  if (*++oli != ':')
  {                          
    optarg = NULL;
    if (!*place)
    {
      ++optind;
    }
  }
  else
  {                                   
    if (*place)                     
    {
      optarg = place;
    }
    else if (nargc <= ++optind)
    {             
      place = EMSG;
      if (*ostr == ':')
      {
        return BADARG;
      }
      if (opterr)
      {
        (void)fprintf(stderr, "option requires an argument -- %c\n", optopt);
      }
      return BADCH;
    }
    else
    {
                       
      optarg = nargv[optind];
    }
    place = EMSG;
    ++optind;
  }
  return optopt;                              
}
