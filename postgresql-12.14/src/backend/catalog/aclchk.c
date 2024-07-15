                                                                            
   
            
                                                   
   
                                                                         
                                                                        
   
   
                  
                                  
   
         
                
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_am.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_init_privs.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_transform.h"
#include "commands/dbcommands.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/proclang.h"
#include "commands/tablespace.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_func.h"
#include "parser/parse_type.h"
#include "utils/acl.h"
#include "utils/aclchk_internal.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                                                     
   
typedef struct
{
  Oid roleid;                  
  Oid nspid;                                        
                                                      
  bool is_grant;
  ObjectType objtype;
  bool all_privs;
  AclMode privileges;
  List *grantees;
  bool grant_option;
  DropBehavior behavior;
} InternalDefaultACL;

   
                                                                         
                                                                           
                                                                           
   
bool binary_upgrade_record_init_privs = false;

static void
ExecGrantStmt_oids(InternalGrant *istmt);
static void
ExecGrant_Relation(InternalGrant *grantStmt);
static void
ExecGrant_Database(InternalGrant *grantStmt);
static void
ExecGrant_Fdw(InternalGrant *grantStmt);
static void
ExecGrant_ForeignServer(InternalGrant *grantStmt);
static void
ExecGrant_Function(InternalGrant *grantStmt);
static void
ExecGrant_Language(InternalGrant *grantStmt);
static void
ExecGrant_Largeobject(InternalGrant *grantStmt);
static void
ExecGrant_Namespace(InternalGrant *grantStmt);
static void
ExecGrant_Tablespace(InternalGrant *grantStmt);
static void
ExecGrant_Type(InternalGrant *grantStmt);

static void
SetDefaultACLsInSchemas(InternalDefaultACL *iacls, List *nspnames);
static void
SetDefaultACL(InternalDefaultACL *iacls);

static List *
objectNamesToOids(ObjectType objtype, List *objnames);
static List *
objectsInSchemaToOids(ObjectType objtype, List *nspnames);
static List *
getRelationsInNamespace(Oid namespaceId, char relkind);
static void
expand_col_privileges(List *colnames, Oid table_oid, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges);
static void
expand_all_col_privileges(Oid table_oid, Form_pg_class classForm, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges);
static AclMode
string_to_privilege(const char *privname);
static const char *
privilege_to_string(AclMode privilege);
static AclMode
restrict_and_check_grant(bool is_grant, AclMode avail_goptions, bool all_privs, AclMode privileges, Oid objectId, Oid grantorId, ObjectType objtype, const char *objname, AttrNumber att_number, const char *colname);
static AclMode
pg_aclmask(ObjectType objtype, Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mask, AclMaskHow how);
static void
recordExtensionInitPriv(Oid objoid, Oid classoid, int objsubid, Acl *new_acl);
static void
recordExtensionInitPrivWorker(Oid objoid, Oid classoid, int objsubid, Acl *new_acl);

#ifdef ACLDEBUG
static void
dumpacl(Acl *acl)
{
  int i;
  AclItem *aip;

  elog(DEBUG2, "acl size = %d, # acls = %d", ACL_SIZE(acl), ACL_NUM(acl));
  aip = ACL_DAT(acl);
  for (i = 0; i < ACL_NUM(acl); ++i)
  {
    elog(DEBUG2, "	acl[%d]: %s", i, DatumGetCString(DirectFunctionCall1(aclitemout, PointerGetDatum(aip + i))));
  }
}
#endif               

   
                                                                  
                                                                
                                                               
   
                                        
   
static Acl *
merge_acl_with_grant(Acl *old_acl, bool is_grant, bool grant_option, DropBehavior behavior, List *grantees, AclMode privileges, Oid grantorId, Oid ownerId)
{
  unsigned modechg;
  ListCell *j;
  Acl *new_acl;

  modechg = is_grant ? ACL_MODECHG_ADD : ACL_MODECHG_DEL;

#ifdef ACLDEBUG
  dumpacl(old_acl);
#endif
  new_acl = old_acl;

  foreach (j, grantees)
  {
    AclItem aclitem;
    Acl *newer_acl;

    aclitem.ai_grantee = lfirst_oid(j);

       
                                                                          
                                                                       
                                                                         
                                  
       
    if (is_grant && grant_option && aclitem.ai_grantee == ACL_ID_PUBLIC)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("grant options can only be granted to roles")));
    }

    aclitem.ai_grantor = grantorId;

       
                                                                     
                                                                           
                                                                      
                                                                           
                                                                  
       
    ACLITEM_SET_PRIVS_GOPTIONS(aclitem, (is_grant || !grant_option) ? privileges : ACL_NO_RIGHTS, (!is_grant || grant_option) ? privileges : ACL_NO_RIGHTS);

    newer_acl = aclupdate(new_acl, &aclitem, modechg, ownerId, behavior);

                                                        
    pfree(new_acl);
    new_acl = newer_acl;

#ifdef ACLDEBUG
    dumpacl(new_acl);
#endif
  }

  return new_acl;
}

   
                                                                   
                                                      
   
static AclMode
restrict_and_check_grant(bool is_grant, AclMode avail_goptions, bool all_privs, AclMode privileges, Oid objectId, Oid grantorId, ObjectType objtype, const char *objname, AttrNumber att_number, const char *colname)
{
  AclMode this_privileges;
  AclMode whole_mask;

  switch (objtype)
  {
  case OBJECT_COLUMN:
    whole_mask = ACL_ALL_RIGHTS_COLUMN;
    break;
  case OBJECT_TABLE:
    whole_mask = ACL_ALL_RIGHTS_RELATION;
    break;
  case OBJECT_SEQUENCE:
    whole_mask = ACL_ALL_RIGHTS_SEQUENCE;
    break;
  case OBJECT_DATABASE:
    whole_mask = ACL_ALL_RIGHTS_DATABASE;
    break;
  case OBJECT_FUNCTION:
    whole_mask = ACL_ALL_RIGHTS_FUNCTION;
    break;
  case OBJECT_LANGUAGE:
    whole_mask = ACL_ALL_RIGHTS_LANGUAGE;
    break;
  case OBJECT_LARGEOBJECT:
    whole_mask = ACL_ALL_RIGHTS_LARGEOBJECT;
    break;
  case OBJECT_SCHEMA:
    whole_mask = ACL_ALL_RIGHTS_SCHEMA;
    break;
  case OBJECT_TABLESPACE:
    whole_mask = ACL_ALL_RIGHTS_TABLESPACE;
    break;
  case OBJECT_FDW:
    whole_mask = ACL_ALL_RIGHTS_FDW;
    break;
  case OBJECT_FOREIGN_SERVER:
    whole_mask = ACL_ALL_RIGHTS_FOREIGN_SERVER;
    break;
  case OBJECT_EVENT_TRIGGER:
    elog(ERROR, "grantable rights not supported for event triggers");
                                              
    return ACL_NO_RIGHTS;
  case OBJECT_TYPE:
    whole_mask = ACL_ALL_RIGHTS_TYPE;
    break;
  default:
    elog(ERROR, "unrecognized object type: %d", objtype);
                                              
    return ACL_NO_RIGHTS;
  }

     
                                                                           
                                                                         
           
     
  if (avail_goptions == ACL_NO_RIGHTS)
  {
    if (pg_aclmask(objtype, objectId, att_number, grantorId, whole_mask | ACL_GRANT_OPTION_FOR(whole_mask), ACLMASK_ANY) == ACL_NO_RIGHTS)
    {
      if (objtype == OBJECT_COLUMN && colname)
      {
        aclcheck_error_col(ACLCHECK_NO_PRIV, objtype, objname, colname);
      }
      else
      {
        aclcheck_error(ACLCHECK_NO_PRIV, objtype, objname);
      }
    }
  }

     
                                                                         
                                                                            
                                                                            
                                                                           
                                                              
     
  this_privileges = privileges & ACL_OPTION_TO_PRIVS(avail_goptions);
  if (is_grant)
  {
    if (this_privileges == 0)
    {
      if (objtype == OBJECT_COLUMN && colname)
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_GRANTED), errmsg("no privileges were granted for column \"%s\" of relation \"%s\"", colname, objname)));
      }
      else
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_GRANTED), errmsg("no privileges were granted for \"%s\"", objname)));
      }
    }
    else if (!all_privs && this_privileges != privileges)
    {
      if (objtype == OBJECT_COLUMN && colname)
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_GRANTED), errmsg("not all privileges were granted for column \"%s\" of relation \"%s\"", colname, objname)));
      }
      else
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_GRANTED), errmsg("not all privileges were granted for \"%s\"", objname)));
      }
    }
  }
  else
  {
    if (this_privileges == 0)
    {
      if (objtype == OBJECT_COLUMN && colname)
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_REVOKED), errmsg("no privileges could be revoked for column \"%s\" of relation \"%s\"", colname, objname)));
      }
      else
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_REVOKED), errmsg("no privileges could be revoked for \"%s\"", objname)));
      }
    }
    else if (!all_privs && this_privileges != privileges)
    {
      if (objtype == OBJECT_COLUMN && colname)
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_REVOKED), errmsg("not all privileges could be revoked for column \"%s\" of relation \"%s\"", colname, objname)));
      }
      else
      {
        ereport(WARNING, (errcode(ERRCODE_WARNING_PRIVILEGE_NOT_REVOKED), errmsg("not all privileges could be revoked for \"%s\"", objname)));
      }
    }
  }

  return this_privileges;
}

   
                                                           
   
void
ExecuteGrantStmt(GrantStmt *stmt)
{
  InternalGrant istmt;
  ListCell *cell;
  const char *errormsg;
  AclMode all_privileges;

     
                                                             
     
  istmt.is_grant = stmt->is_grant;
  istmt.objtype = stmt->objtype;

                                              
  switch (stmt->targtype)
  {
  case ACL_TARGET_OBJECT:
    istmt.objects = objectNamesToOids(stmt->objtype, stmt->objects);
    break;
  case ACL_TARGET_ALL_IN_SCHEMA:
    istmt.objects = objectsInSchemaToOids(stmt->objtype, stmt->objects);
    break;
                                                     
  default:
    elog(ERROR, "unrecognized GrantStmt.targtype: %d", (int)stmt->targtype);
  }

                                    
                                     
  istmt.col_privs = NIL;                           
  istmt.grantees = NIL;                    
  istmt.grant_option = stmt->grant_option;
  istmt.behavior = stmt->behavior;

     
                                                                             
                                                                         
                                                                         
     
  foreach (cell, stmt->grantees)
  {
    RoleSpec *grantee = (RoleSpec *)lfirst(cell);
    Oid grantee_uid;

    switch (grantee->roletype)
    {
    case ROLESPEC_PUBLIC:
      grantee_uid = ACL_ID_PUBLIC;
      break;
    default:
      grantee_uid = get_rolespec_oid(grantee, false);
      break;
    }
    istmt.grantees = lappend_oid(istmt.grantees, grantee_uid);
  }

     
                                                                           
                                                     
     
  switch (stmt->objtype)
  {
  case OBJECT_TABLE:

       
                                                                   
                                                                    
                        
       
    all_privileges = ACL_ALL_RIGHTS_RELATION | ACL_ALL_RIGHTS_SEQUENCE;
    errormsg = gettext_noop("invalid privilege type %s for relation");
    break;
  case OBJECT_SEQUENCE:
    all_privileges = ACL_ALL_RIGHTS_SEQUENCE;
    errormsg = gettext_noop("invalid privilege type %s for sequence");
    break;
  case OBJECT_DATABASE:
    all_privileges = ACL_ALL_RIGHTS_DATABASE;
    errormsg = gettext_noop("invalid privilege type %s for database");
    break;
  case OBJECT_DOMAIN:
    all_privileges = ACL_ALL_RIGHTS_TYPE;
    errormsg = gettext_noop("invalid privilege type %s for domain");
    break;
  case OBJECT_FUNCTION:
    all_privileges = ACL_ALL_RIGHTS_FUNCTION;
    errormsg = gettext_noop("invalid privilege type %s for function");
    break;
  case OBJECT_LANGUAGE:
    all_privileges = ACL_ALL_RIGHTS_LANGUAGE;
    errormsg = gettext_noop("invalid privilege type %s for language");
    break;
  case OBJECT_LARGEOBJECT:
    all_privileges = ACL_ALL_RIGHTS_LARGEOBJECT;
    errormsg = gettext_noop("invalid privilege type %s for large object");
    break;
  case OBJECT_SCHEMA:
    all_privileges = ACL_ALL_RIGHTS_SCHEMA;
    errormsg = gettext_noop("invalid privilege type %s for schema");
    break;
  case OBJECT_PROCEDURE:
    all_privileges = ACL_ALL_RIGHTS_FUNCTION;
    errormsg = gettext_noop("invalid privilege type %s for procedure");
    break;
  case OBJECT_ROUTINE:
    all_privileges = ACL_ALL_RIGHTS_FUNCTION;
    errormsg = gettext_noop("invalid privilege type %s for routine");
    break;
  case OBJECT_TABLESPACE:
    all_privileges = ACL_ALL_RIGHTS_TABLESPACE;
    errormsg = gettext_noop("invalid privilege type %s for tablespace");
    break;
  case OBJECT_TYPE:
    all_privileges = ACL_ALL_RIGHTS_TYPE;
    errormsg = gettext_noop("invalid privilege type %s for type");
    break;
  case OBJECT_FDW:
    all_privileges = ACL_ALL_RIGHTS_FDW;
    errormsg = gettext_noop("invalid privilege type %s for foreign-data wrapper");
    break;
  case OBJECT_FOREIGN_SERVER:
    all_privileges = ACL_ALL_RIGHTS_FOREIGN_SERVER;
    errormsg = gettext_noop("invalid privilege type %s for foreign server");
    break;
  default:
    elog(ERROR, "unrecognized GrantStmt.objtype: %d", (int)stmt->objtype);
                             
    all_privileges = ACL_NO_RIGHTS;
    errormsg = NULL;
  }

  if (stmt->privileges == NIL)
  {
    istmt.all_privs = true;

       
                                                                     
                                    
       
    istmt.privileges = ACL_NO_RIGHTS;
  }
  else
  {
    istmt.all_privs = false;
    istmt.privileges = ACL_NO_RIGHTS;

    foreach (cell, stmt->privileges)
    {
      AccessPriv *privnode = (AccessPriv *)lfirst(cell);
      AclMode priv;

         
                                                                       
                                                                   
         
      if (privnode->cols)
      {
        if (stmt->objtype != OBJECT_TABLE)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("column privileges are only valid for relations")));
        }
        istmt.col_privs = lappend(istmt.col_privs, privnode);
        continue;
      }

      if (privnode->priv_name == NULL)                      
      {
        elog(ERROR, "AccessPriv node must specify privilege or columns");
      }
      priv = string_to_privilege(privnode->priv_name);

      if (priv & ~((AclMode)all_privileges))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg(errormsg, privilege_to_string(priv))));
      }

      istmt.privileges |= priv;
    }
  }

  ExecGrantStmt_oids(&istmt);
}

   
                      
   
                                                              
   
static void
ExecGrantStmt_oids(InternalGrant *istmt)
{
  switch (istmt->objtype)
  {
  case OBJECT_TABLE:
  case OBJECT_SEQUENCE:
    ExecGrant_Relation(istmt);
    break;
  case OBJECT_DATABASE:
    ExecGrant_Database(istmt);
    break;
  case OBJECT_DOMAIN:
  case OBJECT_TYPE:
    ExecGrant_Type(istmt);
    break;
  case OBJECT_FDW:
    ExecGrant_Fdw(istmt);
    break;
  case OBJECT_FOREIGN_SERVER:
    ExecGrant_ForeignServer(istmt);
    break;
  case OBJECT_FUNCTION:
  case OBJECT_PROCEDURE:
  case OBJECT_ROUTINE:
    ExecGrant_Function(istmt);
    break;
  case OBJECT_LANGUAGE:
    ExecGrant_Language(istmt);
    break;
  case OBJECT_LARGEOBJECT:
    ExecGrant_Largeobject(istmt);
    break;
  case OBJECT_SCHEMA:
    ExecGrant_Namespace(istmt);
    break;
  case OBJECT_TABLESPACE:
    ExecGrant_Tablespace(istmt);
    break;
  default:
    elog(ERROR, "unrecognized GrantStmt.objtype: %d", (int)istmt->objtype);
  }

     
                                                                          
                                                                             
                                                                         
              
     
  if (EventTriggerSupportsObjectType(istmt->objtype))
  {
    EventTriggerCollectGrant(istmt);
  }
}

   
                     
   
                                                                 
   
                                                                          
                                                                            
                                                                           
            
   
