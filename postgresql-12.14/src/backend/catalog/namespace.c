                                                                            
   
               
                                                        
   
                                                                          
                                                                     
                                                                        
                                                     
   
   
                                                                         
                                                                        
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/dependency.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_func.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/sinvaladt.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

   
                                                                         
                                                                    
                    
   
                                                                         
                                                                        
                                                                             
                                                                            
   
                                                                      
                                                                         
                                                                            
                                                                         
                                                                          
                                                                          
                                                                     
   
                                                                             
                                                                         
                                                                  
   
                                                                            
                                                                              
   
                                                                            
                                                                            
                                                                          
                                                                           
                                                                             
                                                                           
                                                                             
                                                                              
                                                                             
                                                                             
                                                                              
                                                                               
               
   
                                                                            
                                                                   
                                                                     
                                                                         
                                                                            
                                                                       
                    
   
                                                                        
                                                                        
                                                                        
                                                                        
                                                                       
                               
   
                                                                   
                                                                        
                                                                             
                                                                         
                                                                        
                                                    
   
                                                                           
                                                                    
                                                                            
                                                               
   
                                                                             
   

                                                       

static List *activeSearchPath = NIL;

                                                              
static Oid activeCreationNamespace = InvalidOid;

                                                                            
static bool activeTempCreationPending = false;

                                                                             

static List *baseSearchPath = NIL;

static Oid baseCreationNamespace = InvalidOid;

static bool baseTempCreationPending = false;

static Oid namespaceUser = InvalidOid;

                                                                 
static bool baseSearchPathValid = true;

                                                                               

typedef struct
{
  List *searchPath;                                   
  Oid creationNamespace;                                     
  int nestLevel;                                           
} OverrideStackEntry;

static List *overrideStack = NIL;

   
                                                                             
                                                                          
                                                                               
   
                                                                              
                                                                              
   
                                                                              
                                                                            
                                                                    
                                                                             
                                                                       
                                                                          
   
static Oid myTempNamespace = InvalidOid;

static Oid myTempToastNamespace = InvalidOid;

static SubTransactionId myTempNamespaceSubID = InvalidSubTransactionId;

   
                                                                           
                                      
   
char *namespace_search_path = NULL;

                     
static void
recomputeNamespacePath(void);
static void
AccessTempTableNamespace(bool force);
static void
InitTempTableNamespace(void);
static void
RemoveTempRelations(Oid tempNamespaceId);
static void
RemoveTempRelationsCallback(int code, Datum arg);
static void
NamespaceCallback(Datum arg, int cacheid, uint32 hashvalue);
static bool
MatchNamedCall(HeapTuple proctup, int nargs, List *argnames, int **argnumbers);

   
                            
                                                      
                                                              
   
                                                                               
                                             
   
                                                                           
         
   
                                                                             
               
   
                                                             
   
                                                                         
                                                                              
                        
   
                                                                           
                                        
   
Oid
RangeVarGetRelidExtended(const RangeVar *relation, LOCKMODE lockmode, uint32 flags, RangeVarGetRelidCallback callback, void *callback_arg)
{
  uint64 inval_count;
  Oid relId;
  Oid oldRelId = InvalidOid;
  bool retry = false;
  bool missing_ok = (flags & RVR_MISSING_OK) != 0;

                                        
  Assert(!((flags & RVR_NOWAIT) && (flags & RVR_SKIP_LOCKED)));

     
                                                   
     
  if (relation->catalogname)
  {
    if (strcmp(relation->catalogname, get_database_name(MyDatabaseId)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cross-database references are not implemented: \"%s.%s.%s\"", relation->catalogname, relation->schemaname, relation->relname)));
    }
  }

     
                                                                             
                                                                      
                                                                             
                                                                             
                                
     
                                                                           
                                                                         
                                                                            
                                                                         
                                                                           
                                                                            
                                                                          
                                                                        
     
  for (;;)
  {
       
                                                                        
                                                                           
                                                         
       
    inval_count = SharedInvalidMessageCounter;

       
                                                                           
                                                                        
                                                                         
                                                                  
                                                                          
                                                
       
    if (relation->relpersistence == RELPERSISTENCE_TEMP)
    {
      if (!OidIsValid(myTempNamespace))
      {
        relId = InvalidOid;                                  
      }
      else
      {
        if (relation->schemaname)
        {
          Oid namespaceId;

          namespaceId = LookupExplicitNamespace(relation->schemaname, missing_ok);

             
                                                                 
                                
             
          if (namespaceId != myTempNamespace)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("temporary tables cannot specify a schema name")));
          }
        }

        relId = get_relname_relid(relation->relname, myTempNamespace);
      }
    }
    else if (relation->schemaname)
    {
      Oid namespaceId;

                                  
      namespaceId = LookupExplicitNamespace(relation->schemaname, missing_ok);
      if (missing_ok && !OidIsValid(namespaceId))
      {
        relId = InvalidOid;
      }
      else
      {
        relId = get_relname_relid(relation->relname, namespaceId);
      }
    }
    else
    {
                                     
      relId = RelnameGetRelid(relation->relname);
    }

       
                                                
       
                                                                      
                                                                           
                                                                           
                                                                     
                                                                        
                                                           
       
    if (callback)
    {
      callback(relation, relId, oldRelId, callback_arg);
    }

       
                                                                     
                                                                       
                                                                          
                                                                           
                                                    
       
    if (lockmode == NoLock)
    {
      break;
    }

       
                                                                           
                                                                           
                      
       
                                                                         
                                                                         
             
       
    if (retry)
    {
      if (relId == oldRelId)
      {
        break;
      }
      if (OidIsValid(oldRelId))
      {
        UnlockRelationOid(oldRelId, lockmode);
      }
    }

       
                                                                      
                                                                        
                                                                    
                                                                  
                  
       
    if (!OidIsValid(relId))
    {
      AcceptInvalidationMessages();
    }
    else if (!(flags & (RVR_NOWAIT | RVR_SKIP_LOCKED)))
    {
      LockRelationOid(relId, lockmode);
    }
    else if (!ConditionalLockRelationOid(relId, lockmode))
    {
      int elevel = (flags & RVR_SKIP_LOCKED) ? DEBUG1 : ERROR;

      if (relation->schemaname)
      {
        ereport(elevel, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on relation \"%s.%s\"", relation->schemaname, relation->relname)));
      }
      else
      {
        ereport(elevel, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on relation \"%s\"", relation->relname)));
      }

      return InvalidOid;
    }

       
                                                              
       
    if (inval_count == SharedInvalidMessageCounter)
    {
      break;
    }

       
                                                                          
                                                                
                   
       
    retry = true;
    oldRelId = relId;
  }

  if (!OidIsValid(relId))
  {
    int elevel = missing_ok ? DEBUG1 : ERROR;

    if (relation->schemaname)
    {
      ereport(elevel, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s.%s\" does not exist", relation->schemaname, relation->relname)));
    }
    else
    {
      ereport(elevel, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s\" does not exist", relation->relname)));
    }
  }
  return relId;
}

   
                                
                                                          
                                            
   
                                                                         
                                                                            
                                                                            
   
Oid
RangeVarGetCreationNamespace(const RangeVar *newRelation)
{
  Oid namespaceId;

     
                                                   
     
  if (newRelation->catalogname)
  {
    if (strcmp(newRelation->catalogname, get_database_name(MyDatabaseId)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cross-database references are not implemented: \"%s.%s.%s\"", newRelation->catalogname, newRelation->schemaname, newRelation->relname)));
    }
  }

  if (newRelation->schemaname)
  {
                                 
    if (strcmp(newRelation->schemaname, "pg_temp") == 0)
    {
                                     
      AccessTempTableNamespace(false);
      return myTempNamespace;
    }
                                
    namespaceId = get_namespace_oid(newRelation->schemaname, false);
                                                
  }
  else if (newRelation->relpersistence == RELPERSISTENCE_TEMP)
  {
                                   
    AccessTempTableNamespace(false);
    return myTempNamespace;
  }
  else
  {
                                            
    recomputeNamespacePath();
    if (activeTempCreationPending)
    {
                                             
      AccessTempTableNamespace(true);
      return myTempNamespace;
    }
    namespaceId = activeCreationNamespace;
    if (!OidIsValid(namespaceId))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("no schema has been selected to create in")));
    }
  }

                                                                   

  return namespaceId;
}

   
                                        
   
                                                                          
                                                                          
                                                                         
             
   
                                                                              
                                                                             
                            
   
                                                                              
                                                                              
                                                                       
                                                                         
                                      
   
                                                                          
                                                                       
                                                                            
                                      
   
                                                                                 
                                                
   
Oid
RangeVarGetAndCheckCreationNamespace(RangeVar *relation, LOCKMODE lockmode, Oid *existing_relation_id)
{
  uint64 inval_count;
  Oid relid;
  Oid oldrelid = InvalidOid;
  Oid nspid;
  Oid oldnspid = InvalidOid;
  bool retry = false;

     
                                                   
     
  if (relation->catalogname)
  {
    if (strcmp(relation->catalogname, get_database_name(MyDatabaseId)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cross-database references are not implemented: \"%s.%s.%s\"", relation->catalogname, relation->schemaname, relation->relname)));
    }
  }

     
                                                                       
                                                                            
                                                                           
                                                                     
     
  for (;;)
  {
    AclResult aclresult;

    inval_count = SharedInvalidMessageCounter;

                                                                     
    nspid = RangeVarGetCreationNamespace(relation);
    Assert(OidIsValid(nspid));
    if (existing_relation_id != NULL)
    {
      relid = get_relname_relid(relation->relname, nspid);
    }
    else
    {
      relid = InvalidOid;
    }

       
                                                                         
                                                                      
                    
       
    if (IsBootstrapProcessingMode())
    {
      break;
    }

                                      
    aclresult = pg_namespace_aclcheck(nspid, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(nspid));
    }

    if (retry)
    {
                                           
      if (relid == oldrelid && nspid == oldnspid)
      {
        break;
      }
                                                                
      if (nspid != oldnspid)
      {
        UnlockDatabaseObject(NamespaceRelationId, oldnspid, 0, AccessShareLock);
      }
                                                                    
      if (relid != oldrelid && OidIsValid(oldrelid) && lockmode != NoLock)
      {
        UnlockRelationOid(oldrelid, lockmode);
      }
    }

                         
    if (nspid != oldnspid)
    {
      LockDatabaseObject(NamespaceRelationId, nspid, 0, AccessShareLock);
    }

                                                               
    if (lockmode != NoLock && OidIsValid(relid))
    {
      if (!pg_class_ownercheck(relid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relid)), relation->relname);
      }
      if (relid != oldrelid)
      {
        LockRelationOid(relid, lockmode);
      }
    }

                                                                
    if (inval_count == SharedInvalidMessageCounter)
    {
      break;
    }

                                                          
    retry = true;
    oldrelid = relid;
    oldnspid = nspid;
  }

  RangeVarAdjustRelationPersistence(relation, nspid);
  if (existing_relation_id != NULL)
  {
    *existing_relation_id = relid;
  }
  return nspid;
}

   
                                                                              
                                                                    
   
