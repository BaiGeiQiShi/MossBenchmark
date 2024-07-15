                                                                            
   
          
                                                          
   
   
                                                                         
                                                                        
   
   
                  
                       
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

                                                                 
#if defined(WIN32) && !defined(_MSC_VER)
extern int _CRT_glob = 0;                                          
#endif

   
                                                                              
                                                                           
                                                                              
                                                  
   
                                                                         
                                                                      
                                                                          
                                                                               
                            
   
#ifndef FRONTEND
#define log_error(errcodefn, ...) ereport(LOG, (errcodefn, errmsg_internal(__VA_ARGS__)))
#else
#define log_error(errcodefn, ...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#endif

#ifdef _MSC_VER
#define getcwd(cwd, len) GetCurrentDirectory(len, cwd)
#endif

static int
validate_exec(const char *path);
static int
resolve_symlinks(char *path);
static char *
pipe_read_line(char *cmd, char *line, int maxsize);

#ifdef WIN32
static BOOL
GetTokenUser(HANDLE hToken, PTOKEN_USER *ppTokenUser);
#endif

   
                                                          
   
                                                               
                                                                          
                                                            
   
static int
validate_exec(const char *path)
{
  struct stat buf;
  int is_r;
  int is_x;

#ifdef WIN32
  char path_exe[MAXPGPATH + sizeof(".exe") - 1];

                                               
  if (strlen(path) >= strlen(".exe") && pg_strcasecmp(path + strlen(path) - strlen(".exe"), ".exe") != 0)
  {
    strlcpy(path_exe, path, sizeof(path_exe) - 4);
    strcat(path_exe, ".exe");
    path = path_exe;
  }
#endif

     
                                                        
     
                                                                       
                                               
     
  if (stat(path, &buf) < 0)
  {
    return -1;
  }

  if (!S_ISREG(buf.st_mode))
  {
    return -1;
  }

     
                                                                        
                       
     
#ifndef WIN32
  is_r = (access(path, R_OK) == 0);
  is_x = (access(path, X_OK) == 0);
#else
  is_r = buf.st_mode & S_IRUSR;
  is_x = buf.st_mode & S_IXUSR;
#endif
  return is_x ? (is_r ? 0 : -2) : -1;
}

   
                                                               
   
                                                
                                                          
                                 
   
                                                                       
                                                                    
                                                                    
                                                                          
                                                                         
                                                                 
   
