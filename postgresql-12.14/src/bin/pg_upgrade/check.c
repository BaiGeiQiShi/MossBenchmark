   
           
   
                                     
   
                                                                
                              
   

#include "postgres_fe.h"

#include "catalog/pg_authid_d.h"
#include "fe_utils/string_utils.h"
#include "mb/pg_wchar.h"
#include "pg_upgrade.h"

static void
check_new_cluster_is_empty(void);
static void
check_databases_are_compatible(void);
static void
check_locale_and_encoding(DbInfo *olddb, DbInfo *newdb);
static bool
equivalent_locale(int category, const char *loca, const char *locb);
static void
check_is_install_user(ClusterInfo *cluster);
static void
check_proper_datallowconn(ClusterInfo *cluster);
static void
check_for_prepared_transactions(ClusterInfo *cluster);
static void
check_for_isn_and_int8_passing_mismatch(ClusterInfo *cluster);
static void
check_for_tables_with_oids(ClusterInfo *cluster);
static void
check_for_composite_data_type_usage(ClusterInfo *cluster);
static void
check_for_reg_data_type_usage(ClusterInfo *cluster);
static void
check_for_jsonb_9_4_usage(ClusterInfo *cluster);
static void
check_for_pg_role_prefix(ClusterInfo *cluster);
static void
check_for_new_tablespace_dir(ClusterInfo *new_cluster);
static char *
get_canonical_locale_name(int category, const char *locale);

   
                      
                                              
                                                        
                                                         
                       
   
static char *
fix_path_separator(char *path)
{
#ifdef WIN32

  char *result;
  char *c;

  result = pg_strdup(path);

  for (c = result; *c != '\0'; c++)
  {
    if (*c == '/')
    {
      *c = '\\';
    }
  }

  return result;
#else

  return path;
#endif
}

void
output_check_banner(bool live_check)
{
  if (user_opts.check && live_check)
  {
    pg_log(PG_REPORT, "Performing Consistency Checks on Old Live Server\n"
                      "------------------------------------------------\n");
  }
  else
  {
    pg_log(PG_REPORT, "Performing Consistency Checks\n"
                      "-----------------------------\n");
  }
}

void
check_and_dump_old_cluster(bool live_check)
{
                 

  if (!live_check)
  {
    start_postmaster(&old_cluster, true);
  }

                                                                   
  get_db_and_rel_infos(&old_cluster);

  init_tablespaces();

  get_loadable_libraries();

     
                                     
     
  check_is_install_user(&old_cluster);
  check_proper_datallowconn(&old_cluster);
  check_for_prepared_transactions(&old_cluster);
  check_for_composite_data_type_usage(&old_cluster);
  check_for_reg_data_type_usage(&old_cluster);
  check_for_isn_and_int8_passing_mismatch(&old_cluster);

     
                                                                     
                                                               
     
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 1100)
  {
    check_for_tables_with_oids(&old_cluster);
  }

     
                                                                          
                                                                            
                                                                          
     
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 1100)
  {
    old_11_check_for_sql_identifier_data_type_usage(&old_cluster);
  }

     
                                                                             
                  
     
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 906)
  {
    old_9_6_check_for_unknown_data_type_usage(&old_cluster);
    if (user_opts.check)
    {
      old_9_6_invalidate_hash_indexes(&old_cluster, true);
    }
  }

                                                             
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 905)
  {
    check_for_pg_role_prefix(&old_cluster);
  }

  if (GET_MAJOR_VERSION(old_cluster.major_version) == 904 && old_cluster.controldata.cat_ver < JSONB_FORMAT_CHANGE_CAT_VER)
  {
    check_for_jsonb_9_4_usage(&old_cluster);
  }

                                                                   
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 903)
  {
    old_9_3_check_for_line_data_type_usage(&old_cluster);
  }

                                                  
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 804)
  {
    new_9_0_populate_pg_largeobject_metadata(&old_cluster, true);
  }

     
                                                                            
                                
     
  if (!user_opts.check)
  {
    generate_old_dump();
  }

  if (!live_check)
  {
    stop_postmaster(false);
  }
}