void
RangeVarAdjustRelationPersistence(RangeVar *newRelation, Oid nspid)
{
  switch (newRelation->relpersistence)
  {
  case RELPERSISTENCE_TEMP:
    if (!isTempOrTempToastNamespace(nspid))
    {
      if (isAnyTempNamespace(nspid))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot create relations in temporary schemas of other sessions")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot create temporary relation in non-temporary schema")));
      }
    }
    break;
  case RELPERSISTENCE_PERMANENT:
    if (isTempOrTempToastNamespace(nspid))
    {
      newRelation->relpersistence = RELPERSISTENCE_TEMP;
    }
    else if (isAnyTempNamespace(nspid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot create relations in temporary schemas of other sessions")));
    }
    break;
  default:
    if (isAnyTempNamespace(nspid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("only temporary relations may be created in temporary schemas")));
    }
  }
}

   
                   
                                                 
                                                                   
   
Oid
RelnameGetRelid(const char *relname)
{
  Oid relid;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    relid = get_relname_relid(relname, namespaceId);
    if (OidIsValid(relid))
    {
      return relid;
    }
  }

                         
  return InvalidOid;
}

   
                     
                                                                       
                                                                     
                                        
   
bool
RelationIsVisible(Oid relid)
{
  HeapTuple reltup;
  Form_pg_class relform;
  Oid relnamespace;
  bool visible;

  reltup = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(reltup))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  relform = (Form_pg_class)GETSTRUCT(reltup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  relnamespace = relform->relnamespace;
  if (relnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, relnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                           
                                                          
       
    char *relname = NameStr(relform->relname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == relnamespace)
      {
                                    
        visible = true;
        break;
      }
      if (OidIsValid(get_relname_relid(relname, namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(reltup);

  return visible;
}

   
                    
                                      
   
Oid
TypenameGetTypid(const char *typname)
{
  return TypenameGetTypidExtended(typname, true);
}

   
                            
                                                 
                                                               
   
                                                    
   
Oid
TypenameGetTypidExtended(const char *typname, bool temp_ok)
{
  Oid typid;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    if (!temp_ok && namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    typid = GetSysCacheOid2(TYPENAMENSP, Anum_pg_type_oid, PointerGetDatum(typname), ObjectIdGetDatum(namespaceId));
    if (OidIsValid(typid))
    {
      return typid;
    }
  }

                         
  return InvalidOid;
}

   
                 
                                                                   
                                                                     
                                    
   
bool
TypeIsVisible(Oid typid)
{
  HeapTuple typtup;
  Form_pg_type typform;
  Oid typnamespace;
  bool visible;

  typtup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (!HeapTupleIsValid(typtup))
  {
    elog(ERROR, "cache lookup failed for type %u", typid);
  }
  typform = (Form_pg_type)GETSTRUCT(typtup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  typnamespace = typform->typnamespace;
  if (typnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, typnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                          
                                                   
       
    char *typname = NameStr(typform->typname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == typnamespace)
      {
                                    
        visible = true;
        break;
      }
      if (SearchSysCacheExists2(TYPENAMENSP, PointerGetDatum(typname), ObjectIdGetDatum(namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(typtup);

  return visible;
}

   
                         
                                                                 
                                             
   
                                                                    
                                                                             
                                                     
   
                                                                              
                                                                             
                                                                             
                                                                             
                                                                 
   
                                                                              
                                                                            
                                                                        
                                                                         
                                                                              
                                                
   
                                                                             
                                                                              
                                                                               
                                                                  
                          
   
                                                                           
                                                                             
                                                                            
                   
   
                                                                            
                                                                               
                                                                            
                                                                           
                                                                  
                                                                            
   
                                                                        
                                                                       
                                                                             
                     
   
                                                                           
                                                                         
                                                                            
            
   
                                                                             
                                                                             
                                                                              
                                                                             
                                                                              
                                                                             
                                                                           
                                                                      
                                                                              
                                                                    
   
                                                                           
                                                                        
                                         
   
FuncCandidateList
FuncnameGetCandidates(List *names, int nargs, List *argnames, bool expand_variadic, bool expand_defaults, bool missing_ok)
{
  FuncCandidateList resultList = NULL;
  bool any_special = false;
  char *schemaname;
  char *funcname;
  Oid namespaceId;
  CatCList *catlist;
  int i;

                              
  Assert(nargs >= 0 || !(expand_variadic | expand_defaults));

                                 
  DeconstructQualifiedName(names, &schemaname, &funcname);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (!OidIsValid(namespaceId))
    {
      return NULL;
    }
  }
  else
  {
                                                   
    namespaceId = InvalidOid;
    recomputeNamespacePath();
  }

                                    
  catlist = SearchSysCacheList1(PROCNAMEARGSNSP, CStringGetDatum(funcname));

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple proctup = &catlist->members[i]->tuple;
    Form_pg_proc procform = (Form_pg_proc)GETSTRUCT(proctup);
    int pronargs = procform->pronargs;
    int effective_nargs;
    int pathpos = 0;
    bool variadic;
    bool use_defaults;
    Oid va_elem_type;
    int *argnumbers = NULL;
    FuncCandidateList newResult;

    if (OidIsValid(namespaceId))
    {
                                                      
      if (procform->pronamespace != namespaceId)
      {
        continue;
      }
    }
    else
    {
         
                                                                        
                             
         
      ListCell *nsp;

      foreach (nsp, activeSearchPath)
      {
        if (procform->pronamespace == lfirst_oid(nsp) && procform->pronamespace != myTempNamespace)
        {
          break;
        }
        pathpos++;
      }
      if (nsp == NULL)
      {
        continue;                                 
      }
    }

    if (argnames != NIL)
    {
         
                                           
         
                                                                       
                                                                        
                                                                        
         
      if (OidIsValid(procform->provariadic) && expand_variadic)
      {
        continue;
      }
      va_elem_type = InvalidOid;
      variadic = false;

         
                               
         
      Assert(nargs >= 0);                                     

      if (pronargs > nargs && expand_defaults)
      {
                                                      
        if (nargs + procform->pronargdefaults < pronargs)
        {
          continue;
        }
        use_defaults = true;
      }
      else
      {
        use_defaults = false;
      }

                                                               
      if (pronargs != nargs && !use_defaults)
      {
        continue;
      }

                                                                      
      if (!MatchNamedCall(proctup, nargs, argnames, &argnumbers))
      {
        continue;
      }

                                                       
      any_special = true;
    }
    else
    {
         
                                       
         
                                                                         
                                                                 
                        
         
      if (pronargs <= nargs && expand_variadic)
      {
        va_elem_type = procform->provariadic;
        variadic = OidIsValid(va_elem_type);
        any_special |= variadic;
      }
      else
      {
        va_elem_type = InvalidOid;
        variadic = false;
      }

         
                                                                  
         
      if (pronargs > nargs && expand_defaults)
      {
                                                      
        if (nargs + procform->pronargdefaults < pronargs)
        {
          continue;
        }
        use_defaults = true;
        any_special = true;
      }
      else
      {
        use_defaults = false;
      }

                                                               
      if (nargs >= 0 && pronargs != nargs && !variadic && !use_defaults)
      {
        continue;
      }
    }

       
                                                                         
                                                                          
                                                                          
                                              
       
    effective_nargs = Max(pronargs, nargs);
    newResult = (FuncCandidateList)palloc(offsetof(struct _FuncCandidateList, args) + effective_nargs * sizeof(Oid));
    newResult->pathpos = pathpos;
    newResult->oid = procform->oid;
    newResult->nargs = effective_nargs;
    newResult->argnumbers = argnumbers;
    if (argnumbers)
    {
                                                                 
      Oid *proargtypes = procform->proargtypes.values;
      int i;

      for (i = 0; i < pronargs; i++)
      {
        newResult->args[i] = proargtypes[argnumbers[i]];
      }
    }
    else
    {
                                                               
      memcpy(newResult->args, procform->proargtypes.values, pronargs * sizeof(Oid));
    }
    if (variadic)
    {
      int i;

      newResult->nvargs = effective_nargs - pronargs + 1;
                                                                  
      for (i = pronargs - 1; i < effective_nargs; i++)
      {
        newResult->args[i] = va_elem_type;
      }
    }
    else
    {
      newResult->nvargs = 0;
    }
    newResult->ndargs = use_defaults ? pronargs - nargs : 0;

       
                                                                         
                                                                      
                                                                          
                                                                        
                                                                        
                                 
       
    if (resultList != NULL && (any_special || !OidIsValid(namespaceId)))
    {
         
                                                                        
                                                                       
                                                                       
                                                                        
                                                                    
                                                                     
                                 
         
                                                                    
         
      FuncCandidateList prevResult;

      if (catlist->ordered && !any_special)
      {
                                              
        if (effective_nargs == resultList->nargs && memcmp(newResult->args, resultList->args, effective_nargs * sizeof(Oid)) == 0)
        {
          prevResult = resultList;
        }
        else
        {
          prevResult = NULL;
        }
      }
      else
      {
        int cmp_nargs = newResult->nargs - newResult->ndargs;

        for (prevResult = resultList; prevResult; prevResult = prevResult->next)
        {
          if (cmp_nargs == prevResult->nargs - prevResult->ndargs && memcmp(newResult->args, prevResult->args, cmp_nargs * sizeof(Oid)) == 0)
          {
            break;
          }
        }
      }

      if (prevResult)
      {
           
                                                                     
                                                                  
                                                                     
                                                                     
                      
           
        int preference;

        if (pathpos != prevResult->pathpos)
        {
             
                                                               
             
          preference = pathpos - prevResult->pathpos;
        }
        else if (variadic && prevResult->nvargs == 0)
        {
             
                                                                 
                                                                  
                                                                    
                                    
             
          preference = 1;
        }
        else if (!variadic && prevResult->nvargs > 0)
        {
          preference = -1;
        }
        else
        {
                       
                                                                  
                                                       
                                                               
                                                                
                                                               
                                                           
                       
             
          preference = 0;
        }

        if (preference > 0)
        {
                                    
          pfree(newResult);
          continue;
        }
        else if (preference < 0)
        {
                                                    
          if (prevResult == resultList)
          {
            resultList = prevResult->next;
          }
          else
          {
            FuncCandidateList prevPrevResult;

            for (prevPrevResult = resultList; prevPrevResult; prevPrevResult = prevPrevResult->next)
            {
              if (prevResult == prevPrevResult->next)
              {
                prevPrevResult->next = prevResult->next;
                break;
              }
            }
            Assert(prevPrevResult);                         
          }
          pfree(prevResult);
                                                     
        }
        else
        {
                                                         
          prevResult->oid = InvalidOid;
          pfree(newResult);
          continue;
        }
      }
    }

       
                                     
       
    newResult->next = resultList;
    resultList = newResult;
  }

  ReleaseSysCacheList(catlist);

  return resultList;
}

   
                  
                                                                    
                                                     
   
                                                                       
                                                                            
                                              
   
                                                                        
                                                                
   
                                                                            
                                                                        
                                                                        
                                     
   
static bool
MatchNamedCall(HeapTuple proctup, int nargs, List *argnames, int **argnumbers)
{
  Form_pg_proc procform = (Form_pg_proc)GETSTRUCT(proctup);
  int pronargs = procform->pronargs;
  int numposargs = nargs - list_length(argnames);
  int pronallargs;
  Oid *p_argtypes;
  char **p_argnames;
  char *p_argmodes;
  bool arggiven[FUNC_MAX_ARGS];
  bool isnull;
  int ap;                         
  int pp;                       
  ListCell *lc;

  Assert(argnames != NIL);
  Assert(numposargs >= 0);
  Assert(nargs <= pronargs);

                                                       
  (void)SysCacheGetAttr(PROCOID, proctup, Anum_pg_proc_proargnames, &isnull);
  if (isnull)
  {
    return false;
  }

                                                      
  pronallargs = get_func_arg_info(proctup, &p_argtypes, &p_argnames, &p_argmodes);
  Assert(p_argnames != NULL);

                                     
  *argnumbers = (int *)palloc(pronargs * sizeof(int));
  memset(arggiven, false, pronargs * sizeof(bool));

                                                                  
  for (ap = 0; ap < numposargs; ap++)
  {
    (*argnumbers)[ap] = ap;
    arggiven[ap] = true;
  }

                                  
  foreach (lc, argnames)
  {
    char *argname = (char *)lfirst(lc);
    bool found;
    int i;

    pp = 0;
    found = false;
    for (i = 0; i < pronallargs; i++)
    {
                                          
      if (p_argmodes && (p_argmodes[i] != FUNC_PARAM_IN && p_argmodes[i] != FUNC_PARAM_INOUT && p_argmodes[i] != FUNC_PARAM_VARIADIC))
      {
        continue;
      }
      if (p_argnames[i] && strcmp(p_argnames[i], argname) == 0)
      {
                                                           
        if (arggiven[pp])
        {
          return false;
        }
        arggiven[pp] = true;
        (*argnumbers)[ap] = pp;
        found = true;
        break;
      }
                                                 
      pp++;
    }
                                            
    if (!found)
    {
      return false;
    }
    ap++;
  }

  Assert(ap == nargs);                                      

                                   
  if (nargs < pronargs)
  {
    int first_arg_with_default = pronargs - procform->pronargdefaults;

    for (pp = numposargs; pp < pronargs; pp++)
    {
      if (arggiven[pp])
      {
        continue;
      }
                                                          
      if (pp < first_arg_with_default)
      {
        return false;
      }
      (*argnumbers)[ap++] = pp;
    }
  }

  Assert(ap == pronargs);                                        

  return true;
}

   
                     
                                                                       
                                                                     
                                                                    
   
bool
FunctionIsVisible(Oid funcid)
{
  HeapTuple proctup;
  Form_pg_proc procform;
  Oid pronamespace;
  bool visible;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(proctup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  pronamespace = procform->pronamespace;
  if (pronamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, pronamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                        
                                                                        
                                                          
       
    char *proname = NameStr(procform->proname);
    int nargs = procform->pronargs;
    FuncCandidateList clist;

    visible = false;

    clist = FuncnameGetCandidates(list_make1(makeString(proname)), nargs, NIL, false, false, false);

    for (; clist; clist = clist->next)
    {
      if (memcmp(clist->args, procform->proargtypes.values, nargs * sizeof(Oid)) == 0)
      {
                                                             
        visible = (clist->oid == funcid);
        break;
      }
    }
  }

  ReleaseSysCache(proctup);

  return visible;
}

   
                    
                                                                        
                                                            
   
                                                                        
                 
   
                                                                             
                                                                         
                                                  
   
Oid
OpernameGetOprid(List *names, Oid oprleft, Oid oprright)
{
  char *schemaname;
  char *opername;
  CatCList *catlist;
  ListCell *l;

                                 
  DeconstructQualifiedName(names, &schemaname, &opername);

  if (schemaname)
  {
                                           
    Oid namespaceId;

    namespaceId = LookupExplicitNamespace(schemaname, true);
    if (OidIsValid(namespaceId))
    {
      HeapTuple opertup;

      opertup = SearchSysCache4(OPERNAMENSP, CStringGetDatum(opername), ObjectIdGetDatum(oprleft), ObjectIdGetDatum(oprright), ObjectIdGetDatum(namespaceId));
      if (HeapTupleIsValid(opertup))
      {
        Form_pg_operator operclass = (Form_pg_operator)GETSTRUCT(opertup);
        Oid result = operclass->oid;

        ReleaseSysCache(opertup);
        return result;
      }
    }

    return InvalidOid;
  }

                                                  
  catlist = SearchSysCacheList3(OPERNAMENSP, CStringGetDatum(opername), ObjectIdGetDatum(oprleft), ObjectIdGetDatum(oprright));

  if (catlist->n_members == 0)
  {
                                 
    ReleaseSysCacheList(catlist);
    return InvalidOid;
  }

     
                                                                          
                                                                        
                                                           
     
  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);
    int i;

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    for (i = 0; i < catlist->n_members; i++)
    {
      HeapTuple opertup = &catlist->members[i]->tuple;
      Form_pg_operator operform = (Form_pg_operator)GETSTRUCT(opertup);

      if (operform->oprnamespace == namespaceId)
      {
        Oid result = operform->oid;

        ReleaseSysCacheList(catlist);
        return result;
      }
    }
  }

  ReleaseSysCacheList(catlist);
  return InvalidOid;
}

   
                         
                                                                
                                             
   
                                                                        
                            
   
                                                                        
                                                                          
                                                                       
                                                                        
                                          
   
                                                                          
                                                                         
   
FuncCandidateList
OpernameGetCandidates(List *names, char oprkind, bool missing_schema_ok)
{
  FuncCandidateList resultList = NULL;
  char *resultSpace = NULL;
  int nextResult = 0;
  char *schemaname;
  char *opername;
  Oid namespaceId;
  CatCList *catlist;
  int i;

                                 
  DeconstructQualifiedName(names, &schemaname, &opername);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_schema_ok);
    if (missing_schema_ok && !OidIsValid(namespaceId))
    {
      return NULL;
    }
  }
  else
  {
                                                   
    namespaceId = InvalidOid;
    recomputeNamespacePath();
  }

                                    
  catlist = SearchSysCacheList1(OPERNAMENSP, CStringGetDatum(opername));

     
                                                                         
                                                                            
                                                                            
                                                                            
                                                                           
                                                                        
                                                                  
     
#define SPACE_PER_OP MAXALIGN(offsetof(struct _FuncCandidateList, args) + 2 * sizeof(Oid))

  if (catlist->n_members > 0)
  {
    resultSpace = palloc(catlist->n_members * SPACE_PER_OP);
  }

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple opertup = &catlist->members[i]->tuple;
    Form_pg_operator operform = (Form_pg_operator)GETSTRUCT(opertup);
    int pathpos = 0;
    FuncCandidateList newResult;

                                                                    
    if (oprkind && operform->oprkind != oprkind)
    {
      continue;
    }

    if (OidIsValid(namespaceId))
    {
                                                      
      if (operform->oprnamespace != namespaceId)
      {
        continue;
      }
                                                             
    }
    else
    {
         
                                                                        
                             
         
      ListCell *nsp;

      foreach (nsp, activeSearchPath)
      {
        if (operform->oprnamespace == lfirst_oid(nsp) && operform->oprnamespace != myTempNamespace)
        {
          break;
        }
        pathpos++;
      }
      if (nsp == NULL)
      {
        continue;                                 
      }

         
                                                                  
                                                                      
                                                          
         
                                                                        
                                                                       
                                                                       
                                                                        
                      
         
      if (resultList)
      {
        FuncCandidateList prevResult;

        if (catlist->ordered)
        {
          if (operform->oprleft == resultList->args[0] && operform->oprright == resultList->args[1])
          {
            prevResult = resultList;
          }
          else
          {
            prevResult = NULL;
          }
        }
        else
        {
          for (prevResult = resultList; prevResult; prevResult = prevResult->next)
          {
            if (operform->oprleft == prevResult->args[0] && operform->oprright == prevResult->args[1])
            {
              break;
            }
          }
        }
        if (prevResult)
        {
                                                      
          Assert(pathpos != prevResult->pathpos);
          if (pathpos > prevResult->pathpos)
          {
            continue;                           
          }
                                       
          prevResult->pathpos = pathpos;
          prevResult->oid = operform->oid;
          continue;                               
        }
      }
    }

       
                                     
       
    newResult = (FuncCandidateList)(resultSpace + nextResult);
    nextResult += SPACE_PER_OP;

    newResult->pathpos = pathpos;
    newResult->oid = operform->oid;
    newResult->nargs = 2;
    newResult->nvargs = 0;
    newResult->ndargs = 0;
    newResult->argnumbers = NULL;
    newResult->args[0] = operform->oprleft;
    newResult->args[1] = operform->oprright;
    newResult->next = resultList;
    resultList = newResult;
  }

  ReleaseSysCacheList(catlist);

  return resultList;
}

   
                     
                                                                        
                                                                     
                                                                    
   
bool
OperatorIsVisible(Oid oprid)
{
  HeapTuple oprtup;
  Form_pg_operator oprform;
  Oid oprnamespace;
  bool visible;

  oprtup = SearchSysCache1(OPEROID, ObjectIdGetDatum(oprid));
  if (!HeapTupleIsValid(oprtup))
  {
    elog(ERROR, "cache lookup failed for operator %u", oprid);
  }
  oprform = (Form_pg_operator)GETSTRUCT(oprtup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  oprnamespace = oprform->oprnamespace;
  if (oprnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, oprnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                         
                                                                           
                                                         
       
    char *oprname = NameStr(oprform->oprname);

    visible = (OpernameGetOprid(list_make1(makeString(oprname)), oprform->oprleft, oprform->oprright) == oprid);
  }

  ReleaseSysCache(oprtup);

  return visible;
}

   
                       
                                                      
                                                                  
   
                                                                         
                                           
   
Oid
OpclassnameGetOpcid(Oid amid, const char *opcname)
{
  Oid opcid;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    opcid = GetSysCacheOid3(CLAAMNAMENSP, Anum_pg_opclass_oid, ObjectIdGetDatum(amid), PointerGetDatum(opcname), ObjectIdGetDatum(namespaceId));
    if (OidIsValid(opcid))
    {
      return opcid;
    }
  }

                         
  return InvalidOid;
}

   
                    
                                                                       
                                                                     
                                       
   
bool
OpclassIsVisible(Oid opcid)
{
  HeapTuple opctup;
  Form_pg_opclass opcform;
  Oid opcnamespace;
  bool visible;

  opctup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opcid));
  if (!HeapTupleIsValid(opctup))
  {
    elog(ERROR, "cache lookup failed for opclass %u", opcid);
  }
  opcform = (Form_pg_opclass)GETSTRUCT(opctup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  opcnamespace = opcform->opcnamespace;
  if (opcnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, opcnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                          
                                                                        
                            
       
    char *opcname = NameStr(opcform->opcname);

    visible = (OpclassnameGetOpcid(opcform->opcmethod, opcname) == opcid);
  }

  ReleaseSysCache(opctup);

  return visible;
}

   
                        
                                                       
                                                                   
   
                                                                         
                                           
   
Oid
OpfamilynameGetOpfid(Oid amid, const char *opfname)
{
  Oid opfid;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    opfid = GetSysCacheOid3(OPFAMILYAMNAMENSP, Anum_pg_opfamily_oid, ObjectIdGetDatum(amid), PointerGetDatum(opfname), ObjectIdGetDatum(namespaceId));
    if (OidIsValid(opfid))
    {
      return opfid;
    }
  }

                         
  return InvalidOid;
}

   
                     
                                                                        
                                                                     
                                        
   
bool
OpfamilyIsVisible(Oid opfid)
{
  HeapTuple opftup;
  Form_pg_opfamily opfform;
  Oid opfnamespace;
  bool visible;

  opftup = SearchSysCache1(OPFAMILYOID, ObjectIdGetDatum(opfid));
  if (!HeapTupleIsValid(opftup))
  {
    elog(ERROR, "cache lookup failed for opfamily %u", opfid);
  }
  opfform = (Form_pg_opfamily)GETSTRUCT(opftup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  opfnamespace = opfform->opfnamespace;
  if (opfnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, opfnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                           
                                                                         
                             
       
    char *opfname = NameStr(opfform->opfname);

    visible = (OpfamilynameGetOpfid(opfform->opfmethod, opfname) == opfid);
  }

  ReleaseSysCache(opftup);

  return visible;
}

   
                    
                                                                     
                                                                      
   
static Oid
lookup_collation(const char *collname, Oid collnamespace, int32 encoding)
{
  Oid collid;
  HeapTuple colltup;
  Form_pg_collation collform;

                                                       
  collid = GetSysCacheOid3(COLLNAMEENCNSP, Anum_pg_collation_oid, PointerGetDatum(collname), Int32GetDatum(encoding), ObjectIdGetDatum(collnamespace));
  if (OidIsValid(collid))
  {
    return collid;
  }

     
                                                                           
                                                                       
                                                                           
                                          
     
  colltup = SearchSysCache3(COLLNAMEENCNSP, PointerGetDatum(collname), Int32GetDatum(-1), ObjectIdGetDatum(collnamespace));
  if (!HeapTupleIsValid(colltup))
  {
    return InvalidOid;
  }
  collform = (Form_pg_collation)GETSTRUCT(colltup);
  if (collform->collprovider == COLLPROVIDER_ICU)
  {
    if (is_encoding_supported_by_icu(encoding))
    {
      collid = collform->oid;
    }
    else
    {
      collid = InvalidOid;
    }
  }
  else
  {
    collid = collform->oid;
  }
  ReleaseSysCache(colltup);
  return collid;
}

   
                      
                                                  
                                                                    
   
                                                                       
                        
   
Oid
CollationGetCollid(const char *collname)
{
  int32 dbencoding = GetDatabaseEncoding();
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);
    Oid collid;

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    collid = lookup_collation(collname, namespaceId, dbencoding);
    if (OidIsValid(collid))
    {
      return collid;
    }
  }

                         
  return InvalidOid;
}

   
                      
                                                                        
                                                                     
                                         
   
                                                                            
                               
   
bool
CollationIsVisible(Oid collid)
{
  HeapTuple colltup;
  Form_pg_collation collform;
  Oid collnamespace;
  bool visible;

  colltup = SearchSysCache1(COLLOID, ObjectIdGetDatum(collid));
  if (!HeapTupleIsValid(colltup))
  {
    elog(ERROR, "cache lookup failed for collation %u", collid);
  }
  collform = (Form_pg_collation)GETSTRUCT(colltup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  collnamespace = collform->collnamespace;
  if (collnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, collnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                         
                                                                           
                                                             
                           
       
    char *collname = NameStr(collform->collname);

    visible = (CollationGetCollid(collname) == collid);
  }

  ReleaseSysCache(colltup);

  return visible;
}

   
                      
                                                   
                                                                     
   
                                                    
   
Oid
ConversionGetConid(const char *conname)
{
  Oid conid;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    conid = GetSysCacheOid2(CONNAMENSP, Anum_pg_conversion_oid, PointerGetDatum(conname), ObjectIdGetDatum(namespaceId));
    if (OidIsValid(conid))
    {
      return conid;
    }
  }

                         
  return InvalidOid;
}

   
                       
                                                                         
                                                                     
                                          
   
