                                                                            
   
          
                                            
   
                                                                         
   
                  
                         
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "datatype/timestamp.h"
#include "miscadmin.h"
#include "pgtz.h"
#include "storage/fd.h"
#include "utils/hsearch.h"

                                                           
pg_tz *session_timezone = NULL;

                                                           
pg_tz *log_timezone = NULL;

static bool
scan_directory_ci(const char *dirname, const char *fname, int fnamelen, char *canonname, int canonnamelen);

   
                                                   
   
static const char *
pg_TZDIR(void)
{
#ifndef SYSTEMTZDIR
                                                          
  static bool done_tzdir = false;
  static char tzdir[MAXPGPATH];

  if (done_tzdir)
  {
    return tzdir;
  }

  get_share_path(my_exec_path, tzdir);
  strlcpy(tzdir + strlen(tzdir), "/timezone", MAXPGPATH - strlen(tzdir));

  done_tzdir = true;
  return tzdir;
#else
                                                          
  return SYSTEMTZDIR;
#endif
}

   
                                                                     
                                             
   
                                                                         
                                                              
   
                                                                             
                                                                           
   
int
pg_open_tzfile(const char *name, char *canonname)
{
  const char *fname;
  char fullname[MAXPGPATH];
  int fullnamelen;
  int orignamelen;

                                                              
  strlcpy(fullname, pg_TZDIR(), sizeof(fullname));
  orignamelen = fullnamelen = strlen(fullname);

  if (fullnamelen + 1 + strlen(name) >= MAXPGPATH)
  {
    return -1;                    
  }

     
                                                                          
                                                                             
                                                                            
                                                                            
                                       
     
  if (canonname == NULL)
  {
    int result;

    fullname[fullnamelen] = '/';
                                           
    strcpy(fullname + fullnamelen + 1, name);
    result = open(fullname, O_RDONLY | PG_BINARY, 0);
    if (result >= 0)
    {
      return result;
    }
                                                                 
    fullname[fullnamelen] = '\0';
  }

     
                                                                         
                                       
     
  fname = name;
  for (;;)
  {
    const char *slashptr;
    int fnamelen;

    slashptr = strchr(fname, '/');
    if (slashptr)
    {
      fnamelen = slashptr - fname;
    }
    else
    {
      fnamelen = strlen(fname);
    }
    if (!scan_directory_ci(fullname, fname, fnamelen, fullname + fullnamelen + 1, MAXPGPATH - fullnamelen - 1))
    {
      return -1;
    }
    fullname[fullnamelen++] = '/';
    fullnamelen += strlen(fullname + fullnamelen);
    if (slashptr)
    {
      fname = slashptr + 1;
    }
    else
    {
      break;
    }
  }

  if (canonname)
  {
    strlcpy(canonname, fullname + orignamelen + 1, TZ_STRLEN_MAX + 1);
  }

  return open(fullname, O_RDONLY | PG_BINARY, 0);
}

   
                                                                  
                                                                          
                                                            
   
static bool
scan_directory_ci(const char *dirname, const char *fname, int fnamelen, char *canonname, int canonnamelen)
{
  bool found = false;
  DIR *dirdesc;
  struct dirent *direntry;

  dirdesc = AllocateDir(dirname);

  while ((direntry = ReadDirExtended(dirdesc, dirname, LOG)) != NULL)
  {
       
                                                                           
                                                                          
       
    if (direntry->d_name[0] == '.')
    {
      continue;
    }

    if (strlen(direntry->d_name) == fnamelen && pg_strncasecmp(direntry->d_name, fname, fnamelen) == 0)
    {
                           
      strlcpy(canonname, direntry->d_name, canonnamelen);
      found = true;
      break;
    }
  }

  FreeDir(dirdesc);

  return found;
}

   
                                                               
                                                                     
                                                                  
                                                    
   
typedef struct
{
                                                                    
  char tznameupper[TZ_STRLEN_MAX + 1];
  pg_tz tz;
} pg_tz_cache;

static HTAB *timezone_cache = NULL;

static bool
init_timezone_hashtable(void)
{
  HASHCTL hash_ctl;

  MemSet(&hash_ctl, 0, sizeof(hash_ctl));

  hash_ctl.keysize = TZ_STRLEN_MAX + 1;
  hash_ctl.entrysize = sizeof(pg_tz_cache);

  timezone_cache = hash_create("Timezones", 4, &hash_ctl, HASH_ELEM);
  if (!timezone_cache)
  {
    return false;
  }

  return true;
}

   
                                            
                                                    
   
                                                                               
                                                                             
                                                                            
                                                                           
                                                                    
                                                                           
                                                           
                                                                         
                                                                      
   
