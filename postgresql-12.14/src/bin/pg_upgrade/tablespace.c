   
                
   
                        
   
                                                                
                                   
   

#include "postgres_fe.h"

#include "pg_upgrade.h"

static void
get_tablespace_paths(void);
static void
set_tablespace_directory_suffix(ClusterInfo *cluster);

void
init_tablespaces(void)
{
  get_tablespace_paths();

  set_tablespace_directory_suffix(&old_cluster);
  set_tablespace_directory_suffix(&new_cluster);

  if (os_info.num_old_tablespaces > 0 && strcmp(old_cluster.tablespace_suffix, new_cluster.tablespace_suffix) == 0)
  {
    pg_fatal("Cannot upgrade to/from the same system catalog version when\n"
             "using tablespaces.\n");
  }
}

   
                          
   
                                                                       
                                                              
   
static void
get_tablespace_paths(void)
{
  PGconn *conn = connectToServer(&old_cluster, "template1");
  PGresult *res;
  int tblnum;
  int i_spclocation;
  char query[QUERY_ALLOC];

  snprintf(query, sizeof(query),
      "SELECT	%s "
      "FROM	pg_catalog.pg_tablespace "
      "WHERE	spcname != 'pg_default' AND "
      "		spcname != 'pg_global'",
                                              
      (GET_MAJOR_VERSION(old_cluster.major_version) <= 901) ? "spclocation" : "pg_catalog.pg_tablespace_location(oid) AS spclocation");

  res = executeQueryOrDie(conn, "%s", query);

  if ((os_info.num_old_tablespaces = PQntuples(res)) != 0)
  {
    os_info.old_tablespaces = (char **)pg_malloc(os_info.num_old_tablespaces * sizeof(char *));
  }
  else
  {
    os_info.old_tablespaces = NULL;
  }

  i_spclocation = PQfnumber(res, "spclocation");

  for (tblnum = 0; tblnum < os_info.num_old_tablespaces; tblnum++)
  {
    struct stat statBuf;

    os_info.old_tablespaces[tblnum] = pg_strdup(PQgetvalue(res, tblnum, i_spclocation));

       
                                                                 
                                                                
                                                                  
                                                               
                                                                           
                                                                     
                                                           
       
    if (stat(os_info.old_tablespaces[tblnum], &statBuf) != 0)
    {
      if (errno == ENOENT)
      {
        report_status(PG_FATAL, "tablespace directory \"%s\" does not exist\n", os_info.old_tablespaces[tblnum]);
      }
      else
      {
        report_status(PG_FATAL, "could not stat tablespace directory \"%s\": %s\n", os_info.old_tablespaces[tblnum], strerror(errno));
      }
    }
    if (!S_ISDIR(statBuf.st_mode))
    {
      report_status(PG_FATAL, "tablespace path \"%s\" is not a directory\n", os_info.old_tablespaces[tblnum]);
    }
  }

  PQclear(res);

  PQfinish(conn);

  return;
}

static void
set_tablespace_directory_suffix(ClusterInfo *cluster)
{
  if (GET_MAJOR_VERSION(cluster->major_version) <= 804)
  {
    cluster->tablespace_suffix = pg_strdup("");
  }
  else
  {
                                                          

                                                               
    cluster->tablespace_suffix = psprintf("/PG_%s_%d", cluster->major_version_str, cluster->controldata.cat_ver);
  }
}
