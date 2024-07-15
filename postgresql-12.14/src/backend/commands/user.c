                                                                            
   
          
                                                              
   
                                                                         
                                                                        
   
                               
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_db_role_setting.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/seclabel.h"
#include "commands/user.h"
#include "libpq/crypt.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

                                                     
Oid binary_upgrade_next_pg_authid_oid = InvalidOid;

                   
int Password_encryption = PASSWORD_TYPE_MD5;

                                                             
check_password_hook_type check_password_hook = NULL;

static void
AddRoleMems(const char *rolename, Oid roleid, List *memberSpecs, List *memberIds, Oid grantorId, bool admin_opt);
static void
DelRoleMems(const char *rolename, Oid roleid, List *memberSpecs, List *memberIds, bool admin_opt);

                                                     
static bool
have_createrole_privilege(void)
{
  return has_createrole_privilege(GetUserId());
}

   
               
   
Oid
CreateRole(ParseState *pstate, CreateRoleStmt *stmt)
{
  Relation pg_authid_rel;
  TupleDesc pg_authid_dsc;
  HeapTuple tuple;
  Datum new_record[Natts_pg_authid];
  bool new_record_nulls[Natts_pg_authid];
  Oid roleid;
  ListCell *item;
  ListCell *option;
  char *password = NULL;                         
  bool issuper = false;                                       
  bool inherit = true;                                      
  bool createrole = false;                                     
  bool createdb = false;                                          
  bool canlogin = false;                                
  bool isreplication = false;                                  
  bool bypassrls = false;                                               
  int connlimit = -1;                                          
  List *addroleto = NIL;                                          
  List *rolemembers = NIL;                                          
  List *adminmembers = NIL;                                        
  char *validUntil = NULL;                                       
  Datum validUntil_datum;                                     
  bool validUntil_null;
  DefElem *dpassword = NULL;
  DefElem *dissuper = NULL;
  DefElem *dinherit = NULL;
  DefElem *dcreaterole = NULL;
  DefElem *dcreatedb = NULL;
  DefElem *dcanlogin = NULL;
  DefElem *disreplication = NULL;
  DefElem *dconnlimit = NULL;
  DefElem *daddroleto = NULL;
  DefElem *drolemembers = NULL;
  DefElem *dadminmembers = NULL;
  DefElem *dvalidUntil = NULL;
  DefElem *dbypassRLS = NULL;

                                                                      
  switch (stmt->stmt_type)
  {
  case ROLESTMT_ROLE:
    break;
  case ROLESTMT_USER:
    canlogin = true;
                                                              
    break;
  case ROLESTMT_GROUP:
    break;
  }

                                                    
  foreach (option, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(option);

    if (strcmp(defel->defname, "password") == 0)
    {
      if (dpassword)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dpassword = defel;
    }
    else if (strcmp(defel->defname, "sysid") == 0)
    {
      ereport(NOTICE, (errmsg("SYSID can no longer be specified")));
    }
    else if (strcmp(defel->defname, "superuser") == 0)
    {
      if (dissuper)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dissuper = defel;
    }
    else if (strcmp(defel->defname, "inherit") == 0)
    {
      if (dinherit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dinherit = defel;
    }
    else if (strcmp(defel->defname, "createrole") == 0)
    {
      if (dcreaterole)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dcreaterole = defel;
    }
    else if (strcmp(defel->defname, "createdb") == 0)
    {
      if (dcreatedb)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dcreatedb = defel;
    }
    else if (strcmp(defel->defname, "canlogin") == 0)
    {
      if (dcanlogin)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dcanlogin = defel;
    }
    else if (strcmp(defel->defname, "isreplication") == 0)
    {
      if (disreplication)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      disreplication = defel;
    }
    else if (strcmp(defel->defname, "connectionlimit") == 0)
    {
      if (dconnlimit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dconnlimit = defel;
    }
    else if (strcmp(defel->defname, "addroleto") == 0)
    {
      if (daddroleto)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      daddroleto = defel;
    }
    else if (strcmp(defel->defname, "rolemembers") == 0)
    {
      if (drolemembers)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      drolemembers = defel;
    }
    else if (strcmp(defel->defname, "adminmembers") == 0)
    {
      if (dadminmembers)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dadminmembers = defel;
    }
    else if (strcmp(defel->defname, "validUntil") == 0)
    {
      if (dvalidUntil)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dvalidUntil = defel;
    }
    else if (strcmp(defel->defname, "bypassrls") == 0)
    {
      if (dbypassRLS)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options"), parser_errposition(pstate, defel->location)));
      }
      dbypassRLS = defel;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

  if (dpassword && dpassword->arg)
  {
    password = strVal(dpassword->arg);
  }
  if (dissuper)
  {
    issuper = intVal(dissuper->arg) != 0;
  }
  if (dinherit)
  {
    inherit = intVal(dinherit->arg) != 0;
  }
  if (dcreaterole)
  {
    createrole = intVal(dcreaterole->arg) != 0;
  }
  if (dcreatedb)
  {
    createdb = intVal(dcreatedb->arg) != 0;
  }
  if (dcanlogin)
  {
    canlogin = intVal(dcanlogin->arg) != 0;
  }
  if (disreplication)
  {
    isreplication = intVal(disreplication->arg) != 0;
  }
  if (dconnlimit)
  {
    connlimit = intVal(dconnlimit->arg);
    if (connlimit < -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid connection limit: %d", connlimit)));
    }
  }
  if (daddroleto)
  {
    addroleto = (List *)daddroleto->arg;
  }
  if (drolemembers)
  {
    rolemembers = (List *)drolemembers->arg;
  }
  if (dadminmembers)
  {
    adminmembers = (List *)dadminmembers->arg;
  }
  if (dvalidUntil)
  {
    validUntil = strVal(dvalidUntil->arg);
  }
  if (dbypassRLS)
  {
    bypassrls = intVal(dbypassRLS->arg) != 0;
  }

                                    
  if (issuper)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create superusers")));
    }
  }
  else if (isreplication)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create replication users")));
    }
  }
  else if (bypassrls)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to change bypassrls attribute")));
    }
  }
  else
  {
    if (!have_createrole_privilege())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create role")));
    }
  }

     
                                                                        
                      
     
  if (IsReservedName(stmt->role))
  {
    ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("role name \"%s\" is reserved", stmt->role), errdetail("Role names starting with \"pg_\" are reserved.")));
  }

     
                                                                     
                                              
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strncmp(stmt->role, "regress_", 8) != 0)
  {
    elog(WARNING, "roles created by regression test cases should have names starting with \"regress_\"");
  }