pg_tz *
pg_tzset(const char *name)
{
  pg_tz_cache *tzp;
  struct state tzstate;
  char uppername[TZ_STRLEN_MAX + 1];
  char canonname[TZ_STRLEN_MAX + 1];
  char *p;

  if (strlen(name) > TZ_STRLEN_MAX)
  {
    return NULL;                       
  }

  if (!timezone_cache)
  {
    if (!init_timezone_hashtable())
    {
      return NULL;
    }
  }

     
                                                                           
                                                                          
                                                                             
                                   
     
  p = uppername;
  while (*name)
  {
    *p++ = pg_toupper((unsigned char)*name++);
  }
  *p = '\0';

  tzp = (pg_tz_cache *)hash_search(timezone_cache, uppername, HASH_FIND, NULL);
  if (tzp)
  {
                                                     
    return &tzp->tz;
  }

     
                                                                 
     
  if (strcmp(uppername, "GMT") == 0)
  {
    if (!tzparse(uppername, &tzstate, true))
    {
                                                     
      elog(ERROR, "could not initialize GMT time zone");
    }
                                         
    strcpy(canonname, uppername);
  }
  else if (tzload(uppername, canonname, &tzstate, true) != 0)
  {
    if (uppername[0] == ':' || !tzparse(uppername, &tzstate, false))
    {
                                                                   
      return NULL;
    }
                                                                   
    strcpy(canonname, uppername);
  }

                                  
  tzp = (pg_tz_cache *)hash_search(timezone_cache, uppername, HASH_ENTER, NULL);

                                                              
  strcpy(tzp->tz.TZname, canonname);
  memcpy(&tzp->tz.state, &tzstate, sizeof(tzstate));

  return &tzp->tz;
}

   
                                     
                                                                 
                                            
   
                                                                           
                                                                       
                                                                 
   
                                                                           
                                         
   
pg_tz *
pg_tzset_offset(long gmtoffset)
{
  long absoffset = (gmtoffset < 0) ? -gmtoffset : gmtoffset;
  char offsetstr[64];
  char tzname[128];

  snprintf(offsetstr, sizeof(offsetstr), "%02ld", absoffset / SECS_PER_HOUR);
  absoffset %= SECS_PER_HOUR;
  if (absoffset != 0)
  {
    snprintf(offsetstr + strlen(offsetstr), sizeof(offsetstr) - strlen(offsetstr), ":%02ld", absoffset / SECS_PER_MINUTE);
    absoffset %= SECS_PER_MINUTE;
    if (absoffset != 0)
    {
      snprintf(offsetstr + strlen(offsetstr), sizeof(offsetstr) - strlen(offsetstr), ":%02ld", absoffset);
    }
  }
  if (gmtoffset > 0)
  {
    snprintf(tzname, sizeof(tzname), "<-%s>+%s", offsetstr, offsetstr);
  }
  else
  {
    snprintf(tzname, sizeof(tzname), "<+%s>-%s", offsetstr, offsetstr);
  }

  return pg_tzset(tzname);
}

   
                               
   
                                                                          
                                                                           
                                                                       
                                                                  
                                             
   
void
pg_timezone_initialize(void)
{
     
                                                                            
                                                                             
                                                                           
                                                                     
                                      
     
  session_timezone = pg_tzset("GMT");
  log_timezone = session_timezone;
}

   
                                              
   
                                                                            
                                                             
   
                                                              
   
#define MAX_TZDIR_DEPTH 10

struct pg_tzenum
{
  int baselen;
  int depth;
  DIR *dirdesc[MAX_TZDIR_DEPTH];
  char *dirname[MAX_TZDIR_DEPTH];
  struct pg_tz tz;
};

                                               

pg_tzenum *
pg_tzenumerate_start(void)
{
  pg_tzenum *ret = (pg_tzenum *)palloc0(sizeof(pg_tzenum));
  char *startdir = pstrdup(pg_TZDIR());

  ret->baselen = strlen(startdir) + 1;
  ret->depth = 0;
  ret->dirname[0] = startdir;
  ret->dirdesc[0] = AllocateDir(startdir);
  if (!ret->dirdesc[0])
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", startdir)));
  }
  return ret;
}

void
pg_tzenumerate_end(pg_tzenum *dir)
{
  while (dir->depth >= 0)
  {
    FreeDir(dir->dirdesc[dir->depth]);
    pfree(dir->dirname[dir->depth]);
    dir->depth--;
  }
  pfree(dir);
}

pg_tz *
pg_tzenumerate_next(pg_tzenum *dir)
{
  while (dir->depth >= 0)
  {
    struct dirent *direntry;
    char fullname[MAXPGPATH * 2];
    struct stat statbuf;

    direntry = ReadDir(dir->dirdesc[dir->depth], dir->dirname[dir->depth]);

    if (!direntry)
    {
                                 
      FreeDir(dir->dirdesc[dir->depth]);
      pfree(dir->dirname[dir->depth]);
      dir->depth--;
      continue;
    }

    if (direntry->d_name[0] == '.')
    {
      continue;
    }

    snprintf(fullname, sizeof(fullname), "%s/%s", dir->dirname[dir->depth], direntry->d_name);
    if (stat(fullname, &statbuf) != 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat \"%s\": %m", fullname)));
    }

    if (S_ISDIR(statbuf.st_mode))
    {
                                      
      if (dir->depth >= MAX_TZDIR_DEPTH - 1)
      {
        ereport(ERROR, (errmsg_internal("timezone directory stack overflow")));
      }
      dir->depth++;
      dir->dirname[dir->depth] = pstrdup(fullname);
      dir->dirdesc[dir->depth] = AllocateDir(fullname);
      if (!dir->dirdesc[dir->depth])
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not open directory \"%s\": %m", fullname)));
      }

                                                   
      continue;
    }

       
                                                                          
                                                                          
                                                                     
                    
       
    if (tzload(fullname + dir->baselen, NULL, &dir->tz.state, true) != 0)
    {
                                               
      continue;
    }

    if (!pg_tz_acceptable(&dir->tz))
    {
                                    
      continue;
    }

                                                      
    strlcpy(dir->tz.TZname, fullname + dir->baselen, sizeof(dir->tz.TZname));

                             
    return &dir->tz;
  }

                          
  return NULL;
}
