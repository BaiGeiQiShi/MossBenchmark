   
          
   
                       
   
                                                                
                             
   

#include "postgres_fe.h"

#include <fcntl.h>

#include "pg_upgrade.h"

static void
check_data_dir(ClusterInfo *cluster);
static void
check_bin_dir(ClusterInfo *cluster);
static void
get_bin_version(ClusterInfo *cluster);
static void
validate_exec(const char *dir, const char *cmdName);

#ifdef WIN32
static int
win32_check_directory_write_permissions(void);
#endif

   
                   
   
                                                
   
static void
get_bin_version(ClusterInfo *cluster)
{
  char cmd[MAXPGPATH], cmd_output[MAX_STRING];
  FILE *output;
  int v1 = 0, v2 = 0;

  snprintf(cmd, sizeof(cmd), "\"%s/pg_ctl\" --version", cluster->bindir);

  if ((output = popen(cmd, "r")) == NULL || fgets(cmd_output, sizeof(cmd_output), output) == NULL)
  {
    pg_fatal("could not get pg_ctl version data using %s: %s\n", cmd, strerror(errno));
  }

  pclose(output);

  if (sscanf(cmd_output, "%*s %*s %d.%d", &v1, &v2) < 1)
  {
    pg_fatal("could not get pg_ctl version output from %s\n", cmd);
  }

  if (v1 < 10)
  {
                               
    cluster->bin_version = v1 * 10000 + v2 * 100;
  }
  else
  {
                              
    cluster->bin_version = v1 * 10000;
  }
}

   
               
                                                                          
           
   
                                                                            
                                                                  
                                           
   
                                                                                 
                                               
   
                                                                            
   
bool
exec_prog(const char *log_file, const char *opt_log_file, bool report_error, bool exit_on_error, const char *fmt, ...)
{
  int result = 0;
  int written;

#define MAXCMDLEN (2 * MAXPGPATH)
  char cmd[MAXCMDLEN];
  FILE *log;
  va_list ap;

#ifdef WIN32
  static DWORD mainThreadId = 0;

                                                             
  if (mainThreadId == 0)
  {
    mainThreadId = GetCurrentThreadId();
  }
#endif

  written = 0;
  va_start(ap, fmt);
  written += vsnprintf(cmd + written, MAXCMDLEN - written, fmt, ap);
  va_end(ap);
  if (written >= MAXCMDLEN)
  {
    pg_fatal("command too long\n");
  }
  written += snprintf(cmd + written, MAXCMDLEN - written, " >> \"%s\" 2>&1", log_file);
  if (written >= MAXCMDLEN)
  {
    pg_fatal("command too long\n");
  }

  pg_log(PG_VERBOSE, "%s\n", cmd);

#ifdef WIN32

     
                                                                             
                                                                    
                                                                            
                                                                       
                                                                      
                                                                             
                                  
     
  if (mainThreadId != GetCurrentThreadId())
  {
    result = system(cmd);
  }
#endif

  log = fopen(log_file, "a");

#ifdef WIN32
  {
       
                                                                        
                                                                        
                                                                          
                                                                        
                                                  
       
    int iter;

    for (iter = 0; iter < 4 && log == NULL; iter++)
    {
      pg_usleep(1000000);            
      log = fopen(log_file, "a");
    }
  }
#endif

  if (log == NULL)
  {
    pg_fatal("could not open log file \"%s\": %m\n", log_file);
  }

#ifdef WIN32
                                                     
  if (mainThreadId == GetCurrentThreadId())
  {
    fprintf(log, "\n\n");
  }
#endif
  fprintf(log, "command: %s\n", cmd);
#ifdef WIN32
                                                    
  if (mainThreadId != GetCurrentThreadId())
  {
    fprintf(log, "\n\n");
  }
#endif

     
                                                                             
                                                                     
     
  fclose(log);

#ifdef WIN32
                         
  if (mainThreadId == GetCurrentThreadId())
#endif
    result = system(cmd);

  if (result != 0 && report_error)
  {
                                                                          
    report_status(PG_REPORT, "\n*failure*");
    fflush(stdout);

    pg_log(PG_VERBOSE, "There were problems executing \"%s\"\n", cmd);
    if (opt_log_file)
    {
      pg_log(exit_on_error ? PG_FATAL : PG_REPORT,
          "Consult the last few lines of \"%s\" or \"%s\" for\n"
          "the probable cause of the failure.\n",
          log_file, opt_log_file);
    }
    else
    {
      pg_log(exit_on_error ? PG_FATAL : PG_REPORT,
          "Consult the last few lines of \"%s\" for\n"
          "the probable cause of the failure.\n",
          log_file);
    }
  }

#ifndef WIN32

     
                                                                         
                                                                             
                                                                         
                                                                           
                                                                        
     
  if ((log = fopen(log_file, "a")) == NULL)
  {
    pg_fatal("could not write to log file \"%s\": %m\n", log_file);
  }
  fprintf(log, "\n\n");
  fclose(log);
#endif

  return result == 0;
}

   
                          
   
                                                  
   
bool
pid_lock_file_exists(const char *datadir)
{
  char path[MAXPGPATH];
  int fd;

  snprintf(path, sizeof(path), "%s/postmaster.pid", datadir);

  if ((fd = open(path, O_RDONLY, 0)) < 0)
  {
                                                               
    if (errno != ENOENT && errno != ENOTDIR)
    {
      pg_fatal("could not open file \"%s\" for reading: %s\n", path, strerror(errno));
    }

    return false;
  }

  close(fd);
  return true;
}

   
                        
   
                                                                     
                          
   
                                                 
   
