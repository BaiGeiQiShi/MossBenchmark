                                                                            
   
              
                                                 
   
                                                                       
                                                                       
                                                                       
                                                                      
                                                                          
                        
   
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>

#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/tzparser.h"

#define WHITESPACE " \t\n\r"

static bool
validateTzEntry(tzEntry *tzentry);
static bool
splitTzLine(const char *filename, int lineno, char *line, tzEntry *tzentry);
static int
addToArray(tzEntry **base, int *arraysize, int n, tzEntry *entry, bool override);
static int
ParseTzFile(const char *filename, int depth, tzEntry **base, int *arraysize, int n);

   
                                                   
   
                                  
   
static bool
validateTzEntry(tzEntry *tzentry)
{
  unsigned char *p;

     
                                                                  
                 
     
  if (strlen(tzentry->abbrev) > TOKMAXLEN)
  {
    GUC_check_errmsg("time zone abbreviation \"%s\" is too long (maximum %d characters) in time zone file \"%s\", line %d", tzentry->abbrev, TOKMAXLEN, tzentry->filename, tzentry->lineno);
    return false;
  }

     
                                                        
     
  if (tzentry->offset > 14 * 60 * 60 || tzentry->offset < -14 * 60 * 60)
  {
    GUC_check_errmsg("time zone offset %d is out of range in time zone file \"%s\", line %d", tzentry->offset, tzentry->filename, tzentry->lineno);
    return false;
  }

     
                                                                      
     
  for (p = (unsigned char *)tzentry->abbrev; *p; p++)
  {
    *p = pg_tolower(*p);
  }

  return true;
}

   
                                                       
   
                      
              
                     
   
                                                              
   
static bool
splitTzLine(const char *filename, int lineno, char *line, tzEntry *tzentry)
{
  char *abbrev;
  char *offset;
  char *offset_endptr;
  char *remain;
  char *is_dst;

  tzentry->lineno = lineno;
  tzentry->filename = filename;

  abbrev = strtok(line, WHITESPACE);
  if (!abbrev)
  {
    GUC_check_errmsg("missing time zone abbreviation in time zone file \"%s\", line %d", filename, lineno);
    return false;
  }
  tzentry->abbrev = pstrdup(abbrev);

  offset = strtok(NULL, WHITESPACE);
  if (!offset)
  {
    GUC_check_errmsg("missing time zone offset in time zone file \"%s\", line %d", filename, lineno);
    return false;
  }

                                                             
  if (isdigit((unsigned char)*offset) || *offset == '+' || *offset == '-')
  {
    tzentry->zone = NULL;
    tzentry->offset = strtol(offset, &offset_endptr, 10);
    if (offset_endptr == offset || *offset_endptr != '\0')
    {
      GUC_check_errmsg("invalid number for time zone offset in time zone file \"%s\", line %d", filename, lineno);
      return false;
    }

    is_dst = strtok(NULL, WHITESPACE);
    if (is_dst && pg_strcasecmp(is_dst, "D") == 0)
    {
      tzentry->is_dst = true;
      remain = strtok(NULL, WHITESPACE);
    }
    else
    {
                                          
      tzentry->is_dst = false;
      remain = is_dst;
    }
  }
  else
  {
       
                                                                     
                                                                         
                                                                      
       
    tzentry->zone = pstrdup(offset);
    tzentry->offset = 0;
    tzentry->is_dst = false;
    remain = strtok(NULL, WHITESPACE);
  }

  if (!remain)                                   
  {
    return true;
  }

  if (remain[0] != '#')                        
  {
    GUC_check_errmsg("invalid syntax in time zone file \"%s\", line %d", filename, lineno);
    return false;
  }
  return true;
}

   
                                  
   
                                                                   
                                                                            
                                                
                             
                                    
   
                                                                  
   
static int
addToArray(tzEntry **base, int *arraysize, int n, tzEntry *entry, bool override)
{
  tzEntry *arrayptr;
  int low;
  int high;

     
                                                                             
                                                                         
                                    
     
  arrayptr = *base;
  low = 0;
  high = n - 1;
  while (low <= high)
  {
    int mid = (low + high) >> 1;
    tzEntry *midptr = arrayptr + mid;
    int cmp;

    cmp = strcmp(entry->abbrev, midptr->abbrev);
    if (cmp < 0)
    {
      high = mid - 1;
    }
    else if (cmp > 0)
    {
      low = mid + 1;
    }
    else
    {
         
                                                                 
         
      if ((midptr->zone == NULL && entry->zone == NULL && midptr->offset == entry->offset && midptr->is_dst == entry->is_dst) || (midptr->zone != NULL && entry->zone != NULL && strcmp(midptr->zone, entry->zone) == 0))
      {
                                    
        return n;
      }
      if (override)
      {
                                                              
        midptr->zone = entry->zone;
        midptr->offset = entry->offset;
        midptr->is_dst = entry->is_dst;
        return n;
      }
                                                            
      GUC_check_errmsg("time zone abbreviation \"%s\" is multiply defined", entry->abbrev);
      GUC_check_errdetail("Entry in time zone file \"%s\", line %d, conflicts with entry in file \"%s\", line %d.", midptr->filename, midptr->lineno, entry->filename, entry->lineno);
      return -1;
    }
  }

     
                                         
     
  if (n >= *arraysize)
  {
    *arraysize *= 2;
    *base = (tzEntry *)repalloc(*base, *arraysize * sizeof(tzEntry));
  }

  arrayptr = *base + low;

  memmove(arrayptr + 1, arrayptr, (n - low) * sizeof(tzEntry));

  memcpy(arrayptr, entry, sizeof(tzEntry));

  return n + 1;
}

   
                                                                          
   
                                                              
                                  
                                                               
                                                                            
                                                
   
                                                                  
   
static int
ParseTzFile(const char *filename, int depth, tzEntry **base, int *arraysize, int n)
{
  char share_path[MAXPGPATH];
  char file_path[MAXPGPATH];
  FILE *tzFile;
  char tzbuf[1024];
  char *line;
  tzEntry tzentry;
  int lineno = 0;
  bool override = false;
  const char *p;

     
                                                                        
                                                                       
                                                                       
               
     
  for (p = filename; *p; p++)
  {
    if (!isalpha((unsigned char)*p))
    {
                                                                        
      if (depth > 0)
      {
        GUC_check_errmsg("invalid time zone file name \"%s\"", filename);
      }
      return -1;
    }
  }

     
                                                                           
                                                                         
                                                   
     
  if (depth > 3)
  {
    GUC_check_errmsg("time zone file recursion limit exceeded in file \"%s\"", filename);
    return -1;
  }

  get_share_path(my_exec_path, share_path);
  snprintf(file_path, sizeof(file_path), "%s/timezonesets/%s", share_path, filename);
  tzFile = AllocateFile(file_path, "r");
  if (!tzFile)
  {
       
                                                                          
                                                                       
                                                                          
                                                            
       
    int save_errno = errno;
    DIR *tzdir;

    snprintf(file_path, sizeof(file_path), "%s/timezonesets", share_path);
    tzdir = AllocateDir(file_path);
    if (tzdir == NULL)
    {
      GUC_check_errmsg("could not open directory \"%s\": %m", file_path);
      GUC_check_errhint("This may indicate an incomplete PostgreSQL installation, or that the file \"%s\" has been moved away from its proper location.", my_exec_path);
      return -1;
    }
    FreeDir(tzdir);
    errno = save_errno;

       
                                                                  
                           
       
    if (errno != ENOENT || depth > 0)
    {
      GUC_check_errmsg("could not read time zone file \"%s\": %m", filename);
    }

    return -1;
  }

  while (!feof(tzFile))
  {
    lineno++;
    if (fgets(tzbuf, sizeof(tzbuf), tzFile) == NULL)
    {
      if (ferror(tzFile))
      {
        GUC_check_errmsg("could not read time zone file \"%s\": %m", filename);
        n = -1;
        break;
      }
                                       
      break;
    }
    if (strlen(tzbuf) == sizeof(tzbuf) - 1)
    {
                                          
      GUC_check_errmsg("line is too long in time zone file \"%s\", line %d", filename, lineno);
      n = -1;
      break;
    }

                              
    line = tzbuf;
    while (*line && isspace((unsigned char)*line))
    {
      line++;
    }

    if (*line == '\0')                 
    {
      continue;
    }
    if (*line == '#')                   
    {
      continue;
    }

    if (pg_strncasecmp(line, "@INCLUDE", strlen("@INCLUDE")) == 0)
    {
                                                                   
      char *includeFile = pstrdup(line + strlen("@INCLUDE"));

      includeFile = strtok(includeFile, WHITESPACE);
      if (!includeFile || !*includeFile)
      {
        GUC_check_errmsg("@INCLUDE without file name in time zone file \"%s\", line %d", filename, lineno);
        n = -1;
        break;
      }
      n = ParseTzFile(includeFile, depth + 1, base, arraysize, n);
      if (n < 0)
      {
        break;
      }
      continue;
    }

    if (pg_strncasecmp(line, "@OVERRIDE", strlen("@OVERRIDE")) == 0)
    {
      override = true;
      continue;
    }

    if (!splitTzLine(filename, lineno, line, &tzentry))
    {
      n = -1;
      break;
    }
    if (!validateTzEntry(&tzentry))
    {
      n = -1;
      break;
    }
    n = addToArray(base, arraysize, n, &tzentry, override);
    if (n < 0)
    {
      break;
    }
  }

  FreeFile(tzFile);

  return n;
}

   
                                                                        
   
                                                                            
                                                                           
                                               
   
TimeZoneAbbrevTable *
load_tzoffsets(const char *filename)
{
  TimeZoneAbbrevTable *result = NULL;
  MemoryContext tmpContext;
  MemoryContext oldContext;
  tzEntry *array;
  int arraysize;
  int n;

     
                                                                           
                    
     
  tmpContext = AllocSetContextCreate(CurrentMemoryContext, "TZParserMemory", ALLOCSET_SMALL_SIZES);
  oldContext = MemoryContextSwitchTo(tmpContext);

                                             
  arraysize = 128;
  array = (tzEntry *)palloc(arraysize * sizeof(tzEntry));

                         
  n = ParseTzFile(filename, 0, &array, &arraysize, 0);

                                                                            
  if (n >= 0)
  {
    result = ConvertTimeZoneAbbrevs(array, n);
    if (!result)
    {
      GUC_check_errmsg("out of memory");
    }
  }

                
  MemoryContextSwitchTo(oldContext);
  MemoryContextDelete(tmpContext);

  return result;
}
