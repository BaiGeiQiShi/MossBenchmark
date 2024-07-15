                                                                            
   
                   
                                             
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_collation.h"
#include "commands/alter.h"
#include "commands/collationcmds.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/pg_locale.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  char *localename;                                         
  char *alias;                                    
  int enc;                        
} CollAliasData;

   
                    
   
ObjectAddress
DefineCollation(ParseState *pstate, List *names, List *parameters, bool if_not_exists)
{
  char *collName;
  Oid collNamespace;
  AclResult aclresult;
  ListCell *pl;
  DefElem *fromEl = NULL;
  DefElem *localeEl = NULL;
  DefElem *lccollateEl = NULL;
  DefElem *lcctypeEl = NULL;
  DefElem *providerEl = NULL;
  DefElem *deterministicEl = NULL;
  DefElem *versionEl = NULL;
  char *collcollate = NULL;
  char *collctype = NULL;
  char *collproviderstr = NULL;
  bool collisdeterministic = true;
  int collencoding = 0;
  char collprovider = 0;
  char *collversion = NULL;
  Oid newoid;
  ObjectAddress address;

  collNamespace = QualifiedNameGetCreationNamespace(names, &collName);

  aclresult = pg_namespace_aclcheck(collNamespace, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(collNamespace));
  }

  foreach (pl, parameters)
  {
    DefElem *defel = lfirst_node(DefElem, pl);
    DefElem **defelp;

    if (strcmp(defel->defname, "from") == 0)
    {
      defelp = &fromEl;
    }
    else if (strcmp(defel->defname, "locale") == 0)
    {
      defelp = &localeEl;
    }
    else if (strcmp(defel->defname, "lc_collate") == 0)
    {
      defelp = &lccollateEl;
    }
    else if (strcmp(defel->defname, "lc_ctype") == 0)
    {
      defelp = &lcctypeEl;
    }
    else if (strcmp(defel->defname, "provider") == 0)
    {
      defelp = &providerEl;
    }
    else if (strcmp(defel->defname, "deterministic") == 0)
    {
      defelp = &deterministicEl;
    }
    else if (strcmp(defel->defname, "version") == 0)
    {
      defelp = &versionEl;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("collation attribute \"%s\" not recognized", defel->defname), parser_errposition(pstate, defel->location)));
      break;
    }

    *defelp = defel;
  }

  if ((localeEl && (lccollateEl || lcctypeEl)) || (fromEl && list_length(parameters) != 1))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
  }

  if (fromEl)
  {
    Oid collid;
    HeapTuple tp;

    collid = get_collation_oid(defGetQualifiedName(fromEl), false);
    tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(collid));
    if (!HeapTupleIsValid(tp))
    {
      elog(ERROR, "cache lookup failed for collation %u", collid);
    }

    collcollate = pstrdup(NameStr(((Form_pg_collation)GETSTRUCT(tp))->collcollate));
    collctype = pstrdup(NameStr(((Form_pg_collation)GETSTRUCT(tp))->collctype));
    collprovider = ((Form_pg_collation)GETSTRUCT(tp))->collprovider;
    collisdeterministic = ((Form_pg_collation)GETSTRUCT(tp))->collisdeterministic;
    collencoding = ((Form_pg_collation)GETSTRUCT(tp))->collencoding;

    ReleaseSysCache(tp);

       
                                                                        
                                                                         
                                                                        
                                                                           
                                
       
    if (collprovider == COLLPROVIDER_DEFAULT)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("collation \"default\" cannot be copied")));
    }
  }

  if (localeEl)
  {
    collcollate = defGetString(localeEl);
    collctype = defGetString(localeEl);
  }

  if (lccollateEl)
  {
    collcollate = defGetString(lccollateEl);
  }

  if (lcctypeEl)
  {
    collctype = defGetString(lcctypeEl);
  }

  if (providerEl)
  {
    collproviderstr = defGetString(providerEl);
  }

  if (deterministicEl)
  {
    collisdeterministic = defGetBoolean(deterministicEl);
  }

  if (versionEl)
  {
    collversion = defGetString(versionEl);
  }

  if (collproviderstr)
  {
    if (pg_strcasecmp(collproviderstr, "icu") == 0)
    {
      collprovider = COLLPROVIDER_ICU;
    }
    else if (pg_strcasecmp(collproviderstr, "libc") == 0)
    {
      collprovider = COLLPROVIDER_LIBC;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("unrecognized collation provider: %s", collproviderstr)));
    }
  }
  else if (!fromEl)
  {
    collprovider = COLLPROVIDER_LIBC;
  }

  if (!collcollate)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("parameter \"lc_collate\" must be specified")));
  }

  if (!collctype)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("parameter \"lc_ctype\" must be specified")));
  }

     
                                                                       
                                                                           
                                                              
     
  if (!collisdeterministic && collprovider != COLLPROVIDER_ICU)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations not supported with this provider")));
  }

  if (!fromEl)
  {
    if (collprovider == COLLPROVIDER_ICU)
    {
#ifdef USE_ICU
         
                                                                      
                                                                        
                                                                      
                                                                   
                                                                     
                                                                         
         
                                                                    
                                                 
         
      if (!is_encoding_supported_by_icu(GetDatabaseEncoding()))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("current database's encoding is not supported with this provider")));
      }