void
check_new_cluster(void)
{
  get_db_and_rel_infos(&new_cluster);

  check_new_cluster_is_empty();
  check_databases_are_compatible();

  check_loadable_libraries();

  switch (user_opts.transfer_mode)
  {
  case TRANSFER_MODE_CLONE:
    check_file_clone();
    break;
  case TRANSFER_MODE_COPY:
    break;
  case TRANSFER_MODE_LINK:
    check_hard_link();
    break;
  }

  check_is_install_user(&new_cluster);

  check_for_prepared_transactions(&new_cluster);

  check_for_new_tablespace_dir(&new_cluster);
}

void
report_clusters_compatible(void)
{
  if (user_opts.check)
  {
    pg_log(PG_REPORT, "\n*Clusters are compatible*\n");
                           
    stop_postmaster(false);
    exit(0);
  }

  pg_log(PG_REPORT, "\n"
                    "If pg_upgrade fails after this point, you must re-initdb the\n"
                    "new cluster before continuing.\n");
}

void
issue_warnings_and_set_wal_level(void)
{
     
                                                                             
                                                                             
                                                                          
                                                
     
  start_postmaster(&new_cluster, true);

                                                               
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 804)
  {
    new_9_0_populate_pg_largeobject_metadata(&new_cluster, false);
  }

                                           
  if (GET_MAJOR_VERSION(old_cluster.major_version) <= 906)
  {
    old_9_6_invalidate_hash_indexes(&new_cluster, false);
  }

  report_extension_updates(&new_cluster);

  stop_postmaster(false);
}

void
output_completion_banner(char *analyze_script_file_name, char *deletion_script_file_name)
{
  pg_log(PG_REPORT,
      "Optimizer statistics are not transferred by pg_upgrade so,\n"
      "once you start the new server, consider running:\n"
      "    %s\n\n",
      analyze_script_file_name);

  if (deletion_script_file_name)
  {
    pg_log(PG_REPORT,
        "Running this script will delete the old cluster's data files:\n"
        "    %s\n",
        deletion_script_file_name);
  }
  else
  {
    pg_log(PG_REPORT, "Could not create a script to delete the old cluster's data files\n"
                      "because user-defined tablespaces or the new cluster's data directory\n"
                      "exist in the old cluster directory.  The old cluster's contents must\n"
                      "be deleted manually.\n");
  }
}

void
check_cluster_versions(void)
{
  prep_status("Checking cluster versions");

                                                          
  Assert(old_cluster.major_version != 0);
  Assert(new_cluster.major_version != 0);

     
                                                                     
              
     

  if (GET_MAJOR_VERSION(old_cluster.major_version) < 804)
  {
    pg_fatal("This utility can only upgrade from PostgreSQL version 8.4 and later.\n");
  }

                                                        
  if (GET_MAJOR_VERSION(new_cluster.major_version) != GET_MAJOR_VERSION(PG_VERSION_NUM))
  {
    pg_fatal("This utility can only upgrade to PostgreSQL version %s.\n", PG_MAJORVERSION);
  }

     
                                                                       
                                                                         
                     
     
  if (old_cluster.major_version > new_cluster.major_version)
  {
    pg_fatal("This utility cannot be used to downgrade to older major PostgreSQL versions.\n");
  }

                                                             
  if (GET_MAJOR_VERSION(old_cluster.major_version) != GET_MAJOR_VERSION(old_cluster.bin_version))
  {
    pg_fatal("Old cluster data and binary directories are from different major versions.\n");
  }
  if (GET_MAJOR_VERSION(new_cluster.major_version) != GET_MAJOR_VERSION(new_cluster.bin_version))
  {
    pg_fatal("New cluster data and binary directories are from different major versions.\n");
  }

  check_ok();
}

