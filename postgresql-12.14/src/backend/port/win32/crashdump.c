                                                                            
   
                     
                                                               
   
                                                                          
                                                          
                                                                          
                                                                         
                                                                                 
          
   
                                                                       
   
               
               
                                                                
   
                                                                               
                                                                              
                     
   
                        
                        
                                                                              
                                                                              
                                                                               
                                                                               
   
   
                                                                         
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#define WIN32_LEAN_AND_MEAN

   
                                                                             
                                                                
                                                                      
                                                                              
                     
   
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4091)
#endif
#include <dbghelp.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

   
                                                                         
                
                                                                        
   
                         
   
                                                                  
                                                                
   
                                                    
                                                       
   

typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

   
                                                                                 
                                                                             
                                                                          
                                                                           
                
   
                                                                           
                                                                           
                             
   
static LONG WINAPI
crashDumpHandler(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
     
                                                                        
                                     
     
  DWORD attribs = GetFileAttributesA("crashdumps");

  if (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY))
  {
                                                                      
    HMODULE hDll = NULL;
    MINIDUMPWRITEDUMP pDump = NULL;
    MINIDUMP_TYPE dumpType;
    char dumpPath[_MAX_PATH];
    HANDLE selfProcHandle = GetCurrentProcess();
    DWORD selfPid = GetProcessId(selfProcHandle);
    HANDLE dumpFile;
    DWORD systemTicks;
    struct _MINIDUMP_EXCEPTION_INFORMATION ExInfo;

    ExInfo.ThreadId = GetCurrentThreadId();
    ExInfo.ExceptionPointers = pExceptionInfo;
    ExInfo.ClientPointers = FALSE;

                                                    
    hDll = LoadLibrary("dbghelp.dll");
    if (hDll == NULL)
    {
      write_stderr("could not load dbghelp.dll, cannot write crash dump\n");
      return EXCEPTION_CONTINUE_SEARCH;
    }

    pDump = (MINIDUMPWRITEDUMP)GetProcAddress(hDll, "MiniDumpWriteDump");

    if (pDump == NULL)
    {
      write_stderr("could not load required functions in dbghelp.dll, cannot write crash dump\n");
      return EXCEPTION_CONTINUE_SEARCH;
    }

       
                                                                        
                                                                    
                                    
                                                                      
       
    dumpType = MiniDumpNormal | MiniDumpWithHandleData | MiniDumpWithDataSegs;

    if (GetProcAddress(hDll, "EnumDirTree") != NULL)
    {
                                                                 
      dumpType |= MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithPrivateReadWriteMemory;
    }

    systemTicks = GetTickCount();
    snprintf(dumpPath, _MAX_PATH, "crashdumps\\postgres-pid%0i-%0i.mdmp", (int)selfPid, (int)systemTicks);
    dumpPath[_MAX_PATH - 1] = '\0';

    dumpFile = CreateFile(dumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dumpFile == INVALID_HANDLE_VALUE)
    {
      write_stderr("could not open crash dump file \"%s\" for writing: error code %lu\n", dumpPath, GetLastError());
      return EXCEPTION_CONTINUE_SEARCH;
    }

    if ((*pDump)(selfProcHandle, selfPid, dumpFile, dumpType, &ExInfo, NULL, NULL))
    {
      write_stderr("wrote crash dump to file \"%s\"\n", dumpPath);
    }
    else
    {
      write_stderr("could not write crash dump to file \"%s\": error code %lu\n", dumpPath, GetLastError());
    }

    CloseHandle(dumpFile);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

void
pgwin32_install_crashdump_handler(void)
{
  SetUnhandledExceptionFilter(crashDumpHandler);
}
