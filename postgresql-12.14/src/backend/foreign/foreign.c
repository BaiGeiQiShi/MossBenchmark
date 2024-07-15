                                                                            
   
             
                                                                    
   
                                                                         
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                                                                    
   
ForeignDataWrapper *
GetForeignDataWrapper(Oid fdwid)
{
  return GetForeignDataWrapperExtended(fdwid, 0);
}

   
                                                                    
                                                                          
                                         
   
ForeignDataWrapper *
GetForeignDataWrapperExtended(Oid fdwid, bits16 flags)
{
  Form_pg_foreign_data_wrapper fdwform;
  ForeignDataWrapper *fdw;
  Datum datum;
  HeapTuple tp;
  bool isnull;

  tp = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid));

  if (!HeapTupleIsValid(tp))
  {
    if ((flags & FDW_MISSING_OK) == 0)
    {
      elog(ERROR, "cache lookup failed for foreign-data wrapper %u", fdwid);
    }
    return NULL;
  }

  fdwform = (Form_pg_foreign_data_wrapper)GETSTRUCT(tp);

  fdw = (ForeignDataWrapper *)palloc(sizeof(ForeignDataWrapper));
  fdw->fdwid = fdwid;
  fdw->owner = fdwform->fdwowner;
  fdw->fdwname = pstrdup(NameStr(fdwform->fdwname));
  fdw->fdwhandler = fdwform->fdwhandler;
  fdw->fdwvalidator = fdwform->fdwvalidator;

                              
  datum = SysCacheGetAttr(FOREIGNDATAWRAPPEROID, tp, Anum_pg_foreign_data_wrapper_fdwoptions, &isnull);
  if (isnull)
  {
    fdw->options = NIL;
  }
  else
  {
    fdw->options = untransformRelOptions(datum);
  }

  ReleaseSysCache(tp);

  return fdw;
}

   
                                                                  
                       
   
ForeignDataWrapper *
GetForeignDataWrapperByName(const char *fdwname, bool missing_ok)
{
  Oid fdwId = get_foreign_data_wrapper_oid(fdwname, missing_ok);

  if (!OidIsValid(fdwId))
  {
    return NULL;
  }

  return GetForeignDataWrapper(fdwId);
}

   
                                                             
   
ForeignServer *
GetForeignServer(Oid serverid)
{
  return GetForeignServerExtended(serverid, 0);
}

   
                                                                        
                                                                        
                                
   
ForeignServer *
GetForeignServerExtended(Oid serverid, bits16 flags)
{
  Form_pg_foreign_server serverform;
  ForeignServer *server;
  HeapTuple tp;
  Datum datum;
  bool isnull;

  tp = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid));

  if (!HeapTupleIsValid(tp))
  {
    if ((flags & FSV_MISSING_OK) == 0)
    {
      elog(ERROR, "cache lookup failed for foreign server %u", serverid);
    }
    return NULL;
  }

  serverform = (Form_pg_foreign_server)GETSTRUCT(tp);

  server = (ForeignServer *)palloc(sizeof(ForeignServer));
  server->serverid = serverid;
  server->servername = pstrdup(NameStr(serverform->srvname));
  server->owner = serverform->srvowner;
  server->fdwid = serverform->srvfdw;

                           
  datum = SysCacheGetAttr(FOREIGNSERVEROID, tp, Anum_pg_foreign_server_srvtype, &isnull);
  server->servertype = isnull ? NULL : TextDatumGetCString(datum);

                              
  datum = SysCacheGetAttr(FOREIGNSERVEROID, tp, Anum_pg_foreign_server_srvversion, &isnull);
  server->serverversion = isnull ? NULL : TextDatumGetCString(datum);

                              
  datum = SysCacheGetAttr(FOREIGNSERVEROID, tp, Anum_pg_foreign_server_srvoptions, &isnull);
  if (isnull)
  {
    server->options = NIL;
  }
  else
  {
    server->options = untransformRelOptions(datum);
  }

  ReleaseSysCache(tp);

  return server;
}

   
                                                                           
   
ForeignServer *
GetForeignServerByName(const char *srvname, bool missing_ok)
{
  Oid serverid = get_foreign_server_oid(srvname, missing_ok);

  if (!OidIsValid(serverid))
  {
    return NULL;
  }

  return GetForeignServer(serverid);
}

   
                                              
   
                                                                  
                                           
   
