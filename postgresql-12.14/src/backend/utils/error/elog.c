                                                                            
   
          
                                 
   
                                                                              
                                                                              
                                                                                
                                                                           
                                        
   
                                                                  
   
                                                                         
                                                                           
                                                           
   
                                                                       
                                                                       
                                                                     
                                                                         
                                                                              
                                                                              
                                                                               
                                                                           
                                                                          
                                                                      
   
                                                                         
                                                                         
                                                                        
                                                                        
                                                                      
                                                                          
                                                                               
                                                                         
                                                                          
                                                                            
                                                                          
                                                                      
                                                                          
                                                                      
              
   
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#include "access/transam.h"
#include "access/xact.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"

                                                        
#undef _
#define _(x) err_gettext(x)

                      
ErrorContextCallback *error_context_stack = NULL;

sigjmp_buf *PG_exception_stack = NULL;

extern bool redirection_done;

   
                                                                          
                                                                           
                                                                             
                                                                      
                      
   
emit_log_hook_type emit_log_hook = NULL;

                    
int Log_error_verbosity = PGERROR_VERBOSE;
char *Log_line_prefix = NULL;                                     
int Log_destination = LOG_DESTINATION_STDERR;
char *Log_destination_string = NULL;
bool syslog_sequence_numbers = true;
bool syslog_split_messages = true;

#ifdef HAVE_SYSLOG

   
                                                                            
                                                                            
                                                                            
                                                                            
                                                                   
   
#ifndef PG_SYSLOG_LIMIT
#define PG_SYSLOG_LIMIT 900
#endif

static bool openlog_done = false;
static char *syslog_ident = NULL;
static int syslog_facility = LOG_LOCAL0;

static void
write_syslog(int level, const char *line);
#endif

#ifdef WIN32
extern char *event_source;

static void
write_eventlog(int level, const char *line, int len);
#endif

                                                                        
#define ERRORDATA_STACK_SIZE 5

static ErrorData errordata[ERRORDATA_STACK_SIZE];

static int errordata_stack_depth = -1;                                    

static int recursion_depth = 0;                                 

   
                                                                            
                                      
   
static struct timeval saved_timeval;
static bool saved_timeval_set = false;

#define FORMATTED_TS_LEN 128
static char formatted_start_time[FORMATTED_TS_LEN];
static char formatted_log_time[FORMATTED_TS_LEN];

                                                            
#define CHECK_STACK_DEPTH()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (errordata_stack_depth < 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      errordata_stack_depth = -1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      ereport(ERROR, (errmsg_internal("errstart was not called")));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

static const char *
err_gettext(const char *str) pg_attribute_format_arg(1);
static void
set_errdata_field(MemoryContextData *cxt, char **ptr, const char *str);
static void
write_console(const char *line, int len);
static void
setup_formatted_log_time(void);
static void
setup_formatted_start_time(void);
static const char *
process_log_prefix_padding(const char *p, int *padding);
static void
log_line_prefix(StringInfo buf, ErrorData *edata);
static void
write_csvlog(ErrorData *edata);
static void
send_message_to_server_log(ErrorData *edata);
static void
write_pipe_chunks(char *data, int len, int dest);
static void
send_message_to_frontend(ErrorData *edata);
static const char *
error_severity(int elevel);
static void
append_with_tabs(StringInfo buf, const char *str);
static bool
is_log_level_output(int elevel, int log_min_level);

   
                                                                              
   
                                                                            
                                                                             
                        
   
bool
in_error_recursion_trouble(void)
{
                                               
  return (recursion_depth > 2);
}

   
                                                                       
                                                                        
                                 
   
static inline const char *
err_gettext(const char *str)
{
#ifdef ENABLE_NLS
  if (in_error_recursion_trouble())
  {
    return str;
  }
  else
  {
    return gettext(str);
  }
#else
  return str;
#endif
}

   
                                               
   
                                                                             
                                                                          
                                                                             
                     
   
                                                                          
                                                                        
   
bool
errstart(int elevel, const char *filename, int lineno, const char *funcname, const char *domain)
{
  ErrorData *edata;
  bool output_to_server;
  bool output_to_client = false;
  int i;

     
                                                                       
                                                                       
     
  if (elevel >= ERROR)
  {
       
                                                                    
                                 
       
    if (CritSectionCount > 0)
    {
      elevel = PANIC;
    }

       
                                                  
       
                                                                         
                                          
       
                                                                
       
                                                                      
                                                                     
                            
       
    if (elevel == ERROR)
    {
      if (PG_exception_stack == NULL || ExitOnAnyError || proc_exit_inprogress)
      {
        elevel = FATAL;
      }
    }

       
                                                                      
                                                                          
                                                                          
                                                                         
                                                                           
                                                               
       
    for (i = 0; i <= errordata_stack_depth; i++)
    {
      elevel = Max(elevel, errordata[i].elevel);
    }
  }

     
                                                                       
                                                                            
                                              
     

                                                                  
  output_to_server = is_log_level_output(elevel, log_min_messages);

                                                              
  if (whereToSendOutput == DestRemote && elevel != LOG_SERVER_ONLY)
  {
       
                                                                 
                                                                     
                                                                     
                              
       
    if (ClientAuthInProgress)
    {
      output_to_client = (elevel >= ERROR);
    }
    else
    {
      output_to_client = (elevel >= client_min_messages || elevel == INFO);
    }
  }

                                                                      
  if (elevel < ERROR && !output_to_server && !output_to_client)
  {
    return false;
  }

     
                                                                    
                                                                    
     
  if (ErrorContext == NULL)
  {
                                                                  
    write_stderr("error occurred at %s:%d before error message processing is available\n", filename ? filename : "(unknown file)", lineno);
    exit(2);
  }

     
                                                        
     

  if (recursion_depth++ > 0 && elevel >= ERROR)
  {
       
                                                                   
                                                                     
                                                         
       
    MemoryContextReset(ErrorContext);

       
                                                                      
                                                                      
                                                                      
                                                       
       
    if (in_error_recursion_trouble())
    {
      error_context_stack = NULL;
      debug_query_string = NULL;
    }
  }
  if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
  {
       
                                                                       
                                                                   
                 
       
    errordata_stack_depth = -1;                         
    ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
  }

                                            
  edata = &errordata[errordata_stack_depth];
  MemSet(edata, 0, sizeof(ErrorData));
  edata->elevel = elevel;
  edata->output_to_server = output_to_server;
  edata->output_to_client = output_to_client;
  if (filename)
  {
    const char *slash;

                                                                 
    slash = strrchr(filename, '/');
    if (slash)
    {
      filename = slash + 1;
    }
  }
  edata->filename = filename;
  edata->lineno = lineno;
  edata->funcname = funcname;
                                                
  edata->domain = domain ? domain : PG_TEXTDOMAIN("postgres");
                                                                            
  edata->context_domain = edata->domain;
                                              
  if (elevel >= ERROR)
  {
    edata->sqlerrcode = ERRCODE_INTERNAL_ERROR;
  }
  else if (elevel == WARNING)
  {
    edata->sqlerrcode = ERRCODE_WARNING;
  }
  else
  {
    edata->sqlerrcode = ERRCODE_SUCCESSFUL_COMPLETION;
  }
                                                                        
  edata->saved_errno = errno;

     
                                                                            
     
  edata->assoc_context = ErrorContext;

  recursion_depth--;
  return true;
}

   
                                              
   
                                                                    
   
                                                                       
                                               
   