bool
ConversionIsVisible(Oid conid)
{
  HeapTuple contup;
  Form_pg_conversion conform;
  Oid connamespace;
  bool visible;

  contup = SearchSysCache1(CONVOID, ObjectIdGetDatum(conid));
  if (!HeapTupleIsValid(contup))
  {
    elog(ERROR, "cache lookup failed for conversion %u", conid);
  }
  conform = (Form_pg_conversion)GETSTRUCT(contup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  connamespace = conform->connamespace;
  if (connamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, connamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                          
                                                                           
                              
       
    char *conname = NameStr(conform->conname);

    visible = (ConversionGetConid(conname) == conid);
  }

  ReleaseSysCache(contup);

  return visible;
}

   
                                                                                   
   
                                                                     
   
Oid
get_statistics_object_oid(List *names, bool missing_ok)
{
  char *schemaname;
  char *stats_name;
  Oid namespaceId;
  Oid stats_oid = InvalidOid;
  ListCell *l;

                                 
  DeconstructQualifiedName(names, &schemaname, &stats_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      stats_oid = InvalidOid;
    }
    else
    {
      stats_oid = GetSysCacheOid2(STATEXTNAMENSP, Anum_pg_statistic_ext_oid, PointerGetDatum(stats_name), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }
      stats_oid = GetSysCacheOid2(STATEXTNAMENSP, Anum_pg_statistic_ext_oid, PointerGetDatum(stats_name), ObjectIdGetDatum(namespaceId));
      if (OidIsValid(stats_oid))
      {
        break;
      }
    }
  }

  if (!OidIsValid(stats_oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("statistics object \"%s\" does not exist", NameListToString(names))));
  }

  return stats_oid;
}

   
                          
                                                                            
                                                                         
                                                 
   