#endif

     
                                                                         
            
     
  pg_authid_rel = table_open(AuthIdRelationId, RowExclusiveLock);
  pg_authid_dsc = RelationGetDescr(pg_authid_rel);

  if (OidIsValid(get_role_oid(stmt->role, true)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("role \"%s\" already exists", stmt->role)));
  }

                                           
  if (validUntil)
  {
    validUntil_datum = DirectFunctionCall3(timestamptz_in, CStringGetDatum(validUntil), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
    validUntil_null = false;
  }
  else
  {
    validUntil_datum = (Datum)0;
    validUntil_null = true;
  }

     
                                                             
     
  if (check_password_hook && password)
  {
    (*check_password_hook)(stmt->role, password, get_password_type(password), validUntil_datum, validUntil_null);
  }

     
                             
     
  MemSet(new_record, 0, sizeof(new_record));
  MemSet(new_record_nulls, false, sizeof(new_record_nulls));

  new_record[Anum_pg_authid_rolname - 1] = DirectFunctionCall1(namein, CStringGetDatum(stmt->role));

  new_record[Anum_pg_authid_rolsuper - 1] = BoolGetDatum(issuper);
  new_record[Anum_pg_authid_rolinherit - 1] = BoolGetDatum(inherit);
  new_record[Anum_pg_authid_rolcreaterole - 1] = BoolGetDatum(createrole);
  new_record[Anum_pg_authid_rolcreatedb - 1] = BoolGetDatum(createdb);
  new_record[Anum_pg_authid_rolcanlogin - 1] = BoolGetDatum(canlogin);
  new_record[Anum_pg_authid_rolreplication - 1] = BoolGetDatum(isreplication);
  new_record[Anum_pg_authid_rolconnlimit - 1] = Int32GetDatum(connlimit);

  if (password)
  {
    char *shadow_pass;
    char *logdetail;

       
                                                                         
                                                                           
                                                                           
                                                                      
                                            
       
                                                                           
                                                                      
                                                                       
                                                                     
       
    if (password[0] == '\0' || plain_crypt_verify(stmt->role, password, "", &logdetail) == STATUS_OK)
    {
      ereport(NOTICE, (errmsg("empty string is not a valid password, clearing password")));
      new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
    }
    else
    {
                                                         
      shadow_pass = encrypt_password(Password_encryption, stmt->role, password);
      new_record[Anum_pg_authid_rolpassword - 1] = CStringGetTextDatum(shadow_pass);
    }
  }
  else
  {
    new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
  }

  new_record[Anum_pg_authid_rolvaliduntil - 1] = validUntil_datum;
  new_record_nulls[Anum_pg_authid_rolvaliduntil - 1] = validUntil_null;

  new_record[Anum_pg_authid_rolbypassrls - 1] = BoolGetDatum(bypassrls);

     
                                                                     
                              
     
  if (IsBinaryUpgrade)
  {
    if (!OidIsValid(binary_upgrade_next_pg_authid_oid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_authid OID value not set when in binary upgrade mode")));
    }

    roleid = binary_upgrade_next_pg_authid_oid;
    binary_upgrade_next_pg_authid_oid = InvalidOid;
  }
  else
  {
    roleid = GetNewOidWithIndex(pg_authid_rel, AuthIdOidIndexId, Anum_pg_authid_oid);
  }

  new_record[Anum_pg_authid_oid - 1] = ObjectIdGetDatum(roleid);

  tuple = heap_form_tuple(pg_authid_dsc, new_record, new_record_nulls);

     
                                              
     
  CatalogTupleInsert(pg_authid_rel, tuple);

     
                                                                     
                           
     
  if (addroleto || adminmembers || rolemembers)
  {
    CommandCounterIncrement();
  }

     
                                                       
     
  if (addroleto)
  {
    RoleSpec *thisrole = makeNode(RoleSpec);
    List *thisrole_list = list_make1(thisrole);
    List *thisrole_oidlist = list_make1_oid(roleid);

    thisrole->roletype = ROLESPEC_CSTRING;
    thisrole->rolename = stmt->role;
    thisrole->location = -1;

    foreach (item, addroleto)
    {
      RoleSpec *oldrole = lfirst(item);
      HeapTuple oldroletup = get_rolespec_tuple(oldrole);
      Form_pg_authid oldroleform = (Form_pg_authid)GETSTRUCT(oldroletup);
      Oid oldroleid = oldroleform->oid;
      char *oldrolename = NameStr(oldroleform->rolname);

      AddRoleMems(oldrolename, oldroleid, thisrole_list, thisrole_oidlist, GetUserId(), false);

      ReleaseSysCache(oldroletup);
    }
  }

     
                                                                            
                                
     
  AddRoleMems(stmt->role, roleid, adminmembers, roleSpecsToIds(adminmembers), GetUserId(), true);
  AddRoleMems(stmt->role, roleid, rolemembers, roleSpecsToIds(rolemembers), GetUserId(), false);

                                       
  InvokeObjectPostCreateHook(AuthIdRelationId, roleid, 0);

     
                                                 
     
  table_close(pg_authid_rel, NoLock);

  return roleid;
}

   
              
   
                                                                         
                                                                          
                                                           
   
Oid
AlterRole(AlterRoleStmt *stmt)
{
  Datum new_record[Natts_pg_authid];
  bool new_record_nulls[Natts_pg_authid];
  bool new_record_repl[Natts_pg_authid];
  Relation pg_authid_rel;
  TupleDesc pg_authid_dsc;
  HeapTuple tuple, new_tuple;
  Form_pg_authid authform;
  ListCell *option;
  char *rolename = NULL;
  char *password = NULL;                      
  int issuper = -1;                                        
  int inherit = -1;                                      
  int createrole = -1;                                      
  int createdb = -1;                                           
  int canlogin = -1;                                 
  int isreplication = -1;                                   
  int connlimit = -1;                                       
  List *rolemembers = NIL;                                
  char *validUntil = NULL;                                    
  Datum validUntil_datum;                                  
  bool validUntil_null;
  int bypassrls = -1;
  DefElem *dpassword = NULL;
  DefElem *dissuper = NULL;
  DefElem *dinherit = NULL;
  DefElem *dcreaterole = NULL;
  DefElem *dcreatedb = NULL;
  DefElem *dcanlogin = NULL;
  DefElem *disreplication = NULL;
  DefElem *dconnlimit = NULL;
  DefElem *drolemembers = NULL;
  DefElem *dvalidUntil = NULL;
  DefElem *dbypassRLS = NULL;
  Oid roleid;

  check_rolespec_name(stmt->role, _("Cannot alter reserved roles."));

                                                    
  foreach (option, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(option);

    if (strcmp(defel->defname, "password") == 0)
    {
      if (dpassword)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dpassword = defel;
    }
    else if (strcmp(defel->defname, "superuser") == 0)
    {
      if (dissuper)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dissuper = defel;
    }
    else if (strcmp(defel->defname, "inherit") == 0)
    {
      if (dinherit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dinherit = defel;
    }
    else if (strcmp(defel->defname, "createrole") == 0)
    {
      if (dcreaterole)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dcreaterole = defel;
    }
    else if (strcmp(defel->defname, "createdb") == 0)
    {
      if (dcreatedb)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dcreatedb = defel;
    }
    else if (strcmp(defel->defname, "canlogin") == 0)
    {
      if (dcanlogin)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dcanlogin = defel;
    }
    else if (strcmp(defel->defname, "isreplication") == 0)
    {
      if (disreplication)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      disreplication = defel;
    }
    else if (strcmp(defel->defname, "connectionlimit") == 0)
    {
      if (dconnlimit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dconnlimit = defel;
    }
    else if (strcmp(defel->defname, "rolemembers") == 0 && stmt->action != 0)
    {
      if (drolemembers)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      drolemembers = defel;
    }
    else if (strcmp(defel->defname, "validUntil") == 0)
    {
      if (dvalidUntil)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dvalidUntil = defel;
    }
    else if (strcmp(defel->defname, "bypassrls") == 0)
    {
      if (dbypassRLS)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      dbypassRLS = defel;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

  if (dpassword && dpassword->arg)
  {
    password = strVal(dpassword->arg);
  }
  if (dissuper)
  {
    issuper = intVal(dissuper->arg);
  }
  if (dinherit)
  {
    inherit = intVal(dinherit->arg);
  }
  if (dcreaterole)
  {
    createrole = intVal(dcreaterole->arg);
  }
  if (dcreatedb)
  {
    createdb = intVal(dcreatedb->arg);
  }
  if (dcanlogin)
  {
    canlogin = intVal(dcanlogin->arg);
  }
  if (disreplication)
  {
    isreplication = intVal(disreplication->arg);
  }
  if (dconnlimit)
  {
    connlimit = intVal(dconnlimit->arg);
    if (connlimit < -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid connection limit: %d", connlimit)));
    }
  }
  if (drolemembers)
  {
    rolemembers = (List *)drolemembers->arg;
  }
  if (dvalidUntil)
  {
    validUntil = strVal(dvalidUntil->arg);
  }
  if (dbypassRLS)
  {
    bypassrls = intVal(dbypassRLS->arg);
  }

     
                                                                
     
  pg_authid_rel = table_open(AuthIdRelationId, RowExclusiveLock);
  pg_authid_dsc = RelationGetDescr(pg_authid_rel);

  tuple = get_rolespec_tuple(stmt->role);
  authform = (Form_pg_authid)GETSTRUCT(tuple);
  rolename = pstrdup(NameStr(authform->rolname));
  roleid = authform->oid;

     
                                                                          
                                                                     
                                                                             
                                  
     
  if (authform->rolsuper || issuper >= 0)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter superusers")));
    }
  }
  else if (authform->rolreplication || isreplication >= 0)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter replication users")));
    }
  }
  else if (bypassrls >= 0)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to change bypassrls attribute")));
    }
  }
  else if (!have_createrole_privilege())
  {
                                                                  
    if (!(inherit < 0 && createrole < 0 && createdb < 0 && canlogin < 0 && !dconnlimit && !rolemembers && !validUntil && dpassword && roleid == GetUserId()))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied")));
    }
  }

                                           
  if (validUntil)
  {
    validUntil_datum = DirectFunctionCall3(timestamptz_in, CStringGetDatum(validUntil), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
    validUntil_null = false;
  }
  else
  {
                                                      
    validUntil_datum = SysCacheGetAttr(AUTHNAME, tuple, Anum_pg_authid_rolvaliduntil, &validUntil_null);
  }

     
                                                             
     
  if (check_password_hook && password)
  {
    (*check_password_hook)(rolename, password, get_password_type(password), validUntil_datum, validUntil_null);
  }

     
                                                                    
     
  MemSet(new_record, 0, sizeof(new_record));
  MemSet(new_record_nulls, false, sizeof(new_record_nulls));
  MemSet(new_record_repl, false, sizeof(new_record_repl));

     
                            
     
  if (issuper >= 0)
  {
    new_record[Anum_pg_authid_rolsuper - 1] = BoolGetDatum(issuper > 0);
    new_record_repl[Anum_pg_authid_rolsuper - 1] = true;
  }

  if (inherit >= 0)
  {
    new_record[Anum_pg_authid_rolinherit - 1] = BoolGetDatum(inherit > 0);
    new_record_repl[Anum_pg_authid_rolinherit - 1] = true;
  }

  if (createrole >= 0)
  {
    new_record[Anum_pg_authid_rolcreaterole - 1] = BoolGetDatum(createrole > 0);
    new_record_repl[Anum_pg_authid_rolcreaterole - 1] = true;
  }

  if (createdb >= 0)
  {
    new_record[Anum_pg_authid_rolcreatedb - 1] = BoolGetDatum(createdb > 0);
    new_record_repl[Anum_pg_authid_rolcreatedb - 1] = true;
  }

  if (canlogin >= 0)
  {
    new_record[Anum_pg_authid_rolcanlogin - 1] = BoolGetDatum(canlogin > 0);
    new_record_repl[Anum_pg_authid_rolcanlogin - 1] = true;
  }

  if (isreplication >= 0)
  {
    new_record[Anum_pg_authid_rolreplication - 1] = BoolGetDatum(isreplication > 0);
    new_record_repl[Anum_pg_authid_rolreplication - 1] = true;
  }

  if (dconnlimit)
  {
    new_record[Anum_pg_authid_rolconnlimit - 1] = Int32GetDatum(connlimit);
    new_record_repl[Anum_pg_authid_rolconnlimit - 1] = true;
  }

                
  if (password)
  {
    char *shadow_pass;
    char *logdetail;

                                                             
    if (password[0] == '\0' || plain_crypt_verify(rolename, password, "", &logdetail) == STATUS_OK)
    {
      ereport(NOTICE, (errmsg("empty string is not a valid password, clearing password")));
      new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
    }
    else
    {
                                                         
      shadow_pass = encrypt_password(Password_encryption, rolename, password);
      new_record[Anum_pg_authid_rolpassword - 1] = CStringGetTextDatum(shadow_pass);
    }
    new_record_repl[Anum_pg_authid_rolpassword - 1] = true;
  }

                      
  if (dpassword && dpassword->arg == NULL)
  {
    new_record_repl[Anum_pg_authid_rolpassword - 1] = true;
    new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
  }

                   
  new_record[Anum_pg_authid_rolvaliduntil - 1] = validUntil_datum;
  new_record_nulls[Anum_pg_authid_rolvaliduntil - 1] = validUntil_null;
  new_record_repl[Anum_pg_authid_rolvaliduntil - 1] = true;

  if (bypassrls >= 0)
  {
    new_record[Anum_pg_authid_rolbypassrls - 1] = BoolGetDatum(bypassrls > 0);
    new_record_repl[Anum_pg_authid_rolbypassrls - 1] = true;
  }

  new_tuple = heap_modify_tuple(tuple, pg_authid_dsc, new_record, new_record_nulls, new_record_repl);
  CatalogTupleUpdate(pg_authid_rel, &tuple->t_self, new_tuple);

  InvokeObjectPostAlterHook(AuthIdRelationId, roleid, 0);

  ReleaseSysCache(tuple);
  heap_freetuple(new_tuple);

     
                                                                     
                           
     
  if (rolemembers)
  {
    CommandCounterIncrement();
  }

  if (stmt->action == +1)                          
  {
    AddRoleMems(rolename, roleid, rolemembers, roleSpecsToIds(rolemembers), GetUserId(), false);
  }
  else if (stmt->action == -1)                             
  {
    DelRoleMems(rolename, roleid, rolemembers, roleSpecsToIds(rolemembers), false);
  }

     
                                                 
     
  table_close(pg_authid_rel, NoLock);

  return roleid;
}

   
                      
   
Oid
AlterRoleSet(AlterRoleSetStmt *stmt)
{
  HeapTuple roletuple;
  Form_pg_authid roleform;
  Oid databaseid = InvalidOid;
  Oid roleid = InvalidOid;

  if (stmt->role)
  {
    check_rolespec_name(stmt->role, _("Cannot alter reserved roles."));

    roletuple = get_rolespec_tuple(stmt->role);
    roleform = (Form_pg_authid)GETSTRUCT(roletuple);
    roleid = roleform->oid;

       
                                                                        
                 
       
    shdepLockAndCheckObject(AuthIdRelationId, roleid);

       
                                                                      
                                                            
       
    if (roleform->rolsuper)
    {
      if (!superuser())
      {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter superusers")));
      }
    }
    else
    {
      if (!have_createrole_privilege() && roleid != GetUserId())
      {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied")));
      }
    }

    ReleaseSysCache(roletuple);
  }

                                                   
  if (stmt->database != NULL)
  {
    databaseid = get_database_oid(stmt->database, false);
    shdepLockAndCheckObject(DatabaseRelationId, databaseid);

    if (!stmt->role)
    {
         
                                                                       
                                                                   
         
      if (!pg_database_ownercheck(databaseid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, stmt->database);
      }
    }
  }

  if (!stmt->role && !stmt->database)
  {
                                                       
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter settings globally")));
    }
  }

  AlterSetting(databaseid, roleid, stmt->setstmt);

  return roleid;
}

   
             
   
void
DropRole(DropRoleStmt *stmt)
{
  Relation pg_authid_rel, pg_auth_members_rel;
  ListCell *item;

  if (!have_createrole_privilege())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to drop role")));
  }

     
                                                                      
              
     
  pg_authid_rel = table_open(AuthIdRelationId, RowExclusiveLock);
  pg_auth_members_rel = table_open(AuthMemRelationId, RowExclusiveLock);

  foreach (item, stmt->roles)
  {
    RoleSpec *rolspec = lfirst(item);
    char *role;
    HeapTuple tuple, tmp_tuple;
    Form_pg_authid roleform;
    ScanKeyData scankey;
    char *detail;
    char *detail_log;
    SysScanDesc sscan;
    Oid roleid;

    if (rolspec->roletype != ROLESPEC_CSTRING)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot use special role specifier in DROP ROLE")));
    }
    role = rolspec->rolename;

    tuple = SearchSysCache1(AUTHNAME, PointerGetDatum(role));
    if (!HeapTupleIsValid(tuple))
    {
      if (!stmt->missing_ok)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", role)));
      }
      else
      {
        ereport(NOTICE, (errmsg("role \"%s\" does not exist, skipping", role)));
      }

      continue;
    }

    roleform = (Form_pg_authid)GETSTRUCT(tuple);
    roleid = roleform->oid;

    if (roleid == GetUserId())
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("current user cannot be dropped")));
    }
    if (roleid == GetOuterUserId())
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("current user cannot be dropped")));
    }
    if (roleid == GetSessionUserId())
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("session user cannot be dropped")));
    }

       
                                                                       
                                                                   
                                                                
       
    if (roleform->rolsuper && !superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to drop superusers")));
    }

                                              
    InvokeObjectDropHook(AuthIdRelationId, roleid, 0);

       
                                                                          
                                                            
       
    LockSharedObject(AuthIdRelationId, roleid, 0, AccessExclusiveLock);

                                                              
    if (checkSharedDependencies(AuthIdRelationId, roleid, &detail, &detail_log))
    {
      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("role \"%s\" cannot be dropped because some objects depend on it", role), errdetail_internal("%s", detail), errdetail_log("%s", detail_log)));
    }

       
                                                
       
    CatalogTupleDelete(pg_authid_rel, &tuple->t_self);

    ReleaseSysCache(tuple);

       
                                                                          
                                                         
       
                                                                         
       
    ScanKeyInit(&scankey, Anum_pg_auth_members_roleid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));

    sscan = systable_beginscan(pg_auth_members_rel, AuthMemRoleMemIndexId, true, NULL, 1, &scankey);

    while (HeapTupleIsValid(tmp_tuple = systable_getnext(sscan)))
    {
      CatalogTupleDelete(pg_auth_members_rel, &tmp_tuple->t_self);
    }

    systable_endscan(sscan);

    ScanKeyInit(&scankey, Anum_pg_auth_members_member, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));

    sscan = systable_beginscan(pg_auth_members_rel, AuthMemMemRoleIndexId, true, NULL, 1, &scankey);

    while (HeapTupleIsValid(tmp_tuple = systable_getnext(sscan)))
    {
      CatalogTupleDelete(pg_auth_members_rel, &tmp_tuple->t_self);
    }

    systable_endscan(sscan);

       
                                                            
       
    DeleteSharedComments(roleid, AuthIdRelationId);
    DeleteSharedSecurityLabel(roleid, AuthIdRelationId);

       
                                      
       
    DropSetting(InvalidOid, roleid);

       
                                                                          
                                                                         
                                                                           
                                                                          
                                                                        
                                                                      
                
       
    CommandCounterIncrement();
  }

     
                                                       
     
  table_close(pg_auth_members_rel, NoLock);
  table_close(pg_authid_rel, NoLock);
}

   
               
   
ObjectAddress
RenameRole(const char *oldname, const char *newname)
{
  HeapTuple oldtuple, newtuple;
  TupleDesc dsc;
  Relation rel;
  Datum datum;
  bool isnull;
  Datum repl_val[Natts_pg_authid];
  bool repl_null[Natts_pg_authid];
  bool repl_repl[Natts_pg_authid];
  int i;
  Oid roleid;
  ObjectAddress address;
  Form_pg_authid authform;

  rel = table_open(AuthIdRelationId, RowExclusiveLock);
  dsc = RelationGetDescr(rel);

  oldtuple = SearchSysCache1(AUTHNAME, CStringGetDatum(oldname));
  if (!HeapTupleIsValid(oldtuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", oldname)));
  }

     
                                                                           
                                                                             
                                                                           
                                                                           
                               
     

  authform = (Form_pg_authid)GETSTRUCT(oldtuple);
  roleid = authform->oid;

  if (roleid == GetSessionUserId())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("session user cannot be renamed")));
  }
  if (roleid == GetOuterUserId())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("current user cannot be renamed")));
  }

     
                                                                       
                                                                
     
  if (IsReservedName(NameStr(authform->rolname)))
  {
    ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("role name \"%s\" is reserved", NameStr(authform->rolname)), errdetail("Role names starting with \"pg_\" are reserved.")));
  }

  if (IsReservedName(newname))
  {
    ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("role name \"%s\" is reserved", newname), errdetail("Role names starting with \"pg_\" are reserved.")));
  }

     
                                                                     
                                              
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strncmp(newname, "regress_", 8) != 0)
  {
    elog(WARNING, "roles created by regression test cases should have names starting with \"regress_\"");
  }
