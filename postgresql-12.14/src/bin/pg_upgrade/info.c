   
          
   
                                 
   
                                                                
                             
   

#include "postgres_fe.h"

#include "pg_upgrade.h"

#include "access/transam.h"
#include "catalog/pg_class_d.h"

static void
create_rel_filename_map(const char *old_data, const char *new_data, const DbInfo *old_db, const DbInfo *new_db, const RelInfo *old_rel, const RelInfo *new_rel, FileNameMap *map);
static void
report_unmatched_relation(const RelInfo *rel, const DbInfo *db, bool is_new_db);
static void
free_db_and_rel_infos(DbInfoArr *db_arr);
static void
get_db_infos(ClusterInfo *cluster);
static void
get_rel_infos(ClusterInfo *cluster, DbInfo *dbinfo);
static void
free_rel_infos(RelInfoArr *rel_arr);
static void
print_db_infos(DbInfoArr *dbinfo);
static void
print_rel_infos(RelInfoArr *rel_arr);

   
                      
   
                                                           
   
                                                                   
                            
   
FileNameMap *
gen_db_file_maps(DbInfo *old_db, DbInfo *new_db, int *nmaps, const char *old_pgdata, const char *new_pgdata)
{
  FileNameMap *maps;
  int old_relnum, new_relnum;
  int num_maps = 0;
  bool all_matched = true;

                                                                         
  maps = (FileNameMap *)pg_malloc(sizeof(FileNameMap) * old_db->rel_arr.nrels);

     
                                                                            
                                                                          
                                                          
     
  old_relnum = new_relnum = 0;
  while (old_relnum < old_db->rel_arr.nrels || new_relnum < new_db->rel_arr.nrels)
  {
    RelInfo *old_rel = (old_relnum < old_db->rel_arr.nrels) ? &old_db->rel_arr.rels[old_relnum] : NULL;
    RelInfo *new_rel = (new_relnum < new_db->rel_arr.nrels) ? &new_db->rel_arr.rels[new_relnum] : NULL;

                                                       
    if (!new_rel)
    {
         
                                                                     
                                                                 
         
      report_unmatched_relation(old_rel, old_db, false);
      all_matched = false;
      old_relnum++;
      continue;
    }
    if (!old_rel)
    {
         
                                                                         
                                                              
                                                                     
                             
         
      if (strcmp(new_rel->nspname, "pg_toast") != 0)
      {
        report_unmatched_relation(new_rel, new_db, true);
        all_matched = false;
      }
      new_relnum++;
      continue;
    }

                                  
    if (old_rel->reloid < new_rel->reloid)
    {
                                                   
      report_unmatched_relation(old_rel, old_db, false);
      all_matched = false;
      old_relnum++;
      continue;
    }
    else if (old_rel->reloid > new_rel->reloid)
    {
                                                   
      if (strcmp(new_rel->nspname, "pg_toast") != 0)
      {
        report_unmatched_relation(new_rel, new_db, true);
        all_matched = false;
      }
      new_relnum++;
      continue;
    }

       
                                                                        
                                                                      
                                               
       
                                                                    
                                                                           
                                                        
       
    if (strcmp(old_rel->nspname, new_rel->nspname) != 0 || (strcmp(old_rel->relname, new_rel->relname) != 0 && (GET_MAJOR_VERSION(old_cluster.major_version) >= 900 || strcmp(old_rel->nspname, "pg_toast") != 0)))
    {
      pg_log(PG_WARNING,
          "Relation names for OID %u in database \"%s\" do not match: "
          "old name \"%s.%s\", new name \"%s.%s\"\n",
          old_rel->reloid, old_db->db_name, old_rel->nspname, old_rel->relname, new_rel->nspname, new_rel->relname);
      all_matched = false;
      old_relnum++;
      new_relnum++;
      continue;
    }

                                    
    create_rel_filename_map(old_pgdata, new_pgdata, old_db, new_db, old_rel, new_rel, maps + num_maps);
    num_maps++;
    old_relnum++;
    new_relnum++;
  }

  if (!all_matched)
  {
    pg_fatal("Failed to match up old and new tables in database \"%s\"\n", old_db->db_name);
  }

  *nmaps = num_maps;
  return maps;
}

   
                             
   
                                                            
   
static void
create_rel_filename_map(const char *old_data, const char *new_data, const DbInfo *old_db, const DbInfo *new_db, const RelInfo *old_rel, const RelInfo *new_rel, FileNameMap *map)
{
                                                                    
  if (strlen(old_rel->tablespace) == 0)
  {
       
                                                                         
                                      
       
    map->old_tablespace = old_data;
    map->old_tablespace_suffix = "/base";
  }
  else
  {
                                                                          
    map->old_tablespace = old_rel->tablespace;
    map->old_tablespace_suffix = old_cluster.tablespace_suffix;
  }

                                       
  if (strlen(new_rel->tablespace) == 0)
  {
    map->new_tablespace = new_data;
    map->new_tablespace_suffix = "/base";
  }
  else
  {
    map->new_tablespace = new_rel->tablespace;
    map->new_tablespace_suffix = new_cluster.tablespace_suffix;
  }

  map->old_db_oid = old_db->db_oid;
  map->new_db_oid = new_db->db_oid;

     
                                                               
                                                                   
     
  map->old_relfilenode = old_rel->relfilenode;

                                                           
  map->new_relfilenode = new_rel->relfilenode;

                                                                        
  map->nspname = old_rel->nspname;
  map->relname = old_rel->relname;
}

   
                                                                      
                                  
   
