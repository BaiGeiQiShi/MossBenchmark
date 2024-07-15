                                                                            
   
                                                                   
   
                                                                         
                                                                        
   
                                       
   
                                                                            
   

#include "postgres_fe.h"

#include "pg_regress.h"

char saved_argv0[MAXPGPATH];
char isolation_exec[MAXPGPATH];
bool looked_up_isolation_exec = false;

#define PG_ISOLATION_VERSIONSTR "isolationtester (PostgreSQL) " PG_VERSION "\n"

   
                                                                   
                                       
   
static PID_TYPE
isolation_start_test(const char *testname, _stringlist **resultfiles, _stringlist **expectfiles, _stringlist **tags)
{
  PID_TYPE pid;
  char infile[MAXPGPATH];
  char outfile[MAXPGPATH];
  char expectfile[MAXPGPATH];
  char psql_cmd[MAXPGPATH * 3];
  size_t offset = 0;
  char *appnameenv;

                                                                           
  if (!looked_up_isolation_exec)
  {
                                         
    if (find_other_exec(saved_argv0, "isolationtester", PG_ISOLATION_VERSIONSTR, isolation_exec) != 0)
    {
      fprintf(stderr, _("could not find proper isolationtester binary\n"));
      exit(2);
    }
    looked_up_isolation_exec = true;
  }

     
                                                                             
                                                                            
                                                                             
                                 
     
  snprintf(infile, sizeof(infile), "%s/specs/%s.spec", outputdir, testname);
  if (!file_exists(infile))
  {
    snprintf(infile, sizeof(infile), "%s/specs/%s.spec", inputdir, testname);
  }

  snprintf(outfile, sizeof(outfile), "%s/results/%s.out", outputdir, testname);

  snprintf(expectfile, sizeof(expectfile), "%s/expected/%s.out", outputdir, testname);
  if (!file_exists(expectfile))
  {
    snprintf(expectfile, sizeof(expectfile), "%s/expected/%s.out", inputdir, testname);
  }

  add_stringlist_item(resultfiles, outfile);
  add_stringlist_item(expectfiles, expectfile);

  if (launcher)
  {
    offset += snprintf(psql_cmd + offset, sizeof(psql_cmd) - offset, "%s ", launcher);
    if (offset >= sizeof(psql_cmd))
    {
      fprintf(stderr, _("command too long\n"));
      exit(2);
    }
  }

  offset += snprintf(psql_cmd + offset, sizeof(psql_cmd) - offset, "\"%s\" \"dbname=%s\" < \"%s\" > \"%s\" 2>&1", isolation_exec, dblist->str, infile, outfile);
  if (offset >= sizeof(psql_cmd))
  {
    fprintf(stderr, _("command too long\n"));
    exit(2);
  }

  appnameenv = psprintf("PGAPPNAME=isolation/%s", testname);
  putenv(appnameenv);

  pid = spawn_process(psql_cmd);

  if (pid == INVALID_PID)
  {
    fprintf(stderr, _("could not start process for test %s\n"), testname);
    exit(2);
  }

  unsetenv("PGAPPNAME");
  free(appnameenv);

  return pid;
}

static void
isolation_init(int argc, char **argv)
{
  size_t argv0_len;

     
                                                                         
                                                                 
                                                                           
                                                                             
                                                                          
                                                                          
                                                                      
     
  argv0_len = strlcpy(saved_argv0, argv[0], MAXPGPATH);
  if (argv0_len >= MAXPGPATH)
  {
    fprintf(stderr, _("path for isolationtester executable is longer than %d bytes\n"), (int)(MAXPGPATH - 1));
    exit(2);
  }

                                            
  add_stringlist_item(&dblist, "isolation_regression");
}

int
main(int argc, char *argv[])
{
  return regression_main(argc, argv, isolation_init, isolation_start_test);
}
