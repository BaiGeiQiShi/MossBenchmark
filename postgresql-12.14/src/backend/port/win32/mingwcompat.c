                                                                            
   
                 
                                   
   
                                                                         
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#ifndef _MSC_VER
   
                                                                               
                                                                    
              
   
const struct in6_addr in6addr_any = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};

   
                                                                          
                                                                           
                                          
   
static HMODULE kernel32 = NULL;

   
                                                            
                       
   
static void
LoadKernel32()
{
  if (kernel32 != NULL)
  {
    return;
  }

  kernel32 = LoadLibraryEx("kernel32.dll", NULL, 0);
  if (kernel32 == NULL)
  {
    ereport(FATAL, (errmsg_internal("could not load kernel32.dll: error code %lu", GetLastError())));
  }
}

   
                                                                 
                
   
typedef BOOL(WINAPI *__RegisterWaitForSingleObject)(PHANDLE, HANDLE, WAITORTIMERCALLBACK, PVOID, ULONG, ULONG);
static __RegisterWaitForSingleObject _RegisterWaitForSingleObject = NULL;

BOOL WINAPI
RegisterWaitForSingleObject(PHANDLE phNewWaitObject, HANDLE hObject, WAITORTIMERCALLBACK Callback, PVOID Context, ULONG dwMilliseconds, ULONG dwFlags)
{
  if (_RegisterWaitForSingleObject == NULL)
  {
    LoadKernel32();

    _RegisterWaitForSingleObject = (__RegisterWaitForSingleObject)GetProcAddress(kernel32, "RegisterWaitForSingleObject");

    if (_RegisterWaitForSingleObject == NULL)
    {
      ereport(FATAL, (errmsg_internal("could not locate RegisterWaitForSingleObject in kernel32.dll: error code %lu", GetLastError())));
    }
  }

  return (_RegisterWaitForSingleObject)(phNewWaitObject, hObject, Callback, Context, dwMilliseconds, dwFlags);
}

#endif