void
errfinish(int dummy, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  int elevel;
  MemoryContext oldcontext;
  ErrorContextCallback *econtext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  elevel = edata->elevel;

     
                                                                            
                         
     
  oldcontext = MemoryContextSwitchTo(ErrorContext);

     
                                                                        
                                                                            
                                              
     
  for (econtext = error_context_stack; econtext != NULL; econtext = econtext->previous)
  {
    econtext->callback(econtext->arg);
  }

     
                                                                         
                                                                             
     
  if (elevel == ERROR)
  {
       
                                                                          
                                           
       
                                                                       
                                                                        
                                                                         
                                                                         
                                          
       
    InterruptHoldoffCount = 0;
    QueryCancelHoldoffCount = 0;

    CritSectionCount = 0;                                    

       
                                                                        
                                                       
       

    recursion_depth--;
    PG_RE_THROW();
  }

     
                                                                     
                                                                         
                                                                          
                                                                           
                                                                     
     
  if (elevel >= FATAL && whereToSendOutput == DestRemote)
  {
    pq_endcopyout(true);
  }

                                            
  EmitErrorReport();

                                                                           
  if (edata->message)
  {
    pfree(edata->message);
  }
  if (edata->detail)
  {
    pfree(edata->detail);
  }
  if (edata->detail_log)
  {
    pfree(edata->detail_log);
  }
  if (edata->hint)
  {
    pfree(edata->hint);
  }
  if (edata->context)
  {
    pfree(edata->context);
  }
  if (edata->schema_name)
  {
    pfree(edata->schema_name);
  }
  if (edata->table_name)
  {
    pfree(edata->table_name);
  }
  if (edata->column_name)
  {
    pfree(edata->column_name);
  }
  if (edata->datatype_name)
  {
    pfree(edata->datatype_name);
  }
  if (edata->constraint_name)
  {
    pfree(edata->constraint_name);
  }
  if (edata->internalquery)
  {
    pfree(edata->internalquery);
  }

  errordata_stack_depth--;

                                   
  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;

     
                                                           
     
  if (elevel == FATAL)
  {
       
                                                              
       
                                                                         
                                                              
       
    if (PG_exception_stack == NULL && whereToSendOutput == DestRemote)
    {
      whereToSendOutput = DestNone;
    }

       
                                                                      
                                                                          
                                                                          
                                                             
       
    fflush(stdout);
    fflush(stderr);

       
                                                                           
                                                                       
                                                                  
       
    proc_exit(1);
  }

  if (elevel >= PANIC)
  {
       
                                                                        
                                               
       
                                                                        
                   
       
    fflush(stdout);
    fflush(stderr);
    abort();
  }

     
                                                                           
                                                                            
                                                             
     
  CHECK_FOR_INTERRUPTS();
}

   
                                                            
   
                                                                  
   
int
errcode(int sqlerrcode)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  edata->sqlerrcode = sqlerrcode;

  return 0;                                   
}

   
                                                                            
   
                                                                          
                                                                 
   
                                                                      
                      
   
int
errcode_for_file_access(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  switch (edata->saved_errno)
  {
                                    
  case EPERM:                      
  case EACCES:                        
#ifdef EROFS
  case EROFS:                            
#endif
    edata->sqlerrcode = ERRCODE_INSUFFICIENT_PRIVILEGE;
    break;

                        
  case ENOENT:                                
    edata->sqlerrcode = ERRCODE_UNDEFINED_FILE;
    break;

                        
  case EEXIST:                  
    edata->sqlerrcode = ERRCODE_DUPLICATE_FILE;
    break;

                                    
  case ENOTDIR:                                                      
  case EISDIR:                                                      
#if defined(ENOTEMPTY) && (ENOTEMPTY != EEXIST)                       
  case ENOTEMPTY:                                                        
#endif
    edata->sqlerrcode = ERRCODE_WRONG_OBJECT_TYPE;
    break;

                                
  case ENOSPC:                              
    edata->sqlerrcode = ERRCODE_DISK_FULL;
    break;

  case ENFILE:                          
  case EMFILE:                          
    edata->sqlerrcode = ERRCODE_INSUFFICIENT_RESOURCES;
    break;

                          
  case EIO:                
    edata->sqlerrcode = ERRCODE_IO_ERROR;
    break;

                                                   
  default:
    edata->sqlerrcode = ERRCODE_INTERNAL_ERROR;
    break;
  }

  return 0;                                   
}

   
                                                                              
   
                                                                          
                                                              
   
                                                                      
                      
   
int
errcode_for_socket_access(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  switch (edata->saved_errno)
  {
                            
  case EPIPE:
#ifdef ECONNRESET
  case ECONNRESET:
#endif
    edata->sqlerrcode = ERRCODE_CONNECTION_FAILURE;
    break;

                                                   
  default:
    edata->sqlerrcode = ERRCODE_INTERNAL_ERROR;
    break;
  }

  return 0;                                   
}

   
                                                                              
                                                                           
                                                                           
                                                                      
                                                                               
                           
   
                                                                         
                                                                           
                     
   
#define EVALUATE_MESSAGE(domain, targetfield, appendval, translateit)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    StringInfoData buf;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if ((translateit) && !in_error_recursion_trouble())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      fmt = dgettext((domain), fmt);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    initStringInfo(&buf);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    if ((appendval) && edata->targetfield)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      appendStringInfoString(&buf, edata->targetfield);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      appendStringInfoChar(&buf, '\n');                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    for (;;)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      va_list args;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      int needed;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      errno = edata->saved_errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      va_start(args, fmt);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      needed = appendStringInfoVA(&buf, fmt, args);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      va_end(args);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      if (needed == 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      enlargeStringInfo(&buf, needed);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (edata->targetfield)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      pfree(edata->targetfield);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    edata->targetfield = pstrdup(buf.data);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    pfree(buf.data);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  }

   
                                                                             
                                                                            
                                                                 
   
#define EVALUATE_MESSAGE_PLURAL(domain, targetfield, appendval)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    const char *fmt;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    StringInfoData buf;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (!in_error_recursion_trouble())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      fmt = dngettext((domain), fmt_singular, fmt_plural, n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      fmt = (n == 1 ? fmt_singular : fmt_plural);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    initStringInfo(&buf);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    if ((appendval) && edata->targetfield)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      appendStringInfoString(&buf, edata->targetfield);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      appendStringInfoChar(&buf, '\n');                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    for (;;)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      va_list args;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      int needed;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      errno = edata->saved_errno;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      va_start(args, n);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      needed = appendStringInfoVA(&buf, fmt, args);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      va_end(args);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      if (needed == 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      enlargeStringInfo(&buf, needed);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (edata->targetfield)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
      pfree(edata->targetfield);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    edata->targetfield = pstrdup(buf.data);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    pfree(buf.data);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  }

   
                                                                    
   
                                                                    
                                                                         
   
                                                                  
                                                                 
   
int
errmsg(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  edata->message_id = fmt;
  EVALUATE_MESSAGE(edata->domain, message, false, true);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                             
   
                                                                               
                                                           
                                                                            
                                                                             
                                                                           
                                                                         
                    
   
int
errmsg_internal(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  edata->message_id = fmt;
  EVALUATE_MESSAGE(edata->domain, message, false, false);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                            
                                                      
   
int
errmsg_plural(const char *fmt_singular, const char *fmt_plural, unsigned long n, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  edata->message_id = fmt_singular;
  EVALUATE_MESSAGE_PLURAL(edata->domain, message, false);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                      
   
int
errdetail(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE(edata->domain, detail, false, true);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                               
   
                                                                  
                                                                              
                                                                            
                                                                      
                                                                    
   
int
errdetail_internal(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE(edata->domain, detail, false, false);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                              
   
int
errdetail_log(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE(edata->domain, detail_log, false, true);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                                     
                                                      
   
int
errdetail_log_plural(const char *fmt_singular, const char *fmt_plural, unsigned long n, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE_PLURAL(edata->domain, detail_log, false);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                              
                                                      
   
int
errdetail_plural(const char *fmt_singular, const char *fmt_plural, unsigned long n, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE_PLURAL(edata->domain, detail, false);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                  
   
int
errhint(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE(edata->domain, hint, false, true);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                            
   
                                                                         
                                                                               
           
   
int
errcontext_msg(const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  EVALUATE_MESSAGE(edata->context_domain, context, true, true);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
  return 0;                                   
}

   
                                                                           
   
                                                                            
                                                                          
                                                                            
                                                                          
                                                 
   
int
set_errcontext_domain(const char *domain)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

                                                
  edata->context_domain = domain ? domain : PG_TEXTDOMAIN("postgres");

  return 0;                                   
}

   
                                                                     
   
                                                                             
   
int
errhidestmt(bool hide_stmt)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  edata->hide_stmt = hide_stmt;

  return 0;                                   
}

   
                                                                      
   
                                                                              
                                                             
   
int
errhidecontext(bool hide_ctx)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  edata->hide_ctx = hide_ctx;

  return 0;                                   
}

   
                                                                    
   
                                                                       
                                                                        
                                                                   
   
int
errfunction(const char *funcname)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  edata->funcname = funcname;
  edata->show_funcname = true;

  return 0;                                   
}

   
                                                            
   
int
errposition(int cursorpos)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  edata->cursorpos = cursorpos;

  return 0;                                   
}

   
                                                                             
   
int
internalerrposition(int cursorpos)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  edata->internalpos = cursorpos;

  return 0;                                   
}

   
                                                                     
   
                                                                        
                                                                             
                                      
   
int
internalerrquery(const char *query)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  if (edata->internalquery)
  {
    pfree(edata->internalquery);
    edata->internalquery = NULL;
  }

  if (query)
  {
    edata->internalquery = MemoryContextStrdup(edata->assoc_context, query);
  }

  return 0;                                   
}

   
                                                                        
                                    
   
                                                                             
                                                    
   
                                                                           
                                                                      
   
int
err_generic_string(int field, const char *str)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  switch (field)
  {
  case PG_DIAG_SCHEMA_NAME:
    set_errdata_field(edata->assoc_context, &edata->schema_name, str);
    break;
  case PG_DIAG_TABLE_NAME:
    set_errdata_field(edata->assoc_context, &edata->table_name, str);
    break;
  case PG_DIAG_COLUMN_NAME:
    set_errdata_field(edata->assoc_context, &edata->column_name, str);
    break;
  case PG_DIAG_DATATYPE_NAME:
    set_errdata_field(edata->assoc_context, &edata->datatype_name, str);
    break;
  case PG_DIAG_CONSTRAINT_NAME:
    set_errdata_field(edata->assoc_context, &edata->constraint_name, str);
    break;
  default:
    elog(ERROR, "unsupported ErrorData field id: %d", field);
    break;
  }

  return 0;                                   
}

   
                                                       
   
static void
set_errdata_field(MemoryContextData *cxt, char **ptr, const char *str)
{
  Assert(*ptr == NULL);
  *ptr = MemoryContextStrdup(cxt, str);
}

   
                                                               
   
                                                                            
                                                                     
   
int
geterrcode(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  return edata->sqlerrcode;
}

   
                                                                          
   
                                                                            
                                                                     
   
int
geterrposition(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  return edata->cursorpos;
}

   
                                                               
   
                                                                            
                                                                     
   
int
getinternalerrposition(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];

                                                    
  CHECK_STACK_DEPTH();

  return edata->internalpos;
}

   
                                            
   
                                                                    
                                                                        
   
                                                                         
                                                                          
                                                                           
                                                                             
                                                   
   