void
check_cluster_compatibility(bool live_check)
{
                                            
  get_control_data(&old_cluster, live_check);
  get_control_data(&new_cluster, false);
  check_control_data(&old_cluster.controldata, &new_cluster.controldata);

                                                  
  if (live_check && GET_MAJOR_VERSION(old_cluster.major_version) <= 900 && old_cluster.port == DEF_PGUPORT)
  {
    pg_fatal("When checking a pre-PG 9.1 live old server, "
             "you must specify the old server's port number.\n");
  }

  if (live_check && old_cluster.port == new_cluster.port)
  {
    pg_fatal("When checking a live server, "
             "the old and new port numbers must be different.\n");
  }
}

   
                               
   
                                                                            
                   
   
static void
check_locale_and_encoding(DbInfo *olddb, DbInfo *newdb)
{
  if (olddb->db_encoding != newdb->db_encoding)
  {
    pg_fatal("encodings for database \"%s\" do not match:  old \"%s\", new \"%s\"\n", olddb->db_name, pg_encoding_to_char(olddb->db_encoding), pg_encoding_to_char(newdb->db_encoding));
  }
  if (!equivalent_locale(LC_COLLATE, olddb->db_collate, newdb->db_collate))
  {
    pg_fatal("lc_collate values for database \"%s\" do not match:  old \"%s\", new \"%s\"\n", olddb->db_name, olddb->db_collate, newdb->db_collate);
  }
  if (!equivalent_locale(LC_CTYPE, olddb->db_ctype, newdb->db_ctype))
  {
    pg_fatal("lc_ctype values for database \"%s\" do not match:  old \"%s\", new \"%s\"\n", olddb->db_name, olddb->db_ctype, newdb->db_ctype);
  }
}

   
                       
   
                                                                             
                               
   
                                                                       
                                                                     
                                                                            
                                   
   
static bool
equivalent_locale(int category, const char *loca, const char *locb)
{
  const char *chara;
  const char *charb;
  char *canona;
  char *canonb;
  int lena;
  int lenb;

     
                                                                             
                                                                             
                                                                
     
  if (pg_strcasecmp(loca, locb) == 0)
  {
    return true;
  }

     
                                                                            
                
     
  canona = get_canonical_locale_name(category, loca);
  chara = strrchr(canona, '.');
  lena = chara ? (chara - canona) : strlen(canona);

  canonb = get_canonical_locale_name(category, locb);
  charb = strrchr(canonb, '.');
  lenb = charb ? (charb - canonb) : strlen(canonb);

  if (lena == lenb && pg_strncasecmp(canona, canonb, lena) == 0)
  {
    pg_free(canona);
    pg_free(canonb);
    return true;
  }

  pg_free(canona);
  pg_free(canonb);
  return false;
}

static void
check_new_cluster_is_empty(void)
{
  int dbnum;

  for (dbnum = 0; dbnum < new_cluster.dbarr.ndbs; dbnum++)
  {
    int relnum;
    RelInfoArr *rel_arr = &new_cluster.dbarr.dbs[dbnum].rel_arr;

    for (relnum = 0; relnum < rel_arr->nrels; relnum++)
    {
                                                          
      if (strcmp(rel_arr->rels[relnum].nspname, "pg_catalog") != 0)
      {
        pg_fatal("New cluster database \"%s\" is not empty: found relation \"%s.%s\"\n", new_cluster.dbarr.dbs[dbnum].db_name, rel_arr->rels[relnum].nspname, rel_arr->rels[relnum].relname);
      }
    }
  }
}

   
                                                                       
                                                              
   
