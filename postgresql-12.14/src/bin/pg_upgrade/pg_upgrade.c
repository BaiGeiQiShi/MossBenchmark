   
                
   
                    
   
                                                                
                                   
   

   
                                                                         
                                           
   
                                                                         
                                                                      
                                                                   
   
                                                                      
                                                                     
                                                                         
                                                                      
                                                                        
                                                                   
   
                                                                           
                                  
   
                                                                           
                                  
   
                                                                           
                                                                              
                                                                 
   

#include "postgres_fe.h"

#include "pg_upgrade.h"
#include "catalog/pg_class_d.h"
#include "common/file_perm.h"
#include "common/logging.h"
#include "common/restricted_token.h"
#include "fe_utils/string_utils.h"

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

static void
prepare_new_cluster(void);
static void
prepare_new_globals(void);
static void
create_new_objects(void);
static void
copy_xact_xlog_xid(void);
static void
set_frozenxids(bool minmxid_only);
static void
setup(char *argv0, bool *live_check);
static void
cleanup(void);

ClusterInfo old_cluster, new_cluster;
OSInfo os_info;

char *output_files[] = {SERVER_LOG_FILE,
#ifdef WIN32
                                      
    SERVER_START_LOG_FILE,
#endif
    UTILITY_LOG_FILE, INTERNAL_LOG_FILE, NULL};

int
main(int argc, char **argv)
{
  char *analyze_script_file_name = NULL;
  char *deletion_script_file_name = NULL;
  bool live_check = false;

  pg_logging_init(argv[0]);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_upgrade"));

                                                                           
  umask(PG_MODE_MASK_OWNER);

  parseCommandLine(argc, argv);

  get_restricted_token();

  adjust_data_dir(&old_cluster);
  adjust_data_dir(&new_cluster);

  setup(argv[0], &live_check);

  output_check_banner(live_check);

  check_cluster_versions();

  get_sock_dir(&old_cluster, live_check);
  get_sock_dir(&new_cluster, false);

  check_cluster_compatibility(live_check);

                                            
  if (!GetDataDirectoryCreatePerm(new_cluster.pgdata))
  {
    pg_log(PG_FATAL, "could not read permissions of directory \"%s\": %s\n", new_cluster.pgdata, strerror(errno));
    exit(1);
  }

  umask(pg_mode_mask);

  check_and_dump_old_cluster(live_check);

                 
  start_postmaster(&new_cluster, true);

  check_new_cluster();
  report_clusters_compatible();

  pg_log(PG_REPORT, "\n"
                    "Performing Upgrade\n"
                    "------------------\n");

  prepare_new_cluster();

  stop_postmaster(false);

     
                                        
     

  copy_xact_xlog_xid();

                                            

                 
  start_postmaster(&new_cluster, true);

  prepare_new_globals();

  create_new_objects();

  stop_postmaster(false);

     
                                                                          
                                                                           
                                                                      
                                                                        
     
  if (user_opts.transfer_mode == TRANSFER_MODE_LINK)
  {
    disable_old_cluster();
  }

  transfer_all_new_tablespaces(&old_cluster.dbarr, &new_cluster.dbarr, old_cluster.pgdata, new_cluster.pgdata);

     
                                                                       
                                                                           
                                                                           
                                                                    
     
  prep_status("Setting next OID for new cluster");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -o %u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_nxtoid, new_cluster.pgdata);
  check_ok();

  prep_status("Sync data directory to disk");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/initdb\" --sync-only \"%s\"", new_cluster.bindir, new_cluster.pgdata);
  check_ok();

  create_script_for_cluster_analyze(&analyze_script_file_name);
  create_script_for_old_cluster_deletion(&deletion_script_file_name);

  issue_warnings_and_set_wal_level();

  pg_log(PG_REPORT, "\n"
                    "Upgrade Complete\n"
                    "----------------\n");

  output_completion_banner(analyze_script_file_name, deletion_script_file_name);

  pg_free(analyze_script_file_name);
  pg_free(deletion_script_file_name);

  cleanup();

  return 0;
}