#endif
      collencoding = -1;
    }
    else
    {
      collencoding = GetDatabaseEncoding();
      check_encoding_locale_matches(collencoding, collcollate, collctype);
    }
  }

  if (!collversion)
  {
    collversion = get_collation_actual_version(collprovider, collcollate);
  }

  newoid = CollationCreate(collName, collNamespace, GetUserId(), collprovider, collisdeterministic, collencoding, collcollate, collctype, collversion, if_not_exists, false);                

  if (!OidIsValid(newoid))
  {
    return InvalidObjectAddress;
  }

     
                                                                            
                                                                
     
  CommandCounterIncrement();
  if (!lc_collate_is_c(newoid) || !lc_ctype_is_c(newoid))
  {
    (void)pg_newlocale_from_collation(newoid);
  }

  ObjectAddressSet(address, CollationRelationId, newoid);

  return address;
}

   
                                                        
   
                                                                             
                                                                    
   
void
IsThereCollationInNamespace(const char *collname, Oid nspOid)
{
                                                              
  if (SearchSysCacheExists3(COLLNAMEENCNSP, CStringGetDatum(collname), Int32GetDatum(GetDatabaseEncoding()), ObjectIdGetDatum(nspOid)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("collation \"%s\" for encoding \"%s\" already exists in schema \"%s\"", collname, GetDatabaseEncodingName(), get_namespace_name(nspOid))));
  }

                                                   
  if (SearchSysCacheExists3(COLLNAMEENCNSP, CStringGetDatum(collname), Int32GetDatum(-1), ObjectIdGetDatum(nspOid)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("collation \"%s\" already exists in schema \"%s\"", collname, get_namespace_name(nspOid))));
  }
}

   
                   
   
ObjectAddress
AlterCollation(AlterCollationStmt *stmt)
{
  Relation rel;
  Oid collOid;
  HeapTuple tup;
  Form_pg_collation collForm;
  Datum collversion;
  bool isnull;
  char *oldversion;
  char *newversion;
  ObjectAddress address;

  rel = table_open(CollationRelationId, RowExclusiveLock);
  collOid = get_collation_oid(stmt->collname, false);

  if (!pg_collation_ownercheck(collOid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_COLLATION, NameListToString(stmt->collname));
  }

  tup = SearchSysCacheCopy1(COLLOID, ObjectIdGetDatum(collOid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for collation %u", collOid);
  }

  collForm = (Form_pg_collation)GETSTRUCT(tup);
  collversion = SysCacheGetAttr(COLLOID, tup, Anum_pg_collation_collversion, &isnull);
  oldversion = isnull ? NULL : TextDatumGetCString(collversion);

  newversion = get_collation_actual_version(collForm->collprovider, NameStr(collForm->collcollate));

                                                         
  if ((!oldversion && newversion) || (oldversion && !newversion))
  {
    elog(ERROR, "invalid collation version change");
  }
  else if (oldversion && newversion && strcmp(newversion, oldversion) != 0)
  {
    bool nulls[Natts_pg_collation];
    bool replaces[Natts_pg_collation];
    Datum values[Natts_pg_collation];

    ereport(NOTICE, (errmsg("changing version from %s to %s", oldversion, newversion)));

    memset(values, 0, sizeof(values));
    memset(nulls, false, sizeof(nulls));
    memset(replaces, false, sizeof(replaces));

    values[Anum_pg_collation_collversion - 1] = CStringGetTextDatum(newversion);
    replaces[Anum_pg_collation_collversion - 1] = true;

    tup = heap_modify_tuple(tup, RelationGetDescr(rel), values, nulls, replaces);
  }
  else
  {
    ereport(NOTICE, (errmsg("version has not changed")));
  }

  CatalogTupleUpdate(rel, &tup->t_self, tup);

  InvokeObjectPostAlterHook(CollationRelationId, collOid, 0);

  ObjectAddressSet(address, CollationRelationId, collOid);

  heap_freetuple(tup);
  table_close(rel, NoLock);

  return address;
}

Datum
pg_collation_actual_version(PG_FUNCTION_ARGS)
{
  Oid collid = PG_GETARG_OID(0);
  HeapTuple tp;
  char *collcollate;
  char collprovider;
  char *version;

  tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(collid));
  if (!HeapTupleIsValid(tp))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("collation with OID %u does not exist", collid)));
  }

  collcollate = pstrdup(NameStr(((Form_pg_collation)GETSTRUCT(tp))->collcollate));
  collprovider = ((Form_pg_collation)GETSTRUCT(tp))->collprovider;

  ReleaseSysCache(tp);

  version = get_collation_actual_version(collprovider, collcollate);

  if (version)
  {
    PG_RETURN_TEXT_P(cstring_to_text(version));
  }
  else
  {
    PG_RETURN_NULL();
  }
}

                                                             
#if defined(HAVE_LOCALE_T) && !defined(WIN32)
#define READ_LOCALE_A_OUTPUT
#endif

