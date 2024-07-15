                                                                            
   
         
                                                                      
   
                                                                         
                                                                        
   
   
                  
                                 
   
                                                                            
   
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_type.h"
#include "catalog/pg_class.h"
#include "commands/dbcommands.h"
#include "commands/proclang.h"
#include "commands/tablespace.h"
#include "foreign/foreign.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/hashutils.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

typedef struct
{
  const char *name;
  AclMode value;
} priv_map;

   
                                                                             
                                                                          
                                                                           
                                                                          
   
                                                                       
                                                                       
                                                         
   
                                                                             
                            
   
                           
                                                       
                                                                     
                                                     
                                                              
   
                                     
                                                        
                                                                           
                                              
                                                               
   
static Oid cached_privs_role = InvalidOid;
static List *cached_privs_roles = NIL;
static Oid cached_member_role = InvalidOid;
static List *cached_membership_roles = NIL;

static const char *
getid(const char *s, char *n);
static void
putid(char *p, const char *s);
static Acl *
allocacl(int n);
static void
check_acl(const Acl *acl);
static const char *
aclparse(const char *s, AclItem *aip);
static bool
aclitem_match(const AclItem *a1, const AclItem *a2);
static int
aclitemComparator(const void *arg1, const void *arg2);
static void
check_circularity(const Acl *old_acl, const AclItem *mod_aip, Oid ownerId);
static Acl *
recursive_revoke(Acl *acl, Oid grantee, AclMode revoke_privs, Oid ownerId, DropBehavior behavior);

static AclMode
convert_priv_string(text *priv_type_text);
static AclMode
convert_any_priv_string(text *priv_type_text, const priv_map *privileges);

static Oid
convert_table_name(text *tablename);
static AclMode
convert_table_priv_string(text *priv_type_text);
static AclMode
convert_sequence_priv_string(text *priv_type_text);
static AttrNumber
convert_column_name(Oid tableoid, text *column);
static AclMode
convert_column_priv_string(text *priv_type_text);
static Oid
convert_database_name(text *databasename);
static AclMode
convert_database_priv_string(text *priv_type_text);
static Oid
convert_foreign_data_wrapper_name(text *fdwname);
static AclMode
convert_foreign_data_wrapper_priv_string(text *priv_type_text);
static Oid
convert_function_name(text *functionname);
static AclMode
convert_function_priv_string(text *priv_type_text);
static Oid
convert_language_name(text *languagename);
static AclMode
convert_language_priv_string(text *priv_type_text);
static Oid
convert_schema_name(text *schemaname);
static AclMode
convert_schema_priv_string(text *priv_type_text);
static Oid
convert_server_name(text *servername);
static AclMode
convert_server_priv_string(text *priv_type_text);
static Oid
convert_tablespace_name(text *tablespacename);
static AclMode
convert_tablespace_priv_string(text *priv_type_text);
static Oid
convert_type_name(text *typename);
static AclMode
convert_type_priv_string(text *priv_type_text);
static AclMode
convert_role_priv_string(text *priv_type_text);
static AclResult
pg_role_aclcheck(Oid role_oid, Oid roleid, AclMode mode);

static void
RoleMembershipCacheCallback(Datum arg, int cacheid, uint32 hashvalue);

   
         
                                                                        
                                                                       
                                           
   
            
                                                                           
                                     
                                                                      
                                                                 
   
static const char *
getid(const char *s, char *n)
{
  int len = 0;
  bool in_quotes = false;

  Assert(s && n);

  while (isspace((unsigned char)*s))
  {
    s++;
  }
                                                           
  for (; *s != '\0' && (isalnum((unsigned char)*s) || *s == '_' || *s == '"' || in_quotes); s++)
  {
    if (*s == '"')
    {
                                                            
      if (*(s + 1) != '"')
      {
        in_quotes = !in_quotes;
        continue;
      }
                                                                
      s++;
    }

                                         
    if (len >= NAMEDATALEN - 1)
    {
      ereport(ERROR, (errcode(ERRCODE_NAME_TOO_LONG), errmsg("identifier too long"), errdetail("Identifier must be less than %d characters.", NAMEDATALEN)));
    }

    n[len++] = *s;
  }
  n[len] = '\0';
  while (isspace((unsigned char)*s))
  {
    s++;
  }
  return s;
}

   
                                                            
                                                                   
                                                                             
   
static void
putid(char *p, const char *s)
{
  const char *src;
  bool safe = true;

  for (src = s; *src; src++)
  {
                                                             
    if (!isalnum((unsigned char)*src) && *src != '_')
    {
      safe = false;
      break;
    }
  }
  if (!safe)
  {
    *p++ = '"';
  }
  for (src = s; *src; src++)
  {
                                                                 
    if (*src == '"')
    {
      *p++ = '"';
    }
    *p++ = *src;
  }
  if (!safe)
  {
    *p++ = '"';
  }
  *p = '\0';
}

   
            
                                                          
                                        
                                                                     
                                                                     
                       
   
                                                                 
                                                       
   
                                                                       
                          
   
            
                                                             
                          
                                                                   
                                                        
   
static const char *
aclparse(const char *s, AclItem *aip)
{
  AclMode privs, goption, read;
  char name[NAMEDATALEN];
  char name2[NAMEDATALEN];

  Assert(s && aip);

#ifdef ACLDEBUG
  elog(LOG, "aclparse: input = \"%s\"", s);
#endif
  s = getid(s, name);
  if (*s != '=')
  {
                                            
    if (strcmp(name, "group") != 0 && strcmp(name, "user") != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("unrecognized key word: \"%s\"", name), errhint("ACL key word must be \"group\" or \"user\".")));
    }
    s = getid(s, name);                                            
    if (name[0] == '\0')
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("missing name"), errhint("A name must follow the \"group\" or \"user\" key word.")));
    }
  }

  if (*s != '=')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("missing \"=\" sign")));
  }

  privs = goption = ACL_NO_RIGHTS;

  for (++s, read = 0; isalpha((unsigned char)*s) || *s == '*'; s++)
  {
    switch (*s)
    {
    case '*':
      goption |= read;
      break;
    case ACL_INSERT_CHR:
      read = ACL_INSERT;
      break;
    case ACL_SELECT_CHR:
      read = ACL_SELECT;
      break;
    case ACL_UPDATE_CHR:
      read = ACL_UPDATE;
      break;
    case ACL_DELETE_CHR:
      read = ACL_DELETE;
      break;
    case ACL_TRUNCATE_CHR:
      read = ACL_TRUNCATE;
      break;
    case ACL_REFERENCES_CHR:
      read = ACL_REFERENCES;
      break;
    case ACL_TRIGGER_CHR:
      read = ACL_TRIGGER;
      break;
    case ACL_EXECUTE_CHR:
      read = ACL_EXECUTE;
      break;
    case ACL_USAGE_CHR:
      read = ACL_USAGE;
      break;
    case ACL_CREATE_CHR:
      read = ACL_CREATE;
      break;
    case ACL_CREATE_TEMP_CHR:
      read = ACL_CREATE_TEMP;
      break;
    case ACL_CONNECT_CHR:
      read = ACL_CONNECT;
      break;
    case 'R':                                 
      read = 0;
      break;
    default:
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid mode character: must be one of \"%s\"", ACL_ALL_RIGHTS_STR)));
    }

    privs |= read;
  }

  if (name[0] == '\0')
  {
    aip->ai_grantee = ACL_ID_PUBLIC;
  }
  else
  {
    aip->ai_grantee = get_role_oid(name, false);
  }

     
                                                                            
                       
     
  if (*s == '/')
  {
    s = getid(s + 1, name2);
    if (name2[0] == '\0')
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("a name must follow the \"/\" sign")));
    }
    aip->ai_grantor = get_role_oid(name2, false);
  }
  else
  {
    aip->ai_grantor = BOOTSTRAP_SUPERUSERID;
    ereport(WARNING, (errcode(ERRCODE_INVALID_GRANTOR), errmsg("defaulting grantor to user ID %u", BOOTSTRAP_SUPERUSERID)));
  }

  ACLITEM_SET_PRIVS_GOPTIONS(*aip, privs, goption);

#ifdef ACLDEBUG
  elog(LOG, "aclparse: correctly read [%u %x %x]", aip->ai_grantee, privs, goption);
#endif

  return s;
}

   
            
                                                      
   
            
                
   
static Acl *
allocacl(int n)
{
  Acl *new_acl;
  Size size;

  if (n < 0)
  {
    elog(ERROR, "invalid size: %d", n);
  }
  size = ACL_N_SIZE(n);
  new_acl = (Acl *)palloc0(size);
  SET_VARSIZE(new_acl, size);
  new_acl->ndim = 1;
  new_acl->dataoffset = 0;                                
  new_acl->elemtype = ACLITEMOID;
  ARR_LBOUND(new_acl)[0] = 1;
  ARR_DIMS(new_acl)[0] = n;
  return new_acl;
}

   
                           
   
Acl *
make_empty_acl(void)
{
  return allocacl(0);
}

   
               
   
Acl *
aclcopy(const Acl *orig_acl)
{
  Acl *result_acl;

  result_acl = allocacl(ACL_NUM(orig_acl));

  memcpy(ACL_DAT(result_acl), ACL_DAT(orig_acl), ACL_NUM(orig_acl) * sizeof(AclItem));

  return result_acl;
}

   
                        
   
                                                                             
                                           
   
Acl *
aclconcat(const Acl *left_acl, const Acl *right_acl)
{
  Acl *result_acl;

  result_acl = allocacl(ACL_NUM(left_acl) + ACL_NUM(right_acl));

  memcpy(ACL_DAT(result_acl), ACL_DAT(left_acl), ACL_NUM(left_acl) * sizeof(AclItem));

  memcpy(ACL_DAT(result_acl) + ACL_NUM(left_acl), ACL_DAT(right_acl), ACL_NUM(right_acl) * sizeof(AclItem));

  return result_acl;
}

   
                  
   
                                                                  
                               
   
Acl *
aclmerge(const Acl *left_acl, const Acl *right_acl, Oid ownerId)
{
  Acl *result_acl;
  AclItem *aip;
  int i, num;

                                                        
  if (left_acl == NULL || ACL_NUM(left_acl) == 0)
  {
    if (right_acl == NULL || ACL_NUM(right_acl) == 0)
    {
      return NULL;
    }
    else
    {
      return aclcopy(right_acl);
    }
  }
  else
  {
    if (right_acl == NULL || ACL_NUM(right_acl) == 0)
    {
      return aclcopy(left_acl);
    }
  }

                                                   
  result_acl = aclcopy(left_acl);

  aip = ACL_DAT(right_acl);
  num = ACL_NUM(right_acl);

  for (i = 0; i < num; i++, aip++)
  {
    Acl *tmp_acl;

    tmp_acl = aclupdate(result_acl, aip, ACL_MODECHG_ADD, ownerId, DROP_RESTRICT);
    pfree(result_acl);
    result_acl = tmp_acl;
  }

  return result_acl;
}

   
                                                                     
   
void
aclitemsort(Acl *acl)
{
  if (acl != NULL && ACL_NUM(acl) > 1)
  {
    qsort(ACL_DAT(acl), ACL_NUM(acl), sizeof(AclItem), aclitemComparator);
  }
}

   
                                       
   
                                                                          
                                                                      
                        
   
bool
aclequal(const Acl *left_acl, const Acl *right_acl)
{
                                                        
  if (left_acl == NULL || ACL_NUM(left_acl) == 0)
  {
    if (right_acl == NULL || ACL_NUM(right_acl) == 0)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  else
  {
    if (right_acl == NULL || ACL_NUM(right_acl) == 0)
    {
      return false;
    }
  }

  if (ACL_NUM(left_acl) != ACL_NUM(right_acl))
  {
    return false;
  }

  if (memcmp(ACL_DAT(left_acl), ACL_DAT(right_acl), ACL_NUM(left_acl) * sizeof(AclItem)) == 0)
  {
    return true;
  }

  return false;
}

   
                                                                             
   
static void
check_acl(const Acl *acl)
{
  if (ARR_ELEMTYPE(acl) != ACLITEMOID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ACL array contains wrong data type")));
  }
  if (ARR_NDIM(acl) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ACL arrays must be one-dimensional")));
  }
  if (ARR_HASNULL(acl))
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("ACL arrays must not contain null values")));
  }
}

   
             
                                                                      
                                                                       
   
            
                    
   
