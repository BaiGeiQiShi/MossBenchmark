                                                                            
   
                
                                      
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "parser/parser.h"
#include "parser/parse_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static int32
typenameTypeMod(ParseState *pstate, const TypeName *typeName, Type typ);

   
                  
                              
   
Type
LookupTypeName(ParseState *pstate, const TypeName *typeName, int32 *typmod_p, bool missing_ok)
{
  return LookupTypeNameExtended(pstate, typeName, typmod_p, true, missing_ok);
}

   
                          
                                                                            
                                                                      
                                                                        
                           
   
                                                                            
            
   
                                                                               
                                                                           
                              
   
                                                                          
                                                                              
                                                                             
                                                                          
                                   
   
                                                                             
                                                                            
                                                                              
                                           
   
                                                                 
   
Type
LookupTypeNameExtended(ParseState *pstate, const TypeName *typeName, int32 *typmod_p, bool temp_ok, bool missing_ok)
{
  Oid typoid;
  HeapTuple tup;
  int32 typmod;

  if (typeName->names == NIL)
  {
                                                                          
    typoid = typeName->typeOid;
  }
  else if (typeName->pct_type)
  {
                                                             
    RangeVar *rel = makeRangeVar(NULL, NULL, typeName->location);
    char *field = NULL;
    Oid relid;
    AttrNumber attnum;

                                   
    switch (list_length(typeName->names))
    {
    case 1:
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("improper %%TYPE reference (too few dotted names): %s", NameListToString(typeName->names)), parser_errposition(pstate, typeName->location)));
      break;
    case 2:
      rel->relname = strVal(linitial(typeName->names));
      field = strVal(lsecond(typeName->names));
      break;
    case 3:
      rel->schemaname = strVal(linitial(typeName->names));
      rel->relname = strVal(lsecond(typeName->names));
      field = strVal(lthird(typeName->names));
      break;
    case 4:
      rel->catalogname = strVal(linitial(typeName->names));
      rel->schemaname = strVal(lsecond(typeName->names));
      rel->relname = strVal(lthird(typeName->names));
      field = strVal(lfourth(typeName->names));
      break;
    default:
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("improper %%TYPE reference (too many dotted names): %s", NameListToString(typeName->names)), parser_errposition(pstate, typeName->location)));
      break;
    }

       
                          
       
                                                                         
                                                                    
                                                           
       
    relid = RangeVarGetRelid(rel, NoLock, missing_ok);
    attnum = get_attnum(relid, field);
    if (attnum == InvalidAttrNumber)
    {
      if (missing_ok)
      {
        typoid = InvalidOid;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", field, rel->relname), parser_errposition(pstate, typeName->location)));
      }
    }
    else
    {
      typoid = get_atttype(relid, attnum);

                                                               
      Assert(typeName->arrayBounds == NIL);

                                                                  
      ereport(NOTICE, (errmsg("type reference %s converted to %s", TypeNameToString(typeName), format_type_be(typoid))));
    }
  }
  else
  {
                                         
    char *schemaname;
    char *typname;

                                   
    DeconstructQualifiedName(typeName->names, &schemaname, &typname);

    if (schemaname)
    {
                                        
      Oid namespaceId;
      ParseCallbackState pcbstate;

      setup_parser_errposition_callback(&pcbstate, pstate, typeName->location);

      namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
      if (OidIsValid(namespaceId))
      {
        typoid = GetSysCacheOid2(TYPENAMENSP, Anum_pg_type_oid, PointerGetDatum(typname), ObjectIdGetDatum(namespaceId));
      }
      else
      {
        typoid = InvalidOid;
      }

      cancel_parser_errposition_callback(&pcbstate);
    }
    else
    {
                                                            
      typoid = TypenameGetTypidExtended(typname, temp_ok);
    }

                                                              
    if (typeName->arrayBounds != NIL)
    {
      typoid = get_array_type(typoid);
    }
  }

  if (!OidIsValid(typoid))
  {
    if (typmod_p)
    {
      *typmod_p = -1;
    }
    return NULL;
  }

  tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typoid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for type %u", typoid);
  }

  typmod = typenameTypeMod(pstate, typeName, (Type)tup);

  if (typmod_p)
  {
    *typmod_p = typmod;
  }

  return (Type)tup;
}

   
                     
                                                                            
                                                                            
                    
   
                                                                          
                                                                         
                           
   
                                                                 
   
