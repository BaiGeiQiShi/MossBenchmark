                                                                            
   
          
                                     
   
                                                                         
                                                                        
   
   
                  
                     
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <ctype.h>
#include <sys/stat.h>
#ifdef WIN32
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0500
#ifdef near
#undef near
#endif
#define near
#include <shlobj.h>
#else
#include <unistd.h>
#endif

#include "pg_config_paths.h"

#ifndef WIN32
#define IS_PATH_VAR_SEP(ch) ((ch) == ':')
#else
#define IS_PATH_VAR_SEP(ch) ((ch) == ';')
#endif

static void
make_relative_path(char *ret_path, const char *target_path, const char *bin_path, const char *my_exec_path);
static void
trim_directory(char *path);
static void
trim_trailing_separator(char *path);

   
              
   
                                                                         
                                                      
   
#ifdef WIN32

static char *
skip_drive(const char *path)
{
  if (IS_DIR_SEP(path[0]) && IS_DIR_SEP(path[1]))
  {
    path += 2;
    while (*path && !IS_DIR_SEP(*path))
    {
      path++;
    }
  }
  else if (isalpha((unsigned char)path[0]) && path[1] == ':')
  {
    path += 2;
  }
  return (char *)path;
}
#else

#define skip_drive(path) (path)
#endif

   
                    
   
                                                         
   
bool
has_drive_prefix(const char *path)
{
#ifdef WIN32
  return skip_drive(path) != path;
#else
  return false;
#endif
}

   
                       
   
                                                              
                      
   
char *
first_dir_separator(const char *filename)
{
  const char *p;

  for (p = skip_drive(filename); *p; p++)
  {
    if (IS_DIR_SEP(*p))
    {
      return unconstify(char *, p);
    }
  }
  return NULL;
}

   
                            
   
                                                              
                                                    
   
char *
first_path_var_separator(const char *pathlist)
{
  const char *p;

                                
  for (p = pathlist; *p; p++)
  {
    if (IS_PATH_VAR_SEP(*p))
    {
      return unconstify(char *, p);
    }
  }
  return NULL;
}

   
                      
   
                                                             
                      
   
char *
last_dir_separator(const char *filename)
{
  const char *p, *ret = NULL;

  for (p = skip_drive(filename); *p; p++)
  {
    if (IS_DIR_SEP(*p))
    {
      ret = p;
    }
  }
  return unconstify(char *, ret);
}

   
                                                          
   
                                              
   
                                                              
                                                               
                                                             
                                                               
                                                                   
   
                                                                
                                                              
                          
   
void
make_native_path(char *filename)
{
#ifdef WIN32
  char *p;

  for (p = filename; *p; p++)
  {
    if (*p == '/')
    {
      *p = '\\';
    }
  }
#endif
}

   
                                                                         
                                                                         
                                                
                 
   