static void
check_databases_are_compatible(void)
{
  int newdbnum;
  int olddbnum;
  DbInfo *newdbinfo;
  DbInfo *olddbinfo;

  for (newdbnum = 0; newdbnum < new_cluster.dbarr.ndbs; newdbnum++)
  {
    newdbinfo = &new_cluster.dbarr.dbs[newdbnum];

                                                            
    for (olddbnum = 0; olddbnum < old_cluster.dbarr.ndbs; olddbnum++)
    {
      olddbinfo = &old_cluster.dbarr.dbs[olddbnum];
      if (strcmp(newdbinfo->db_name, olddbinfo->db_name) == 0)
      {
        check_locale_and_encoding(olddbinfo, newdbinfo);
        break;
      }
    }
  }
}

   
                                       
   
                                                            
   
void
create_script_for_cluster_analyze(char **analyze_script_file_name)
{
  FILE *script = NULL;
  PQExpBufferData user_specification;

  prep_status("Creating script to analyze new cluster");

  initPQExpBuffer(&user_specification);
  if (os_info.user_specified)
  {
    appendPQExpBufferStr(&user_specification, "-U ");
    appendShellString(&user_specification, os_info.user);
    appendPQExpBufferChar(&user_specification, ' ');
  }

  *analyze_script_file_name = psprintf("%sanalyze_new_cluster.%s", SCRIPT_PREFIX, SCRIPT_EXT);

  if ((script = fopen_priv(*analyze_script_file_name, "w")) == NULL)
  {
    pg_fatal("could not open file \"%s\": %s\n", *analyze_script_file_name, strerror(errno));
  }

#ifndef WIN32
                          
  fprintf(script, "#!/bin/sh\n\n");
#else
                                
  fprintf(script, "@echo off\n");
#endif

  fprintf(script, "echo %sThis script will generate minimal optimizer statistics rapidly%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %sso your system is usable, and then gather statistics twice more%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %swith increasing accuracy.  When it is done, your system will%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %shave the default level of optimizer statistics.%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo%s\n\n", ECHO_BLANK);

  fprintf(script, "echo %sIf you have used ALTER TABLE to modify the statistics target for%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %sany tables, you might want to remove them and restore them after%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %srunning this script because they will delay fast statistics generation.%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo%s\n\n", ECHO_BLANK);

  fprintf(script, "echo %sIf you would like default statistics as quickly as possible, cancel%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %sthis script and run:%s\n", ECHO_QUOTE, ECHO_QUOTE);
  fprintf(script, "echo %s    \"%s/vacuumdb\" %s--all --analyze-only%s\n", ECHO_QUOTE, new_cluster.bindir, user_specification.data, ECHO_QUOTE);
  fprintf(script, "echo%s\n\n", ECHO_BLANK);

  fprintf(script, "\"%s/vacuumdb\" %s--all --analyze-in-stages\n", new_cluster.bindir, user_specification.data);

  fprintf(script, "echo%s\n\n", ECHO_BLANK);
  fprintf(script, "echo %sDone%s\n", ECHO_QUOTE, ECHO_QUOTE);

  fclose(script);

#ifndef WIN32
  if (chmod(*analyze_script_file_name, S_IRWXU) != 0)
  {
    pg_fatal("could not add execute permission to file \"%s\": %s\n", *analyze_script_file_name, strerror(errno));
  }
#endif

  termPQExpBuffer(&user_specification);

  check_ok();
}

   
                                                                      
                                                                
                                                                    
                                                                
                                                                    
                                                                     
                          
   
                                                                     
                                                                    
                                                                       
           
   