void
elog_start(const char *filename, int lineno, const char *funcname)
{
  ErrorData *edata;

                                                                 
  if (ErrorContext == NULL)
  {
                                                                  
    write_stderr("error occurred at %s:%d before error message processing is available\n", filename ? filename : "(unknown file)", lineno);
    exit(2);
  }

  if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
  {
       
                                                                       
                                                                   
                                                                        
                                                                         
                  
       
    errordata_stack_depth = -1;                         
    ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
  }

  edata = &errordata[errordata_stack_depth];
  if (filename)
  {
    const char *slash;

                                                                 
    slash = strrchr(filename, '/');
    if (slash)
    {
      filename = slash + 1;
    }
  }
  edata->filename = filename;
  edata->lineno = lineno;
  edata->funcname = funcname;
                                                                       
  edata->saved_errno = errno;

                                                                
  edata->assoc_context = ErrorContext;
}

   
                                               
   
void
elog_finish(int elevel, const char *fmt, ...)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  CHECK_STACK_DEPTH();

     
                                                                     
     
  errordata_stack_depth--;
  errno = edata->saved_errno;
  if (!errstart(elevel, edata->filename, edata->lineno, edata->funcname, NULL))
  {
    return;                    
  }

     
                                                       
     
  recursion_depth++;
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

  edata->message_id = fmt;
  EVALUATE_MESSAGE(edata->domain, message, false, false);

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;

     
                                    
     
  errfinish(0);
}

   
                                                                            
                              
   
                                      
   
                                                                               
   
                                                                         
                                                                             
                                                                              
                                                                              
                                                                           
                   
   
                                                                          
                                                        
   
static int save_format_errnumber;
static const char *save_format_domain;

void
pre_format_elog_string(int errnumber, const char *domain)
{
                                                                        
  save_format_errnumber = errnumber;
                                 
  save_format_domain = domain;
}

char *
format_elog_string(const char *fmt, ...)
{
  ErrorData errdata;
  ErrorData *edata;
  MemoryContext oldcontext;

                                             
  edata = &errdata;
  MemSet(edata, 0, sizeof(ErrorData));
                                                
  edata->domain = save_format_domain ? save_format_domain : PG_TEXTDOMAIN("postgres");
                                                
  edata->saved_errno = save_format_errnumber;

  oldcontext = MemoryContextSwitchTo(ErrorContext);

  edata->message_id = fmt;
  EVALUATE_MESSAGE(edata->domain, message, false, true);

  MemoryContextSwitchTo(oldcontext);

  return edata->message;
}

   
                                                   
   
                                                                               
                                                                            
                           
   
void
EmitErrorReport(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  MemoryContext oldcontext;

  recursion_depth++;
  CHECK_STACK_DEPTH();
  oldcontext = MemoryContextSwitchTo(edata->assoc_context);

     
                                                                            
                                                                             
                                                                       
                
     
                                                                           
                                                                           
                                                                           
                                                                          
                                                                            
                                                                           
                 
     
                                                                         
                                                                             
                                                                          
                                                         
     
  if (edata->output_to_server && emit_log_hook)
  {
    (*emit_log_hook)(edata);
  }

                                      
  if (edata->output_to_server)
  {
    send_message_to_server_log(edata);
  }

                                  
  if (edata->output_to_client)
  {
    send_message_to_frontend(edata);
  }

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;
}

   
                                                                    
   
                                                                            
                                                                     
                                                                               
   
ErrorData *
CopyErrorData(void)
{
  ErrorData *edata = &errordata[errordata_stack_depth];
  ErrorData *newedata;

     
                                                                            
                                                    
     
  CHECK_STACK_DEPTH();

  Assert(CurrentMemoryContext != ErrorContext);

                              
  newedata = (ErrorData *)palloc(sizeof(ErrorData));
  memcpy(newedata, edata, sizeof(ErrorData));

                                                  
  if (newedata->message)
  {
    newedata->message = pstrdup(newedata->message);
  }
  if (newedata->detail)
  {
    newedata->detail = pstrdup(newedata->detail);
  }
  if (newedata->detail_log)
  {
    newedata->detail_log = pstrdup(newedata->detail_log);
  }
  if (newedata->hint)
  {
    newedata->hint = pstrdup(newedata->hint);
  }
  if (newedata->context)
  {
    newedata->context = pstrdup(newedata->context);
  }
  if (newedata->schema_name)
  {
    newedata->schema_name = pstrdup(newedata->schema_name);
  }
  if (newedata->table_name)
  {
    newedata->table_name = pstrdup(newedata->table_name);
  }
  if (newedata->column_name)
  {
    newedata->column_name = pstrdup(newedata->column_name);
  }
  if (newedata->datatype_name)
  {
    newedata->datatype_name = pstrdup(newedata->datatype_name);
  }
  if (newedata->constraint_name)
  {
    newedata->constraint_name = pstrdup(newedata->constraint_name);
  }
  if (newedata->internalquery)
  {
    newedata->internalquery = pstrdup(newedata->internalquery);
  }

                                                     
  newedata->assoc_context = CurrentMemoryContext;

  return newedata;
}

   
                                                                   
   
                                                                          
                                    
   
void
FreeErrorData(ErrorData *edata)
{
  if (edata->message)
  {
    pfree(edata->message);
  }
  if (edata->detail)
  {
    pfree(edata->detail);
  }
  if (edata->detail_log)
  {
    pfree(edata->detail_log);
  }
  if (edata->hint)
  {
    pfree(edata->hint);
  }
  if (edata->context)
  {
    pfree(edata->context);
  }
  if (edata->schema_name)
  {
    pfree(edata->schema_name);
  }
  if (edata->table_name)
  {
    pfree(edata->table_name);
  }
  if (edata->column_name)
  {
    pfree(edata->column_name);
  }
  if (edata->datatype_name)
  {
    pfree(edata->datatype_name);
  }
  if (edata->constraint_name)
  {
    pfree(edata->constraint_name);
  }
  if (edata->internalquery)
  {
    pfree(edata->internalquery);
  }
  pfree(edata);
}

   
                                                                  
   
                                                                        
                                                                      
                                                                           
                                                 
   
void
FlushErrorState(void)
{
     
                                                                          
                                                                      
                                                                     
                                          
     
  errordata_stack_depth = -1;
  recursion_depth = 0;
                                       
  MemoryContextResetAndDeleteChildren(ErrorContext);
}

   
                                                                          
   
                                                                            
                                                                       
                                                                    
                                                                     
                                                                    
                                                      
   