void
cleanup_path(char *path)
{
#ifdef WIN32
  char *ptr;

     
                                                                             
                                                                          
                                                                         
                      
     
  GetShortPathName(path, path, MAXPGPATH - 1);

                            
  for (ptr = path; *ptr; ptr++)
  {
    if (*ptr == '\\')
    {
      *ptr = '/';
    }
  }
#endif
}

   
                                                                      
   
                                                         
   
                                                           
   
                                                               
   
void
join_path_components(char *ret_path, const char *head, const char *tail)
{
  if (ret_path != head)
  {
    strlcpy(ret_path, head, MAXPGPATH);
  }

     
                                                   
     
                                                                           
                                                                          
     
  while (tail[0] == '.' && IS_DIR_SEP(tail[1]))
  {
    tail += 2;
  }

  if (*tail)
  {
                                                       
    snprintf(ret_path + strlen(ret_path), MAXPGPATH - strlen(ret_path), "%s%s", (*(skip_drive(head)) != '\0') ? "/" : "", tail);
  }
}

   
                     
                                        
                                      
                             
                                            
                           
                                       
   
void
canonicalize_path(char *path)
{
  char *p, *to_p;
  char *spath;
  bool was_sep = false;
  int pending_strips;

#ifdef WIN32

     
                                                                          
                                                                           
     
  for (p = path; *p; p++)
  {
    if (*p == '\\')
    {
      *p = '/';
    }
  }

     
                                                                            
                                             
     
  if (p > path && *(p - 1) == '"')
  {
    *(p - 1) = '/';
  }
#endif

     
                                                                          
                                                                            
                                                  
     
  trim_trailing_separator(path);

     
                                          
     
  p = path;
#ifdef WIN32
                                                  
  if (*p)
  {
    p++;
  }
#endif
  to_p = p;
  for (; *p; p++, to_p++)
  {
                                                     
    while (*p == '/' && was_sep)
    {
      p++;
    }
    if (to_p != p)
    {
      *to_p = *p;
    }
    was_sep = (*p == '/');
  }
  *to_p = '\0';

     
                                                                
     
                                                                           
                                                                         
                                                                          
                                                                        
                                                                        
            
     
  spath = skip_drive(path);
  pending_strips = 0;
  for (;;)
  {
    int len = strlen(spath);

    if (len >= 2 && strcmp(spath + len - 2, "/.") == 0)
    {
      trim_directory(path);
    }
    else if (strcmp(spath, ".") == 0)
    {
                                                                  
      if (pending_strips > 0)
      {
        *spath = '\0';
      }
      break;
    }
    else if ((len >= 3 && strcmp(spath + len - 3, "/..") == 0) || strcmp(spath, "..") == 0)
    {
      trim_directory(path);
      pending_strips++;
    }
    else if (pending_strips > 0 && *spath != '\0')
    {
                                                          
      trim_directory(path);
      pending_strips--;
                                               
      if (*spath == '\0')
      {
        strcpy(spath, ".");
      }
    }
    else
    {
      break;
    }
  }

  if (pending_strips > 0)
  {
       
                                                                         
                                                                        
                                     
       
    while (--pending_strips > 0)
    {
      strcat(path, "../");
    }
    strcat(path, "..");
  }
}

   
                                                                         
   
                                                                        
   
                                                                        
                                               
   
bool
path_contains_parent_reference(const char *path)
{
  int path_len;

  path = skip_drive(path);                                         

  path_len = strlen(path);

     
                                                                            
                                                  
     
  if (strcmp(path, "..") == 0 || strncmp(path, "../", 3) == 0 || strstr(path, "/../") != NULL || (path_len >= 3 && strcmp(path + path_len - 3, "/..") == 0))
  {
    return true;
  }

  return false;
}

   
                                                                            
                                                                      
                                                                    
                                                                      
                
   
bool
path_is_relative_and_below_cwd(const char *path)
{
  if (is_absolute_path(path))
  {
    return false;
  }
                                          
  else if (path_contains_parent_reference(path))
  {
    return false;
  }
#ifdef WIN32

     
                                                                          
                                                                         
                                                                             
                                                                          
                                                                         
                                                                            
                                 
     
  else if (isalpha((unsigned char)path[0]) && path[1] == ':' && !IS_DIR_SEP(path[2]))
  {
    return false;
  }
#endif
  else
  {
    return true;
  }
}

   
                                                                   
   
                                                                         
                         
   
bool
path_is_prefix_of_path(const char *path1, const char *path2)
{
  int path1_len = strlen(path1);

  if (strncmp(path1, path2, path1_len) == 0 && (IS_DIR_SEP(path2[path1_len]) || path2[path1_len] == '\0'))
  {
    return true;
  }
  return false;
}

   
                                                       
                                  
   