#endif

                                            
  if (SearchSysCacheExists1(AUTHNAME, CStringGetDatum(newname)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("role \"%s\" already exists", newname)));
  }

     
                                                                             
     
  if (((Form_pg_authid)GETSTRUCT(oldtuple))->rolsuper)
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to rename superusers")));
    }
  }
  else
  {
    if (!have_createrole_privilege())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to rename role")));
    }
  }

                                        
  for (i = 0; i < Natts_pg_authid; i++)
  {
    repl_repl[i] = false;
  }

  repl_repl[Anum_pg_authid_rolname - 1] = true;
  repl_val[Anum_pg_authid_rolname - 1] = DirectFunctionCall1(namein, CStringGetDatum(newname));
  repl_null[Anum_pg_authid_rolname - 1] = false;

  datum = heap_getattr(oldtuple, Anum_pg_authid_rolpassword, dsc, &isnull);

  if (!isnull && get_password_type(TextDatumGetCString(datum)) == PASSWORD_TYPE_MD5)
  {
                                                                     
    repl_repl[Anum_pg_authid_rolpassword - 1] = true;
    repl_null[Anum_pg_authid_rolpassword - 1] = true;

    ereport(NOTICE, (errmsg("MD5 password cleared because of role rename")));
  }

  newtuple = heap_modify_tuple(oldtuple, dsc, repl_val, repl_null, repl_repl);
  CatalogTupleUpdate(rel, &oldtuple->t_self, newtuple);

  InvokeObjectPostAlterHook(AuthIdRelationId, roleid, 0);

  ObjectAddressSet(address, AuthIdRelationId, roleid);

  ReleaseSysCache(oldtuple);

     
                                                 
     
  table_close(rel, NoLock);

  return address;
}

   
                 
   
                                    
   
void
GrantRole(GrantRoleStmt *stmt)
{
  Relation pg_authid_rel;
  Oid grantor;
  List *grantee_ids;
  ListCell *item;

  if (stmt->grantor)
  {
    grantor = get_rolespec_oid(stmt->grantor, false);
  }
  else
  {
    grantor = GetUserId();
  }

  grantee_ids = roleSpecsToIds(stmt->grantee_roles);

                                                                     
  pg_authid_rel = table_open(AuthIdRelationId, AccessShareLock);

     
                                                                          
                                                                       
             
     
                                                                   
     
  foreach (item, stmt->granted_roles)
  {
    AccessPriv *priv = (AccessPriv *)lfirst(item);
    char *rolename = priv->priv_name;
    Oid roleid;

                                                               
    if (rolename == NULL || priv->cols != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("column names cannot be included in GRANT/REVOKE ROLE")));
    }

    roleid = get_role_oid(rolename, false);
    if (stmt->is_grant)
    {
      AddRoleMems(rolename, roleid, stmt->grantee_roles, grantee_ids, grantor, stmt->admin_opt);
    }
    else
    {
      DelRoleMems(rolename, roleid, stmt->grantee_roles, grantee_ids, stmt->admin_opt);
    }
  }

     
                                                 
     
  table_close(pg_authid_rel, NoLock);
}

   
                    
   
                                                    
   
void
DropOwnedObjects(DropOwnedStmt *stmt)
{
  List *role_ids = roleSpecsToIds(stmt->roles);
  ListCell *cell;

                        
  foreach (cell, role_ids)
  {
    Oid roleid = lfirst_oid(cell);

    if (!has_privs_of_role(GetUserId(), roleid))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to drop objects")));
    }
  }

                 
  shdepDropOwned(role_ids, stmt->behavior);
}

   
                        
   
                                                                         
   
void
ReassignOwnedObjects(ReassignOwnedStmt *stmt)
{
  List *role_ids = roleSpecsToIds(stmt->roles);
  ListCell *cell;
  Oid newrole;

                        
  foreach (cell, role_ids)
  {
    Oid roleid = lfirst_oid(cell);

    if (!has_privs_of_role(GetUserId(), roleid))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to reassign objects")));
    }
  }

                                                      
  newrole = get_rolespec_oid(stmt->newrole, false);

  if (!has_privs_of_role(GetUserId(), newrole))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to reassign objects")));
  }

                 
  shdepReassignOwned(role_ids, newrole);
}

   
                  
   
                                                                              
   
                                   
   
List *
roleSpecsToIds(List *memberNames)
{
  List *result = NIL;
  ListCell *l;

  foreach (l, memberNames)
  {
    RoleSpec *rolespec = lfirst_node(RoleSpec, l);
    Oid roleid;

    roleid = get_rolespec_oid(rolespec, false);
    result = lappend_oid(result, roleid);
  }
  return result;
}

   
                                                          
   
                                                                   
                                 
                                                                                
                                   
                                             
                                     
   
                                                                      
   
