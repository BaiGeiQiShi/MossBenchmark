                                                                            
   
                 
   
                                                
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_config_map.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_func.h"
#include "tsearch/ts_cache.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
MakeConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap);
static void
DropConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap);

                                                                       

   
                                                                    
   
                                                               
   
static Datum
get_ts_parser_func(DefElem *defel, int attnum)
{
  List *funcName = defGetQualifiedName(defel);
  Oid typeId[3];
  Oid retTypeId;
  int nargs;
  Oid procOid;

  retTypeId = INTERNALOID;                       
  typeId[0] = INTERNALOID;
  switch (attnum)
  {
  case Anum_pg_ts_parser_prsstart:
    nargs = 2;
    typeId[1] = INT4OID;
    break;
  case Anum_pg_ts_parser_prstoken:
    nargs = 3;
    typeId[1] = INTERNALOID;
    typeId[2] = INTERNALOID;
    break;
  case Anum_pg_ts_parser_prsend:
    nargs = 1;
    retTypeId = VOIDOID;
    break;
  case Anum_pg_ts_parser_prsheadline:
    nargs = 3;
    typeId[1] = INTERNALOID;
    typeId[2] = TSQUERYOID;
    break;
  case Anum_pg_ts_parser_prslextype:
    nargs = 1;

       
                                                                       
                                                                 
                                                                    
       
    break;
  default:
                            
    elog(ERROR, "unrecognized attribute for text search parser: %d", attnum);
    nargs = 0;                          
  }

  procOid = LookupFuncName(funcName, nargs, typeId, false);
  if (get_func_rettype(procOid) != retTypeId)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("function %s should return type %s", func_signature_string(funcName, nargs, NIL, typeId), format_type_be(retTypeId))));
  }

  return ObjectIdGetDatum(procOid);
}

   
                                                       
   
                                                  
   
static ObjectAddress
makeParserDependencies(HeapTuple tuple)
{
  Form_pg_ts_parser prs = (Form_pg_ts_parser)GETSTRUCT(tuple);
  ObjectAddress myself, referenced;

  myself.classId = TSParserRelationId;
  myself.objectId = prs->oid;
  myself.objectSubId = 0;

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = prs->prsnamespace;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                               
  recordDependencyOnCurrentExtension(&myself, false);

                                 
  referenced.classId = ProcedureRelationId;
  referenced.objectSubId = 0;

  referenced.objectId = prs->prsstart;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  referenced.objectId = prs->prstoken;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  referenced.objectId = prs->prsend;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  referenced.objectId = prs->prslextype;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  if (OidIsValid(prs->prsheadline))
  {
    referenced.objectId = prs->prsheadline;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

  return myself;
}

   
                             
   
ObjectAddress
DefineTSParser(List *names, List *parameters)
{
  char *prsname;
  ListCell *pl;
  Relation prsRel;
  HeapTuple tup;
  Datum values[Natts_pg_ts_parser];
  bool nulls[Natts_pg_ts_parser];
  NameData pname;
  Oid prsOid;
  Oid namespaceoid;
  ObjectAddress address;

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create text search parsers")));
  }

  prsRel = table_open(TSParserRelationId, RowExclusiveLock);

                                                     
  namespaceoid = QualifiedNameGetCreationNamespace(names, &prsname);

                                                   
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  prsOid = GetNewOidWithIndex(prsRel, TSParserOidIndexId, Anum_pg_ts_parser_oid);
  values[Anum_pg_ts_parser_oid - 1] = ObjectIdGetDatum(prsOid);
  namestrcpy(&pname, prsname);
  values[Anum_pg_ts_parser_prsname - 1] = NameGetDatum(&pname);
  values[Anum_pg_ts_parser_prsnamespace - 1] = ObjectIdGetDatum(namespaceoid);

     
                                                                        
     
  foreach (pl, parameters)
  {
    DefElem *defel = (DefElem *)lfirst(pl);

    if (strcmp(defel->defname, "start") == 0)
    {
      values[Anum_pg_ts_parser_prsstart - 1] = get_ts_parser_func(defel, Anum_pg_ts_parser_prsstart);
    }
    else if (strcmp(defel->defname, "gettoken") == 0)
    {
      values[Anum_pg_ts_parser_prstoken - 1] = get_ts_parser_func(defel, Anum_pg_ts_parser_prstoken);
    }
    else if (strcmp(defel->defname, "end") == 0)
    {
      values[Anum_pg_ts_parser_prsend - 1] = get_ts_parser_func(defel, Anum_pg_ts_parser_prsend);
    }
    else if (strcmp(defel->defname, "headline") == 0)
    {
      values[Anum_pg_ts_parser_prsheadline - 1] = get_ts_parser_func(defel, Anum_pg_ts_parser_prsheadline);
    }
    else if (strcmp(defel->defname, "lextypes") == 0)
    {
      values[Anum_pg_ts_parser_prslextype - 1] = get_ts_parser_func(defel, Anum_pg_ts_parser_prslextype);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("text search parser parameter \"%s\" not recognized", defel->defname)));
    }
  }

     
                
     
  if (!OidIsValid(DatumGetObjectId(values[Anum_pg_ts_parser_prsstart - 1])))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search parser start method is required")));
  }

  if (!OidIsValid(DatumGetObjectId(values[Anum_pg_ts_parser_prstoken - 1])))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search parser gettoken method is required")));
  }

  if (!OidIsValid(DatumGetObjectId(values[Anum_pg_ts_parser_prsend - 1])))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search parser end method is required")));
  }

  if (!OidIsValid(DatumGetObjectId(values[Anum_pg_ts_parser_prslextype - 1])))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search parser lextypes method is required")));
  }

     
                        
     
  tup = heap_form_tuple(prsRel->rd_att, values, nulls);

  CatalogTupleInsert(prsRel, tup);

  address = makeParserDependencies(tup);

                                                     
  InvokeObjectPostCreateHook(TSParserRelationId, prsOid, 0);

  heap_freetuple(tup);

  table_close(prsRel, RowExclusiveLock);

  return address;
}

   
                               
   
void
RemoveTSParserById(Oid prsId)
{
  Relation relation;
  HeapTuple tup;

  relation = table_open(TSParserRelationId, RowExclusiveLock);

  tup = SearchSysCache1(TSPARSEROID, ObjectIdGetDatum(prsId));

  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search parser %u", prsId);
  }

  CatalogTupleDelete(relation, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(relation, RowExclusiveLock);
}

                                                                          

   
                                                     
   
                                            
   
static ObjectAddress
makeDictionaryDependencies(HeapTuple tuple)
{
  Form_pg_ts_dict dict = (Form_pg_ts_dict)GETSTRUCT(tuple);
  ObjectAddress myself, referenced;

  myself.classId = TSDictionaryRelationId;
  myself.objectId = dict->oid;
  myself.objectSubId = 0;

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = dict->dictnamespace;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                           
  recordDependencyOnOwner(myself.classId, myself.objectId, dict->dictowner);

                               
  recordDependencyOnCurrentExtension(&myself, false);

                              
  referenced.classId = TSTemplateRelationId;
  referenced.objectId = dict->dicttemplate;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  return myself;
}

   
                                                                       
   
static void
verify_dictoptions(Oid tmplId, List *dictoptions)
{
  HeapTuple tup;
  Form_pg_ts_template tform;
  Oid initmethod;

     
                                                                         
                                                                       
                                                                             
                                                                             
                                                                       
     
  if (!IsUnderPostmaster)
  {
    return;
  }

  tup = SearchSysCache1(TSTEMPLATEOID, ObjectIdGetDatum(tmplId));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for text search template %u", tmplId);
  }
  tform = (Form_pg_ts_template)GETSTRUCT(tup);

  initmethod = tform->tmplinit;

  if (!OidIsValid(initmethod))
  {
                                                          
    if (dictoptions)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("text search template \"%s\" does not accept options", NameStr(tform->tmplname))));
    }
  }
  else
  {
       
                                                                           
                
       
    dictoptions = copyObject(dictoptions);

       
                                                                           
                                                                      
       
    (void)OidFunctionCall1(initmethod, PointerGetDatum(dictoptions));
  }

  ReleaseSysCache(tup);
}

   
                                 
   
ObjectAddress
DefineTSDictionary(List *names, List *parameters)
{
  ListCell *pl;
  Relation dictRel;
  HeapTuple tup;
  Datum values[Natts_pg_ts_dict];
  bool nulls[Natts_pg_ts_dict];
  NameData dname;
  Oid templId = InvalidOid;
  List *dictoptions = NIL;
  Oid dictOid;
  Oid namespaceoid;
  AclResult aclresult;
  char *dictname;
  ObjectAddress address;

                                                     
  namespaceoid = QualifiedNameGetCreationNamespace(names, &dictname);

                                                         
  aclresult = pg_namespace_aclcheck(namespaceoid, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceoid));
  }

     
                                                                        
     
  foreach (pl, parameters)
  {
    DefElem *defel = (DefElem *)lfirst(pl);

    if (strcmp(defel->defname, "template") == 0)
    {
      templId = get_ts_template_oid(defGetQualifiedName(defel), false);
    }
    else
    {
                                                           
      dictoptions = lappend(dictoptions, defel);
    }
  }

     
                
     
  if (!OidIsValid(templId))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search template is required")));
  }

  verify_dictoptions(templId, dictoptions);

  dictRel = table_open(TSDictionaryRelationId, RowExclusiveLock);

     
                        
     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  dictOid = GetNewOidWithIndex(dictRel, TSDictionaryOidIndexId, Anum_pg_ts_dict_oid);
  values[Anum_pg_ts_dict_oid - 1] = ObjectIdGetDatum(dictOid);
  namestrcpy(&dname, dictname);
  values[Anum_pg_ts_dict_dictname - 1] = NameGetDatum(&dname);
  values[Anum_pg_ts_dict_dictnamespace - 1] = ObjectIdGetDatum(namespaceoid);
  values[Anum_pg_ts_dict_dictowner - 1] = ObjectIdGetDatum(GetUserId());
  values[Anum_pg_ts_dict_dicttemplate - 1] = ObjectIdGetDatum(templId);
  if (dictoptions)
  {
    values[Anum_pg_ts_dict_dictinitoption - 1] = PointerGetDatum(serialize_deflist(dictoptions));
  }
  else
  {
    nulls[Anum_pg_ts_dict_dictinitoption - 1] = true;
  }

  tup = heap_form_tuple(dictRel->rd_att, values, nulls);

  CatalogTupleInsert(dictRel, tup);

  address = makeDictionaryDependencies(tup);

                                                         
  InvokeObjectPostCreateHook(TSDictionaryRelationId, dictOid, 0);

  heap_freetuple(tup);

  table_close(dictRel, RowExclusiveLock);

  return address;
}

   
                                   
   
void
RemoveTSDictionaryById(Oid dictId)
{
  Relation relation;
  HeapTuple tup;

  relation = table_open(TSDictionaryRelationId, RowExclusiveLock);

  tup = SearchSysCache1(TSDICTOID, ObjectIdGetDatum(dictId));

  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search dictionary %u", dictId);
  }

  CatalogTupleDelete(relation, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(relation, RowExclusiveLock);
}

   
                                
   
ObjectAddress
AlterTSDictionary(AlterTSDictionaryStmt *stmt)
{
  HeapTuple tup, newtup;
  Relation rel;
  Oid dictId;
  ListCell *pl;
  List *dictoptions;
  Datum opt;
  bool isnull;
  Datum repl_val[Natts_pg_ts_dict];
  bool repl_null[Natts_pg_ts_dict];
  bool repl_repl[Natts_pg_ts_dict];
  ObjectAddress address;

  dictId = get_ts_dict_oid(stmt->dictname, false);

  rel = table_open(TSDictionaryRelationId, RowExclusiveLock);

  tup = SearchSysCache1(TSDICTOID, ObjectIdGetDatum(dictId));

  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search dictionary %u", dictId);
  }

                     
  if (!pg_ts_dict_ownercheck(dictId, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_TSDICTIONARY, NameListToString(stmt->dictname));
  }

                                               
  opt = SysCacheGetAttr(TSDICTOID, tup, Anum_pg_ts_dict_dictinitoption, &isnull);
  if (isnull)
  {
    dictoptions = NIL;
  }
  else
  {
    dictoptions = deserialize_deflist(opt);
  }

     
                                                      
     
  foreach (pl, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(pl);
    ListCell *cell;
    ListCell *prev;
    ListCell *next;

       
                              
       
    prev = NULL;
    for (cell = list_head(dictoptions); cell; cell = next)
    {
      DefElem *oldel = (DefElem *)lfirst(cell);

      next = lnext(cell);
      if (strcmp(oldel->defname, defel->defname) == 0)
      {
        dictoptions = list_delete_cell(dictoptions, cell, prev);
      }
      else
      {
        prev = cell;
      }
    }

       
                                         
       
    if (defel->arg)
    {
      dictoptions = lappend(dictoptions, defel);
    }
  }

     
              
     
  verify_dictoptions(((Form_pg_ts_dict)GETSTRUCT(tup))->dicttemplate, dictoptions);

     
                        
     
  memset(repl_val, 0, sizeof(repl_val));
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));

  if (dictoptions)
  {
    repl_val[Anum_pg_ts_dict_dictinitoption - 1] = PointerGetDatum(serialize_deflist(dictoptions));
  }
  else
  {
    repl_null[Anum_pg_ts_dict_dictinitoption - 1] = true;
  }
  repl_repl[Anum_pg_ts_dict_dictinitoption - 1] = true;

  newtup = heap_modify_tuple(tup, RelationGetDescr(rel), repl_val, repl_null, repl_repl);

  CatalogTupleUpdate(rel, &newtup->t_self, newtup);

  InvokeObjectPostAlterHook(TSDictionaryRelationId, dictId, 0);

  ObjectAddressSet(address, TSDictionaryRelationId, dictId);

     
                                                                           
                                                                            
                                                             
     

  heap_freetuple(newtup);
  ReleaseSysCache(tup);

  table_close(rel, RowExclusiveLock);

  return address;
}

                                                                        

   
                                                                      
   
                                                                 
   
static Datum
get_ts_template_func(DefElem *defel, int attnum)
{
  List *funcName = defGetQualifiedName(defel);
  Oid typeId[4];
  Oid retTypeId;
  int nargs;
  Oid procOid;

  retTypeId = INTERNALOID;
  typeId[0] = INTERNALOID;
  typeId[1] = INTERNALOID;
  typeId[2] = INTERNALOID;
  typeId[3] = INTERNALOID;
  switch (attnum)
  {
  case Anum_pg_ts_template_tmplinit:
    nargs = 1;
    break;
  case Anum_pg_ts_template_tmpllexize:
    nargs = 4;
    break;
  default:
                            
    elog(ERROR, "unrecognized attribute for text search template: %d", attnum);
    nargs = 0;                          
  }

  procOid = LookupFuncName(funcName, nargs, typeId, false);
  if (get_func_rettype(procOid) != retTypeId)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("function %s should return type %s", func_signature_string(funcName, nargs, NIL, typeId), format_type_be(retTypeId))));
  }

  return ObjectIdGetDatum(procOid);
}

   
                                                         
   
static ObjectAddress
makeTSTemplateDependencies(HeapTuple tuple)
{
  Form_pg_ts_template tmpl = (Form_pg_ts_template)GETSTRUCT(tuple);
  ObjectAddress myself, referenced;

  myself.classId = TSTemplateRelationId;
  myself.objectId = tmpl->oid;
  myself.objectSubId = 0;

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = tmpl->tmplnamespace;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                               
  recordDependencyOnCurrentExtension(&myself, false);

                                 
  referenced.classId = ProcedureRelationId;
  referenced.objectSubId = 0;

  referenced.objectId = tmpl->tmpllexize;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

  if (OidIsValid(tmpl->tmplinit))
  {
    referenced.objectId = tmpl->tmplinit;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

  return myself;
}

   
                               
   
ObjectAddress
DefineTSTemplate(List *names, List *parameters)
{
  ListCell *pl;
  Relation tmplRel;
  HeapTuple tup;
  Datum values[Natts_pg_ts_template];
  bool nulls[Natts_pg_ts_template];
  NameData dname;
  int i;
  Oid tmplOid;
  Oid namespaceoid;
  char *tmplname;
  ObjectAddress address;

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create text search templates")));
  }

                                                     
  namespaceoid = QualifiedNameGetCreationNamespace(names, &tmplname);

  tmplRel = table_open(TSTemplateRelationId, RowExclusiveLock);

  for (i = 0; i < Natts_pg_ts_template; i++)
  {
    nulls[i] = false;
    values[i] = ObjectIdGetDatum(InvalidOid);
  }

  tmplOid = GetNewOidWithIndex(tmplRel, TSTemplateOidIndexId, Anum_pg_ts_dict_oid);
  values[Anum_pg_ts_template_oid - 1] = ObjectIdGetDatum(tmplOid);
  namestrcpy(&dname, tmplname);
  values[Anum_pg_ts_template_tmplname - 1] = NameGetDatum(&dname);
  values[Anum_pg_ts_template_tmplnamespace - 1] = ObjectIdGetDatum(namespaceoid);

     
                                                                        
     
  foreach (pl, parameters)
  {
    DefElem *defel = (DefElem *)lfirst(pl);

    if (strcmp(defel->defname, "init") == 0)
    {
      values[Anum_pg_ts_template_tmplinit - 1] = get_ts_template_func(defel, Anum_pg_ts_template_tmplinit);
      nulls[Anum_pg_ts_template_tmplinit - 1] = false;
    }
    else if (strcmp(defel->defname, "lexize") == 0)
    {
      values[Anum_pg_ts_template_tmpllexize - 1] = get_ts_template_func(defel, Anum_pg_ts_template_tmpllexize);
      nulls[Anum_pg_ts_template_tmpllexize - 1] = false;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("text search template parameter \"%s\" not recognized", defel->defname)));
    }
  }

     
                
     
  if (!OidIsValid(DatumGetObjectId(values[Anum_pg_ts_template_tmpllexize - 1])))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search template lexize method is required")));
  }

     
                        
     
  tup = heap_form_tuple(tmplRel->rd_att, values, nulls);

  CatalogTupleInsert(tmplRel, tup);

  address = makeTSTemplateDependencies(tup);

                                                       
  InvokeObjectPostCreateHook(TSTemplateRelationId, tmplOid, 0);

  heap_freetuple(tup);

  table_close(tmplRel, RowExclusiveLock);

  return address;
}

   
                                 
   
void
RemoveTSTemplateById(Oid tmplId)
{
  Relation relation;
  HeapTuple tup;

  relation = table_open(TSTemplateRelationId, RowExclusiveLock);

  tup = SearchSysCache1(TSTEMPLATEOID, ObjectIdGetDatum(tmplId));

  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search template %u", tmplId);
  }

  CatalogTupleDelete(relation, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(relation, RowExclusiveLock);
}

                                                                             

   
                                          
                                
   
