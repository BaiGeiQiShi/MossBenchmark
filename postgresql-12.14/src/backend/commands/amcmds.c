                                                                            
   
            
                                                               
   
                                                                         
                                                                        
   
   
                  
                                   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
lookup_am_handler_func(List *handler_name, char amtype);
static const char *
get_am_type_string(char amtype);

   
                      
                                   
   
ObjectAddress
CreateAccessMethod(CreateAmStmt *stmt)
{
  Relation rel;
  ObjectAddress myself;
  ObjectAddress referenced;
  Oid amoid;
  Oid amhandler;
  bool nulls[Natts_pg_am];
  Datum values[Natts_pg_am];
  HeapTuple tup;

  rel = table_open(AccessMethodRelationId, RowExclusiveLock);

                          
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create access method \"%s\"", stmt->amname), errhint("Must be superuser to create an access method.")));
  }

                             
  amoid = GetSysCacheOid1(AMNAME, Anum_pg_am_oid, CStringGetDatum(stmt->amname));
  if (OidIsValid(amoid))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("access method \"%s\" already exists", stmt->amname)));
  }

     
                                                                      
     
  amhandler = lookup_am_handler_func(stmt->handler_name, stmt->amtype);

     
                              
     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  amoid = GetNewOidWithIndex(rel, AmOidIndexId, Anum_pg_am_oid);
  values[Anum_pg_am_oid - 1] = ObjectIdGetDatum(amoid);
  values[Anum_pg_am_amname - 1] = DirectFunctionCall1(namein, CStringGetDatum(stmt->amname));
  values[Anum_pg_am_amhandler - 1] = ObjectIdGetDatum(amhandler);
  values[Anum_pg_am_amtype - 1] = CharGetDatum(stmt->amtype);

  tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);

  CatalogTupleInsert(rel, tup);
  heap_freetuple(tup);

  myself.classId = AccessMethodRelationId;
  myself.objectId = amoid;
  myself.objectSubId = 0;

                                             
  referenced.classId = ProcedureRelationId;
  referenced.objectId = amhandler;
  referenced.objectSubId = 0;

  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  recordDependencyOnCurrentExtension(&myself, false);

  table_close(rel, RowExclusiveLock);

  return myself;
}

   
                                   
   
void
RemoveAccessMethodById(Oid amOid)
{
  Relation relation;
  HeapTuple tup;

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to drop access methods")));
  }

  relation = table_open(AccessMethodRelationId, RowExclusiveLock);

  tup = SearchSysCache1(AMOID, ObjectIdGetDatum(amOid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for access method %u", amOid);
  }

  CatalogTupleDelete(relation, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(relation, RowExclusiveLock);
}

   
                   
                                             
   
                                                                          
                                 
   
                                                                           
               
   
static Oid
get_am_type_oid(const char *amname, char amtype, bool missing_ok)
{
  HeapTuple tup;
  Oid oid = InvalidOid;

  tup = SearchSysCache1(AMNAME, CStringGetDatum(amname));
  if (HeapTupleIsValid(tup))
  {
    Form_pg_am amform = (Form_pg_am)GETSTRUCT(tup);

    if (amtype != '\0' && amform->amtype != amtype)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("access method \"%s\" is not of type %s", NameStr(amform->amname), get_am_type_string(amtype))));
    }

    oid = amform->oid;
    ReleaseSysCache(tup);
  }

  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("access method \"%s\" does not exist", amname)));
  }
  return oid;
}

   
                                                                   
                                              
   
Oid
get_index_am_oid(const char *amname, bool missing_ok)
{
  return get_am_type_oid(amname, AMTYPE_INDEX, missing_ok);
}

   
                                                                   
                                              
   
Oid
get_table_am_oid(const char *amname, bool missing_ok)
{
  return get_am_type_oid(amname, AMTYPE_TABLE, missing_ok);
}

   
                                                              
                             
   
Oid
get_am_oid(const char *amname, bool missing_ok)
{
  return get_am_type_oid(amname, '\0', missing_ok);
}

   
                                                                             
   
char *
get_am_name(Oid amOid)
{
  HeapTuple tup;
  char *result = NULL;

  tup = SearchSysCache1(AMOID, ObjectIdGetDatum(amOid));
  if (HeapTupleIsValid(tup))
  {
    Form_pg_am amform = (Form_pg_am)GETSTRUCT(tup);

    result = pstrdup(NameStr(amform->amname));
    ReleaseSysCache(tup);
  }
  return result;
}

   
                                                                                
   
static const char *
get_am_type_string(char amtype)
{
  switch (amtype)
  {
  case AMTYPE_INDEX:
    return "INDEX";
  case AMTYPE_TABLE:
    return "TABLE";
  default:
                          
    elog(ERROR, "invalid access method type '%c'", amtype);
    return NULL;                          
  }
}

   
                                                                         
                                                                 
   
                                                                     
   
static Oid
lookup_am_handler_func(List *handler_name, char amtype)
{
  Oid handlerOid;
  Oid funcargtypes[1] = {INTERNALOID};
  Oid expectedType = InvalidOid;

  if (handler_name == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("handler function is not specified")));
  }

                                                   
  handlerOid = LookupFuncName(handler_name, 1, funcargtypes, false);

                                                      
  switch (amtype)
  {
  case AMTYPE_INDEX:
    expectedType = INDEX_AM_HANDLEROID;
    break;
  case AMTYPE_TABLE:
    expectedType = TABLE_AM_HANDLEROID;
    break;
  default:
    elog(ERROR, "unrecognized access method type \"%c\"", amtype);
  }

  if (get_func_rettype(handlerOid) != expectedType)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("function %s must return type %s", get_func_name(handlerOid), format_type_extended(expectedType, -1, 0))));
  }

  return handlerOid;
}