void
verify_directories(void)
{
#ifndef WIN32
  if (access(".", R_OK | W_OK | X_OK) != 0)
#else
  if (win32_check_directory_write_permissions() != 0)
#endif
    pg_fatal("You must have read and write access in the current directory.\n");

  check_bin_dir(&old_cluster);
  check_data_dir(&old_cluster);
  check_bin_dir(&new_cluster);
  check_data_dir(&new_cluster);
}

#ifdef WIN32
   
                                             
   
                                                                      
                                                   
                                                                       
   
static int
win32_check_directory_write_permissions(void)
{
  int fd;

     
                                                                         
                                                                      
     
  if ((fd = open(GLOBALS_DUMP_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) < 0)
  {
    return -1;
  }
  close(fd);

  return unlink(GLOBALS_DUMP_FILE);
}
#endif

   
                      
   
                                                                       
                                    
   
static void
check_single_dir(const char *pg_data, const char *subdir)
{
  struct stat statBuf;
  char subDirName[MAXPGPATH];

  snprintf(subDirName, sizeof(subDirName), "%s%s%s", pg_data,
                                                                 
      *subdir ? "/" : "", subdir);

  if (stat(subDirName, &statBuf) != 0)
  {
    report_status(PG_FATAL, "check for \"%s\" failed: %s\n", subDirName, strerror(errno));
  }
  else if (!S_ISDIR(statBuf.st_mode))
  {
    report_status(PG_FATAL, "\"%s\" is not a directory\n", subDirName);
  }
}

   
                    
   
                                                                         
                                                                         
                                                                            
                                              
   
   
static void
check_data_dir(ClusterInfo *cluster)
{
  const char *pg_data = cluster->pgdata;

                               
  cluster->major_version = get_major_server_version(cluster);

  check_single_dir(pg_data, "");
  check_single_dir(pg_data, "base");
  check_single_dir(pg_data, "global");
  check_single_dir(pg_data, "pg_multixact");
  check_single_dir(pg_data, "pg_subtrans");
  check_single_dir(pg_data, "pg_tblspc");
  check_single_dir(pg_data, "pg_twophase");

                                                 
  if (GET_MAJOR_VERSION(cluster->major_version) <= 906)
  {
    check_single_dir(pg_data, "pg_xlog");
  }
  else
  {
    check_single_dir(pg_data, "pg_wal");
  }

                                                  
  if (GET_MAJOR_VERSION(cluster->major_version) <= 906)
  {
    check_single_dir(pg_data, "pg_clog");
  }
  else
  {
    check_single_dir(pg_data, "pg_xact");
  }
}

   
                   
   
                                                                     
                                                                     
                                                                       
           
   
static void
check_bin_dir(ClusterInfo *cluster)
{
  struct stat statBuf;

                    
  if (stat(cluster->bindir, &statBuf) != 0)
  {
    report_status(PG_FATAL, "check for \"%s\" failed: %s\n", cluster->bindir, strerror(errno));
  }
  else if (!S_ISDIR(statBuf.st_mode))
  {
    report_status(PG_FATAL, "\"%s\" is not a directory\n", cluster->bindir);
  }

  validate_exec(cluster->bindir, "postgres");
  validate_exec(cluster->bindir, "pg_ctl");

     
                                                                          
                                                                             
                                 
     
  get_bin_version(cluster);

                                                                  
  if (GET_MAJOR_VERSION(cluster->bin_version) <= 906)
  {
    validate_exec(cluster->bindir, "pg_resetxlog");
  }
  else
  {
    validate_exec(cluster->bindir, "pg_resetwal");
  }
  if (cluster == &new_cluster)
  {
                                                  
    validate_exec(cluster->bindir, "psql");
    validate_exec(cluster->bindir, "pg_dump");
    validate_exec(cluster->bindir, "pg_dumpall");
  }
}

   
                   
   
                                         
   
static void
validate_exec(const char *dir, const char *cmdName)
{
  char path[MAXPGPATH];
  struct stat buf;

  snprintf(path, sizeof(path), "%s/%s", dir, cmdName);

#ifdef WIN32
                                                 
  if (strlen(path) <= strlen(EXE_EXT) || pg_strcasecmp(path + strlen(path) - strlen(EXE_EXT), EXE_EXT) != 0)
  {
    strlcat(path, EXE_EXT, sizeof(path));
  }
#endif

     
                                                        
     
  if (stat(path, &buf) < 0)
  {
    pg_fatal("check for \"%s\" failed: %s\n", path, strerror(errno));
  }
  else if (!S_ISREG(buf.st_mode))
  {
    pg_fatal("check for \"%s\" failed: not a regular file\n", path);
  }

     
                                                                        
                       
     
#ifndef WIN32
  if (access(path, R_OK) != 0)
#else
  if ((buf.st_mode & S_IRUSR) == 0)
#endif
    pg_fatal("check for \"%s\" failed: cannot read file (permission denied)\n", path);

#ifndef WIN32
  if (access(path, X_OK) != 0)
#else
  if ((buf.st_mode & S_IXUSR) == 0)
#endif
    pg_fatal("check for \"%s\" failed: cannot execute (permission denied)\n", path);
}