bool
StatisticsObjIsVisible(Oid relid)
{
  HeapTuple stxtup;
  Form_pg_statistic_ext stxform;
  Oid stxnamespace;
  bool visible;

  stxtup = SearchSysCache1(STATEXTOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(stxtup))
  {
    elog(ERROR, "cache lookup failed for statistics object %u", relid);
  }
  stxform = (Form_pg_statistic_ext)GETSTRUCT(stxtup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  stxnamespace = stxform->stxnamespace;
  if (stxnamespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, stxnamespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                           
                                                                 
       
    char *stxname = NameStr(stxform->stxname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == stxnamespace)
      {
                                    
        visible = true;
        break;
      }
      if (SearchSysCacheExists2(STATEXTNAMENSP, PointerGetDatum(stxname), ObjectIdGetDatum(namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(stxtup);

  return visible;
}

   
                                                                   
   
                                                                     
   
Oid
get_ts_parser_oid(List *names, bool missing_ok)
{
  char *schemaname;
  char *parser_name;
  Oid namespaceId;
  Oid prsoid = InvalidOid;
  ListCell *l;

                                 
  DeconstructQualifiedName(names, &schemaname, &parser_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      prsoid = InvalidOid;
    }
    else
    {
      prsoid = GetSysCacheOid2(TSPARSERNAMENSP, Anum_pg_ts_parser_oid, PointerGetDatum(parser_name), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      prsoid = GetSysCacheOid2(TSPARSERNAMENSP, Anum_pg_ts_parser_oid, PointerGetDatum(parser_name), ObjectIdGetDatum(namespaceId));
      if (OidIsValid(prsoid))
      {
        break;
      }
    }
  }

  if (!OidIsValid(prsoid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search parser \"%s\" does not exist", NameListToString(names))));
  }

  return prsoid;
}

   
                     
                                                                     
                                                                     
                                      
   
bool
TSParserIsVisible(Oid prsId)
{
  HeapTuple tup;
  Form_pg_ts_parser form;
  Oid namespace;
  bool visible;

  tup = SearchSysCache1(TSPARSEROID, ObjectIdGetDatum(prsId));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search parser %u", prsId);
  }
  form = (Form_pg_ts_parser)GETSTRUCT(tup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  namespace = form->prsnamespace;
  if (namespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, namespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                         
                                                        
       
    char *name = NameStr(form->prsname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      if (namespaceId == namespace)
      {
                                    
        visible = true;
        break;
      }
      if (SearchSysCacheExists2(TSPARSERNAMENSP, PointerGetDatum(name), ObjectIdGetDatum(namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(tup);

  return visible;
}

   
                                                                     
   
                                                                 
   
Oid
get_ts_dict_oid(List *names, bool missing_ok)
{
  char *schemaname;
  char *dict_name;
  Oid namespaceId;
  Oid dictoid = InvalidOid;
  ListCell *l;

                                 
  DeconstructQualifiedName(names, &schemaname, &dict_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      dictoid = InvalidOid;
    }
    else
    {
      dictoid = GetSysCacheOid2(TSDICTNAMENSP, Anum_pg_ts_dict_oid, PointerGetDatum(dict_name), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      dictoid = GetSysCacheOid2(TSDICTNAMENSP, Anum_pg_ts_dict_oid, PointerGetDatum(dict_name), ObjectIdGetDatum(namespaceId));
      if (OidIsValid(dictoid))
      {
        break;
      }
    }
  }

  if (!OidIsValid(dictoid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search dictionary \"%s\" does not exist", NameListToString(names))));
  }

  return dictoid;
}

   
                         
                                                                         
                                                                     
                                          
   
bool
TSDictionaryIsVisible(Oid dictId)
{
  HeapTuple tup;
  Form_pg_ts_dict form;
  Oid namespace;
  bool visible;

  tup = SearchSysCache1(TSDICTOID, ObjectIdGetDatum(dictId));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search dictionary %u", dictId);
  }
  form = (Form_pg_ts_dict)GETSTRUCT(tup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  namespace = form->dictnamespace;
  if (namespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, namespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                          
                                                                
       
    char *name = NameStr(form->dictname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      if (namespaceId == namespace)
      {
                                    
        visible = true;
        break;
      }
      if (SearchSysCacheExists2(TSDICTNAMENSP, PointerGetDatum(name), ObjectIdGetDatum(namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(tup);

  return visible;
}

   
                                                                       
   
                                                                     
   
Oid
get_ts_template_oid(List *names, bool missing_ok)
{
  char *schemaname;
  char *template_name;
  Oid namespaceId;
  Oid tmploid = InvalidOid;
  ListCell *l;

                                 
  DeconstructQualifiedName(names, &schemaname, &template_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      tmploid = InvalidOid;
    }
    else
    {
      tmploid = GetSysCacheOid2(TSTEMPLATENAMENSP, Anum_pg_ts_template_oid, PointerGetDatum(template_name), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      tmploid = GetSysCacheOid2(TSTEMPLATENAMENSP, Anum_pg_ts_template_oid, PointerGetDatum(template_name), ObjectIdGetDatum(namespaceId));
      if (OidIsValid(tmploid))
      {
        break;
      }
    }
  }

  if (!OidIsValid(tmploid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search template \"%s\" does not exist", NameListToString(names))));
  }

  return tmploid;
}

   
                       
                                                                       
                                                                     
                                        
   
bool
TSTemplateIsVisible(Oid tmplId)
{
  HeapTuple tup;
  Form_pg_ts_template form;
  Oid namespace;
  bool visible;

  tup = SearchSysCache1(TSTEMPLATEOID, ObjectIdGetDatum(tmplId));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search template %u", tmplId);
  }
  form = (Form_pg_ts_template)GETSTRUCT(tup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  namespace = form->tmplnamespace;
  if (namespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, namespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                           
                                                          
       
    char *name = NameStr(form->tmplname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      if (namespaceId == namespace)
      {
                                    
        visible = true;
        break;
      }
      if (SearchSysCacheExists2(TSTEMPLATENAMENSP, PointerGetDatum(name), ObjectIdGetDatum(namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(tup);

  return visible;
}

   
                                                                   
   
                                                                     
   
Oid
get_ts_config_oid(List *names, bool missing_ok)
{
  char *schemaname;
  char *config_name;
  Oid namespaceId;
  Oid cfgoid = InvalidOid;
  ListCell *l;

                                 
  DeconstructQualifiedName(names, &schemaname, &config_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      cfgoid = InvalidOid;
    }
    else
    {
      cfgoid = GetSysCacheOid2(TSCONFIGNAMENSP, Anum_pg_ts_config_oid, PointerGetDatum(config_name), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      cfgoid = GetSysCacheOid2(TSCONFIGNAMENSP, Anum_pg_ts_config_oid, PointerGetDatum(config_name), ObjectIdGetDatum(namespaceId));
      if (OidIsValid(cfgoid))
      {
        break;
      }
    }
  }

  if (!OidIsValid(cfgoid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search configuration \"%s\" does not exist", NameListToString(names))));
  }

  return cfgoid;
}

   
                     
                                                                      
                                                                          
                                                                      
   
bool
TSConfigIsVisible(Oid cfgid)
{
  HeapTuple tup;
  Form_pg_ts_config form;
  Oid namespace;
  bool visible;

  tup = SearchSysCache1(TSCONFIGOID, ObjectIdGetDatum(cfgid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search configuration %u", cfgid);
  }
  form = (Form_pg_ts_config)GETSTRUCT(tup);

  recomputeNamespacePath();

     
                                                                             
                                                                           
                                 
     
  namespace = form->cfgnamespace;
  if (namespace != PG_CATALOG_NAMESPACE && !list_member_oid(activeSearchPath, namespace))
  {
    visible = false;
  }
  else
  {
       
                                                                        
                                                                       
                                                                        
       
    char *name = NameStr(form->cfgname);
    ListCell *l;

    visible = false;
    foreach (l, activeSearchPath)
    {
      Oid namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      if (namespaceId == namespace)
      {
                                    
        visible = true;
        break;
      }
      if (SearchSysCacheExists2(TSCONFIGNAMENSP, PointerGetDatum(name), ObjectIdGetDatum(namespaceId)))
      {
                                                
        break;
      }
    }
  }

  ReleaseSysCache(tup);

  return visible;
}

   
                            
                                                                         
                                             
   
                                                                  
   
void
DeconstructQualifiedName(List *names, char **nspname_p, char **objname_p)
{
  char *catalogname;
  char *schemaname = NULL;
  char *objname = NULL;

  switch (list_length(names))
  {
  case 1:
    objname = strVal(linitial(names));
    break;
  case 2:
    schemaname = strVal(linitial(names));
    objname = strVal(lsecond(names));
    break;
  case 3:
    catalogname = strVal(linitial(names));
    schemaname = strVal(lsecond(names));
    objname = strVal(lthird(names));

       
                                                     
       
    if (strcmp(catalogname, get_database_name(MyDatabaseId)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cross-database references are not implemented: %s", NameListToString(names))));
    }
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("improper qualified name (too many dotted names): %s", NameListToString(names))));
    break;
  }

  *nspname_p = schemaname;
  *objname_p = objname;
}

   
                          
                           
   
                                                          
   
                                                                    
                                                                 
                                                                   
   
Oid
LookupNamespaceNoError(const char *nspname)
{
                               
  if (strcmp(nspname, "pg_temp") == 0)
  {
    if (OidIsValid(myTempNamespace))
    {
      InvokeNamespaceSearchHook(myTempNamespace, true);
      return myTempNamespace;
    }

       
                                                                         
                                                                           
                                                                           
       
    return InvalidOid;
  }

  return get_namespace_oid(nspname, true);
}

   
                           
                                                                    
                                                    
   
                             
   
Oid
LookupExplicitNamespace(const char *nspname, bool missing_ok)
{
  Oid namespaceId;
  AclResult aclresult;

                               
  if (strcmp(nspname, "pg_temp") == 0)
  {
    if (OidIsValid(myTempNamespace))
    {
      return myTempNamespace;
    }

       
                                                                         
                                                                           
                                                                        
       
  }

  namespaceId = get_namespace_oid(nspname, missing_ok);
  if (missing_ok && !OidIsValid(namespaceId))
  {
    return InvalidOid;
  }

  aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, nspname);
  }
                                          
  InvokeNamespaceSearchHook(namespaceId, true);

  return namespaceId;
}

   
                           
                                                               
   
                                                                      
                                                                          
   
                                                                         
                                                         
   
Oid
LookupCreationNamespace(const char *nspname)
{
  Oid namespaceId;
  AclResult aclresult;

                               
  if (strcmp(nspname, "pg_temp") == 0)
  {
                                   
    AccessTempTableNamespace(false);
    return myTempNamespace;
  }

  namespaceId = get_namespace_oid(nspname, false);

  aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, nspname);
  }

  return namespaceId;
}

   
                                          
   
                                                                         
                                                                              
                 
   
void
CheckSetNamespace(Oid oldNspOid, Oid nspOid)
{
                                                     
  if (isAnyTempNamespace(nspOid) || isAnyTempNamespace(oldNspOid))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot move objects into or out of temporary schemas")));
  }

                             
  if (nspOid == PG_TOAST_NAMESPACE || oldNspOid == PG_TOAST_NAMESPACE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot move objects into or out of TOAST schema")));
  }
}

   
                                     
                                                                     
                                                                       
                                                                      
   
                                                                        
                                                                 
   
                                                                         
                                                         
   
Oid
QualifiedNameGetCreationNamespace(List *names, char **objname_p)
{
  char *schemaname;
  Oid namespaceId;

                                 
  DeconstructQualifiedName(names, &schemaname, objname_p);

  if (schemaname)
  {
                                 
    if (strcmp(schemaname, "pg_temp") == 0)
    {
                                     
      AccessTempTableNamespace(false);
      return myTempNamespace;
    }
                                
    namespaceId = get_namespace_oid(schemaname, false);
                                                
  }
  else
  {
                                            
    recomputeNamespacePath();
    if (activeTempCreationPending)
    {
                                             
      AccessTempTableNamespace(true);
      return myTempNamespace;
    }
    namespaceId = activeCreationNamespace;
    if (!OidIsValid(namespaceId))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("no schema has been selected to create in")));
    }
  }

  return namespaceId;
}

   
                                                               
   
                                                                           
                                 
   
Oid
get_namespace_oid(const char *nspname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(NAMESPACENAME, Anum_pg_namespace_oid, CStringGetDatum(nspname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("schema \"%s\" does not exist", nspname)));
  }

  return oid;
}

   
                            
                                                                         
   
RangeVar *
makeRangeVarFromNameList(List *names)
{
  RangeVar *rel = makeRangeVar(NULL, NULL, -1);

  switch (list_length(names))
  {
  case 1:
    rel->relname = strVal(linitial(names));
    break;
  case 2:
    rel->schemaname = strVal(linitial(names));
    rel->relname = strVal(lsecond(names));
    break;
  case 3:
    rel->catalogname = strVal(linitial(names));
    rel->schemaname = strVal(lsecond(names));
    rel->relname = strVal(lthird(names));
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("improper relation name (too many dotted names): %s", NameListToString(names))));
    break;
  }

  return rel;
}

   
                    
                                                                    
   
                                                                         
                                                  
   
                                                                       
                                                                         
   
char *
NameListToString(List *names)
{
  StringInfoData string;
  ListCell *l;

  initStringInfo(&string);

  foreach (l, names)
  {
    Node *name = (Node *)lfirst(l);

    if (l != list_head(names))
    {
      appendStringInfoChar(&string, '.');
    }

    if (IsA(name, String))
    {
      appendStringInfoString(&string, strVal(name));
    }
    else if (IsA(name, A_Star))
    {
      appendStringInfoChar(&string, '*');
    }
    else
    {
      elog(ERROR, "unexpected node type in name list: %d", (int)nodeTag(name));
    }
  }

  return string.data;
}

   
                          
                                                                    
   
                                                                          
                                                                      
   
char *
NameListToQuotedString(List *names)
{
  StringInfoData string;
  ListCell *l;

  initStringInfo(&string);

  foreach (l, names)
  {
    if (l != list_head(names))
    {
      appendStringInfoChar(&string, '.');
    }
    appendStringInfoString(&string, quote_identifier(strVal(lfirst(l))));
  }

  return string.data;
}

   
                                                                          
   
bool
isTempNamespace(Oid namespaceId)
{
  if (OidIsValid(myTempNamespace) && myTempNamespace == namespaceId)
  {
    return true;
  }
  return false;
}

   
                                                                          
               
   
bool
isTempToastNamespace(Oid namespaceId)
{
  if (OidIsValid(myTempToastNamespace) && myTempToastNamespace == namespaceId)
  {
    return true;
  }
  return false;
}

   
                                                                          
                                                     
   
bool
isTempOrTempToastNamespace(Oid namespaceId)
{
  if (OidIsValid(myTempNamespace) && (myTempNamespace == namespaceId || myTempToastNamespace == namespaceId))
  {
    return true;
  }
  return false;
}

   
                                                                           
                                                                            
                      
   
bool
isAnyTempNamespace(Oid namespaceId)
{
  bool result;
  char *nspname;

                                                                             
  nspname = get_namespace_name(namespaceId);
  if (!nspname)
  {
    return false;                         
  }
  result = (strncmp(nspname, "pg_temp_", 8) == 0) || (strncmp(nspname, "pg_toast_temp_", 14) == 0);
  pfree(nspname);
  return result;
}

   
                                                                      
                                                                           
   
                                                                          
                                                                        
   
bool
isOtherTempNamespace(Oid namespaceId)
{
                                                  
  if (isTempOrTempToastNamespace(namespaceId))
  {
    return false;
  }
                                                    
  return isAnyTempNamespace(namespaceId);
}

   
                                                                             
                 
   
                                                                         
                                                                         
                                                                         
                                                   
   
TempNamespaceStatus
checkTempNamespaceStatus(Oid namespaceId)
{
  PGPROC *proc;
  int backendId;

  Assert(OidIsValid(MyDatabaseId));

  backendId = GetTempNamespaceBackendId(namespaceId);

                                                           
  if (backendId == InvalidBackendId)
  {
    return TEMP_NAMESPACE_NOT_TEMP;
  }

                             
  proc = BackendIdGetProc(backendId);
  if (proc == NULL)
  {
    return TEMP_NAMESPACE_IDLE;
  }

                                                                        
  if (proc->databaseId != MyDatabaseId)
  {
    return TEMP_NAMESPACE_IDLE;
  }

                                                     
  if (proc->tempNamespaceId != namespaceId)
  {
    return TEMP_NAMESPACE_IDLE;
  }

                                 
  return TEMP_NAMESPACE_IN_USE;
}

   
                                                                
                            
   
bool
isTempNamespaceInUse(Oid namespaceId)
{
  return checkTempNamespaceStatus(namespaceId) == TEMP_NAMESPACE_IN_USE;
}

   
                                                                           
                                                                         
                                                                      
                                                          
   
int
GetTempNamespaceBackendId(Oid namespaceId)
{
  int result;
  char *nspname;

                                                                            
  nspname = get_namespace_name(namespaceId);
  if (!nspname)
  {
    return InvalidBackendId;                         
  }
  if (strncmp(nspname, "pg_temp_", 8) == 0)
  {
    result = atoi(nspname + 8);
  }
  else if (strncmp(nspname, "pg_toast_temp_", 14) == 0)
  {
    result = atoi(nspname + 14);
  }
  else
  {
    result = InvalidBackendId;
  }
  pfree(nspname);
  return result;
}

   
                                                                              
                                                                             
                                                                                
   
Oid
GetTempToastNamespace(void)
{
  Assert(OidIsValid(myTempToastNamespace));
  return myTempToastNamespace;
}

   
                                                                         
   
                                                                           
                               
   
void
GetTempNamespaceState(Oid *tempNamespaceId, Oid *tempToastNamespaceId)
{
                                                                             
  *tempNamespaceId = myTempNamespace;
  *tempToastNamespaceId = myTempToastNamespace;
}

   
                                                                       
   
                                                                               
                                                                             
                                                                               
         
   
void
SetTempNamespaceState(Oid tempNamespaceId, Oid tempToastNamespaceId)
{
                                                             
  Assert(myTempNamespace == InvalidOid);
  Assert(myTempToastNamespace == InvalidOid);
  Assert(myTempNamespaceSubID == InvalidSubTransactionId);

                                                  
  myTempNamespace = tempNamespaceId;
  myTempToastNamespace = tempToastNamespaceId;

     
                                                                         
                                                                          
                                                                             
                    
     

  baseSearchPathValid = false;                               
}

   
                                                                        
                                   
   
                                                                     
                                                                        
                                                                              
   
OverrideSearchPath *
GetOverrideSearchPath(MemoryContext context)
{
  OverrideSearchPath *result;
  List *schemas;
  MemoryContext oldcxt;

  recomputeNamespacePath();

  oldcxt = MemoryContextSwitchTo(context);

  result = (OverrideSearchPath *)palloc0(sizeof(OverrideSearchPath));
  schemas = list_copy(activeSearchPath);
  while (schemas && linitial_oid(schemas) != activeCreationNamespace)
  {
    if (linitial_oid(schemas) == myTempNamespace)
    {
      result->addTemp = true;
    }
    else
    {
      Assert(linitial_oid(schemas) == PG_CATALOG_NAMESPACE);
      result->addCatalog = true;
    }
    schemas = list_delete_first(schemas);
  }
  result->schemas = schemas;

  MemoryContextSwitchTo(oldcxt);

  return result;
}

   
                                                                   
   
                                                              
   
OverrideSearchPath *
CopyOverrideSearchPath(OverrideSearchPath *path)
{
  OverrideSearchPath *result;

  result = (OverrideSearchPath *)palloc(sizeof(OverrideSearchPath));
  result->schemas = list_copy(path->schemas);
  result->addCatalog = path->addCatalog;
  result->addTemp = path->addTemp;

  return result;
}

   
                                                                       
   
bool
OverrideSearchPathMatchesCurrent(OverrideSearchPath *path)
{
  ListCell *lc, *lcp;

  recomputeNamespacePath();

                                                                         
  lc = list_head(activeSearchPath);

                                                                 
  if (path->addTemp)
  {
    if (lc && lfirst_oid(lc) == myTempNamespace)
    {
      lc = lnext(lc);
    }
    else
    {
      return false;
    }
  }
                                                            
  if (path->addCatalog)
  {
    if (lc && lfirst_oid(lc) == PG_CATALOG_NAMESPACE)
    {
      lc = lnext(lc);
    }
    else
    {
      return false;
    }
  }
                                                                
  if (activeCreationNamespace != (lc ? lfirst_oid(lc) : InvalidOid))
  {
    return false;
  }
                                                                     
  foreach (lcp, path->schemas)
  {
    if (lc && lfirst_oid(lc) == lfirst_oid(lcp))
    {
      lc = lnext(lc);
    }
    else
    {
      return false;
    }
  }
  if (lc)
  {
    return false;
  }
  return true;
}

   
                                                                 
   
                                                                       
                                                                
   
                                                                         
                                                                          
                                                                             
                                                                          
                                                                             
                                                        
   
                                                                         
                                                                            
                                                                           
   
void
PushOverrideSearchPath(OverrideSearchPath *newpath)
{
  OverrideStackEntry *entry;
  List *oidlist;
  Oid firstNS;
  MemoryContext oldcxt;

     
                                                                   
                                                                           
     
  oldcxt = MemoryContextSwitchTo(TopMemoryContext);

  oidlist = list_copy(newpath->schemas);

     
                                                     
     
  if (oidlist == NIL)
  {
    firstNS = InvalidOid;
  }
  else
  {
    firstNS = linitial_oid(oidlist);
  }

     
                                                                           
                                                                     
                            
     
  if (newpath->addCatalog)
  {
    oidlist = lcons_oid(PG_CATALOG_NAMESPACE, oidlist);
  }

  if (newpath->addTemp && OidIsValid(myTempNamespace))
  {
    oidlist = lcons_oid(myTempNamespace, oidlist);
  }

     
                                                                        
     
  entry = (OverrideStackEntry *)palloc(sizeof(OverrideStackEntry));
  entry->searchPath = oidlist;
  entry->creationNamespace = firstNS;
  entry->nestLevel = GetCurrentTransactionNestLevel();

  overrideStack = lcons(entry, overrideStack);

                           
  activeSearchPath = entry->searchPath;
  activeCreationNamespace = entry->creationNamespace;
  activeTempCreationPending = false;                      

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                  
   
                                                                             
                                                                        
   
void
PopOverrideSearchPath(void)
{
  OverrideStackEntry *entry;

                      
  if (overrideStack == NIL)
  {
    elog(ERROR, "bogus PopOverrideSearchPath call");
  }
  entry = (OverrideStackEntry *)linitial(overrideStack);
  if (entry->nestLevel != GetCurrentTransactionNestLevel())
  {
    elog(ERROR, "bogus PopOverrideSearchPath call");
  }

                                       
  overrideStack = list_delete_first(overrideStack);
  list_free(entry->searchPath);
  pfree(entry);

                                     
  if (overrideStack)
  {
    entry = (OverrideStackEntry *)linitial(overrideStack);
    activeSearchPath = entry->searchPath;
    activeCreationNamespace = entry->creationNamespace;
    activeTempCreationPending = false;                      
  }
  else
  {
                                                                  
    activeSearchPath = baseSearchPath;
    activeCreationNamespace = baseCreationNamespace;
    activeTempCreationPending = baseTempCreationPending;
  }
}

   
                                                                   
   
                                                                       
                        
   
Oid
get_collation_oid(List *name, bool missing_ok)
{
  char *schemaname;
  char *collation_name;
  int32 dbencoding = GetDatabaseEncoding();
  Oid namespaceId;
  Oid colloid;
  ListCell *l;

                                 
  DeconstructQualifiedName(name, &schemaname, &collation_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      return InvalidOid;
    }

    colloid = lookup_collation(collation_name, namespaceId, dbencoding);
    if (OidIsValid(colloid))
    {
      return colloid;
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      colloid = lookup_collation(collation_name, namespaceId, dbencoding);
      if (OidIsValid(colloid))
      {
        return colloid;
      }
    }
  }

                         
  if (!missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("collation \"%s\" for encoding \"%s\" does not exist", NameListToString(name), GetDatabaseEncodingName())));
  }
  return InvalidOid;
}

   
                                                                     
   
Oid
get_conversion_oid(List *name, bool missing_ok)
{
  char *schemaname;
  char *conversion_name;
  Oid namespaceId;
  Oid conoid = InvalidOid;
  ListCell *l;

                                 
  DeconstructQualifiedName(name, &schemaname, &conversion_name);

  if (schemaname)
  {
                                
    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (missing_ok && !OidIsValid(namespaceId))
    {
      conoid = InvalidOid;
    }
    else
    {
      conoid = GetSysCacheOid2(CONNAMENSP, Anum_pg_conversion_oid, PointerGetDatum(conversion_name), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                      
    recomputeNamespacePath();

    foreach (l, activeSearchPath)
    {
      namespaceId = lfirst_oid(l);

      if (namespaceId == myTempNamespace)
      {
        continue;                                    
      }

      conoid = GetSysCacheOid2(CONNAMENSP, Anum_pg_conversion_oid, PointerGetDatum(conversion_name), ObjectIdGetDatum(namespaceId));
      if (OidIsValid(conoid))
      {
        return conoid;
      }
    }
  }

                         
  if (!OidIsValid(conoid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("conversion \"%s\" does not exist", NameListToString(name))));
  }
  return conoid;
}

   
                                                                     
   
Oid
FindDefaultConversionProc(int32 for_encoding, int32 to_encoding)
{
  Oid proc;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    proc = FindDefaultConversion(namespaceId, for_encoding, to_encoding);
    if (OidIsValid(proc))
    {
      return proc;
    }
  }

                         
  return InvalidOid;
}

   
                                                                        
   
static void
recomputeNamespacePath(void)
{
  Oid roleid = GetUserId();
  char *rawname;
  List *namelist;
  List *oidlist;
  List *newpath;
  ListCell *l;
  bool temp_missing;
  Oid firstNS;
  MemoryContext oldcxt;

                                                        
  if (overrideStack)
  {
    return;
  }

                                            
  if (baseSearchPathValid && namespaceUser == roleid)
  {
    return;
  }

                                                              
  rawname = pstrdup(namespace_search_path);

                                             
  if (!SplitIdentifierString(rawname, ',', &namelist))
  {
                                   
                                                                 
    elog(ERROR, "invalid list syntax");
  }

     
                                                                        
                                                                           
                                                                        
                                                                   
     
  oidlist = NIL;
  temp_missing = false;
  foreach (l, namelist)
  {
    char *curname = (char *)lfirst(l);
    Oid namespaceId;

    if (strcmp(curname, "$user") == 0)
    {
                                                                     
      HeapTuple tuple;

      tuple = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
      if (HeapTupleIsValid(tuple))
      {
        char *rname;

        rname = NameStr(((Form_pg_authid)GETSTRUCT(tuple))->rolname);
        namespaceId = get_namespace_oid(rname, true);
        ReleaseSysCache(tuple);
        if (OidIsValid(namespaceId) && !list_member_oid(oidlist, namespaceId) && pg_namespace_aclcheck(namespaceId, roleid, ACL_USAGE) == ACLCHECK_OK && InvokeNamespaceSearchHook(namespaceId, false))
        {
          oidlist = lappend_oid(oidlist, namespaceId);
        }
      }
    }
    else if (strcmp(curname, "pg_temp") == 0)
    {
                                                         
      if (OidIsValid(myTempNamespace))
      {
        if (!list_member_oid(oidlist, myTempNamespace) && InvokeNamespaceSearchHook(myTempNamespace, false))
        {
          oidlist = lappend_oid(oidlist, myTempNamespace);
        }
      }
      else
      {
                                                                
        if (oidlist == NIL)
        {
          temp_missing = true;
        }
      }
    }
    else
    {
                                      
      namespaceId = get_namespace_oid(curname, true);
      if (OidIsValid(namespaceId) && !list_member_oid(oidlist, namespaceId) && pg_namespace_aclcheck(namespaceId, roleid, ACL_USAGE) == ACLCHECK_OK && InvokeNamespaceSearchHook(namespaceId, false))
      {
        oidlist = lappend_oid(oidlist, namespaceId);
      }
    }
  }

     
                                                                     
                                                                           
                                                    
     
  if (oidlist == NIL)
  {
    firstNS = InvalidOid;
  }
  else
  {
    firstNS = linitial_oid(oidlist);
  }

     
                                                                           
                                                                     
                            
     
  if (!list_member_oid(oidlist, PG_CATALOG_NAMESPACE))
  {
    oidlist = lcons_oid(PG_CATALOG_NAMESPACE, oidlist);
  }

  if (OidIsValid(myTempNamespace) && !list_member_oid(oidlist, myTempNamespace))
  {
    oidlist = lcons_oid(myTempNamespace, oidlist);
  }

     
                                                                            
                              
     
  oldcxt = MemoryContextSwitchTo(TopMemoryContext);
  newpath = list_copy(oidlist);
  MemoryContextSwitchTo(oldcxt);

                                              
  list_free(baseSearchPath);
  baseSearchPath = newpath;
  baseCreationNamespace = firstNS;
  baseTempCreationPending = temp_missing;

                            
  baseSearchPathValid = true;
  namespaceUser = roleid;

                           
  activeSearchPath = baseSearchPath;
  activeCreationNamespace = baseCreationNamespace;
  activeTempCreationPending = baseTempCreationPending;

                 
  pfree(rawname);
  list_free(namelist);
  list_free(oidlist);
}

   
                            
                                                                     
                                                                      
                                                                     
                                                                      
                                                                   
   
