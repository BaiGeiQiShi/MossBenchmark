   
                                        
   
                                           
                                                                      
   
                               
                                       
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   
                          
   

#include "c.h"

#include "getopt_long.h"

#define BADCH '?'
#define BADARG ':'
#define EMSG ""

   
               
                                                       
   
                                                                          
                                                                              
                                                                             
                                                                           
                  
   
int
getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
  static char *place = EMSG;                               
  char *oli;                                               

  if (!*place)
  {                              
    if (optind >= argc)
    {
      place = EMSG;
      return -1;
    }

    place = argv[optind];

    if (place[0] != '-')
    {
      place = EMSG;
      return -1;
    }

    place++;

    if (!*place)
    {
                                            
      place = EMSG;
      return -1;
    }

    if (place[0] == '-' && place[1] == '\0')
    {
                                                  
      ++optind;
      place = EMSG;
      return -1;
    }

    if (place[0] == '-' && place[1])
    {
                       
      size_t namelen;
      int i;

      place++;

      namelen = strcspn(place, "=");
      for (i = 0; longopts[i].name != NULL; i++)
      {
        if (strlen(longopts[i].name) == namelen && strncmp(place, longopts[i].name, namelen) == 0)
        {
          int has_arg = longopts[i].has_arg;

          if (has_arg != no_argument)
          {
            if (place[namelen] == '=')
            {
              optarg = place + namelen + 1;
            }
            else if (optind < argc - 1 && has_arg == required_argument)
            {
              optind++;
              optarg = argv[optind];
            }
            else
            {
              if (optstring[0] == ':')
              {
                return BADARG;
              }

              if (opterr && has_arg == required_argument)
              {
                fprintf(stderr, "%s: option requires an argument -- %s\n", argv[0], place);
              }

              place = EMSG;
              optind++;

              if (has_arg == required_argument)
              {
                return BADCH;
              }
              optarg = NULL;
            }
          }
          else
          {
            optarg = NULL;
            if (place[namelen] != 0)
            {
                              
            }
          }

          optind++;

          if (longindex)
          {
            *longindex = i;
          }

          place = EMSG;

          if (longopts[i].flag == NULL)
          {
            return longopts[i].val;
          }
          else
          {
            *longopts[i].flag = longopts[i].val;
            return 0;
          }
        }
      }

      if (opterr && optstring[0] != ':')
      {
        fprintf(stderr, "%s: illegal option -- %s\n", argv[0], place);
      }
      place = EMSG;
      optind++;
      return BADCH;
    }
  }

                    
  optopt = (int)*place++;

  oli = strchr(optstring, optopt);
  if (!oli)
  {
    if (!*place)
    {
      ++optind;
    }
    if (opterr && *optstring != ':')
    {
      fprintf(stderr, "%s: illegal option -- %c\n", argv[0], optopt);
    }
    return BADCH;
  }

  if (oli[1] != ':')
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
    else if (argc <= ++optind)
    {             
      place = EMSG;
      if (*optstring == ':')
      {
        return BADARG;
      }
      if (opterr)
      {
        fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optopt);
      }
      return BADCH;
    }
    else
    {
                       
      optarg = argv[optind];
    }
    place = EMSG;
    ++optind;
  }
  return optopt;
}