static List *
objectNamesToOids(ObjectType objtype, List *objnames)
{
  List *objects = NIL;
  ListCell *cell;

  Assert(objnames != NIL);

  switch (objtype)
  {
  case OBJECT_TABLE:
  case OBJECT_SEQUENCE:
    foreach (cell, objnames)
    {
      RangeVar *relvar = (RangeVar *)lfirst(cell);
      Oid relOid;

      relOid = RangeVarGetRelid(relvar, NoLock, false);
      objects = lappend_oid(objects, relOid);
    }
    break;
  case OBJECT_DATABASE:
    foreach (cell, objnames)
    {
      char *dbname = strVal(lfirst(cell));
      Oid dbid;

      dbid = get_database_oid(dbname, false);
      objects = lappend_oid(objects, dbid);
    }
    break;
  case OBJECT_DOMAIN:
  case OBJECT_TYPE:
    foreach (cell, objnames)
    {
      List *typname = (List *)lfirst(cell);
      Oid oid;

      oid = typenameTypeId(NULL, makeTypeNameFromNameList(typname));
      objects = lappend_oid(objects, oid);
    }
    break;
  case OBJECT_FUNCTION:
    foreach (cell, objnames)
    {
      ObjectWithArgs *func = (ObjectWithArgs *)lfirst(cell);
      Oid funcid;

      funcid = LookupFuncWithArgs(OBJECT_FUNCTION, func, false);
      objects = lappend_oid(objects, funcid);
    }
    break;
  case OBJECT_LANGUAGE:
    foreach (cell, objnames)
    {
      char *langname = strVal(lfirst(cell));
      Oid oid;

      oid = get_language_oid(langname, false);
      objects = lappend_oid(objects, oid);
    }
    break;
  case OBJECT_LARGEOBJECT:
    foreach (cell, objnames)
    {
      Oid lobjOid = oidparse(lfirst(cell));

      if (!LargeObjectExists(lobjOid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("large object %u does not exist", lobjOid)));
      }

      objects = lappend_oid(objects, lobjOid);
    }
    break;
  case OBJECT_SCHEMA:
    foreach (cell, objnames)
    {
      char *nspname = strVal(lfirst(cell));
      Oid oid;

      oid = get_namespace_oid(nspname, false);
      objects = lappend_oid(objects, oid);
    }
    break;
  case OBJECT_PROCEDURE:
    foreach (cell, objnames)
    {
      ObjectWithArgs *func = (ObjectWithArgs *)lfirst(cell);
      Oid procid;

      procid = LookupFuncWithArgs(OBJECT_PROCEDURE, func, false);
      objects = lappend_oid(objects, procid);
    }
    break;
  case OBJECT_ROUTINE:
    foreach (cell, objnames)
    {
      ObjectWithArgs *func = (ObjectWithArgs *)lfirst(cell);
      Oid routid;

      routid = LookupFuncWithArgs(OBJECT_ROUTINE, func, false);
      objects = lappend_oid(objects, routid);
    }
    break;
  case OBJECT_TABLESPACE:
    foreach (cell, objnames)
    {
      char *spcname = strVal(lfirst(cell));
      Oid spcoid;

      spcoid = get_tablespace_oid(spcname, false);
      objects = lappend_oid(objects, spcoid);
    }
    break;
  case OBJECT_FDW:
    foreach (cell, objnames)
    {
      char *fdwname = strVal(lfirst(cell));
      Oid fdwid = get_foreign_data_wrapper_oid(fdwname, false);

      objects = lappend_oid(objects, fdwid);
    }
    break;
  case OBJECT_FOREIGN_SERVER:
    foreach (cell, objnames)
    {
      char *srvname = strVal(lfirst(cell));
      Oid srvid = get_foreign_server_oid(srvname, false);

      objects = lappend_oid(objects, srvid);
    }
    break;
  default:
    elog(ERROR, "unrecognized GrantStmt.objtype: %d", (int)objtype);
  }

  return objects;
}

   
                         
   
                                                                          
                                                                         
                                                         
   
static List *
objectsInSchemaToOids(ObjectType objtype, List *nspnames)
{
  List *objects = NIL;
  ListCell *cell;

  foreach (cell, nspnames)
  {
    char *nspname = strVal(lfirst(cell));
    Oid namespaceId;
    List *objs;

    namespaceId = LookupExplicitNamespace(nspname, false);

    switch (objtype)
    {
    case OBJECT_TABLE:
      objs = getRelationsInNamespace(namespaceId, RELKIND_RELATION);
      objects = list_concat(objects, objs);
      objs = getRelationsInNamespace(namespaceId, RELKIND_VIEW);
      objects = list_concat(objects, objs);
      objs = getRelationsInNamespace(namespaceId, RELKIND_MATVIEW);
      objects = list_concat(objects, objs);
      objs = getRelationsInNamespace(namespaceId, RELKIND_FOREIGN_TABLE);
      objects = list_concat(objects, objs);
      objs = getRelationsInNamespace(namespaceId, RELKIND_PARTITIONED_TABLE);
      objects = list_concat(objects, objs);
      break;
    case OBJECT_SEQUENCE:
      objs = getRelationsInNamespace(namespaceId, RELKIND_SEQUENCE);
      objects = list_concat(objects, objs);
      break;
    case OBJECT_FUNCTION:
    case OBJECT_PROCEDURE:
    case OBJECT_ROUTINE:
    {
      ScanKeyData key[2];
      int keycount;
      Relation rel;
      TableScanDesc scan;
      HeapTuple tuple;

      keycount = 0;
      ScanKeyInit(&key[keycount++], Anum_pg_proc_pronamespace, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(namespaceId));

      if (objtype == OBJECT_FUNCTION)
      {
                                                      
        ScanKeyInit(&key[keycount++], Anum_pg_proc_prokind, BTEqualStrategyNumber, F_CHARNE, CharGetDatum(PROKIND_PROCEDURE));
      }
      else if (objtype == OBJECT_PROCEDURE)
      {
        ScanKeyInit(&key[keycount++], Anum_pg_proc_prokind, BTEqualStrategyNumber, F_CHAREQ, CharGetDatum(PROKIND_PROCEDURE));
      }

      rel = table_open(ProcedureRelationId, AccessShareLock);
      scan = table_beginscan_catalog(rel, keycount, key);

      while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
      {
        Oid oid = ((Form_pg_proc)GETSTRUCT(tuple))->oid;

        objects = lappend_oid(objects, oid);
      }

      table_endscan(scan);
      table_close(rel, AccessShareLock);
    }
    break;
    default:
                             
      elog(ERROR, "unrecognized GrantStmt.objtype: %d", (int)objtype);
    }
  }

  return objects;
}

   
                           
   
                                                                             
   
static List *
getRelationsInNamespace(Oid namespaceId, char relkind)
{
  List *relations = NIL;
  ScanKeyData key[2];
  Relation rel;
  TableScanDesc scan;
  HeapTuple tuple;

  ScanKeyInit(&key[0], Anum_pg_class_relnamespace, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(namespaceId));
  ScanKeyInit(&key[1], Anum_pg_class_relkind, BTEqualStrategyNumber, F_CHAREQ, CharGetDatum(relkind));

  rel = table_open(RelationRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 2, key);

  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Oid oid = ((Form_pg_class)GETSTRUCT(tuple))->oid;

    relations = lappend_oid(relations, oid);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  return relations;
}

   
                                      
   
void
ExecAlterDefaultPrivilegesStmt(ParseState *pstate, AlterDefaultPrivilegesStmt *stmt)
{
  GrantStmt *action = stmt->action;
  InternalDefaultACL iacls;
  ListCell *cell;
  List *rolespecs = NIL;
  List *nspnames = NIL;
  DefElem *drolespecs = NULL;
  DefElem *dnspnames = NULL;
  AclMode all_privileges;
  const char *errormsg;

                                                       
  foreach (cell, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(cell);

    if (strcmp(defel->defname, "schemas") == 0)
    {
      if (dnspnames)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dnspnames = defel;
    }
    else if (strcmp(defel->defname, "roles") == 0)
    {
      if (drolespecs)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      drolespecs = defel;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

  if (dnspnames)
  {
    nspnames = (List *)dnspnames->arg;
  }
  if (drolespecs)
  {
    rolespecs = (List *)drolespecs->arg;
  }

                                                                      
                                 
                                                     
  iacls.is_grant = action->is_grant;
  iacls.objtype = action->objtype;
                                    
                                     
  iacls.grantees = NIL;                   
  iacls.grant_option = action->grant_option;
  iacls.behavior = action->behavior;

     
                                                                             
                                                                         
                                                                         
     
  foreach (cell, action->grantees)
  {
    RoleSpec *grantee = (RoleSpec *)lfirst(cell);
    Oid grantee_uid;

    switch (grantee->roletype)
    {
    case ROLESPEC_PUBLIC:
      grantee_uid = ACL_ID_PUBLIC;
      break;
    default:
      grantee_uid = get_rolespec_oid(grantee, false);
      break;
    }
    iacls.grantees = lappend_oid(iacls.grantees, grantee_uid);
  }

     
                                                                      
                      
     
  switch (action->objtype)
  {
  case OBJECT_TABLE:
    all_privileges = ACL_ALL_RIGHTS_RELATION;
    errormsg = gettext_noop("invalid privilege type %s for relation");
    break;
  case OBJECT_SEQUENCE:
    all_privileges = ACL_ALL_RIGHTS_SEQUENCE;
    errormsg = gettext_noop("invalid privilege type %s for sequence");
    break;
  case OBJECT_FUNCTION:
    all_privileges = ACL_ALL_RIGHTS_FUNCTION;
    errormsg = gettext_noop("invalid privilege type %s for function");
    break;
  case OBJECT_PROCEDURE:
    all_privileges = ACL_ALL_RIGHTS_FUNCTION;
    errormsg = gettext_noop("invalid privilege type %s for procedure");
    break;
  case OBJECT_ROUTINE:
    all_privileges = ACL_ALL_RIGHTS_FUNCTION;
    errormsg = gettext_noop("invalid privilege type %s for routine");
    break;
  case OBJECT_TYPE:
    all_privileges = ACL_ALL_RIGHTS_TYPE;
    errormsg = gettext_noop("invalid privilege type %s for type");
    break;
  case OBJECT_SCHEMA:
    all_privileges = ACL_ALL_RIGHTS_SCHEMA;
    errormsg = gettext_noop("invalid privilege type %s for schema");
    break;
  default:
    elog(ERROR, "unrecognized GrantStmt.objtype: %d", (int)action->objtype);
                             
    all_privileges = ACL_NO_RIGHTS;
    errormsg = NULL;
  }

  if (action->privileges == NIL)
  {
    iacls.all_privs = true;

       
                                                                     
                                    
       
    iacls.privileges = ACL_NO_RIGHTS;
  }
  else
  {
    iacls.all_privs = false;
    iacls.privileges = ACL_NO_RIGHTS;

    foreach (cell, action->privileges)
    {
      AccessPriv *privnode = (AccessPriv *)lfirst(cell);
      AclMode priv;

      if (privnode->cols)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("default privileges cannot be set for columns")));
      }

      if (privnode->priv_name == NULL)                      
      {
        elog(ERROR, "AccessPriv node must specify privilege");
      }
      priv = string_to_privilege(privnode->priv_name);

      if (priv & ~((AclMode)all_privileges))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg(errormsg, privilege_to_string(priv))));
      }

      iacls.privileges |= priv;
    }
  }

  if (rolespecs == NIL)
  {
                                    
    iacls.roleid = GetUserId();

    SetDefaultACLsInSchemas(&iacls, nspnames);
  }
  else
  {
                                                         
    ListCell *rolecell;

    foreach (rolecell, rolespecs)
    {
      RoleSpec *rolespec = lfirst(rolecell);

      iacls.roleid = get_rolespec_oid(rolespec, false);

         
                                                                         
                                                                        
                                                                       
                             
         
      check_is_member_of_role(GetUserId(), iacls.roleid);

      SetDefaultACLsInSchemas(&iacls, nspnames);
    }
  }
}

   
                                                                 
   
                                                         
   
static void
SetDefaultACLsInSchemas(InternalDefaultACL *iacls, List *nspnames)
{
  if (nspnames == NIL)
  {
                                                                  
    iacls->nspid = InvalidOid;

    SetDefaultACL(iacls);
  }
  else
  {
                                                                  
    ListCell *nspcell;

    foreach (nspcell, nspnames)
    {
      char *nspname = strVal(lfirst(nspcell));

      iacls->nspid = get_namespace_oid(nspname, false);

         
                                                                       
                                                                         
                                                                   
                                                                       
                                                              
                                                                  
                                                                        
                                                                       
                                       
         

      SetDefaultACL(iacls);
    }
  }
}

   
                                           
   
static void
SetDefaultACL(InternalDefaultACL *iacls)
{
  AclMode this_privileges = iacls->privileges;
  char objtype;
  Relation rel;
  HeapTuple tuple;
  bool isNew;
  Acl *def_acl;
  Acl *old_acl;
  Acl *new_acl;
  HeapTuple newtuple;
  Datum values[Natts_pg_default_acl];
  bool nulls[Natts_pg_default_acl];
  bool replaces[Natts_pg_default_acl];
  int noldmembers;
  int nnewmembers;
  Oid *oldmembers;
  Oid *newmembers;

  rel = table_open(DefaultAclRelationId, RowExclusiveLock);

     
                                                                          
                                                                             
                                                                         
                                          
     
  if (!OidIsValid(iacls->nspid))
  {
    def_acl = acldefault(iacls->objtype, iacls->roleid);
  }
  else
  {
    def_acl = make_empty_acl();
  }

     
                                                                      
                      
     
  switch (iacls->objtype)
  {
  case OBJECT_TABLE:
    objtype = DEFACLOBJ_RELATION;
    if (iacls->all_privs && this_privileges == ACL_NO_RIGHTS)
    {
      this_privileges = ACL_ALL_RIGHTS_RELATION;
    }
    break;

  case OBJECT_SEQUENCE:
    objtype = DEFACLOBJ_SEQUENCE;
    if (iacls->all_privs && this_privileges == ACL_NO_RIGHTS)
    {
      this_privileges = ACL_ALL_RIGHTS_SEQUENCE;
    }
    break;

  case OBJECT_FUNCTION:
    objtype = DEFACLOBJ_FUNCTION;
    if (iacls->all_privs && this_privileges == ACL_NO_RIGHTS)
    {
      this_privileges = ACL_ALL_RIGHTS_FUNCTION;
    }
    break;

  case OBJECT_TYPE:
    objtype = DEFACLOBJ_TYPE;
    if (iacls->all_privs && this_privileges == ACL_NO_RIGHTS)
    {
      this_privileges = ACL_ALL_RIGHTS_TYPE;
    }
    break;

  case OBJECT_SCHEMA:
    if (OidIsValid(iacls->nspid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("cannot use IN SCHEMA clause when using GRANT/REVOKE ON SCHEMAS")));
    }
    objtype = DEFACLOBJ_NAMESPACE;
    if (iacls->all_privs && this_privileges == ACL_NO_RIGHTS)
    {
      this_privileges = ACL_ALL_RIGHTS_SCHEMA;
    }
    break;

  default:
    elog(ERROR, "unrecognized objtype: %d", (int)iacls->objtype);
    objtype = 0;                          
    break;
  }

                                                               
  tuple = SearchSysCache3(DEFACLROLENSPOBJ, ObjectIdGetDatum(iacls->roleid), ObjectIdGetDatum(iacls->nspid), CharGetDatum(objtype));

  if (HeapTupleIsValid(tuple))
  {
    Datum aclDatum;
    bool isNull;

    aclDatum = SysCacheGetAttr(DEFACLROLENSPOBJ, tuple, Anum_pg_default_acl_defaclacl, &isNull);
    if (!isNull)
    {
      old_acl = DatumGetAclPCopy(aclDatum);
    }
    else
    {
      old_acl = NULL;                                           
    }
    isNew = false;
  }
  else
  {
    old_acl = NULL;
    isNew = true;
  }

  if (old_acl != NULL)
  {
       
                                                                          
                                                           
                                                 
       
    noldmembers = aclmembers(old_acl, &oldmembers);
  }
  else
  {
                                                               
    old_acl = aclcopy(def_acl);
                                                                 
    noldmembers = 0;
    oldmembers = NULL;
  }

     
                                                                           
           
     
  new_acl = merge_acl_with_grant(old_acl, iacls->is_grant, iacls->grant_option, iacls->behavior, iacls->grantees, this_privileges, iacls->roleid, iacls->roleid);

     
                                                                       
                                                                           
                                                            
     
  aclitemsort(new_acl);
  aclitemsort(def_acl);
  if (aclequal(new_acl, def_acl))
  {
                                                  
    if (!isNew)
    {
      ObjectAddress myself;

         
                                                                 
                                                                    
                                                              
         
      myself.classId = DefaultAclRelationId;
      myself.objectId = ((Form_pg_default_acl)GETSTRUCT(tuple))->oid;
      myself.objectSubId = 0;

      performDeletion(&myself, DROP_RESTRICT, 0);
    }
  }
  else
  {
    Oid defAclOid;

                                                          
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    if (isNew)
    {
                            
      defAclOid = GetNewOidWithIndex(rel, DefaultAclOidIndexId, Anum_pg_default_acl_oid);
      values[Anum_pg_default_acl_oid - 1] = ObjectIdGetDatum(defAclOid);
      values[Anum_pg_default_acl_defaclrole - 1] = ObjectIdGetDatum(iacls->roleid);
      values[Anum_pg_default_acl_defaclnamespace - 1] = ObjectIdGetDatum(iacls->nspid);
      values[Anum_pg_default_acl_defaclobjtype - 1] = CharGetDatum(objtype);
      values[Anum_pg_default_acl_defaclacl - 1] = PointerGetDatum(new_acl);

      newtuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);
      CatalogTupleInsert(rel, newtuple);
    }
    else
    {
      defAclOid = ((Form_pg_default_acl)GETSTRUCT(tuple))->oid;

                                 
      values[Anum_pg_default_acl_defaclacl - 1] = PointerGetDatum(new_acl);
      replaces[Anum_pg_default_acl_defaclacl - 1] = true;

      newtuple = heap_modify_tuple(tuple, RelationGetDescr(rel), values, nulls, replaces);
      CatalogTupleUpdate(rel, &newtuple->t_self, newtuple);
    }

                                                      
    if (isNew)
    {
                              
      recordDependencyOnOwner(DefaultAclRelationId, defAclOid, iacls->roleid);

                                   
      if (OidIsValid(iacls->nspid))
      {
        ObjectAddress myself, referenced;

        myself.classId = DefaultAclRelationId;
        myself.objectId = defAclOid;
        myself.objectSubId = 0;

        referenced.classId = NamespaceRelationId;
        referenced.objectId = iacls->nspid;
        referenced.objectSubId = 0;

        recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);
      }
    }

       
                                             
       
    nnewmembers = aclmembers(new_acl, &newmembers);

    updateAclDependencies(DefaultAclRelationId, defAclOid, 0, iacls->roleid, noldmembers, oldmembers, nnewmembers, newmembers);

    if (isNew)
    {
      InvokeObjectPostCreateHook(DefaultAclRelationId, defAclOid, 0);
    }
    else
    {
      InvokeObjectPostAlterHook(DefaultAclRelationId, defAclOid, 0);
    }
  }

  if (HeapTupleIsValid(tuple))
  {
    ReleaseSysCache(tuple);
  }

  table_close(rel, RowExclusiveLock);

                                                       
  CommandCounterIncrement();
}

   
                           
   
                                                               
   
void
RemoveRoleFromObjectACL(Oid roleid, Oid classid, Oid objid)
{
  if (classid == DefaultAclRelationId)
  {
    InternalDefaultACL iacls;
    Form_pg_default_acl pg_default_acl_tuple;
    Relation rel;
    ScanKeyData skey[1];
    SysScanDesc scan;
    HeapTuple tuple;

                                                  
    rel = table_open(DefaultAclRelationId, AccessShareLock);

    ScanKeyInit(&skey[0], Anum_pg_default_acl_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objid));

    scan = systable_beginscan(rel, DefaultAclOidIndexId, true, NULL, 1, skey);

    tuple = systable_getnext(scan);

    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "could not find tuple for default ACL %u", objid);
    }

    pg_default_acl_tuple = (Form_pg_default_acl)GETSTRUCT(tuple);

    iacls.roleid = pg_default_acl_tuple->defaclrole;
    iacls.nspid = pg_default_acl_tuple->defaclnamespace;

    switch (pg_default_acl_tuple->defaclobjtype)
    {
    case DEFACLOBJ_RELATION:
      iacls.objtype = OBJECT_TABLE;
      break;
    case DEFACLOBJ_SEQUENCE:
      iacls.objtype = OBJECT_SEQUENCE;
      break;
    case DEFACLOBJ_FUNCTION:
      iacls.objtype = OBJECT_FUNCTION;
      break;
    case DEFACLOBJ_TYPE:
      iacls.objtype = OBJECT_TYPE;
      break;
    case DEFACLOBJ_NAMESPACE:
      iacls.objtype = OBJECT_SCHEMA;
      break;
    default:
                              
      elog(ERROR, "unexpected default ACL type: %d", (int)pg_default_acl_tuple->defaclobjtype);
      break;
    }

    systable_endscan(scan);
    table_close(rel, AccessShareLock);

    iacls.is_grant = false;
    iacls.all_privs = true;
    iacls.privileges = ACL_NO_RIGHTS;
    iacls.grantees = list_make1_oid(roleid);
    iacls.grant_option = false;
    iacls.behavior = DROP_CASCADE;

               
    SetDefaultACL(&iacls);
  }
  else
  {
    InternalGrant istmt;

    switch (classid)
    {
    case RelationRelationId:
                                               
      istmt.objtype = OBJECT_TABLE;
      break;
    case DatabaseRelationId:
      istmt.objtype = OBJECT_DATABASE;
      break;
    case TypeRelationId:
      istmt.objtype = OBJECT_TYPE;
      break;
    case ProcedureRelationId:
      istmt.objtype = OBJECT_ROUTINE;
      break;
    case LanguageRelationId:
      istmt.objtype = OBJECT_LANGUAGE;
      break;
    case LargeObjectRelationId:
      istmt.objtype = OBJECT_LARGEOBJECT;
      break;
    case NamespaceRelationId:
      istmt.objtype = OBJECT_SCHEMA;
      break;
    case TableSpaceRelationId:
      istmt.objtype = OBJECT_TABLESPACE;
      break;
    case ForeignServerRelationId:
      istmt.objtype = OBJECT_FOREIGN_SERVER;
      break;
    case ForeignDataWrapperRelationId:
      istmt.objtype = OBJECT_FDW;
      break;
    default:
      elog(ERROR, "unexpected object class %u", classid);
      break;
    }
    istmt.is_grant = false;
    istmt.objects = list_make1_oid(objid);
    istmt.all_privs = true;
    istmt.privileges = ACL_NO_RIGHTS;
    istmt.col_privs = NIL;
    istmt.grantees = list_make1_oid(roleid);
    istmt.grant_option = false;
    istmt.behavior = DROP_CASCADE;

    ExecGrantStmt_oids(&istmt);
  }
}

   
                                 
   
void
RemoveDefaultACLById(Oid defaclOid)
{
  Relation rel;
  ScanKeyData skey[1];
  SysScanDesc scan;
  HeapTuple tuple;

  rel = table_open(DefaultAclRelationId, RowExclusiveLock);

  ScanKeyInit(&skey[0], Anum_pg_default_acl_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(defaclOid));

  scan = systable_beginscan(rel, DefaultAclOidIndexId, true, NULL, 1, skey);

  tuple = systable_getnext(scan);

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for default ACL %u", defaclOid);
  }

  CatalogTupleDelete(rel, &tuple->t_self);

  systable_endscan(scan);
  table_close(rel, RowExclusiveLock);
}

   
                         
   
                                                                        
                                                                     
                                                                        
   
static void
expand_col_privileges(List *colnames, Oid table_oid, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges)
{
  ListCell *cell;

  foreach (cell, colnames)
  {
    char *colname = strVal(lfirst(cell));
    AttrNumber attnum;

    attnum = get_attnum(table_oid, colname);
    if (attnum == InvalidAttrNumber)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colname, get_rel_name(table_oid))));
    }
    attnum -= FirstLowInvalidHeapAttributeNumber;
    if (attnum <= 0 || attnum >= num_col_privileges)
    {
      elog(ERROR, "column number out of range");                   
    }
    col_privileges[attnum] |= this_privileges;
  }
}

   
                             
   
                                                                              
                                                                         
                                                                        
   
static void
expand_all_col_privileges(Oid table_oid, Form_pg_class classForm, AclMode this_privileges, AclMode *col_privileges, int num_col_privileges)
{
  AttrNumber curr_att;

  Assert(classForm->relnatts - FirstLowInvalidHeapAttributeNumber < num_col_privileges);
  for (curr_att = FirstLowInvalidHeapAttributeNumber + 1; curr_att <= classForm->relnatts; curr_att++)
  {
    HeapTuple attTuple;
    bool isdropped;

    if (curr_att == InvalidAttrNumber)
    {
      continue;
    }

                                                    
    if (classForm->relkind == RELKIND_VIEW && curr_att < 0)
    {
      continue;
    }

    attTuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(table_oid), Int16GetDatum(curr_att));
    if (!HeapTupleIsValid(attTuple))
    {
      elog(ERROR, "cache lookup failed for attribute %d of relation %u", curr_att, table_oid);
    }

    isdropped = ((Form_pg_attribute)GETSTRUCT(attTuple))->attisdropped;

    ReleaseSysCache(attTuple);

                                
    if (isdropped)
    {
      continue;
    }

    col_privileges[curr_att - FirstLowInvalidHeapAttributeNumber] |= this_privileges;
  }
}

   
                                                            
                                                        
   
