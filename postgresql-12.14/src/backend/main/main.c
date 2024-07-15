                                                                            
   
          
                                                      
   
                                                                          
                                                                       
                                                                       
                                                 
   
   
                                                                         
                                                                        
   
   
                  
                             
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#if defined(__NetBSD__)
#include <sys/param.h>
#endif

#if defined(_M_AMD64) && _MSC_VER == 1800
#include <math.h>
#include <versionhelpers.h>
#endif

#include "bootstrap/bootstrap.h"
#include "common/username.h"
#include "port/atomics.h"
#include "postmaster/postmaster.h"
#include "storage/s_lock.h"
#include "storage/spin.h"
#include "tcop/tcopprot.h"
#include "utils/help_config.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/ps_status.h"

const char *progname;

static void
startup_hacks(const char *progname);
static void
init_locale(const char *categoryname, int category, const char *locale);
static void
help(const char *progname);
static void
check_root(const char *progname);

   
                                                      
   
int
main(int argc, char *argv[])
{
  bool do_check_root = true;

     
                                                                            
                                                                      
     
#if defined(WIN32) && defined(HAVE_MINIDUMP_TYPE)
  pgwin32_install_crashdump_handler();
#endif

  progname = get_progname(argv[0]);

     
                                     
     
  startup_hacks(progname);

     
                                                                            
                                                                             
                                                                            
                                                                            
     
                                                                        
                                                                           
                                                                          
                     
     
  argv = save_ps_display_args(argc, argv);

     
                                                               
     
                                                                  
                                                                             
                                                        
     
  MemoryContextInit();

     
                                                                         
                                                                         
                                                                          
                                                                           
                                                                             
                                     
     

  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("postgres"));

#ifdef WIN32

     
                                                                           
                                                                          
                                                                            
                                                                          
     
  {
    char *env_locale;

    if ((env_locale = getenv("LC_COLLATE")) != NULL)
    {
      init_locale("LC_COLLATE", LC_COLLATE, env_locale);
    }
    else
    {
      init_locale("LC_COLLATE", LC_COLLATE, "");
    }

    if ((env_locale = getenv("LC_CTYPE")) != NULL)
    {
      init_locale("LC_CTYPE", LC_CTYPE, env_locale);
    }
    else
    {
      init_locale("LC_CTYPE", LC_CTYPE, "");
    }
  }
#else
  init_locale("LC_COLLATE", LC_COLLATE, "");
  init_locale("LC_CTYPE", LC_CTYPE, "");
#endif

#ifdef LC_MESSAGES
  init_locale("LC_MESSAGES", LC_MESSAGES, "");
#endif

     
                                                                             
                                 
     
  init_locale("LC_MONETARY", LC_MONETARY, "C");
  init_locale("LC_NUMERIC", LC_NUMERIC, "C");
  init_locale("LC_TIME", LC_TIME, "C");

     
                                                                     
                                                                     
                                                          
     
  unsetenv("LC_ALL");

  check_strxfrm_bug();

     
                                                                            
                               
     
  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      help(progname);
      exit(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      fputs(PG_BACKEND_VERSIONSTR, stdout);
      exit(0);
    }

       
                                                                           
                                                                      
                                                                          
                                                                        
                                                                          
                                                                         
                                                                        
                                                    
       
    if (strcmp(argv[1], "--describe-config") == 0)
    {
      do_check_root = false;
    }
    else if (argc > 2 && strcmp(argv[1], "-C") == 0)
    {
      do_check_root = false;
    }
  }

     
                                                                             
             
     
  if (do_check_root)
  {
    check_root(progname);
  }

     
                                                                         
     

#ifdef EXEC_BACKEND
  if (argc > 1 && strncmp(argv[1], "--fork", 6) == 0)
  {
    SubPostmasterMain(argc, argv);                      
  }
#endif

#ifdef WIN32

     
                                           
     
                                                                          
                  
     
  pgwin32_signal_initialize();
#endif

  if (argc > 1 && strcmp(argv[1], "--boot") == 0)
  {
    AuxiliaryProcessMain(argc, argv);                      
  }
  else if (argc > 1 && strcmp(argv[1], "--describe-config") == 0)
  {
    GucInfoMain();                      
  }
  else if (argc > 1 && strcmp(argv[1], "--single") == 0)
  {
    PostgresMain(argc, argv, NULL,                               
        strdup(get_user_name_or_exit(progname)));                      
  }
  else
  {
    PostmasterMain(argc, argv);                      
  }
  abort();                          
}

   
                                                                  
                                                                          
                                                                            
                                                                    
                                             
   
                                                                     
                                                                   
                                                      
   
static void
startup_hacks(const char *progname)
{
     
                                                     
     
#ifdef WIN32
  {
    WSADATA wsaData;
    int err;

                                                   
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

                         
    err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0)
    {
      write_stderr("%s: WSAStartup failed: %d\n", progname, err);
      exit(1);
    }

                                                                       
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

#if defined(_M_AMD64) && _MSC_VER == 1800

                 
                                                                      
                                                                      
                                                             
       
                                                                                                                                                      
                 
       
    if (!IsWindows7SP1OrGreater())
    {
      _set_FMA3_enable(0);
    }
#endif                                            
  }
