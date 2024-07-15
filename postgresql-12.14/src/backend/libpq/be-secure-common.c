                                                                            
   
                      
   
                                                      
   
                                                                  
                                                                           
                                                                             
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "libpq/libpq.h"
#include "storage/fd.h"

   
                              
   
                                                                               
                      
   
                                                                            
                                             
   
int
run_ssl_passphrase_command(const char *prompt, bool is_server_start, char *buf, int size)
{
  int loglevel = is_server_start ? ERROR : LOG;
  StringInfoData command;
  char *p;
  FILE *fh;
  int pclose_rc;
  size_t len = 0;

  Assert(prompt);
  Assert(size > 0);
  buf[0] = '\0';

  initStringInfo(&command);

  for (p = ssl_passphrase_command; *p; p++)
  {
    if (p[0] == '%')
    {
      switch (p[1])
      {
      case 'p':
        appendStringInfoString(&command, prompt);
        p++;
        break;
      case '%':
        appendStringInfoChar(&command, '%');
        p++;
        break;
      default:
        appendStringInfoChar(&command, p[0]);
      }
    }
    else
    {
      appendStringInfoChar(&command, p[0]);
    }
  }

  fh = OpenPipeStream(command.data, "r");
  if (fh == NULL)
  {
    ereport(loglevel, (errcode_for_file_access(), errmsg("could not execute command \"%s\": %m", command.data)));
    goto error;
  }

  if (!fgets(buf, size, fh))
  {
    if (ferror(fh))
    {
      ereport(loglevel, (errcode_for_file_access(), errmsg("could not read from command \"%s\": %m", command.data)));
      goto error;
    }
  }

  pclose_rc = ClosePipeStream(fh);
  if (pclose_rc == -1)
  {
    ereport(loglevel, (errcode_for_file_access(), errmsg("could not close pipe to external command: %m")));
    goto error;
  }
  else if (pclose_rc != 0)
  {
    ereport(loglevel, (errcode_for_file_access(), errmsg("command \"%s\" failed", command.data), errdetail_internal("%s", wait_result_to_str(pclose_rc))));
    goto error;
  }

                                                                     
  len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
  {
    buf[--len] = '\0';
  }

error:
  pfree(command.data);
  return len;
}

   
                                        
   
bool
check_ssl_key_file_permissions(const char *ssl_key_file, bool isServerStart)
{
  int loglevel = isServerStart ? FATAL : LOG;
  struct stat buf;

  if (stat(ssl_key_file, &buf) != 0)
  {
    ereport(loglevel, (errcode_for_file_access(), errmsg("could not access private key file \"%s\": %m", ssl_key_file)));
    return false;
  }

                                       
  if (!S_ISREG(buf.st_mode))
  {
    ereport(loglevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("private key file \"%s\" is not a regular file", ssl_key_file)));
    return false;
  }

     
                                                                        
                                                                            
                                                                           
                                                                          
                                                 
     
                                                       
                                                                           
                                                                          
                                                     
     
                                                                          
                                                                           
                
     
#if !defined(WIN32) && !defined(__CYGWIN__)
  if (buf.st_uid != geteuid() && buf.st_uid != 0)
  {
    ereport(loglevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("private key file \"%s\" must be owned by the database user or root", ssl_key_file)));
    return false;
  }

  if ((buf.st_uid == geteuid() && buf.st_mode & (S_IRWXG | S_IRWXO)) || (buf.st_uid == 0 && buf.st_mode & (S_IWGRP | S_IXGRP | S_IRWXO)))
  {
    ereport(loglevel, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("private key file \"%s\" has group or world access", ssl_key_file), errdetail("File must have permissions u=rw (0600) or less if owned by the database user, or permissions u=rw,g=r (0640) or less if owned by root.")));
    return false;
  }
#endif

  return true;
}