static void
AddRoleMems(const char *rolename, Oid roleid, List *memberSpecs, List *memberIds, Oid grantorId, bool admin_opt)
{
  Relation pg_authmem_rel;
  TupleDesc pg_authmem_dsc;
  ListCell *specitem;
  ListCell *iditem;

  Assert(list_length(memberSpecs) == list_length(memberIds));

                                              
  if (!memberIds)
  {
    return;
  }

     
                                                                            
                                                                         
     
  if (superuser_arg(roleid))
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter superusers")));
    }
  }
  else
  {
    if (!have_createrole_privilege() && !is_admin_of_role(grantorId, roleid))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must have admin option on role \"%s\"", rolename)));
    }
  }

     
                                                                      
                                                                           
                                                                         
     
                                                                      
                                                                           
                                                                            
                              
     
  if (grantorId != GetUserId() && !superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to set grantor")));
  }

  pg_authmem_rel = table_open(AuthMemRelationId, RowExclusiveLock);
  pg_authmem_dsc = RelationGetDescr(pg_authmem_rel);

  forboth(specitem, memberSpecs, iditem, memberIds)
  {
    RoleSpec *memberRole = lfirst_node(RoleSpec, specitem);
    Oid memberid = lfirst_oid(iditem);
    HeapTuple authmem_tuple;
    HeapTuple tuple;
    Datum new_record[Natts_pg_auth_members];
    bool new_record_nulls[Natts_pg_auth_members];
    bool new_record_repl[Natts_pg_auth_members];

       
                                                                       
                                                                           
                                                                         
                                                                         
                                                                    
       
    if (is_member_of_role_nosuper(roleid, memberid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), (errmsg("role \"%s\" is a member of role \"%s\"", rolename, get_rolespec_name(memberRole)))));
    }

       
                                                                       
                                                  
       
    authmem_tuple = SearchSysCache2(AUTHMEMROLEMEM, ObjectIdGetDatum(roleid), ObjectIdGetDatum(memberid));
    if (HeapTupleIsValid(authmem_tuple) && (!admin_opt || ((Form_pg_auth_members)GETSTRUCT(authmem_tuple))->admin_option))
    {
      ereport(NOTICE, (errmsg("role \"%s\" is already a member of role \"%s\"", get_rolespec_name(memberRole), rolename)));
      ReleaseSysCache(authmem_tuple);
      continue;
    }

                                           
    MemSet(new_record, 0, sizeof(new_record));
    MemSet(new_record_nulls, false, sizeof(new_record_nulls));
    MemSet(new_record_repl, false, sizeof(new_record_repl));

    new_record[Anum_pg_auth_members_roleid - 1] = ObjectIdGetDatum(roleid);
    new_record[Anum_pg_auth_members_member - 1] = ObjectIdGetDatum(memberid);
    new_record[Anum_pg_auth_members_grantor - 1] = ObjectIdGetDatum(grantorId);
    new_record[Anum_pg_auth_members_admin_option - 1] = BoolGetDatum(admin_opt);

    if (HeapTupleIsValid(authmem_tuple))
    {
      new_record_repl[Anum_pg_auth_members_grantor - 1] = true;
      new_record_repl[Anum_pg_auth_members_admin_option - 1] = true;
      tuple = heap_modify_tuple(authmem_tuple, pg_authmem_dsc, new_record, new_record_nulls, new_record_repl);
      CatalogTupleUpdate(pg_authmem_rel, &tuple->t_self, tuple);
      ReleaseSysCache(authmem_tuple);
    }
    else
    {
      tuple = heap_form_tuple(pg_authmem_dsc, new_record, new_record_nulls);
      CatalogTupleInsert(pg_authmem_rel, tuple);
    }

                                                                     
    CommandCounterIncrement();
  }

     
                                                  
     
  table_close(pg_authmem_rel, NoLock);
}

   
                                                               
   
                                                                     
                                   
                                                                                
                                   
                                        
   
                                                                      
   
static void
DelRoleMems(const char *rolename, Oid roleid, List *memberSpecs, List *memberIds, bool admin_opt)
{
  Relation pg_authmem_rel;
  TupleDesc pg_authmem_dsc;
  ListCell *specitem;
  ListCell *iditem;

  Assert(list_length(memberSpecs) == list_length(memberIds));

                                              
  if (!memberIds)
  {
    return;
  }

     
                                                                            
                                                                         
     
  if (superuser_arg(roleid))
  {
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter superusers")));
    }
  }
  else
  {
    if (!have_createrole_privilege() && !is_admin_of_role(GetUserId(), roleid))
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must have admin option on role \"%s\"", rolename)));
    }
  }

  pg_authmem_rel = table_open(AuthMemRelationId, RowExclusiveLock);
  pg_authmem_dsc = RelationGetDescr(pg_authmem_rel);

  forboth(specitem, memberSpecs, iditem, memberIds)
  {
    RoleSpec *memberRole = lfirst(specitem);
    Oid memberid = lfirst_oid(iditem);
    HeapTuple authmem_tuple;

       
                                       
       
    authmem_tuple = SearchSysCache2(AUTHMEMROLEMEM, ObjectIdGetDatum(roleid), ObjectIdGetDatum(memberid));
    if (!HeapTupleIsValid(authmem_tuple))
    {
      ereport(WARNING, (errmsg("role \"%s\" is not a member of role \"%s\"", get_rolespec_name(memberRole), rolename)));
      continue;
    }

    if (!admin_opt)
    {
                                       
      CatalogTupleDelete(pg_authmem_rel, &authmem_tuple->t_self);
    }
    else
    {
                                          
      HeapTuple tuple;
      Datum new_record[Natts_pg_auth_members];
      bool new_record_nulls[Natts_pg_auth_members];
      bool new_record_repl[Natts_pg_auth_members];

                                        
      MemSet(new_record, 0, sizeof(new_record));
      MemSet(new_record_nulls, false, sizeof(new_record_nulls));
      MemSet(new_record_repl, false, sizeof(new_record_repl));

      new_record[Anum_pg_auth_members_admin_option - 1] = BoolGetDatum(false);
      new_record_repl[Anum_pg_auth_members_admin_option - 1] = true;

      tuple = heap_modify_tuple(authmem_tuple, pg_authmem_dsc, new_record, new_record_nulls, new_record_repl);
      CatalogTupleUpdate(pg_authmem_rel, &tuple->t_self, tuple);
    }

    ReleaseSysCache(authmem_tuple);

                                                                     
    CommandCounterIncrement();
  }

     
                                                  
     
  table_close(pg_authmem_rel, NoLock);
}
