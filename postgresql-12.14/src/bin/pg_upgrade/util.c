   
          
   
                     
   
                                                                
                             
   

#include "postgres_fe.h"

#include "common/username.h"
#include "pg_upgrade.h"

#include <signal.h>

LogOpts log_opts;

static void
pg_log_v(eLogType type, const char *fmt, va_list ap) pg_attribute_printf(2, 0);

   
                   
   
                                                                       
   
void
report_status(eLogType type, const char *fmt, ...)
{
  va_list args;
  char message[MAX_STRING];

  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  pg_log(type, "%s\n", message);
}

                                             
void
end_progress_output(void)
{
     
                                                                         
                          
     
  prep_status(" ");
}

   
               
   
                                                                         
                                                                                  
                                       
   
                                            
                                                                 
   
                                                   
                                       
         
                                                   
   
void
prep_status(const char *fmt, ...)
{
  va_list args;
  char message[MAX_STRING];

  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  if (strlen(message) > 0 && message[strlen(message) - 1] == '\n')
  {
    pg_log(PG_REPORT, "%s", message);
  }
  else
  {
                                                  
    pg_log(PG_REPORT, "%-*s", MESSAGE_WIDTH, message);
  }
}

static void
pg_log_v(eLogType type, const char *fmt, va_list ap)
{
  char message[QUERY_ALLOC];

  vsnprintf(message, sizeof(message), _(fmt), ap);

                                                                
                                                                   
  if (((type != PG_VERBOSE && type != PG_STATUS) || log_opts.verbose) && log_opts.internal != NULL)
  {
    if (type == PG_STATUS)
    {
                                                                 
      fprintf(log_opts.internal, "  %s\n", message);
    }
    else
    {
      fprintf(log_opts.internal, "%s", message);
    }
    fflush(log_opts.internal);
  }

  switch (type)
  {
  case PG_VERBOSE:
    if (log_opts.verbose)
    {
      printf("%s", message);
    }
    break;

  case PG_STATUS:
                                                                      
    if (isatty(fileno(stdout)))
    {
                                              
      printf("  %s%-*.*s\r",
                                                             
          strlen(message) <= MESSAGE_WIDTH - 2 ? "" : "...", MESSAGE_WIDTH - 2, MESSAGE_WIDTH - 2,
                                           
          strlen(message) <= MESSAGE_WIDTH - 2 ? message : message + strlen(message) - MESSAGE_WIDTH + 3 + 2);
    }
    else
    {
      printf("  %s\n", message);
    }
    break;

  case PG_REPORT:
  case PG_WARNING:
    printf("%s", message);
    break;

  case PG_FATAL:
    printf("\n%s", message);
    printf(_("Failure, exiting\n"));
    exit(1);
    break;

  default:
    break;
  }
  fflush(stdout);
}

void
pg_log(eLogType type, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  pg_log_v(type, fmt, args);
  va_end(args);
}

void
pg_fatal(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  pg_log_v(PG_FATAL, fmt, args);
  va_end(args);
  printf(_("Failure, exiting\n"));
  exit(1);
}

void
check_ok(void)
{
                      
  report_status(PG_REPORT, "ok");
  fflush(stdout);
}

   
                      
                                            
   
                                                                         
                                                     
   
char *
quote_identifier(const char *s)
{
  char *result = pg_malloc(strlen(s) * 2 + 3);
  char *r = result;

  *r++ = '"';
  while (*s)
  {
    if (*s == '"')
    {
      *r++ = *s;
    }
    *r++ = *s;
    s++;
  }
  *r++ = '"';
  *r++ = '\0';

  return result;
}

   
                   
   
int
get_user_info(char **user_name_p)
{
  int user_id;
  const char *user_name;
  char *errstr;

#ifndef WIN32
  user_id = geteuid();
#else
  user_id = 1;
#endif

  user_name = get_user_name(&errstr);
  if (!user_name)
  {
    pg_fatal("%s\n", errstr);
  }

                   
  *user_name_p = pg_strdup(user_name);

  return user_id;
}

   
              
   
                         
   
unsigned int
str2uint(const char *str)
{
  return strtoul(str, NULL, 10);
}

   
               
   
                                                   
                                           
   
void
pg_putenv(const char *var, const char *val)
{
  if (val)
  {
#ifndef WIN32
    char *envstr;

    envstr = psprintf("%s=%s", var, val);
    putenv(envstr);

       
                                                                        
                                                               
       
#else
    SetEnvironmentVariableA(var, val);
#endif
  }
  else
  {
#ifndef WIN32
    unsetenv(var);
#else
    SetEnvironmentVariableA(var, "");
#endif
  }
}