UserMapping *
GetUserMapping(Oid userid, Oid serverid)
{
  Datum datum;
  HeapTuple tp;
  bool isnull;
  UserMapping *um;

  tp = SearchSysCache2(USERMAPPINGUSERSERVER, ObjectIdGetDatum(userid), ObjectIdGetDatum(serverid));

  if (!HeapTupleIsValid(tp))
  {
                                                       
    tp = SearchSysCache2(USERMAPPINGUSERSERVER, ObjectIdGetDatum(InvalidOid), ObjectIdGetDatum(serverid));
  }

  if (!HeapTupleIsValid(tp))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("user mapping not found for \"%s\"", MappingUserName(userid))));
  }

  um = (UserMapping *)palloc(sizeof(UserMapping));
  um->umid = ((Form_pg_user_mapping)GETSTRUCT(tp))->oid;
  um->userid = userid;
  um->serverid = serverid;

                             
  datum = SysCacheGetAttr(USERMAPPINGUSERSERVER, tp, Anum_pg_user_mapping_umoptions, &isnull);
  if (isnull)
  {
    um->options = NIL;
  }
  else
  {
    um->options = untransformRelOptions(datum);
  }

  ReleaseSysCache(tp);

  return um;
}

   
                                                                           
   
ForeignTable *
GetForeignTable(Oid relid)
{
  Form_pg_foreign_table tableform;
  ForeignTable *ft;
  HeapTuple tp;
  Datum datum;
  bool isnull;

  tp = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for foreign table %u", relid);
  }
  tableform = (Form_pg_foreign_table)GETSTRUCT(tp);

  ft = (ForeignTable *)palloc(sizeof(ForeignTable));
  ft->relid = relid;
  ft->serverid = tableform->ftserver;

                             
  datum = SysCacheGetAttr(FOREIGNTABLEREL, tp, Anum_pg_foreign_table_ftoptions, &isnull);
  if (isnull)
  {
    ft->options = NIL;
  }
  else
  {
    ft->options = untransformRelOptions(datum);
  }

  ReleaseSysCache(tp);

  return ft;
}

   
                                                                        
                       
   
List *
GetForeignColumnOptions(Oid relid, AttrNumber attnum)
{
  List *options;
  HeapTuple tp;
  Datum datum;
  bool isnull;

  tp = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
  }
  datum = SysCacheGetAttr(ATTNUM, tp, Anum_pg_attribute_attfdwoptions, &isnull);
  if (isnull)
  {
    options = NIL;
  }
  else
  {
    options = untransformRelOptions(datum);
  }

  ReleaseSysCache(tp);

  return options;
}

   
                                                                           
                                 
   
FdwRoutine *
GetFdwRoutine(Oid fdwhandler)
{
  Datum datum;
  FdwRoutine *routine;

  datum = OidFunctionCall0(fdwhandler);
  routine = (FdwRoutine *)DatumGetPointer(datum);

  if (routine == NULL || !IsA(routine, FdwRoutine))
  {
    elog(ERROR, "foreign-data wrapper handler function %u did not return an FdwRoutine struct", fdwhandler);
  }

  return routine;
}

   
                                                          
                                                    
   
Oid
GetForeignServerIdByRelId(Oid relid)
{
  HeapTuple tp;
  Form_pg_foreign_table tableform;
  Oid serverid;

  tp = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for foreign table %u", relid);
  }
  tableform = (Form_pg_foreign_table)GETSTRUCT(tp);
  serverid = tableform->ftserver;
  ReleaseSysCache(tp);

  return serverid;
}

   
                                                                             
                                                                     
   
FdwRoutine *
GetFdwRoutineByServerId(Oid serverid)
{
  HeapTuple tp;
  Form_pg_foreign_data_wrapper fdwform;
  Form_pg_foreign_server serverform;
  Oid fdwid;
  Oid fdwhandler;

                                                    
  tp = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for foreign server %u", serverid);
  }
  serverform = (Form_pg_foreign_server)GETSTRUCT(tp);
  fdwid = serverform->srvfdw;
  ReleaseSysCache(tp);

                                             
  tp = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for foreign-data wrapper %u", fdwid);
  }
  fdwform = (Form_pg_foreign_data_wrapper)GETSTRUCT(tp);
  fdwhandler = fdwform->fdwhandler;

                                                   
  if (!OidIsValid(fdwhandler))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("foreign-data wrapper \"%s\" has no handler", NameStr(fdwform->fdwname))));
  }

  ReleaseSysCache(tp);

                                               
  return GetFdwRoutine(fdwhandler);
}

   
                                                                          
                                                                    
   
