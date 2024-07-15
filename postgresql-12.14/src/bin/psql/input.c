   
                                              
   
                                                                
   
                        
   
#include "postgres_fe.h"

#ifndef WIN32
#include <unistd.h>
#endif
#include <fcntl.h>
#include <limits.h>

#include "input.h"
#include "settings.h"
#include "tab-complete.h"
#include "common.h"

#include "common/logging.h"

#ifndef WIN32
#define PSQLHISTORY ".psql_history"
#else
#define PSQLHISTORY "psql_history"
#endif

                                                          
                                                              
#ifdef USE_READLINE
static bool useReadline;
static bool useHistory;

static char *psql_history;

static int history_lines_added;

   
                                                                       
   
                                                                 
                                                                
                                                          
                     
   
#define NL_IN_HISTORY 0x01
#endif

static void
finishInput(void);

   
                      
   
                                                                
   
                                        
                                                                          
                                                                             
   
                                    
   
                                                                  
   
char *
gets_interactive(const char *prompt, PQExpBuffer query_buf)
{
#ifdef USE_READLINE
  if (useReadline)
  {
    char *result;

       
                                                                           
                                                                       
                                                                           
                                                                      
                                                                       
       
#ifdef HAVE_RL_RESET_SCREEN_SIZE
    rl_reset_screen_size();
#endif

                                                                     
    tab_completion_query_buf = query_buf;

                                                          
    sigint_interrupt_enabled = true;

                                                                     
    result = readline((char *)prompt);

                              
    sigint_interrupt_enabled = false;

                          
    tab_completion_query_buf = NULL;

    return result;
  }
#endif

  fputs(prompt, stdout);
  fflush(stdout);
  return gets_fromFile(stdin);
}

   
                                                                               
   
void
pg_append_history(const char *s, PQExpBuffer history_buf)
{
#ifdef USE_READLINE
  if (useHistory && s)
  {
    appendPQExpBufferStr(history_buf, s);
    if (!s[0] || s[strlen(s) - 1] != '\n')
    {
      appendPQExpBufferChar(history_buf, '\n');
    }
  }
#endif
}

   
                                                                   
                                   
   
                                                                          
                                                                         
                                               
   
void
pg_send_history(PQExpBuffer history_buf)
{
#ifdef USE_READLINE
  static char *prev_hist = NULL;

  char *s = history_buf->data;
  int i;

                                                              
  for (i = strlen(s) - 1; i >= 0 && s[i] == '\n'; i--)
    ;
  s[i + 1] = '\0';

  if (useHistory && s[0])
  {
    if (((pset.histcontrol & hctl_ignorespace) && s[0] == ' ') || ((pset.histcontrol & hctl_ignoredups) && prev_hist && strcmp(s, prev_hist) == 0))
    {
                                                           
    }
    else
    {
                                                             
      if (prev_hist)
      {
        free(prev_hist);
      }
      prev_hist = pg_strdup(s);
                                   
      add_history(s);
                                                      
      history_lines_added++;
    }
  }

  resetPQExpBuffer(history_buf);
#endif
}

   
                 
   
                                                                           
                                                                   
   
                                                                  
   
                                                                         
                                            
   
char *
gets_fromFile(FILE *source)
{
  static PQExpBuffer buffer = NULL;

  char line[1024];

  if (buffer == NULL)                          
  {
    buffer = createPQExpBuffer();
  }
  else
  {
    resetPQExpBuffer(buffer);
  }

  for (;;)
  {
    char *result;

                                                          
    sigint_interrupt_enabled = true;

                       
    result = fgets(line, sizeof(line), source);

                              
    sigint_interrupt_enabled = false;

                       
    if (result == NULL)
    {
      if (ferror(source))
      {
        pg_log_error("could not read from input file: %m");
        return NULL;
      }
      break;
    }

    appendPQExpBufferStr(buffer, line);

    if (PQExpBufferBroken(buffer))
    {
      pg_log_error("out of memory");
      return NULL;
    }

              
    if (buffer->len > 0 && buffer->data[buffer->len - 1] == '\n')
    {
      buffer->data[buffer->len - 1] = '\0';
      return pg_strdup(buffer->data);
    }
  }

  if (buffer->len > 0)                                           
  {
    return pg_strdup(buffer->data);
  }

                           
  return NULL;
}

#ifdef USE_READLINE

   
                                                                    
   
                                                                              
                                                                              
                                                                               
                                                                           
                                                                            
                                                                  
   
                                                                              
                                                                             
                                                                            
                                                                               
                                                                               
                                                                             
                                                                            
                                                                           
                                                                          
                                                                           
                                                                           
   
                     
   
                                    
      
                                          
      
                           
   
#define BEGIN_ITERATE_HISTORY(VARNAME)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    HIST_ENTRY *VARNAME;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    bool use_prev_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    history_set_pos(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    use_prev_ = (previous_history() != NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    history_set_pos(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    for (VARNAME = current_history(); VARNAME != NULL; VARNAME = use_prev_ ? previous_history() : next_history())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      (void)0