static void
report_unmatched_relation(const RelInfo *rel, const DbInfo *db, bool is_new_db)
{
  Oid reloid = rel->reloid;                                
  char reldesc[1000];
  int i;

  snprintf(reldesc, sizeof(reldesc), "\"%s.%s\"", rel->nspname, rel->relname);
  if (rel->indtable)
  {
    for (i = 0; i < db->rel_arr.nrels; i++)
    {
      const RelInfo *hrel = &db->rel_arr.rels[i];

      if (hrel->reloid == rel->indtable)
      {
        snprintf(reldesc + strlen(reldesc), sizeof(reldesc) - strlen(reldesc), _(" which is an index on \"%s.%s\""), hrel->nspname, hrel->relname);
                                                              
        rel = hrel;
        break;
      }
    }
    if (i >= db->rel_arr.nrels)
    {
      snprintf(reldesc + strlen(reldesc), sizeof(reldesc) - strlen(reldesc), _(" which is an index on OID %u"), rel->indtable);
    }
  }
  if (rel->toastheap)
  {
    for (i = 0; i < db->rel_arr.nrels; i++)
    {
      const RelInfo *brel = &db->rel_arr.rels[i];

      if (brel->reloid == rel->toastheap)
      {
        snprintf(reldesc + strlen(reldesc), sizeof(reldesc) - strlen(reldesc), _(" which is the TOAST table for \"%s.%s\""), brel->nspname, brel->relname);
        break;
      }
    }
    if (i >= db->rel_arr.nrels)
    {
      snprintf(reldesc + strlen(reldesc), sizeof(reldesc) - strlen(reldesc), _(" which is the TOAST table for OID %u"), rel->toastheap);
    }
  }

  if (is_new_db)
  {
    pg_log(PG_WARNING, "No match found in old cluster for new relation with OID %u in database \"%s\": %s\n", reloid, db->db_name, reldesc);
  }
  else
  {
    pg_log(PG_WARNING, "No match found in new cluster for old relation with OID %u in database \"%s\": %s\n", reloid, db->db_name, reldesc);
  }
}