const char *
get_progname(const char *argv0)
{
  const char *nodir_name;
  char *progname;

  nodir_name = last_dir_separator(argv0);
  if (nodir_name)
  {
    nodir_name++;
  }
  else
  {
    nodir_name = skip_drive(argv0);
  }

     
                                                                             
                       
     
  progname = strdup(nodir_name);
  if (progname == NULL)
  {
    fprintf(stderr, "%s: out of memory\n", nodir_name);
    abort();                                     
  }

#if defined(__CYGWIN__) || defined(WIN32)
                                               
  if (strlen(progname) > sizeof(EXE) - 1 && pg_strcasecmp(progname + strlen(progname) - (sizeof(EXE) - 1), EXE) == 0)
  {
    progname[strlen(progname) - (sizeof(EXE) - 1)] = '\0';
  }
#endif

  return progname;
}

   
                                                                              
                                                       
   
static int
dir_strcmp(const char *s1, const char *s2)
{
  while (*s1 && *s2)
  {
    if (
#ifndef WIN32
        *s1 != *s2
#else
                                                    
        pg_tolower((unsigned char)*s1) != pg_tolower((unsigned char)*s2)
#endif
        && !(IS_DIR_SEP(*s1) && IS_DIR_SEP(*s2)))
      return (int)*s1 - (int)*s2;
    s1++, s2++;
  }
  if (*s1)
  {
    return 1;                
  }
  if (*s2)
  {
    return -1;                
  }
  return 0;
}

   
                                                                           
   
                                                                     
   
                                                           
                                                                        
                                                                    
                                                        
   
                                                                            
                                                                   
                                                                              
                                                                              
                             
   
                
                                                 
                                 
                                               
                                                                       
                                                                      
                                                                  
   
static void
make_relative_path(char *ret_path, const char *target_path, const char *bin_path, const char *my_exec_path)
{
  int prefix_len;
  int tail_start;
  int tail_len;
  int i;

     
                                                                    
                                                                     
     
  prefix_len = 0;
  for (i = 0; target_path[i] && bin_path[i]; i++)
  {
    if (IS_DIR_SEP(target_path[i]) && IS_DIR_SEP(bin_path[i]))
    {
      prefix_len = i + 1;
    }
    else if (target_path[i] != bin_path[i])
    {
      break;
    }
  }
  if (prefix_len == 0)
  {
    goto no_match;                        
  }
  tail_len = strlen(bin_path) - prefix_len;

     
                                                                 
                                                      
     
  strlcpy(ret_path, my_exec_path, MAXPGPATH);
  trim_directory(ret_path);                                
  canonicalize_path(ret_path);

     
                 
     
  tail_start = (int)strlen(ret_path) - tail_len;
  if (tail_start > 0 && IS_DIR_SEP(ret_path[tail_start - 1]) && dir_strcmp(ret_path + tail_start, bin_path + prefix_len) == 0)
  {
    ret_path[tail_start] = '\0';
    trim_trailing_separator(ret_path);
    join_path_components(ret_path, ret_path, target_path + prefix_len);
    canonicalize_path(ret_path);
    return;
  }

no_match:
  strlcpy(ret_path, target_path, MAXPGPATH);
  canonicalize_path(ret_path);
}

   
                      
   
                                                                          
                                                 
   
                                                                       
   
                                                                    
                                                   
   
                                                                             
                                                                             
                         
   
char *
make_absolute_path(const char *path)
{
  char *new;

                                                                    
  if (path == NULL)
  {
    return NULL;
  }

  if (!is_absolute_path(path))
  {
    char *buf;
    size_t buflen;

    buflen = MAXPGPATH;
    for (;;)
    {
      buf = malloc(buflen);
      if (!buf)
      {
#ifndef FRONTEND
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
#else
        fprintf(stderr, _("out of memory\n"));
        return NULL;
#endif
      }

      if (getcwd(buf, buflen))
      {
        break;
      }
      else if (errno == ERANGE)
      {
        free(buf);
        buflen *= 2;
        continue;
      }
      else
      {
        int save_errno = errno;

        free(buf);
        errno = save_errno;
#ifndef FRONTEND
        elog(ERROR, "could not get current working directory: %m");
#else
        fprintf(stderr, _("could not get current working directory: %s\n"), strerror(errno));
        return NULL;
#endif
      }
    }

    new = malloc(strlen(buf) + strlen(path) + 2);
    if (!new)
    {
      free(buf);
#ifndef FRONTEND
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
#else
      fprintf(stderr, _("out of memory\n"));
      return NULL;
#endif
    }
    sprintf(new, "%s/%s", buf, path);
    free(buf);
  }
  else
  {
    new = strdup(path);
    if (!new)
    {
#ifndef FRONTEND
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
#else
      fprintf(stderr, _("out of memory\n"));
      return NULL;
#endif
    }
  }

                                               
  canonicalize_path(new);

  return new;
}

   
                  
   
void
get_share_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, PGSHAREDIR, PGBINDIR, my_exec_path);
}

   
                
   
void
get_etc_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, SYSCONFDIR, PGBINDIR, my_exec_path);
}

   
                    
   
void
get_include_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, INCLUDEDIR, PGBINDIR, my_exec_path);
}

   
                       
   
void
get_pkginclude_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, PKGINCLUDEDIR, PGBINDIR, my_exec_path);
}

   
                          
   
void
get_includeserver_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, INCLUDEDIRSERVER, PGBINDIR, my_exec_path);
}

   
                
   
void
get_lib_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, LIBDIR, PGBINDIR, my_exec_path);
}

   
                   
   
void
get_pkglib_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, PKGLIBDIR, PGBINDIR, my_exec_path);
}

   
                   
   
void
get_locale_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, LOCALEDIR, PGBINDIR, my_exec_path);
}

   
                
   
void
get_doc_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, DOCDIR, PGBINDIR, my_exec_path);
}

   
                 
   
void
get_html_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, HTMLDIR, PGBINDIR, my_exec_path);
}

   
                
   
void
get_man_path(const char *my_exec_path, char *ret_path)
{
  make_relative_path(ret_path, MANDIR, PGBINDIR, my_exec_path);
}

   
                 
   
                                                                         
                                                               
   
