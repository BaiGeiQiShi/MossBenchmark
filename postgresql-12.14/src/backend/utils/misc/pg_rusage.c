                                                                            
   
               
                                                  
   
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "utils/pg_rusage.h"

   
                              
   
void
pg_rusage_init(PGRUsage *ru0)
{
  getrusage(RUSAGE_SELF, &ru0->ru);
  gettimeofday(&ru0->tv, NULL);
}

   
                                                                  
                                                                 
                                                               
                 
   
const char *
pg_rusage_show(const PGRUsage *ru0)
{
  static char result[100];
  PGRUsage ru1;

  pg_rusage_init(&ru1);

  if (ru1.tv.tv_usec < ru0->tv.tv_usec)
  {
    ru1.tv.tv_sec--;
    ru1.tv.tv_usec += 1000000;
  }
  if (ru1.ru.ru_stime.tv_usec < ru0->ru.ru_stime.tv_usec)
  {
    ru1.ru.ru_stime.tv_sec--;
    ru1.ru.ru_stime.tv_usec += 1000000;
  }
  if (ru1.ru.ru_utime.tv_usec < ru0->ru.ru_utime.tv_usec)
  {
    ru1.ru.ru_utime.tv_sec--;
    ru1.ru.ru_utime.tv_usec += 1000000;
  }

  snprintf(result, sizeof(result), _("CPU: user: %d.%02d s, system: %d.%02d s, elapsed: %d.%02d s"), (int)(ru1.ru.ru_utime.tv_sec - ru0->ru.ru_utime.tv_sec), (int)(ru1.ru.ru_utime.tv_usec - ru0->ru.ru_utime.tv_usec) / 10000, (int)(ru1.ru.ru_stime.tv_sec - ru0->ru.ru_stime.tv_sec), (int)(ru1.ru.ru_stime.tv_usec - ru0->ru.ru_stime.tv_usec) / 10000, (int)(ru1.tv.tv_sec - ru0->tv.tv_sec), (int)(ru1.tv.tv_usec - ru0->tv.tv_usec) / 10000);

  return result;
}