#if defined(READ_LOCALE_A_OUTPUT) || defined(USE_ICU)
   
                                             
   
static bool
is_all_ascii(const char *str)
{
  while (*str)
  {
    if (IS_HIGHBIT_SET(*str))
    {
      return false;
    }
    str++;
  }
  return true;
}
#endif                                      

#ifdef READ_LOCALE_A_OUTPUT
   
                                                                       
                                                                      
                                                               
              
   
static bool
normalize_libc_locale_name(char *new, const char *old)
{
  char *n = new;
  const char *o = old;
  bool changed = false;

  while (*o)
  {
    if (*o == '.')
    {
                                                              
      o++;
      while ((*o >= 'A' && *o <= 'Z') || (*o >= 'a' && *o <= 'z') || (*o >= '0' && *o <= '9') || (*o == '-'))
      {
        o++;
      }
      changed = true;
    }
    else
    {
      *n++ = *o++;
    }
  }
  *n = '\0';

  return changed;
}

   
                                            
   
static int
cmpaliases(const void *a, const void *b)
{
  const CollAliasData *ca = (const CollAliasData *)a;
  const CollAliasData *cb = (const CollAliasData *)b;

                                                                       
  return strcmp(ca->localename, cb->localename);
}
#endif                           

#ifdef USE_ICU
   
                                               
                                    
   
static char *
get_icu_language_tag(const char *localename)
{
  char buf[ULOC_FULLNAME_CAPACITY];
  UErrorCode status;

  status = U_ZERO_ERROR;
  uloc_toLanguageTag(localename, buf, sizeof(buf), true, &status);
  if (U_FAILURE(status))
  {
    ereport(ERROR, (errmsg("could not convert locale name \"%s\" to language tag: %s", localename, u_errorName(status))));
  }

  return pstrdup(buf);
}

   
                                                                     
                                                                      
                                                                    
                                                                           
   
static char *
get_icu_locale_comment(const char *localename)
{
  UErrorCode status;
  UChar displayname[128];
  int32 len_uchar;
  int32 i;
  char *result;

  status = U_ZERO_ERROR;
  len_uchar = uloc_getDisplayName(localename, "en", displayname, lengthof(displayname), &status);
  if (U_FAILURE(status))
  {
    return NULL;                                       
  }

                                                                     
  for (i = 0; i < len_uchar; i++)
  {
    if (displayname[i] > 127)
    {
      return NULL;
    }
  }

                      
  result = palloc(len_uchar + 1);
  for (i = 0; i < len_uchar; i++)
  {
    result[i] = displayname[i];
  }
  result[len_uchar] = '\0';

  return result;
}
#endif              

   
                                                                            
   
Datum
pg_import_system_collations(PG_FUNCTION_ARGS)
{
  Oid nspid = PG_GETARG_OID(0);
  int ncreated = 0;

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser to import system collations"))));
  }

  if (!SearchSysCacheExists1(NAMESPACEOID, ObjectIdGetDatum(nspid)))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("schema with OID %u does not exist", nspid)));
  }

                                                                          