static void
AccessTempTableNamespace(bool force)
{
     
                                                                       
                  
     
  MyXactFlags |= XACT_FLAGS_ACCESSEDTEMPNAMESPACE;

     
                                                                       
                                                                             
                           
     
  if (!force && OidIsValid(myTempNamespace))
  {
    return;
  }

     
                                                                   
                    
     
  InitTempTableNamespace();
}

   
                          
                                                                         
   
static void
InitTempTableNamespace(void)
{
  char namespaceName[NAMEDATALEN];
  Oid namespaceId;
  Oid toastspaceId;

  Assert(!OidIsValid(myTempNamespace));

     
                                                                         
                                                                           
                                                
     
                                                                             
                                                                             
                                                                          
                                                                             
     
  if (pg_database_aclcheck(MyDatabaseId, GetUserId(), ACL_CREATE_TEMP) != ACLCHECK_OK)
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create temporary tables in database \"%s\"", get_database_name(MyDatabaseId))));
  }

     
                                                                         
                                                                    
                                                                        
                                                                       
                                                                           
                                                                       
                                                                            
                                                            
     
  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION), errmsg("cannot create temporary tables during recovery")));
  }

                                                               
  if (IsParallelWorker())
  {
    ereport(ERROR, (errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION), errmsg("cannot create temporary tables during a parallel operation")));
  }

  snprintf(namespaceName, sizeof(namespaceName), "pg_temp_%d", MyBackendId);

  namespaceId = get_namespace_oid(namespaceName, true);
  if (!OidIsValid(namespaceId))
  {
       
                                                                         
                                                                          
                                                                          
                                                                       
                                                                        
                                                                   
       
    namespaceId = NamespaceCreate(namespaceName, BOOTSTRAP_SUPERUSERID, true);
                                                           
    CommandCounterIncrement();
  }
  else
  {
       
                                                                         
                                        
       
    RemoveTempRelations(namespaceId);
  }

     
                                                                          
                                                                             
                                                                   
     
  snprintf(namespaceName, sizeof(namespaceName), "pg_toast_temp_%d", MyBackendId);

  toastspaceId = get_namespace_oid(namespaceName, true);
  if (!OidIsValid(toastspaceId))
  {
    toastspaceId = NamespaceCreate(namespaceName, BOOTSTRAP_SUPERUSERID, true);
                                                           
    CommandCounterIncrement();
  }

     
                                                                             
                                                                            
                                            
     
  myTempNamespace = namespaceId;
  myTempToastNamespace = toastspaceId;

     
                                                                           
                                                                       
                                                                           
                                                                             
                                                                          
                                                                           
                                                                             
                                       
     
  MyProc->tempNamespaceId = namespaceId;

                                      
  AssertState(myTempNamespaceSubID == InvalidSubTransactionId);
  myTempNamespaceSubID = GetCurrentSubTransactionId();

  baseSearchPathValid = false;                           
}

   
                                              
   
void
AtEOXact_Namespace(bool isCommit, bool parallel)
{
     
                                                                         
                                                                          
                                                                           
                                                                           
                                                                           
                                                 
     
  if (myTempNamespaceSubID != InvalidSubTransactionId && !parallel)
  {
    if (isCommit)
    {
      before_shmem_exit(RemoveTempRelationsCallback, 0);
    }
    else
    {
      myTempNamespace = InvalidOid;
      myTempToastNamespace = InvalidOid;
      baseSearchPathValid = false;                           

         
                                                                       
                                   
         
                                                                       
                                                                     
                                                                       
                               
         
      MyProc->tempNamespaceId = InvalidOid;
    }
    myTempNamespaceSubID = InvalidSubTransactionId;
  }

     
                                                            
     
  if (overrideStack)
  {
    if (isCommit)
    {
      elog(WARNING, "leaked override search path");
    }
    while (overrideStack)
    {
      OverrideStackEntry *entry;

      entry = (OverrideStackEntry *)linitial(overrideStack);
      overrideStack = list_delete_first(overrideStack);
      list_free(entry->searchPath);
      pfree(entry);
    }
                                                                  
    activeSearchPath = baseSearchPath;
    activeCreationNamespace = baseCreationNamespace;
    activeTempCreationPending = baseTempCreationPending;
  }
}

   
                         
   
                                                                   
                                      
   
                                                             
   
