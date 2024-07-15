                                                                            
   
              
                   
   
                                                                         
                                                                        
   
                  
                           
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <pwd.h>
#include <unistd.h>

#include "common/username.h"

   
                                                    
                                                                          
   
const char *
get_user_name(char **errstr)
{
#ifndef WIN32
  struct passwd *pw;
  uid_t user_id = geteuid();

  *errstr = NULL;

  errno = 0;                              
  pw = getpwuid(user_id);
  if (!pw)
  {
    *errstr = psprintf(_("could not look up effective user ID %ld: %s"), (long)user_id, errno ? strerror(errno) : _("user does not exist"));
    return NULL;
  }

  return pw->pw_name;
#else
                                                                      
                                                     
  static char username[256 + 1];
  DWORD len = sizeof(username);

  *errstr = NULL;

  if (!GetUserName(username, &len))
  {
    *errstr = psprintf(_("user name lookup failure: error code %lu"), GetLastError());
    return NULL;
  }

  return username;
#endif
}

   
                                                             
   
const char *
get_user_name_or_exit(const char *progname)
{
  const char *user_name;
  char *errstr;

  user_name = get_user_name(&errstr);

  if (!user_name)
  {
    fprintf(stderr, "%s: %s\n", progname, errstr);
    exit(1);
  }
  return user_name;
}