void
ThrowErrorData(ErrorData *edata)
{
  ErrorData *newedata;
  MemoryContext oldcontext;

  if (!errstart(edata->elevel, edata->filename, edata->lineno, edata->funcname, NULL))
  {
    return;                                         
  }

  newedata = &errordata[errordata_stack_depth];
  recursion_depth++;
  oldcontext = MemoryContextSwitchTo(newedata->assoc_context);

                                                          
  if (edata->sqlerrcode != 0)
  {
    newedata->sqlerrcode = edata->sqlerrcode;
  }
  if (edata->message)
  {
    newedata->message = pstrdup(edata->message);
  }
  if (edata->detail)
  {
    newedata->detail = pstrdup(edata->detail);
  }
  if (edata->detail_log)
  {
    newedata->detail_log = pstrdup(edata->detail_log);
  }
  if (edata->hint)
  {
    newedata->hint = pstrdup(edata->hint);
  }
  if (edata->context)
  {
    newedata->context = pstrdup(edata->context);
  }
                                          
  if (edata->schema_name)
  {
    newedata->schema_name = pstrdup(edata->schema_name);
  }
  if (edata->table_name)
  {
    newedata->table_name = pstrdup(edata->table_name);
  }
  if (edata->column_name)
  {
    newedata->column_name = pstrdup(edata->column_name);
  }
  if (edata->datatype_name)
  {
    newedata->datatype_name = pstrdup(edata->datatype_name);
  }
  if (edata->constraint_name)
  {
    newedata->constraint_name = pstrdup(edata->constraint_name);
  }
  newedata->cursorpos = edata->cursorpos;
  newedata->internalpos = edata->internalpos;
  if (edata->internalquery)
  {
    newedata->internalquery = pstrdup(edata->internalquery);
  }

  MemoryContextSwitchTo(oldcontext);
  recursion_depth--;

                          
  errfinish(0);
}

   
                                                       
   
                                                                          
                                                                            
                                                                          
                                                                      
   
void
ReThrowError(ErrorData *edata)
{
  ErrorData *newedata;

  Assert(edata->elevel == ERROR);

                                                 
  recursion_depth++;
  MemoryContextSwitchTo(ErrorContext);

  if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
  {
       
                                                                       
                                                                   
                 
       
    errordata_stack_depth = -1;                         
    ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
  }

  newedata = &errordata[errordata_stack_depth];
  memcpy(newedata, edata, sizeof(ErrorData));

                                                  
  if (newedata->message)
  {
    newedata->message = pstrdup(newedata->message);
  }
  if (newedata->detail)
  {
    newedata->detail = pstrdup(newedata->detail);
  }
  if (newedata->detail_log)
  {
    newedata->detail_log = pstrdup(newedata->detail_log);
  }
  if (newedata->hint)
  {
    newedata->hint = pstrdup(newedata->hint);
  }
  if (newedata->context)
  {
    newedata->context = pstrdup(newedata->context);
  }
  if (newedata->schema_name)
  {
    newedata->schema_name = pstrdup(newedata->schema_name);
  }
  if (newedata->table_name)
  {
    newedata->table_name = pstrdup(newedata->table_name);
  }
  if (newedata->column_name)
  {
    newedata->column_name = pstrdup(newedata->column_name);
  }
  if (newedata->datatype_name)
  {
    newedata->datatype_name = pstrdup(newedata->datatype_name);
  }
  if (newedata->constraint_name)
  {
    newedata->constraint_name = pstrdup(newedata->constraint_name);
  }
  if (newedata->internalquery)
  {
    newedata->internalquery = pstrdup(newedata->internalquery);
  }

                                                  
  newedata->assoc_context = ErrorContext;

  recursion_depth--;
  PG_RE_THROW();
}

   
                                                                     
   
void
pg_re_throw(void)
{
                                                                     
  if (PG_exception_stack != NULL)
  {
    siglongjmp(*PG_exception_stack, 1);
  }
  else
  {
       
                                                                           
                                                                         
                                                                        
                                                                          
                                                                          
                            
       
    ErrorData *edata = &errordata[errordata_stack_depth];

    Assert(errordata_stack_depth >= 0);
    Assert(edata->elevel == ERROR);
    edata->elevel = FATAL;

       
                                                                          
                                                                       
                                                     
       
    if (IsPostmasterEnvironment)
    {
      edata->output_to_server = is_log_level_output(FATAL, log_min_messages);
    }
    else
    {
      edata->output_to_server = (FATAL >= log_min_messages);
    }
    if (whereToSendOutput == DestRemote)
    {
      edata->output_to_client = true;
    }

       
                                                                         
                                                                       
                                                                       
       
    error_context_stack = NULL;

    errfinish(0);
  }

                          
  ExceptionalCondition("pg_re_throw tried to return", "FailedAssertion", __FILE__, __LINE__);
}

   
                                                                      
   
                                                                            
                                                                              
                                                     
   
                                                                              
                                                                       
                                                                       
   
char *
GetErrorContextStack(void)
{
  ErrorData *edata;
  ErrorContextCallback *econtext;

     
                                                        
     
  recursion_depth++;

  if (++errordata_stack_depth >= ERRORDATA_STACK_SIZE)
  {
       
                                                                       
                                                                   
                 
       
    errordata_stack_depth = -1;                         
    ereport(PANIC, (errmsg_internal("ERRORDATA_STACK_SIZE exceeded")));
  }

     
                                                            
     
  edata = &errordata[errordata_stack_depth];
  MemSet(edata, 0, sizeof(ErrorData));

     
                                                                         
                                                                      
     
  edata->assoc_context = CurrentMemoryContext;

     
                                                                            
                          
     
                                                                          
                                                                             
                                       
     
  for (econtext = error_context_stack; econtext != NULL; econtext = econtext->previous)
  {
    econtext->callback(econtext->arg);
  }

     
                                                                          
                                                                            
                                                                       
                                         
     
  errordata_stack_depth--;
  recursion_depth--;

     
                                                                            
                                      
     
  return edata->context;
}

   
                                       
   
void
DebugFileOpen(void)
{
  int fd, istty;

  if (OutputFileName[0])
  {
       
                                           
       
                                                                    
       
    if ((fd = open(OutputFileName, O_CREAT | O_APPEND | O_WRONLY, 0666)) < 0)
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", OutputFileName)));
    }
    istty = isatty(fd);
    close(fd);

       
                                                     
       
    if (!freopen(OutputFileName, "a", stderr))
    {
      ereport(FATAL, (errcode_for_file_access(), errmsg("could not reopen file \"%s\" as stderr: %m", OutputFileName)));
    }

       
                                                                           
                                                                           
                                                                         
                
       
    if (istty && IsUnderPostmaster)
    {
      if (!freopen(OutputFileName, "a", stdout))
      {
        ereport(FATAL, (errcode_for_file_access(), errmsg("could not reopen file \"%s\" as stdout: %m", OutputFileName)));
      }
    }
  }
}

#ifdef HAVE_SYSLOG

   
                                                   
   
void
set_syslog_parameters(const char *ident, int facility)
{
     
                                                                          
                                                                          
                                                                             
                                                      
     
                                                                            
                                                                             
                                                            
     
  if (syslog_ident == NULL || strcmp(syslog_ident, ident) != 0 || syslog_facility != facility)
  {
    if (openlog_done)
    {
      closelog();
      openlog_done = false;
    }
    if (syslog_ident)
    {
      free(syslog_ident);
    }
    syslog_ident = strdup(ident);
                                                             
    syslog_facility = facility;
  }
}

   
                                  
   
static void
write_syslog(int level, const char *line)
{
  static unsigned long seq = 0;

  int len;
  const char *nlpos;

                                              
  if (!openlog_done)
  {
    openlog(syslog_ident ? syslog_ident : "postgres", LOG_PID | LOG_NDELAY | LOG_NOWAIT, syslog_facility);
    openlog_done = true;
  }

     
                                                                     
               
     
  seq++;

     
                                                                            
                                                                             
                                                                             
     
                                                                             
                                           
     
  len = strlen(line);
  nlpos = strchr(line, '\n');
  if (syslog_split_messages && (len > PG_SYSLOG_LIMIT || nlpos != NULL))
  {
    int chunk_nr = 0;

    while (len > 0)
    {
      char buf[PG_SYSLOG_LIMIT + 1];
      int buflen;
      int i;

                                                         
      if (line[0] == '\n')
      {
        line++;
        len--;
                                                                   
        nlpos = strchr(line, '\n');
        continue;
      }

                                                         
      if (nlpos != NULL)
      {
        buflen = nlpos - line;
      }
      else
      {
        buflen = len;
      }
      buflen = Min(buflen, PG_SYSLOG_LIMIT);
      memcpy(buf, line, buflen);
      buf[buflen] = '\0';

                                             
      buflen = pg_mbcliplen(buf, buflen, buflen);
      if (buflen <= 0)
      {
        return;
      }
      buf[buflen] = '\0';

                                  
      if (line[buflen] != '\0' && !isspace((unsigned char)line[buflen]))
      {
                                            
        i = buflen - 1;
        while (i > 0 && !isspace((unsigned char)buf[i]))
        {
          i--;
        }

        if (i > 0)                                         
        {
          buflen = i;
          buf[i] = '\0';
        }
      }

      chunk_nr++;

      if (syslog_sequence_numbers)
      {
        syslog(level, "[%lu-%d] %s", seq, chunk_nr, buf);
      }
      else
      {
        syslog(level, "[%d] %s", chunk_nr, buf);
      }

      line += buflen;
      len -= buflen;
    }
  }
  else
  {
                              
    if (syslog_sequence_numbers)
    {
      syslog(level, "[%lu] %s", seq, line);
    }
    else
    {
      syslog(level, "%s", line);
    }
  }
}
#endif                  