static void
setup(char *argv0, bool *live_check)
{
  char exec_path[MAXPGPATH];                                 

     
                                                                           
                                                            
     
  check_pghost_envvar();

  verify_directories();

                                                                 
  if (pid_lock_file_exists(old_cluster.pgdata))
  {
       
                                                                         
                                                                          
                                                                          
                                                                        
                                                                          
                                                                          
                             
       
    if (start_postmaster(&old_cluster, false))
    {
      stop_postmaster(false);
    }
    else
    {
      if (!user_opts.check)
      {
        pg_fatal("There seems to be a postmaster servicing the old cluster.\n"
                 "Please shutdown that postmaster and try again.\n");
      }
      else
      {
        *live_check = true;
      }
    }
  }

                                        
  if (pid_lock_file_exists(new_cluster.pgdata))
  {
    if (start_postmaster(&new_cluster, false))
    {
      stop_postmaster(false);
    }
    else
    {
      pg_fatal("There seems to be a postmaster servicing the new cluster.\n"
               "Please shutdown that postmaster and try again.\n");
    }
  }

                                         
  if (find_my_exec(argv0, exec_path) < 0)
  {
    pg_fatal("%s: could not find own program executable\n", argv0);
  }

                                                
  *last_dir_separator(exec_path) = '\0';
  canonicalize_path(exec_path);
  os_info.exec_path = pg_strdup(exec_path);
}

static void
prepare_new_cluster(void)
{
     
                                                                           
                                                                       
                                                             
     
  prep_status("Analyzing all rows in the new cluster");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/vacuumdb\" %s --all --analyze %s", new_cluster.bindir, cluster_conn_opts(&new_cluster), log_opts.verbose ? "--verbose" : "");
  check_ok();

     
                                                                             
                                                                          
                                                                            
                    
     
  prep_status("Freezing all rows in the new cluster");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/vacuumdb\" %s --all --freeze %s", new_cluster.bindir, cluster_conn_opts(&new_cluster), log_opts.verbose ? "--verbose" : "");
  check_ok();
}

static void
prepare_new_globals(void)
{
     
                                                                          
     
  set_frozenxids(false);

     
                                                         
     
  prep_status("Restoring global objects in the new cluster");

  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/psql\" " EXEC_PSQL_ARGS " %s -f \"%s\"", new_cluster.bindir, cluster_conn_opts(&new_cluster), GLOBALS_DUMP_FILE);
  check_ok();
}

static void
create_new_objects(void)
{
  int dbnum;

  prep_status("Restoring database schemas in the new cluster\n");

     
                                                                        
                                                                            
                                                       
     
  for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
  {
    char sql_file_name[MAXPGPATH], log_file_name[MAXPGPATH];
    DbInfo *old_db = &old_cluster.dbarr.dbs[dbnum];
    const char *create_opts;

                                             
    if (strcmp(old_db->db_name, "template1") != 0)
    {
      continue;
    }

    pg_log(PG_STATUS, "%s", old_db->db_name);
    snprintf(sql_file_name, sizeof(sql_file_name), DB_DUMP_FILE_MASK, old_db->db_oid);
    snprintf(log_file_name, sizeof(log_file_name), DB_DUMP_LOG_FILE_MASK, old_db->db_oid);

       
                                                                         
                                                                   
                                                                 
                   
       
    create_opts = "--clean --create";

    exec_prog(log_file_name, NULL, true, true,
        "\"%s/pg_restore\" %s %s --exit-on-error --verbose "
        "--dbname postgres \"%s\"",
        new_cluster.bindir, cluster_conn_opts(&new_cluster), create_opts, sql_file_name);

    break;                                          
  }

  for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
  {
    char sql_file_name[MAXPGPATH], log_file_name[MAXPGPATH];
    DbInfo *old_db = &old_cluster.dbarr.dbs[dbnum];
    const char *create_opts;

                                     
    if (strcmp(old_db->db_name, "template1") == 0)
    {
      continue;
    }

    pg_log(PG_STATUS, "%s", old_db->db_name);
    snprintf(sql_file_name, sizeof(sql_file_name), DB_DUMP_FILE_MASK, old_db->db_oid);
    snprintf(log_file_name, sizeof(log_file_name), DB_DUMP_LOG_FILE_MASK, old_db->db_oid);

       
                                                                         
                                                                   
                                                                 
                   
       
    if (strcmp(old_db->db_name, "postgres") == 0)
    {
      create_opts = "--clean --create";
    }
    else
    {
      create_opts = "--create";
    }

    parallel_exec_prog(log_file_name, NULL,
        "\"%s/pg_restore\" %s %s --exit-on-error --verbose "
        "--dbname template1 \"%s\"",
        new_cluster.bindir, cluster_conn_opts(&new_cluster), create_opts, sql_file_name);
  }

                         
  while (reap_child(true) == true)
    ;

  end_progress_output();
  check_ok();

     
                                                                            
                                                     
     
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 902)
  {
    set_frozenxids(true);
  }

                                                                         
  get_db_and_rel_infos(&new_cluster);
}

   
                                                               
   
static void
remove_new_subdir(const char *subdir, bool rmtopdir)
{
  char new_path[MAXPGPATH];

  prep_status("Deleting files from new %s", subdir);

  snprintf(new_path, sizeof(new_path), "%s/%s", new_cluster.pgdata, subdir);
  if (!rmtree(new_path, rmtopdir))
  {
    pg_fatal("could not delete directory \"%s\"\n", new_path);
  }

  check_ok();
}

   
                                               
   
static void
copy_subdir_files(const char *old_subdir, const char *new_subdir)
{
  char old_path[MAXPGPATH];
  char new_path[MAXPGPATH];

  remove_new_subdir(new_subdir, true);

  snprintf(old_path, sizeof(old_path), "%s/%s", old_cluster.pgdata, old_subdir);
  snprintf(new_path, sizeof(new_path), "%s/%s", new_cluster.pgdata, new_subdir);

  prep_status("Copying old %s to new server", old_subdir);

  exec_prog(UTILITY_LOG_FILE, NULL, true, true,
#ifndef WIN32
      "cp -Rf \"%s\" \"%s\"",
#else
                                                                     
      "xcopy /e /y /q /r \"%s\" \"%s\\\"",
#endif
      old_path, new_path);

  check_ok();
}

