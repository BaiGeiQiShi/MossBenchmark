   
              
   
                         
   
                                                                
                                 
   

#include "postgres_fe.h"

#include <sys/wait.h>
#ifdef WIN32
#include <io.h>
#endif

#include "pg_upgrade.h"

static int parallel_jobs;

#ifdef WIN32
   
                                                                       
                                                                    
                                                                          
   
HANDLE *thread_handles;

typedef struct
{
  char *log_file;
  char *opt_log_file;
  char *cmd;
} exec_thread_arg;

typedef struct
{
  DbInfoArr *old_db_arr;
  DbInfoArr *new_db_arr;
  char *old_pgdata;
  char *new_pgdata;
  char *old_tablespace;
} transfer_thread_arg;

exec_thread_arg **exec_thread_args;
transfer_thread_arg **transfer_thread_args;

                                                                                
void **cur_thread_args;

DWORD
win32_exec_prog(exec_thread_arg *args);
DWORD
win32_transfer_all_new_dbs(transfer_thread_arg *args);
#endif

   
                      
   
                                                                          
                                                                       
   
void
parallel_exec_prog(const char *log_file, const char *opt_log_file, const char *fmt, ...)
{
  va_list args;
  char cmd[MAX_STRING];

#ifndef WIN32
  pid_t child;
#else
  HANDLE child;
  exec_thread_arg *new_arg;
#endif

  va_start(args, fmt);
  vsnprintf(cmd, sizeof(cmd), fmt, args);
  va_end(args);

  if (user_opts.jobs <= 1)
  {
                                                  
    exec_prog(log_file, opt_log_file, true, true, "%s", cmd);
  }
  else
  {
                  
#ifdef WIN32
    if (thread_handles == NULL)
    {
      thread_handles = pg_malloc(user_opts.jobs * sizeof(HANDLE));
    }

    if (exec_thread_args == NULL)
    {
      int i;

      exec_thread_args = pg_malloc(user_opts.jobs * sizeof(exec_thread_arg *));

         
                                                                       
                                                                         
                                                          
         
      for (i = 0; i < user_opts.jobs; i++)
      {
        exec_thread_args[i] = pg_malloc0(sizeof(exec_thread_arg));
      }
    }

    cur_thread_args = (void **)exec_thread_args;
#endif
                                   
    while (reap_child(false) == true)
      ;

                                        
    if (parallel_jobs >= user_opts.jobs)
    {
      reap_child(true);
    }

                                          
    parallel_jobs++;

                                                       
    fflush(NULL);

#ifndef WIN32
    child = fork();
    if (child == 0)
    {
                                                
      _exit(!exec_prog(log_file, opt_log_file, true, true, "%s", cmd));
    }
    else if (child < 0)
    {
                       
      pg_fatal("could not create worker process: %s\n", strerror(errno));
    }
#else
                                                   
    new_arg = exec_thread_args[parallel_jobs - 1];

                                                                      
    if (new_arg->log_file)
    {
      pg_free(new_arg->log_file);
    }
    new_arg->log_file = pg_strdup(log_file);
    if (new_arg->opt_log_file)
    {
      pg_free(new_arg->opt_log_file);
    }
    new_arg->opt_log_file = opt_log_file ? pg_strdup(opt_log_file) : NULL;
    if (new_arg->cmd)
    {
      pg_free(new_arg->cmd);
    }
    new_arg->cmd = pg_strdup(cmd);

    child = (HANDLE)_beginthreadex(NULL, 0, (void *)win32_exec_prog, new_arg, 0, NULL);
    if (child == 0)
    {
      pg_fatal("could not create worker thread: %s\n", strerror(errno));
    }

    thread_handles[parallel_jobs - 1] = child;
#endif
  }

  return;
}

#ifdef WIN32
DWORD
win32_exec_prog(exec_thread_arg *args)
{
  int ret;

  ret = !exec_prog(args->log_file, args->opt_log_file, true, true, "%s", args->cmd);

                         
  return ret;
}
#endif

   
                                 
   
                                                                                    
                                                    
   
