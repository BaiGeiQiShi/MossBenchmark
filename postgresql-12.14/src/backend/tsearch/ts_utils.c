                                                                            
   
              
                              
   
                                                                         
   
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "miscadmin.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"

   
                                                                      
                                                                      
                                                                         
               
   
                                    
   
char *
get_tsearch_config_filename(const char *basename, const char *extension)
{
  char sharepath[MAXPGPATH];
  char *result;

     
                                                                           
                                                                          
                                                                       
                                                                             
                                                                            
                                                                             
                                                                   
     
  if (strspn(basename, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(basename))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid text search configuration file name \"%s\"", basename)));
  }

  get_share_path(my_exec_path, sharepath);
  result = palloc(MAXPGPATH);
  snprintf(result, MAXPGPATH, "%s/tsearch_data/%s.%s", sharepath, basename, extension);

  return result;
}

   
                                                             
                                                                     
                            
   
void
readstoplist(const char *fname, StopList *s, char *(*wordop)(const char *))
{
  char **stop = NULL;

  s->len = 0;
  if (fname && *fname)
  {
    char *filename = get_tsearch_config_filename(fname, "stop");
    tsearch_readline_state trst;
    char *line;
    int reallen = 0;

    if (!tsearch_readline_begin(&trst, filename))
    {
      ereport(ERROR, (errcode(ERRCODE_CONFIG_FILE_ERROR), errmsg("could not open stop-word file \"%s\": %m", filename)));
    }

    while ((line = tsearch_readline(&trst)) != NULL)
    {
      char *pbuf = line;

                               
      while (*pbuf && !t_isspace(pbuf))
      {
        pbuf += pg_mblen(pbuf);
      }
      *pbuf = '\0';

                            
      if (*line == '\0')
      {
        pfree(line);
        continue;
      }

      if (s->len >= reallen)
      {
        if (reallen == 0)
        {
          reallen = 64;
          stop = (char **)palloc(sizeof(char *) * reallen);
        }
        else
        {
          reallen *= 2;
          stop = (char **)repalloc((void *)stop, sizeof(char *) * reallen);
        }
      }

      if (wordop)
      {
        stop[s->len] = wordop(line);
        if (stop[s->len] != line)
        {
          pfree(line);
        }
      }
      else
      {
        stop[s->len] = line;
      }

      (s->len)++;
    }

    tsearch_readline_end(&trst);
    pfree(filename);
  }

  s->stop = stop;

                                      
  if (s->stop && s->len > 0)
  {
    qsort(s->stop, s->len, sizeof(char *), pg_qsort_strcmp);
  }
}

bool
searchstoplist(StopList *s, char *key)
{
  return (s->stop && s->len > 0 && bsearch(&key, s->stop, s->len, sizeof(char *), pg_qsort_strcmp)) ? true : false;
}