Datum
aclitemin(PG_FUNCTION_ARGS)
{
  const char *s = PG_GETARG_CSTRING(0);
  AclItem *aip;

  aip = (AclItem *)palloc(sizeof(AclItem));
  s = aclparse(s, aip);
  while (isspace((unsigned char)*s))
  {
    ++s;
  }
  if (*s)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("extra garbage at the end of the ACL specification")));
  }

  PG_RETURN_ACLITEM_P(aip);
}

   
              
                                                                     
                                                                         
   
            
                   
   
Datum
aclitemout(PG_FUNCTION_ARGS)
{
  AclItem *aip = PG_GETARG_ACLITEM_P(0);
  char *p;
  char *out;
  HeapTuple htup;
  unsigned i;

  out = palloc(strlen("=/") + 2 * N_ACL_RIGHTS + 2 * (2 * NAMEDATALEN + 2) + 1);

  p = out;
  *p = '\0';

  if (aip->ai_grantee != ACL_ID_PUBLIC)
  {
    htup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(aip->ai_grantee));
    if (HeapTupleIsValid(htup))
    {
      putid(p, NameStr(((Form_pg_authid)GETSTRUCT(htup))->rolname));
      ReleaseSysCache(htup);
    }
    else
    {
                                                          
      sprintf(p, "%u", aip->ai_grantee);
    }
  }
  while (*p)
  {
    ++p;
  }

  *p++ = '=';

  for (i = 0; i < N_ACL_RIGHTS; ++i)
  {
    if (ACLITEM_GET_PRIVS(*aip) & (1 << i))
    {
      *p++ = ACL_ALL_RIGHTS_STR[i];
    }
    if (ACLITEM_GET_GOPTIONS(*aip) & (1 << i))
    {
      *p++ = '*';
    }
  }

  *p++ = '/';
  *p = '\0';

  htup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(aip->ai_grantor));
  if (HeapTupleIsValid(htup))
  {
    putid(p, NameStr(((Form_pg_authid)GETSTRUCT(htup))->rolname));
    ReleaseSysCache(htup);
  }
  else
  {
                                                        
    sprintf(p, "%u", aip->ai_grantor);
  }

  PG_RETURN_CSTRING(out);
}

   
                 
                                                                
                                                     
   
static bool
aclitem_match(const AclItem *a1, const AclItem *a2)
{
  return a1->ai_grantee == a2->ai_grantee && a1->ai_grantor == a2->ai_grantor;
}

   
                     
                                           
   
static int
aclitemComparator(const void *arg1, const void *arg2)
{
  const AclItem *a1 = (const AclItem *)arg1;
  const AclItem *a2 = (const AclItem *)arg2;

  if (a1->ai_grantee > a2->ai_grantee)
  {
    return 1;
  }
  if (a1->ai_grantee < a2->ai_grantee)
  {
    return -1;
  }
  if (a1->ai_grantor > a2->ai_grantor)
  {
    return 1;
  }
  if (a1->ai_grantor < a2->ai_grantor)
  {
    return -1;
  }
  if (a1->ai_privs > a2->ai_privs)
  {
    return 1;
  }
  if (a1->ai_privs < a2->ai_privs)
  {
    return -1;
  }
  return 0;
}

   
                             
   
Datum
aclitem_eq(PG_FUNCTION_ARGS)
{
  AclItem *a1 = PG_GETARG_ACLITEM_P(0);
  AclItem *a2 = PG_GETARG_ACLITEM_P(1);
  bool result;

  result = a1->ai_privs == a2->ai_privs && a1->ai_grantee == a2->ai_grantee && a1->ai_grantor == a2->ai_grantor;
  PG_RETURN_BOOL(result);
}

   
                         
   
                                                                          
                                                                          
                                                                     
   
Datum
hash_aclitem(PG_FUNCTION_ARGS)
{
  AclItem *a = PG_GETARG_ACLITEM_P(0);

                                                                  
  PG_RETURN_UINT32((uint32)(a->ai_privs + a->ai_grantee + a->ai_grantor));
}

   
                                     
   
                                                                           
   
Datum
hash_aclitem_extended(PG_FUNCTION_ARGS)
{
  AclItem *a = PG_GETARG_ACLITEM_P(0);
  uint64 seed = PG_GETARG_INT64(1);
  uint32 sum = (uint32)(a->ai_privs + a->ai_grantee + a->ai_grantor);

  return (seed == 0) ? UInt64GetDatum(sum) : hash_uint32_extended(sum, seed);
}

   
                                                                         
   
                                                                          
                                                                      
                                                                      
                                               
   
                                                                      
                                        
   
Acl *
acldefault(ObjectType objtype, Oid ownerId)
{
  AclMode world_default;
  AclMode owner_default;
  int nacl;
  Acl *acl;
  AclItem *aip;

  switch (objtype)
  {
  case OBJECT_COLUMN:
                                                      
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_NO_RIGHTS;
    break;
  case OBJECT_TABLE:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_RELATION;
    break;
  case OBJECT_SEQUENCE:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_SEQUENCE;
    break;
  case OBJECT_DATABASE:
                                                                   
    world_default = ACL_CREATE_TEMP | ACL_CONNECT;
    owner_default = ACL_ALL_RIGHTS_DATABASE;
    break;
  case OBJECT_FUNCTION:
                                           
    world_default = ACL_EXECUTE;
    owner_default = ACL_ALL_RIGHTS_FUNCTION;
    break;
  case OBJECT_LANGUAGE:
                                         
    world_default = ACL_USAGE;
    owner_default = ACL_ALL_RIGHTS_LANGUAGE;
    break;
  case OBJECT_LARGEOBJECT:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_LARGEOBJECT;
    break;
  case OBJECT_SCHEMA:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_SCHEMA;
    break;
  case OBJECT_TABLESPACE:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_TABLESPACE;
    break;
  case OBJECT_FDW:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_FDW;
    break;
  case OBJECT_FOREIGN_SERVER:
    world_default = ACL_NO_RIGHTS;
    owner_default = ACL_ALL_RIGHTS_FOREIGN_SERVER;
    break;
  case OBJECT_DOMAIN:
  case OBJECT_TYPE:
    world_default = ACL_USAGE;
    owner_default = ACL_ALL_RIGHTS_TYPE;
    break;
  default:
    elog(ERROR, "unrecognized objtype: %d", (int)objtype);
    world_default = ACL_NO_RIGHTS;                          
    owner_default = ACL_NO_RIGHTS;
    break;
  }

  nacl = 0;
  if (world_default != ACL_NO_RIGHTS)
  {
    nacl++;
  }
  if (owner_default != ACL_NO_RIGHTS)
  {
    nacl++;
  }

  acl = allocacl(nacl);
  aip = ACL_DAT(acl);

  if (world_default != ACL_NO_RIGHTS)
  {
    aip->ai_grantee = ACL_ID_PUBLIC;
    aip->ai_grantor = ownerId;
    ACLITEM_SET_PRIVS_GOPTIONS(*aip, world_default, ACL_NO_RIGHTS);
    aip++;
  }

     
                                                                            
                                                                            
                                                                           
                                                                      
                                                                        
                                                                        
                                                                      
                                            
     
  if (owner_default != ACL_NO_RIGHTS)
  {
    aip->ai_grantee = ownerId;
    aip->ai_grantor = ownerId;
    ACLITEM_SET_PRIVS_GOPTIONS(*aip, owner_default, ACL_NO_RIGHTS);
  }

  return acl;
}

   
                                                                                
                    
   
Datum
acldefault_sql(PG_FUNCTION_ARGS)
{
  char objtypec = PG_GETARG_CHAR(0);
  Oid owner = PG_GETARG_OID(1);
  ObjectType objtype = 0;

  switch (objtypec)
  {
  case 'c':
    objtype = OBJECT_COLUMN;
    break;
  case 'r':
    objtype = OBJECT_TABLE;
    break;
  case 's':
    objtype = OBJECT_SEQUENCE;
    break;
  case 'd':
    objtype = OBJECT_DATABASE;
    break;
  case 'f':
    objtype = OBJECT_FUNCTION;
    break;
  case 'l':
    objtype = OBJECT_LANGUAGE;
    break;
  case 'L':
    objtype = OBJECT_LARGEOBJECT;
    break;
  case 'n':
    objtype = OBJECT_SCHEMA;
    break;
  case 't':
    objtype = OBJECT_TABLESPACE;
    break;
  case 'F':
    objtype = OBJECT_FDW;
    break;
  case 'S':
    objtype = OBJECT_FOREIGN_SERVER;
    break;
  case 'T':
    objtype = OBJECT_TYPE;
    break;
  default:
    elog(ERROR, "unrecognized objtype abbreviation: %c", objtypec);
  }

  PG_RETURN_ACL_P(acldefault(objtype, owner));
}

   
                                                              
   
                                
                                                                        
                                                                 
                                
                                                                
   
                                                                              
                              
   
                                                                   
   
                                                                            
   
