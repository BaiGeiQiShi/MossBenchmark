                                                                            
   
              
                                       
   
                                                                         
                                                                        
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_pltemplate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/proclang.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parser.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  bool tmpltrusted;                  
  bool tmpldbacreate;                                   
  char *tmplhandler;                                 
  char *tmplinline;                                                  
  char *tmplvalidator;                                          
  char *tmpllibrary;                               
} PLTemplate;

static ObjectAddress
create_proc_lang(const char *languageName, bool replace, Oid languageOwner, Oid handlerOid, Oid inlineOid, Oid valOid, bool trusted);
static PLTemplate *
find_language_template(const char *languageName);

   
                   
   
ObjectAddress
CreateProceduralLanguage(CreatePLangStmt *stmt)
{
  PLTemplate *pltemplate;
  ObjectAddress tmpAddr;
  Oid handlerOid, inlineOid, valOid;
  Oid funcrettype;
  Oid funcargtypes[1];

     
                                                                           
                                                           
     
  if ((pltemplate = find_language_template(stmt->plname)) != NULL)
  {
    List *funcname;

       
                                                             
       
    if (stmt->plhandler)
    {
      ereport(NOTICE, (errmsg("using pg_pltemplate information instead of CREATE LANGUAGE parameters")));
    }

       
                        
       
    if (!superuser())
    {
      if (!pltemplate->tmpldbacreate)
      {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create procedural language \"%s\"", stmt->plname)));
      }
      if (!pg_database_ownercheck(MyDatabaseId, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, get_database_name(MyDatabaseId));
      }
    }

       
                                                                        
                                                                        
                    
       
    funcname = SystemFuncName(pltemplate->tmplhandler);
    handlerOid = LookupFuncName(funcname, 0, funcargtypes, true);
    if (OidIsValid(handlerOid))
    {
      funcrettype = get_func_rettype(handlerOid);
      if (funcrettype != LANGUAGE_HANDLEROID)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("function %s must return type %s", NameListToString(funcname), "language_handler")));
      }
    }
    else
    {
      tmpAddr = ProcedureCreate(pltemplate->tmplhandler, PG_CATALOG_NAMESPACE, false,                                                                                          
          false,                                                                                                                                                                  
          LANGUAGE_HANDLEROID, BOOTSTRAP_SUPERUSERID, ClanguageId, F_FMGR_C_VALIDATOR, pltemplate->tmplhandler, pltemplate->tmpllibrary, PROKIND_FUNCTION, false,                       
          false,                                                                                                                                                                   
          false,                                                                                                                                                                
          PROVOLATILE_VOLATILE, PROPARALLEL_UNSAFE, buildoidvector(funcargtypes, 0), PointerGetDatum(NULL), PointerGetDatum(NULL), PointerGetDatum(NULL), NIL, PointerGetDatum(NULL), PointerGetDatum(NULL), InvalidOid, 1, 0);
      handlerOid = tmpAddr.objectId;
    }

       
                                                                           
                                   
       
    if (pltemplate->tmplinline)
    {
      funcname = SystemFuncName(pltemplate->tmplinline);
      funcargtypes[0] = INTERNALOID;
      inlineOid = LookupFuncName(funcname, 1, funcargtypes, true);
      if (!OidIsValid(inlineOid))
      {
        tmpAddr = ProcedureCreate(pltemplate->tmplinline, PG_CATALOG_NAMESPACE, false,                                                                              
            false,                                                                                                                                                     
            VOIDOID, BOOTSTRAP_SUPERUSERID, ClanguageId, F_FMGR_C_VALIDATOR, pltemplate->tmplinline, pltemplate->tmpllibrary, PROKIND_FUNCTION, false,                       
            false,                                                                                                                                                      
            true,                                                                                                                                                    
            PROVOLATILE_VOLATILE, PROPARALLEL_UNSAFE, buildoidvector(funcargtypes, 1), PointerGetDatum(NULL), PointerGetDatum(NULL), PointerGetDatum(NULL), NIL, PointerGetDatum(NULL), PointerGetDatum(NULL), InvalidOid, 1, 0);
        inlineOid = tmpAddr.objectId;
      }
    }
    else
    {
      inlineOid = InvalidOid;
    }

       
                                                                        
                        
       
    if (pltemplate->tmplvalidator)
    {
      funcname = SystemFuncName(pltemplate->tmplvalidator);
      funcargtypes[0] = OIDOID;
      valOid = LookupFuncName(funcname, 1, funcargtypes, true);
      if (!OidIsValid(valOid))
      {
        tmpAddr = ProcedureCreate(pltemplate->tmplvalidator, PG_CATALOG_NAMESPACE, false,                                                                              
            false,                                                                                                                                                        
            VOIDOID, BOOTSTRAP_SUPERUSERID, ClanguageId, F_FMGR_C_VALIDATOR, pltemplate->tmplvalidator, pltemplate->tmpllibrary, PROKIND_FUNCTION, false,                       
            false,                                                                                                                                                         
            true,                                                                                                                                                       
            PROVOLATILE_VOLATILE, PROPARALLEL_UNSAFE, buildoidvector(funcargtypes, 1), PointerGetDatum(NULL), PointerGetDatum(NULL), PointerGetDatum(NULL), NIL, PointerGetDatum(NULL), PointerGetDatum(NULL), InvalidOid, 1, 0);
        valOid = tmpAddr.objectId;
      }
    }
    else
    {
      valOid = InvalidOid;
    }

                       
    return create_proc_lang(stmt->plname, stmt->replace, GetUserId(), handlerOid, inlineOid, valOid, pltemplate->tmpltrusted);
  }
  else
  {
       
                                                                    
                                                                        
                                            
       
    if (!stmt->plhandler)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("unsupported language \"%s\"", stmt->plname), errhint("The supported languages are listed in the pg_pltemplate system catalog.")));
    }

       
                        
       
    if (!superuser())
    {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create custom procedural language")));
    }

       
                                                                           
                   
       
    handlerOid = LookupFuncName(stmt->plhandler, 0, funcargtypes, false);
    funcrettype = get_func_rettype(handlerOid);
    if (funcrettype != LANGUAGE_HANDLEROID)
    {
         
                                                                      
                                                              
                                                                       
         
      if (funcrettype == OPAQUEOID)
      {
        ereport(WARNING, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("changing return type of function %s from %s to %s", NameListToString(stmt->plhandler), "opaque", "language_handler")));
        SetFunctionReturnType(handlerOid, LANGUAGE_HANDLEROID);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("function %s must return type %s", NameListToString(stmt->plhandler), "language_handler")));
      }
    }

                                      
    if (stmt->plinline)
    {
      funcargtypes[0] = INTERNALOID;
      inlineOid = LookupFuncName(stmt->plinline, 1, funcargtypes, false);
                                                               
    }
    else
    {
      inlineOid = InvalidOid;
    }

                                         
    if (stmt->plvalidator)
    {
      funcargtypes[0] = OIDOID;
      valOid = LookupFuncName(stmt->plvalidator, 1, funcargtypes, false);
                                                               
    }
    else
    {
      valOid = InvalidOid;
    }

                       
    return create_proc_lang(stmt->plname, stmt->replace, GetUserId(), handlerOid, inlineOid, valOid, stmt->pltrusted);
  }
}

   
                              
   
static ObjectAddress
create_proc_lang(const char *languageName, bool replace, Oid languageOwner, Oid handlerOid, Oid inlineOid, Oid valOid, bool trusted)
{
  Relation rel;
  TupleDesc tupDesc;
  Datum values[Natts_pg_language];
  bool nulls[Natts_pg_language];
  bool replaces[Natts_pg_language];
  NameData langname;
  HeapTuple oldtup;
  HeapTuple tup;
  Oid langoid;
  bool is_update;
  ObjectAddress myself, referenced;

  rel = table_open(LanguageRelationId, RowExclusiveLock);
  tupDesc = RelationGetDescr(rel);

                                   
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));
  memset(replaces, true, sizeof(replaces));

  namestrcpy(&langname, languageName);
  values[Anum_pg_language_lanname - 1] = NameGetDatum(&langname);
  values[Anum_pg_language_lanowner - 1] = ObjectIdGetDatum(languageOwner);
  values[Anum_pg_language_lanispl - 1] = BoolGetDatum(true);
  values[Anum_pg_language_lanpltrusted - 1] = BoolGetDatum(trusted);
  values[Anum_pg_language_lanplcallfoid - 1] = ObjectIdGetDatum(handlerOid);
  values[Anum_pg_language_laninline - 1] = ObjectIdGetDatum(inlineOid);
  values[Anum_pg_language_lanvalidator - 1] = ObjectIdGetDatum(valOid);
  nulls[Anum_pg_language_lanacl - 1] = true;

                                         
  oldtup = SearchSysCache1(LANGNAME, PointerGetDatum(languageName));

  if (HeapTupleIsValid(oldtup))
  {
    Form_pg_language oldform = (Form_pg_language)GETSTRUCT(oldtup);

                                           
    if (!replace)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("language \"%s\" already exists", languageName)));
    }
    if (!pg_language_ownercheck(oldform->oid, languageOwner))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_LANGUAGE, languageName);
    }

       
                                                                   
                                                                     
       
    replaces[Anum_pg_language_oid - 1] = false;
    replaces[Anum_pg_language_lanowner - 1] = false;
    replaces[Anum_pg_language_lanacl - 1] = false;

                        
    tup = heap_modify_tuple(oldtup, tupDesc, values, nulls, replaces);
    CatalogTupleUpdate(rel, &tup->t_self, tup);

    langoid = oldform->oid;
    ReleaseSysCache(oldtup);
    is_update = true;
  }
  else
  {
                                 
    langoid = GetNewOidWithIndex(rel, LanguageOidIndexId, Anum_pg_language_oid);
    values[Anum_pg_language_oid - 1] = ObjectIdGetDatum(langoid);
    tup = heap_form_tuple(tupDesc, values, nulls);
    CatalogTupleInsert(rel, tup);
    is_update = false;
  }

     
                                                                      
                                                                     
                                                                       
                                                                            
     
  myself.classId = LanguageRelationId;
  myself.objectId = langoid;
  myself.objectSubId = 0;

  if (is_update)
  {
    deleteDependencyRecordsFor(myself.classId, myself.objectId, true);
  }

                                       
  if (!is_update)
  {
    recordDependencyOnOwner(myself.classId, myself.objectId, languageOwner);
  }

                               
  recordDependencyOnCurrentExtension(&myself, is_update);

                                             
  referenced.classId = ProcedureRelationId;
  referenced.objectId = handlerOid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                         
  if (OidIsValid(inlineOid))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = inlineOid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                                    
  if (OidIsValid(valOid))
  {
    referenced.classId = ProcedureRelationId;
    referenced.objectId = valOid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                                                      
  InvokeObjectPostCreateHook(LanguageRelationId, myself.objectId, 0);

  table_close(rel, RowExclusiveLock);

  return myself;
}

   
                                                                            
   
static PLTemplate *
find_language_template(const char *languageName)
{
  PLTemplate *result;
  Relation rel;
  SysScanDesc scan;
  ScanKeyData key;
  HeapTuple tup;

  rel = table_open(PLTemplateRelationId, AccessShareLock);

  ScanKeyInit(&key, Anum_pg_pltemplate_tmplname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(languageName));
  scan = systable_beginscan(rel, PLTemplateNameIndexId, true, NULL, 1, &key);

  tup = systable_getnext(scan);
  if (HeapTupleIsValid(tup))
  {
    Form_pg_pltemplate tmpl = (Form_pg_pltemplate)GETSTRUCT(tup);
    Datum datum;
    bool isnull;

    result = (PLTemplate *)palloc0(sizeof(PLTemplate));
    result->tmpltrusted = tmpl->tmpltrusted;
    result->tmpldbacreate = tmpl->tmpldbacreate;

                                                                     
    datum = heap_getattr(tup, Anum_pg_pltemplate_tmplhandler, RelationGetDescr(rel), &isnull);
    if (!isnull)
    {
      result->tmplhandler = TextDatumGetCString(datum);
    }

    datum = heap_getattr(tup, Anum_pg_pltemplate_tmplinline, RelationGetDescr(rel), &isnull);
    if (!isnull)
    {
      result->tmplinline = TextDatumGetCString(datum);
    }

    datum = heap_getattr(tup, Anum_pg_pltemplate_tmplvalidator, RelationGetDescr(rel), &isnull);
    if (!isnull)
    {
      result->tmplvalidator = TextDatumGetCString(datum);
    }

    datum = heap_getattr(tup, Anum_pg_pltemplate_tmpllibrary, RelationGetDescr(rel), &isnull);
    if (!isnull)
    {
      result->tmpllibrary = TextDatumGetCString(datum);
    }

                                                               
    if (!result->tmplhandler || !result->tmpllibrary)
    {
      result = NULL;
    }
  }
  else
  {
    result = NULL;
  }

  systable_endscan(scan);

  table_close(rel, AccessShareLock);

  return result;
}

   
                                                                           
   
bool
PLTemplateExists(const char *languageName)
{
  return (find_language_template(languageName) != NULL);
}

   
                              
   
void
DropProceduralLanguageById(Oid langOid)
{
  Relation rel;
  HeapTuple langTup;

  rel = table_open(LanguageRelationId, RowExclusiveLock);

  langTup = SearchSysCache1(LANGOID, ObjectIdGetDatum(langOid));
  if (!HeapTupleIsValid(langTup))                        
  {
    elog(ERROR, "cache lookup failed for language %u", langOid);
  }

  CatalogTupleDelete(rel, &langTup->t_self);

  ReleaseSysCache(langTup);

  table_close(rel, RowExclusiveLock);
}

   
                                                             
   
                                                                          
                                 
   
Oid
get_language_oid(const char *langname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(LANGNAME, Anum_pg_language_oid, CStringGetDatum(langname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("language \"%s\" does not exist", langname)));
  }
  return oid;
}
