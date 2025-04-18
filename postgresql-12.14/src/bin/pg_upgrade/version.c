   
             
   
                                      
   
                                                                
                                
   

#include "postgres_fe.h"

#include "pg_upgrade.h"

#include "catalog/pg_class_d.h"
#include "fe_utils/string_utils.h"

   
                                              
                          
                                                 
   
void
new_9_0_populate_pg_largeobject_metadata(ClusterInfo *cluster, bool check_mode)
{
  int dbnum;
  FILE *script = NULL;
  bool found = false;
  char output_path[MAXPGPATH];

  prep_status("Checking for large objects");

  snprintf(output_path, sizeof(output_path), "pg_largeobject.sql");

  for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
  {
    PGresult *res;
    int i_count;
    DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
    PGconn *conn = connectToServer(cluster, active_db->db_name);

                                             
    res = executeQueryOrDie(conn, "SELECT count(*) "
                                  "FROM	pg_catalog.pg_largeobject ");

    i_count = PQfnumber(res, "count");
    if (atoi(PQgetvalue(res, 0, i_count)) != 0)
    {
      found = true;
      if (!check_mode)
      {
        PQExpBufferData connectbuf;

        if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
        {
          pg_fatal("could not open file \"%s\": %s\n", output_path, strerror(errno));
        }

        initPQExpBuffer(&connectbuf);
        appendPsqlMetaConnect(&connectbuf, active_db->db_name);
        fputs(connectbuf.data, script);
        termPQExpBuffer(&connectbuf);

        fprintf(script, "SELECT pg_catalog.lo_create(t.loid)\n"
                        "FROM (SELECT DISTINCT loid FROM pg_catalog.pg_largeobject) AS t;\n");
      }
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
    report_status(PG_WARNING, "warning");
    if (check_mode)
    {
      pg_log(PG_WARNING, "\n"
                         "Your installation contains large objects.  The new database has an\n"
                         "additional large object permission table.  After upgrading, you will be\n"
                         "given a command to populate the pg_largeobject_metadata table with\n"
                         "default permissions.\n\n");
    }
    else
    {
      pg_log(PG_WARNING,
          "\n"
          "Your installation contains large objects.  The new database has an\n"
          "additional large object permission table, so default permissions must be\n"
          "defined for all large objects.  The file\n"
          "    %s\n"
          "when executed by psql by the database superuser will set the default\n"
          "permissions.\n\n",
          output_path);
    }
  }
  else
  {
    check_ok();
  }
}

   
                                
                                                                          
   
                                                                  
   
                                                                       
                                                                           
                                                                
   
                                                                             
                                          
   
bool
check_for_data_types_usage(ClusterInfo *cluster, const char *base_query, const char *output_path)
{
  bool found = false;
  FILE *script = NULL;
  int dbnum;

  for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
  {
    DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
    PGconn *conn = connectToServer(cluster, active_db->db_name);
    PQExpBufferData querybuf;
    PGresult *res;
    bool db_used = false;
    int ntups;
    int rowno;
    int i_nspname, i_relname, i_attname;

       
                                                                    
                                                                        
                                                                      
                                                                          
       
    initPQExpBuffer(&querybuf);
    appendPQExpBuffer(&querybuf,
        "WITH RECURSIVE oids AS ( "
                                                           
        "	%s "
        "	UNION ALL "
        "	SELECT * FROM ( "
                                                                   
        "		WITH x AS (SELECT oid FROM oids) "
                                                 
        "			SELECT t.oid FROM pg_catalog.pg_type t, x WHERE typbasetype = x.oid AND typtype = 'd' "
        "			UNION ALL "
                                                  
        "			SELECT t.oid FROM pg_catalog.pg_type t, x WHERE typelem = x.oid AND typtype = 'b' "
        "			UNION ALL "
                                                                 
        "			SELECT t.oid FROM pg_catalog.pg_type t, pg_catalog.pg_class c, pg_catalog.pg_attribute a, x "
        "			WHERE t.typtype = 'c' AND "
        "				  t.oid = c.reltype AND "
        "				  c.oid = a.attrelid AND "
        "				  NOT a.attisdropped AND "
        "				  a.atttypid = x.oid ",
        base_query);

                               
    if (GET_MAJOR_VERSION(cluster->major_version) >= 902)
    {
      appendPQExpBuffer(&querybuf, "			UNION ALL "
                                                                                   
                                   "			SELECT t.oid FROM pg_catalog.pg_type t, pg_catalog.pg_range r, x "
                                   "			WHERE t.typtype = 'r' AND r.rngtypid = t.oid AND r.rngsubtype = x.oid");
    }

    appendPQExpBuffer(&querybuf, "	) foo "
                                 ") "
                                                                                   
                                 "SELECT n.nspname, c.relname, a.attname "
                                 "FROM	pg_catalog.pg_class c, "
                                 "		pg_catalog.pg_namespace n, "
                                 "		pg_catalog.pg_attribute a "
                                 "WHERE	c.oid = a.attrelid AND "
                                 "		NOT a.attisdropped AND "
                                 "		a.atttypid IN (SELECT oid FROM oids) AND "
                                 "		c.relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_MATVIEW) ", " CppAsString2(RELKIND_INDEX) ") AND "
                                                                                                                                                                   "		c.relnamespace = n.oid AND "
                                                                                                                                                                                                              
                                                                                                                                                                   "		n.nspname !~ '^pg_temp_' AND "
                                                                                                                                                                   "		n.nspname !~ '^pg_toast_temp_' AND "
                                                                                                                                                                                                     
                                                                                                                                                                   "		n.nspname NOT IN ('pg_catalog', 'information_schema')");

    res = executeQueryOrDie(conn, "%s", querybuf.data);

    ntups = PQntuples(res);
    i_nspname = PQfnumber(res, "nspname");
    i_relname = PQfnumber(res, "relname");
    i_attname = PQfnumber(res, "attname");
    for (rowno = 0; rowno < ntups; rowno++)
    {
      found = true;
      if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
      {
        pg_fatal("could not open file \"%s\": %s\n", output_path, strerror(errno));
      }
      if (!db_used)
      {
        fprintf(script, "In database: %s\n", active_db->db_name);
        db_used = true;
      }
      fprintf(script, "  %s.%s.%s\n", PQgetvalue(res, rowno, i_nspname), PQgetvalue(res, rowno, i_relname), PQgetvalue(res, rowno, i_attname));
    }

    PQclear(res);

    termPQExpBuffer(&querybuf);

    PQfinish(conn);
  }

  if (script)
  {
    fclose(script);
  }

  return found;
}

   
                               
                                                                           
   
                                                                  
   
                                                                    
                                                                    
                                
   