Oid
LookupTypeNameOid(ParseState *pstate, const TypeName *typeName, bool missing_ok)
{
  Oid typoid;
  Type tup;

  tup = LookupTypeName(pstate, typeName, NULL, missing_ok);
  if (tup == NULL)
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" does not exist", TypeNameToString(typeName)), parser_errposition(pstate, typeName->location)));
    }

    return InvalidOid;
  }

  typoid = ((Form_pg_type)GETSTRUCT(tup))->oid;
  ReleaseSysCache(tup);

  return typoid;
}

   
                                                                       
   
                                                                      
                                                                           
                                                                          
   
Type
typenameType(ParseState *pstate, const TypeName *typeName, int32 *typmod_p)
{
  Type tup;

  tup = LookupTypeName(pstate, typeName, typmod_p, false);
  if (tup == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" does not exist", TypeNameToString(typeName)), parser_errposition(pstate, typeName->location)));
  }
  if (!((Form_pg_type)GETSTRUCT(tup))->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" is only a shell", TypeNameToString(typeName)), parser_errposition(pstate, typeName->location)));
  }
  return tup;
}

   
                                                            
   
                                                                       
                           
   
Oid
typenameTypeId(ParseState *pstate, const TypeName *typeName)
{
  Oid typoid;
  Type tup;

  tup = typenameType(pstate, typeName, NULL);
  typoid = ((Form_pg_type)GETSTRUCT(tup))->oid;
  ReleaseSysCache(tup);

  return typoid;
}

   
                                                                             
   
                                                                          
                                       
   
void
typenameTypeIdAndMod(ParseState *pstate, const TypeName *typeName, Oid *typeid_p, int32 *typmod_p)
{
  Type tup;

  tup = typenameType(pstate, typeName, typmod_p);
  *typeid_p = ((Form_pg_type)GETSTRUCT(tup))->oid;
  ReleaseSysCache(tup);
}

   
                                                                        
   
                                                                             
                              
   
                                                                          
                                      
   
                                                                 
   
static int32
typenameTypeMod(ParseState *pstate, const TypeName *typeName, Type typ)
{
  int32 result;
  Oid typmodin;
  Datum *datums;
  int n;
  ListCell *l;
  ArrayType *arrtypmod;
  ParseCallbackState pcbstate;

                                                           
  if (typeName->typmods == NIL)
  {
    return typeName->typemod;
  }

     
                                                                            
                                                                     
                        
     
  if (!((Form_pg_type)GETSTRUCT(typ))->typisdefined)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("type modifier cannot be specified for shell type \"%s\"", TypeNameToString(typeName)), parser_errposition(pstate, typeName->location)));
  }

  typmodin = ((Form_pg_type)GETSTRUCT(typ))->typmodin;

  if (typmodin == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("type modifier is not allowed for type \"%s\"", TypeNameToString(typeName)), parser_errposition(pstate, typeName->location)));
  }

     
                                                                            
                                                                        
                                                        
     
  datums = (Datum *)palloc(list_length(typeName->typmods) * sizeof(Datum));
  n = 0;
  foreach (l, typeName->typmods)
  {
    Node *tm = (Node *)lfirst(l);
    char *cstr = NULL;

    if (IsA(tm, A_Const))
    {
      A_Const *ac = (A_Const *)tm;

      if (IsA(&ac->val, Integer))
      {
        cstr = psprintf("%ld", (long)ac->val.val.ival);
      }
      else if (IsA(&ac->val, Float) || IsA(&ac->val, String))
      {
                                                     
        cstr = ac->val.val.str;
      }
    }
    else if (IsA(tm, ColumnRef))
    {
      ColumnRef *cr = (ColumnRef *)tm;

      if (list_length(cr->fields) == 1 && IsA(linitial(cr->fields), String))
      {
        cstr = strVal(linitial(cr->fields));
      }
    }
    if (!cstr)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("type modifiers must be simple constants or identifiers"), parser_errposition(pstate, typeName->location)));
    }
    datums[n++] = CStringGetDatum(cstr);
  }

                                                                       
  arrtypmod = construct_array(datums, n, CSTRINGOID, -2, false, 'c');

                                                                    
  setup_parser_errposition_callback(&pcbstate, pstate, typeName->location);

  result = DatumGetInt32(OidFunctionCall1(typmodin, PointerGetDatum(arrtypmod)));

  cancel_parser_errposition_callback(&pcbstate);

  pfree(datums);
  pfree(arrtypmod);

  return result;
}

   
                          
                                                                         
                                                                          
   
                                                                         
                                                  
   