static HeapTuple
GetTSConfigTuple(List *names)
{
  HeapTuple tup;
  Oid cfgId;

  cfgId = get_ts_config_oid(names, true);
  if (!OidIsValid(cfgId))
  {
    return NULL;
  }

  tup = SearchSysCache1(TSCONFIGOID, ObjectIdGetDatum(cfgId));

  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for text search configuration %u", cfgId);
  }

  return tup;
}

   
                                                                  
   
                                                                          
                           
   
static ObjectAddress
makeConfigurationDependencies(HeapTuple tuple, bool removeOld, Relation mapRel)
{
  Form_pg_ts_config cfg = (Form_pg_ts_config)GETSTRUCT(tuple);
  ObjectAddresses *addrs;
  ObjectAddress myself, referenced;

  myself.classId = TSConfigRelationId;
  myself.objectId = cfg->oid;
  myself.objectSubId = 0;

                                                                           
  if (removeOld)
  {
    deleteDependencyRecordsFor(myself.classId, myself.objectId, true);
    deleteSharedDependencyRecordsFor(myself.classId, myself.objectId, 0);
  }

     
                                                                 
                                                                    
                                                                             
     
  addrs = new_object_addresses();

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = cfg->cfgnamespace;
  referenced.objectSubId = 0;
  add_exact_object_address(&referenced, addrs);

                           
  recordDependencyOnOwner(myself.classId, myself.objectId, cfg->cfgowner);

                               
  recordDependencyOnCurrentExtension(&myself, removeOld);

                            
  referenced.classId = TSParserRelationId;
  referenced.objectId = cfg->cfgparser;
  referenced.objectSubId = 0;
  add_exact_object_address(&referenced, addrs);

                                                         
  if (mapRel)
  {
    ScanKeyData skey;
    SysScanDesc scan;
    HeapTuple maptup;

                                                              
    CommandCounterIncrement();

    ScanKeyInit(&skey, Anum_pg_ts_config_map_mapcfg, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(myself.objectId));

    scan = systable_beginscan(mapRel, TSConfigMapIndexId, true, NULL, 1, &skey);

    while (HeapTupleIsValid((maptup = systable_getnext(scan))))
    {
      Form_pg_ts_config_map cfgmap = (Form_pg_ts_config_map)GETSTRUCT(maptup);

      referenced.classId = TSDictionaryRelationId;
      referenced.objectId = cfgmap->mapdict;
      referenced.objectSubId = 0;
      add_exact_object_address(&referenced, addrs);
    }

    systable_endscan(scan);
  }

                                                        
  record_object_address_dependencies(&myself, addrs, DEPENDENCY_NORMAL);

  free_object_addresses(addrs);

  return myself;
}

   
                                    
   
ObjectAddress
DefineTSConfiguration(List *names, List *parameters, ObjectAddress *copied)
{
  Relation cfgRel;
  Relation mapRel = NULL;
  HeapTuple tup;
  Datum values[Natts_pg_ts_config];
  bool nulls[Natts_pg_ts_config];
  AclResult aclresult;
  Oid namespaceoid;
  char *cfgname;
  NameData cname;
  Oid sourceOid = InvalidOid;
  Oid prsOid = InvalidOid;
  Oid cfgOid;
  ListCell *pl;
  ObjectAddress address;

                                                     
  namespaceoid = QualifiedNameGetCreationNamespace(names, &cfgname);

                                                         
  aclresult = pg_namespace_aclcheck(namespaceoid, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceoid));
  }

     
                                                                        
     
  foreach (pl, parameters)
  {
    DefElem *defel = (DefElem *)lfirst(pl);

    if (strcmp(defel->defname, "parser") == 0)
    {
      prsOid = get_ts_parser_oid(defGetQualifiedName(defel), false);
    }
    else if (strcmp(defel->defname, "copy") == 0)
    {
      sourceOid = get_ts_config_oid(defGetQualifiedName(defel), false);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("text search configuration parameter \"%s\" not recognized", defel->defname)));
    }
  }

  if (OidIsValid(sourceOid) && OidIsValid(prsOid))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot specify both PARSER and COPY options")));
  }

                                                 
  if (copied && OidIsValid(sourceOid))
  {
    ObjectAddressSet(*copied, TSConfigRelationId, sourceOid);
  }

     
                                     
     
  if (OidIsValid(sourceOid))
  {
    Form_pg_ts_config cfg;

    tup = SearchSysCache1(TSCONFIGOID, ObjectIdGetDatum(sourceOid));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for text search configuration %u", sourceOid);
    }

    cfg = (Form_pg_ts_config)GETSTRUCT(tup);

                             
    prsOid = cfg->cfgparser;

    ReleaseSysCache(tup);
  }

     
                
     
  if (!OidIsValid(prsOid))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("text search parser is required")));
  }

  cfgRel = table_open(TSConfigRelationId, RowExclusiveLock);

     
                                        
     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  cfgOid = GetNewOidWithIndex(cfgRel, TSConfigOidIndexId, Anum_pg_ts_config_oid);
  values[Anum_pg_ts_config_oid - 1] = ObjectIdGetDatum(cfgOid);
  namestrcpy(&cname, cfgname);
  values[Anum_pg_ts_config_cfgname - 1] = NameGetDatum(&cname);
  values[Anum_pg_ts_config_cfgnamespace - 1] = ObjectIdGetDatum(namespaceoid);
  values[Anum_pg_ts_config_cfgowner - 1] = ObjectIdGetDatum(GetUserId());
  values[Anum_pg_ts_config_cfgparser - 1] = ObjectIdGetDatum(prsOid);

  tup = heap_form_tuple(cfgRel->rd_att, values, nulls);

  CatalogTupleInsert(cfgRel, tup);

  if (OidIsValid(sourceOid))
  {
       
                                               
       
    ScanKeyData skey;
    SysScanDesc scan;
    HeapTuple maptup;

    mapRel = table_open(TSConfigMapRelationId, RowExclusiveLock);

    ScanKeyInit(&skey, Anum_pg_ts_config_map_mapcfg, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(sourceOid));

    scan = systable_beginscan(mapRel, TSConfigMapIndexId, true, NULL, 1, &skey);

    while (HeapTupleIsValid((maptup = systable_getnext(scan))))
    {
      Form_pg_ts_config_map cfgmap = (Form_pg_ts_config_map)GETSTRUCT(maptup);
      HeapTuple newmaptup;
      Datum mapvalues[Natts_pg_ts_config_map];
      bool mapnulls[Natts_pg_ts_config_map];

      memset(mapvalues, 0, sizeof(mapvalues));
      memset(mapnulls, false, sizeof(mapnulls));

      mapvalues[Anum_pg_ts_config_map_mapcfg - 1] = cfgOid;
      mapvalues[Anum_pg_ts_config_map_maptokentype - 1] = cfgmap->maptokentype;
      mapvalues[Anum_pg_ts_config_map_mapseqno - 1] = cfgmap->mapseqno;
      mapvalues[Anum_pg_ts_config_map_mapdict - 1] = cfgmap->mapdict;

      newmaptup = heap_form_tuple(mapRel->rd_att, mapvalues, mapnulls);

      CatalogTupleInsert(mapRel, newmaptup);

      heap_freetuple(newmaptup);
    }

    systable_endscan(scan);
  }

  address = makeConfigurationDependencies(tup, false, mapRel);

                                                            
  InvokeObjectPostCreateHook(TSConfigRelationId, cfgOid, 0);

  heap_freetuple(tup);

  if (mapRel)
  {
    table_close(mapRel, RowExclusiveLock);
  }
  table_close(cfgRel, RowExclusiveLock);

  return address;
}

   
                                      
   
void
RemoveTSConfigurationById(Oid cfgId)
{
  Relation relCfg, relMap;
  HeapTuple tup;
  ScanKeyData skey;
  SysScanDesc scan;

                                     
  relCfg = table_open(TSConfigRelationId, RowExclusiveLock);

  tup = SearchSysCache1(TSCONFIGOID, ObjectIdGetDatum(cfgId));

  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for text search dictionary %u", cfgId);
  }

  CatalogTupleDelete(relCfg, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(relCfg, RowExclusiveLock);

                                           
  relMap = table_open(TSConfigMapRelationId, RowExclusiveLock);

  ScanKeyInit(&skey, Anum_pg_ts_config_map_mapcfg, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(cfgId));

  scan = systable_beginscan(relMap, TSConfigMapIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid((tup = systable_getnext(scan))))
  {
    CatalogTupleDelete(relMap, &tup->t_self);
  }

  systable_endscan(scan);

  table_close(relMap, RowExclusiveLock);
}

   
                                                      
   
ObjectAddress
AlterTSConfiguration(AlterTSConfigurationStmt *stmt)
{
  HeapTuple tup;
  Oid cfgId;
  Relation relMap;
  ObjectAddress address;

                              
  tup = GetTSConfigTuple(stmt->cfgname);
  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("text search configuration \"%s\" does not exist", NameListToString(stmt->cfgname))));
  }

  cfgId = ((Form_pg_ts_config)GETSTRUCT(tup))->oid;

                     
  if (!pg_ts_config_ownercheck(cfgId, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_TSCONFIGURATION, NameListToString(stmt->cfgname));
  }

  relMap = table_open(TSConfigMapRelationId, RowExclusiveLock);

                            
  if (stmt->dicts)
  {
    MakeConfigurationMapping(stmt, tup, relMap);
  }
  else if (stmt->tokentype)
  {
    DropConfigurationMapping(stmt, tup, relMap);
  }

                           
  makeConfigurationDependencies(tup, true, relMap);

  InvokeObjectPostAlterHook(TSConfigRelationId, cfgId, 0);

  ObjectAddressSet(address, TSConfigRelationId, cfgId);

  table_close(relMap, RowExclusiveLock);

  ReleaseSysCache(tup);

  return address;
}

   
                                                                          
   
static int *
getTokenTypes(Oid prsId, List *tokennames)
{
  TSParserCacheEntry *prs = lookup_ts_parser_cache(prsId);
  LexDescr *list;
  int *res, i, ntoken;
  ListCell *tn;

  ntoken = list_length(tokennames);
  if (ntoken == 0)
  {
    return NULL;
  }
  res = (int *)palloc(sizeof(int) * ntoken);

  if (!OidIsValid(prs->lextypeOid))
  {
    elog(ERROR, "method lextype isn't defined for text search parser %u", prsId);
  }

                                        
  list = (LexDescr *)DatumGetPointer(OidFunctionCall1(prs->lextypeOid, (Datum)0));

  i = 0;
  foreach (tn, tokennames)
  {
    Value *val = (Value *)lfirst(tn);
    bool found = false;
    int j;

    j = 0;
    while (list && list[j].lexid)
    {
      if (strcmp(strVal(val), list[j].alias) == 0)
      {
        res[i] = list[j].lexid;
        found = true;
        break;
      }
      j++;
    }
    if (!found)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("token type \"%s\" does not exist", strVal(val))));
    }
    i++;
  }

  return res;
}

   
                                                     
   
static void
MakeConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap)
{
  Form_pg_ts_config tsform;
  Oid cfgId;
  ScanKeyData skey[2];
  SysScanDesc scan;
  HeapTuple maptup;
  int i;
  int j;
  Oid prsId;
  int *tokens, ntoken;
  Oid *dictIds;
  int ndict;
  ListCell *c;

  tsform = (Form_pg_ts_config)GETSTRUCT(tup);
  cfgId = tsform->oid;
  prsId = tsform->cfgparser;

  tokens = getTokenTypes(prsId, stmt->tokentype);
  ntoken = list_length(stmt->tokentype);

  if (stmt->override)
  {
       
                                                                  
       
    for (i = 0; i < ntoken; i++)
    {
      ScanKeyInit(&skey[0], Anum_pg_ts_config_map_mapcfg, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(cfgId));
      ScanKeyInit(&skey[1], Anum_pg_ts_config_map_maptokentype, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(tokens[i]));

      scan = systable_beginscan(relMap, TSConfigMapIndexId, true, NULL, 2, skey);

      while (HeapTupleIsValid((maptup = systable_getnext(scan))))
      {
        CatalogTupleDelete(relMap, &maptup->t_self);
      }

      systable_endscan(scan);
    }
  }

     
                                                            
     
  ndict = list_length(stmt->dicts);
  dictIds = (Oid *)palloc(sizeof(Oid) * ndict);
  i = 0;
  foreach (c, stmt->dicts)
  {
    List *names = (List *)lfirst(c);

    dictIds[i] = get_ts_dict_oid(names, false);
    i++;
  }

  if (stmt->replace)
  {
       
                                                         
       
    Oid dictOld = dictIds[0], dictNew = dictIds[1];

    ScanKeyInit(&skey[0], Anum_pg_ts_config_map_mapcfg, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(cfgId));

    scan = systable_beginscan(relMap, TSConfigMapIndexId, true, NULL, 1, skey);

    while (HeapTupleIsValid((maptup = systable_getnext(scan))))
    {
      Form_pg_ts_config_map cfgmap = (Form_pg_ts_config_map)GETSTRUCT(maptup);

         
                                                 
         
      if (tokens)
      {
        bool tokmatch = false;

        for (j = 0; j < ntoken; j++)
        {
          if (cfgmap->maptokentype == tokens[j])
          {
            tokmatch = true;
            break;
          }
        }
        if (!tokmatch)
        {
          continue;
        }
      }

         
                                     
         
      if (cfgmap->mapdict == dictOld)
      {
        Datum repl_val[Natts_pg_ts_config_map];
        bool repl_null[Natts_pg_ts_config_map];
        bool repl_repl[Natts_pg_ts_config_map];
        HeapTuple newtup;

        memset(repl_val, 0, sizeof(repl_val));
        memset(repl_null, false, sizeof(repl_null));
        memset(repl_repl, false, sizeof(repl_repl));

        repl_val[Anum_pg_ts_config_map_mapdict - 1] = ObjectIdGetDatum(dictNew);
        repl_repl[Anum_pg_ts_config_map_mapdict - 1] = true;

        newtup = heap_modify_tuple(maptup, RelationGetDescr(relMap), repl_val, repl_null, repl_repl);
        CatalogTupleUpdate(relMap, &newtup->t_self, newtup);
      }
    }

    systable_endscan(scan);
  }
  else
  {
       
                                
       
    for (i = 0; i < ntoken; i++)
    {
      for (j = 0; j < ndict; j++)
      {
        Datum values[Natts_pg_ts_config_map];
        bool nulls[Natts_pg_ts_config_map];

        memset(nulls, false, sizeof(nulls));
        values[Anum_pg_ts_config_map_mapcfg - 1] = ObjectIdGetDatum(cfgId);
        values[Anum_pg_ts_config_map_maptokentype - 1] = Int32GetDatum(tokens[i]);
        values[Anum_pg_ts_config_map_mapseqno - 1] = Int32GetDatum(j + 1);
        values[Anum_pg_ts_config_map_mapdict - 1] = ObjectIdGetDatum(dictIds[j]);

        tup = heap_form_tuple(relMap->rd_att, values, nulls);
        CatalogTupleInsert(relMap, tup);

        heap_freetuple(tup);
      }
    }
  }

  EventTriggerCollectAlterTSConfig(stmt, cfgId, dictIds, ndict);
}

   
                                                
   
static void
DropConfigurationMapping(AlterTSConfigurationStmt *stmt, HeapTuple tup, Relation relMap)
{
  Form_pg_ts_config tsform;
  Oid cfgId;
  ScanKeyData skey[2];
  SysScanDesc scan;
  HeapTuple maptup;
  int i;
  Oid prsId;
  int *tokens;
  ListCell *c;

  tsform = (Form_pg_ts_config)GETSTRUCT(tup);
  cfgId = tsform->oid;
  prsId = tsform->cfgparser;

  tokens = getTokenTypes(prsId, stmt->tokentype);

  i = 0;
  foreach (c, stmt->tokentype)
  {
    Value *val = (Value *)lfirst(c);
    bool found = false;

    ScanKeyInit(&skey[0], Anum_pg_ts_config_map_mapcfg, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(cfgId));
    ScanKeyInit(&skey[1], Anum_pg_ts_config_map_maptokentype, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(tokens[i]));

    scan = systable_beginscan(relMap, TSConfigMapIndexId, true, NULL, 2, skey);

    while (HeapTupleIsValid((maptup = systable_getnext(scan))))
    {
      CatalogTupleDelete(relMap, &maptup->t_self);
      found = true;
    }

    systable_endscan(scan);

    if (!found)
    {
      if (!stmt->missing_ok)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("mapping for token type \"%s\" does not exist", strVal(val))));
      }
      else
      {
        ereport(NOTICE, (errmsg("mapping for token type \"%s\" does not exist, skipping", strVal(val))));
      }
    }

    i++;
  }

  EventTriggerCollectAlterTSConfig(stmt, cfgId, NULL, 0);
}

   
                                                                               
   
                                                                       
                                                                         
                                                                          
                 
   
                                                                           
                                                                              
   
text *
serialize_deflist(List *deflist)
{
  text *result;
  StringInfoData buf;
  ListCell *l;

  initStringInfo(&buf);

  foreach (l, deflist)
  {
    DefElem *defel = (DefElem *)lfirst(l);
    char *val = defGetString(defel);

    appendStringInfo(&buf, "%s = ", quote_identifier(defel->defname));
                                                                           
    if (strchr(val, '\\'))
    {
      appendStringInfoChar(&buf, ESCAPE_STRING_SYNTAX);
    }
    appendStringInfoChar(&buf, '\'');
    while (*val)
    {
      char ch = *val++;

      if (SQL_STR_DOUBLE(ch, true))
      {
        appendStringInfoChar(&buf, ch);
      }
      appendStringInfoChar(&buf, ch);
    }
    appendStringInfoChar(&buf, '\'');
    if (lnext(l) != NULL)
    {
      appendStringInfoString(&buf, ", ");
    }
  }

  result = cstring_to_text_with_len(buf.data, buf.len);
  pfree(buf.data);
  return result;
}

   
                                                                              
   
                                                                            
                                                                       
                                                     
   
List *
deserialize_deflist(Datum txt)
{
  text *in = DatumGetTextPP(txt);                           
  List *result = NIL;
  int len = VARSIZE_ANY_EXHDR(in);
  char *ptr, *endptr, *workspace, *wsptr = NULL, *startvalue = NULL;
  typedef enum
  {
    CS_WAITKEY,
    CS_INKEY,
    CS_INQKEY,
    CS_WAITEQ,
    CS_WAITVALUE,
    CS_INSQVALUE,
    CS_INDQVALUE,
    CS_INWVALUE
  } ds_state;
  ds_state state = CS_WAITKEY;

  workspace = (char *)palloc(len + 1);                            
  ptr = VARDATA_ANY(in);
  endptr = ptr + len;
  for (; ptr < endptr; ptr++)
  {
    switch (state)
    {
    case CS_WAITKEY:
      if (isspace((unsigned char)*ptr) || *ptr == ',')
      {
        continue;
      }
      if (*ptr == '"')
      {
        wsptr = workspace;
        state = CS_INQKEY;
      }
      else
      {
        wsptr = workspace;
        *wsptr++ = *ptr;
        state = CS_INKEY;
      }
      break;
    case CS_INKEY:
      if (isspace((unsigned char)*ptr))
      {
        *wsptr++ = '\0';
        state = CS_WAITEQ;
      }
      else if (*ptr == '=')
      {
        *wsptr++ = '\0';
        state = CS_WAITVALUE;
      }
      else
      {
        *wsptr++ = *ptr;
      }
      break;
    case CS_INQKEY:
      if (*ptr == '"')
      {
        if (ptr + 1 < endptr && ptr[1] == '"')
        {
                                               
          *wsptr++ = *ptr++;
        }
        else
        {
          *wsptr++ = '\0';
          state = CS_WAITEQ;
        }
      }
      else
      {
        *wsptr++ = *ptr;
      }
      break;
    case CS_WAITEQ:
      if (*ptr == '=')
      {
        state = CS_WAITVALUE;
      }
      else if (!isspace((unsigned char)*ptr))
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid parameter list format: \"%s\"", text_to_cstring(in))));
      }
      break;
    case CS_WAITVALUE:
      if (*ptr == '\'')
      {
        startvalue = wsptr;
        state = CS_INSQVALUE;
      }
      else if (*ptr == 'E' && ptr + 1 < endptr && ptr[1] == '\'')
      {
        ptr++;
        startvalue = wsptr;
        state = CS_INSQVALUE;
      }
      else if (*ptr == '"')
      {
        startvalue = wsptr;
        state = CS_INDQVALUE;
      }
      else if (!isspace((unsigned char)*ptr))
      {
        startvalue = wsptr;
        *wsptr++ = *ptr;
        state = CS_INWVALUE;
      }
      break;
    case CS_INSQVALUE:
      if (*ptr == '\'')
      {
        if (ptr + 1 < endptr && ptr[1] == '\'')
        {
                                               
          *wsptr++ = *ptr++;
        }
        else
        {
          *wsptr++ = '\0';
          result = lappend(result, makeDefElem(pstrdup(workspace), (Node *)makeString(pstrdup(startvalue)), -1));
          state = CS_WAITKEY;
        }
      }
      else if (*ptr == '\\')
      {
        if (ptr + 1 < endptr && ptr[1] == '\\')
        {
                                                    
          *wsptr++ = *ptr++;
        }
        else
        {
          *wsptr++ = *ptr;
        }
      }
      else
      {
        *wsptr++ = *ptr;
      }
      break;
    case CS_INDQVALUE:
      if (*ptr == '"')
      {
        if (ptr + 1 < endptr && ptr[1] == '"')
        {
                                               
          *wsptr++ = *ptr++;
        }
        else
        {
          *wsptr++ = '\0';
          result = lappend(result, makeDefElem(pstrdup(workspace), (Node *)makeString(pstrdup(startvalue)), -1));
          state = CS_WAITKEY;
        }
      }
      else
      {
        *wsptr++ = *ptr;
      }
      break;
    case CS_INWVALUE:
      if (*ptr == ',' || isspace((unsigned char)*ptr))
      {
        *wsptr++ = '\0';
        result = lappend(result, makeDefElem(pstrdup(workspace), (Node *)makeString(pstrdup(startvalue)), -1));
        state = CS_WAITKEY;
      }
      else
      {
        *wsptr++ = *ptr;
      }
      break;
    default:
      elog(ERROR, "unrecognized deserialize_deflist state: %d", state);
    }
  }

  if (state == CS_INWVALUE)
  {
    *wsptr++ = '\0';
    result = lappend(result, makeDefElem(pstrdup(workspace), (Node *)makeString(pstrdup(startvalue)), -1));
  }
  else if (state != CS_WAITKEY)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid parameter list format: \"%s\"", text_to_cstring(in))));
  }

  pfree(workspace);

  return result;
}