static void
check_for_new_tablespace_dir(ClusterInfo *new_cluster)
{
  int tblnum;
  char new_tablespace_dir[MAXPGPATH];

  prep_status("Checking for new cluster tablespace directories");

  for (tblnum = 0; tblnum < os_info.num_old_tablespaces; tblnum++)
  {
    struct stat statbuf;

    snprintf(new_tablespace_dir, MAXPGPATH, "%s%s", os_info.old_tablespaces[tblnum], new_cluster->tablespace_suffix);

    if (stat(new_tablespace_dir, &statbuf) == 0 || errno != ENOENT)
    {
      pg_fatal("new cluster tablespace directory already exists: \"%s\"\n", new_tablespace_dir);
    }
  }

  check_ok();
}

   
                                            
   
                                                        
   
void
create_script_for_old_cluster_deletion(char **deletion_script_file_name)
{
  FILE *script = NULL;
  int tblnum;
  char old_cluster_pgdata[MAXPGPATH], new_cluster_pgdata[MAXPGPATH];

  *deletion_script_file_name = psprintf("%sdelete_old_cluster.%s", SCRIPT_PREFIX, SCRIPT_EXT);

  strlcpy(old_cluster_pgdata, old_cluster.pgdata, MAXPGPATH);
  canonicalize_path(old_cluster_pgdata);

  strlcpy(new_cluster_pgdata, new_cluster.pgdata, MAXPGPATH);
  canonicalize_path(new_cluster_pgdata);

                                                                  
  if (path_is_prefix_of_path(old_cluster_pgdata, new_cluster_pgdata))
  {
    pg_log(PG_WARNING, "\nWARNING:  new data directory should not be inside the old data directory, e.g. %s\n", old_cluster_pgdata);

                                                                  
    unlink(*deletion_script_file_name);
    pg_free(*deletion_script_file_name);
    *deletion_script_file_name = NULL;
    return;
  }

     
                                                                   
                                                                            
           
     
  for (tblnum = 0; tblnum < os_info.num_old_tablespaces; tblnum++)
  {
    char old_tablespace_dir[MAXPGPATH];

    strlcpy(old_tablespace_dir, os_info.old_tablespaces[tblnum], MAXPGPATH);
    canonicalize_path(old_tablespace_dir);
    if (path_is_prefix_of_path(old_cluster_pgdata, old_tablespace_dir))
    {
                                                                       
      pg_log(PG_WARNING, "\nWARNING:  user-defined tablespace locations should not be inside the data directory, e.g. %s\n", old_tablespace_dir);

                                                                    
      unlink(*deletion_script_file_name);
      pg_free(*deletion_script_file_name);
      *deletion_script_file_name = NULL;
      return;
    }
  }

  prep_status("Creating script to delete old cluster");

  if ((script = fopen_priv(*deletion_script_file_name, "w")) == NULL)
  {
    pg_fatal("could not open file \"%s\": %s\n", *deletion_script_file_name, strerror(errno));
  }

#ifndef WIN32
                          
  fprintf(script, "#!/bin/sh\n\n");
#endif

                                               
  fprintf(script, RMDIR_CMD " %c%s%c\n", PATH_QUOTE, fix_path_separator(old_cluster.pgdata), PATH_QUOTE);

                                                  
  for (tblnum = 0; tblnum < os_info.num_old_tablespaces; tblnum++)
  {
       
                                                                       
                                               
       
    if (strlen(old_cluster.tablespace_suffix) == 0)
    {
                                           
      int dbnum;

      fprintf(script, "\n");
                              
      if (GET_MAJOR_VERSION(old_cluster.major_version) <= 804)
      {
        fprintf(script, RM_CMD " %s%cPG_VERSION\n", fix_path_separator(os_info.old_tablespaces[tblnum]), PATH_SEPARATOR);
      }

      for (dbnum = 0; dbnum < old_cluster.dbarr.ndbs; dbnum++)
      {
        fprintf(script, RMDIR_CMD " %c%s%c%d%c\n", PATH_QUOTE, fix_path_separator(os_info.old_tablespaces[tblnum]), PATH_SEPARATOR, old_cluster.dbarr.dbs[dbnum].db_oid, PATH_QUOTE);
      }
    }
    else
    {
      char *suffix_path = pg_strdup(old_cluster.tablespace_suffix);

         
                                                                       
                                             
         
      fprintf(script, RMDIR_CMD " %c%s%s%c\n", PATH_QUOTE, fix_path_separator(os_info.old_tablespaces[tblnum]), fix_path_separator(suffix_path), PATH_QUOTE);
      pfree(suffix_path);
    }
  }

  fclose(script);

#ifndef WIN32
  if (chmod(*deletion_script_file_name, S_IRWXU) != 0)
  {
    pg_fatal("could not add execute permission to file \"%s\": %s\n", *deletion_script_file_name, strerror(errno));
  }
#endif

  check_ok();
}

   
                           
   
                                                           
                       
   