static void
appendTypeNameToBuffer(const TypeName *typeName, StringInfo string)
{
  if (typeName->names != NIL)
  {
                                            
    ListCell *l;

    foreach (l, typeName->names)
    {
      if (l != list_head(typeName->names))
      {
        appendStringInfoChar(string, '.');
      }
      appendStringInfoString(string, strVal(lfirst(l)));
    }
  }
  else
  {
                                           
    appendStringInfoString(string, format_type_be(typeName->typeOid));
  }

     
                                                                 
                    
     
  if (typeName->pct_type)
  {
    appendStringInfoString(string, "%TYPE");
  }

  if (typeName->arrayBounds != NIL)
  {
    appendStringInfoString(string, "[]");
  }
}

   
                    
                                                          
   
                                                                         
                                                  
   
char *
TypeNameToString(const TypeName *typeName)
{
  StringInfoData string;

  initStringInfo(&string);
  appendTypeNameToBuffer(typeName, &string);
  return string.data;
}

   
                        
                                                                     
   
char *
TypeNameListToString(List *typenames)
{
  StringInfoData string;
  ListCell *l;

  initStringInfo(&string);
  foreach (l, typenames)
  {
    TypeName *typeName = lfirst_node(TypeName, l);

    if (l != list_head(typenames))
    {
      appendStringInfoChar(&string, ',');
    }
    appendTypeNameToBuffer(typeName, &string);
  }
  return string.data;
}

   
                   
   
                                                                           
   
Oid
LookupCollation(ParseState *pstate, List *collnames, int location)
{
  Oid colloid;
  ParseCallbackState pcbstate;

  if (pstate)
  {
    setup_parser_errposition_callback(&pcbstate, pstate, location);
  }

  colloid = get_collation_oid(collnames, false);

  if (pstate)
  {
    cancel_parser_errposition_callback(&pcbstate);
  }

  return colloid;
}

   
                         
   
                                                                      
                                                                 
   
                                                                     
   
Oid
GetColumnDefCollation(ParseState *pstate, ColumnDef *coldef, Oid typeOid)
{
  Oid result;
  Oid typcollation = get_typcollation(typeOid);
  int location = coldef->location;

  if (coldef->collClause)
  {
                                                                
    location = coldef->collClause->location;
    result = LookupCollation(pstate, coldef->collClause->collname, location);
  }
  else if (OidIsValid(coldef->collOid))
  {
                                            
    result = coldef->collOid;
  }
  else
  {
                                                 
    result = typcollation;
  }

                                                              
  if (OidIsValid(result) && !OidIsValid(typcollation))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be(typeOid)), parser_errposition(pstate, location)));
  }

  return result;
}

                                              
                                                                      
Type
typeidType(Oid id)
{
  HeapTuple tup;

  tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(id));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for type %u", id);
  }
  return (Type)tup;
}

                                                      
Oid
typeTypeId(Type tp)
{
  if (tp == NULL)                       
  {
    elog(ERROR, "typeTypeId() called with NULL type struct");
  }
  return ((Form_pg_type)GETSTRUCT(tp))->oid;
}

                                                            
int16
typeLen(Type t)
{
  Form_pg_type typ;

  typ = (Form_pg_type)GETSTRUCT(t);
  return typ->typlen;
}

                                                               
bool
typeByVal(Type t)
{
  Form_pg_type typ;

  typ = (Form_pg_type)GETSTRUCT(t);
  return typ->typbyval;
}

                                                         
char *
typeTypeName(Type t)
{
  Form_pg_type typ;

  typ = (Form_pg_type)GETSTRUCT(t);
                                                                          
  return pstrdup(NameStr(typ->typname));
}

                                                                  
Oid
typeTypeRelid(Type typ)
{
  Form_pg_type typtup;

  typtup = (Form_pg_type)GETSTRUCT(typ);
  return typtup->typrelid;
}

                                                                      
Oid
typeTypeCollation(Type typ)
{
  Form_pg_type typtup;

  typtup = (Form_pg_type)GETSTRUCT(typ);
  return typtup->typcollation;
}

   
                                                                            
                                                                             
                                                                         
   
Datum
stringTypeDatum(Type tp, char *string, int32 atttypmod)
{
  Form_pg_type typform = (Form_pg_type)GETSTRUCT(tp);
  Oid typinput = typform->typinput;
  Oid typioparam = getTypeIOParam(tp);

  return OidInputFunctionCall(typinput, string, typioparam, atttypmod);
}

   
                                                                             
                                                       
   