void
parallel_transfer_all_new_dbs(DbInfoArr *old_db_arr, DbInfoArr *new_db_arr, char *old_pgdata, char *new_pgdata, char *old_tablespace)
{
#ifndef WIN32
  pid_t child;
#else
  HANDLE child;
  transfer_thread_arg *new_arg;
#endif

  if (user_opts.jobs <= 1)
  {
    transfer_all_new_dbs(old_db_arr, new_db_arr, old_pgdata, new_pgdata, NULL);
  }
  else
  {
                  
#ifdef WIN32
    if (thread_handles == NULL)
    {
      thread_handles = pg_malloc(user_opts.jobs * sizeof(HANDLE));
    }

    if (transfer_thread_args == NULL)
    {
      int i;

      transfer_thread_args = pg_malloc(user_opts.jobs * sizeof(transfer_thread_arg *));

         
                                                                       
                                                                         
                                                          
         
      for (i = 0; i < user_opts.jobs; i++)
      {
        transfer_thread_args[i] = pg_malloc0(sizeof(transfer_thread_arg));
      }
    }

    cur_thread_args = (void **)transfer_thread_args;
#endif
                                   
    while (reap_child(false) == true)
      ;

                                        
    if (parallel_jobs >= user_opts.jobs)
    {
      reap_child(true);
    }

                                          
    parallel_jobs++;

                                                       
    fflush(NULL);

#ifndef WIN32
    child = fork();
    if (child == 0)
    {
      transfer_all_new_dbs(old_db_arr, new_db_arr, old_pgdata, new_pgdata, old_tablespace);
                                                             
                                                
      _exit(0);
    }
    else if (child < 0)
    {
                       
      pg_fatal("could not create worker process: %s\n", strerror(errno));
    }
#else
                                                   
    new_arg = transfer_thread_args[parallel_jobs - 1];

                                                                      
    new_arg->old_db_arr = old_db_arr;
    new_arg->new_db_arr = new_db_arr;
    if (new_arg->old_pgdata)
    {
      pg_free(new_arg->old_pgdata);
    }
    new_arg->old_pgdata = pg_strdup(old_pgdata);
    if (new_arg->new_pgdata)
    {
      pg_free(new_arg->new_pgdata);
    }
    new_arg->new_pgdata = pg_strdup(new_pgdata);
    if (new_arg->old_tablespace)
    {
      pg_free(new_arg->old_tablespace);
    }
    new_arg->old_tablespace = old_tablespace ? pg_strdup(old_tablespace) : NULL;

    child = (HANDLE)_beginthreadex(NULL, 0, (void *)win32_transfer_all_new_dbs, new_arg, 0, NULL);
    if (child == 0)
    {
      pg_fatal("could not create worker thread: %s\n", strerror(errno));
    }

    thread_handles[parallel_jobs - 1] = child;
#endif
  }

  return;
}

#ifdef WIN32
DWORD
win32_transfer_all_new_dbs(transfer_thread_arg *args)
{
  transfer_all_new_dbs(args->old_db_arr, args->new_db_arr, args->old_pgdata, args->new_pgdata, args->old_tablespace);

                         
  return 0;
}
#endif

   
                                                
   
bool
reap_child(bool wait_for_child)
{
#ifndef WIN32
  int work_status;
  pid_t child;
#else
  int thread_num;
  DWORD res;
#endif

  if (user_opts.jobs <= 1 || parallel_jobs == 0)
  {
    return false;
  }

#ifndef WIN32
  child = waitpid(-1, &work_status, wait_for_child ? 0 : WNOHANG);
  if (child == (pid_t)-1)
  {
    pg_fatal("waitpid() failed: %s\n", strerror(errno));
  }
  if (child == 0)
  {
    return false;                                       
  }
  if (work_status != 0)
  {
    pg_fatal("child process exited abnormally: status %d\n", work_status);
  }
#else
                              
  thread_num = WaitForMultipleObjects(parallel_jobs, thread_handles, false, wait_for_child ? INFINITE : 0);

  if (thread_num == WAIT_TIMEOUT || thread_num == WAIT_FAILED)
  {
    return false;
  }

                                              
  thread_num -= WAIT_OBJECT_0;

                      
  GetExitCodeThread(thread_handles[thread_num], &res);
  if (res != 0)
  {
    pg_fatal("child worker exited abnormally: %s\n", strerror(errno));
  }

                                       
  CloseHandle(thread_handles[thread_num]);

                                                 
  if (thread_num != parallel_jobs - 1)
  {
    void *tmp_args;

    thread_handles[thread_num] = thread_handles[parallel_jobs - 1];

       
                                                                         
                                                                         
                                                                         
                                         
       
    tmp_args = cur_thread_args[thread_num];
    cur_thread_args[thread_num] = cur_thread_args[parallel_jobs - 1];
    cur_thread_args[parallel_jobs - 1] = tmp_args;
  }
#endif

                                          
  parallel_jobs--;

  return true;
}