static void
check_is_install_user(ClusterInfo *cluster)
{
  PGresult *res;
  PGconn *conn = connectToServer(cluster, "template1");

  prep_status("Checking database user is the install user");

                                                                
  res = executeQueryOrDie(conn, "SELECT rolsuper, oid "
                                "FROM pg_catalog.pg_roles "
                                "WHERE rolname = current_user "
                                "AND rolname !~ '^pg_'");

     
                                                                           
                                                                            
                      
     
  if (PQntuples(res) != 1 || atooid(PQgetvalue(res, 0, 1)) != BOOTSTRAP_SUPERUSERID)
  {
    pg_fatal("database user \"%s\" is not the install user\n", os_info.user);
  }

  PQclear(res);

  res = executeQueryOrDie(conn, "SELECT COUNT(*) "
                                "FROM pg_catalog.pg_roles "
                                "WHERE rolname !~ '^pg_'");

  if (PQntuples(res) != 1)
  {
    pg_fatal("could not determine the number of users\n");
  }

     
                                                                             
                                                                        
                                   
     
  if (cluster == &new_cluster && atooid(PQgetvalue(res, 0, 0)) != 1)
  {
    pg_fatal("Only the install user can be defined in the new cluster.\n");
  }

  PQclear(res);

  PQfinish(conn);

  check_ok();
}

static void
check_proper_datallowconn(ClusterInfo *cluster)
{
  int dbnum;
  PGconn *conn_template1;
  PGresult *dbres;
  int ntups;
  int i_datname;
  int i_datallowconn;

  prep_status("Checking database connection settings");

  conn_template1 = connectToServer(cluster, "template1");

                          
  dbres = executeQueryOrDie(conn_template1, "SELECT	datname, datallowconn "
                                            "FROM	pg_catalog.pg_database");

  i_datname = PQfnumber(dbres, "datname");
  i_datallowconn = PQfnumber(dbres, "datallowconn");

  ntups = PQntuples(dbres);
  for (dbnum = 0; dbnum < ntups; dbnum++)
  {
    char *datname = PQgetvalue(dbres, dbnum, i_datname);
    char *datallowconn = PQgetvalue(dbres, dbnum, i_datallowconn);

    if (strcmp(datname, "template0") == 0)
    {
                                                                           
      if (strcmp(datallowconn, "t") == 0)
      {
        pg_fatal("template0 must not allow connections, "
                 "i.e. its pg_database.datallowconn must be false\n");
      }
    }
    else
    {
         
                                                                     
                 
         
      if (strcmp(datallowconn, "f") == 0)
      {
        pg_fatal("All non-template0 databases must allow connections, "
                 "i.e. their pg_database.datallowconn must be true\n");
      }
    }
  }

  PQclear(dbres);

  PQfinish(conn_template1);

  check_ok();
}

   
                                     
   
                                                                           
                       
   