#endif            

     
                                                                           
                                                                
     
  SpinLockInit(&dummy_spinlock);
}

   
                                                                             
                                                                            
                                                                               
                                                                        
                                    
   
static void
init_locale(const char *categoryname, int category, const char *locale)
{
  if (pg_perm_setlocale(category, locale) == NULL && pg_perm_setlocale(category, "C") == NULL)
  {
    elog(FATAL, "could not adopt \"%s\" locale nor C locale for %s", locale, categoryname);
  }
}

   
                                                                      
                       
   
                                                                          
                                                                              
                                                                    
   
static void
help(const char *progname)
{
  printf(_("%s is the PostgreSQL server.\n\n"), progname);
  printf(_("Usage:\n  %s [OPTION]...\n\n"), progname);
  printf(_("Options:\n"));
  printf(_("  -B NBUFFERS        number of shared buffers\n"));
  printf(_("  -c NAME=VALUE      set run-time parameter\n"));
  printf(_("  -C NAME            print value of run-time parameter, then exit\n"));
  printf(_("  -d 1-5             debugging level\n"));
  printf(_("  -D DATADIR         database directory\n"));
  printf(_("  -e                 use European date input format (DMY)\n"));
  printf(_("  -F                 turn fsync off\n"));
  printf(_("  -h HOSTNAME        host name or IP address to listen on\n"));
  printf(_("  -i                 enable TCP/IP connections\n"));
  printf(_("  -k DIRECTORY       Unix-domain socket location\n"));
#ifdef USE_SSL
  printf(_("  -l                 enable SSL connections\n"));
#endif
  printf(_("  -N MAX-CONNECT     maximum number of allowed connections\n"));
  printf(_("  -o OPTIONS         pass \"OPTIONS\" to each server process (obsolete)\n"));
  printf(_("  -p PORT            port number to listen on\n"));
  printf(_("  -s                 show statistics after each query\n"));
  printf(_("  -S WORK-MEM        set amount of memory for sorts (in kB)\n"));
  printf(_("  -V, --version      output version information, then exit\n"));
  printf(_("  --NAME=VALUE       set run-time parameter\n"));
  printf(_("  --describe-config  describe configuration parameters, then exit\n"));
  printf(_("  -?, --help         show this help, then exit\n"));

  printf(_("\nDeveloper options:\n"));
  printf(_("  -f s|i|o|b|t|n|m|h forbid use of some plan types\n"));
  printf(_("  -n                 do not reinitialize shared memory after abnormal exit\n"));
  printf(_("  -O                 allow system table structure changes\n"));
  printf(_("  -P                 disable system indexes\n"));
  printf(_("  -t pa|pl|ex        show timings after each query\n"));
  printf(_("  -T                 send SIGSTOP to all backend processes if one dies\n"));
  printf(_("  -W NUM             wait NUM seconds to allow attach from a debugger\n"));

  printf(_("\nOptions for single-user mode:\n"));
  printf(_("  --single           selects single-user mode (must be first argument)\n"));
  printf(_("  DBNAME             database name (defaults to user name)\n"));
  printf(_("  -d 0-5             override debugging level\n"));
  printf(_("  -E                 echo statement before execution\n"));
  printf(_("  -j                 do not use newline as interactive query delimiter\n"));
  printf(_("  -r FILENAME        send stdout and stderr to given file\n"));

  printf(_("\nOptions for bootstrapping mode:\n"));
  printf(_("  --boot             selects bootstrapping mode (must be first argument)\n"));
  printf(_("  DBNAME             database name (mandatory argument in bootstrapping mode)\n"));
  printf(_("  -r FILENAME        send stdout and stderr to given file\n"));
  printf(_("  -x NUM             internal use\n"));

  printf(_("\nPlease read the documentation for the complete list of run-time\n"
           "configuration settings and how to set them on the command line or in\n"
           "the configuration file.\n\n"
           "Report bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

static void
check_root(const char *progname)
{
#ifndef WIN32
  if (geteuid() == 0)
  {
    write_stderr("\"root\" execution of the PostgreSQL server is not permitted.\n"
                 "The server must be started under an unprivileged user ID to prevent\n"
                 "possible system security compromise.  See the documentation for\n"
                 "more information on how to properly start the server.\n");
    exit(1);
  }

     
                                                                            
                                                                          
                                                                            
                                                                         
                                                                        
                                                           
     
  if (getuid() != geteuid())
  {
    write_stderr("%s: real and effective user IDs must match\n", progname);
    exit(1);
  }
#else             
  if (pgwin32_is_admin())
  {
    write_stderr("Execution of PostgreSQL by a user with administrative permissions is not\n"
                 "permitted.\n"
                 "The server must be started under an unprivileged user ID to prevent\n"
                 "possible system security compromises.  See the documentation for\n"
                 "more information on how to properly start the server.\n");
    exit(1);
  }
#endif            
}
