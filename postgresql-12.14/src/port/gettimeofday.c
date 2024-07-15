   
                  
                                      
   
                           
   
                                
                                
   
                                                                     
                                                                 
                                                                
                                                             
                                    
   
                                                                   
                                                                      
                                                                 
                                                                        
                                      
   
                                                                        
                                                                         
                                                                       
                                                                        
                                                     
   

#include "c.h"

#include <sys/time.h>

                                                           
static const unsigned __int64 epoch = UINT64CONST(116444736000000000);

   
                                                                    
                          
   
#define FILETIME_UNITS_PER_SEC 10000000L
#define FILETIME_UNITS_PER_USEC 10

   
                                                                           
                                                                        
                          
   
typedef VOID(WINAPI *PgGetSystemTimeFn)(LPFILETIME);

                                                               
static void WINAPI
init_gettimeofday(LPFILETIME lpSystemTimeAsFileTime);

                                                 
static PgGetSystemTimeFn pg_get_system_time = &init_gettimeofday;

   
                                                                           
                                                                
                            
   
static void WINAPI
init_gettimeofday(LPFILETIME lpSystemTimeAsFileTime)
{
     
                                                                       
                                                                            
                                                                            
     
                                                                         
              
     
                                                                         
                                                                            
                                                                      
                                             
     
  pg_get_system_time = (PgGetSystemTimeFn)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetSystemTimePreciseAsFileTime");
  if (pg_get_system_time == NULL)
  {
       
                                                                          
                                                                
       
                                                                      
                                                                          
                                                                    
                                                      
                                                                         
       
    pg_get_system_time = &GetSystemTimeAsFileTime;
  }

  (*pg_get_system_time)(lpSystemTimeAsFileTime);
}

   
                                                                                
   
                                                                            
                   
   
int
gettimeofday(struct timeval *tp, struct timezone *tzp)
{
  FILETIME file_time;
  ULARGE_INTEGER ularge;

  (*pg_get_system_time)(&file_time);
  ularge.LowPart = file_time.dwLowDateTime;
  ularge.HighPart = file_time.dwHighDateTime;

  tp->tv_sec = (long)((ularge.QuadPart - epoch) / FILETIME_UNITS_PER_SEC);
  tp->tv_usec = (long)(((ularge.QuadPart - epoch) % FILETIME_UNITS_PER_SEC) / FILETIME_UNITS_PER_USEC);

  return 0;
}