#ifdef WIN32
   
                                                                               
                                                                             
                                                                          
   
static int
GetACPEncoding(void)
{
  static int encoding = -2;

  if (encoding == -2)
  {
    encoding = pg_codepage_to_encoding(GetACP());
  }

  return encoding;
}

   
                                                 
   
static void
write_eventlog(int level, const char *line, int len)
{
  WCHAR *utf16;
  int eventlevel = EVENTLOG_ERROR_TYPE;
  static HANDLE evtHandle = INVALID_HANDLE_VALUE;

  if (evtHandle == INVALID_HANDLE_VALUE)
  {
    evtHandle = RegisterEventSource(NULL, event_source ? event_source : DEFAULT_EVENT_SOURCE);
    if (evtHandle == NULL)
    {
      evtHandle = INVALID_HANDLE_VALUE;
      return;
    }
  }

  switch (level)
  {
  case DEBUG5:
  case DEBUG4:
  case DEBUG3:
  case DEBUG2:
  case DEBUG1:
  case LOG:
  case LOG_SERVER_ONLY:
  case INFO:
  case NOTICE:
    eventlevel = EVENTLOG_INFORMATION_TYPE;
    break;
  case WARNING:
    eventlevel = EVENTLOG_WARNING_TYPE;
    break;
  case ERROR:
  case FATAL:
  case PANIC:
  default:
    eventlevel = EVENTLOG_ERROR_TYPE;
    break;
  }

     
                                                                    
                                                                             
                                                                           
                                                       
     
                                                                      
                                                              
                           
     
                                                                             
                                                                      
     
  if (!in_error_recursion_trouble() && CurrentMemoryContext != NULL && GetMessageEncoding() != GetACPEncoding())
  {
    utf16 = pgwin32_message_to_UTF16(line, len, NULL);
    if (utf16)
    {
      ReportEventW(evtHandle, eventlevel, 0, 0,                          
          NULL, 1, 0, (LPCWSTR *)&utf16, NULL);
                                                             

      pfree(utf16);
      return;
    }
  }
  ReportEventA(evtHandle, eventlevel, 0, 0,                          
      NULL, 1, 0, &line, NULL);
}
#endif            

