   
                 
                                        
   
                                                                              
                                                                             
                                                                             
                                                     
   
                                                                         
   
                  
                                            
   

#include "postgres_fe.h"

#include "libpq-fe.h"

int
main(int argc, char *argv[])
{
  PQconninfoOption *opts;
  PQconninfoOption *defs;
  PQconninfoOption *opt;
  PQconninfoOption *def;
  char *errmsg = NULL;
  bool local = true;

  if (argc != 2)
  {
    return 1;
  }

  opts = PQconninfoParse(argv[1], &errmsg);
  if (opts == NULL)
  {
    fprintf(stderr, "uri-regress: %s", errmsg);
    return 1;
  }

  defs = PQconndefaults();
  if (defs == NULL)
  {
    fprintf(stderr, "uri-regress: cannot fetch default options\n");
    return 1;
  }

     
                                                                          
     
                                                                           
                                 
     
  for (opt = opts, def = defs; opt->keyword; ++opt, ++def)
  {
    if (opt->val != NULL)
    {
      if (def->val == NULL || strcmp(opt->val, def->val) != 0)
      {
        printf("%s='%s' ", opt->keyword, opt->val);
      }

         
                                                                         
                                                                      
         
                                                             
                                                                     
                                                                    
                                     
         
      if (*opt->val && (strcmp(opt->keyword, "hostaddr") == 0 || (strcmp(opt->keyword, "host") == 0 && *opt->val != '/')))
      {
        local = false;
      }
    }
  }

  if (local)
  {
    printf("(local)\n");
  }
  else
  {
    printf("(inet)\n");
  }

  return 0;
}
