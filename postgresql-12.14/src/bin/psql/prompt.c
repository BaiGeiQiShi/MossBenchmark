   
                                              
   
                                                                
   
                         
   
#include "postgres_fe.h"

#ifdef WIN32
#include <io.h>
#include <win32.h>
#endif

#ifdef HAVE_UNIX_SOCKETS
#include <unistd.h>
#include <netdb.h>
#endif

#include "common.h"
#include "input.h"
#include "prompt.h"
#include "settings.h"

                             
              
   
                                                                       
                                                             
                                            
   
                               
                                                                     
                                                
                                                                           
                    
                                    
                           
                         
                                                            
                                        
                                                         
                                                       
                              
                       
                                                                      
                                                                       
                                                               
                       
   
                                                          
                                                         
                                                                   
   
                                                                            
                          
                                                       
                                                            
   
                                                                     
   
                                                                            
                              
                             
   

char *
get_prompt(promptStatus_t status, ConditionalStack cstack)
{
#define MAX_PROMPT_SIZE 256
  static char destination[MAX_PROMPT_SIZE + 1];
  char buf[MAX_PROMPT_SIZE + 1];
  bool esc = false;
  const char *p;
  const char *prompt_string = "? ";

  switch (status)
  {
  case PROMPT_READY:
    prompt_string = pset.prompt1;
    break;

  case PROMPT_CONTINUE:
  case PROMPT_SINGLEQUOTE:
  case PROMPT_DOUBLEQUOTE:
  case PROMPT_DOLLARQUOTE:
  case PROMPT_COMMENT:
  case PROMPT_PAREN:
    prompt_string = pset.prompt2;
    break;

  case PROMPT_COPY:
    prompt_string = pset.prompt3;
    break;
  }

  destination[0] = '\0';

  for (p = prompt_string; *p && strlen(destination) < sizeof(destination) - 1; p++)
  {
    memset(buf, 0, sizeof(buf));
    if (esc)
    {
      switch (*p)
      {
                              
      case '/':
        if (pset.db)
        {
          strlcpy(buf, PQdb(pset.db), sizeof(buf));
        }
        break;
      case '~':
        if (pset.db)
        {
          const char *var;

          if (strcmp(PQdb(pset.db), PQuser(pset.db)) == 0 || ((var = getenv("PGDATABASE")) && strcmp(var, PQdb(pset.db)) == 0))
          {
            strlcpy(buf, "~", sizeof(buf));
          }
          else
          {
            strlcpy(buf, PQdb(pset.db), sizeof(buf));
          }
        }
        break;

                                             
      case 'M':
      case 'm':
        if (pset.db)
        {
          const char *host = PQhost(pset.db);

                           
          if (host && host[0] && !is_absolute_path(host))
          {
            strlcpy(buf, host, sizeof(buf));
            if (*p == 'm')
            {
              buf[strcspn(buf, ".")] = '\0';
            }
          }
#ifdef HAVE_UNIX_SOCKETS
                           
          else
          {
            if (!host || strcmp(host, DEFAULT_PGSOCKET_DIR) == 0 || *p == 'm')
            {
              strlcpy(buf, "[local]", sizeof(buf));
            }
            else
            {
              snprintf(buf, sizeof(buf), "[local:%s]", host);
            }
          }
#endif
        }
        break;
                                   
      case '>':
        if (pset.db && PQport(pset.db))
        {
          strlcpy(buf, PQport(pset.db), sizeof(buf));
        }
        break;
                                 
      case 'n':
        if (pset.db)
        {
          strlcpy(buf, session_username(), sizeof(buf));
        }
        break;
                         
      case 'p':
        if (pset.db)
        {
          int pid = PQbackendPID(pset.db);

          if (pid)
          {
            snprintf(buf, sizeof(buf), "%d", pid);
          }
        }
        break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        *buf = (char)strtol(p, unconstify(char **, &p), 8);
        --p;
        break;
      case 'R':
        switch (status)
        {
        case PROMPT_READY:
          if (cstack != NULL && !conditional_active(cstack))
          {
            buf[0] = '@';
          }
          else if (!pset.db)
          {
            buf[0] = '!';
          }
          else if (!pset.singleline)
          {
            buf[0] = '=';
          }
          else
          {
            buf[0] = '^';
          }
          break;
        case PROMPT_CONTINUE:
          buf[0] = '-';
          break;
        case PROMPT_SINGLEQUOTE:
          buf[0] = '\'';
          break;
        case PROMPT_DOUBLEQUOTE:
          buf[0] = '"';
          break;
        case PROMPT_DOLLARQUOTE:
          buf[0] = '$';
          break;
        case PROMPT_COMMENT:
          buf[0] = '*';
          break;
        case PROMPT_PAREN:
          buf[0] = '(';
          break;
        default:
          buf[0] = '\0';
          break;
        }
        break;

      case 'x':
        if (!pset.db)
        {
          buf[0] = '?';
        }
        else
        {
          switch (PQtransactionStatus(pset.db))
          {
          case PQTRANS_IDLE:
            buf[0] = '\0';
            break;
          case PQTRANS_ACTIVE:
          case PQTRANS_INTRANS:
            buf[0] = '*';
            break;
          case PQTRANS_INERROR:
            buf[0] = '!';
            break;
          default:
            buf[0] = '?';
            break;
          }
        }
        break;

      case 'l':
        snprintf(buf, sizeof(buf), UINT64_FORMAT, pset.stmt_lineno);
        break;

      case '?':
                          
        break;

      case '#':
        if (is_superuser())
        {
          buf[0] = '#';
        }
        else
        {
          buf[0] = '>';
        }
        break;

                             
      case '`':
      {
        FILE *fd;
        char *file = pg_strdup(p + 1);
        int cmdend;

        cmdend = strcspn(file, "`");
        file[cmdend] = '\0';
        fd = popen(file, "r");
        if (fd)
        {
          if (fgets(buf, sizeof(buf), fd) == NULL)
          {
            buf[0] = '\0';
          }
          pclose(fd);
        }
        if (strlen(buf) > 0 && buf[strlen(buf) - 1] == '\n')
        {
          buf[strlen(buf) - 1] = '\0';
        }
        free(file);
        p += cmdend + 1;
        break;
      }

                                  
      case ':':
      {
        char *name;
        const char *val;
        int nameend;

        name = pg_strdup(p + 1);
        nameend = strcspn(name, ":");
        name[nameend] = '\0';
        val = GetVariable(pset.vars, name);
        if (val)
        {
          strlcpy(buf, val, sizeof(buf));
        }
        free(name);
        p += nameend + 1;
        break;
      }

      case '[':
      case ']':
#if defined(USE_READLINE) && defined(RL_PROMPT_START_IGNORE)

           
                                                             
                                                                   
                                                              
           
        buf[0] = (*p == '[') ? RL_PROMPT_START_IGNORE : RL_PROMPT_END_IGNORE;
        buf[1] = '\0';
#endif                   
        break;

      default:
        buf[0] = *p;
        buf[1] = '\0';
        break;
      }
      esc = false;
    }
    else if (*p == '%')
    {
      esc = true;
    }
    else
    {
      buf[0] = *p;
      buf[1] = '\0';
      esc = false;
    }

    if (!esc)
    {
      strlcat(destination, buf, sizeof(destination));
    }
  }

  return destination;
}