bool
get_home_path(char *ret_path)
{
#ifndef WIN32
  char pwdbuf[BUFSIZ];
  struct passwd pwdstr;
  struct passwd *pwd = NULL;

  (void)pqGetpwuid(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pwd);
  if (pwd == NULL)
  {
    return false;
  }
  strlcpy(ret_path, pwd->pw_dir, MAXPGPATH);
  return true;
#else
  char *tmppath;

     
                                                                          
                                                                           
                                                                          
                                                                        
                                                                     
     
  tmppath = getenv("APPDATA");
  if (!tmppath)
  {
    return false;
  }
  snprintf(ret_path, MAXPGPATH, "%s/postgresql", tmppath);
  return true;
#endif
}

   
                        
   
                                                                        
               
   
                                                                          
                                                                        
                                                                      
   
                                                                       
                                                                       
                                                                
                              
   
void
get_parent_directory(char *path)
{
  trim_directory(path);
}

   
                  
   
                                                                            
                                                                             
                           
   
static void
trim_directory(char *path)
{
  char *p;

  path = skip_drive(path);

  if (path[0] == '\0')
  {
    return;
  }

                                       
  for (p = path + strlen(path) - 1; IS_DIR_SEP(*p) && p > path; p--)
    ;
                                   
  for (; !IS_DIR_SEP(*p) && p > path; p--)
    ;
                                                                 
  for (; p > path && IS_DIR_SEP(*(p - 1)); p--)
    ;
                                   
  if (p == path && IS_DIR_SEP(*p))
  {
    p++;
  }
  *p = '\0';
}

   
                           
   
                                                      
   
static void
trim_trailing_separator(char *path)
{
  char *p;

  path = skip_drive(path);
  p = path + strlen(path);
  if (p > path)
  {
    for (p--; p > path && IS_DIR_SEP(*p); p--)
    {
      *p = '\0';
    }
  }
}