void
print_maps(FileNameMap *maps, int n_maps, const char *db_name)
{
  if (log_opts.verbose)
  {
    int mapnum;

    pg_log(PG_VERBOSE, "mappings for database \"%s\":\n", db_name);

    for (mapnum = 0; mapnum < n_maps; mapnum++)
    {
      pg_log(PG_VERBOSE, "%s.%s: %u to %u\n", maps[mapnum].nspname, maps[mapnum].relname, maps[mapnum].old_relfilenode, maps[mapnum].new_relfilenode);
    }

    pg_log(PG_VERBOSE, "\n\n");
  }
}

   
                          
   
                                                                     
                                                                
   
void
get_db_and_rel_infos(ClusterInfo *cluster)
{
  int dbnum;

  if (cluster->dbarr.dbs != NULL)
  {
    free_db_and_rel_infos(&cluster->dbarr);
  }

  get_db_infos(cluster);

  for (dbnum = 0; dbnum < cluster->dbarr.ndbs; dbnum++)
  {
    get_rel_infos(cluster, &cluster->dbarr.dbs[dbnum]);
  }

  if (cluster == &old_cluster)
  {
    pg_log(PG_VERBOSE, "\nsource databases:\n");
  }
  else
  {
    pg_log(PG_VERBOSE, "\ntarget databases:\n");
  }

  if (log_opts.verbose)
  {
    print_db_infos(&cluster->dbarr);
  }
}

   
                  
   
                                                           
              
   