static void
write_console(const char *line, int len)
{
  int rc;

#ifdef WIN32

     
                                                                            
                                             
     
                                                                           
                                                                           
                                                                       
                                                                       
                                   
     
                                                                             
                                                         
     
                                                                      
                                                              
                           
     
  if (!in_error_recursion_trouble() && !redirection_done && CurrentMemoryContext != NULL)
  {
    WCHAR *utf16;
    int utf16len;

    utf16 = pgwin32_message_to_UTF16(line, len, &utf16len);
    if (utf16 != NULL)
    {
      HANDLE stdHandle;
      DWORD written;

      stdHandle = GetStdHandle(STD_ERROR_HANDLE);
      if (WriteConsoleW(stdHandle, utf16, utf16len, &written, NULL))
      {
        pfree(utf16);
        return;
      }

         
                                                                  
                              
         
      pfree(utf16);
    }
  }
#else

     
                                                                           
                                                                     
                                                     
     
#endif

     
                                                                             
                                                                       
     
  rc = write(fileno(stderr), line, len);
  (void)rc;
}

   
                                                                               
   
static void
setup_formatted_log_time(void)
{
  pg_time_t stamp_time;
  char msbuf[13];

  if (!saved_timeval_set)
  {
    gettimeofday(&saved_timeval, NULL);
    saved_timeval_set = true;
  }

  stamp_time = (pg_time_t)saved_timeval.tv_sec;

     
                                                                            
                                                                       
                                           
     
  pg_strftime(formatted_log_time, FORMATTED_TS_LEN,
                                          
      "%Y-%m-%d %H:%M:%S     %Z", pg_localtime(&stamp_time, log_timezone));

                                          
  sprintf(msbuf, ".%03d", (int)(saved_timeval.tv_usec / 1000));
  memcpy(formatted_log_time + 19, msbuf, 4);
}

   
                              
   
static void
setup_formatted_start_time(void)
{
  pg_time_t stamp_time = (pg_time_t)MyStartTime;

     
                                                                            
                                                                       
                                           
     
  pg_strftime(formatted_start_time, FORMATTED_TS_LEN, "%Y-%m-%d %H:%M:%S %Z", pg_localtime(&stamp_time, log_timezone));
}

   
                                                                            
                             
   
                                                                
                                          
   
static const char *
process_log_prefix_padding(const char *p, int *ppadding)
{
  int paddingsign = 1;
  int padding = 0;

  if (*p == '-')
  {
    p++;

    if (*p == '\0')                              
    {
      return NULL;
    }
    paddingsign = -1;
  }

                                                       
  while (*p >= '0' && *p <= '9')
  {
    padding = padding * 10 + (*p++ - '0');
  }

                                                            
  if (*p == '\0')
  {
    return NULL;
  }

  padding *= paddingsign;
  *ppadding = padding;
  return p;
}

   
                                                                 
   
static void
log_line_prefix(StringInfo buf, ErrorData *edata)
{
                                       
  static long log_line_number = 0;

                                                  
  static int log_my_pid = 0;
  int padding;
  const char *p;

     
                                                                          
                                                                             
                                                                         
                                              
     
  if (log_my_pid != MyProcPid)
  {
    log_line_number = 0;
    log_my_pid = MyProcPid;
    formatted_start_time[0] = '\0';
  }
  log_line_number++;

  if (Log_line_prefix == NULL)
  {
    return;                                 
  }

  for (p = Log_line_prefix; *p != '\0'; p++)
  {
    if (*p != '%')
    {
                                   
      appendStringInfoChar(buf, *p);
      continue;
    }

                                                 
    p++;
    if (*p == '\0')
    {
      break;                               
    }
    else if (*p == '%')
    {
                              
      appendStringInfoChar(buf, '%');
      continue;
    }

       
                                                                        
                                                                        
               
       
                                                                           
                                                                          
                                                                   
       
                                                                         
                                                                        
                                                                       
                  
       
    if (*p > '9')
    {
      padding = 0;
    }
    else if ((p = process_log_prefix_padding(p, &padding)) == NULL)
    {
      break;
    }

                            
    switch (*p)
    {
    case 'a':
      if (MyProcPort)
      {
        const char *appname = application_name;

        if (appname == NULL || *appname == '\0')
        {
          appname = _("[unknown]");
        }
        if (padding != 0)
        {
          appendStringInfo(buf, "%*s", padding, appname);
        }
        else
        {
          appendStringInfoString(buf, appname);
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }

      break;
    case 'u':
      if (MyProcPort)
      {
        const char *username = MyProcPort->user_name;

        if (username == NULL || *username == '\0')
        {
          username = _("[unknown]");
        }
        if (padding != 0)
        {
          appendStringInfo(buf, "%*s", padding, username);
        }
        else
        {
          appendStringInfoString(buf, username);
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }
      break;
    case 'd':
      if (MyProcPort)
      {
        const char *dbname = MyProcPort->database_name;

        if (dbname == NULL || *dbname == '\0')
        {
          dbname = _("[unknown]");
        }
        if (padding != 0)
        {
          appendStringInfo(buf, "%*s", padding, dbname);
        }
        else
        {
          appendStringInfoString(buf, dbname);
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }
      break;
    case 'c':
      if (padding != 0)
      {
        char strfbuf[128];

        snprintf(strfbuf, sizeof(strfbuf) - 1, "%lx.%x", (long)(MyStartTime), MyProcPid);
        appendStringInfo(buf, "%*s", padding, strfbuf);
      }
      else
      {
        appendStringInfo(buf, "%lx.%x", (long)(MyStartTime), MyProcPid);
      }
      break;
    case 'p':
      if (padding != 0)
      {
        appendStringInfo(buf, "%*d", padding, MyProcPid);
      }
      else
      {
        appendStringInfo(buf, "%d", MyProcPid);
      }
      break;
    case 'l':
      if (padding != 0)
      {
        appendStringInfo(buf, "%*ld", padding, log_line_number);
      }
      else
      {
        appendStringInfo(buf, "%ld", log_line_number);
      }
      break;
    case 'm':
      setup_formatted_log_time();
      if (padding != 0)
      {
        appendStringInfo(buf, "%*s", padding, formatted_log_time);
      }
      else
      {
        appendStringInfoString(buf, formatted_log_time);
      }
      break;
    case 't':
    {
      pg_time_t stamp_time = (pg_time_t)time(NULL);
      char strfbuf[128];

      pg_strftime(strfbuf, sizeof(strfbuf), "%Y-%m-%d %H:%M:%S %Z", pg_localtime(&stamp_time, log_timezone));
      if (padding != 0)
      {
        appendStringInfo(buf, "%*s", padding, strfbuf);
      }
      else
      {
        appendStringInfoString(buf, strfbuf);
      }
    }
    break;
    case 'n':
    {
      char strfbuf[128];

      if (!saved_timeval_set)
      {
        gettimeofday(&saved_timeval, NULL);
        saved_timeval_set = true;
      }

      snprintf(strfbuf, sizeof(strfbuf), "%ld.%03d", (long)saved_timeval.tv_sec, (int)(saved_timeval.tv_usec / 1000));

      if (padding != 0)
      {
        appendStringInfo(buf, "%*s", padding, strfbuf);
      }
      else
      {
        appendStringInfoString(buf, strfbuf);
      }
    }
    break;
    case 's':
      if (formatted_start_time[0] == '\0')
      {
        setup_formatted_start_time();
      }
      if (padding != 0)
      {
        appendStringInfo(buf, "%*s", padding, formatted_start_time);
      }
      else
      {
        appendStringInfoString(buf, formatted_start_time);
      }
      break;
    case 'i':
      if (MyProcPort)
      {
        const char *psdisp;
        int displen;

        psdisp = get_ps_display(&displen);
        if (padding != 0)
        {
          appendStringInfo(buf, "%*s", padding, psdisp);
        }
        else
        {
          appendBinaryStringInfo(buf, psdisp, displen);
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }
      break;
    case 'r':
      if (MyProcPort && MyProcPort->remote_host)
      {
        if (padding != 0)
        {
          if (MyProcPort->remote_port && MyProcPort->remote_port[0] != '\0')
          {
               
                                                           
                                                            
                                                         
                                                              
                                                         
               

            char *hostport;

            hostport = psprintf("%s(%s)", MyProcPort->remote_host, MyProcPort->remote_port);
            appendStringInfo(buf, "%*s", padding, hostport);
            pfree(hostport);
          }
          else
          {
            appendStringInfo(buf, "%*s", padding, MyProcPort->remote_host);
          }
        }
        else
        {
                                                            
          appendStringInfoString(buf, MyProcPort->remote_host);
          if (MyProcPort->remote_port && MyProcPort->remote_port[0] != '\0')
          {
            appendStringInfo(buf, "(%s)", MyProcPort->remote_port);
          }
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }
      break;
    case 'h':
      if (MyProcPort && MyProcPort->remote_host)
      {
        if (padding != 0)
        {
          appendStringInfo(buf, "%*s", padding, MyProcPort->remote_host);
        }
        else
        {
          appendStringInfoString(buf, MyProcPort->remote_host);
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }
      break;
    case 'q':
                                                         
                                     
      if (MyProcPort == NULL)
      {
        return;
      }
      break;
    case 'v':
                                                     
      if (MyProc != NULL && MyProc->backendId != InvalidBackendId)
      {
        if (padding != 0)
        {
          char strfbuf[128];

          snprintf(strfbuf, sizeof(strfbuf) - 1, "%d/%u", MyProc->backendId, MyProc->lxid);
          appendStringInfo(buf, "%*s", padding, strfbuf);
        }
        else
        {
          appendStringInfo(buf, "%d/%u", MyProc->backendId, MyProc->lxid);
        }
      }
      else if (padding != 0)
      {
        appendStringInfoSpaces(buf, padding > 0 ? padding : -padding);
      }
      break;
    case 'x':
      if (padding != 0)
      {
        appendStringInfo(buf, "%*u", padding, GetTopTransactionIdIfAny());
      }
      else
      {
        appendStringInfo(buf, "%u", GetTopTransactionIdIfAny());
      }
      break;
    case 'e':
      if (padding != 0)
      {
        appendStringInfo(buf, "%*s", padding, unpack_sql_state(edata->sqlerrcode));
      }
      else
      {
        appendStringInfoString(buf, unpack_sql_state(edata->sqlerrcode));
      }
      break;
    default:
                                    
      break;
    }
  }
}

   
                                                      
                                                                     
                                 
   
static inline void
appendCSVLiteral(StringInfo buf, const char *data)
{
  const char *p = data;
  char c;

                                                 
  if (p == NULL)
  {
    return;
  }

  appendStringInfoCharMacro(buf, '"');
  while ((c = *p++) != '\0')
  {
    if (c == '"')
    {
      appendStringInfoCharMacro(buf, '"');
    }
    appendStringInfoCharMacro(buf, c);
  }
  appendStringInfoCharMacro(buf, '"');
}

   
                                                                              
                                                          
   
static void
write_csvlog(ErrorData *edata)
{
  StringInfoData buf;
  bool print_stmt = false;

                                       
  static long log_line_number = 0;

                                                  
  static int log_my_pid = 0;

     
                                                                          
                                                                             
                        
     
  if (log_my_pid != MyProcPid)
  {
    log_line_number = 0;
    log_my_pid = MyProcPid;
    formatted_start_time[0] = '\0';
  }
  log_line_number++;

  initStringInfo(&buf);

     
                                 
     
                                                                          
                                                                            
                                                               
     
  if (formatted_log_time[0] == '\0')
  {
    setup_formatted_log_time();
  }

  appendStringInfoString(&buf, formatted_log_time);
  appendStringInfoChar(&buf, ',');

                
  if (MyProcPort)
  {
    appendCSVLiteral(&buf, MyProcPort->user_name);
  }
  appendStringInfoChar(&buf, ',');

                     
  if (MyProcPort)
  {
    appendCSVLiteral(&buf, MyProcPort->database_name);
  }
  appendStringInfoChar(&buf, ',');

                   
  if (MyProcPid != 0)
  {
    appendStringInfo(&buf, "%d", MyProcPid);
  }
  appendStringInfoChar(&buf, ',');

                            
  if (MyProcPort && MyProcPort->remote_host)
  {
    appendStringInfoChar(&buf, '"');
    appendStringInfoString(&buf, MyProcPort->remote_host);
    if (MyProcPort->remote_port && MyProcPort->remote_port[0] != '\0')
    {
      appendStringInfoChar(&buf, ':');
      appendStringInfoString(&buf, MyProcPort->remote_port);
    }
    appendStringInfoChar(&buf, '"');
  }
  appendStringInfoChar(&buf, ',');

                  
  appendStringInfo(&buf, "%lx.%x", (long)MyStartTime, MyProcPid);
  appendStringInfoChar(&buf, ',');

                   
  appendStringInfo(&buf, "%ld", log_line_number);
  appendStringInfoChar(&buf, ',');

                  
  if (MyProcPort)
  {
    StringInfoData msgbuf;
    const char *psdisp;
    int displen;

    initStringInfo(&msgbuf);

    psdisp = get_ps_display(&displen);
    appendBinaryStringInfo(&msgbuf, psdisp, displen);
    appendCSVLiteral(&buf, msgbuf.data);

    pfree(msgbuf.data);
  }
  appendStringInfoChar(&buf, ',');

                               
  if (formatted_start_time[0] == '\0')
  {
    setup_formatted_start_time();
  }
  appendStringInfoString(&buf, formatted_start_time);
  appendStringInfoChar(&buf, ',');

                              
                                                 
  if (MyProc != NULL && MyProc->backendId != InvalidBackendId)
  {
    appendStringInfo(&buf, "%d/%u", MyProc->backendId, MyProc->lxid);
  }
  appendStringInfoChar(&buf, ',');

                      
  appendStringInfo(&buf, "%u", GetTopTransactionIdIfAny());
  appendStringInfoChar(&buf, ',');

                      
  appendStringInfoString(&buf, _(error_severity(edata->elevel)));
  appendStringInfoChar(&buf, ',');

                      
  appendStringInfoString(&buf, unpack_sql_state(edata->sqlerrcode));
  appendStringInfoChar(&buf, ',');

                  
  appendCSVLiteral(&buf, edata->message);
  appendStringInfoChar(&buf, ',');

                                  
  if (edata->detail_log)
  {
    appendCSVLiteral(&buf, edata->detail_log);
  }
  else
  {
    appendCSVLiteral(&buf, edata->detail);
  }
  appendStringInfoChar(&buf, ',');

               
  appendCSVLiteral(&buf, edata->hint);
  appendStringInfoChar(&buf, ',');

                      
  appendCSVLiteral(&buf, edata->internalquery);
  appendStringInfoChar(&buf, ',');

                                                         
  if (edata->internalpos > 0 && edata->internalquery != NULL)
  {
    appendStringInfo(&buf, "%d", edata->internalpos);
  }
  appendStringInfoChar(&buf, ',');

                  
  if (!edata->hide_ctx)
  {
    appendCSVLiteral(&buf, edata->context);
  }
  appendStringInfoChar(&buf, ',');

                                                                  
  if (is_log_level_output(edata->elevel, log_min_error_statement) && debug_query_string != NULL && !edata->hide_stmt)
  {
    print_stmt = true;
  }
  if (print_stmt)
  {
    appendCSVLiteral(&buf, debug_query_string);
  }
  appendStringInfoChar(&buf, ',');
  if (print_stmt && edata->cursorpos > 0)
  {
    appendStringInfo(&buf, "%d", edata->cursorpos);
  }
  appendStringInfoChar(&buf, ',');

                           
  if (Log_error_verbosity >= PGERROR_VERBOSE)
  {
    StringInfoData msgbuf;

    initStringInfo(&msgbuf);

    if (edata->funcname && edata->filename)
    {
      appendStringInfo(&msgbuf, "%s, %s:%d", edata->funcname, edata->filename, edata->lineno);
    }
    else if (edata->filename)
    {
      appendStringInfo(&msgbuf, "%s:%d", edata->filename, edata->lineno);
    }
    appendCSVLiteral(&buf, msgbuf.data);
    pfree(msgbuf.data);
  }
  appendStringInfoChar(&buf, ',');

                        
  if (application_name)
  {
    appendCSVLiteral(&buf, application_name);
  }

  appendStringInfoChar(&buf, '\n');

                                                                         
  if (am_syslogger)
  {
    write_syslogger_file(buf.data, buf.len, LOG_DESTINATION_CSVLOG);
  }
  else
  {
    write_pipe_chunks(buf.data, buf.len, LOG_DESTINATION_CSVLOG);
  }

  pfree(buf.data);
}

   
                                                                    
                  
   
char *
unpack_sql_state(int sql_state)
{
  static char buf[12];
  int i;

  for (i = 0; i < 5; i++)
  {
    buf[i] = PGUNSIXBIT(sql_state);
    sql_state >>= 6;
  }

  buf[i] = '\0';
  return buf;
}

   
                                      
   
static void
send_message_to_server_log(ErrorData *edata)
{
  StringInfoData buf;

  initStringInfo(&buf);

  saved_timeval_set = false;
  formatted_log_time[0] = '\0';

  log_line_prefix(&buf, edata);
  appendStringInfo(&buf, "%s:  ", _(error_severity(edata->elevel)));

  if (Log_error_verbosity >= PGERROR_VERBOSE)
  {
    appendStringInfo(&buf, "%s: ", unpack_sql_state(edata->sqlerrcode));
  }

  if (edata->message)
  {
    append_with_tabs(&buf, edata->message);
  }
  else
  {
    append_with_tabs(&buf, _("missing error text"));
  }

  if (edata->cursorpos > 0)
  {
    appendStringInfo(&buf, _(" at character %d"), edata->cursorpos);
  }
  else if (edata->internalpos > 0)
  {
    appendStringInfo(&buf, _(" at character %d"), edata->internalpos);
  }

  appendStringInfoChar(&buf, '\n');

  if (Log_error_verbosity >= PGERROR_DEFAULT)
  {
    if (edata->detail_log)
    {
      log_line_prefix(&buf, edata);
      appendStringInfoString(&buf, _("DETAIL:  "));
      append_with_tabs(&buf, edata->detail_log);
      appendStringInfoChar(&buf, '\n');
    }
    else if (edata->detail)
    {
      log_line_prefix(&buf, edata);
      appendStringInfoString(&buf, _("DETAIL:  "));
      append_with_tabs(&buf, edata->detail);
      appendStringInfoChar(&buf, '\n');
    }
    if (edata->hint)
    {
      log_line_prefix(&buf, edata);
      appendStringInfoString(&buf, _("HINT:  "));
      append_with_tabs(&buf, edata->hint);
      appendStringInfoChar(&buf, '\n');
    }
    if (edata->internalquery)
    {
      log_line_prefix(&buf, edata);
      appendStringInfoString(&buf, _("QUERY:  "));
      append_with_tabs(&buf, edata->internalquery);
      appendStringInfoChar(&buf, '\n');
    }
    if (edata->context && !edata->hide_ctx)
    {
      log_line_prefix(&buf, edata);
      appendStringInfoString(&buf, _("CONTEXT:  "));
      append_with_tabs(&buf, edata->context);
      appendStringInfoChar(&buf, '\n');
    }
    if (Log_error_verbosity >= PGERROR_VERBOSE)
    {
                                                         
      if (edata->funcname && edata->filename)
      {
        log_line_prefix(&buf, edata);
        appendStringInfo(&buf, _("LOCATION:  %s, %s:%d\n"), edata->funcname, edata->filename, edata->lineno);
      }
      else if (edata->filename)
      {
        log_line_prefix(&buf, edata);
        appendStringInfo(&buf, _("LOCATION:  %s:%d\n"), edata->filename, edata->lineno);
      }
    }
  }

     
                                                                          
     
  if (is_log_level_output(edata->elevel, log_min_error_statement) && debug_query_string != NULL && !edata->hide_stmt)
  {
    log_line_prefix(&buf, edata);
    appendStringInfoString(&buf, _("STATEMENT:  "));
    append_with_tabs(&buf, debug_query_string);
    appendStringInfoChar(&buf, '\n');
  }

#ifdef HAVE_SYSLOG
                                   
  if (Log_destination & LOG_DESTINATION_SYSLOG)
  {
    int syslog_level;

    switch (edata->elevel)
    {
    case DEBUG5:
    case DEBUG4:
    case DEBUG3:
    case DEBUG2:
    case DEBUG1:
      syslog_level = LOG_DEBUG;
      break;
    case LOG:
    case LOG_SERVER_ONLY:
    case INFO:
      syslog_level = LOG_INFO;
      break;
    case NOTICE:
    case WARNING:
      syslog_level = LOG_NOTICE;
      break;
    case ERROR:
      syslog_level = LOG_WARNING;
      break;
    case FATAL:
      syslog_level = LOG_ERR;
      break;
    case PANIC:
    default:
      syslog_level = LOG_CRIT;
      break;
    }

    write_syslog(syslog_level, buf.data);
  }
#endif                  

#ifdef WIN32
                                     
  if (Log_destination & LOG_DESTINATION_EVENTLOG)
  {
    write_eventlog(edata->elevel, buf.data, buf.len);
  }
#endif            

                                   
  if ((Log_destination & LOG_DESTINATION_STDERR) || whereToSendOutput == DestDebug)
  {
       
                                                                    
                                                                       
                                                     
       
    if (redirection_done && !am_syslogger)
    {
      write_pipe_chunks(buf.data, buf.len, LOG_DESTINATION_STDERR);
    }
#ifdef WIN32

       
                                                                          
                                                                  
       
                                                                           
                                                              
       
    else if (pgwin32_is_service())
    {
      write_eventlog(edata->elevel, buf.data, buf.len);
    }
#endif
    else
    {
      write_console(buf.data, buf.len);
    }
  }

                                                                         
  if (am_syslogger)
  {
    write_syslogger_file(buf.data, buf.len, LOG_DESTINATION_STDERR);
  }

                                   
  if (Log_destination & LOG_DESTINATION_CSVLOG)
  {
    if (redirection_done || am_syslogger)
    {
         
                                                                         
                                                                
         
      pfree(buf.data);
      write_csvlog(edata);
    }
    else
    {
         
                                                                     
                                         
         
      if (!(Log_destination & LOG_DESTINATION_STDERR) && whereToSendOutput != DestDebug)
      {
        write_console(buf.data, buf.len);
      }
      pfree(buf.data);
    }
  }
  else
  {
    pfree(buf.data);
  }
}

   
                                                         
   
                                                                           
                                                                       
                                                                           
                                                                        
                                                                          
                                                                           
                                                             
   
                                                                        
                                                                         
                                                                               
                                                                              
                                                                              
                                                                           
                                                                             
                                       
   
static void
write_pipe_chunks(char *data, int len, int dest)
{
  PipeProtoChunk p;
  int fd = fileno(stderr);
  int rc;

  Assert(len > 0);

  p.proto.nuls[0] = p.proto.nuls[1] = '\0';
  p.proto.pid = MyProcPid;

                                    
  while (len > PIPE_MAX_PAYLOAD)
  {
    p.proto.is_last = (dest == LOG_DESTINATION_CSVLOG ? 'F' : 'f');
    p.proto.len = PIPE_MAX_PAYLOAD;
    memcpy(p.proto.data, data, PIPE_MAX_PAYLOAD);
    rc = write(fd, &p, PIPE_HEADER_SIZE + PIPE_MAX_PAYLOAD);
    (void)rc;
    data += PIPE_MAX_PAYLOAD;
    len -= PIPE_MAX_PAYLOAD;
  }

                            
  p.proto.is_last = (dest == LOG_DESTINATION_CSVLOG ? 'T' : 't');
  p.proto.len = len;
  memcpy(p.proto.data, data, len);
  rc = write(fd, &p, PIPE_HEADER_SIZE + len);
  (void)rc;
}

   
                                                                        
   
                                                                     
                                                                       
                                                                        
                                                                      
                                                                          
                                                                             
                                
   
static void
err_sendstring(StringInfo buf, const char *str)
{
  if (in_error_recursion_trouble())
  {
    pq_send_ascii_string(buf, str);
  }
  else
  {
    pq_sendstring(buf, str);
  }
}

   
                                
   
static void
send_message_to_frontend(ErrorData *edata)
{
  StringInfoData msgbuf;

                                                                  
  pq_beginmessage(&msgbuf, (edata->elevel < ERROR) ? 'N' : 'E');

  if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
  {
                                        
    const char *sev;
    char tbuf[12];
    int ssval;
    int i;

    sev = error_severity(edata->elevel);
    pq_sendbyte(&msgbuf, PG_DIAG_SEVERITY);
    err_sendstring(&msgbuf, _(sev));
    pq_sendbyte(&msgbuf, PG_DIAG_SEVERITY_NONLOCALIZED);
    err_sendstring(&msgbuf, sev);

                                   
    ssval = edata->sqlerrcode;
    for (i = 0; i < 5; i++)
    {
      tbuf[i] = PGUNSIXBIT(ssval);
      ssval >>= 6;
    }
    tbuf[i] = '\0';

    pq_sendbyte(&msgbuf, PG_DIAG_SQLSTATE);
    err_sendstring(&msgbuf, tbuf);

                                                                    
    pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_PRIMARY);
    if (edata->message)
    {
      err_sendstring(&msgbuf, edata->message);
    }
    else
    {
      err_sendstring(&msgbuf, _("missing error text"));
    }

    if (edata->detail)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_DETAIL);
      err_sendstring(&msgbuf, edata->detail);
    }

                                                   

    if (edata->hint)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_MESSAGE_HINT);
      err_sendstring(&msgbuf, edata->hint);
    }

    if (edata->context)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_CONTEXT);
      err_sendstring(&msgbuf, edata->context);
    }

    if (edata->schema_name)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_SCHEMA_NAME);
      err_sendstring(&msgbuf, edata->schema_name);
    }

    if (edata->table_name)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_TABLE_NAME);
      err_sendstring(&msgbuf, edata->table_name);
    }

    if (edata->column_name)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_COLUMN_NAME);
      err_sendstring(&msgbuf, edata->column_name);
    }

    if (edata->datatype_name)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_DATATYPE_NAME);
      err_sendstring(&msgbuf, edata->datatype_name);
    }

    if (edata->constraint_name)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_CONSTRAINT_NAME);
      err_sendstring(&msgbuf, edata->constraint_name);
    }

    if (edata->cursorpos > 0)
    {
      snprintf(tbuf, sizeof(tbuf), "%d", edata->cursorpos);
      pq_sendbyte(&msgbuf, PG_DIAG_STATEMENT_POSITION);
      err_sendstring(&msgbuf, tbuf);
    }

    if (edata->internalpos > 0)
    {
      snprintf(tbuf, sizeof(tbuf), "%d", edata->internalpos);
      pq_sendbyte(&msgbuf, PG_DIAG_INTERNAL_POSITION);
      err_sendstring(&msgbuf, tbuf);
    }

    if (edata->internalquery)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_INTERNAL_QUERY);
      err_sendstring(&msgbuf, edata->internalquery);
    }

    if (edata->filename)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_FILE);
      err_sendstring(&msgbuf, edata->filename);
    }

    if (edata->lineno > 0)
    {
      snprintf(tbuf, sizeof(tbuf), "%d", edata->lineno);
      pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_LINE);
      err_sendstring(&msgbuf, tbuf);
    }

    if (edata->funcname)
    {
      pq_sendbyte(&msgbuf, PG_DIAG_SOURCE_FUNCTION);
      err_sendstring(&msgbuf, edata->funcname);
    }

    pq_sendbyte(&msgbuf, '\0');                 
  }
  else
  {
                                                             
    StringInfoData buf;

    initStringInfo(&buf);

    appendStringInfo(&buf, "%s:  ", _(error_severity(edata->elevel)));

    if (edata->show_funcname && edata->funcname)
    {
      appendStringInfo(&buf, "%s: ", edata->funcname);
    }

    if (edata->message)
    {
      appendStringInfoString(&buf, edata->message);
    }
    else
    {
      appendStringInfoString(&buf, _("missing error text"));
    }

    if (edata->cursorpos > 0)
    {
      appendStringInfo(&buf, _(" at character %d"), edata->cursorpos);
    }
    else if (edata->internalpos > 0)
    {
      appendStringInfo(&buf, _(" at character %d"), edata->internalpos);
    }

    appendStringInfoChar(&buf, '\n');

    err_sendstring(&msgbuf, buf.data);

    pfree(buf.data);
  }

  pq_endmessage(&msgbuf);

     
                                                                           
                                                                           
                                                                             
                                                                        
                                                                            
                               
     
  pq_flush();
}

   
                                                   
   

   
                                                     
   
                                                                             
                                                 
   