Acl *
aclupdate(const Acl *old_acl, const AclItem *mod_aip, int modechg, Oid ownerId, DropBehavior behavior)
{
  Acl *new_acl = NULL;
  AclItem *old_aip, *new_aip = NULL;
  AclMode old_rights, old_goptions, new_rights, new_goptions;
  int dst, num;

                                                            
  check_acl(old_acl);

                                                        
  if (modechg != ACL_MODECHG_DEL && ACLITEM_GET_GOPTIONS(*mod_aip) != ACL_NO_RIGHTS)
  {
    check_circularity(old_acl, mod_aip, ownerId);
  }

  num = ACL_NUM(old_acl);
  old_aip = ACL_DAT(old_acl);

     
                                                                           
                                                                             
                                                                          
              
     

  for (dst = 0; dst < num; ++dst)
  {
    if (aclitem_match(mod_aip, old_aip + dst))
    {
                                                  
      new_acl = allocacl(num);
      new_aip = ACL_DAT(new_acl);
      memcpy(new_acl, old_acl, ACL_SIZE(old_acl));
      break;
    }
  }

  if (dst == num)
  {
                                   
    new_acl = allocacl(num + 1);
    new_aip = ACL_DAT(new_acl);
    memcpy(new_aip, old_aip, num * sizeof(AclItem));

                                                      
    new_aip[dst].ai_grantee = mod_aip->ai_grantee;
    new_aip[dst].ai_grantor = mod_aip->ai_grantor;
    ACLITEM_SET_PRIVS_GOPTIONS(new_aip[dst], ACL_NO_RIGHTS, ACL_NO_RIGHTS);
    num++;                                     
  }

  old_rights = ACLITEM_GET_RIGHTS(new_aip[dst]);
  old_goptions = ACLITEM_GET_GOPTIONS(new_aip[dst]);

                                              
  switch (modechg)
  {
  case ACL_MODECHG_ADD:
    ACLITEM_SET_RIGHTS(new_aip[dst], old_rights | ACLITEM_GET_RIGHTS(*mod_aip));
    break;
  case ACL_MODECHG_DEL:
    ACLITEM_SET_RIGHTS(new_aip[dst], old_rights & ~ACLITEM_GET_RIGHTS(*mod_aip));
    break;
  case ACL_MODECHG_EQL:
    ACLITEM_SET_RIGHTS(new_aip[dst], ACLITEM_GET_RIGHTS(*mod_aip));
    break;
  }

  new_rights = ACLITEM_GET_RIGHTS(new_aip[dst]);
  new_goptions = ACLITEM_GET_GOPTIONS(new_aip[dst]);

     
                                                                        
     
  if (new_rights == ACL_NO_RIGHTS)
  {
    memmove(new_aip + dst, new_aip + dst + 1, (num - dst - 1) * sizeof(AclItem));
                                                 
    ARR_DIMS(new_acl)[0] = num - 1;
    SET_VARSIZE(new_acl, ACL_N_SIZE(num - 1));
  }

     
                                                                            
                                                 
     
  if ((old_goptions & ~new_goptions) != 0)
  {
    Assert(mod_aip->ai_grantee != ACL_ID_PUBLIC);
    new_acl = recursive_revoke(new_acl, mod_aip->ai_grantee, (old_goptions & ~new_goptions), ownerId, behavior);
  }

  return new_acl;
}

   
                                                                         
   
                                                   
                                           
                                           
   
                                                                   
   
                                                                            
   
Acl *
aclnewowner(const Acl *old_acl, Oid oldOwnerId, Oid newOwnerId)
{
  Acl *new_acl;
  AclItem *new_aip;
  AclItem *old_aip;
  AclItem *dst_aip;
  AclItem *src_aip;
  AclItem *targ_aip;
  bool newpresent = false;
  int dst, src, targ, num;

  check_acl(old_acl);

     
                                                                     
                                                                             
                                  
     
  num = ACL_NUM(old_acl);
  old_aip = ACL_DAT(old_acl);
  new_acl = allocacl(num);
  new_aip = ACL_DAT(new_acl);
  memcpy(new_aip, old_aip, num * sizeof(AclItem));
  for (dst = 0, dst_aip = new_aip; dst < num; dst++, dst_aip++)
  {
    if (dst_aip->ai_grantor == oldOwnerId)
    {
      dst_aip->ai_grantor = newOwnerId;
    }
    else if (dst_aip->ai_grantor == newOwnerId)
    {
      newpresent = true;
    }
    if (dst_aip->ai_grantee == oldOwnerId)
    {
      dst_aip->ai_grantee = newOwnerId;
    }
    else if (dst_aip->ai_grantee == newOwnerId)
    {
      newpresent = true;
    }
  }

     
                                                                           
                                                                            
                                                                       
                                                                            
                          
     
                                                                             
                                                                            
                                                                            
                                                                            
                                                                          
                                                                             
                                                                    
                                                        
     
  if (newpresent)
  {
    dst = 0;
    for (targ = 0, targ_aip = new_aip; targ < num; targ++, targ_aip++)
    {
                                                
      if (ACLITEM_GET_RIGHTS(*targ_aip) == ACL_NO_RIGHTS)
      {
        continue;
      }
                                         
      for (src = targ + 1, src_aip = targ_aip + 1; src < num; src++, src_aip++)
      {
        if (ACLITEM_GET_RIGHTS(*src_aip) == ACL_NO_RIGHTS)
        {
          continue;
        }
        if (aclitem_match(targ_aip, src_aip))
        {
          ACLITEM_SET_RIGHTS(*targ_aip, ACLITEM_GET_RIGHTS(*targ_aip) | ACLITEM_GET_RIGHTS(*src_aip));
                                          
          ACLITEM_SET_RIGHTS(*src_aip, ACL_NO_RIGHTS);
        }
      }
                              
      new_aip[dst] = *targ_aip;
      dst++;
    }
                                             
    ARR_DIMS(new_acl)[0] = dst;
    SET_VARSIZE(new_acl, ACL_N_SIZE(dst));
  }

  return new_acl;
}

   
                                                                             
                                                                        
                                                                        
                                                                            
                                                                            
                           
   
                                                                            
                                                                             
                        
   
static void
check_circularity(const Acl *old_acl, const AclItem *mod_aip, Oid ownerId)
{
  Acl *acl;
  AclItem *aip;
  int i, num;
  AclMode own_privs;

  check_acl(old_acl);

     
                                                                      
                                                    
     
  Assert(mod_aip->ai_grantee != ACL_ID_PUBLIC);

                                                            
  if (mod_aip->ai_grantor == ownerId)
  {
    return;
  }

                           
  acl = allocacl(ACL_NUM(old_acl));
  memcpy(acl, old_acl, ACL_SIZE(old_acl));

                                                                         
cc_restart:
  num = ACL_NUM(acl);
  aip = ACL_DAT(acl);
  for (i = 0; i < num; i++)
  {
    if (aip[i].ai_grantee == mod_aip->ai_grantee && ACLITEM_GET_GOPTIONS(aip[i]) != ACL_NO_RIGHTS)
    {
      Acl *new_acl;

                                                                
      new_acl = aclupdate(acl, &aip[i], ACL_MODECHG_DEL, ownerId, DROP_CASCADE);

      pfree(acl);
      acl = new_acl;

      goto cc_restart;
    }
  }

                                                                     
  own_privs = aclmask(acl, mod_aip->ai_grantor, ownerId, ACL_GRANT_OPTION_FOR(ACLITEM_GET_GOPTIONS(*mod_aip)), ACLMASK_ALL);
  own_privs = ACL_OPTION_TO_PRIVS(own_privs);

  if ((ACLITEM_GET_GOPTIONS(*mod_aip) & ~own_privs) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_GRANT_OPERATION), errmsg("grant options cannot be granted back to your own grantor")));
  }

  pfree(acl);
}

   
                                                                      
                                                                       
                                                                  
                                                                    
                                                   
   
                           
                                                                    
                                                 
                                
                                                                
   
                                                
   
static Acl *
recursive_revoke(Acl *acl, Oid grantee, AclMode revoke_privs, Oid ownerId, DropBehavior behavior)
{
  AclMode still_has;
  AclItem *aip;
  int i, num;

  check_acl(acl);

                                                                      
  if (grantee == ownerId)
  {
    return acl;
  }

                                                                           
  still_has = aclmask(acl, grantee, ownerId, ACL_GRANT_OPTION_FOR(revoke_privs), ACLMASK_ALL);
  revoke_privs &= ~ACL_OPTION_TO_PRIVS(still_has);
  if (revoke_privs == ACL_NO_RIGHTS)
  {
    return acl;
  }

restart:
  num = ACL_NUM(acl);
  aip = ACL_DAT(acl);
  for (i = 0; i < num; i++)
  {
    if (aip[i].ai_grantor == grantee && (ACLITEM_GET_PRIVS(aip[i]) & revoke_privs) != 0)
    {
      AclItem mod_acl;
      Acl *new_acl;

      if (behavior == DROP_RESTRICT)
      {
        ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("dependent privileges exist"), errhint("Use CASCADE to revoke them too.")));
      }

      mod_acl.ai_grantor = grantee;
      mod_acl.ai_grantee = aip[i].ai_grantee;
      ACLITEM_SET_PRIVS_GOPTIONS(mod_acl, revoke_privs, revoke_privs);

      new_acl = aclupdate(acl, &mod_acl, ACL_MODECHG_DEL, ownerId, behavior);

      pfree(acl);
      acl = new_acl;

      goto restart;
    }
  }

  return acl;
}

   
                                                                 
   
                                                                    
                                                                   
                                                                    
                                                              
   
                                                                    
                                                                 
                                                             
   
                   
   
                                                  
                                                                
   
                                                  
                                                                    
   
                                                               
                                                                   
   
AclMode
aclmask(const Acl *acl, Oid roleid, Oid ownerId, AclMode mask, AclMaskHow how)
{
  AclMode result;
  AclMode remaining;
  AclItem *aidat;
  int i, num;

     
                                                                   
                         
     
  if (acl == NULL)
  {
    elog(ERROR, "null ACL");
  }

  check_acl(acl);

                                
  if (mask == 0)
  {
    return 0;
  }

  result = 0;

                                                     
  if ((mask & ACLITEM_ALL_GOPTION_BITS) && has_privs_of_role(roleid, ownerId))
  {
    result = mask & ACLITEM_ALL_GOPTION_BITS;
    if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
    {
      return result;
    }
  }

  num = ACL_NUM(acl);
  aidat = ACL_DAT(acl);

     
                                                              
     
  for (i = 0; i < num; i++)
  {
    AclItem *aidata = &aidat[i];

    if (aidata->ai_grantee == ACL_ID_PUBLIC || aidata->ai_grantee == roleid)
    {
      result |= aidata->ai_privs & mask;
      if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
      {
        return result;
      }
    }
  }

     
                                                                             
                                                                          
                                                                         
                                                                          
           
     
  remaining = mask & ~result;
  for (i = 0; i < num; i++)
  {
    AclItem *aidata = &aidat[i];

    if (aidata->ai_grantee == ACL_ID_PUBLIC || aidata->ai_grantee == roleid)
    {
      continue;                         
    }

    if ((aidata->ai_privs & remaining) && has_privs_of_role(roleid, aidata->ai_grantee))
    {
      result |= aidata->ai_privs & mask;
      if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
      {
        return result;
      }
      remaining = mask & ~result;
    }
  }

  return result;
}

   
                                                                        
   
                                                                          
                                                                       
   
static AclMode
aclmask_direct(const Acl *acl, Oid roleid, Oid ownerId, AclMode mask, AclMaskHow how)
{
  AclMode result;
  AclItem *aidat;
  int i, num;

     
                                                                   
                         
     
  if (acl == NULL)
  {
    elog(ERROR, "null ACL");
  }

  check_acl(acl);

                                
  if (mask == 0)
  {
    return 0;
  }

  result = 0;

                                                     
  if ((mask & ACLITEM_ALL_GOPTION_BITS) && roleid == ownerId)
  {
    result = mask & ACLITEM_ALL_GOPTION_BITS;
    if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
    {
      return result;
    }
  }

  num = ACL_NUM(acl);
  aidat = ACL_DAT(acl);

     
                                                                     
     
  for (i = 0; i < num; i++)
  {
    AclItem *aidata = &aidat[i];

    if (aidata->ai_grantee == roleid)
    {
      result |= aidata->ai_privs & mask;
      if ((how == ACLMASK_ALL) ? (result == mask) : (result != 0))
      {
        return result;
      }
    }
  }

  return result;
}

   
              
                                                  
                                                            
   
                                                                         
                                                                     
   
