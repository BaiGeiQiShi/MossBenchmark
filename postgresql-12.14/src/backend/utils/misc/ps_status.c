                                                                       
               
   
                                                                      
                                                                       
              
   
                                      
   
                                                                
                                                
                                                                       
   

#include "postgres.h"

#include <unistd.h>
#ifdef HAVE_SYS_PSTAT_H
#include <sys/pstat.h>                
#endif
#ifdef HAVE_PS_STRINGS
#include <machine/vmparam.h>                  
#include <sys/exec.h>
#endif
#if defined(__darwin__)
#include <crt_externs.h>
#endif

#include "libpq/libpq.h"
#include "miscadmin.h"
#include "utils/ps_status.h"
#include "utils/guc.h"

extern char **environ;
bool update_process_title = true;

   
                                            
   
                            
                                                            
                              
                       
                                                       
                          
                
                                    
             
                     
                                               
                         
                      
                                
                               
                       
                                               
                                         
                
                                                         
               
                              
                                              
   
#if defined(HAVE_SETPROCTITLE_FAST)
#define PS_USE_SETPROCTITLE_FAST
#elif defined(HAVE_SETPROCTITLE)
#define PS_USE_SETPROCTITLE
#elif defined(HAVE_PSTAT) && defined(PSTAT_SETCMD)
#define PS_USE_PSTAT
#elif defined(HAVE_PS_STRINGS)
#define PS_USE_PS_STRINGS
#elif (defined(BSD) || defined(__hurd__)) && !defined(__darwin__)
#define PS_USE_CHANGE_ARGV
#elif defined(__linux__) || defined(_AIX) || defined(__sgi) || (defined(sun) && !defined(BSD)) || defined(__svr5__) || defined(__darwin__)
#define PS_USE_CLOBBER_ARGV
#elif defined(WIN32)
#define PS_USE_WIN32
#else
#define PS_USE_NONE
#endif

                                                          
#if defined(_AIX) || defined(__linux__) || defined(__darwin__)
#define PS_PADDING '\0'
#else
#define PS_PADDING ' '
#endif

#ifndef PS_USE_CLOBBER_ARGV
                                                                
#define PS_BUFFER_SIZE 256
static char ps_buffer[PS_BUFFER_SIZE];
static const size_t ps_buffer_size = PS_BUFFER_SIZE;
#else                           
static char *ps_buffer;                                     
static size_t ps_buffer_size;                                    
static size_t last_status_len;                                        
#endif                          

static size_t ps_buffer_cur_len;                                

static size_t ps_buffer_fixed_size;                                  

                                            
static int save_argc;
static char **save_argv;

   
                                                                     
                                                                         
                                                          
   
                                                                            
                                                                           
                                                                              
                                                   
   
                                                                       
                                                        
   
char **
save_ps_display_args(int argc, char **argv)
{
  save_argc = argc;
  save_argv = argv;

#if defined(PS_USE_CLOBBER_ARGV)

     
                                                                           
                                                        
     
  {
    char *end_of_area = NULL;
    char **new_environ;
    int i;

       
                                         
       
    for (i = 0; i < argc; i++)
    {
      if (i == 0 || end_of_area + 1 == argv[i])
      {
        end_of_area = argv[i] + strlen(argv[i]);
      }
    }

    if (end_of_area == NULL)                             
    {
      ps_buffer = NULL;
      ps_buffer_size = 0;
      return argv;
    }

       
                                                           
       
    for (i = 0; environ[i] != NULL; i++)
    {
      if (end_of_area + 1 == environ[i])
      {
        end_of_area = environ[i] + strlen(environ[i]);
      }
    }

    ps_buffer = argv[0];
    last_status_len = ps_buffer_size = end_of_area - argv[0];

       
                                           
       
    new_environ = (char **)malloc((i + 1) * sizeof(char *));
    if (!new_environ)
    {
      write_stderr("out of memory\n");
      exit(1);
    }
    for (i = 0; environ[i] != NULL; i++)
    {
      new_environ[i] = strdup(environ[i]);
      if (!new_environ[i])
      {
        write_stderr("out of memory\n");
        exit(1);
      }
    }
    new_environ[i] = NULL;
    environ = new_environ;
  }
#endif                          

#if defined(PS_USE_CHANGE_ARGV) || defined(PS_USE_CLOBBER_ARGV)

     
                                                                       
                                
     
                                                                    
                                                                          
                                                                          
                                                                    
                                                                            
                                                                            
                
     
  {
    char **new_argv;
    int i;

    new_argv = (char **)malloc((argc + 1) * sizeof(char *));
    if (!new_argv)
    {
      write_stderr("out of memory\n");
      exit(1);
    }
    for (i = 0; i < argc; i++)
    {
      new_argv[i] = strdup(argv[i]);
      if (!new_argv[i])
      {
        write_stderr("out of memory\n");
        exit(1);
      }
    }
    new_argv[argc] = NULL;

#if defined(__darwin__)

       
                                                                           
                                                      
       
    *_NSGetArgv() = new_argv;
#endif

    argv = new_argv;
  }
#endif                                                

  return argv;
}

   
                                                                      
                                                                         
   