void
AtEOSubXact_Namespace(bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid)
{
  OverrideStackEntry *entry;

  if (myTempNamespaceSubID == mySubid)
  {
    if (isCommit)
    {
      myTempNamespaceSubID = parentSubid;
    }
    else
    {
      myTempNamespaceSubID = InvalidSubTransactionId;
                                                          
      myTempNamespace = InvalidOid;
      myTempToastNamespace = InvalidOid;
      baseSearchPathValid = false;                           

         
                                                                       
                                   
         
                                                                       
                                                                        
                                                                       
                               
         
      MyProc->tempNamespaceId = InvalidOid;
    }
  }

     
                                                            
     
  while (overrideStack)
  {
    entry = (OverrideStackEntry *)linitial(overrideStack);
    if (entry->nestLevel < GetCurrentTransactionNestLevel())
    {
      break;
    }
    if (isCommit)
    {
      elog(WARNING, "leaked override search path");
    }
    overrideStack = list_delete_first(overrideStack);
    list_free(entry->searchPath);
    pfree(entry);
  }

                                     
  if (overrideStack)
  {
    entry = (OverrideStackEntry *)linitial(overrideStack);
    activeSearchPath = entry->searchPath;
    activeCreationNamespace = entry->creationNamespace;
    activeTempCreationPending = false;                      
  }
  else
  {
                                                                  
    activeSearchPath = baseSearchPath;
    activeCreationNamespace = baseCreationNamespace;
    activeTempCreationPending = baseTempCreationPending;
  }
}

   
                                                         
   
                                                                       
                                                                        
                                                                       
                      
   