bool
check_for_data_type_usage(ClusterInfo *cluster, const char *type_name, const char *output_path)
{
  bool found;
  char *base_query;

  base_query = psprintf("SELECT '%s'::pg_catalog.regtype AS oid", type_name);

  found = check_for_data_types_usage(cluster, base_query, output_path);

  free(base_query);

  return found;
}

   
                                            
              
                                                                          
                                                                     
                                                                 
                          
   
void
old_9_3_check_for_line_data_type_usage(ClusterInfo *cluster)
{
  char output_path[MAXPGPATH];

  prep_status("Checking for incompatible \"line\" data type");

  snprintf(output_path, sizeof(output_path), "tables_using_line.txt");

  if (check_for_data_type_usage(cluster, "pg_catalog.line", output_path))
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains the \"line\" data type in user tables.  This\n"
             "data type changed its internal and input/output format between your old\n"
             "and new clusters so this cluster cannot currently be upgraded.  You can\n"
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

   
                                               
             
                                                                        
                                                                       
                                                                       
                                                                       
                                                                     
                                                                      
                                                                     
                                                                      
                                                                         
   
void
old_9_6_check_for_unknown_data_type_usage(ClusterInfo *cluster)
{
  char output_path[MAXPGPATH];

  prep_status("Checking for invalid \"unknown\" user columns");

  snprintf(output_path, sizeof(output_path), "tables_using_unknown.txt");

  if (check_for_data_type_usage(cluster, "pg_catalog.unknown", output_path))
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains the \"unknown\" data type in user tables.  This\n"
             "data type is no longer allowed in tables, so this cluster cannot currently\n"
             "be upgraded.  You can remove the problem tables and restart the upgrade.\n"
             "A list of the problem columns is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                                     
             
                                                       
   
void
old_9_6_invalidate_hash_indexes(ClusterInfo *cluster, bool check_mode)
{
  int dbnum;
  FILE *script = NULL;
  bool found = false;
  char *output_path = "reindex_hash.sql";

  prep_status("Checking for hash indexes");

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
                                  "		pg_catalog.pg_index i, "
                                  "		pg_catalog.pg_am a, "
                                  "		pg_catalog.pg_namespace n "
                                  "WHERE	i.indexrelid = c.oid AND "
                                  "		c.relam = a.oid AND "
                                  "		c.relnamespace = n.oid AND "
                                  "		a.amname = 'hash'");

    ntups = PQntuples(res);
    i_nspname = PQfnumber(res, "nspname");
    i_relname = PQfnumber(res, "relname");
    for (rowno = 0; rowno < ntups; rowno++)
    {
      found = true;
      if (!check_mode)
      {
        if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
        {
          pg_fatal("could not open file \"%s\": %s\n", output_path, strerror(errno));
        }
        if (!db_used)
        {
          PQExpBufferData connectbuf;

          initPQExpBuffer(&connectbuf);
          appendPsqlMetaConnect(&connectbuf, active_db->db_name);
          fputs(connectbuf.data, script);
          termPQExpBuffer(&connectbuf);
          db_used = true;
        }
        fprintf(script, "REINDEX INDEX %s.%s;\n", quote_identifier(PQgetvalue(res, rowno, i_nspname)), quote_identifier(PQgetvalue(res, rowno, i_relname)));
      }
    }

    PQclear(res);

    if (!check_mode && db_used)
    {
                                        
      PQclear(executeQueryOrDie(conn, "UPDATE pg_catalog.pg_index i "
                                      "SET	indisvalid = false "
                                      "FROM	pg_catalog.pg_class c, "
                                      "		pg_catalog.pg_am a, "
                                      "		pg_catalog.pg_namespace n "
                                      "WHERE	i.indexrelid = c.oid AND "
                                      "		c.relam = a.oid AND "
                                      "		c.relnamespace = n.oid AND "
                                      "		a.amname = 'hash'"));
    }

    PQfinish(conn);
  }

  if (script)
  {
    fclose(script);
  }

  if (found)
  {
    report_status(PG_WARNING, "warning");
    if (check_mode)
    {
      pg_log(PG_WARNING, "\n"
                         "Your installation contains hash indexes.  These indexes have different\n"
                         "internal formats between your old and new clusters, so they must be\n"
                         "reindexed with the REINDEX command.  After upgrading, you will be given\n"
                         "REINDEX instructions.\n\n");
    }
    else
    {
      pg_log(PG_WARNING,
          "\n"
          "Your installation contains hash indexes.  These indexes have different\n"
          "internal formats between your old and new clusters, so they must be\n"
          "reindexed with the REINDEX command.  The file\n"
          "    %s\n"
          "when executed by psql by the database superuser will recreate all invalid\n"
          "indexes; until then, none of these indexes will be used.\n\n",
          output_path);
    }
  }
  else
  {
    check_ok();
  }
}

   
                                                     
            
                                                                          
                                                                         
                                                                         
                                    
   
