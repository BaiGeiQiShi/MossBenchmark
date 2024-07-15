                                                                            
   
                   
                                                        
   
                                                                         
   
                  
                              
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

   
                                                                    
            
   
static pg_attribute_printf(1, 2) void log_error(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
#ifndef FRONTEND
  write_stderr(fmt, ap);
#else
  fprintf(stderr, fmt, ap);
#endif
  va_end(ap);
}

   
                                                                      
                   
   
                                                                        
            
   
int
pgwin32_is_admin(void)
{
  PSID AdministratorsSid;
  PSID PowerUsersSid;
  SID_IDENTIFIER_AUTHORITY NtAuthority = {SECURITY_NT_AUTHORITY};
  BOOL IsAdministrators;
  BOOL IsPowerUsers;

  if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsSid))
  {
    log_error(_("could not get SID for Administrators group: error code %lu\n"), GetLastError());
    exit(1);
  }

  if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_POWER_USERS, 0, 0, 0, 0, 0, 0, &PowerUsersSid))
  {
    log_error(_("could not get SID for PowerUsers group: error code %lu\n"), GetLastError());
    exit(1);
  }

  if (!CheckTokenMembership(NULL, AdministratorsSid, &IsAdministrators) || !CheckTokenMembership(NULL, PowerUsersSid, &IsPowerUsers))
  {
    log_error(_("could not check access token membership: error code %lu\n"), GetLastError());
    exit(1);
  }

  FreeSid(AdministratorsSid);
  FreeSid(PowerUsersSid);

  if (IsAdministrators || IsPowerUsers)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

   
                                                                         
         
   
                                                            
                                                                          
                                                       
   
                                                                           
                                                                           
                  
   
                  
                    
                
              
   
                                                                           
                                                                      
                                                                        
                          
   
int
pgwin32_is_service(void)
{
  static int _is_service = -1;
  BOOL IsMember;
  PSID ServiceSid;
  PSID LocalSystemSid;
  SID_IDENTIFIER_AUTHORITY NtAuthority = {SECURITY_NT_AUTHORITY};

                                 
  if (_is_service != -1)
  {
    return _is_service;
  }

                                   
  if (!AllocateAndInitializeSid(&NtAuthority, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &LocalSystemSid))
  {
    fprintf(stderr, "could not get SID for local system account\n");
    return -1;
  }

  if (!CheckTokenMembership(NULL, LocalSystemSid, &IsMember))
  {
    fprintf(stderr, "could not check access token membership: error code %lu\n", GetLastError());
    FreeSid(LocalSystemSid);
    return -1;
  }
  FreeSid(LocalSystemSid);

  if (IsMember)
  {
    _is_service = 1;
    return _is_service;
  }

                                          
  if (!AllocateAndInitializeSid(&NtAuthority, 1, SECURITY_SERVICE_RID, 0, 0, 0, 0, 0, 0, 0, &ServiceSid))
  {
    fprintf(stderr, "could not get SID for service group: error code %lu\n", GetLastError());
    return -1;
  }

  if (!CheckTokenMembership(NULL, ServiceSid, &IsMember))
  {
    fprintf(stderr, "could not check access token membership: error code %lu\n", GetLastError());
    FreeSid(ServiceSid);
    return -1;
  }
  FreeSid(ServiceSid);

  if (IsMember)
  {
    _is_service = 1;
  }
  else
  {
    _is_service = 0;
  }

  return _is_service;
}