static void
RemoveTempRelations(Oid tempNamespaceId)
{
  ObjectAddress object;

     
                                                                           
                                                                        
                                                                             
                                                                           
                       
     
  object.classId = NamespaceRelationId;
  object.objectId = tempNamespaceId;
  object.objectSubId = 0;

  performDeletion(&object, DROP_CASCADE, PERFORM_DELETION_INTERNAL | PERFORM_DELETION_QUIETLY | PERFORM_DELETION_SKIP_ORIGINAL | PERFORM_DELETION_SKIP_EXTENSIONS);
}

   
                                                      
   
static void
RemoveTempRelationsCallback(int code, Datum arg)
{
  if (OidIsValid(myTempNamespace))                            
  {
                                                      
    AbortOutOfAnyTransaction();
    StartTransactionCommand();
    PushActiveSnapshot(GetTransactionSnapshot());

    RemoveTempRelations(myTempNamespace);

    PopActiveSnapshot();
    CommitTransactionCommand();
  }
}

   
                                                        
   
void
ResetTempTableNamespace(void)
{
  if (OidIsValid(myTempNamespace))
  {
    RemoveTempRelations(myTempNamespace);
  }
}

   
                                                         
   

                                                
bool
check_search_path(char **newval, void **extra, GucSource source)
{
  char *rawname;
  List *namelist;

                                        
  rawname = pstrdup(*newval);

                                             
  if (!SplitIdentifierString(rawname, ',', &namelist))
  {
                                   
    GUC_check_errdetail("List syntax is invalid.");
    pfree(rawname);
    list_free(namelist);
    return false;
  }

     
                                                                         
                                                                       
                                                                          
                                                                             
                                                               
     

  pfree(rawname);
  list_free(namelist);

  return true;
}

                                             
void
assign_search_path(const char *newval, void *extra)
{
     
                                                                            
                                                                       
                                               
     
  baseSearchPathValid = false;
}

   
                                                                
   
                                                                           
   
void
InitializeSearchPath(void)
{
  if (IsBootstrapProcessingMode())
  {
       
                                                                       
                                                                           
       
    MemoryContext oldcxt;

    oldcxt = MemoryContextSwitchTo(TopMemoryContext);
    baseSearchPath = list_make1_oid(PG_CATALOG_NAMESPACE);
    MemoryContextSwitchTo(oldcxt);
    baseCreationNamespace = PG_CATALOG_NAMESPACE;
    baseTempCreationPending = false;
    baseSearchPathValid = true;
    namespaceUser = GetUserId();
    activeSearchPath = baseSearchPath;
    activeCreationNamespace = baseCreationNamespace;
    activeTempCreationPending = baseTempCreationPending;
  }
  else
  {
       
                                                                           
                             
       
    CacheRegisterSyscacheCallback(NAMESPACEOID, NamespaceCallback, (Datum)0);
                                                        
    baseSearchPathValid = false;
  }
}

   
                     
                                     
   