#ifdef READ_LOCALE_A_OUTPUT
  {
    FILE *locale_a_handle;
    char localebuf[NAMEDATALEN];                                      
    int nvalid = 0;
    Oid collid;
    CollAliasData *aliases;
    int naliases, maxaliases, i;

                                     
    maxaliases = 100;
    aliases = (CollAliasData *)palloc(maxaliases * sizeof(CollAliasData));
    naliases = 0;

    locale_a_handle = OpenPipeStream("locale -a", "r");
    if (locale_a_handle == NULL)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not execute command \"%s\": %m", "locale -a")));
    }

    while (fgets(localebuf, sizeof(localebuf), locale_a_handle))
    {
      size_t len;
      int enc;
      char alias[NAMEDATALEN];

      len = strlen(localebuf);

      if (len == 0 || localebuf[len - 1] != '\n')
      {
        elog(DEBUG1, "locale name too long, skipped: \"%s\"", localebuf);
        continue;
      }
      localebuf[len - 1] = '\0';

         
                                                                       
                                                                      
                                                                  
                                                                   
                                       
         
      if (!is_all_ascii(localebuf))
      {
        elog(DEBUG1, "locale name has non-ASCII characters, skipped: \"%s\"", localebuf);
        continue;
      }

      enc = pg_get_encoding_from_locale(localebuf, false);
      if (enc < 0)
      {
                                                                    
        continue;
      }
      if (!PG_VALID_BE_ENCODING(enc))
      {
        continue;                                               
      }
      if (enc == PG_SQL_ASCII)
      {
        continue;                                         
      }

                                                         
      nvalid++;

         
                                                                      
                                                                      
                                                                        
                                                                   
                                                                     
                                                                    
                              
         
      collid = CollationCreate(localebuf, nspid, GetUserId(), COLLPROVIDER_LIBC, true, enc, localebuf, localebuf, get_collation_actual_version(COLLPROVIDER_LIBC, localebuf), true, true);
      if (OidIsValid(collid))
      {
        ncreated++;

                                                                        
        CommandCounterIncrement();
      }

         
                                                                      
                                                                    
                                                                       
              
         
                                                                       
                                                                         
                                          
         
      if (normalize_libc_locale_name(alias, localebuf))
      {
        if (naliases >= maxaliases)
        {
          maxaliases *= 2;
          aliases = (CollAliasData *)repalloc(aliases, maxaliases * sizeof(CollAliasData));
        }
        aliases[naliases].localename = pstrdup(localebuf);
        aliases[naliases].alias = pstrdup(alias);
        aliases[naliases].enc = enc;
        naliases++;
      }
    }

    ClosePipeStream(locale_a_handle);

       
                                                                           
                                                                           
                                                                           
                                                                      
                                                                           
                                                                     
                                                                      
                                                                       
                                                                        
       
    if (naliases > 1)
    {
      qsort((void *)aliases, naliases, sizeof(CollAliasData), cmpaliases);
    }

                                                                       
    for (i = 0; i < naliases; i++)
    {
      char *locale = aliases[i].localename;
      char *alias = aliases[i].alias;
      int enc = aliases[i].enc;

      collid = CollationCreate(alias, nspid, GetUserId(), COLLPROVIDER_LIBC, true, enc, locale, locale, get_collation_actual_version(COLLPROVIDER_LIBC, locale), true, true);
      if (OidIsValid(collid))
      {
        ncreated++;

        CommandCounterIncrement();
      }
    }

                                                                  
    if (nvalid == 0)
    {
      ereport(WARNING, (errmsg("no usable system locales were found")));
    }
  }
#endif                           

     
                                  
     
                                                                  
                                                                           
                                                                          
                                                                           
                                                                             
                
     
#ifdef USE_ICU
  {
    int i;

       
                                                                         
                         
       
    for (i = -1; i < uloc_countAvailable(); i++)
    {
      const char *name;
      char *langtag;
      char *icucomment;
      const char *collcollate;
      Oid collid;

      if (i == -1)
      {
        name = "";                      
      }
      else
      {
        name = uloc_getAvailable(i);
      }

      langtag = get_icu_language_tag(name);
      collcollate = U_ICU_VERSION_MAJOR_NUM >= 54 ? langtag : name;

         
                                                                   
                      
         
      if (!is_all_ascii(langtag) || !is_all_ascii(collcollate))
      {
        continue;
      }

      collid = CollationCreate(psprintf("%s-x-icu", langtag), nspid, GetUserId(), COLLPROVIDER_ICU, true, -1, collcollate, collcollate, get_collation_actual_version(COLLPROVIDER_ICU, collcollate), true, true);
      if (OidIsValid(collid))
      {
        ncreated++;

        CommandCounterIncrement();

        icucomment = get_icu_locale_comment(name);
        if (icucomment)
        {
          CreateComments(collid, CollationRelationId, 0, icucomment);
        }
      }
    }
  }
#endif              

  PG_RETURN_INT32(ncreated);
}