FdwRoutine *
GetFdwRoutineByRelId(Oid relid)
{
  Oid serverid;

                                             
  serverid = GetForeignServerIdByRelId(relid);

                                                
  return GetFdwRoutineByServerId(serverid);
}

   
                                                                              
                                                                    
   
                                                                          
                                                                       
   
                                                                         
                                                                             
                                                                          
   
FdwRoutine *
GetFdwRoutineForRelation(Relation relation, bool makecopy)
{
  FdwRoutine *fdwroutine;
  FdwRoutine *cfdwroutine;

  if (relation->rd_fdwroutine == NULL)
  {
                                                                  
    fdwroutine = GetFdwRoutineByRelId(RelationGetRelid(relation));

                                                             
    cfdwroutine = (FdwRoutine *)MemoryContextAlloc(CacheMemoryContext, sizeof(FdwRoutine));
    memcpy(cfdwroutine, fdwroutine, sizeof(FdwRoutine));
    relation->rd_fdwroutine = cfdwroutine;

                                                                    
    return fdwroutine;
  }

                                                                  
  if (makecopy)
  {
    fdwroutine = (FdwRoutine *)palloc(sizeof(FdwRoutine));
    memcpy(fdwroutine, relation->rd_fdwroutine, sizeof(FdwRoutine));
    return fdwroutine;
  }

                                                                             
  return relation->rd_fdwroutine;
}

   
                                                                           
   
                                                                        
                                      
   
bool
IsImportableForeignTable(const char *tablename, ImportForeignSchemaStmt *stmt)
{
  ListCell *lc;

  switch (stmt->list_type)
  {
  case FDW_IMPORT_SCHEMA_ALL:
    return true;

  case FDW_IMPORT_SCHEMA_LIMIT_TO:
    foreach (lc, stmt->table_list)
    {
      RangeVar *rv = (RangeVar *)lfirst(lc);

      if (strcmp(tablename, rv->relname) == 0)
      {
        return true;
      }
    }
    return false;

  case FDW_IMPORT_SCHEMA_EXCEPT:
    foreach (lc, stmt->table_list)
    {
      RangeVar *rv = (RangeVar *)lfirst(lc);

      if (strcmp(tablename, rv->relname) == 0)
      {
        return false;
      }
    }
    return true;
  }
  return false;                         
}

   
                                                                      
                             
   
static void
deflist_to_tuplestore(ReturnSetInfo *rsinfo, List *options)
{
  ListCell *cell;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  Datum values[2];
  bool nulls[2];
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize) || rsinfo->expectedDesc == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }

  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

     
                                 
     
  tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);
  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  foreach (cell, options)
  {
    DefElem *def = lfirst(cell);

    values[0] = CStringGetTextDatum(def->defname);
    nulls[0] = false;
    if (def->arg)
    {
      values[1] = CStringGetTextDatum(((Value *)(def->arg))->val.str);
      nulls[1] = false;
    }
    else
    {
      values[1] = (Datum)0;
      nulls[1] = true;
    }
    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

                                          
  tuplestore_donestoring(tupstore);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                      
                       
   
Datum
pg_options_to_table(PG_FUNCTION_ARGS)
{
  Datum array = PG_GETARG_DATUM(0);

  deflist_to_tuplestore((ReturnSetInfo *)fcinfo->resultinfo, untransformRelOptions(array));

  return (Datum)0;
}

   
                                                                             
   
struct ConnectionOption
{
  const char *optname;
  Oid optcontext;                                                
};

   
                                               
   
                                                                 
   
static const struct ConnectionOption libpq_conninfo_options[] = {{"authtype", ForeignServerRelationId}, {"service", ForeignServerRelationId}, {"user", UserMappingRelationId}, {"password", UserMappingRelationId}, {"connect_timeout", ForeignServerRelationId}, {"dbname", ForeignServerRelationId}, {"host", ForeignServerRelationId}, {"hostaddr", ForeignServerRelationId}, {"port", ForeignServerRelationId}, {"tty", ForeignServerRelationId}, {"options", ForeignServerRelationId}, {"requiressl", ForeignServerRelationId}, {"sslmode", ForeignServerRelationId}, {"gsslib", ForeignServerRelationId}, {NULL, InvalidOid}};

   
                                                                  
                                                                      
               
   
static bool
is_conninfo_option(const char *option, Oid context)
{
  const struct ConnectionOption *opt;

  for (opt = libpq_conninfo_options; opt->optname; opt++)
  {
    if (context == opt->optcontext && strcmp(opt->optname, option) == 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                                
                                                                    
   
                                                              
                                                                       
   
                                                                           
                                                                            
                                                                           
                                     
   
Datum
postgresql_fdw_validator(PG_FUNCTION_ARGS)
{
  List *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
  Oid catalog = PG_GETARG_OID(1);

  ListCell *cell;

  foreach (cell, options_list)
  {
    DefElem *def = lfirst(cell);

    if (!is_conninfo_option(def->defname, catalog))
    {
      const struct ConnectionOption *opt;
      StringInfoData buf;

         
                                                                     
                                                    
         
      initStringInfo(&buf);
      for (opt = libpq_conninfo_options; opt->optname; opt++)
      {
        if (catalog == opt->optcontext)
        {
          appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "", opt->optname);
        }
      }

      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid option \"%s\"", def->defname), errhint("Valid options in this context are: %s", buf.data)));

      PG_RETURN_BOOL(false);
    }
  }

  PG_RETURN_BOOL(true);
}

   
                                                                    
   
                                                                            
                      
   