static void
get_db_infos(ClusterInfo *cluster)
{
  PGconn *conn = connectToServer(cluster, "template1");
  PGresult *res;
  int ntups;
  int tupnum;
  DbInfo *dbinfos;
  int i_datname, i_oid, i_encoding, i_datcollate, i_datctype, i_spclocation;
  char query[QUERY_ALLOC];

  snprintf(query, sizeof(query),
      "SELECT d.oid, d.datname, d.encoding, d.datcollate, d.datctype, "
      "%s AS spclocation "
      "FROM pg_catalog.pg_database d "
      " LEFT OUTER JOIN pg_catalog.pg_tablespace t "
      " ON d.dattablespace = t.oid "
      "WHERE d.datallowconn = true "
                                                                
      "ORDER BY 2",
                                              
      (GET_MAJOR_VERSION(cluster->major_version) <= 901) ? "t.spclocation" : "pg_catalog.pg_tablespace_location(t.oid)");

  res = executeQueryOrDie(conn, "%s", query);

  i_oid = PQfnumber(res, "oid");
  i_datname = PQfnumber(res, "datname");
  i_encoding = PQfnumber(res, "encoding");
  i_datcollate = PQfnumber(res, "datcollate");
  i_datctype = PQfnumber(res, "datctype");
  i_spclocation = PQfnumber(res, "spclocation");

  ntups = PQntuples(res);
  dbinfos = (DbInfo *)pg_malloc(sizeof(DbInfo) * ntups);

  for (tupnum = 0; tupnum < ntups; tupnum++)
  {
    dbinfos[tupnum].db_oid = atooid(PQgetvalue(res, tupnum, i_oid));
    dbinfos[tupnum].db_name = pg_strdup(PQgetvalue(res, tupnum, i_datname));
    dbinfos[tupnum].db_encoding = atoi(PQgetvalue(res, tupnum, i_encoding));
    dbinfos[tupnum].db_collate = pg_strdup(PQgetvalue(res, tupnum, i_datcollate));
    dbinfos[tupnum].db_ctype = pg_strdup(PQgetvalue(res, tupnum, i_datctype));
    snprintf(dbinfos[tupnum].db_tablespace, sizeof(dbinfos[tupnum].db_tablespace), "%s", PQgetvalue(res, tupnum, i_spclocation));
  }
  PQclear(res);

  PQfinish(conn);

  cluster->dbarr.dbs = dbinfos;
  cluster->dbarr.ndbs = ntups;
}

   
                   
   
                                                                         
                            
   
                                                                     
                                                                               
   
static void
get_rel_infos(ClusterInfo *cluster, DbInfo *dbinfo)
{
  PGconn *conn = connectToServer(cluster, dbinfo->db_name);
  PGresult *res;
  RelInfo *relinfos;
  int ntups;
  int relnum;
  int num_rels = 0;
  char *nspname = NULL;
  char *relname = NULL;
  char *tablespace = NULL;
  int i_spclocation, i_nspname, i_relname, i_reloid, i_indtable, i_toastheap, i_relfilenode, i_reltablespace;
  char query[QUERY_ALLOC];
  char *last_namespace = NULL, *last_tablespace = NULL;

  query[0] = '\0';                                       

     
                                                                          
                                                                            
                                                                       
                                                                       
     
                                                                       
                                                                            
                                     
     
  snprintf(query + strlen(query), sizeof(query) - strlen(query),
      "WITH regular_heap (reloid, indtable, toastheap) AS ( "
      "  SELECT c.oid, 0::oid, 0::oid "
      "  FROM pg_catalog.pg_class c JOIN pg_catalog.pg_namespace n "
      "         ON c.relnamespace = n.oid "
      "  WHERE relkind IN (" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_MATVIEW) ") AND "
                                                                                                                                          
                                                                                               "    ((n.nspname !~ '^pg_temp_' AND "
                                                                                               "      n.nspname !~ '^pg_toast_temp_' AND "
                                                                                               "      n.nspname NOT IN ('pg_catalog', 'information_schema', "
                                                                                               "                        'binary_upgrade', 'pg_toast') AND "
                                                                                               "      c.oid >= %u::pg_catalog.oid) OR "
                                                                                               "     (n.nspname = 'pg_catalog' AND "
                                                                                               "      relname IN ('pg_largeobject') ))), ",
      FirstNormalObjectId);

     
                                                                          
                                                                       
                                                                          
     
  snprintf(query + strlen(query), sizeof(query) - strlen(query),
      "  toast_heap (reloid, indtable, toastheap) AS ( "
      "  SELECT c.reltoastrelid, 0::oid, c.oid "
      "  FROM regular_heap JOIN pg_catalog.pg_class c "
      "      ON regular_heap.reloid = c.oid "
      "  WHERE c.reltoastrelid != 0), ");

     
                                                                         
                                                                         
                                                                           
               
     
  snprintf(query + strlen(query), sizeof(query) - strlen(query),
      "  all_index (reloid, indtable, toastheap) AS ( "
      "  SELECT indexrelid, indrelid, 0::oid "
      "  FROM pg_catalog.pg_index "
      "  WHERE indisvalid AND indisready "
      "    AND indrelid IN "
      "        (SELECT reloid FROM regular_heap "
      "         UNION ALL "
      "         SELECT reloid FROM toast_heap)) ");

     
                                                                             
                                                                  
     
  snprintf(query + strlen(query), sizeof(query) - strlen(query),
      "SELECT all_rels.*, n.nspname, c.relname, "
      "  c.relfilenode, c.reltablespace, %s "
      "FROM (SELECT * FROM regular_heap "
      "      UNION ALL "
      "      SELECT * FROM toast_heap "
      "      UNION ALL "
      "      SELECT * FROM all_index) all_rels "
      "  JOIN pg_catalog.pg_class c "
      "      ON all_rels.reloid = c.oid "
      "  JOIN pg_catalog.pg_namespace n "
      "     ON c.relnamespace = n.oid "
      "  LEFT OUTER JOIN pg_catalog.pg_tablespace t "
      "     ON c.reltablespace = t.oid "
      "ORDER BY 1;",
                                                            
      (GET_MAJOR_VERSION(cluster->major_version) >= 902) ? "pg_catalog.pg_tablespace_location(t.oid) AS spclocation" : "t.spclocation");

  res = executeQueryOrDie(conn, "%s", query);

  ntups = PQntuples(res);

  relinfos = (RelInfo *)pg_malloc(sizeof(RelInfo) * ntups);

  i_reloid = PQfnumber(res, "reloid");
  i_indtable = PQfnumber(res, "indtable");
  i_toastheap = PQfnumber(res, "toastheap");
  i_nspname = PQfnumber(res, "nspname");
  i_relname = PQfnumber(res, "relname");
  i_relfilenode = PQfnumber(res, "relfilenode");
  i_reltablespace = PQfnumber(res, "reltablespace");
  i_spclocation = PQfnumber(res, "spclocation");

  for (relnum = 0; relnum < ntups; relnum++)
  {
    RelInfo *curr = &relinfos[num_rels++];

    curr->reloid = atooid(PQgetvalue(res, relnum, i_reloid));
    curr->indtable = atooid(PQgetvalue(res, relnum, i_indtable));
    curr->toastheap = atooid(PQgetvalue(res, relnum, i_toastheap));

    nspname = PQgetvalue(res, relnum, i_nspname);
    curr->nsp_alloc = false;

       
                                                                         
                                                                           
                           
       
                                                      
    if (last_namespace && strcmp(nspname, last_namespace) == 0)
    {
      curr->nspname = last_namespace;
    }
    else
    {
      last_namespace = curr->nspname = pg_strdup(nspname);
      curr->nsp_alloc = true;
    }

    relname = PQgetvalue(res, relnum, i_relname);
    curr->relname = pg_strdup(relname);

    curr->relfilenode = atooid(PQgetvalue(res, relnum, i_relfilenode));
    curr->tblsp_alloc = false;

                                            
    if (atooid(PQgetvalue(res, relnum, i_reltablespace)) != 0)
    {
         
                                                                  
                                                         
         
      tablespace = PQgetvalue(res, relnum, i_spclocation);

                                                        
      if (last_tablespace && strcmp(tablespace, last_tablespace) == 0)
      {
        curr->tablespace = last_tablespace;
      }
      else
      {
        last_tablespace = curr->tablespace = pg_strdup(tablespace);
        curr->tblsp_alloc = true;
      }
    }
    else
    {
                                                                       
      curr->tablespace = dbinfo->db_tablespace;
    }
  }
  PQclear(res);

  PQfinish(conn);

  dbinfo->rel_arr.rels = relinfos;
  dbinfo->rel_arr.nrels = num_rels;
}

