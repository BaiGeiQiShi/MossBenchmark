                                                                            
   
                     
                                                     
   
   
                                                                         
                                                                        
   
                                     
   
                                                                            
   
#include "postgres_fe.h"

#include "parallel.h"
#include "pg_backup_utils.h"

                                   
const char *progname = NULL;

#define MAX_ON_EXIT_NICELY 20

static struct
{
  on_exit_nicely_callback function;
  void *arg;
} on_exit_nicely_list[MAX_ON_EXIT_NICELY];

static int on_exit_nicely_index;

   
                                                
   
                                                                
                                                                  
                                                             
   
void
set_dump_section(const char *arg, int *dumpSections)
{
                                                     
  if (*dumpSections == DUMP_UNSECTIONED)
  {
    *dumpSections = 0;
  }

  if (strcmp(arg, "pre-data") == 0)
  {
    *dumpSections |= DUMP_PRE_DATA;
  }
  else if (strcmp(arg, "data") == 0)
  {
    *dumpSections |= DUMP_DATA;
  }
  else if (strcmp(arg, "post-data") == 0)
  {
    *dumpSections |= DUMP_POST_DATA;
  }
  else
  {
    pg_log_error("unrecognized section name: \"%s\"", arg);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }
}

                                                                
void
on_exit_nicely(on_exit_nicely_callback function, void *arg)
{
  if (on_exit_nicely_index >= MAX_ON_EXIT_NICELY)
  {
    pg_log_fatal("out of on_exit_nicely slots");
    exit_nicely(1);
  }
  on_exit_nicely_list[on_exit_nicely_index].function = function;
  on_exit_nicely_list[on_exit_nicely_index].arg = arg;
  on_exit_nicely_index++;
}

   
                                                                           
                                 
   
                                                                               
                          
   
                                                                           
                                                                             
                                                                              
                                                                              
                                                                            
                                                                           
                                                                             
                                                                           
                                           
   
void
exit_nicely(int code)
{
  int i;

  for (i = on_exit_nicely_index - 1; i >= 0; i--)
  {
    on_exit_nicely_list[i].function(code, on_exit_nicely_list[i].arg);
  }

#ifdef WIN32
  if (parallel_init_done && GetCurrentThreadId() != mainThreadId)
  {
    _endthreadex(code);
  }
#endif

  exit(code);
}
