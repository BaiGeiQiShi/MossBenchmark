                                                                            
   
             
                                             
                                             
   
   
                  
                               
   
                                                                            
   
#include "postgres_fe.h"

#include <olectl.h>

                      
HANDLE g_module = NULL;                     

   
                                                 
                                                           
                                                                  
   
char event_source[256] = DEFAULT_EVENT_SOURCE;

                
HRESULT
DllInstall(BOOL bInstall, LPCWSTR pszCmdLine);
STDAPI
DllRegisterServer(void);
STDAPI
DllUnregisterServer(void);
BOOL WINAPI
DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);

   
                                                          
   

HRESULT
DllInstall(BOOL bInstall, LPCWSTR pszCmdLine)
{
  if (pszCmdLine && *pszCmdLine != '\0')
  {
    wcstombs(event_source, pszCmdLine, sizeof(event_source));
  }

     
                                                                        
     
                                                                          
                                                                            
                                                                
     
                                                                            
                                                                             
                                                                   
     
  if (bInstall)
  {
    DllRegisterServer();
  }
  return S_OK;
}

   
                                                                      
   

STDAPI
DllRegisterServer(void)
{
  HKEY key;
  DWORD data;
  char buffer[_MAX_PATH];
  char key_name[400];

                                           
  if (!GetModuleFileName((HMODULE)g_module, buffer, sizeof(buffer)))
  {
    MessageBox(NULL, "Could not retrieve DLL filename", "PostgreSQL error", MB_OK | MB_ICONSTOP);
    return SELFREG_E_TYPELIB;
  }

     
                                                                             
                            
     
  _snprintf(key_name, sizeof(key_name), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", event_source);
  if (RegCreateKey(HKEY_LOCAL_MACHINE, key_name, &key))
  {
    MessageBox(NULL, "Could not create the registry key.", "PostgreSQL error", MB_OK | MB_ICONSTOP);
    return SELFREG_E_TYPELIB;
  }

                                                    
  if (RegSetValueEx(key, "EventMessageFile", 0, REG_EXPAND_SZ, (LPBYTE)buffer, strlen(buffer) + 1))
  {
    MessageBox(NULL, "Could not set the event message file.", "PostgreSQL error", MB_OK | MB_ICONSTOP);
    return SELFREG_E_TYPELIB;
  }

                                                                   
  data = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

  if (RegSetValueEx(key, "TypesSupported", 0, REG_DWORD, (LPBYTE)&data, sizeof(DWORD)))
  {
    MessageBox(NULL, "Could not set the supported types.", "PostgreSQL error", MB_OK | MB_ICONSTOP);
    return SELFREG_E_TYPELIB;
  }

  RegCloseKey(key);
  return S_OK;
}

   
                                                                                                        
   

STDAPI
DllUnregisterServer(void)
{
  char key_name[400];

     
                                                                            
                                
     

  _snprintf(key_name, sizeof(key_name), "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", event_source);
  if (RegDeleteKey(HKEY_LOCAL_MACHINE, key_name))
  {
    MessageBox(NULL, "Could not delete the registry key.", "PostgreSQL error", MB_OK | MB_ICONSTOP);
    return SELFREG_E_TYPELIB;
  }
  return S_OK;
}

   
                                                      
   

BOOL WINAPI
DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  if (ul_reason_for_call == DLL_PROCESS_ATTACH)
  {
    g_module = hModule;
  }
  return TRUE;
}