static void
free_db_and_rel_infos(DbInfoArr *db_arr)
{
  int dbnum;

  for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
  {
    free_rel_infos(&db_arr->dbs[dbnum].rel_arr);
    pg_free(db_arr->dbs[dbnum].db_name);
  }
  pg_free(db_arr->dbs);
  db_arr->dbs = NULL;
  db_arr->ndbs = 0;
}

static void
free_rel_infos(RelInfoArr *rel_arr)
{
  int relnum;

  for (relnum = 0; relnum < rel_arr->nrels; relnum++)
  {
    if (rel_arr->rels[relnum].nsp_alloc)
    {
      pg_free(rel_arr->rels[relnum].nspname);
    }
    pg_free(rel_arr->rels[relnum].relname);
    if (rel_arr->rels[relnum].tblsp_alloc)
    {
      pg_free(rel_arr->rels[relnum].tablespace);
    }
  }
  pg_free(rel_arr->rels);
  rel_arr->nrels = 0;
}

static void
print_db_infos(DbInfoArr *db_arr)
{
  int dbnum;

  for (dbnum = 0; dbnum < db_arr->ndbs; dbnum++)
  {
    pg_log(PG_VERBOSE, "Database: %s\n", db_arr->dbs[dbnum].db_name);
    print_rel_infos(&db_arr->dbs[dbnum].rel_arr);
    pg_log(PG_VERBOSE, "\n\n");
  }
}

static void
print_rel_infos(RelInfoArr *rel_arr)
{
  int relnum;

  for (relnum = 0; relnum < rel_arr->nrels; relnum++)
  {
    pg_log(PG_VERBOSE, "relname: %s.%s: reloid: %u reltblspace: %s\n", rel_arr->rels[relnum].nspname, rel_arr->rels[relnum].relname, rel_arr->rels[relnum].reloid, rel_arr->rels[relnum].tablespace);
  }
}