int
find_my_exec(const char *argv0, char *retpath)
{
  char cwd[MAXPGPATH], test_path[MAXPGPATH];
  char *path;

  if (!getcwd(cwd, MAXPGPATH))
  {
    log_error(errcode_for_file_access(), _("could not identify current directory: %m"));
    return -1;
  }

     
                                                           
     
  if (first_dir_separator(argv0) != NULL)
  {
    if (is_absolute_path(argv0))
    {
      StrNCpy(retpath, argv0, MAXPGPATH);
    }
    else
    {
      join_path_components(retpath, cwd, argv0);
    }
    canonicalize_path(retpath);

    if (validate_exec(retpath) == 0)
    {
      return resolve_symlinks(retpath);
    }

    log_error(errcode(ERRCODE_WRONG_OBJECT_TYPE), _("invalid binary \"%s\""), retpath);
    return -1;
  }

#ifdef WIN32
                                                                          
  join_path_components(retpath, cwd, argv0);
  if (validate_exec(retpath) == 0)
  {
    return resolve_symlinks(retpath);
  }
#endif

     
                                                                             
                                        
     
  if ((path = getenv("PATH")) && *path)
  {
    char *startp = NULL, *endp = NULL;

    do
    {
      if (!startp)
      {
        startp = path;
      }
      else
      {
        startp = endp + 1;
      }

      endp = first_path_var_separator(startp);
      if (!endp)
      {
        endp = startp + strlen(startp);                   
      }

      StrNCpy(test_path, startp, Min(endp - startp + 1, MAXPGPATH));

      if (is_absolute_path(test_path))
      {
        join_path_components(retpath, test_path, argv0);
      }
      else
      {
        join_path_components(retpath, cwd, test_path);
        join_path_components(retpath, retpath, argv0);
      }
      canonicalize_path(retpath);

      switch (validate_exec(retpath))
      {
      case 0:               
        return resolve_symlinks(retpath);
      case -1:                                            
        break;
      case -2:                             
        log_error(errcode(ERRCODE_WRONG_OBJECT_TYPE), _("could not read binary \"%s\""), retpath);
        break;
      }
    } while (*endp);
  }

  log_error(errcode(ERRCODE_UNDEFINED_FILE), _("could not find a \"%s\" to execute"), argv0);
  return -1;
}

   
                                                              
   
                                                               
   
                                 
   
                                                                           
                                                                           
                                                 
   
static int
resolve_symlinks(char *path)
{
#ifdef HAVE_READLINK
  struct stat buf;
  char orig_wd[MAXPGPATH], link_buf[MAXPGPATH];
  char *fname;

     
                                                                            
                                                                      
                                                                      
                                                                      
                                                     
     
                                                                       
                                                                          
                                                                          
                                                         
     
  if (!getcwd(orig_wd, MAXPGPATH))
  {
    log_error(errcode_for_file_access(), _("could not identify current directory: %m"));
    return -1;
  }

  for (;;)
  {
    char *lsep;
    int rllen;

    lsep = last_dir_separator(path);
    if (lsep)
    {
      *lsep = '\0';
      if (chdir(path) == -1)
      {
        log_error(errcode_for_file_access(), _("could not change directory to \"%s\": %m"), path);
        return -1;
      }
      fname = lsep + 1;
    }
    else
    {
      fname = path;
    }

    if (lstat(fname, &buf) < 0 || !S_ISLNK(buf.st_mode))
    {
      break;
    }

    errno = 0;
    rllen = readlink(fname, link_buf, sizeof(link_buf));
    if (rllen < 0 || rllen >= sizeof(link_buf))
    {
      log_error(errcode_for_file_access(), _("could not read symbolic link \"%s\": %m"), fname);
      return -1;
    }
    link_buf[rllen] = '\0';
    strcpy(path, link_buf);
  }

                                                           
  strlcpy(link_buf, fname, sizeof(link_buf));

  if (!getcwd(path, MAXPGPATH))
  {
    log_error(errcode_for_file_access(), _("could not identify current directory: %m"));
    return -1;
  }
  join_path_components(path, path, link_buf);
  canonicalize_path(path);

  if (chdir(orig_wd) == -1)
  {
    log_error(errcode_for_file_access(), _("could not change directory to \"%s\": %m"), orig_wd);
    return -1;
  }
#endif                    

  return 0;
}

   
                                                   
                                            
   
int
find_other_exec(const char *argv0, const char *target, const char *versionstr, char *retpath)
{
  char cmd[MAXPGPATH];
  char line[MAXPGPATH];

  if (find_my_exec(argv0, retpath) < 0)
  {
    return -1;
  }

                                                     
  *last_dir_separator(retpath) = '\0';
  canonicalize_path(retpath);

                                           
  snprintf(retpath + strlen(retpath), MAXPGPATH - strlen(retpath), "/%s%s", target, EXE);

  if (validate_exec(retpath) != 0)
  {
    return -1;
  }

  snprintf(cmd, sizeof(cmd), "\"%s\" -V", retpath);

  if (!pipe_read_line(cmd, line, sizeof(line)))
  {
    return -1;
  }

  if (strcmp(line, versionstr) != 0)
  {
    return -2;
  }

  return 0;
}

   
                                                                   
                                                                  
                                    
   
                                                                    
                   
   