static void
ExecGrant_Attribute(InternalGrant *istmt, Oid relOid, const char *relname, AttrNumber attnum, Oid ownerId, AclMode col_privileges, Relation attRelation, const Acl *old_rel_acl)
{
  HeapTuple attr_tuple;
  Form_pg_attribute pg_attribute_tuple;
  Acl *old_acl;
  Acl *new_acl;
  Acl *merged_acl;
  Datum aclDatum;
  bool isNull;
  Oid grantorId;
  AclMode avail_goptions;
  bool need_update;
  HeapTuple newtuple;
  Datum values[Natts_pg_attribute];
  bool nulls[Natts_pg_attribute];
  bool replaces[Natts_pg_attribute];
  int noldmembers;
  int nnewmembers;
  Oid *oldmembers;
  Oid *newmembers;

  attr_tuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relOid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(attr_tuple))
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relOid);
  }
  pg_attribute_tuple = (Form_pg_attribute)GETSTRUCT(attr_tuple);

     
                                                                         
                     
     
  aclDatum = SysCacheGetAttr(ATTNUM, attr_tuple, Anum_pg_attribute_attacl, &isNull);
  if (isNull)
  {
    old_acl = acldefault(OBJECT_COLUMN, ownerId);
                                                                 
    noldmembers = 0;
    oldmembers = NULL;
  }
  else
  {
    old_acl = DatumGetAclPCopy(aclDatum);
                                                     
    noldmembers = aclmembers(old_acl, &oldmembers);
  }

     
                                                                             
                                                                   
                                                                             
                                                                
     
  merged_acl = aclconcat(old_rel_acl, old_acl);

                                                                    
  select_best_grantor(GetUserId(), col_privileges, merged_acl, ownerId, &grantorId, &avail_goptions);

  pfree(merged_acl);

     
                                                                         
                                                                          
                                                                           
                                                                     
                                                                             
                                                           
     
  col_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, (col_privileges == ACL_ALL_RIGHTS_COLUMN), col_privileges, relOid, grantorId, OBJECT_COLUMN, relname, attnum, NameStr(pg_attribute_tuple->attname));

     
                       
     
  new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, col_privileges, grantorId, ownerId);

     
                                                                        
                                    
     
  nnewmembers = aclmembers(new_acl, &newmembers);

                                                      
  MemSet(values, 0, sizeof(values));
  MemSet(nulls, false, sizeof(nulls));
  MemSet(replaces, false, sizeof(replaces));

     
                                                                            
                                                                             
                                                                           
                                                                            
                                                   
     
  if (ACL_NUM(new_acl) > 0)
  {
    values[Anum_pg_attribute_attacl - 1] = PointerGetDatum(new_acl);
    need_update = true;
  }
  else
  {
    nulls[Anum_pg_attribute_attacl - 1] = true;
    need_update = !isNull;
  }
  replaces[Anum_pg_attribute_attacl - 1] = true;

  if (need_update)
  {
    newtuple = heap_modify_tuple(attr_tuple, RelationGetDescr(attRelation), values, nulls, replaces);

    CatalogTupleUpdate(attRelation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(relOid, RelationRelationId, attnum, ACL_NUM(new_acl) > 0 ? new_acl : NULL);

                                               
    updateAclDependencies(RelationRelationId, relOid, attnum, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);
  }

  pfree(new_acl);

  ReleaseSysCache(attr_tuple);
}

   
                                                    
   
static void
ExecGrant_Relation(InternalGrant *istmt)
{
  Relation relation;
  Relation attRelation;
  ListCell *cell;

  relation = table_open(RelationRelationId, RowExclusiveLock);
  attRelation = table_open(AttributeRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid relOid = lfirst_oid(cell);
    Datum aclDatum;
    Form_pg_class pg_class_tuple;
    bool isNull;
    AclMode this_privileges;
    AclMode *col_privileges;
    int num_col_privileges;
    bool have_col_privileges;
    Acl *old_acl;
    Acl *old_rel_acl;
    int noldmembers;
    Oid *oldmembers;
    Oid ownerId;
    HeapTuple tuple;
    ListCell *cell_colprivs;

    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relOid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", relOid);
    }
    pg_class_tuple = (Form_pg_class)GETSTRUCT(tuple);

                                           
    if (pg_class_tuple->relkind == RELKIND_INDEX || pg_class_tuple->relkind == RELKIND_PARTITIONED_INDEX)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an index", NameStr(pg_class_tuple->relname))));
    }

                                              
    if (pg_class_tuple->relkind == RELKIND_COMPOSITE_TYPE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a composite type", NameStr(pg_class_tuple->relname))));
    }

                                                
    if (istmt->objtype == OBJECT_SEQUENCE && pg_class_tuple->relkind != RELKIND_SEQUENCE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", NameStr(pg_class_tuple->relname))));
    }

                                                             
    if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
    {
      if (pg_class_tuple->relkind == RELKIND_SEQUENCE)
      {
        this_privileges = ACL_ALL_RIGHTS_SEQUENCE;
      }
      else
      {
        this_privileges = ACL_ALL_RIGHTS_RELATION;
      }
    }
    else
    {
      this_privileges = istmt->privileges;
    }

       
                                                                           
                                                                    
                                                                           
                
       
    if (istmt->objtype == OBJECT_TABLE)
    {
      if (pg_class_tuple->relkind == RELKIND_SEQUENCE)
      {
           
                                                                
                                                                    
                         
           
        if (this_privileges & ~((AclMode)ACL_ALL_RIGHTS_SEQUENCE))
        {
             
                                                                    
                                                                   
                                                     
             
          ereport(WARNING, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("sequence \"%s\" only supports USAGE, SELECT, and UPDATE privileges", NameStr(pg_class_tuple->relname))));
          this_privileges &= (AclMode)ACL_ALL_RIGHTS_SEQUENCE;
        }
      }
      else
      {
        if (this_privileges & ~((AclMode)ACL_ALL_RIGHTS_RELATION))
        {
             
                                                                     
                                                                  
                                                                
                    
             
          ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("invalid privilege type %s for table", "USAGE")));
        }
      }
    }

       
                                                                        
                                                                         
                                                          
       
    num_col_privileges = pg_class_tuple->relnatts - FirstLowInvalidHeapAttributeNumber + 1;
    col_privileges = (AclMode *)palloc0(num_col_privileges * sizeof(AclMode));
    have_col_privileges = false;

       
                                                                   
                                                                        
                                                                         
                                                                        
                                                 
       
    if (!istmt->is_grant && (this_privileges & ACL_ALL_RIGHTS_COLUMN) != 0)
    {
      expand_all_col_privileges(relOid, pg_class_tuple, this_privileges & ACL_ALL_RIGHTS_COLUMN, col_privileges, num_col_privileges);
      have_col_privileges = true;
    }

       
                                                                         
                                      
       
    ownerId = pg_class_tuple->relowner;
    aclDatum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relacl, &isNull);
    if (isNull)
    {
      switch (pg_class_tuple->relkind)
      {
      case RELKIND_SEQUENCE:
        old_acl = acldefault(OBJECT_SEQUENCE, ownerId);
        break;
      default:
        old_acl = acldefault(OBJECT_TABLE, ownerId);
        break;
      }
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                    
    old_rel_acl = aclcopy(old_acl);

       
                                                               
       
    if (this_privileges != ACL_NO_RIGHTS)
    {
      AclMode avail_goptions;
      Acl *new_acl;
      Oid grantorId;
      HeapTuple newtuple;
      Datum values[Natts_pg_class];
      bool nulls[Natts_pg_class];
      bool replaces[Natts_pg_class];
      int nnewmembers;
      Oid *newmembers;
      ObjectType objtype;

                                                                        
      select_best_grantor(GetUserId(), this_privileges, old_acl, ownerId, &grantorId, &avail_goptions);

      switch (pg_class_tuple->relkind)
      {
      case RELKIND_SEQUENCE:
        objtype = OBJECT_SEQUENCE;
        break;
      default:
        objtype = OBJECT_TABLE;
        break;
      }

         
                                                                         
                                                            
         
      this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, this_privileges, relOid, grantorId, objtype, NameStr(pg_class_tuple->relname), 0, NULL);

         
                           
         
      new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

         
                                                                        
                                            
         
      nnewmembers = aclmembers(new_acl, &newmembers);

                                                          
      MemSet(values, 0, sizeof(values));
      MemSet(nulls, false, sizeof(nulls));
      MemSet(replaces, false, sizeof(replaces));

      replaces[Anum_pg_class_relacl - 1] = true;
      values[Anum_pg_class_relacl - 1] = PointerGetDatum(new_acl);

      newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

      CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                    
      recordExtensionInitPriv(relOid, RelationRelationId, 0, new_acl);

                                                 
      updateAclDependencies(RelationRelationId, relOid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

      pfree(new_acl);
    }

       
                                                                         
                                                                     
                                                                
       
    foreach (cell_colprivs, istmt->col_privs)
    {
      AccessPriv *col_privs = (AccessPriv *)lfirst(cell_colprivs);

      if (col_privs->priv_name == NULL)
      {
        this_privileges = ACL_ALL_RIGHTS_COLUMN;
      }
      else
      {
        this_privileges = string_to_privilege(col_privs->priv_name);
      }

      if (this_privileges & ~((AclMode)ACL_ALL_RIGHTS_COLUMN))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("invalid privilege type %s for column", privilege_to_string(this_privileges))));
      }

      if (pg_class_tuple->relkind == RELKIND_SEQUENCE && this_privileges & ~((AclMode)ACL_SELECT))
      {
           
                                                                     
                                                                     
                                      
           
        ereport(WARNING, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("sequence \"%s\" only supports SELECT column privileges", NameStr(pg_class_tuple->relname))));

        this_privileges &= (AclMode)ACL_SELECT;
      }

      expand_col_privileges(col_privs->cols, relOid, this_privileges, col_privileges, num_col_privileges);
      have_col_privileges = true;
    }

    if (have_col_privileges)
    {
      AttrNumber i;

      for (i = 0; i < num_col_privileges; i++)
      {
        if (col_privileges[i] == ACL_NO_RIGHTS)
        {
          continue;
        }
        ExecGrant_Attribute(istmt, relOid, NameStr(pg_class_tuple->relname), i + FirstLowInvalidHeapAttributeNumber, ownerId, col_privileges[i], attRelation, old_rel_acl);
      }
    }

    pfree(old_rel_acl);
    pfree(col_privileges);

    ReleaseSysCache(tuple);

                                                         
    CommandCounterIncrement();
  }

  table_close(attRelation, RowExclusiveLock);
  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Database(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_DATABASE;
  }

  relation = table_open(DatabaseRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid datId = lfirst_oid(cell);
    Form_pg_database pg_database_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple newtuple;
    Datum values[Natts_pg_database];
    bool nulls[Natts_pg_database];
    bool replaces[Natts_pg_database];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;
    HeapTuple tuple;

    tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(datId));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for database %u", datId);
    }

    pg_database_tuple = (Form_pg_database)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = pg_database_tuple->datdba;
    aclDatum = heap_getattr(tuple, Anum_pg_database_datacl, RelationGetDescr(relation), &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_DATABASE, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, datId, grantorId, OBJECT_DATABASE, NameStr(pg_database_tuple->datname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_database_datacl - 1] = true;
    values[Anum_pg_database_datacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                               
    updateAclDependencies(DatabaseRelationId, pg_database_tuple->oid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Fdw(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_FDW;
  }

  relation = table_open(ForeignDataWrapperRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid fdwid = lfirst_oid(cell);
    Form_pg_foreign_data_wrapper pg_fdw_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple tuple;
    HeapTuple newtuple;
    Datum values[Natts_pg_foreign_data_wrapper];
    bool nulls[Natts_pg_foreign_data_wrapper];
    bool replaces[Natts_pg_foreign_data_wrapper];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;

    tuple = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for foreign-data wrapper %u", fdwid);
    }

    pg_fdw_tuple = (Form_pg_foreign_data_wrapper)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = pg_fdw_tuple->fdwowner;
    aclDatum = SysCacheGetAttr(FOREIGNDATAWRAPPEROID, tuple, Anum_pg_foreign_data_wrapper_fdwacl, &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_FDW, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, fdwid, grantorId, OBJECT_FDW, NameStr(pg_fdw_tuple->fdwname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_foreign_data_wrapper_fdwacl - 1] = true;
    values[Anum_pg_foreign_data_wrapper_fdwacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(fdwid, ForeignDataWrapperRelationId, 0, new_acl);

                                               
    updateAclDependencies(ForeignDataWrapperRelationId, pg_fdw_tuple->oid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_ForeignServer(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_FOREIGN_SERVER;
  }

  relation = table_open(ForeignServerRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid srvid = lfirst_oid(cell);
    Form_pg_foreign_server pg_server_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple tuple;
    HeapTuple newtuple;
    Datum values[Natts_pg_foreign_server];
    bool nulls[Natts_pg_foreign_server];
    bool replaces[Natts_pg_foreign_server];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;

    tuple = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(srvid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for foreign server %u", srvid);
    }

    pg_server_tuple = (Form_pg_foreign_server)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = pg_server_tuple->srvowner;
    aclDatum = SysCacheGetAttr(FOREIGNSERVEROID, tuple, Anum_pg_foreign_server_srvacl, &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_FOREIGN_SERVER, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, srvid, grantorId, OBJECT_FOREIGN_SERVER, NameStr(pg_server_tuple->srvname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_foreign_server_srvacl - 1] = true;
    values[Anum_pg_foreign_server_srvacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(srvid, ForeignServerRelationId, 0, new_acl);

                                               
    updateAclDependencies(ForeignServerRelationId, pg_server_tuple->oid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Function(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_FUNCTION;
  }

  relation = table_open(ProcedureRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid funcId = lfirst_oid(cell);
    Form_pg_proc pg_proc_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple tuple;
    HeapTuple newtuple;
    Datum values[Natts_pg_proc];
    bool nulls[Natts_pg_proc];
    bool replaces[Natts_pg_proc];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;

    tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcId));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", funcId);
    }

    pg_proc_tuple = (Form_pg_proc)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = pg_proc_tuple->proowner;
    aclDatum = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_proacl, &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_FUNCTION, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, funcId, grantorId, OBJECT_FUNCTION, NameStr(pg_proc_tuple->proname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_proc_proacl - 1] = true;
    values[Anum_pg_proc_proacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(funcId, ProcedureRelationId, 0, new_acl);

                                               
    updateAclDependencies(ProcedureRelationId, funcId, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Language(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_LANGUAGE;
  }

  relation = table_open(LanguageRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid langId = lfirst_oid(cell);
    Form_pg_language pg_language_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple tuple;
    HeapTuple newtuple;
    Datum values[Natts_pg_language];
    bool nulls[Natts_pg_language];
    bool replaces[Natts_pg_language];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;

    tuple = SearchSysCache1(LANGOID, ObjectIdGetDatum(langId));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for language %u", langId);
    }

    pg_language_tuple = (Form_pg_language)GETSTRUCT(tuple);

    if (!pg_language_tuple->lanpltrusted)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("language \"%s\" is not trusted", NameStr(pg_language_tuple->lanname)),
                         errdetail("GRANT and REVOKE are not allowed on untrusted languages, "
                                   "because only superusers can use untrusted languages.")));
    }

       
                                                                         
                                      
       
    ownerId = pg_language_tuple->lanowner;
    aclDatum = SysCacheGetAttr(LANGNAME, tuple, Anum_pg_language_lanacl, &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_LANGUAGE, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, langId, grantorId, OBJECT_LANGUAGE, NameStr(pg_language_tuple->lanname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_language_lanacl - 1] = true;
    values[Anum_pg_language_lanacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(langId, LanguageRelationId, 0, new_acl);

                                               
    updateAclDependencies(LanguageRelationId, pg_language_tuple->oid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Largeobject(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_LARGEOBJECT;
  }

  relation = table_open(LargeObjectMetadataRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid loid = lfirst_oid(cell);
    Form_pg_largeobject_metadata form_lo_meta;
    char loname[NAMEDATALEN];
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple newtuple;
    Datum values[Natts_pg_largeobject_metadata];
    bool nulls[Natts_pg_largeobject_metadata];
    bool replaces[Natts_pg_largeobject_metadata];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;
    ScanKeyData entry[1];
    SysScanDesc scan;
    HeapTuple tuple;

                                                         
    ScanKeyInit(&entry[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(loid));

    scan = systable_beginscan(relation, LargeObjectMetadataOidIndexId, true, NULL, 1, entry);

    tuple = systable_getnext(scan);
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "could not find tuple for large object %u", loid);
    }

    form_lo_meta = (Form_pg_largeobject_metadata)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = form_lo_meta->lomowner;
    aclDatum = heap_getattr(tuple, Anum_pg_largeobject_metadata_lomacl, RelationGetDescr(relation), &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_LARGEOBJECT, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    snprintf(loname, sizeof(loname), "large object %u", loid);
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, loid, grantorId, OBJECT_LARGEOBJECT, loname, 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_largeobject_metadata_lomacl - 1] = true;
    values[Anum_pg_largeobject_metadata_lomacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(loid, LargeObjectRelationId, 0, new_acl);

                                               
    updateAclDependencies(LargeObjectRelationId, form_lo_meta->oid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    systable_endscan(scan);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Namespace(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_SCHEMA;
  }

  relation = table_open(NamespaceRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid nspid = lfirst_oid(cell);
    Form_pg_namespace pg_namespace_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple tuple;
    HeapTuple newtuple;
    Datum values[Natts_pg_namespace];
    bool nulls[Natts_pg_namespace];
    bool replaces[Natts_pg_namespace];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;

    tuple = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(nspid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for namespace %u", nspid);
    }

    pg_namespace_tuple = (Form_pg_namespace)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = pg_namespace_tuple->nspowner;
    aclDatum = SysCacheGetAttr(NAMESPACENAME, tuple, Anum_pg_namespace_nspacl, &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_SCHEMA, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, nspid, grantorId, OBJECT_SCHEMA, NameStr(pg_namespace_tuple->nspname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_namespace_nspacl - 1] = true;
    values[Anum_pg_namespace_nspacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(nspid, NamespaceRelationId, 0, new_acl);

                                               
    updateAclDependencies(NamespaceRelationId, pg_namespace_tuple->oid, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);

    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Tablespace(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_TABLESPACE;
  }

  relation = table_open(TableSpaceRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid tblId = lfirst_oid(cell);
    Form_pg_tablespace pg_tablespace_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple newtuple;
    Datum values[Natts_pg_tablespace];
    bool nulls[Natts_pg_tablespace];
    bool replaces[Natts_pg_tablespace];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;
    HeapTuple tuple;

                                           
    tuple = SearchSysCache1(TABLESPACEOID, ObjectIdGetDatum(tblId));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for tablespace %u", tblId);
    }

    pg_tablespace_tuple = (Form_pg_tablespace)GETSTRUCT(tuple);

       
                                                                         
                                      
       
    ownerId = pg_tablespace_tuple->spcowner;
    aclDatum = heap_getattr(tuple, Anum_pg_tablespace_spcacl, RelationGetDescr(relation), &isNull);
    if (isNull)
    {
      old_acl = acldefault(OBJECT_TABLESPACE, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, tblId, grantorId, OBJECT_TABLESPACE, NameStr(pg_tablespace_tuple->spcname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_tablespace_spcacl - 1] = true;
    values[Anum_pg_tablespace_spcacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                               
    updateAclDependencies(TableSpaceRelationId, tblId, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);
    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Type(InternalGrant *istmt)
{
  Relation relation;
  ListCell *cell;

  if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
  {
    istmt->privileges = ACL_ALL_RIGHTS_TYPE;
  }

  relation = table_open(TypeRelationId, RowExclusiveLock);

  foreach (cell, istmt->objects)
  {
    Oid typId = lfirst_oid(cell);
    Form_pg_type pg_type_tuple;
    Datum aclDatum;
    bool isNull;
    AclMode avail_goptions;
    AclMode this_privileges;
    Acl *old_acl;
    Acl *new_acl;
    Oid grantorId;
    Oid ownerId;
    HeapTuple newtuple;
    Datum values[Natts_pg_type];
    bool nulls[Natts_pg_type];
    bool replaces[Natts_pg_type];
    int noldmembers;
    int nnewmembers;
    Oid *oldmembers;
    Oid *newmembers;
    HeapTuple tuple;

                                     
    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typId));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for type %u", typId);
    }

    pg_type_tuple = (Form_pg_type)GETSTRUCT(tuple);

    if (pg_type_tuple->typelem != 0 && pg_type_tuple->typlen == -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("cannot set privileges of array types"), errhint("Set the privileges of the element type instead.")));
    }

                                            
    if (istmt->objtype == OBJECT_DOMAIN && pg_type_tuple->typtype != TYPTYPE_DOMAIN)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a domain", NameStr(pg_type_tuple->typname))));
    }

       
                                                                         
                                      
       
    ownerId = pg_type_tuple->typowner;
    aclDatum = heap_getattr(tuple, Anum_pg_type_typacl, RelationGetDescr(relation), &isNull);
    if (isNull)
    {
      old_acl = acldefault(istmt->objtype, ownerId);
                                                                   
      noldmembers = 0;
      oldmembers = NULL;
    }
    else
    {
      old_acl = DatumGetAclPCopy(aclDatum);
                                                       
      noldmembers = aclmembers(old_acl, &oldmembers);
    }

                                                                      
    select_best_grantor(GetUserId(), istmt->privileges, old_acl, ownerId, &grantorId, &avail_goptions);

       
                                                                           
                                                      
       
    this_privileges = restrict_and_check_grant(istmt->is_grant, avail_goptions, istmt->all_privs, istmt->privileges, typId, grantorId, OBJECT_TYPE, NameStr(pg_type_tuple->typname), 0, NULL);

       
                         
       
    new_acl = merge_acl_with_grant(old_acl, istmt->is_grant, istmt->grant_option, istmt->behavior, istmt->grantees, this_privileges, grantorId, ownerId);

       
                                                                          
                                      
       
    nnewmembers = aclmembers(new_acl, &newmembers);

                                                        
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));
    MemSet(replaces, false, sizeof(replaces));

    replaces[Anum_pg_type_typacl - 1] = true;
    values[Anum_pg_type_typacl - 1] = PointerGetDatum(new_acl);

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values, nulls, replaces);

    CatalogTupleUpdate(relation, &newtuple->t_self, newtuple);

                                                  
    recordExtensionInitPriv(typId, TypeRelationId, 0, new_acl);

                                               
    updateAclDependencies(TypeRelationId, typId, 0, ownerId, noldmembers, oldmembers, nnewmembers, newmembers);

    ReleaseSysCache(tuple);
    pfree(new_acl);

                                                         
    CommandCounterIncrement();
  }

  table_close(relation, RowExclusiveLock);
}

static AclMode
string_to_privilege(const char *privname)
{
  if (strcmp(privname, "insert") == 0)
  {
    return ACL_INSERT;
  }
  if (strcmp(privname, "select") == 0)
  {
    return ACL_SELECT;
  }
  if (strcmp(privname, "update") == 0)
  {
    return ACL_UPDATE;
  }
  if (strcmp(privname, "delete") == 0)
  {
    return ACL_DELETE;
  }
  if (strcmp(privname, "truncate") == 0)
  {
    return ACL_TRUNCATE;
  }
  if (strcmp(privname, "references") == 0)
  {
    return ACL_REFERENCES;
  }
  if (strcmp(privname, "trigger") == 0)
  {
    return ACL_TRIGGER;
  }
  if (strcmp(privname, "execute") == 0)
  {
    return ACL_EXECUTE;
  }
  if (strcmp(privname, "usage") == 0)
  {
    return ACL_USAGE;
  }
  if (strcmp(privname, "create") == 0)
  {
    return ACL_CREATE;
  }
  if (strcmp(privname, "temporary") == 0)
  {
    return ACL_CREATE_TEMP;
  }
  if (strcmp(privname, "temp") == 0)
  {
    return ACL_CREATE_TEMP;
  }
  if (strcmp(privname, "connect") == 0)
  {
    return ACL_CONNECT;
  }
  if (strcmp(privname, "rule") == 0)
  {
    return 0;                                 
  }
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized privilege type \"%s\"", privname)));
  return 0;                       
}

static const char *
privilege_to_string(AclMode privilege)
{
  switch (privilege)
  {
  case ACL_INSERT:
    return "INSERT";
  case ACL_SELECT:
    return "SELECT";
  case ACL_UPDATE:
    return "UPDATE";
  case ACL_DELETE:
    return "DELETE";
  case ACL_TRUNCATE:
    return "TRUNCATE";
  case ACL_REFERENCES:
    return "REFERENCES";
  case ACL_TRIGGER:
    return "TRIGGER";
  case ACL_EXECUTE:
    return "EXECUTE";
  case ACL_USAGE:
    return "USAGE";
  case ACL_CREATE:
    return "CREATE";
  case ACL_CREATE_TEMP:
    return "TEMP";
  case ACL_CONNECT:
    return "CONNECT";
  default:
    elog(ERROR, "unrecognized privilege: %d", (int)privilege);
  }
  return NULL;                       
}

   
                                                            
   
                                                                     
                                                
   
void
aclcheck_error(AclResult aclerr, ObjectType objtype, const char *objectname)
{
  switch (aclerr)
  {
  case ACLCHECK_OK:
                                       
    break;
  case ACLCHECK_NO_PRIV:
  {
    const char *msg = "???";

    switch (objtype)
    {
    case OBJECT_AGGREGATE:
      msg = gettext_noop("permission denied for aggregate %s");
      break;
    case OBJECT_COLLATION:
      msg = gettext_noop("permission denied for collation %s");
      break;
    case OBJECT_COLUMN:
      msg = gettext_noop("permission denied for column %s");
      break;
    case OBJECT_CONVERSION:
      msg = gettext_noop("permission denied for conversion %s");
      break;
    case OBJECT_DATABASE:
      msg = gettext_noop("permission denied for database %s");
      break;
    case OBJECT_DOMAIN:
      msg = gettext_noop("permission denied for domain %s");
      break;
    case OBJECT_EVENT_TRIGGER:
      msg = gettext_noop("permission denied for event trigger %s");
      break;
    case OBJECT_EXTENSION:
      msg = gettext_noop("permission denied for extension %s");
      break;
    case OBJECT_FDW:
      msg = gettext_noop("permission denied for foreign-data wrapper %s");
      break;
    case OBJECT_FOREIGN_SERVER:
      msg = gettext_noop("permission denied for foreign server %s");
      break;
    case OBJECT_FOREIGN_TABLE:
      msg = gettext_noop("permission denied for foreign table %s");
      break;
    case OBJECT_FUNCTION:
      msg = gettext_noop("permission denied for function %s");
      break;
    case OBJECT_INDEX:
      msg = gettext_noop("permission denied for index %s");
      break;
    case OBJECT_LANGUAGE:
      msg = gettext_noop("permission denied for language %s");
      break;
    case OBJECT_LARGEOBJECT:
      msg = gettext_noop("permission denied for large object %s");
      break;
    case OBJECT_MATVIEW:
      msg = gettext_noop("permission denied for materialized view %s");
      break;
    case OBJECT_OPCLASS:
      msg = gettext_noop("permission denied for operator class %s");
      break;
    case OBJECT_OPERATOR:
      msg = gettext_noop("permission denied for operator %s");
      break;
    case OBJECT_OPFAMILY:
      msg = gettext_noop("permission denied for operator family %s");
      break;
    case OBJECT_POLICY:
      msg = gettext_noop("permission denied for policy %s");
      break;
    case OBJECT_PROCEDURE:
      msg = gettext_noop("permission denied for procedure %s");
      break;
    case OBJECT_PUBLICATION:
      msg = gettext_noop("permission denied for publication %s");
      break;
    case OBJECT_ROUTINE:
      msg = gettext_noop("permission denied for routine %s");
      break;
    case OBJECT_SCHEMA:
      msg = gettext_noop("permission denied for schema %s");
      break;
    case OBJECT_SEQUENCE:
      msg = gettext_noop("permission denied for sequence %s");
      break;
    case OBJECT_STATISTIC_EXT:
      msg = gettext_noop("permission denied for statistics object %s");
      break;
    case OBJECT_SUBSCRIPTION:
      msg = gettext_noop("permission denied for subscription %s");
      break;
    case OBJECT_TABLE:
      msg = gettext_noop("permission denied for table %s");
      break;
    case OBJECT_TABLESPACE:
      msg = gettext_noop("permission denied for tablespace %s");
      break;
    case OBJECT_TSCONFIGURATION:
      msg = gettext_noop("permission denied for text search configuration %s");
      break;
    case OBJECT_TSDICTIONARY:
      msg = gettext_noop("permission denied for text search dictionary %s");
      break;
    case OBJECT_TYPE:
      msg = gettext_noop("permission denied for type %s");
      break;
    case OBJECT_VIEW:
      msg = gettext_noop("permission denied for view %s");
      break;
                                       
    case OBJECT_ACCESS_METHOD:
    case OBJECT_AMOP:
    case OBJECT_AMPROC:
    case OBJECT_ATTRIBUTE:
    case OBJECT_CAST:
    case OBJECT_DEFAULT:
    case OBJECT_DEFACL:
    case OBJECT_DOMCONSTRAINT:
    case OBJECT_PUBLICATION_REL:
    case OBJECT_ROLE:
    case OBJECT_RULE:
    case OBJECT_TABCONSTRAINT:
    case OBJECT_TRANSFORM:
    case OBJECT_TRIGGER:
    case OBJECT_TSPARSER:
    case OBJECT_TSTEMPLATE:
    case OBJECT_USER_MAPPING:
      elog(ERROR, "unsupported object type %d", objtype);
    }

    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg(msg, objectname)));
    break;
  }
  case ACLCHECK_NOT_OWNER:
  {
    const char *msg = "???";

    switch (objtype)
    {
    case OBJECT_AGGREGATE:
      msg = gettext_noop("must be owner of aggregate %s");
      break;
    case OBJECT_COLLATION:
      msg = gettext_noop("must be owner of collation %s");
      break;
    case OBJECT_CONVERSION:
      msg = gettext_noop("must be owner of conversion %s");
      break;
    case OBJECT_DATABASE:
      msg = gettext_noop("must be owner of database %s");
      break;
    case OBJECT_DOMAIN:
      msg = gettext_noop("must be owner of domain %s");
      break;
    case OBJECT_EVENT_TRIGGER:
      msg = gettext_noop("must be owner of event trigger %s");
      break;
    case OBJECT_EXTENSION:
      msg = gettext_noop("must be owner of extension %s");
      break;
    case OBJECT_FDW:
      msg = gettext_noop("must be owner of foreign-data wrapper %s");
      break;
    case OBJECT_FOREIGN_SERVER:
      msg = gettext_noop("must be owner of foreign server %s");
      break;
    case OBJECT_FOREIGN_TABLE:
      msg = gettext_noop("must be owner of foreign table %s");
      break;
    case OBJECT_FUNCTION:
      msg = gettext_noop("must be owner of function %s");
      break;
    case OBJECT_INDEX:
      msg = gettext_noop("must be owner of index %s");
      break;
    case OBJECT_LANGUAGE:
      msg = gettext_noop("must be owner of language %s");
      break;
    case OBJECT_LARGEOBJECT:
      msg = gettext_noop("must be owner of large object %s");
      break;
    case OBJECT_MATVIEW:
      msg = gettext_noop("must be owner of materialized view %s");
      break;
    case OBJECT_OPCLASS:
      msg = gettext_noop("must be owner of operator class %s");
      break;
    case OBJECT_OPERATOR:
      msg = gettext_noop("must be owner of operator %s");
      break;
    case OBJECT_OPFAMILY:
      msg = gettext_noop("must be owner of operator family %s");
      break;
    case OBJECT_PROCEDURE:
      msg = gettext_noop("must be owner of procedure %s");
      break;
    case OBJECT_PUBLICATION:
      msg = gettext_noop("must be owner of publication %s");
      break;
    case OBJECT_ROUTINE:
      msg = gettext_noop("must be owner of routine %s");
      break;
    case OBJECT_SEQUENCE:
      msg = gettext_noop("must be owner of sequence %s");
      break;
    case OBJECT_SUBSCRIPTION:
      msg = gettext_noop("must be owner of subscription %s");
      break;
    case OBJECT_TABLE:
      msg = gettext_noop("must be owner of table %s");
      break;
    case OBJECT_TYPE:
      msg = gettext_noop("must be owner of type %s");
      break;
    case OBJECT_VIEW:
      msg = gettext_noop("must be owner of view %s");
      break;
    case OBJECT_SCHEMA:
      msg = gettext_noop("must be owner of schema %s");
      break;
    case OBJECT_STATISTIC_EXT:
      msg = gettext_noop("must be owner of statistics object %s");
      break;
    case OBJECT_TABLESPACE:
      msg = gettext_noop("must be owner of tablespace %s");
      break;
    case OBJECT_TSCONFIGURATION:
      msg = gettext_noop("must be owner of text search configuration %s");
      break;
    case OBJECT_TSDICTIONARY:
      msg = gettext_noop("must be owner of text search dictionary %s");
      break;

         
                                                           
                                                    
                                          
                                   
         
    case OBJECT_COLUMN:
    case OBJECT_POLICY:
    case OBJECT_RULE:
    case OBJECT_TABCONSTRAINT:
    case OBJECT_TRIGGER:
      msg = gettext_noop("must be owner of relation %s");
      break;
                                       
    case OBJECT_ACCESS_METHOD:
    case OBJECT_AMOP:
    case OBJECT_AMPROC:
    case OBJECT_ATTRIBUTE:
    case OBJECT_CAST:
    case OBJECT_DEFAULT:
    case OBJECT_DEFACL:
    case OBJECT_DOMCONSTRAINT:
    case OBJECT_PUBLICATION_REL:
    case OBJECT_ROLE:
    case OBJECT_TRANSFORM:
    case OBJECT_TSPARSER:
    case OBJECT_TSTEMPLATE:
    case OBJECT_USER_MAPPING:
      elog(ERROR, "unsupported object type %d", objtype);
    }

    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg(msg, objectname)));
    break;
  }
  default:
    elog(ERROR, "unrecognized AclResult: %d", (int)aclerr);
    break;
  }
}

void
aclcheck_error_col(AclResult aclerr, ObjectType objtype, const char *objectname, const char *colname)
{
  switch (aclerr)
  {
  case ACLCHECK_OK:
                                       
    break;
  case ACLCHECK_NO_PRIV:
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied for column \"%s\" of relation \"%s\"", colname, objectname)));
    break;
  case ACLCHECK_NOT_OWNER:
                                                                     
    aclcheck_error(aclerr, objtype, objectname);
    break;
  default:
    elog(ERROR, "unrecognized AclResult: %d", (int)aclerr);
    break;
  }
}

   
                                                                              
                     
   