Oid
typeidTypeRelid(Oid type_id)
{
  HeapTuple typeTuple;
  Form_pg_type type;
  Oid result;

  typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type_id));
  if (!HeapTupleIsValid(typeTuple))
  {
    elog(ERROR, "cache lookup failed for type %u", type_id);
  }
  type = (Form_pg_type)GETSTRUCT(typeTuple);
  result = type->typrelid;
  ReleaseSysCache(typeTuple);
  return result;
}

   
                                                                             
                                                                            
                                                                          
   
Oid
typeOrDomainTypeRelid(Oid type_id)
{
  HeapTuple typeTuple;
  Form_pg_type type;
  Oid result;

  for (;;)
  {
    typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type_id));
    if (!HeapTupleIsValid(typeTuple))
    {
      elog(ERROR, "cache lookup failed for type %u", type_id);
    }
    type = (Form_pg_type)GETSTRUCT(typeTuple);
    if (type->typtype != TYPTYPE_DOMAIN)
    {
                                                         
      break;
    }
                                                          
    type_id = type->typbasetype;
    ReleaseSysCache(typeTuple);
  }
  result = type->typrelid;
  ReleaseSysCache(typeTuple);
  return result;
}

   
                                                                     
   
static void
pts_error_callback(void *arg)
{
  const char *str = (const char *)arg;

  errcontext("invalid type name \"%s\"", str);

     
                                                                         
                                                                           
                                                      
     
  errposition(0);
}

   
                                                                            
                                                                 
                                                   
                                                                 
   
TypeName *
typeStringToTypeName(const char *str)
{
  StringInfoData buf;
  List *raw_parsetree_list;
  SelectStmt *stmt;
  ResTarget *restarget;
  TypeCast *typecast;
  TypeName *typeName;
  ErrorContextCallback ptserrcontext;

                                                      
  if (strspn(str, " \t\n\r\f") == strlen(str))
  {
    goto fail;
  }

  initStringInfo(&buf);
  appendStringInfo(&buf, "SELECT NULL::%s", str);

     
                                                                     
     
  ptserrcontext.callback = pts_error_callback;
  ptserrcontext.arg = unconstify(char *, str);
  ptserrcontext.previous = error_context_stack;
  error_context_stack = &ptserrcontext;

  raw_parsetree_list = raw_parser(buf.data);

  error_context_stack = ptserrcontext.previous;

     
                                                                             
                                                        
     
  if (list_length(raw_parsetree_list) != 1)
  {
    goto fail;
  }
  stmt = (SelectStmt *)linitial_node(RawStmt, raw_parsetree_list)->stmt;
  if (stmt == NULL || !IsA(stmt, SelectStmt) || stmt->distinctClause != NIL || stmt->intoClause != NULL || stmt->fromClause != NIL || stmt->whereClause != NULL || stmt->groupClause != NIL || stmt->havingClause != NULL || stmt->windowClause != NIL || stmt->valuesLists != NIL || stmt->sortClause != NIL || stmt->limitOffset != NULL || stmt->limitCount != NULL || stmt->lockingClause != NIL || stmt->withClause != NULL || stmt->op != SETOP_NONE)
  {
    goto fail;
  }
  if (list_length(stmt->targetList) != 1)
  {
    goto fail;
  }
  restarget = (ResTarget *)linitial(stmt->targetList);
  if (restarget == NULL || !IsA(restarget, ResTarget) || restarget->name != NULL || restarget->indirection != NIL)
  {
    goto fail;
  }
  typecast = (TypeCast *)restarget->val;
  if (typecast == NULL || !IsA(typecast, TypeCast) || typecast->arg == NULL || !IsA(typecast->arg, A_Const))
  {
    goto fail;
  }

  typeName = typecast->typeName;
  if (typeName == NULL || !IsA(typeName, TypeName))
  {
    goto fail;
  }
  if (typeName->setof)
  {
    goto fail;
  }

  pfree(buf.data);

  return typeName;

fail:
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid type name \"%s\"", str)));
  return NULL;                          
}

   
                                                                            
                                                                 
                                                              
                                                                              
                                    
   
void
parseTypeString(const char *str, Oid *typeid_p, int32 *typmod_p, bool missing_ok)
{
  TypeName *typeName;
  Type tup;

  typeName = typeStringToTypeName(str);

  tup = LookupTypeName(NULL, typeName, typmod_p, missing_ok);
  if (tup == NULL)
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" does not exist", TypeNameToString(typeName)), parser_errposition(NULL, typeName->location)));
    }
    *typeid_p = InvalidOid;
  }
  else
  {
    Form_pg_type typ = (Form_pg_type)GETSTRUCT(tup);

    if (!typ->typisdefined)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" is only a shell", TypeNameToString(typeName)), parser_errposition(NULL, typeName->location)));
    }
    *typeid_p = typ->oid;
    ReleaseSysCache(tup);
  }
}