static char *
pipe_read_line(char *cmd, char *line, int maxsize)
{
#ifndef WIN32
  FILE *pgver;

                                                      
  fflush(stdout);
  fflush(stderr);

  errno = 0;
  if ((pgver = popen(cmd, "r")) == NULL)
  {
    perror("popen failure");
    return NULL;
  }

  errno = 0;
  if (fgets(line, maxsize, pgver) == NULL)
  {
    if (feof(pgver))
    {
      fprintf(stderr, "no data was returned by command \"%s\"\n", cmd);
    }
    else
    {
      perror("fgets failure");
    }
    pclose(pgver);                        
    return NULL;
  }

  if (pclose_check(pgver))
  {
    return NULL;
  }

  return line;
#else             

  SECURITY_ATTRIBUTES sattr;
  HANDLE childstdoutrd, childstdoutwr, childstdoutrddup;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  char *retval = NULL;

  sattr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sattr.bInheritHandle = TRUE;
  sattr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&childstdoutrd, &childstdoutwr, &sattr, 0))
  {
    return NULL;
  }

  if (!DuplicateHandle(GetCurrentProcess(), childstdoutrd, GetCurrentProcess(), &childstdoutrddup, 0, FALSE, DUPLICATE_SAME_ACCESS))
  {
    CloseHandle(childstdoutrd);
    CloseHandle(childstdoutwr);
    return NULL;
  }

  CloseHandle(childstdoutrd);

  ZeroMemory(&pi, sizeof(pi));
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdError = childstdoutwr;
  si.hStdOutput = childstdoutwr;
  si.hStdInput = INVALID_HANDLE_VALUE;

  if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
  {
                                          
    char *lineptr;

    ZeroMemory(line, maxsize);

                                                     
                                                          
    for (lineptr = line; lineptr < line + maxsize - 1;)
    {
      DWORD bytesread = 0;

                                    
      if (WaitForSingleObject(childstdoutrddup, 10000) != WAIT_OBJECT_0)
      {
        break;                                                 
      }

      if (!ReadFile(childstdoutrddup, lineptr, maxsize - (lineptr - line), &bytesread, NULL))
      {
        break;                                               
      }

      lineptr += strlen(lineptr);

      if (!bytesread)
      {
        break;          
      }

      if (strchr(line, '\n'))
      {
        break;                             
      }
    }

    if (lineptr != line)
    {
                                 
      int len;

                                                                    
      lineptr = strchr(line, '\n');
      if (lineptr)
      {
        *(lineptr + 1) = '\0';
      }

      len = strlen(line);

         
                                                                 
                                                                 
                                                                      
                                                                       
                                                    
         
      if (len >= 2 && line[len - 2] == '\r' && line[len - 1] == '\n')
      {
        line[len - 2] = '\n';
        line[len - 1] = '\0';
        len--;
      }

         
                                                                        
                            
         
      if (len == 0 || line[len - 1] != '\n')
      {
        strcat(line, "\n");
      }

      retval = line;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }

  CloseHandle(childstdoutwr);
  CloseHandle(childstdoutrddup);

  return retval;
#endif            
}

   
                                        
   
int
pclose_check(FILE *stream)
{
  int exitstatus;
  char *reason;

  exitstatus = pclose(stream);

  if (exitstatus == 0)
  {
    return 0;                  
  }

  if (exitstatus == -1)
  {
                                                         
    log_error(errcode(ERRCODE_SYSTEM_ERROR), _("pclose failed: %m"));
  }
  else
  {
    reason = wait_result_to_str(exitstatus);
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "%s", reason);
    pfree(reason);
  }
  return exitstatus;
}

   
                          
   
                                                         
   
                                                                     
   
                                                                           
                                                                         
                      
   
void
set_pglocale_pgservice(const char *argv0, const char *app)
{
  char path[MAXPGPATH];
  char my_exec_path[MAXPGPATH];
  char env_path[MAXPGPATH + sizeof("PGSYSCONFDIR=")];                
                                                                       
  char *dup_path;

                                       
  if (strcmp(app, PG_TEXTDOMAIN("postgres")) != 0)
  {
    setlocale(LC_ALL, "");

       
                                                                          
                                                                         
                                                                         
                                                                        
                                                                   
                                                                   
                                                                   
       
  }

  if (find_my_exec(argv0, my_exec_path) < 0)
  {
    return;
  }

#ifdef ENABLE_NLS
  get_locale_path(my_exec_path, path);
  bindtextdomain(app, path);
  textdomain(app);

  if (getenv("PGLOCALEDIR") == NULL)
  {
                              
    snprintf(env_path, sizeof(env_path), "PGLOCALEDIR=%s", path);
    canonicalize_path(env_path + 12);
    dup_path = strdup(env_path);
    if (dup_path)
    {
      putenv(dup_path);
    }
  }
#endif

  if (getenv("PGSYSCONFDIR") == NULL)
  {
    get_etc_path(my_exec_path, path);

                              
    snprintf(env_path, sizeof(env_path), "PGSYSCONFDIR=%s", path);
    canonicalize_path(env_path + 13);
    dup_path = strdup(env_path);
    if (dup_path)
    {
      putenv(dup_path);
    }
  }
}

#ifdef WIN32

   
                                     
   
                                                                 
                                                   
   
                                                                
                                                         
   
                                                                   
                                                                
                                                                        
                                                                     
                                                                          
                                                            
   
                                                                 
                                                                    
                                                                         
                                                                      
                        
   
