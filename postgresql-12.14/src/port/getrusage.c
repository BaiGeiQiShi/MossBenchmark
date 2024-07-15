                                                                            
   
               
                                                
   
                                                                         
                                                                        
   
   
                  
                          
   
                                                                            
   

#include "c.h"

#include "rusagestub.h"

                       
                 
                  
             
          
                                                                    
                                                                       
                                                               
   

int
getrusage(int who, struct rusage *rusage)
{
#ifdef WIN32
  FILETIME starttime;
  FILETIME exittime;
  FILETIME kerneltime;
  FILETIME usertime;
  ULARGE_INTEGER li;

  if (who != RUSAGE_SELF)
  {
                                                                      
    errno = EINVAL;
    return -1;
  }

  if (rusage == (struct rusage *)NULL)
  {
    errno = EFAULT;
    return -1;
  }
  memset(rusage, 0, sizeof(struct rusage));
  if (GetProcessTimes(GetCurrentProcess(), &starttime, &exittime, &kerneltime, &usertime) == 0)
  {
    _dosmaperr(GetLastError());
    return -1;
  }

                                                    
  memcpy(&li, &kerneltime, sizeof(FILETIME));
  li.QuadPart /= 10L;                              
  rusage->ru_stime.tv_sec = li.QuadPart / 1000000L;
  rusage->ru_stime.tv_usec = li.QuadPart % 1000000L;

  memcpy(&li, &usertime, sizeof(FILETIME));
  li.QuadPart /= 10L;                              
  rusage->ru_utime.tv_sec = li.QuadPart / 1000000L;
  rusage->ru_utime.tv_usec = li.QuadPart % 1000000L;
#else                    

  struct tms tms;
  int tick_rate = CLK_TCK;                       
  clock_t u, s;

  if (rusage == (struct rusage *)NULL)
  {
    errno = EFAULT;
    return -1;
  }
  if (times(&tms) < 0)
  {
                            
    return -1;
  }
  switch (who)
  {
  case RUSAGE_SELF:
    u = tms.tms_utime;
    s = tms.tms_stime;
    break;
  case RUSAGE_CHILDREN:
    u = tms.tms_cutime;
    s = tms.tms_cstime;
    break;
  default:
    errno = EINVAL;
    return -1;
  }
#define TICK_TO_SEC(T, RATE) ((T) / (RATE))
#define TICK_TO_USEC(T, RATE) (((T) % (RATE) * 1000000) / RATE)
  rusage->ru_utime.tv_sec = TICK_TO_SEC(u, tick_rate);
  rusage->ru_utime.tv_usec = TICK_TO_USEC(u, tick_rate);
  rusage->ru_stime.tv_sec = TICK_TO_SEC(s, tick_rate);
  rusage->ru_stime.tv_usec = TICK_TO_USEC(u, tick_rate);
#endif            

  return 0;
}
