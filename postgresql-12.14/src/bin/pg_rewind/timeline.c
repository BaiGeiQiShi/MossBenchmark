                                                                            
   
              
                                 
   
                                                                         
   
                                                                            
   
#include "postgres_fe.h"

#include "pg_rewind.h"

#include "access/timeline.h"
#include "access/xlog_internal.h"

   
                                                                         
                                                                  
   
   
                                          
   
                                                                               
                                                                           
                                                                             
       
   
TimeLineHistoryEntry *
rewind_parseTimeLineHistory(char *buffer, TimeLineID targetTLI, int *nentries)
{
  char *fline;
  TimeLineHistoryEntry *entry;
  TimeLineHistoryEntry *entries = NULL;
  int nlines = 0;
  TimeLineID lasttli = 0;
  XLogRecPtr prevend;
  char *bufptr;
  bool lastline = false;

     
                       
     
  prevend = InvalidXLogRecPtr;
  bufptr = buffer;
  while (!lastline)
  {
    char *ptr;
    TimeLineID tli;
    uint32 switchpoint_hi;
    uint32 switchpoint_lo;
    int nfields;

    fline = bufptr;
    while (*bufptr && *bufptr != '\n')
    {
      bufptr++;
    }
    if (!(*bufptr))
    {
      lastline = true;
    }
    else
    {
      *bufptr++ = '\0';
    }

                                                         
    for (ptr = fline; *ptr; ptr++)
    {
      if (!isspace((unsigned char)*ptr))
      {
        break;
      }
    }
    if (*ptr == '\0' || *ptr == '#')
    {
      continue;
    }

    nfields = sscanf(fline, "%u\t%X/%X", &tli, &switchpoint_hi, &switchpoint_lo);

    if (nfields < 1)
    {
                                                               
      pg_log_error("syntax error in history file: %s", fline);
      pg_log_error("Expected a numeric timeline ID.");
      exit(1);
    }
    if (nfields != 3)
    {
      pg_log_error("syntax error in history file: %s", fline);
      pg_log_error("Expected a write-ahead log switchpoint location.");
      exit(1);
    }
    if (entries && tli <= lasttli)
    {
      pg_log_error("invalid data in history file: %s", fline);
      pg_log_error("Timeline IDs must be in increasing sequence.");
      exit(1);
    }

    lasttli = tli;

    nlines++;
    entries = pg_realloc(entries, nlines * sizeof(TimeLineHistoryEntry));

    entry = &entries[nlines - 1];
    entry->tli = tli;
    entry->begin = prevend;
    entry->end = ((uint64)(switchpoint_hi)) << 32 | (uint64)switchpoint_lo;
    prevend = entry->end;

                                              
  }

  if (entries && targetTLI <= lasttli)
  {
    pg_log_error("invalid data in history file");
    pg_log_error("Timeline IDs must be less than child timeline's ID.");
    exit(1);
  }

     
                                                                             
                          
     
  nlines++;
  if (entries)
  {
    entries = pg_realloc(entries, nlines * sizeof(TimeLineHistoryEntry));
  }
  else
  {
    entries = pg_malloc(1 * sizeof(TimeLineHistoryEntry));
  }

  entry = &entries[nlines - 1];
  entry->tli = targetTLI;
  entry->begin = prevend;
  entry->end = InvalidXLogRecPtr;

  *nentries = nlines;
  return entries;
}