void
old_11_check_for_sql_identifier_data_type_usage(ClusterInfo *cluster)
{
  char output_path[MAXPGPATH];

  prep_status("Checking for invalid \"sql_identifier\" user columns");

  snprintf(output_path, sizeof(output_path), "tables_using_sql_identifier.txt");

  if (check_for_data_type_usage(cluster, "information_schema.sql_identifier", output_path))
  {
    pg_log(PG_REPORT, "fatal\n");
    pg_fatal("Your installation contains the \"sql_identifier\" data type in user tables\n"
             "and/or indexes.  The on-disk format for this data type has changed, so this\n"
             "cluster cannot currently be upgraded.  You can remove the problem tables or\n"
             "change the data type to \"name\" and restart the upgrade.\n"
             "A list of the problem columns is in the file:\n"
             "    %s\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}

   
                              
                                             
   
void
report_extension_updates(ClusterInfo *cluster)
{
  int dbnum;
  FILE *script = NULL;
  bool found = false;
  char *output_path = "update_extensions.sql";

  prep_status("Checking for extension updates");

  for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
  {
    PGresult *res;
    bool db_used = false;
    int ntups;
    int rowno;
    int i_name;
    DbInfo *active_db = &cluster->dbarr.dbs[dbnum];
    PGconn *conn = connectToServer(cluster, active_db->db_name);

                                         
    res = executeQueryOrDie(conn, "SELECT name "
                                  "FROM pg_available_extensions "
                                  "WHERE installed_version != default_version");

    ntups = PQntuples(res);
    i_name = PQfnumber(res, "name");
    for (rowno = 0; rowno < ntups; rowno++)
    {
      found = true;

      if (script == NULL && (script = fopen_priv(output_path, "w")) == NULL)
      {
        pg_fatal("could not open file \"%s\": %s\n", output_path, strerror(errno));
      }
      if (!db_used)
      {
        PQExpBufferData connectbuf;

        initPQExpBuffer(&connectbuf);
        appendPsqlMetaConnect(&connectbuf, active_db->db_name);
        fputs(connectbuf.data, script);
        termPQExpBuffer(&connectbuf);
        db_used = true;
      }
      fprintf(script, "ALTER EXTENSION %s UPDATE;\n", quote_identifier(PQgetvalue(res, rowno, i_name)));
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
    report_status(PG_REPORT, "notice");
    pg_log(PG_REPORT,
        "\n"
        "Your installation contains extensions that should be updated\n"
        "with the ALTER EXTENSION command.  The file\n"
        "    %s\n"
        "when executed by psql by the database superuser will update\n"
        "these extensions.\n\n",
        output_path);
  }
  else
  {
    check_ok();
  }
}
