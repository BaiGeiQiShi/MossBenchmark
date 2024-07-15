                                                                            
   
          
            
   
                                                                
   
                                                               
                                           
   
                  
                     
   
                                                                            
   

#include "c.h"

#ifdef WIN32
                    
int
pgkill(int pid, int sig)
{
  char pipename[128];
  BYTE sigData = sig;
  BYTE sigRet = 0;
  DWORD bytes;

                                                                         
  if (sig >= PG_SIGNAL_COUNT || sig < 0)
  {
    errno = EINVAL;
    return -1;
  }
  if (pid <= 0)
  {
                                       
    errno = EINVAL;
    return -1;
  }

                                                                             
  if (sig == SIGKILL)
  {
    HANDLE prochandle;

    if ((prochandle = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid)) == NULL)
    {
      errno = ESRCH;
      return -1;
    }
    if (!TerminateProcess(prochandle, 255))
    {
      _dosmaperr(GetLastError());
      CloseHandle(prochandle);
      return -1;
    }
    CloseHandle(prochandle);
    return 0;
  }
  snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\pgsignal_%u", pid);

  if (CallNamedPipe(pipename, &sigData, 1, &sigRet, 1, &bytes, 1000))
  {
    if (bytes != 1 || sigRet != sig)
    {
      errno = ESRCH;
      return -1;
    }
    return 0;
  }

  switch (GetLastError())
  {
  case ERROR_BROKEN_PIPE:
  case ERROR_BAD_PIPE:

       
                                                                    
                                                              
       
    return 0;

  case ERROR_FILE_NOT_FOUND:
                                                       
    errno = ESRCH;
    return -1;
  case ERROR_ACCESS_DENIED:
    errno = EPERM;
    return -1;
  default:
    errno = EINVAL;                 
    return -1;
  }
}

#endif