static void
NamespaceCallback(Datum arg, int cacheid, uint32 hashvalue)
{
                                                      
  baseSearchPathValid = false;
}

   
                                                                      
                                                                  
                
   
                                                                          
                            
   
                                                                         
                                                         
   
List *
fetch_search_path(bool includeImplicit)
{
  List *result;

  recomputeNamespacePath();

     
                                                                           
                                                                     
                                                                  
                                                                            
                                      
     
  if (activeTempCreationPending)
  {
    AccessTempTableNamespace(true);
    recomputeNamespacePath();
  }

  result = list_copy(activeSearchPath);
  if (!includeImplicit)
  {
    while (result && linitial_oid(result) != activeCreationNamespace)
    {
      result = list_delete_first(result);
    }
  }

  return result;
}

   
                                                                       
                                                                          
                                                    
   
                                                                          
                                                                          
                                                                       
                                                                            
   
int
fetch_search_path_array(Oid *sarray, int sarray_len)
{
  int count = 0;
  ListCell *l;

  recomputeNamespacePath();

  foreach (l, activeSearchPath)
  {
    Oid namespaceId = lfirst_oid(l);

    if (namespaceId == myTempNamespace)
    {
      continue;                                    
    }

    if (count < sarray_len)
    {
      sarray[count] = namespaceId;
    }
    count++;
  }

  return count;
}

   
                                                                
   
                                                                          
                                                                         
                                                                         
                                                                             
                                                                            
                                                                                
                                                                           
                                                                     
   

Datum
pg_table_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(RelationIsVisible(oid));
}

Datum
pg_type_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(TYPEOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(TypeIsVisible(oid));
}

Datum
pg_function_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(PROCOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(FunctionIsVisible(oid));
}

Datum
pg_operator_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(OPEROID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(OperatorIsVisible(oid));
}

Datum
pg_opclass_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(CLAOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(OpclassIsVisible(oid));
}

Datum
pg_opfamily_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(OPFAMILYOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(OpfamilyIsVisible(oid));
}

Datum
pg_collation_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(COLLOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(CollationIsVisible(oid));
}

Datum
pg_conversion_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(CONVOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(ConversionIsVisible(oid));
}

Datum
pg_statistics_obj_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(STATEXTOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(StatisticsObjIsVisible(oid));
}

Datum
pg_ts_parser_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(TSPARSEROID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(TSParserIsVisible(oid));
}

Datum
pg_ts_dict_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(TSDICTOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(TSDictionaryIsVisible(oid));
}

Datum
pg_ts_template_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(TSTEMPLATEOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(TSTemplateIsVisible(oid));
}

Datum
pg_ts_config_is_visible(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  if (!SearchSysCacheExists1(TSCONFIGOID, ObjectIdGetDatum(oid)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_BOOL(TSConfigIsVisible(oid));
}

Datum
pg_my_temp_schema(PG_FUNCTION_ARGS)
{
  PG_RETURN_OID(myTempNamespace);
}

Datum
pg_is_other_temp_schema(PG_FUNCTION_ARGS)
{
  Oid oid = PG_GETARG_OID(0);

  PG_RETURN_BOOL(isOtherTempNamespace(oid));
}