Oid
get_foreign_data_wrapper_oid(const char *fdwname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(FOREIGNDATAWRAPPERNAME, Anum_pg_foreign_data_wrapper_oid, CStringGetDatum(fdwname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign-data wrapper \"%s\" does not exist", fdwname)));
  }
  return oid;
}

   
                                                                 
   
                                                                            
                      
   
Oid
get_foreign_server_oid(const char *servername, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(FOREIGNSERVERNAME, Anum_pg_foreign_server_oid, CStringGetDatum(servername));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("server \"%s\" does not exist", servername)));
  }
  return oid;
}

   
                                                                   
   
                                                                              
           
   
                                                                               
                                                                             
                                                                                
                                                                             
          
   
                                                                         
                                                                             
                                                                         
                                                                        
                                  
   
                                                                          
                                                                              
                                                                           
                                                              
   
Path *
GetExistingLocalJoinPath(RelOptInfo *joinrel)
{
  ListCell *lc;

  Assert(IS_JOIN_REL(joinrel));

  foreach (lc, joinrel->pathlist)
  {
    Path *path = (Path *)lfirst(lc);
    JoinPath *joinpath = NULL;

                                   
    if (path->param_info != NULL)
    {
      continue;
    }

    switch (path->pathtype)
    {
    case T_HashJoin:
    {
      HashPath *hash_path = makeNode(HashPath);

      memcpy(hash_path, path, sizeof(HashPath));
      joinpath = (JoinPath *)hash_path;
    }
    break;

    case T_NestLoop:
    {
      NestPath *nest_path = makeNode(NestPath);

      memcpy(nest_path, path, sizeof(NestPath));
      joinpath = (JoinPath *)nest_path;
    }
    break;

    case T_MergeJoin:
    {
      MergePath *merge_path = makeNode(MergePath);

      memcpy(merge_path, path, sizeof(MergePath));
      joinpath = (JoinPath *)merge_path;
    }
    break;

    default:

         
                                                                 
                                                                   
                                                       
         
      break;
    }

                                                  
    if (!joinpath)
    {
      continue;
    }

       
                                                                         
                                                                       
                                                                 
                   
       
    if (IsA(joinpath->outerjoinpath, ForeignPath))
    {
      ForeignPath *foreign_path;

      foreign_path = (ForeignPath *)joinpath->outerjoinpath;
      if (IS_JOIN_REL(foreign_path->path.parent))
      {
        joinpath->outerjoinpath = foreign_path->fdw_outerpath;
      }
    }

    if (IsA(joinpath->innerjoinpath, ForeignPath))
    {
      ForeignPath *foreign_path;

      foreign_path = (ForeignPath *)joinpath->innerjoinpath;
      if (IS_JOIN_REL(foreign_path->path.parent))
      {
        joinpath->innerjoinpath = foreign_path->fdw_outerpath;
      }
    }

    return (Path *)joinpath;
  }
  return NULL;
}