BOOL
AddUserToTokenDacl(HANDLE hToken)
{
  int i;
  ACL_SIZE_INFORMATION asi;
  ACCESS_ALLOWED_ACE *pace;
  DWORD dwNewAclSize;
  DWORD dwSize = 0;
  DWORD dwTokenInfoLength = 0;
  PACL pacl = NULL;
  PTOKEN_USER pTokenUser = NULL;
  TOKEN_DEFAULT_DACL tddNew;
  TOKEN_DEFAULT_DACL *ptdd = NULL;
  TOKEN_INFORMATION_CLASS tic = TokenDefaultDacl;
  BOOL ret = FALSE;

                                                    
  if (!GetTokenInformation(hToken, tic, (LPVOID)NULL, dwTokenInfoLength, &dwSize))
  {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      ptdd = (TOKEN_DEFAULT_DACL *)LocalAlloc(LPTR, dwSize);
      if (ptdd == NULL)
      {
        log_error(errcode(ERRCODE_OUT_OF_MEMORY), _("out of memory"));
        goto cleanup;
      }

      if (!GetTokenInformation(hToken, tic, (LPVOID)ptdd, dwSize, &dwSize))
      {
        log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information: error code %lu", GetLastError());
        goto cleanup;
      }
    }
    else
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information buffer size: error code %lu", GetLastError());
      goto cleanup;
    }
  }

                        
  if (!GetAclInformation(ptdd->DefaultDacl, (LPVOID)&asi, (DWORD)sizeof(ACL_SIZE_INFORMATION), AclSizeInformation))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get ACL information: error code %lu", GetLastError());
    goto cleanup;
  }

                                
  if (!GetTokenUser(hToken, &pTokenUser))
  {
    goto cleanup;                               
  }

                                          
  dwNewAclSize = asi.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(pTokenUser->User.Sid) - sizeof(DWORD);

                                               
  pacl = (PACL)LocalAlloc(LPTR, dwNewAclSize);
  if (pacl == NULL)
  {
    log_error(errcode(ERRCODE_OUT_OF_MEMORY), _("out of memory"));
    goto cleanup;
  }

  if (!InitializeAcl(pacl, dwNewAclSize, ACL_REVISION))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not initialize ACL: error code %lu", GetLastError());
    goto cleanup;
  }

                                                             
  for (i = 0; i < (int)asi.AceCount; i++)
  {
    if (!GetAce(ptdd->DefaultDacl, i, (LPVOID *)&pace))
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get ACE: error code %lu", GetLastError());
      goto cleanup;
    }

    if (!AddAce(pacl, ACL_REVISION, MAXDWORD, pace, ((PACE_HEADER)pace)->AceSize))
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not add ACE: error code %lu", GetLastError());
      goto cleanup;
    }
  }

                                            
  if (!AddAccessAllowedAceEx(pacl, ACL_REVISION, OBJECT_INHERIT_ACE, GENERIC_ALL, pTokenUser->User.Sid))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not add access allowed ACE: error code %lu", GetLastError());
    goto cleanup;
  }

                                     
  tddNew.DefaultDacl = pacl;

  if (!SetTokenInformation(hToken, tic, (LPVOID)&tddNew, dwNewAclSize))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not set token information: error code %lu", GetLastError());
    goto cleanup;
  }

  ret = TRUE;

cleanup:
  if (pTokenUser)
  {
    LocalFree((HLOCAL)pTokenUser);
  }

  if (pacl)
  {
    LocalFree((HLOCAL)pacl);
  }

  if (ptdd)
  {
    LocalFree((HLOCAL)ptdd);
  }

  return ret;
}

   
                                                         
   
                                                         
   
                                                                             
                               
   
static BOOL
GetTokenUser(HANDLE hToken, PTOKEN_USER *ppTokenUser)
{
  DWORD dwLength;

  *ppTokenUser = NULL;

  if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwLength))
  {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      *ppTokenUser = (PTOKEN_USER)LocalAlloc(LPTR, dwLength);

      if (*ppTokenUser == NULL)
      {
        log_error(errcode(ERRCODE_OUT_OF_MEMORY), _("out of memory"));
        return FALSE;
      }
    }
    else
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information buffer size: error code %lu", GetLastError());
      return FALSE;
    }
  }

  if (!GetTokenInformation(hToken, TokenUser, *ppTokenUser, dwLength, &dwLength))
  {
    LocalFree(*ppTokenUser);
    *ppTokenUser = NULL;

    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information: error code %lu", GetLastError());
    return FALSE;
  }

                                                             
  return TRUE;
}

#endif
