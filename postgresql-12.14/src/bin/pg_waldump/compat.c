                                                                            
   
            
                                                    
   
                                                                         
   
                  
                                
   
                                                                      
                                                                  
   
                                                                            
   

                                              
#define FRONTEND 1
#include "postgres.h"

#include <time.h>

#include "utils/datetime.h"
#include "lib/stringinfo.h"

                             
pg_time_t
timestamptz_to_time_t(TimestampTz t)
{
  pg_time_t result;

  result = (pg_time_t)(t / USECS_PER_SEC + ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY));
  return result;
}

   
                                                                               
                                                                            
                                                                              
                 
   
                                                                           
                                       
   
                                                                             
                                                          
   
const char *
timestamptz_to_str(TimestampTz dt)
{
  static char buf[MAXDATELEN + 1];
  char ts[MAXDATELEN + 1];
  char zone[MAXDATELEN + 1];
  time_t result = (time_t)timestamptz_to_time_t(dt);
  struct tm *ltime = localtime(&result);

  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", ltime);
  strftime(zone, sizeof(zone), "%Z", ltime);

  snprintf(buf, sizeof(buf), "%s.%06d %s", ts, (int)(dt % USECS_PER_SEC), zone);

  return buf;
}

   
                                                                               
                     
   
void
appendStringInfo(StringInfo str, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void
appendStringInfoString(StringInfo str, const char *string)
{
  appendStringInfo(str, "%s", string);
}

void
appendStringInfoChar(StringInfo str, char ch)
{
  appendStringInfo(str, "%c", ch);
}