void
aclcheck_error_type(AclResult aclerr, Oid typeOid)
{
  Oid element_type = get_element_type(typeOid);

  aclcheck_error(aclerr, OBJECT_TYPE, format_type_be(element_type ? element_type : typeOid));
}

   
                                                                     
   
static AclMode
pg_aclmask(ObjectType objtype, Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mask, AclMaskHow how)
{
  switch (objtype)
  {
  case OBJECT_COLUMN:
    return pg_class_aclmask(table_oid, roleid, mask, how) | pg_attribute_aclmask(table_oid, attnum, roleid, mask, how);
  case OBJECT_TABLE:
  case OBJECT_SEQUENCE:
    return pg_class_aclmask(table_oid, roleid, mask, how);
  case OBJECT_DATABASE:
    return pg_database_aclmask(table_oid, roleid, mask, how);
  case OBJECT_FUNCTION:
    return pg_proc_aclmask(table_oid, roleid, mask, how);
  case OBJECT_LANGUAGE:
    return pg_language_aclmask(table_oid, roleid, mask, how);
  case OBJECT_LARGEOBJECT:
    return pg_largeobject_aclmask_snapshot(table_oid, roleid, mask, how, NULL);
  case OBJECT_SCHEMA:
    return pg_namespace_aclmask(table_oid, roleid, mask, how);
  case OBJECT_STATISTIC_EXT:
    elog(ERROR, "grantable rights not supported for statistics objects");
                                              
    return ACL_NO_RIGHTS;
  case OBJECT_TABLESPACE:
    return pg_tablespace_aclmask(table_oid, roleid, mask, how);
  case OBJECT_FDW:
    return pg_foreign_data_wrapper_aclmask(table_oid, roleid, mask, how);
  case OBJECT_FOREIGN_SERVER:
    return pg_foreign_server_aclmask(table_oid, roleid, mask, how);
  case OBJECT_EVENT_TRIGGER:
    elog(ERROR, "grantable rights not supported for event triggers");
                                              
    return ACL_NO_RIGHTS;
  case OBJECT_TYPE:
    return pg_type_aclmask(table_oid, roleid, mask, how);
  default:
    elog(ERROR, "unrecognized objtype: %d", (int)objtype);
                                              
    return ACL_NO_RIGHTS;
  }
}

                                                                    
                                                                           
   
                                                                          
   
                                                                       
                                                                          
                           
                                                                    
   

   
                                                                   
   
                                                                            
                                                                                
                                                                      
                         
   