#define END_ITERATE_HISTORY()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  while (0)

   
                                                                              
   
static void
encode_history(void)
{
  BEGIN_ITERATE_HISTORY(cur_hist);
  {
    char *cur_ptr;

                                                                
    for (cur_ptr = (char *)cur_hist->line; *cur_ptr; cur_ptr++)
    {
      if (*cur_ptr == '\n')
      {
        *cur_ptr = NL_IN_HISTORY;
      }
    }
  }
  END_ITERATE_HISTORY();
}

   
                              
   
static void
decode_history(void)
{
  BEGIN_ITERATE_HISTORY(cur_hist);
  {
    char *cur_ptr;

                                                                
    for (cur_ptr = (char *)cur_hist->line; *cur_ptr; cur_ptr++)
    {
      if (*cur_ptr == NL_IN_HISTORY)
      {
        *cur_ptr = '\n';
      }
    }
  }
  END_ITERATE_HISTORY();
}
#endif                   

   
                                                                         
                         
   
                                                              
   
void
initializeInput(int flags)
{
#ifdef USE_READLINE
  if (flags & 1)
  {
    const char *histfile;
    char home[MAXPGPATH];

    useReadline = true;

                                                      
    initialize_readline();
    rl_initialize();

    useHistory = true;
    using_history();
    history_lines_added = 0;

    histfile = GetVariable(pset.vars, "HISTFILE");

    if (histfile == NULL)
    {
      char *envhist;

      envhist = getenv("PSQL_HISTORY");
      if (envhist != NULL && strlen(envhist) > 0)
      {
        histfile = envhist;
      }
    }

    if (histfile == NULL)
    {
      if (get_home_path(home))
      {
        psql_history = psprintf("%s/%s", home, PSQLHISTORY);
      }
    }
    else
    {
      psql_history = pg_strdup(histfile);
      expand_tilde(&psql_history);
    }

    if (psql_history)
    {
      read_history(psql_history);
      decode_history();
    }
  }
#endif

  atexit(finishInput);
}

   
                                                             
   
                                                                       
                                                                     
   
                                                                
   
#ifdef USE_READLINE
static bool
saveHistory(char *fname, int max_lines)
{
  int errnum;

     
                                                                         
                                                                            
                                                                        
           
     
  if (strcmp(fname, DEVNULL) != 0)
  {
       
                                                                         
                                                                          
                                                                           
                   
       
    encode_history();

       
                                                                      
                                                                         
                                                                        
                                                                          
                                                                    
       
#if defined(HAVE_HISTORY_TRUNCATE_FILE) && defined(HAVE_APPEND_HISTORY)
    {
      int nlines;
      int fd;

                                               
      if (max_lines >= 0)
      {
        nlines = Max(max_lines - history_lines_added, 0);
        (void)history_truncate_file(fname, nlines);
      }
                                                                  
      fd = open(fname, O_CREAT | O_WRONLY | PG_BINARY, 0600);
      if (fd >= 0)
      {
        close(fd);
      }
                                                  
      if (max_lines >= 0)
      {
        nlines = Min(max_lines, history_lines_added);
      }
      else
      {
        nlines = history_lines_added;
      }
      errnum = append_history(nlines, fname);
      if (errnum == 0)
      {
        return true;
      }
    }
#else                                
    {
                                     
      if (max_lines >= 0)
      {
        stifle_history(max_lines);
      }
                                                                        
      errnum = write_history(fname);
      if (errnum == 0)
      {
        return true;
      }
    }
#endif

    pg_log_error("could not save history to file \"%s\": %m", fname);
  }
  return false;
}
#endif

   
                                                                           
                     
   
                                                                          
                                                                          
                                                                           
                                  
   
bool
printHistory(const char *fname, unsigned short int pager)
{
#ifdef USE_READLINE
  FILE *output;
  bool is_pager;

  if (!useHistory)
  {
    return false;
  }

  if (fname == NULL)
  {
                                                         
    output = PageOutput(INT_MAX, pager ? &(pset.popt.topt) : NULL);
    is_pager = true;
  }
  else
  {
    output = fopen(fname, "w");
    if (output == NULL)
    {
      pg_log_error("could not save history to file \"%s\": %m", fname);
      return false;
    }
    is_pager = false;
  }

  BEGIN_ITERATE_HISTORY(cur_hist);
  {
    fprintf(output, "%s\n", cur_hist->line);
  }
  END_ITERATE_HISTORY();

  if (is_pager)
  {
    ClosePager(output);
  }
  else
  {
    fclose(output);
  }

  return true;
#else
  pg_log_error("history is not supported by this installation");
  return false;
#endif
}

static void
finishInput(void)
{
#ifdef USE_READLINE
  if (useHistory && psql_history)
  {
    (void)saveHistory(psql_history, pset.histsize);
    free(psql_history);
    psql_history = NULL;
  }
#endif
}