static const char *
error_severity(int elevel)
{
  const char *prefix;

  switch (elevel)
  {
  case DEBUG1:
  case DEBUG2:
  case DEBUG3:
  case DEBUG4:
  case DEBUG5:
    prefix = gettext_noop("DEBUG");
    break;
  case LOG:
  case LOG_SERVER_ONLY:
    prefix = gettext_noop("LOG");
    break;
  case INFO:
    prefix = gettext_noop("INFO");
    break;
  case NOTICE:
    prefix = gettext_noop("NOTICE");
    break;
  case WARNING:
    prefix = gettext_noop("WARNING");
    break;
  case ERROR:
    prefix = gettext_noop("ERROR");
    break;
  case FATAL:
    prefix = gettext_noop("FATAL");
    break;
  case PANIC:
    prefix = gettext_noop("PANIC");
    break;
  default:
    prefix = "???";
    break;
  }

  return prefix;
}

   
                    
   
                                                                         
            
   
static void
append_with_tabs(StringInfo buf, const char *str)
{
  char ch;

  while ((ch = *str++) != '\0')
  {
    appendStringInfoCharMacro(buf, ch);
    if (ch == '\n')
    {
      appendStringInfoCharMacro(buf, '\t');
    }
  }
}

   
                                                            
                                                        
                                         
   
void
write_stderr(const char *fmt, ...)
{
  va_list ap;

#ifdef WIN32
  char errbuf[2048];                      
#endif

  fmt = _(fmt);

  va_start(ap, fmt);
#ifndef WIN32
                                          
  vfprintf(stderr, fmt, ap);
  fflush(stderr);
#else
  vsnprintf(errbuf, sizeof(errbuf), fmt, ap);

     
                                                                       
                                      
     
  if (pgwin32_is_service())                           
  {
    write_eventlog(ERROR, errbuf, strlen(errbuf));
  }
  else
  {
                                                 
    write_console(errbuf, strlen(errbuf));
    fflush(stderr);
  }
#endif
  va_end(ap);
}

   
                                                                
   
                                                                        
                                                                           
                                                                          
                                                                            
   
static bool
is_log_level_output(int elevel, int log_min_level)
{
  if (elevel == LOG || elevel == LOG_SERVER_ONLY)
  {
    if (log_min_level == LOG || log_min_level <= ERROR)
    {
      return true;
    }
  }
  else if (log_min_level == LOG)
  {
                       
    if (elevel >= FATAL)
    {
      return true;
    }
  }
                      
  else if (elevel >= log_min_level)
  {
    return true;
  }

  return false;
}

   
                                                                               
   
                                                                            
                                                                           
                                                                          
                                                              
                                                                    
                                                                            
   
                                                                          
                                                                             
                                                                  
                          
   
int
trace_recovery(int trace_level)
{
  if (trace_level < LOG && trace_level >= trace_recovery_messages)
  {
    return LOG;
  }

  return trace_level;
}