AclMode
pg_attribute_aclmask(Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple classTuple;
  HeapTuple attTuple;
  Form_pg_class classForm;
  Form_pg_attribute attributeForm;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

     
                                                             
     
  attTuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(table_oid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(attTuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("attribute %d of relation with OID %u does not exist", attnum, table_oid)));
  }
  attributeForm = (Form_pg_attribute)GETSTRUCT(attTuple);

                                           
  if (attributeForm->attisdropped)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("attribute %d of relation with OID %u does not exist", attnum, table_oid)));
  }

  aclDatum = SysCacheGetAttr(ATTNUM, attTuple, Anum_pg_attribute_attacl, &isNull);

     
                                                                             
                                                                         
                           
     
  if (isNull)
  {
    ReleaseSysCache(attTuple);
    return 0;
  }

     
                                                                            
                                                                             
                                                                            
                                                                             
                                                                             
                                               
     
  classTuple = SearchSysCache1(RELOID, ObjectIdGetDatum(table_oid));
  if (!HeapTupleIsValid(classTuple))
  {
    ReleaseSysCache(attTuple);
    return 0;
  }
  classForm = (Form_pg_class)GETSTRUCT(classTuple);

  ownerId = classForm->relowner;

  ReleaseSysCache(classTuple);

                                         
  acl = DatumGetAclP(aclDatum);

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(attTuple);

  return result;
}

   
                                                                  
   
AclMode
pg_class_aclmask(Oid table_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Form_pg_class classForm;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

     
                                                 
     
  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(table_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation with OID %u does not exist", table_oid)));
  }
  classForm = (Form_pg_class)GETSTRUCT(tuple);

     
                                                              
                                                                        
     
                                                                       
                                                                    
                                                                 
     
  if ((mask & (ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_TRUNCATE | ACL_USAGE)) && IsSystemClass(table_oid, classForm) && classForm->relkind != RELKIND_VIEW && !superuser_arg(roleid) && !allowSystemTableMods)
  {
#ifdef ACLDEBUG
    elog(DEBUG2, "permission denied for system catalog update");
#endif
    mask &= ~(ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_TRUNCATE | ACL_USAGE);
  }

     
                                                           
     
  if (superuser_arg(roleid))
  {
#ifdef ACLDEBUG
    elog(DEBUG2, "OID %u is superuser, home free", roleid);
#endif
    ReleaseSysCache(tuple);
    return mask;
  }

     
                                                       
     
  ownerId = classForm->relowner;

  aclDatum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relacl, &isNull);
  if (isNull)
  {
                                      
    switch (classForm->relkind)
    {
    case RELKIND_SEQUENCE:
      acl = acldefault(OBJECT_SEQUENCE, ownerId);
      break;
    default:
      acl = acldefault(OBJECT_TABLE, ownerId);
      break;
    }
    aclDatum = (Datum)0;
  }
  else
  {
                                        
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                     
   
AclMode
pg_database_aclmask(Oid db_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                             
     
  tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(db_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database with OID %u does not exist", db_oid)));
  }

  ownerId = ((Form_pg_database)GETSTRUCT(tuple))->datdba;

  aclDatum = SysCacheGetAttr(DATABASEOID, tuple, Anum_pg_database_datacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_DATABASE, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                  
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                     
   
AclMode
pg_proc_aclmask(Oid proc_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                         
     
  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(proc_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function with OID %u does not exist", proc_oid)));
  }

  ownerId = ((Form_pg_proc)GETSTRUCT(tuple))->proowner;

  aclDatum = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_proacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_FUNCTION, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                  
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                     
   
AclMode
pg_language_aclmask(Oid lang_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                             
     
  tuple = SearchSysCache1(LANGOID, ObjectIdGetDatum(lang_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("language with OID %u does not exist", lang_oid)));
  }

  ownerId = ((Form_pg_language)GETSTRUCT(tuple))->lanowner;

  aclDatum = SysCacheGetAttr(LANGOID, tuple, Anum_pg_language_lanacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_LANGUAGE, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                  
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                        
   
                                                                           
                                                                   
                                                                           
                                                                       
                                                                          
                                                                       
                                                                           
                            
   
AclMode
pg_largeobject_aclmask_snapshot(Oid lobj_oid, Oid roleid, AclMode mask, AclMaskHow how, Snapshot snapshot)
{
  AclMode result;
  Relation pg_lo_meta;
  ScanKeyData entry[1];
  SysScanDesc scan;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                                         
     
  pg_lo_meta = table_open(LargeObjectMetadataRelationId, AccessShareLock);

  ScanKeyInit(&entry[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(lobj_oid));

  scan = systable_beginscan(pg_lo_meta, LargeObjectMetadataOidIndexId, true, snapshot, 1, entry);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("large object %u does not exist", lobj_oid)));
  }

  ownerId = ((Form_pg_largeobject_metadata)GETSTRUCT(tuple))->lomowner;

  aclDatum = heap_getattr(tuple, Anum_pg_largeobject_metadata_lomacl, RelationGetDescr(pg_lo_meta), &isNull);

  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_LARGEOBJECT, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                  
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  systable_endscan(scan);

  table_close(pg_lo_meta, AccessShareLock);

  return result;
}

   
                                                                      
   
AclMode
pg_namespace_aclmask(Oid nsp_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                                                           
                                                                             
                                                                          
                                                                         
                                         
     
                                                                            
                                                                            
                                                                            
                                                                         
                                                                           
             
     
                                                                
                                                                         
                                                                            
                               
     
  if (isTempNamespace(nsp_oid))
  {
    if (pg_database_aclcheck(MyDatabaseId, roleid, ACL_CREATE_TEMP) == ACLCHECK_OK)
    {
      return mask & ACL_ALL_RIGHTS_SCHEMA;
    }
    else
    {
      return mask & ACL_USAGE;
    }
  }

     
                                            
     
  tuple = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(nsp_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("schema with OID %u does not exist", nsp_oid)));
  }

  ownerId = ((Form_pg_namespace)GETSTRUCT(tuple))->nspowner;

  aclDatum = SysCacheGetAttr(NAMESPACEOID, tuple, Anum_pg_namespace_nspacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_SCHEMA, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                  
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                       
   
AclMode
pg_tablespace_aclmask(Oid spc_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                                 
     
  tuple = SearchSysCache1(TABLESPACEOID, ObjectIdGetDatum(spc_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace with OID %u does not exist", spc_oid)));
  }

  ownerId = ((Form_pg_tablespace)GETSTRUCT(tuple))->spcowner;

  aclDatum = SysCacheGetAttr(TABLESPACEOID, tuple, Anum_pg_tablespace_spcacl, &isNull);

  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_TABLESPACE, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                  
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                    
                
   
AclMode
pg_foreign_data_wrapper_aclmask(Oid fdw_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

  Form_pg_foreign_data_wrapper fdwForm;

                                               
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                                           
     
  tuple = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdw_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign-data wrapper with OID %u does not exist", fdw_oid)));
  }
  fdwForm = (Form_pg_foreign_data_wrapper)GETSTRUCT(tuple);

     
                                                                 
     
  ownerId = fdwForm->fdwowner;

  aclDatum = SysCacheGetAttr(FOREIGNDATAWRAPPEROID, tuple, Anum_pg_foreign_data_wrapper_fdwacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_FDW, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                        
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                    
           
   
AclMode
pg_foreign_server_aclmask(Oid srv_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

  Form_pg_foreign_server srvForm;

                                               
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                                           
     
  tuple = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(srv_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign server with OID %u does not exist", srv_oid)));
  }
  srvForm = (Form_pg_foreign_server)GETSTRUCT(tuple);

     
                                                                      
     
  ownerId = srvForm->srvowner;

  aclDatum = SysCacheGetAttr(FOREIGNSERVEROID, tuple, Anum_pg_foreign_server_srvacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_FOREIGN_SERVER, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                        
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                  
   
AclMode
pg_type_aclmask(Oid type_oid, Oid roleid, AclMode mask, AclMaskHow how)
{
  AclMode result;
  HeapTuple tuple;
  Datum aclDatum;
  bool isNull;
  Acl *acl;
  Oid ownerId;

  Form_pg_type typeForm;

                                               
  if (superuser_arg(roleid))
  {
    return mask;
  }

     
                                            
     
  tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type with OID %u does not exist", type_oid)));
  }
  typeForm = (Form_pg_type)GETSTRUCT(tuple);

     
                                                                           
                           
     
  if (OidIsValid(typeForm->typelem) && typeForm->typlen == -1)
  {
    Oid elttype_oid = typeForm->typelem;

    ReleaseSysCache(tuple);

    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(elttype_oid));
                                                                   
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for type %u", elttype_oid);
    }
    typeForm = (Form_pg_type)GETSTRUCT(tuple);
  }

     
                                                     
     
  ownerId = typeForm->typowner;

  aclDatum = SysCacheGetAttr(TYPEOID, tuple, Anum_pg_type_typacl, &isNull);
  if (isNull)
  {
                                      
    acl = acldefault(OBJECT_TYPE, ownerId);
    aclDatum = (Datum)0;
  }
  else
  {
                                        
    acl = DatumGetAclP(aclDatum);
  }

  result = aclmask(acl, roleid, ownerId, mask, how);

                                            
  if (acl && (Pointer)acl != DatumGetPointer(aclDatum))
  {
    pfree(acl);
  }

  ReleaseSysCache(tuple);

  return result;
}

   
                                                                        
   
                                                                           
                                                                        
                      
   
                                                                         
                               
   
AclResult
pg_attribute_aclcheck(Oid table_oid, AttrNumber attnum, Oid roleid, AclMode mode)
{
  if (pg_attribute_aclmask(table_oid, attnum, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                               
   
                                                                            
                                                                              
                                                                
                      
   
                                                                            
                                                                              
                                                                              
                                                      
   
                                                                         
                                  
   
                                                                            
                                                                    
   
AclResult
pg_attribute_aclcheck_all(Oid table_oid, Oid roleid, AclMode mode, AclMaskHow how)
{
  AclResult result;
  HeapTuple classTuple;
  Form_pg_class classForm;
  AttrNumber nattrs;
  AttrNumber curr_att;

     
                                                                   
                                                                          
                                                               
     
  classTuple = SearchSysCache1(RELOID, ObjectIdGetDatum(table_oid));
  if (!HeapTupleIsValid(classTuple))
  {
    return ACLCHECK_NO_PRIV;
  }
  classForm = (Form_pg_class)GETSTRUCT(classTuple);

  nattrs = classForm->relnatts;

  ReleaseSysCache(classTuple);

     
                                                                             
                                                             
     
  result = ACLCHECK_NO_PRIV;

  for (curr_att = 1; curr_att <= nattrs; curr_att++)
  {
    HeapTuple attTuple;
    AclMode attmask;

    attTuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(table_oid), Int16GetDatum(curr_att));
    if (!HeapTupleIsValid(attTuple))
    {
      continue;
    }

                                
    if (((Form_pg_attribute)GETSTRUCT(attTuple))->attisdropped)
    {
      ReleaseSysCache(attTuple);
      continue;
    }

       
                                                                     
                                                                         
                                         
       
    if (heap_attisnull(attTuple, Anum_pg_attribute_attacl, NULL))
    {
      attmask = 0;
    }
    else
    {
      attmask = pg_attribute_aclmask(table_oid, curr_att, roleid, mode, ACLMASK_ANY);
    }

    ReleaseSysCache(attTuple);

    if (attmask != 0)
    {
      result = ACLCHECK_OK;
      if (how == ACLMASK_ANY)
      {
        break;                             
      }
    }
    else
    {
      result = ACLCHECK_NO_PRIV;
      if (how == ACLMASK_ALL)
      {
        break;                          
      }
    }
  }

  return result;
}

   
                                                                       
   
                                                                           
                                                                        
                      
   
AclResult
pg_class_aclcheck(Oid table_oid, Oid roleid, AclMode mode)
{
  if (pg_class_aclmask(table_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                          
   
AclResult
pg_database_aclcheck(Oid db_oid, Oid roleid, AclMode mode)
{
  if (pg_database_aclmask(db_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                          
   
AclResult
pg_proc_aclcheck(Oid proc_oid, Oid roleid, AclMode mode)
{
  if (pg_proc_aclmask(proc_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                          
   
AclResult
pg_language_aclcheck(Oid lang_oid, Oid roleid, AclMode mode)
{
  if (pg_language_aclmask(lang_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                             
   
AclResult
pg_largeobject_aclcheck_snapshot(Oid lobj_oid, Oid roleid, AclMode mode, Snapshot snapshot)
{
  if (pg_largeobject_aclmask_snapshot(lobj_oid, roleid, mode, ACLMASK_ANY, snapshot) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                           
   
AclResult
pg_namespace_aclcheck(Oid nsp_oid, Oid roleid, AclMode mode)
{
  if (pg_namespace_aclmask(nsp_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                            
   
AclResult
pg_tablespace_aclcheck(Oid spc_oid, Oid roleid, AclMode mode)
{
  if (pg_tablespace_aclmask(spc_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                         
                
   
AclResult
pg_foreign_data_wrapper_aclcheck(Oid fdw_oid, Oid roleid, AclMode mode)
{
  if (pg_foreign_data_wrapper_aclmask(fdw_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                         
          
   
AclResult
pg_foreign_server_aclcheck(Oid srv_oid, Oid roleid, AclMode mode)
{
  if (pg_foreign_server_aclmask(srv_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                                      
   
AclResult
pg_type_aclcheck(Oid type_oid, Oid roleid, AclMode mode)
{
  if (pg_type_aclmask(type_oid, roleid, mode, ACLMASK_ANY) != 0)
  {
    return ACLCHECK_OK;
  }
  else
  {
    return ACLCHECK_NO_PRIV;
  }
}

   
                                                      
   
bool
pg_class_ownercheck(Oid class_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(class_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation with OID %u does not exist", class_oid)));
  }

  ownerId = ((Form_pg_class)GETSTRUCT(tuple))->relowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                  
   
bool
pg_type_ownercheck(Oid type_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type with OID %u does not exist", type_oid)));
  }

  ownerId = ((Form_pg_type)GETSTRUCT(tuple))->typowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                       
   
bool
pg_oper_ownercheck(Oid oper_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(OPEROID, ObjectIdGetDatum(oper_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("operator with OID %u does not exist", oper_oid)));
  }

  ownerId = ((Form_pg_operator)GETSTRUCT(tuple))->oprowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                      
   
bool
pg_proc_ownercheck(Oid proc_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(proc_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function with OID %u does not exist", proc_oid)));
  }

  ownerId = ((Form_pg_proc)GETSTRUCT(tuple))->proowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                                
   
bool
pg_language_ownercheck(Oid lan_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(LANGOID, ObjectIdGetDatum(lan_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("language with OID %u does not exist", lan_oid)));
  }

  ownerId = ((Form_pg_language)GETSTRUCT(tuple))->lanowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                        
   
                                                                            
                                       
   
bool
pg_largeobject_ownercheck(Oid lobj_oid, Oid roleid)
{
  Relation pg_lo_meta;
  ScanKeyData entry[1];
  SysScanDesc scan;
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

                                                       
  pg_lo_meta = table_open(LargeObjectMetadataRelationId, AccessShareLock);

  ScanKeyInit(&entry[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(lobj_oid));

  scan = systable_beginscan(pg_lo_meta, LargeObjectMetadataOidIndexId, true, NULL, 1, entry);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("large object %u does not exist", lobj_oid)));
  }

  ownerId = ((Form_pg_largeobject_metadata)GETSTRUCT(tuple))->lomowner;

  systable_endscan(scan);
  table_close(pg_lo_meta, AccessShareLock);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                       
   
bool
pg_namespace_ownercheck(Oid nsp_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(nsp_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("schema with OID %u does not exist", nsp_oid)));
  }

  ownerId = ((Form_pg_namespace)GETSTRUCT(tuple))->nspowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                        
   
bool
pg_tablespace_ownercheck(Oid spc_oid, Oid roleid)
{
  HeapTuple spctuple;
  Oid spcowner;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

                                         
  spctuple = SearchSysCache1(TABLESPACEOID, ObjectIdGetDatum(spc_oid));
  if (!HeapTupleIsValid(spctuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace with OID %u does not exist", spc_oid)));
  }

  spcowner = ((Form_pg_tablespace)GETSTRUCT(spctuple))->spcowner;

  ReleaseSysCache(spctuple);

  return has_privs_of_role(roleid, spcowner);
}

   
                                                             
   
bool
pg_opclass_ownercheck(Oid opc_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(CLAOID, ObjectIdGetDatum(opc_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator class with OID %u does not exist", opc_oid)));
  }

  ownerId = ((Form_pg_opclass)GETSTRUCT(tuple))->opcowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                              
   
bool
pg_opfamily_ownercheck(Oid opf_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(OPFAMILYOID, ObjectIdGetDatum(opf_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator family with OID %u does not exist", opf_oid)));
  }

  ownerId = ((Form_pg_opfamily)GETSTRUCT(tuple))->opfowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                                    
   
bool
pg_ts_dict_ownercheck(Oid dict_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(TSDICTOID, ObjectIdGetDatum(dict_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search dictionary with OID %u does not exist", dict_oid)));
  }

  ownerId = ((Form_pg_ts_dict)GETSTRUCT(tuple))->dictowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                                       
   
bool
pg_ts_config_ownercheck(Oid cfg_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(TSCONFIGOID, ObjectIdGetDatum(cfg_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search configuration with OID %u does not exist", cfg_oid)));
  }

  ownerId = ((Form_pg_ts_config)GETSTRUCT(tuple))->cfgowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                                  
   
bool
pg_foreign_data_wrapper_ownercheck(Oid srv_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(srv_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign-data wrapper with OID %u does not exist", srv_oid)));
  }

  ownerId = ((Form_pg_foreign_data_wrapper)GETSTRUCT(tuple))->fdwowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                            
   
bool
pg_foreign_server_ownercheck(Oid srv_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(srv_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign server with OID %u does not exist", srv_oid)));
  }

  ownerId = ((Form_pg_foreign_server)GETSTRUCT(tuple))->srvowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                            
   
bool
pg_event_trigger_ownercheck(Oid et_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(EVENTTRIGGEROID, ObjectIdGetDatum(et_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("event trigger with OID %u does not exist", et_oid)));
  }

  ownerId = ((Form_pg_event_trigger)GETSTRUCT(tuple))->evtowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                      
   
bool
pg_database_ownercheck(Oid db_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid dba;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(db_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database with OID %u does not exist", db_oid)));
  }

  dba = ((Form_pg_database)GETSTRUCT(tuple))->datdba;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, dba);
}

   
                                                       
   
bool
pg_collation_ownercheck(Oid coll_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(COLLOID, ObjectIdGetDatum(coll_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("collation with OID %u does not exist", coll_oid)));
  }

  ownerId = ((Form_pg_collation)GETSTRUCT(tuple))->collowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                        
   
bool
pg_conversion_ownercheck(Oid conv_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(CONVOID, ObjectIdGetDatum(conv_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("conversion with OID %u does not exist", conv_oid)));
  }

  ownerId = ((Form_pg_conversion)GETSTRUCT(tuple))->conowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                        
   
bool
pg_extension_ownercheck(Oid ext_oid, Oid roleid)
{
  Relation pg_extension;
  ScanKeyData entry[1];
  SysScanDesc scan;
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

                                                                   
  pg_extension = table_open(ExtensionRelationId, AccessShareLock);

  ScanKeyInit(&entry[0], Anum_pg_extension_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(ext_oid));

  scan = systable_beginscan(pg_extension, ExtensionOidIndexId, true, NULL, 1, entry);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("extension with OID %u does not exist", ext_oid)));
  }

  ownerId = ((Form_pg_extension)GETSTRUCT(tuple))->extowner;

  systable_endscan(scan);
  table_close(pg_extension, AccessShareLock);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                         
   
bool
pg_publication_ownercheck(Oid pub_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(PUBLICATIONOID, ObjectIdGetDatum(pub_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("publication with OID %u does not exist", pub_oid)));
  }

  ownerId = ((Form_pg_publication)GETSTRUCT(tuple))->pubowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                          
   
bool
pg_subscription_ownercheck(Oid sub_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(SUBSCRIPTIONOID, ObjectIdGetDatum(sub_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("subscription with OID %u does not exist", sub_oid)));
  }

  ownerId = ((Form_pg_subscription)GETSTRUCT(tuple))->subowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                               
   
bool
pg_statistics_object_ownercheck(Oid stat_oid, Oid roleid)
{
  HeapTuple tuple;
  Oid ownerId;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  tuple = SearchSysCache1(STATEXTOID, ObjectIdGetDatum(stat_oid));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("statistics object with OID %u does not exist", stat_oid)));
  }

  ownerId = ((Form_pg_statistic_ext)GETSTRUCT(tuple))->stxowner;

  ReleaseSysCache(tuple);

  return has_privs_of_role(roleid, ownerId);
}

   
                                                                             
   
                                                                      
                                                                         
                                                                       
                                                                       
                                                                  
                                                                      
                                                                              
   
bool
has_createrole_privilege(Oid roleid)
{
  bool result = false;
  HeapTuple utup;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  utup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (HeapTupleIsValid(utup))
  {
    result = ((Form_pg_authid)GETSTRUCT(utup))->rolcreaterole;
    ReleaseSysCache(utup);
  }
  return result;
}

bool
has_bypassrls_privilege(Oid roleid)
{
  bool result = false;
  HeapTuple utup;

                                                  
  if (superuser_arg(roleid))
  {
    return true;
  }

  utup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (HeapTupleIsValid(utup))
  {
    result = ((Form_pg_authid)GETSTRUCT(utup))->rolbypassrls;
    ReleaseSysCache(utup);
  }
  return result;
}

   
                                                                        
                                                             
                                  
   
static Acl *
get_default_acl_internal(Oid roleId, Oid nsp_oid, char objtype)
{
  Acl *result = NULL;
  HeapTuple tuple;

  tuple = SearchSysCache3(DEFACLROLENSPOBJ, ObjectIdGetDatum(roleId), ObjectIdGetDatum(nsp_oid), CharGetDatum(objtype));

  if (HeapTupleIsValid(tuple))
  {
    Datum aclDatum;
    bool isNull;

    aclDatum = SysCacheGetAttr(DEFACLROLENSPOBJ, tuple, Anum_pg_default_acl_defaclacl, &isNull);
    if (!isNull)
    {
      result = DatumGetAclPCopy(aclDatum);
    }
    ReleaseSysCache(tuple);
  }

  return result;
}

   
                                                                        
   
                                                            
   
                                                                        
                                            
   
Acl *
get_user_default_acl(ObjectType objtype, Oid ownerId, Oid nsp_oid)
{
  Acl *result;
  Acl *glob_acl;
  Acl *schema_acl;
  Acl *def_acl;
  char defaclobjtype;

     
                                                                          
          
     
  if (IsBootstrapProcessingMode())
  {
    return NULL;
  }

                                                           
  switch (objtype)
  {
  case OBJECT_TABLE:
    defaclobjtype = DEFACLOBJ_RELATION;
    break;

  case OBJECT_SEQUENCE:
    defaclobjtype = DEFACLOBJ_SEQUENCE;
    break;

  case OBJECT_FUNCTION:
    defaclobjtype = DEFACLOBJ_FUNCTION;
    break;

  case OBJECT_TYPE:
    defaclobjtype = DEFACLOBJ_TYPE;
    break;

  case OBJECT_SCHEMA:
    defaclobjtype = DEFACLOBJ_NAMESPACE;
    break;

  default:
    return NULL;
  }

                                                   
  glob_acl = get_default_acl_internal(ownerId, InvalidOid, defaclobjtype);
  schema_acl = get_default_acl_internal(ownerId, nsp_oid, defaclobjtype);

                                         
  if (glob_acl == NULL && schema_acl == NULL)
  {
    return NULL;
  }

                                                         
  def_acl = acldefault(objtype, ownerId);

                                                                     
  if (glob_acl == NULL)
  {
    glob_acl = def_acl;
  }

                                          
  result = aclmerge(glob_acl, schema_acl, ownerId);

     
                                                                          
                                                                      
     
  aclitemsort(result);
  aclitemsort(def_acl);
  if (aclequal(result, def_acl))
  {
    result = NULL;
  }

  return result;
}

   
                                                                 
   
void
recordDependencyOnNewAcl(Oid classId, Oid objectId, int32 objsubId, Oid ownerId, Acl *acl)
{
  int nmembers;
  Oid *members;

                                         
  if (acl == NULL)
  {
    return;
  }

                                      
  nmembers = aclmembers(acl, &members);

                                             
  updateAclDependencies(classId, objectId, objsubId, ownerId, 0, NULL, nmembers, members);
}

   
                                                                 
   
                                                                               
                                                     
   
                                                                            
                                                                    
   
void
recordExtObjInitPriv(Oid objoid, Oid classoid)
{
     
                             
     
                                                                            
                                                      
                                                   
     
  if (classoid == RelationRelationId)
  {
    Form_pg_class pg_class_tuple;
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", objoid);
    }
    pg_class_tuple = (Form_pg_class)GETSTRUCT(tuple);

                                        
    if (pg_class_tuple->relkind == RELKIND_INDEX || pg_class_tuple->relkind == RELKIND_PARTITIONED_INDEX)
    {
      return;
    }

                                                       
    if (pg_class_tuple->relkind == RELKIND_COMPOSITE_TYPE)
    {
      return;
    }

       
                                                                    
                                                                         
             
       
    if (pg_class_tuple->relkind != RELKIND_SEQUENCE)
    {
      AttrNumber curr_att;
      AttrNumber nattrs = pg_class_tuple->relnatts;

      for (curr_att = 1; curr_att <= nattrs; curr_att++)
      {
        HeapTuple attTuple;
        Datum attaclDatum;

        attTuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(objoid), Int16GetDatum(curr_att));

        if (!HeapTupleIsValid(attTuple))
        {
          continue;
        }

                                    
        if (((Form_pg_attribute)GETSTRUCT(attTuple))->attisdropped)
        {
          ReleaseSysCache(attTuple);
          continue;
        }

        attaclDatum = SysCacheGetAttr(ATTNUM, attTuple, Anum_pg_attribute_attacl, &isNull);

                                                   
        if (isNull)
        {
          ReleaseSysCache(attTuple);
          continue;
        }

        recordExtensionInitPrivWorker(objoid, classoid, curr_att, DatumGetAclP(attaclDatum));

        ReleaseSysCache(attTuple);
      }
    }

    aclDatum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
                               
  else if (classoid == ForeignDataWrapperRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for foreign data wrapper %u", objoid);
    }

    aclDatum = SysCacheGetAttr(FOREIGNDATAWRAPPEROID, tuple, Anum_pg_foreign_data_wrapper_fdwacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
                         
  else if (classoid == ForeignServerRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(FOREIGNSERVEROID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for foreign server %u", objoid);
    }

    aclDatum = SysCacheGetAttr(FOREIGNSERVEROID, tuple, Anum_pg_foreign_server_srvacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
                   
  else if (classoid == LanguageRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(LANGOID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for language %u", objoid);
    }

    aclDatum = SysCacheGetAttr(LANGOID, tuple, Anum_pg_language_lanacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
                               
  else if (classoid == LargeObjectMetadataRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;
    ScanKeyData entry[1];
    SysScanDesc scan;
    Relation relation;

    relation = table_open(LargeObjectMetadataRelationId, RowExclusiveLock);

                                                         
    ScanKeyInit(&entry[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objoid));

    scan = systable_beginscan(relation, LargeObjectMetadataOidIndexId, true, NULL, 1, entry);

    tuple = systable_getnext(scan);
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "could not find tuple for large object %u", objoid);
    }

    aclDatum = heap_getattr(tuple, Anum_pg_largeobject_metadata_lomacl, RelationGetDescr(relation), &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    systable_endscan(scan);
  }
                    
  else if (classoid == NamespaceRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(NAMESPACEOID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for schema %u", objoid);
    }

    aclDatum = SysCacheGetAttr(NAMESPACEOID, tuple, Anum_pg_namespace_nspacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
               
  else if (classoid == ProcedureRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for function %u", objoid);
    }

    aclDatum = SysCacheGetAttr(PROCOID, tuple, Anum_pg_proc_proacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
               
  else if (classoid == TypeRelationId)
  {
    Datum aclDatum;
    bool isNull;
    HeapTuple tuple;

    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for type %u", objoid);
    }

    aclDatum = SysCacheGetAttr(TYPEOID, tuple, Anum_pg_type_typacl, &isNull);

                                                          
    if (!isNull)
    {
      recordExtensionInitPrivWorker(objoid, classoid, 0, DatumGetAclP(aclDatum));
    }

    ReleaseSysCache(tuple);
  }
  else if (classoid == AccessMethodRelationId || classoid == AggregateRelationId || classoid == CastRelationId || classoid == CollationRelationId || classoid == ConversionRelationId || classoid == EventTriggerRelationId || classoid == OperatorRelationId || classoid == OperatorClassRelationId || classoid == OperatorFamilyRelationId || classoid == NamespaceRelationId || classoid == TSConfigRelationId || classoid == TSDictionaryRelationId || classoid == TSParserRelationId || classoid == TSTemplateRelationId || classoid == TransformRelationId)
  {
                                                       
  }

     
                                                                            
                                         
     
  else
  {
    elog(ERROR, "unrecognized or unsupported class OID: %u", classoid);
  }
}

   
                                                                              
                                                             
   
void
removeExtObjInitPriv(Oid objoid, Oid classoid)
{
     
                                                                            
                                                      
                                                   
     
  if (classoid == RelationRelationId)
  {
    Form_pg_class pg_class_tuple;
    HeapTuple tuple;

    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(objoid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", objoid);
    }
    pg_class_tuple = (Form_pg_class)GETSTRUCT(tuple);

                                        
    if (pg_class_tuple->relkind == RELKIND_INDEX || pg_class_tuple->relkind == RELKIND_PARTITIONED_INDEX)
    {
      return;
    }

                                                       
    if (pg_class_tuple->relkind == RELKIND_COMPOSITE_TYPE)
    {
      return;
    }

       
                                                                    
                                                                         
             
       
    if (pg_class_tuple->relkind != RELKIND_SEQUENCE)
    {
      AttrNumber curr_att;
      AttrNumber nattrs = pg_class_tuple->relnatts;

      for (curr_att = 1; curr_att <= nattrs; curr_att++)
      {
        HeapTuple attTuple;

        attTuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(objoid), Int16GetDatum(curr_att));

        if (!HeapTupleIsValid(attTuple))
        {
          continue;
        }

                                                                     

        recordExtensionInitPrivWorker(objoid, classoid, curr_att, NULL);

        ReleaseSysCache(attTuple);
      }
    }

    ReleaseSysCache(tuple);
  }

                                                           
  recordExtensionInitPrivWorker(objoid, classoid, 0, NULL);
}

   
                                              
   
                                                                              
                          
   
                                                                            
                                                                           
                                                                              
                                                                          
   
                                                                            
                              
   
                                                                               
                             
   
static void
recordExtensionInitPriv(Oid objoid, Oid classoid, int objsubid, Acl *new_acl)
{
     
                                                                           
                                                                       
                                                                           
                                                                        
                                                                       
                                                  
     
  if (!creating_extension && !binary_upgrade_record_init_privs)
  {
    return;
  }

  recordExtensionInitPrivWorker(objoid, classoid, objsubid, new_acl);
}

   
                                                       
   
                                                                              
                                                                        
   
                                                                           
                                                                           
                                                                               
                                                                          
                           
   
static void
recordExtensionInitPrivWorker(Oid objoid, Oid classoid, int objsubid, Acl *new_acl)
{
  Relation relation;
  ScanKeyData key[3];
  SysScanDesc scan;
  HeapTuple tuple;
  HeapTuple oldtuple;

  relation = table_open(InitPrivsRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_init_privs_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objoid));
  ScanKeyInit(&key[1], Anum_pg_init_privs_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classoid));
  ScanKeyInit(&key[2], Anum_pg_init_privs_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(objsubid));

  scan = systable_beginscan(relation, InitPrivsObjIndexId, true, NULL, 3, key);

                                                  
  oldtuple = systable_getnext(scan);

                                                           
  if (HeapTupleIsValid(oldtuple))
  {
    Datum values[Natts_pg_init_privs];
    bool nulls[Natts_pg_init_privs];
    bool replace[Natts_pg_init_privs];

                                                                   
    if (new_acl)
    {
      MemSet(values, 0, sizeof(values));
      MemSet(nulls, false, sizeof(nulls));
      MemSet(replace, false, sizeof(replace));

      values[Anum_pg_init_privs_initprivs - 1] = PointerGetDatum(new_acl);
      replace[Anum_pg_init_privs_initprivs - 1] = true;

      oldtuple = heap_modify_tuple(oldtuple, RelationGetDescr(relation), values, nulls, replace);

      CatalogTupleUpdate(relation, &oldtuple->t_self, oldtuple);
    }
    else
    {
                                                          
      CatalogTupleDelete(relation, &oldtuple->t_self);
    }
  }
  else
  {
    Datum values[Natts_pg_init_privs];
    bool nulls[Natts_pg_init_privs];

       
                                                        
       
                                                                       
                                    
       
    if (new_acl)
    {
                                      
      MemSet(nulls, false, sizeof(nulls));

      values[Anum_pg_init_privs_objoid - 1] = ObjectIdGetDatum(objoid);
      values[Anum_pg_init_privs_classoid - 1] = ObjectIdGetDatum(classoid);
      values[Anum_pg_init_privs_objsubid - 1] = Int32GetDatum(objsubid);

                                                                       
      values[Anum_pg_init_privs_privtype - 1] = CharGetDatum(INITPRIVS_EXTENSION);

      values[Anum_pg_init_privs_initprivs - 1] = PointerGetDatum(new_acl);

      tuple = heap_form_tuple(RelationGetDescr(relation), values, nulls);

      CatalogTupleInsert(relation, tuple);
    }
  }

  systable_endscan(scan);

                                                            
  CommandCounterIncrement();

  table_close(relation, RowExclusiveLock);
}