static void
copy_xact_xlog_xid(void)
{
     
                                                                       
                                  
     
  copy_subdir_files(GET_MAJOR_VERSION(old_cluster.major_version) <= 906 ? "pg_clog" : "pg_xact", GET_MAJOR_VERSION(new_cluster.major_version) <= 906 ? "pg_clog" : "pg_xact");

  prep_status("Setting oldest XID for new cluster");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -f -u %u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_oldstxid, new_cluster.pgdata);
  check_ok();

                                                                
  prep_status("Setting next transaction ID and epoch for new cluster");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -f -x %u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_nxtxid, new_cluster.pgdata);
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -f -e %u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_nxtepoch, new_cluster.pgdata);
                                               
  exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -f -c %u,%u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_nxtxid, old_cluster.controldata.chkpnt_nxtxid, new_cluster.pgdata);
  check_ok();

     
                                                                           
                                                                        
                                                                         
                                                                        
     
  if (old_cluster.controldata.cat_ver >= MULTIXACT_FORMATCHANGE_CAT_VER && new_cluster.controldata.cat_ver >= MULTIXACT_FORMATCHANGE_CAT_VER)
  {
    copy_subdir_files("pg_multixact/offsets", "pg_multixact/offsets");
    copy_subdir_files("pg_multixact/members", "pg_multixact/members");

    prep_status("Setting next multixact ID and offset for new cluster");

       
                                                                           
                                                             
       
    exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -O %u -m %u,%u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_nxtmxoff, old_cluster.controldata.chkpnt_nxtmulti, old_cluster.controldata.chkpnt_oldstMulti, new_cluster.pgdata);
    check_ok();
  }
  else if (new_cluster.controldata.cat_ver >= MULTIXACT_FORMATCHANGE_CAT_VER)
  {
       
                                                                         
                                                                        
                  
       
    remove_new_subdir("pg_multixact/offsets", false);

    prep_status("Setting oldest multixact ID in new cluster");

       
                                                                         
                                                                          
                                                                       
                                                                           
                                                                   
                                                                          
       
    exec_prog(UTILITY_LOG_FILE, NULL, true, true, "\"%s/pg_resetwal\" -m %u,%u \"%s\"", new_cluster.bindir, old_cluster.controldata.chkpnt_nxtmulti + 1, old_cluster.controldata.chkpnt_nxtmulti, new_cluster.pgdata);
    check_ok();
  }

                                                     
  prep_status("Resetting WAL archives");
  exec_prog(UTILITY_LOG_FILE, NULL, true, true,
                                                                       
      "\"%s/pg_resetwal\" -l 00000001%s \"%s\"", new_cluster.bindir, old_cluster.controldata.nextxlogfile + 8, new_cluster.pgdata);
  check_ok();
}

   
                    
   
                                                                      
                                                                           
                                                                             
                                                                             
                                
   
                                                                             
                                                                            
                                                                              
                                                                            
                                                                              
              
   
                                                                     
                                                                     
                                                                           
                                                                              
   
static void
set_frozenxids(bool minmxid_only)
{
  int dbnum;
  PGconn *conn, *conn_template1;
  PGresult *dbres;
  int ntups;
  int i_datname;
  int i_datallowconn;

  if (!minmxid_only)
  {
    prep_status("Setting frozenxid and minmxid counters in new cluster");
  }
  else
  {
    prep_status("Setting minmxid counter in new cluster");
  }

  conn_template1 = connectToServer(&new_cluster, "template1");

  if (!minmxid_only)
  {
                                      
    PQclear(executeQueryOrDie(conn_template1,
        "UPDATE pg_catalog.pg_database "
        "SET	datfrozenxid = '%u'",
        old_cluster.controldata.chkpnt_nxtxid));
  }

                                  
  PQclear(executeQueryOrDie(conn_template1,
      "UPDATE pg_catalog.pg_database "
      "SET	datminmxid = '%u'",
      old_cluster.controldata.chkpnt_nxtmulti));

                          
  dbres = executeQueryOrDie(conn_template1, "SELECT	datname, datallowconn "
                                            "FROM	pg_catalog.pg_database");

  i_datname = PQfnumber(dbres, "datname");
  i_datallowconn = PQfnumber(dbres, "datallowconn");

  ntups = PQntuples(dbres);
  for (dbnum = 0; dbnum < ntups; dbnum++)
  {
    char *datname = PQgetvalue(dbres, dbnum, i_datname);
    char *datallowconn = PQgetvalue(dbres, dbnum, i_datallowconn);

       
                                                                 
                                                                     
                                                                           
                                                                          
                                           
       
    if (strcmp(datallowconn, "f") == 0)
    {
      PQclear(executeQueryOrDie(conn_template1, "ALTER DATABASE %s ALLOW_CONNECTIONS = true", quote_identifier(datname)));
    }

    conn = connectToServer(&new_cluster, datname);

    if (!minmxid_only)
    {
                                     
      PQclear(executeQueryOrDie(conn,
          "UPDATE	pg_catalog.pg_class "
          "SET	relfrozenxid = '%u' "
                                                                    
          "WHERE	relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_TOASTVALUE) ")",
          old_cluster.controldata.chkpnt_nxtxid));
    }

                                 
    PQclear(executeQueryOrDie(conn,
        "UPDATE	pg_catalog.pg_class "
        "SET	relminmxid = '%u' "
                                                                  
        "WHERE	relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_TOASTVALUE) ")",
        old_cluster.controldata.chkpnt_nxtmulti));
    PQfinish(conn);

                                 
    if (strcmp(datallowconn, "f") == 0)
    {
      PQclear(executeQueryOrDie(conn_template1, "ALTER DATABASE %s ALLOW_CONNECTIONS = false", quote_identifier(datname)));
    }
  }

  PQclear(dbres);

  PQfinish(conn_template1);

  check_ok();
}

static void
cleanup(void)
{
  fclose(log_opts.internal);

                                  
  if (!log_opts.retain)
  {
    int dbnum;
    char **filename;

    for (filename = output_files; *filename != NULL; filename++)
    {
      unlink(*filename);
    }

                           
    unlink(GLOBALS_DUMP_FILE);

    if (old_cluster.dbarr.dbs)
    {
      for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
      {
        char sql_file_name[MAXPGPATH], log_file_name[MAXPGPATH];
        DbInfo *old_db = &old_cluster.dbarr.dbs[dbnum];

        snprintf(sql_file_name, sizeof(sql_file_name), DB_DUMP_FILE_MASK, old_db->db_oid);
        unlink(sql_file_name);

        snprintf(log_file_name, sizeof(log_file_name), DB_DUMP_LOG_FILE_MASK, old_db->db_oid);
        unlink(log_file_name);
      }
    }
  }
}