int
aclmembers(const Acl *acl, Oid **roleids)
{
  Oid *list;
  const AclItem *acldat;
  int i, j, k;

  if (acl == NULL || ACL_NUM(acl) == 0)
  {
    *roleids = NULL;
    return 0;
  }

  check_acl(acl);

                                                 
  list = palloc(ACL_NUM(acl) * 2 * sizeof(Oid));
  acldat = ACL_DAT(acl);

     
                                                
     
  j = 0;
  for (i = 0; i < ACL_NUM(acl); i++)
  {
    const AclItem *ai = &acldat[i];

    if (ai->ai_grantee != ACL_ID_PUBLIC)
    {
      list[j++] = ai->ai_grantee;
    }
                                                                   
    if (ai->ai_grantor != ACL_ID_PUBLIC)
    {
      list[j++] = ai->ai_grantor;
    }
  }

                      
  qsort(list, j, sizeof(Oid), oid_cmp);

                                        
  k = 0;
  for (i = 1; i < j; i++)
  {
    if (list[k] != list[i])
    {
      list[++k] = list[i];
    }
  }

     
                                                                             
                                          
     
  *roleids = list;

  return k + 1;
}

   
                                 
   
Datum
aclinsert(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("aclinsert is no longer supported")));

  PG_RETURN_NULL();                          
}

Datum
aclremove(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("aclremove is no longer supported")));

  PG_RETURN_NULL();                          
}

Datum
aclcontains(PG_FUNCTION_ARGS)
{
  Acl *acl = PG_GETARG_ACL_P(0);
  AclItem *aip = PG_GETARG_ACLITEM_P(1);
  AclItem *aidat;
  int i, num;

  check_acl(acl);
  num = ACL_NUM(acl);
  aidat = ACL_DAT(acl);
  for (i = 0; i < num; ++i)
  {
    if (aip->ai_grantee == aidat[i].ai_grantee && aip->ai_grantor == aidat[i].ai_grantor && (ACLITEM_GET_RIGHTS(*aip) & ACLITEM_GET_RIGHTS(aidat[i])) == ACLITEM_GET_RIGHTS(*aip))
    {
      PG_RETURN_BOOL(true);
    }
  }
  PG_RETURN_BOOL(false);
}

Datum
makeaclitem(PG_FUNCTION_ARGS)
{
  Oid grantee = PG_GETARG_OID(0);
  Oid grantor = PG_GETARG_OID(1);
  text *privtext = PG_GETARG_TEXT_PP(2);
  bool goption = PG_GETARG_BOOL(3);
  AclItem *result;
  AclMode priv;

  priv = convert_priv_string(privtext);

  result = (AclItem *)palloc(sizeof(AclItem));

  result->ai_grantee = grantee;
  result->ai_grantor = grantor;

  ACLITEM_SET_PRIVS_GOPTIONS(*result, priv, (goption ? priv : ACL_NO_RIGHTS));

  PG_RETURN_ACLITEM_P(result);
}

static AclMode
convert_priv_string(text *priv_type_text)
{
  char *priv_type = text_to_cstring(priv_type_text);

  if (pg_strcasecmp(priv_type, "SELECT") == 0)
  {
    return ACL_SELECT;
  }
  if (pg_strcasecmp(priv_type, "INSERT") == 0)
  {
    return ACL_INSERT;
  }
  if (pg_strcasecmp(priv_type, "UPDATE") == 0)
  {
    return ACL_UPDATE;
  }
  if (pg_strcasecmp(priv_type, "DELETE") == 0)
  {
    return ACL_DELETE;
  }
  if (pg_strcasecmp(priv_type, "TRUNCATE") == 0)
  {
    return ACL_TRUNCATE;
  }
  if (pg_strcasecmp(priv_type, "REFERENCES") == 0)
  {
    return ACL_REFERENCES;
  }
  if (pg_strcasecmp(priv_type, "TRIGGER") == 0)
  {
    return ACL_TRIGGER;
  }
  if (pg_strcasecmp(priv_type, "EXECUTE") == 0)
  {
    return ACL_EXECUTE;
  }
  if (pg_strcasecmp(priv_type, "USAGE") == 0)
  {
    return ACL_USAGE;
  }
  if (pg_strcasecmp(priv_type, "CREATE") == 0)
  {
    return ACL_CREATE;
  }
  if (pg_strcasecmp(priv_type, "TEMP") == 0)
  {
    return ACL_CREATE_TEMP;
  }
  if (pg_strcasecmp(priv_type, "TEMPORARY") == 0)
  {
    return ACL_CREATE_TEMP;
  }
  if (pg_strcasecmp(priv_type, "CONNECT") == 0)
  {
    return ACL_CONNECT;
  }
  if (pg_strcasecmp(priv_type, "RULE") == 0)
  {
    return 0;                                 
  }

  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized privilege type: \"%s\"", priv_type)));
  return ACL_NO_RIGHTS;                          
}

   
                                                                              
   
                                                                         
                                                                         
                                                                        
                                                                          
                                               
   
static AclMode
convert_any_priv_string(text *priv_type_text, const priv_map *privileges)
{
  AclMode result = 0;
  char *priv_type = text_to_cstring(priv_type_text);
  char *chunk;
  char *next_chunk;

                                                               
  for (chunk = priv_type; chunk; chunk = next_chunk)
  {
    int chunk_len;
    const priv_map *this_priv;

                                
    next_chunk = strchr(chunk, ',');
    if (next_chunk)
    {
      *next_chunk++ = '\0';
    }

                                                        
    while (*chunk && isspace((unsigned char)*chunk))
    {
      chunk++;
    }
    chunk_len = strlen(chunk);
    while (chunk_len > 0 && isspace((unsigned char)chunk[chunk_len - 1]))
    {
      chunk_len--;
    }
    chunk[chunk_len] = '\0';

                                      
    for (this_priv = privileges; this_priv->name; this_priv++)
    {
      if (pg_strcasecmp(this_priv->name, chunk) == 0)
      {
        result |= this_priv->value;
        break;
      }
    }
    if (!this_priv->name)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized privilege type: \"%s\"", chunk)));
    }
  }

  pfree(priv_type);
  return result;
}

static const char *
convert_aclright_to_string(int aclright)
{
  switch (aclright)
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
    return "TEMPORARY";
  case ACL_CONNECT:
    return "CONNECT";
  default:
    elog(ERROR, "unrecognized aclright: %d", aclright);
    return NULL;
  }
}

             
                                    
   
            
   
                                                 
   
                     
   
                                             
                                           
                                            
             
   