static void
check_for_prepared_transactions(ClusterInfo *cluster)
{
  PGresult *res;
  PGconn *conn = connectToServer(cluster, "template1");

  prep_status("Checking for prepared transactions");

  res = executeQueryOrDie(conn, "SELECT * "
                                "FROM pg_catalog.pg_prepared_xacts");

  if (PQntuples(res) != 0)
  {
    if (cluster == &old_cluster)
    {
      pg_fatal("The source cluster contains prepared transactions\n");
    }
    else
    {
      pg_fatal("The target cluster contains prepared transactions\n");
    }
  }

  PQclear(res);

  PQfinish(conn);

  check_ok();
}

   
                                             
   
                                                                           
                                                                        
                                              
   
static void
check_for_isn_and_int8_passing_mismatch(ClusterInfo *cluster)
{
  int dbnum;
  FILE *script = NULL;
  bool found = false;
  char output_path[MAXPGPATH];

  prep_status("Checking for contrib/isn with bigint-passing mismatch");

  if (old_cluster.controldata.float8_pass_by_value == new_cluster.controldata.float8_pass_by_value)
  {
                     
    check_ok();
    return;
  }

  snprintf(output_path, sizeof(output_path), "contrib_isn_and_int8_pass_by_value.txt");

  for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
  {
    PGresult *res;
    bool db_used = false;
    int ntups;
    int rowno;
    int i_nspname, i_proname;
    DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
    PGconn *conn = connectToServer(cluster, active_db->db_name);

                                                    
    res = executeQueryOrDie(conn, "SELECT n.nspname, p.proname "
                                  "FROM	pg_catalog.pg_proc p, "
                                  "		pg_catalog.pg_namespace n "
                                  "WHERE	p.pronamespace = n.oid AND "
                                  "		p.probin = '$libdir/isn'");

    ntups = PQntuples(res);
    i_nspname = PQfnumber(res, "nspname");
    i_proname = PQfnumber(res, "proname");
    for (rowno = 0; rowno < ntups; rowno++)
    {
      found = true;
      if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
      {
        pg_fatal("could not open file \"%s\": %s\n", output_path, strerror(errno));
      }
      if (!db_used)
      {
        fprintf(script, "Database: %s\n", active_db->db_name);
        db_used = true;
      }
      fprintf(script, "  %s.%s\n", PQgetvalue(res, rowno, i_nspname), PQgetvalue(res, rowno, i_proname));
    }

    PQclear(res);

    PQfinish(conn);
  }

  if (script)
  {
    fclose(script);
  }

  if (found)
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains \"contrib/isn\" functions which rely on the\n"
             "bigint data type.  Your old and new clusters pass bigint values\n"
             "differently so this cluster cannot currently be upgraded.  You can\n"
             "manually upgrade databases that use \"contrib/isn\" facilities and remove\n"
             "\"contrib/isn\" from the old cluster and restart the upgrade.  A list of\n"
             "the problem functions is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                                                 
   
static void
check_for_tables_with_oids(ClusterInfo *cluster)
{
  int dbnum;
  FILE *script = NULL;
  bool found = false;
  char output_path[MAXPGPATH];

  prep_status("Checking for tables WITH OIDS");

  snprintf(output_path, sizeof(output_path), "tables_with_oids.txt");

                                          
  for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
  {
    PGresult *res;
    bool db_used = false;
    int ntups;
    int rowno;
    int i_nspname, i_relname;
    DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
    PGconn *conn = connectToServer(cluster, active_db->db_name);

    res = executeQueryOrDie(conn, "SELECT n.nspname, c.relname "
                                  "FROM	pg_catalog.pg_class c, "
                                  "		pg_catalog.pg_namespace n "
                                  "WHERE	c.relnamespace = n.oid AND "
                                  "		c.relhasoids AND"
                                  "       n.nspname NOT IN ('pg_catalog')");

    ntups = PQntuples(res);
    i_nspname = PQfnumber(res, "nspname");
    i_relname = PQfnumber(res, "relname");
    for (rowno = 0; rowno < ntups; rowno++)
    {
      found = true;
      if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
      {
        pg_fatal("could not open file \"%s\": %s\n", output_path, strerror(errno));
      }
      if (!db_used)
      {
        fprintf(script, "Database: %s\n", active_db->db_name);
        db_used = true;
      }
      fprintf(script, "  %s.%s\n", PQgetvalue(res, rowno, i_nspname), PQgetvalue(res, rowno, i_relname));
    }

    PQclear(res);

    PQfinish(conn);
  }

  if (script)
  {
    fclose(script);
  }

  if (found)
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains tables declared WITH OIDS, which is not supported\n"
             "anymore. Consider removing the oid column using\n"
             "    ALTER TABLE ... SET WITHOUT OIDS;\n"
             "A list of tables with the problem is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                                         
                                                                 
   
                                                                        
                                                                        
                                                                    
                                                                         
   
static void
check_for_composite_data_type_usage(ClusterInfo *cluster)
{
  bool found;
  Oid firstUserOid;
  char output_path[MAXPGPATH];
  char *base_query;

  prep_status("Checking for system-defined composite types in user tables");

  snprintf(output_path, sizeof(output_path), "tables_using_composite.txt");

     
                                                                          
                                                                         
                           
     
                                                                    
                                                                           
                                                                          
                                                                             
                                          
     
  firstUserOid = 16384;

  base_query = psprintf("SELECT t.oid FROM pg_catalog.pg_type t "
                        "LEFT JOIN pg_catalog.pg_namespace n ON t.typnamespace = n.oid "
                        " WHERE typtype = 'c' AND (t.oid < %u OR nspname = 'information_schema')",
      firstUserOid);

  found = check_for_data_types_usage(cluster, base_query, output_path);

  free(base_query);

  if (found)
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains system-defined composite type(s) in user tables.\n"
             "These type OIDs are not stable across PostgreSQL versions,\n"
             "so this cluster cannot currently be upgraded.  You can\n"
             "drop the problem columns and restart the upgrade.\n"
             "A list of the problem columns is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                                   
                                                  
                 
                
                
   
                                                                     
                                                                    
                                  
   
static void
check_for_reg_data_type_usage(ClusterInfo *cluster)
{
  bool found;
  char output_path[MAXPGPATH];

  prep_status("Checking for reg* data types in user tables");

  snprintf(output_path, sizeof(output_path), "tables_using_reg.txt");

     
                                                                           
                                                                             
     
  found = check_for_data_types_usage(cluster,
      "SELECT oid FROM pg_catalog.pg_type t "
      "WHERE t.typnamespace = "
      "        (SELECT oid FROM pg_catalog.pg_namespace "
      "         WHERE nspname = 'pg_catalog') "
      "  AND t.typname IN ( "
                                                          
      "           'regcollation', "
      "           'regconfig', "
      "           'regdictionary', "
      "           'regnamespace', "
      "           'regoper', "
      "           'regoperator', "
      "           'regproc', "
      "           'regprocedure' "
                                                          
                                                                 
      "         )",
      output_path);

  if (found)
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains one of the reg* data types in user tables.\n"
             "These data types reference system OIDs that are not preserved by\n"
             "pg_upgrade, so this cluster cannot currently be upgraded.  You can\n"
             "remove the problem tables and restart the upgrade.  A list of the problem\n"
             "columns is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                               
   
                                                                      
   
static void
check_for_jsonb_9_4_usage(ClusterInfo *cluster)
{
  char output_path[MAXPGPATH];

  prep_status("Checking for incompatible \"jsonb\" data type");

  snprintf(output_path, sizeof(output_path), "tables_using_jsonb.txt");

  if (check_for_data_type_usage(cluster, "pg_catalog.jsonb", output_path))
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains the \"jsonb\" data type in user tables.\n"
             "The internal format of \"jsonb\" changed during 9.4 beta so this cluster cannot currently\n"
             "be upgraded.  You can remove the problem tables and restart the upgrade.  A list\n"
             "of the problem columns is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                              
   
                                                          
   
static void
check_for_pg_role_prefix(ClusterInfo *cluster)
{
  PGresult *res;
  PGconn *conn = connectToServer(cluster, "template1");

  prep_status("Checking for roles starting with \"pg_\"");

  res = executeQueryOrDie(conn, "SELECT * "
                                "FROM pg_catalog.pg_roles "
                                "WHERE rolname ~ '^pg_'");

  if (PQntuples(res) != 0)
  {
    if (cluster == &old_cluster)
    {
      pg_fatal("The source cluster contains roles starting with \"pg_\"\n");
    }
    else
    {
      pg_fatal("The target cluster contains roles starting with \"pg_\"\n");
    }
  }

  PQclear(res);

  PQfinish(conn);

  check_ok();
}

   
                             
   
                                                                        
                                                                      
   
static char *
get_canonical_locale_name(int category, const char *locale)
{
  char *save;
  char *res;

                                                      
  save = setlocale(category, NULL);
  if (!save)
  {
    pg_fatal("failed to get the current locale\n");
  }

                                                                            
  save = pg_strdup(save);

                                                               
  res = setlocale(category, locale);

  if (!res)
  {
    pg_fatal("failed to get system locale name for \"%s\"\n", locale);
  }

  res = pg_strdup(res);

                          
  if (!setlocale(category, save))
  {
    pg_fatal("failed to restore old locale \"%s\"\n", save);
  }

  pg_free(save);

  return res;
}