void
init_ps_display(const char *username, const char *dbname, const char *host_info, const char *initial_str)
{
  Assert(username);
  Assert(dbname);
  Assert(host_info);

#ifndef PS_USE_NONE
                                             
  if (!IsUnderPostmaster)
  {
    return;
  }

                                                               
  if (!save_argv)
  {
    return;
  }

#ifdef PS_USE_CLOBBER_ARGV
                                                         
  if (!ps_buffer)
  {
    return;
  }
#endif

     
                                                               
     

#ifdef PS_USE_CHANGE_ARGV
  save_argv[0] = ps_buffer;
  save_argv[1] = NULL;
#endif                         

#ifdef PS_USE_CLOBBER_ARGV
  {
    int i;

                                                            
    for (i = 1; i < save_argc; i++)
    {
      save_argv[i] = ps_buffer + ps_buffer_size;
    }
  }
#endif                          

     
                                      
     

#if defined(PS_USE_SETPROCTITLE) || defined(PS_USE_SETPROCTITLE_FAST)

     
                                                                           
          
     
#define PROGRAM_NAME_PREFIX ""
#else
#define PROGRAM_NAME_PREFIX "postgres: "
#endif

  if (*cluster_name == '\0')
  {
    snprintf(ps_buffer, ps_buffer_size, PROGRAM_NAME_PREFIX "%s %s %s ", username, dbname, host_info);
  }
  else
  {
    snprintf(ps_buffer, ps_buffer_size, PROGRAM_NAME_PREFIX "%s: %s %s %s ", cluster_name, username, dbname, host_info);
  }

  ps_buffer_cur_len = ps_buffer_fixed_size = strlen(ps_buffer);

  set_ps_display(initial_str, true);
#endif                      
}

   
                                                                       
                                                                     
   
void
set_ps_display(const char *activity, bool force)
{
#ifndef PS_USE_NONE
                                                                      
  if (!force && !update_process_title)
  {
    return;
  }

                                             
  if (!IsUnderPostmaster)
  {
    return;
  }

#ifdef PS_USE_CLOBBER_ARGV
                                                         
  if (!ps_buffer)
  {
    return;
  }
#endif

                                                                
  strlcpy(ps_buffer + ps_buffer_fixed_size, activity, ps_buffer_size - ps_buffer_fixed_size);
  ps_buffer_cur_len = strlen(ps_buffer);

                                                    

#ifdef PS_USE_SETPROCTITLE
  setproctitle("%s", ps_buffer);
#elif defined(PS_USE_SETPROCTITLE_FAST)
  setproctitle_fast("%s", ps_buffer);
#endif

#ifdef PS_USE_PSTAT
  {
    union pstun pst;

    pst.pst_command = ps_buffer;
    pstat(PSTAT_SETCMD, pst, ps_buffer_cur_len, 0, 0);
  }
#endif                   

#ifdef PS_USE_PS_STRINGS
  PS_STRINGS->ps_nargvstr = 1;
  PS_STRINGS->ps_argvstr = ps_buffer;
#endif                        

#ifdef PS_USE_CLOBBER_ARGV
                                                                           
  if (last_status_len > ps_buffer_cur_len)
  {
    MemSet(ps_buffer + ps_buffer_cur_len, PS_PADDING, last_status_len - ps_buffer_cur_len);
  }
  last_status_len = ps_buffer_cur_len;
#endif                          

#ifdef PS_USE_WIN32
  {
       
                                                                           
                                                                      
                                                                          
       
    static HANDLE ident_handle = INVALID_HANDLE_VALUE;
    char name[PS_BUFFER_SIZE + 32];

    if (ident_handle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(ident_handle);
    }

    sprintf(name, "pgident(%d): %s", MyProcPid, ps_buffer);

    ident_handle = CreateEvent(NULL, TRUE, FALSE, name);
  }
#endif                   
#endif                      
}

   
                                                                     
                                                                         
                                                                   
                         
   
const char *
get_ps_display(int *displen)
{
#ifdef PS_USE_CLOBBER_ARGV
                                                         
  if (!ps_buffer)
  {
    *displen = 0;
    return "";
  }
#endif

  *displen = (int)(ps_buffer_cur_len - ps_buffer_fixed_size);

  return ps_buffer + ps_buffer_fixed_size;
}