Datum
aclexplode(PG_FUNCTION_ARGS)
{
  Acl *acl = PG_GETARG_ACL_P(0);
  FuncCallContext *funcctx;
  int *idx;
  AclItem *aidat;

  if (SRF_IS_FIRSTCALL())
  {
    TupleDesc tupdesc;
    MemoryContext oldcontext;

    check_acl(acl);

    funcctx = SRF_FIRSTCALL_INIT();
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

       
                                                                          
              
       
    tupdesc = CreateTemplateTupleDesc(4);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "grantor", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "grantee", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "privilege_type", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)4, "is_grantable", BOOLOID, -1, 0);

    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

                                          
    idx = (int *)palloc(sizeof(int[2]));
    idx[0] = 0;                            
    idx[1] = -1;                             
    funcctx->user_fctx = (void *)idx;

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  idx = (int *)funcctx->user_fctx;
  aidat = ACL_DAT(acl);

                                               
  while (idx[0] < ACL_NUM(acl))
  {
    AclItem *aidata;
    AclMode priv_bit;

    idx[1]++;
    if (idx[1] == N_ACL_RIGHTS)
    {
      idx[1] = 0;
      idx[0]++;
      if (idx[0] >= ACL_NUM(acl))           
      {
        break;
      }
    }
    aidata = &aidat[idx[0]];
    priv_bit = 1 << idx[1];

    if (ACLITEM_GET_PRIVS(*aidata) & priv_bit)
    {
      Datum result;
      Datum values[4];
      bool nulls[4];
      HeapTuple tuple;

      values[0] = ObjectIdGetDatum(aidata->ai_grantor);
      values[1] = ObjectIdGetDatum(aidata->ai_grantee);
      values[2] = CStringGetTextDatum(convert_aclright_to_string(priv_bit));
      values[3] = BoolGetDatum((ACLITEM_GET_GOPTIONS(*aidata) & priv_bit) != 0);

      MemSet(nulls, 0, sizeof(nulls));

      tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
      result = HeapTupleGetDatum(tuple);

      SRF_RETURN_NEXT(funcctx, result);
    }
  }

  SRF_RETURN_DONE(funcctx);
}

   
                                
                                                                
                                                                   
                                                          
   
                                                                  
                                                                    
                                                                  
                                   
   

   
                                 
                                           
                                                       
   
Datum
has_table_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name rolename = PG_GETARG_NAME(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid tableoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*rolename));
  tableoid = convert_table_name(tablename);
  mode = convert_table_priv_string(priv_type_text);

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                            
                                           
                                       
                            
   
Datum
has_table_privilege_name(PG_FUNCTION_ARGS)
{
  text *tablename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid tableoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  tableoid = convert_table_name(tablename);
  mode = convert_table_priv_string(priv_type_text);

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                           
                                                 
   
Datum
has_table_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid tableoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_table_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                          
                                           
                                   
                            
   
Datum
has_table_privilege_id(PG_FUNCTION_ARGS)
{
  Oid tableoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_table_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                           
                                                
   
Datum
has_table_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid tableoid;
  AclMode mode;
  AclResult aclresult;

  tableoid = convert_table_name(tablename);
  mode = convert_table_priv_string(priv_type_text);

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                           
                                           
   
Datum
has_table_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid tableoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_table_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                     
   

   
                                                                       
   
static Oid
convert_table_name(text *tablename)
{
  RangeVar *relrv;

  relrv = makeRangeVarFromNameList(textToQualifiedNameList(tablename));

                                                                           
  return RangeVarGetRelid(relrv, NoLock, false);
}

   
                             
                                          
   
static AclMode
convert_table_priv_string(text *priv_type_text)
{
  static const priv_map table_priv_map[] = {{"SELECT", ACL_SELECT}, {"SELECT WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_SELECT)}, {"INSERT", ACL_INSERT}, {"INSERT WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_INSERT)}, {"UPDATE", ACL_UPDATE}, {"UPDATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_UPDATE)}, {"DELETE", ACL_DELETE}, {"DELETE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_DELETE)}, {"TRUNCATE", ACL_TRUNCATE}, {"TRUNCATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_TRUNCATE)}, {"REFERENCES", ACL_REFERENCES}, {"REFERENCES WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_REFERENCES)}, {"TRIGGER", ACL_TRIGGER}, {"TRIGGER WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_TRIGGER)}, {"RULE", 0},                                 
      {"RULE WITH GRANT OPTION", 0}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, table_priv_map);
}

   
                                   
                                                                   
                                                                   
                                                          
   
                                                                  
                                                                    
                                          
   

   
                                    
                                              
                                                          
   
Datum
has_sequence_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name rolename = PG_GETARG_NAME(0);
  text *sequencename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid sequenceoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*rolename));
  mode = convert_sequence_priv_string(priv_type_text);
  sequenceoid = convert_table_name(sequencename);
  if (get_rel_relkind(sequenceoid) != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", text_to_cstring(sequencename))));
  }

  aclresult = pg_class_aclcheck(sequenceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                              
                                          
                            
   
Datum
has_sequence_privilege_name(PG_FUNCTION_ARGS)
{
  text *sequencename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid sequenceoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_sequence_priv_string(priv_type_text);
  sequenceoid = convert_table_name(sequencename);
  if (get_rel_relkind(sequenceoid) != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", text_to_cstring(sequencename))));
  }

  aclresult = pg_class_aclcheck(sequenceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                    
   
Datum
has_sequence_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid sequenceoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;
  char relkind;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_sequence_priv_string(priv_type_text);
  relkind = get_rel_relkind(sequenceoid);
  if (relkind == '\0')
  {
    PG_RETURN_NULL();
  }
  else if (relkind != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", get_rel_name(sequenceoid))));
  }

  aclresult = pg_class_aclcheck(sequenceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                              
                                      
                            
   
Datum
has_sequence_privilege_id(PG_FUNCTION_ARGS)
{
  Oid sequenceoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;
  char relkind;

  roleid = GetUserId();
  mode = convert_sequence_priv_string(priv_type_text);
  relkind = get_rel_relkind(sequenceoid);
  if (relkind == '\0')
  {
    PG_RETURN_NULL();
  }
  else if (relkind != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", get_rel_name(sequenceoid))));
  }

  aclresult = pg_class_aclcheck(sequenceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                   
   
Datum
has_sequence_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *sequencename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid sequenceoid;
  AclMode mode;
  AclResult aclresult;

  mode = convert_sequence_priv_string(priv_type_text);
  sequenceoid = convert_table_name(sequencename);
  if (get_rel_relkind(sequenceoid) != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", text_to_cstring(sequencename))));
  }

  aclresult = pg_class_aclcheck(sequenceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                              
                                              
   
Datum
has_sequence_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid sequenceoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;
  char relkind;

  mode = convert_sequence_priv_string(priv_type_text);
  relkind = get_rel_relkind(sequenceoid);
  if (relkind == '\0')
  {
    PG_RETURN_NULL();
  }
  else if (relkind != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", get_rel_name(sequenceoid))));
  }

  aclresult = pg_class_aclcheck(sequenceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                          
   
static AclMode
convert_sequence_priv_string(text *priv_type_text)
{
  static const priv_map sequence_priv_map[] = {{"USAGE", ACL_USAGE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_USAGE)}, {"SELECT", ACL_SELECT}, {"SELECT WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_SELECT)}, {"UPDATE", ACL_UPDATE}, {"UPDATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_UPDATE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, sequence_priv_map);
}

   
                                     
                                                                     
                                                                   
                                                          
   
                                                                  
                                                                       
                                                                   
   

   
                                      
                                                         
                                                       
   
Datum
has_any_column_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name rolename = PG_GETARG_NAME(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid tableoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*rolename));
  tableoid = convert_table_name(tablename);
  mode = convert_column_priv_string(priv_type_text);

                                                                      
  aclresult = pg_class_aclcheck(tableoid, roleid, mode);
  if (aclresult != ACLCHECK_OK)
  {
    aclresult = pg_attribute_aclcheck_all(tableoid, roleid, mode, ACLMASK_ANY);
  }

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                 
                                                         
                                       
                            
   
Datum
has_any_column_privilege_name(PG_FUNCTION_ARGS)
{
  text *tablename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid tableoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  tableoid = convert_table_name(tablename);
  mode = convert_column_priv_string(priv_type_text);

                                                                      
  aclresult = pg_class_aclcheck(tableoid, roleid, mode);
  if (aclresult != ACLCHECK_OK)
  {
    aclresult = pg_attribute_aclcheck_all(tableoid, roleid, mode, ACLMASK_ANY);
  }

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                    
                                                         
                                                 
   
Datum
has_any_column_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid tableoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_column_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    PG_RETURN_NULL();
  }

                                                                      
  aclresult = pg_class_aclcheck(tableoid, roleid, mode);
  if (aclresult != ACLCHECK_OK)
  {
    aclresult = pg_attribute_aclcheck_all(tableoid, roleid, mode, ACLMASK_ANY);
  }

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                                         
                                   
                            
   
Datum
has_any_column_privilege_id(PG_FUNCTION_ARGS)
{
  Oid tableoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_column_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    PG_RETURN_NULL();
  }

                                                                      
  aclresult = pg_class_aclcheck(tableoid, roleid, mode);
  if (aclresult != ACLCHECK_OK)
  {
    aclresult = pg_attribute_aclcheck_all(tableoid, roleid, mode, ACLMASK_ANY);
  }

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                    
                                                         
                                                
   
Datum
has_any_column_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid tableoid;
  AclMode mode;
  AclResult aclresult;

  tableoid = convert_table_name(tablename);
  mode = convert_column_priv_string(priv_type_text);

                                                                      
  aclresult = pg_class_aclcheck(tableoid, roleid, mode);
  if (aclresult != ACLCHECK_OK)
  {
    aclresult = pg_attribute_aclcheck_all(tableoid, roleid, mode, ACLMASK_ANY);
  }

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                                         
                                           
   
Datum
has_any_column_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid tableoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_column_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    PG_RETURN_NULL();
  }

                                                                      
  aclresult = pg_class_aclcheck(tableoid, roleid, mode);
  if (aclresult != ACLCHECK_OK)
  {
    aclresult = pg_attribute_aclcheck_all(tableoid, roleid, mode, ACLMASK_ANY);
  }

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                 
                                                                 
                                                                   
                                                        
                                  
   
                                                                  
                                                                    
                                                                     
                                                                       
                                                                    
                                                                     
                                                                    
                                                
   

   
                                                                             
                                
   
                                                                          
   
static int
column_privilege_check(Oid tableoid, AttrNumber attnum, Oid roleid, AclMode mode)
{
  AclResult aclresult;
  HeapTuple attTuple;
  Form_pg_attribute attributeForm;

     
                                                                       
     
  if (attnum == InvalidAttrNumber)
  {
    return -1;
  }

     
                                                                        
                                                                             
                                                                           
                                                                          
                                                                    
                        
     
  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(tableoid)))
  {
    return -1;
  }

  aclresult = pg_class_aclcheck(tableoid, roleid, mode);

  if (aclresult == ACLCHECK_OK)
  {
    return true;
  }

     
                                                                          
                                                                           
                                                                            
     
  attTuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(tableoid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(attTuple))
  {
    return -1;
  }
  attributeForm = (Form_pg_attribute)GETSTRUCT(attTuple);
  if (attributeForm->attisdropped)
  {
    ReleaseSysCache(attTuple);
    return -1;
  }
  ReleaseSysCache(attTuple);

  aclresult = pg_attribute_aclcheck(tableoid, attnum, roleid, mode);

  return (aclresult == ACLCHECK_OK);
}

   
                                       
                                            
                                                                     
   
Datum
has_column_privilege_name_name_name(PG_FUNCTION_ARGS)
{
  Name rolename = PG_GETARG_NAME(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  text *column = PG_GETARG_TEXT_PP(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  Oid roleid;
  Oid tableoid;
  AttrNumber colattnum;
  AclMode mode;
  int privresult;

  roleid = get_role_oid_or_public(NameStr(*rolename));
  tableoid = convert_table_name(tablename);
  colattnum = convert_column_name(tableoid, column);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                         
                                            
                                                                   
   
Datum
has_column_privilege_name_name_attnum(PG_FUNCTION_ARGS)
{
  Name rolename = PG_GETARG_NAME(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  AttrNumber colattnum = PG_GETARG_INT16(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  Oid roleid;
  Oid tableoid;
  AclMode mode;
  int privresult;

  roleid = get_role_oid_or_public(NameStr(*rolename));
  tableoid = convert_table_name(tablename);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                     
                                            
                                                                
   
Datum
has_column_privilege_name_id_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid tableoid = PG_GETARG_OID(1);
  text *column = PG_GETARG_TEXT_PP(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  Oid roleid;
  AttrNumber colattnum;
  AclMode mode;
  int privresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  colattnum = convert_column_name(tableoid, column);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                       
                                            
                                                              
   
Datum
has_column_privilege_name_id_attnum(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid tableoid = PG_GETARG_OID(1);
  AttrNumber colattnum = PG_GETARG_INT16(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  Oid roleid;
  AclMode mode;
  int privresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                     
                                            
                                                                  
   
Datum
has_column_privilege_id_name_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  text *column = PG_GETARG_TEXT_PP(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  Oid tableoid;
  AttrNumber colattnum;
  AclMode mode;
  int privresult;

  tableoid = convert_table_name(tablename);
  colattnum = convert_column_name(tableoid, column);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                       
                                            
                                                                
   
Datum
has_column_privilege_id_name_attnum(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *tablename = PG_GETARG_TEXT_PP(1);
  AttrNumber colattnum = PG_GETARG_INT16(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  Oid tableoid;
  AclMode mode;
  int privresult;

  tableoid = convert_table_name(tablename);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                   
                                            
                                                             
   
Datum
has_column_privilege_id_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid tableoid = PG_GETARG_OID(1);
  text *column = PG_GETARG_TEXT_PP(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  AttrNumber colattnum;
  AclMode mode;
  int privresult;

  colattnum = convert_column_name(tableoid, column);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                     
                                            
                                                           
   
Datum
has_column_privilege_id_id_attnum(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid tableoid = PG_GETARG_OID(1);
  AttrNumber colattnum = PG_GETARG_INT16(2);
  text *priv_type_text = PG_GETARG_TEXT_PP(3);
  AclMode mode;
  int privresult;

  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                  
                                            
                                                      
                            
   
Datum
has_column_privilege_name_name(PG_FUNCTION_ARGS)
{
  text *tablename = PG_GETARG_TEXT_PP(0);
  text *column = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid tableoid;
  AttrNumber colattnum;
  AclMode mode;
  int privresult;

  roleid = GetUserId();
  tableoid = convert_table_name(tablename);
  colattnum = convert_column_name(tableoid, column);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                    
                                            
                                                    
                            
   
Datum
has_column_privilege_name_attnum(PG_FUNCTION_ARGS)
{
  text *tablename = PG_GETARG_TEXT_PP(0);
  AttrNumber colattnum = PG_GETARG_INT16(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid tableoid;
  AclMode mode;
  int privresult;

  roleid = GetUserId();
  tableoid = convert_table_name(tablename);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                
                                            
                                                 
                            
   
Datum
has_column_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid tableoid = PG_GETARG_OID(0);
  text *column = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AttrNumber colattnum;
  AclMode mode;
  int privresult;

  roleid = GetUserId();
  colattnum = convert_column_name(tableoid, column);
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                  
                                            
                                               
                            
   
Datum
has_column_privilege_id_attnum(PG_FUNCTION_ARGS)
{
  Oid tableoid = PG_GETARG_OID(0);
  AttrNumber colattnum = PG_GETARG_INT16(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  int privresult;

  roleid = GetUserId();
  mode = convert_column_priv_string(priv_type_text);

  privresult = column_privilege_check(tableoid, colattnum, roleid, mode);
  if (privresult < 0)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_BOOL(privresult);
}

   
                                                      
   

   
                                                                         
                                                                     
                                                       
   
static AttrNumber
convert_column_name(Oid tableoid, text *column)
{
  char *colname;
  HeapTuple attTuple;
  AttrNumber attnum;

  colname = text_to_cstring(column);

     
                                                                        
                                                                             
                          
     
  attTuple = SearchSysCache2(ATTNAME, ObjectIdGetDatum(tableoid), CStringGetDatum(colname));
  if (HeapTupleIsValid(attTuple))
  {
    Form_pg_attribute attributeForm;

    attributeForm = (Form_pg_attribute)GETSTRUCT(attTuple);
                                                    
    if (attributeForm->attisdropped)
    {
      attnum = InvalidAttrNumber;
    }
    else
    {
      attnum = attributeForm->attnum;
    }
    ReleaseSysCache(attTuple);
  }
  else
  {
    char *tablename = get_rel_name(tableoid);

       
                                                                       
                                                                        
                                                   
       
    if (tablename != NULL)
    {
                                                             
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colname, tablename)));
    }
                                                               
    attnum = InvalidAttrNumber;
  }

  pfree(colname);
  return attnum;
}

   
                              
                                          
   
static AclMode
convert_column_priv_string(text *priv_type_text)
{
  static const priv_map column_priv_map[] = {{"SELECT", ACL_SELECT}, {"SELECT WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_SELECT)}, {"INSERT", ACL_INSERT}, {"INSERT WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_INSERT)}, {"UPDATE", ACL_UPDATE}, {"UPDATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_UPDATE)}, {"REFERENCES", ACL_REFERENCES}, {"REFERENCES WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_REFERENCES)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, column_priv_map);
}

   
                                   
                                                                   
                                                                   
                                                          
   
                                                                  
                                                              
   

   
                                    
                                              
                                                          
   
Datum
has_database_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *databasename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid databaseoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  databaseoid = convert_database_name(databasename);
  mode = convert_database_priv_string(priv_type_text);

  aclresult = pg_database_aclcheck(databaseoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                              
                                          
                            
   
Datum
has_database_privilege_name(PG_FUNCTION_ARGS)
{
  text *databasename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid databaseoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  databaseoid = convert_database_name(databasename);
  mode = convert_database_priv_string(priv_type_text);

  aclresult = pg_database_aclcheck(databaseoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                    
   
Datum
has_database_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid databaseoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_database_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(DATABASEOID, ObjectIdGetDatum(databaseoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_database_aclcheck(databaseoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                              
                                      
                            
   
Datum
has_database_privilege_id(PG_FUNCTION_ARGS)
{
  Oid databaseoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_database_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(DATABASEOID, ObjectIdGetDatum(databaseoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_database_aclcheck(databaseoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                   
   
Datum
has_database_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *databasename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid databaseoid;
  AclMode mode;
  AclResult aclresult;

  databaseoid = convert_database_name(databasename);
  mode = convert_database_priv_string(priv_type_text);

  aclresult = pg_database_aclcheck(databaseoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                              
                                              
   
Datum
has_database_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid databaseoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_database_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(DATABASEOID, ObjectIdGetDatum(databaseoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_database_aclcheck(databaseoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                        
   

   
                                                                          
   
static Oid
convert_database_name(text *databasename)
{
  char *dbname = text_to_cstring(databasename);

  return get_database_oid(dbname, false);
}

   
                                
                                          
   
static AclMode
convert_database_priv_string(text *priv_type_text)
{
  static const priv_map database_priv_map[] = {{"CREATE", ACL_CREATE}, {"CREATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {"TEMPORARY", ACL_CREATE_TEMP}, {"TEMPORARY WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE_TEMP)}, {"TEMP", ACL_CREATE_TEMP}, {"TEMP WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE_TEMP)}, {"CONNECT", ACL_CONNECT}, {"CONNECT WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CONNECT)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, database_priv_map);
}

   
                                               
                                                                               
                                                                 
                                                                   
   
                                                                  
                             
   

   
                                                
                                                          
                                                     
   
Datum
has_foreign_data_wrapper_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *fdwname = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid fdwid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  fdwid = convert_foreign_data_wrapper_name(fdwname);
  mode = convert_foreign_data_wrapper_priv_string(priv_type_text);

  aclresult = pg_foreign_data_wrapper_aclcheck(fdwid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                           
                                                          
                                     
                            
   
Datum
has_foreign_data_wrapper_privilege_name(PG_FUNCTION_ARGS)
{
  text *fdwname = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid fdwid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  fdwid = convert_foreign_data_wrapper_name(fdwname);
  mode = convert_foreign_data_wrapper_priv_string(priv_type_text);

  aclresult = pg_foreign_data_wrapper_aclcheck(fdwid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                              
                                                          
                                                                
   
Datum
has_foreign_data_wrapper_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid fdwid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_foreign_data_wrapper_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_foreign_data_wrapper_aclcheck(fdwid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                         
                                                          
                                                  
                            
   
Datum
has_foreign_data_wrapper_privilege_id(PG_FUNCTION_ARGS)
{
  Oid fdwid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_foreign_data_wrapper_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_foreign_data_wrapper_aclcheck(fdwid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                              
                                                          
                                              
   
Datum
has_foreign_data_wrapper_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *fdwname = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid fdwid;
  AclMode mode;
  AclResult aclresult;

  fdwid = convert_foreign_data_wrapper_name(fdwname);
  mode = convert_foreign_data_wrapper_priv_string(priv_type_text);

  aclresult = pg_foreign_data_wrapper_aclcheck(fdwid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                            
                                                          
                                         
   
Datum
has_foreign_data_wrapper_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid fdwid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_foreign_data_wrapper_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(FOREIGNDATAWRAPPEROID, ObjectIdGetDatum(fdwid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_foreign_data_wrapper_aclcheck(fdwid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                                    
   

   
                                                                     
   
static Oid
convert_foreign_data_wrapper_name(text *fdwname)
{
  char *fdwstr = text_to_cstring(fdwname);

  return get_foreign_data_wrapper_oid(fdwstr, false);
}

   
                                            
                                          
   
static AclMode
convert_foreign_data_wrapper_priv_string(text *priv_type_text)
{
  static const priv_map foreign_data_wrapper_priv_map[] = {{"USAGE", ACL_USAGE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_USAGE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, foreign_data_wrapper_priv_map);
}

   
                                   
                                                                   
                                                                   
                                                          
   
                                                                  
                                                              
   

   
                                    
                                              
                                                          
   
Datum
has_function_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *functionname = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid functionoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  functionoid = convert_function_name(functionname);
  mode = convert_function_priv_string(priv_type_text);

  aclresult = pg_proc_aclcheck(functionoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                              
                                          
                            
   
Datum
has_function_privilege_name(PG_FUNCTION_ARGS)
{
  text *functionname = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid functionoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  functionoid = convert_function_name(functionname);
  mode = convert_function_priv_string(priv_type_text);

  aclresult = pg_proc_aclcheck(functionoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                    
   
Datum
has_function_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid functionoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_function_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(PROCOID, ObjectIdGetDatum(functionoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_proc_aclcheck(functionoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                              
                                      
                            
   
Datum
has_function_privilege_id(PG_FUNCTION_ARGS)
{
  Oid functionoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_function_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(PROCOID, ObjectIdGetDatum(functionoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_proc_aclcheck(functionoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                   
   
Datum
has_function_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *functionname = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid functionoid;
  AclMode mode;
  AclResult aclresult;

  functionoid = convert_function_name(functionname);
  mode = convert_function_priv_string(priv_type_text);

  aclresult = pg_proc_aclcheck(functionoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                              
                                              
   
Datum
has_function_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid functionoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_function_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(PROCOID, ObjectIdGetDatum(functionoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_proc_aclcheck(functionoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                        
   

   
                                                                          
   
static Oid
convert_function_name(text *functionname)
{
  char *funcname = text_to_cstring(functionname);
  Oid oid;

  oid = DatumGetObjectId(DirectFunctionCall1(regprocedurein, CStringGetDatum(funcname)));

  if (!OidIsValid(oid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("function \"%s\" does not exist", funcname)));
  }

  return oid;
}

   
                                
                                          
   
static AclMode
convert_function_priv_string(text *priv_type_text)
{
  static const priv_map function_priv_map[] = {{"EXECUTE", ACL_EXECUTE}, {"EXECUTE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_EXECUTE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, function_priv_map);
}

   
                                   
                                                                   
                                                                   
                                                          
   
                                                                  
                                                              
   

   
                                    
                                              
                                                          
   
Datum
has_language_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *languagename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid languageoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  languageoid = convert_language_name(languagename);
  mode = convert_language_priv_string(priv_type_text);

  aclresult = pg_language_aclcheck(languageoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                              
                                          
                            
   
Datum
has_language_privilege_name(PG_FUNCTION_ARGS)
{
  text *languagename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid languageoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  languageoid = convert_language_name(languagename);
  mode = convert_language_priv_string(priv_type_text);

  aclresult = pg_language_aclcheck(languageoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                    
   
Datum
has_language_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid languageoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_language_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(LANGOID, ObjectIdGetDatum(languageoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_language_aclcheck(languageoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                              
                                      
                            
   
Datum
has_language_privilege_id(PG_FUNCTION_ARGS)
{
  Oid languageoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_language_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(LANGOID, ObjectIdGetDatum(languageoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_language_aclcheck(languageoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                              
                                                   
   
Datum
has_language_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *languagename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid languageoid;
  AclMode mode;
  AclResult aclresult;

  languageoid = convert_language_name(languagename);
  mode = convert_language_priv_string(priv_type_text);

  aclresult = pg_language_aclcheck(languageoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                              
                                              
   
Datum
has_language_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid languageoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_language_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(LANGOID, ObjectIdGetDatum(languageoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_language_aclcheck(languageoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                        
   

   
                                                                          
   
static Oid
convert_language_name(text *languagename)
{
  char *langname = text_to_cstring(languagename);

  return get_language_oid(langname, false);
}

   
                                
                                          
   
static AclMode
convert_language_priv_string(text *priv_type_text)
{
  static const priv_map language_priv_map[] = {{"USAGE", ACL_USAGE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_USAGE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, language_priv_map);
}

   
                                 
                                                                 
                                                               
                                                          
   
                                                                  
                                                              
   

   
                                  
                                            
                                                        
   
Datum
has_schema_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *schemaname = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid schemaoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  schemaoid = convert_schema_name(schemaname);
  mode = convert_schema_priv_string(priv_type_text);

  aclresult = pg_namespace_aclcheck(schemaoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                            
                                        
                            
   
Datum
has_schema_privilege_name(PG_FUNCTION_ARGS)
{
  text *schemaname = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid schemaoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  schemaoid = convert_schema_name(schemaname);
  mode = convert_schema_priv_string(priv_type_text);

  aclresult = pg_namespace_aclcheck(schemaoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                            
                                                  
   
Datum
has_schema_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid schemaoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_schema_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(NAMESPACEOID, ObjectIdGetDatum(schemaoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_namespace_aclcheck(schemaoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                           
                                            
                                    
                            
   
Datum
has_schema_privilege_id(PG_FUNCTION_ARGS)
{
  Oid schemaoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_schema_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(NAMESPACEOID, ObjectIdGetDatum(schemaoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_namespace_aclcheck(schemaoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                            
                                                 
   
Datum
has_schema_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *schemaname = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid schemaoid;
  AclMode mode;
  AclResult aclresult;

  schemaoid = convert_schema_name(schemaname);
  mode = convert_schema_priv_string(priv_type_text);

  aclresult = pg_namespace_aclcheck(schemaoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                              
                                            
                                            
   
Datum
has_schema_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid schemaoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_schema_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(NAMESPACEOID, ObjectIdGetDatum(schemaoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_namespace_aclcheck(schemaoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                      
   

   
                                                                        
   
static Oid
convert_schema_name(text *schemaname)
{
  char *nspname = text_to_cstring(schemaname);

  return get_namespace_oid(nspname, false);
}

   
                              
                                          
   
static AclMode
convert_schema_priv_string(text *priv_type_text)
{
  static const priv_map schema_priv_map[] = {{"CREATE", ACL_CREATE}, {"CREATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {"USAGE", ACL_USAGE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_USAGE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, schema_priv_map);
}

   
                                 
                                                                 
                                                           
                                                                      
   
                                                                  
                             
   

   
                                  
                                                    
                                                        
   
Datum
has_server_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *servername = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid serverid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  serverid = convert_server_name(servername);
  mode = convert_server_priv_string(priv_type_text);

  aclresult = pg_foreign_server_aclcheck(serverid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                             
                                                    
                                        
                            
   
Datum
has_server_privilege_name(PG_FUNCTION_ARGS)
{
  text *servername = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid serverid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  serverid = convert_server_name(servername);
  mode = convert_server_priv_string(priv_type_text);

  aclresult = pg_foreign_server_aclcheck(serverid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                                    
                                                          
   
Datum
has_server_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid serverid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_server_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_foreign_server_aclcheck(serverid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                           
                                                    
                                    
                            
   
Datum
has_server_privilege_id(PG_FUNCTION_ARGS)
{
  Oid serverid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_server_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_foreign_server_aclcheck(serverid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                
                                                    
                                                 
   
Datum
has_server_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *servername = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid serverid;
  AclMode mode;
  AclResult aclresult;

  serverid = convert_server_name(servername);
  mode = convert_server_priv_string(priv_type_text);

  aclresult = pg_foreign_server_aclcheck(serverid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                              
                                                    
                                            
   
Datum
has_server_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid serverid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_server_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(FOREIGNSERVEROID, ObjectIdGetDatum(serverid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_foreign_server_aclcheck(serverid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                      
   

   
                                                                        
   
static Oid
convert_server_name(text *servername)
{
  char *serverstr = text_to_cstring(servername);

  return get_foreign_server_oid(serverstr, false);
}

   
                              
                                          
   
static AclMode
convert_server_priv_string(text *priv_type_text)
{
  static const priv_map server_priv_map[] = {{"USAGE", ACL_USAGE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_USAGE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, server_priv_map);
}

   
                                     
                                                                     
                                                                       
                                                          
   
                                                                  
                             
   

   
                                      
                                                
                                                            
   
Datum
has_tablespace_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *tablespacename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid tablespaceoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  tablespaceoid = convert_tablespace_name(tablespacename);
  mode = convert_tablespace_priv_string(priv_type_text);

  aclresult = pg_tablespace_aclcheck(tablespaceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                 
                                                
                                            
                            
   
Datum
has_tablespace_privilege_name(PG_FUNCTION_ARGS)
{
  text *tablespacename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid tablespaceoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  tablespaceoid = convert_tablespace_name(tablespacename);
  mode = convert_tablespace_priv_string(priv_type_text);

  aclresult = pg_tablespace_aclcheck(tablespaceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                    
                                                
                                                      
   
Datum
has_tablespace_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid tablespaceoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_tablespace_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(TABLESPACEOID, ObjectIdGetDatum(tablespaceoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_tablespace_aclcheck(tablespaceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                               
                                                
                                        
                            
   
Datum
has_tablespace_privilege_id(PG_FUNCTION_ARGS)
{
  Oid tablespaceoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_tablespace_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(TABLESPACEOID, ObjectIdGetDatum(tablespaceoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_tablespace_aclcheck(tablespaceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                    
                                                
                                                     
   
Datum
has_tablespace_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *tablespacename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid tablespaceoid;
  AclMode mode;
  AclResult aclresult;

  tablespaceoid = convert_tablespace_name(tablespacename);
  mode = convert_tablespace_priv_string(priv_type_text);

  aclresult = pg_tablespace_aclcheck(tablespaceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                  
                                                
                                                
   
Datum
has_tablespace_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid tablespaceoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_tablespace_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(TABLESPACEOID, ObjectIdGetDatum(tablespaceoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_tablespace_aclcheck(tablespaceoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                          
   

   
                                                                            
   
static Oid
convert_tablespace_name(text *tablespacename)
{
  char *spcname = text_to_cstring(tablespacename);

  return get_tablespace_oid(spcname, false);
}

   
                                  
                                          
   
static AclMode
convert_tablespace_priv_string(text *priv_type_text)
{
  static const priv_map tablespace_priv_map[] = {{"CREATE", ACL_CREATE}, {"CREATE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, tablespace_priv_map);
}

   
                               
                                                               
                                                           
                                                          
   
                                                                  
                                                              
   

   
                                
                                          
                                                      
   
Datum
has_type_privilege_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  text *typename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid typeoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  typeoid = convert_type_name(typename);
  mode = convert_type_priv_string(priv_type_text);

  aclresult = pg_type_aclcheck(typeoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                           
                                          
                                      
                            
   
Datum
has_type_privilege_name(PG_FUNCTION_ARGS)
{
  text *typename = PG_GETARG_TEXT_PP(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid typeoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  typeoid = convert_type_name(typename);
  mode = convert_type_priv_string(priv_type_text);

  aclresult = pg_type_aclcheck(typeoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                              
                                          
                                                
   
Datum
has_type_privilege_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid typeoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid_or_public(NameStr(*username));
  mode = convert_type_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(TYPEOID, ObjectIdGetDatum(typeoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_type_aclcheck(typeoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                         
                                          
                                  
                            
   
Datum
has_type_privilege_id(PG_FUNCTION_ARGS)
{
  Oid typeoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_type_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(TYPEOID, ObjectIdGetDatum(typeoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_type_aclcheck(typeoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                              
                                          
                                               
   
Datum
has_type_privilege_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  text *typename = PG_GETARG_TEXT_PP(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid typeoid;
  AclMode mode;
  AclResult aclresult;

  typeoid = convert_type_name(typename);
  mode = convert_type_priv_string(priv_type_text);

  aclresult = pg_type_aclcheck(typeoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                            
                                          
                                          
   
Datum
has_type_privilege_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid typeoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_type_priv_string(priv_type_text);

  if (!SearchSysCacheExists1(TYPEOID, ObjectIdGetDatum(typeoid)))
  {
    PG_RETURN_NULL();
  }

  aclresult = pg_type_aclcheck(typeoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                                    
   

   
                                                                      
   
static Oid
convert_type_name(text *typename)
{
  char *typname = text_to_cstring(typename);
  Oid oid;

  oid = DatumGetObjectId(DirectFunctionCall1(regtypein, CStringGetDatum(typname)));

  if (!OidIsValid(oid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" does not exist", typname)));
  }

  return oid;
}

   
                            
                                          
   
static AclMode
convert_type_priv_string(text *priv_type_text)
{
  static const priv_map type_priv_map[] = {{"USAGE", ACL_USAGE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_USAGE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, type_priv_map);
}

   
                        
                                                        
                                                           
                                                          
   
                                                                  
                             
   

   
                         
                                          
                                                      
   
Datum
pg_has_role_name_name(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Name rolename = PG_GETARG_NAME(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  Oid roleoid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid(NameStr(*username), false);
  roleoid = get_role_oid(NameStr(*rolename), false);
  mode = convert_role_priv_string(priv_type_text);

  aclresult = pg_role_aclcheck(roleoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                    
                                          
                                      
                            
   
Datum
pg_has_role_name(PG_FUNCTION_ARGS)
{
  Name rolename = PG_GETARG_NAME(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  Oid roleoid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  roleoid = get_role_oid(NameStr(*rolename), false);
  mode = convert_role_priv_string(priv_type_text);

  aclresult = pg_role_aclcheck(roleoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                       
                                          
                                                
   
Datum
pg_has_role_name_id(PG_FUNCTION_ARGS)
{
  Name username = PG_GETARG_NAME(0);
  Oid roleoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = get_role_oid(NameStr(*username), false);
  mode = convert_role_priv_string(priv_type_text);

  aclresult = pg_role_aclcheck(roleoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                  
                                          
                                  
                            
   
Datum
pg_has_role_id(PG_FUNCTION_ARGS)
{
  Oid roleoid = PG_GETARG_OID(0);
  text *priv_type_text = PG_GETARG_TEXT_PP(1);
  Oid roleid;
  AclMode mode;
  AclResult aclresult;

  roleid = GetUserId();
  mode = convert_role_priv_string(priv_type_text);

  aclresult = pg_role_aclcheck(roleoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                       
                                          
                                               
   
Datum
pg_has_role_id_name(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Name rolename = PG_GETARG_NAME(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  Oid roleoid;
  AclMode mode;
  AclResult aclresult;

  roleoid = get_role_oid(NameStr(*rolename), false);
  mode = convert_role_priv_string(priv_type_text);

  aclresult = pg_role_aclcheck(roleoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                     
                                          
                                          
   
Datum
pg_has_role_id_id(PG_FUNCTION_ARGS)
{
  Oid roleid = PG_GETARG_OID(0);
  Oid roleoid = PG_GETARG_OID(1);
  text *priv_type_text = PG_GETARG_TEXT_PP(2);
  AclMode mode;
  AclResult aclresult;

  mode = convert_role_priv_string(priv_type_text);

  aclresult = pg_role_aclcheck(roleoid, roleid, mode);

  PG_RETURN_BOOL(aclresult == ACLCHECK_OK);
}

   
                                             
   

   
                            
                                          
   
                                                                            
                                                                         
                                                                            
                                                                       
                                                
   
static AclMode
convert_role_priv_string(text *priv_type_text)
{
  static const priv_map role_priv_map[] = {{"USAGE", ACL_USAGE}, {"MEMBER", ACL_CREATE}, {"USAGE WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {"USAGE WITH ADMIN OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {"MEMBER WITH GRANT OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {"MEMBER WITH ADMIN OPTION", ACL_GRANT_OPTION_FOR(ACL_CREATE)}, {NULL, 0}};

  return convert_any_priv_string(priv_type_text, role_priv_map);
}

   
                    
                                            
   
static AclResult
pg_role_aclcheck(Oid role_oid, Oid roleid, AclMode mode)
{
  if (mode & ACL_GRANT_OPTION_FOR(ACL_CREATE))
  {
       
                                                                        
                                                                           
                                                                
       
    if (is_admin_of_role(roleid, role_oid))
    {
      return ACLCHECK_OK;
    }
  }
  if (mode & ACL_CREATE)
  {
    if (is_member_of_role(roleid, role_oid))
    {
      return ACLCHECK_OK;
    }
  }
  if (mode & ACL_USAGE)
  {
    if (has_privs_of_role(roleid, role_oid))
    {
      return ACLCHECK_OK;
    }
  }
  return ACLCHECK_NO_PRIV;
}

   
                                                    
   
void
initialize_acl(void)
{
  if (!IsBootstrapProcessingMode())
  {
       
                                                                           
                                                                    
                                        
       
    CacheRegisterSyscacheCallback(AUTHMEMROLEMEM, RoleMembershipCacheCallback, (Datum)0);
    CacheRegisterSyscacheCallback(AUTHOID, RoleMembershipCacheCallback, (Datum)0);
  }
}

   
                               
                                     
   
static void
RoleMembershipCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{
                                                            
  cached_privs_role = InvalidOid;
  cached_member_role = InvalidOid;
}

                                                
static bool
has_rolinherit(Oid roleid)
{
  bool result = false;
  HeapTuple utup;

  utup = SearchSysCache1(AUTHOID, ObjectIdGetDatum(roleid));
  if (HeapTupleIsValid(utup))
  {
    result = ((Form_pg_authid)GETSTRUCT(utup))->rolinherit;
    ReleaseSysCache(utup);
  }
  return result;
}

   
                                                                       
   
                                                                           
                                                                           
                                                          
   
                                                                       
                                                                           
                                          
   
                                                                       
                                                             
   
static List *
roles_has_privs_of(Oid roleid)
{
  List *roles_list;
  ListCell *l;
  List *new_cached_privs_roles;
  MemoryContext oldctx;

                                                       
  if (OidIsValid(cached_privs_role) && cached_privs_role == roleid)
  {
    return cached_privs_roles;
  }

     
                                                                          
                                                                         
                     
     
                                                                        
                                                                  
                                                                          
                                                                        
                                                               
     
  roles_list = list_make1_oid(roleid);

  foreach (l, roles_list)
  {
    Oid memberid = lfirst_oid(l);
    CatCList *memlist;
    int i;

                                     
    if (!has_rolinherit(memberid))
    {
      continue;
    }

                                                          
    memlist = SearchSysCacheList1(AUTHMEMMEMROLE, ObjectIdGetDatum(memberid));
    for (i = 0; i < memlist->n_members; i++)
    {
      HeapTuple tup = &memlist->members[i]->tuple;
      Oid otherid = ((Form_pg_auth_members)GETSTRUCT(tup))->roleid;

         
                                                                    
                                                                      
                                                           
         
      roles_list = list_append_unique_oid(roles_list, otherid);
    }
    ReleaseSysCacheList(memlist);
  }

     
                                                                       
     
  oldctx = MemoryContextSwitchTo(TopMemoryContext);
  new_cached_privs_roles = list_copy(roles_list);
  MemoryContextSwitchTo(oldctx);
  list_free(roles_list);

     
                                          
     
  cached_privs_role = InvalidOid;                    
  list_free(cached_privs_roles);
  cached_privs_roles = new_cached_privs_roles;
  cached_privs_role = roleid;

                                        
  return cached_privs_roles;
}

   
                                                                
   
                                                                      
   
                                                                       
                                                                           
                                          
   
static List *
roles_is_member_of(Oid roleid)
{
  List *roles_list;
  ListCell *l;
  List *new_cached_membership_roles;
  MemoryContext oldctx;

                                                       
  if (OidIsValid(cached_member_role) && cached_member_role == roleid)
  {
    return cached_membership_roles;
  }

     
                                                                          
                                                                         
                     
     
                                                                        
                                                                  
                                                                          
                                                                        
                                                               
     
  roles_list = list_make1_oid(roleid);

  foreach (l, roles_list)
  {
    Oid memberid = lfirst_oid(l);
    CatCList *memlist;
    int i;

                                                          
    memlist = SearchSysCacheList1(AUTHMEMMEMROLE, ObjectIdGetDatum(memberid));
    for (i = 0; i < memlist->n_members; i++)
    {
      HeapTuple tup = &memlist->members[i]->tuple;
      Oid otherid = ((Form_pg_auth_members)GETSTRUCT(tup))->roleid;

         
                                                                    
                                                                      
                                                           
         
      roles_list = list_append_unique_oid(roles_list, otherid);
    }
    ReleaseSysCacheList(memlist);
  }

     
                                                                       
     
  oldctx = MemoryContextSwitchTo(TopMemoryContext);
  new_cached_membership_roles = list_copy(roles_list);
  MemoryContextSwitchTo(oldctx);
  list_free(roles_list);

     
                                          
     
  cached_member_role = InvalidOid;                    
  list_free(cached_membership_roles);
  cached_membership_roles = new_cached_membership_roles;
  cached_member_role = roleid;

                                        
  return cached_membership_roles;
}

   
                                                                     
   
                                                                           
                                                                           
                                                          
   
bool
has_privs_of_role(Oid member, Oid role)
{
                                 
  if (member == role)
  {
    return true;
  }

                                                                  
  if (superuser_arg(member))
  {
    return true;
  }

     
                                                                     
                                                                        
     
  return list_member_oid(roles_has_privs_of(member), role);
}

   
                                                        
   
                                                                      
   
bool
is_member_of_role(Oid member, Oid role)
{
                                 
  if (member == role)
  {
    return true;
  }

                                                                  
  if (superuser_arg(member))
  {
    return true;
  }

     
                                                                          
                                                            
     
  return list_member_oid(roles_is_member_of(member), role);
}

   
                           
                                                                        
   
void
check_is_member_of_role(Oid member, Oid role)
{
  if (!is_member_of_role(member, role))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be member of role \"%s\"", GetUserNameFromId(role, false))));
  }
}

   
                                                              
   
                                                                     
           
   
bool
is_member_of_role_nosuper(Oid member, Oid role)
{
                                 
  if (member == role)
  {
    return true;
  }

     
                                                                          
                                                            
     
  return list_member_oid(roles_is_member_of(member), role);
}

   
                                                                              
                                                                             
                   
   
bool
is_admin_of_role(Oid member, Oid role)
{
  bool result = false;
  List *roles_list;
  ListCell *l;

  if (superuser_arg(member))
  {
    return true;
  }

  if (member == role)
  {

       
                                                                          
                                                                      
                                                                         
                                                                    
                                                                        
                                                                      
                                                                       
                                                                         
                                                                         
                                                          
       
                                                                          
                                                                      
                                                                       
                                                                           
                                                                          
                                                                    
                                                                    
                                                                     
       
                                                                        
                                                                         
                                                                   
       
                                                                           
                                                                         
       
    return member == GetSessionUserId() && !InLocalUserIdChange() && !InSecurityRestrictedOperation();
  }

     
                                                                          
                                                                             
                                           
     
  roles_list = list_make1_oid(member);

  foreach (l, roles_list)
  {
    Oid memberid = lfirst_oid(l);
    CatCList *memlist;
    int i;

                                                          
    memlist = SearchSysCacheList1(AUTHMEMMEMROLE, ObjectIdGetDatum(memberid));
    for (i = 0; i < memlist->n_members; i++)
    {
      HeapTuple tup = &memlist->members[i]->tuple;
      Oid otherid = ((Form_pg_auth_members)GETSTRUCT(tup))->roleid;

      if (otherid == role && ((Form_pg_auth_members)GETSTRUCT(tup))->admin_option)
      {
                                                           
        result = true;
        break;
      }

      roles_list = list_append_unique_oid(roles_list, otherid);
    }
    ReleaseSysCacheList(memlist);
    if (result)
    {
      break;
    }
  }

  list_free(roles_list);

  return result;
}

                           
static int
count_one_bits(AclMode mask)
{
  int nbits = 0;

                                                          
  while (mask)
  {
    if (mask & 1)
    {
      nbits++;
    }
    mask >>= 1;
  }
  return nbits;
}

   
                                                                    
   
                                                                            
                                                                         
                                                                        
                                                                           
                                                                            
                                         
   
                                                                         
                                                                        
                                                                          
                                                                      
   
                                                      
                                                    
                                          
                                                   
                                                               
                                                                        
   
                                                                             
   
void
select_best_grantor(Oid roleId, AclMode privileges, const Acl *acl, Oid ownerId, Oid *grantorId, AclMode *grantOptions)
{
  AclMode needed_goptions = ACL_GRANT_OPTION_FOR(privileges);
  List *roles_list;
  int nrights;
  ListCell *l;

     
                                                                           
                                                                         
                                                                           
                       
     
  if (roleId == ownerId || superuser_arg(roleId))
  {
    *grantorId = ownerId;
    *grantOptions = needed_goptions;
    return;
  }

     
                                                                       
                                                                            
                                                                         
                                         
     
  roles_list = roles_has_privs_of(roleId);

                                              
  *grantorId = roleId;
  *grantOptions = ACL_NO_RIGHTS;
  nrights = 0;

  foreach (l, roles_list)
  {
    Oid otherrole = lfirst_oid(l);
    AclMode otherprivs;

    otherprivs = aclmask_direct(acl, otherrole, ownerId, needed_goptions, ACLMASK_ALL);
    if (otherprivs == needed_goptions)
    {
                                    
      *grantorId = otherrole;
      *grantOptions = otherprivs;
      return;
    }

       
                                                                   
                  
       
    if (otherprivs != ACL_NO_RIGHTS)
    {
      int nnewrights = count_one_bits(otherprivs);

      if (nnewrights > nrights)
      {
        *grantorId = otherrole;
        *grantOptions = otherprivs;
        nrights = nnewrights;
      }
    }
  }
}

   
                                                             
   
                                                                      
                                 
   
Oid
get_role_oid(const char *rolname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(AUTHNAME, Anum_pg_authid_oid, CStringGetDatum(rolname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", rolname)));
  }
  return oid;
}

   
                                                                      
                           
   
Oid
get_role_oid_or_public(const char *rolname)
{
  if (strcmp(rolname, "public") == 0)
  {
    return ACL_ID_PUBLIC;
  }

  return get_role_oid(rolname, false);
}

   
                                                                              
                                                       
   
                                                                            
                                        
   
Oid
get_rolespec_oid(const RoleSpec *role, bool missing_ok)
{
  Oid oid;

  switch (role->roletype)
  {
  case ROLESPEC_CSTRING:
    Assert(role->rolename);
    oid = get_role_oid(role->rolename, missing_ok);
    break;

  case ROLESPEC_CURRENT_USER:
    oid = GetUserId();
    break;

  case ROLESPEC_SESSION_USER:
    oid = GetSessionUserId();
    break;

  case ROLESPEC_PUBLIC:
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", "public")));
    oid = InvalidOid;                          
    break;

  default:
    elog(ERROR, "unexpected role type %d", role->roletype);
  }

  return oid;
}

   
                                                                            
                                                                
   
HeapTuple
get_rolespec_tuple(const RoleSpec *role)
{
  HeapTuple tuple;

  switch (role->roletype)
  {
  case ROLESPEC_CSTRING:
    Assert(role->rolename);
    tuple = SearchSysCache1(AUTHNAME, CStringGetDatum(role->rolename));
    if (!HeapTupleIsValid(tuple))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", role->rolename)));
    }
    break;

  case ROLESPEC_CURRENT_USER:
    tuple = SearchSysCache1(AUTHOID, GetUserId());
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for role %u", GetUserId());
    }
    break;

  case ROLESPEC_SESSION_USER:
    tuple = SearchSysCache1(AUTHOID, GetSessionUserId());
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for role %u", GetSessionUserId());
    }
    break;

  case ROLESPEC_PUBLIC:
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role \"%s\" does not exist", "public")));
    tuple = NULL;                          
    break;

  default:
    elog(ERROR, "unexpected role type %d", role->roletype);
  }

  return tuple;
}

   
                                                                                
   
char *
get_rolespec_name(const RoleSpec *role)
{
  HeapTuple tp;
  Form_pg_authid authForm;
  char *rolename;

  tp = get_rolespec_tuple(role);
  authForm = (Form_pg_authid)GETSTRUCT(tp);
  rolename = pstrdup(NameStr(authForm->rolname));
  ReleaseSysCache(tp);

  return rolename;
}

   
                                                                               
                                                   
   
                                                                              
                        
   
void
check_rolespec_name(const RoleSpec *role, const char *detail_msg)
{
  if (!role)
  {
    return;
  }

  if (role->roletype != ROLESPEC_CSTRING)
  {
    return;
  }

  if (IsReservedName(role->rolename))
  {
    if (detail_msg)
    {
      ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("role name \"%s\" is reserved", role->rolename), errdetail_internal("%s", detail_msg)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_RESERVED_NAME), errmsg("role name \"%s\" is reserved", role->rolename)));
    }
  }
}
