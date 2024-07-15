   
            
                                                          
   
                                                                
   
                  
                                    
   
   

#include "postgres.h"

#include <sys/stat.h>

#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "commands/dbcommands.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/rel.h"
#include "utils/relfilenodemap.h"
#include "utils/relmapper.h"
#include "utils/syscache.h"

                                            
#define half_rounded(x) (((x) + ((x) < 0 ? -1 : 1)) / 2)

                                                                           
static int64
db_dir_size(const char *path)
{
  int64 dirsize = 0;
  struct dirent *direntry;
  DIR *dirdesc;
  char filename[MAXPGPATH * 2];

  dirdesc = AllocateDir(path);

  if (!dirdesc)
  {
    return 0;
  }

  while ((direntry = ReadDir(dirdesc, path)) != NULL)
  {
    struct stat fst;

    CHECK_FOR_INTERRUPTS();

    if (strcmp(direntry->d_name, ".") == 0 || strcmp(direntry->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(filename, sizeof(filename), "%s/%s", path, direntry->d_name);

    if (stat(filename, &fst) < 0)
    {
      if (errno == ENOENT)
      {
        continue;
      }
      else
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", filename)));
      }
    }
    dirsize += fst.st_size;
  }

  FreeDir(dirdesc);
  return dirsize;
}

   
                                                 
   
static int64
calculate_database_size(Oid dbOid)
{
  int64 totalsize;
  DIR *dirdesc;
  struct dirent *direntry;
  char dirpath[MAXPGPATH];
  char pathname[MAXPGPATH + 21 + sizeof(TABLESPACE_VERSION_DIRECTORY)];
  AclResult aclresult;

     
                                                                            
                       
     
  aclresult = pg_database_aclcheck(dbOid, GetUserId(), ACL_CONNECT);
  if (aclresult != ACLCHECK_OK && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_STATS))
  {
    aclcheck_error(aclresult, OBJECT_DATABASE, get_database_name(dbOid));
  }

                                                  

                                  
  snprintf(pathname, sizeof(pathname), "base/%u", dbOid);
  totalsize = db_dir_size(pathname);

                                        
  snprintf(dirpath, MAXPGPATH, "pg_tblspc");
  dirdesc = AllocateDir(dirpath);

  while ((direntry = ReadDir(dirdesc, dirpath)) != NULL)
  {
    CHECK_FOR_INTERRUPTS();

    if (strcmp(direntry->d_name, ".") == 0 || strcmp(direntry->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(pathname, sizeof(pathname), "pg_tblspc/%s/%s/%u", direntry->d_name, TABLESPACE_VERSION_DIRECTORY, dbOid);
    totalsize += db_dir_size(pathname);
  }

  FreeDir(dirdesc);

  return totalsize;
}

Datum
pg_database_size_oid(PG_FUNCTION_ARGS)
{
  Oid dbOid = PG_GETARG_OID(0);
  int64 size;

  size = calculate_database_size(dbOid);

  if (size == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT64(size);
}

Datum
pg_database_size_name(PG_FUNCTION_ARGS)
{
  Name dbName = PG_GETARG_NAME(0);
  Oid dbOid = get_database_oid(NameStr(*dbName), false);
  int64 size;

  size = calculate_database_size(dbOid);

  if (size == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT64(size);
}

   
                                                                              
                    
   
static int64
calculate_tablespace_size(Oid tblspcOid)
{
  char tblspcPath[MAXPGPATH];
  char pathname[MAXPGPATH * 2];
  int64 totalsize = 0;
  DIR *dirdesc;
  struct dirent *direntry;
  AclResult aclresult;

     
                                                                             
                                                                           
                                      
     
  if (tblspcOid != MyDatabaseTableSpace && !is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_ALL_STATS))
  {
    aclresult = pg_tablespace_aclcheck(tblspcOid, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_TABLESPACE, get_tablespace_name(tblspcOid));
    }
  }

  if (tblspcOid == DEFAULTTABLESPACE_OID)
  {
    snprintf(tblspcPath, MAXPGPATH, "base");
  }
  else if (tblspcOid == GLOBALTABLESPACE_OID)
  {
    snprintf(tblspcPath, MAXPGPATH, "global");
  }
  else
  {
    snprintf(tblspcPath, MAXPGPATH, "pg_tblspc/%u/%s", tblspcOid, TABLESPACE_VERSION_DIRECTORY);
  }

  dirdesc = AllocateDir(tblspcPath);

  if (!dirdesc)
  {
    return -1;
  }

  while ((direntry = ReadDir(dirdesc, tblspcPath)) != NULL)
  {
    struct stat fst;

    CHECK_FOR_INTERRUPTS();

    if (strcmp(direntry->d_name, ".") == 0 || strcmp(direntry->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(pathname, sizeof(pathname), "%s/%s", tblspcPath, direntry->d_name);

    if (stat(pathname, &fst) < 0)
    {
      if (errno == ENOENT)
      {
        continue;
      }
      else
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", pathname)));
      }
    }

    if (S_ISDIR(fst.st_mode))
    {
      totalsize += db_dir_size(pathname);
    }

    totalsize += fst.st_size;
  }

  FreeDir(dirdesc);

  return totalsize;
}

Datum
pg_tablespace_size_oid(PG_FUNCTION_ARGS)
{
  Oid tblspcOid = PG_GETARG_OID(0);
  int64 size;

  size = calculate_tablespace_size(tblspcOid);

  if (size < 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT64(size);
}

Datum
pg_tablespace_size_name(PG_FUNCTION_ARGS)
{
  Name tblspcName = PG_GETARG_NAME(0);
  Oid tblspcOid = get_tablespace_oid(NameStr(*tblspcName), false);
  int64 size;

  size = calculate_tablespace_size(tblspcOid);

  if (size < 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT64(size);
}

   
                                              
   
                                                                             
                                                   
   
static int64
calculate_relation_size(RelFileNode *rfn, BackendId backend, ForkNumber forknum)
{
  int64 totalsize = 0;
  char *relationpath;
  char pathname[MAXPGPATH];
  unsigned int segcount = 0;

  relationpath = relpathbackend(*rfn, backend, forknum);

  for (segcount = 0;; segcount++)
  {
    struct stat fst;

    CHECK_FOR_INTERRUPTS();

    if (segcount == 0)
    {
      snprintf(pathname, MAXPGPATH, "%s", relationpath);
    }
    else
    {
      snprintf(pathname, MAXPGPATH, "%s.%u", relationpath, segcount);
    }

    if (stat(pathname, &fst) < 0)
    {
      if (errno == ENOENT)
      {
        break;
      }
      else
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", pathname)));
      }
    }
    totalsize += fst.st_size;
  }

  return totalsize;
}

Datum
pg_relation_size(PG_FUNCTION_ARGS)
{
  Oid relOid = PG_GETARG_OID(0);
  text *forkName = PG_GETARG_TEXT_PP(1);
  Relation rel;
  int64 size;

  rel = try_relation_open(relOid, AccessShareLock);

     
                                                                             
                                                                          
                                                                        
                                                                       
                                                                           
     
  if (rel == NULL)
  {
    PG_RETURN_NULL();
  }

  size = calculate_relation_size(&(rel->rd_node), rel->rd_backend, forkname_to_number(text_to_cstring(forkName)));

  relation_close(rel, AccessShareLock);

  PG_RETURN_INT64(size);
}

   
                                                                            
                                               
   
static int64
calculate_toast_table_size(Oid toastrelid)
{
  int64 size = 0;
  Relation toastRel;
  ForkNumber forkNum;
  ListCell *lc;
  List *indexlist;

  toastRel = relation_open(toastrelid, AccessShareLock);

                                                  
  for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
  {
    size += calculate_relation_size(&(toastRel->rd_node), toastRel->rd_backend, forkNum);
  }

                                                   
  indexlist = RelationGetIndexList(toastRel);

                                                          
  foreach (lc, indexlist)
  {
    Relation toastIdxRel;

    toastIdxRel = relation_open(lfirst_oid(lc), AccessShareLock);
    for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
    {
      size += calculate_relation_size(&(toastIdxRel->rd_node), toastIdxRel->rd_backend, forkNum);
    }

    relation_close(toastIdxRel, AccessShareLock);
  }
  list_free(indexlist);
  relation_close(toastRel, AccessShareLock);

  return size;
}

   
                                                  
                                                  
                                                                
   
                                                                             
                                                                             
   
static int64
calculate_table_size(Relation rel)
{
  int64 size = 0;
  ForkNumber forkNum;

     
                                     
     
  for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
  {
    size += calculate_relation_size(&(rel->rd_node), rel->rd_backend, forkNum);
  }

     
                            
     
  if (OidIsValid(rel->rd_rel->reltoastrelid))
  {
    size += calculate_toast_table_size(rel->rd_rel->reltoastrelid);
  }

  return size;
}

   
                                                                            
   
                                                                
   
static int64
calculate_indexes_size(Relation rel)
{
  int64 size = 0;

     
                                                 
     
  if (rel->rd_rel->relhasindex)
  {
    List *index_oids = RelationGetIndexList(rel);
    ListCell *cell;

    foreach (cell, index_oids)
    {
      Oid idxOid = lfirst_oid(cell);
      Relation idxRel;
      ForkNumber forkNum;

      idxRel = relation_open(idxOid, AccessShareLock);

      for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
      {
        size += calculate_relation_size(&(idxRel->rd_node), idxRel->rd_backend, forkNum);
      }

      relation_close(idxRel, AccessShareLock);
    }

    list_free(index_oids);
  }

  return size;
}

Datum
pg_table_size(PG_FUNCTION_ARGS)
{
  Oid relOid = PG_GETARG_OID(0);
  Relation rel;
  int64 size;

  rel = try_relation_open(relOid, AccessShareLock);

  if (rel == NULL)
  {
    PG_RETURN_NULL();
  }

  size = calculate_table_size(rel);

  relation_close(rel, AccessShareLock);

  PG_RETURN_INT64(size);
}

Datum
pg_indexes_size(PG_FUNCTION_ARGS)
{
  Oid relOid = PG_GETARG_OID(0);
  Relation rel;
  int64 size;

  rel = try_relation_open(relOid, AccessShareLock);

  if (rel == NULL)
  {
    PG_RETURN_NULL();
  }

  size = calculate_indexes_size(rel);

  relation_close(rel, AccessShareLock);

  PG_RETURN_INT64(size);
}

   
                                                           
                                                         
   
static int64
calculate_total_relation_size(Relation rel)
{
  int64 size;

     
                                                                         
                                                    
     
  size = calculate_table_size(rel);

     
                                              
     
  size += calculate_indexes_size(rel);

  return size;
}

Datum
pg_total_relation_size(PG_FUNCTION_ARGS)
{
  Oid relOid = PG_GETARG_OID(0);
  Relation rel;
  int64 size;

  rel = try_relation_open(relOid, AccessShareLock);

  if (rel == NULL)
  {
    PG_RETURN_NULL();
  }

  size = calculate_total_relation_size(rel);

  relation_close(rel, AccessShareLock);

  PG_RETURN_INT64(size);
}

   
                              
   
Datum
pg_size_pretty(PG_FUNCTION_ARGS)
{
  int64 size = PG_GETARG_INT64(0);
  char buf[64];
  int64 limit = 10 * 1024;
  int64 limit2 = limit * 2 - 1;

  if (Abs(size) < limit)
  {
    snprintf(buf, sizeof(buf), INT64_FORMAT " bytes", size);
  }
  else
  {
       
                                                                          
                                               
       
    size /= (1 << 9);                                      
    if (Abs(size) < limit2)
    {
      snprintf(buf, sizeof(buf), INT64_FORMAT " kB", half_rounded(size));
    }
    else
    {
      size /= (1 << 10);
      if (Abs(size) < limit2)
      {
        snprintf(buf, sizeof(buf), INT64_FORMAT " MB", half_rounded(size));
      }
      else
      {
        size /= (1 << 10);
        if (Abs(size) < limit2)
        {
          snprintf(buf, sizeof(buf), INT64_FORMAT " GB", half_rounded(size));
        }
        else
        {
          size /= (1 << 10);
          snprintf(buf, sizeof(buf), INT64_FORMAT " TB", half_rounded(size));
        }
      }
    }
  }

  PG_RETURN_TEXT_P(cstring_to_text(buf));
}

static char *
numeric_to_cstring(Numeric n)
{
  Datum d = NumericGetDatum(n);

  return DatumGetCString(DirectFunctionCall1(numeric_out, d));
}

static Numeric
int64_to_numeric(int64 v)
{
  Datum d = Int64GetDatum(v);

  return DatumGetNumeric(DirectFunctionCall1(int8_numeric, d));
}

static bool
numeric_is_less(Numeric a, Numeric b)
{
  Datum da = NumericGetDatum(a);
  Datum db = NumericGetDatum(b);

  return DatumGetBool(DirectFunctionCall2(numeric_lt, da, db));
}

static Numeric
numeric_absolute(Numeric n)
{
  Datum d = NumericGetDatum(n);
  Datum result;

  result = DirectFunctionCall1(numeric_abs, d);
  return DatumGetNumeric(result);
}

static Numeric
numeric_half_rounded(Numeric n)
{
  Datum d = NumericGetDatum(n);
  Datum zero;
  Datum one;
  Datum two;
  Datum result;

  zero = DirectFunctionCall1(int8_numeric, Int64GetDatum(0));
  one = DirectFunctionCall1(int8_numeric, Int64GetDatum(1));
  two = DirectFunctionCall1(int8_numeric, Int64GetDatum(2));

  if (DatumGetBool(DirectFunctionCall2(numeric_ge, d, zero)))
  {
    d = DirectFunctionCall2(numeric_add, d, one);
  }
  else
  {
    d = DirectFunctionCall2(numeric_sub, d, one);
  }

  result = DirectFunctionCall2(numeric_div_trunc, d, two);
  return DatumGetNumeric(result);
}

static Numeric
numeric_truncated_divide(Numeric n, int64 divisor)
{
  Datum d = NumericGetDatum(n);
  Datum divisor_numeric;
  Datum result;

  divisor_numeric = DirectFunctionCall1(int8_numeric, Int64GetDatum(divisor));
  result = DirectFunctionCall2(numeric_div_trunc, d, divisor_numeric);
  return DatumGetNumeric(result);
}

Datum
pg_size_pretty_numeric(PG_FUNCTION_ARGS)
{
  Numeric size = PG_GETARG_NUMERIC(0);
  Numeric limit, limit2;
  char *result;

  limit = int64_to_numeric(10 * 1024);
  limit2 = int64_to_numeric(10 * 1024 * 2 - 1);

  if (numeric_is_less(numeric_absolute(size), limit))
  {
    result = psprintf("%s bytes", numeric_to_cstring(size));
  }
  else
  {
                                         
                          
    size = numeric_truncated_divide(size, 1 << 9);

    if (numeric_is_less(numeric_absolute(size), limit2))
    {
      size = numeric_half_rounded(size);
      result = psprintf("%s kB", numeric_to_cstring(size));
    }
    else
    {
                             
      size = numeric_truncated_divide(size, 1 << 10);

      if (numeric_is_less(numeric_absolute(size), limit2))
      {
        size = numeric_half_rounded(size);
        result = psprintf("%s MB", numeric_to_cstring(size));
      }
      else
      {
                               
        size = numeric_truncated_divide(size, 1 << 10);

        if (numeric_is_less(numeric_absolute(size), limit2))
        {
          size = numeric_half_rounded(size);
          result = psprintf("%s GB", numeric_to_cstring(size));
        }
        else
        {
                                 
          size = numeric_truncated_divide(size, 1 << 10);
          size = numeric_half_rounded(size);
          result = psprintf("%s TB", numeric_to_cstring(size));
        }
      }
    }
  }

  PG_RETURN_TEXT_P(cstring_to_text(result));
}

   
                                                    
   
Datum
pg_size_bytes(PG_FUNCTION_ARGS)
{
  text *arg = PG_GETARG_TEXT_PP(0);
  char *str, *strptr, *endptr;
  char saved_char;
  Numeric num;
  int64 result;
  bool have_digits = false;

  str = text_to_cstring(arg);

                               
  strptr = str;
  while (isspace((unsigned char)*strptr))
  {
    strptr++;
  }

                                                                     
  endptr = strptr;

                      
  if (*endptr == '-' || *endptr == '+')
  {
    endptr++;
  }

                                   
  if (isdigit((unsigned char)*endptr))
  {
    have_digits = true;
    do
    {
      endptr++;
    } while (isdigit((unsigned char)*endptr));
  }

                                                              
  if (*endptr == '.')
  {
    endptr++;
    if (isdigit((unsigned char)*endptr))
    {
      have_digits = true;
      do
      {
        endptr++;
      } while (isdigit((unsigned char)*endptr));
    }
  }

                                                              
  if (!have_digits)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid size: \"%s\"", str)));
  }

                                   
  if (*endptr == 'e' || *endptr == 'E')
  {
    long exponent;
    char *cp;

       
                                                                      
                                                                 
       
    exponent = strtol(endptr + 1, &cp, 10);
    (void)exponent;                                       
    if (cp > endptr + 1)
    {
      endptr = cp;
    }
  }

     
                                                                         
                                   
     
  saved_char = *endptr;
  *endptr = '\0';

  num = DatumGetNumeric(DirectFunctionCall3(numeric_in, CStringGetDatum(strptr), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1)));

  *endptr = saved_char;

                                               
  strptr = endptr;
  while (isspace((unsigned char)*strptr))
  {
    strptr++;
  }

                            
  if (*strptr != '\0')
  {
    int64 multiplier = 0;

                                      
    endptr = str + VARSIZE_ANY_EXHDR(arg) - 1;

    while (isspace((unsigned char)*endptr))
    {
      endptr--;
    }

    endptr++;
    *endptr = '\0';

                                           
    if (pg_strcasecmp(strptr, "bytes") == 0)
    {
      multiplier = (int64)1;
    }
    else if (pg_strcasecmp(strptr, "kb") == 0)
    {
      multiplier = (int64)1024;
    }
    else if (pg_strcasecmp(strptr, "mb") == 0)
    {
      multiplier = ((int64)1024) * 1024;
    }

    else if (pg_strcasecmp(strptr, "gb") == 0)
    {
      multiplier = ((int64)1024) * 1024 * 1024;
    }

    else if (pg_strcasecmp(strptr, "tb") == 0)
    {
      multiplier = ((int64)1024) * 1024 * 1024 * 1024;
    }

    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid size: \"%s\"", text_to_cstring(arg)), errdetail("Invalid size unit: \"%s\".", strptr), errhint("Valid units are \"bytes\", \"kB\", \"MB\", \"GB\", and \"TB\".")));
    }

    if (multiplier > 1)
    {
      Numeric mul_num;

      mul_num = DatumGetNumeric(DirectFunctionCall1(int8_numeric, Int64GetDatum(multiplier)));

      num = DatumGetNumeric(DirectFunctionCall2(numeric_mul, NumericGetDatum(mul_num), NumericGetDatum(num)));
    }
  }

  result = DatumGetInt64(DirectFunctionCall1(numeric_int8, NumericGetDatum(num)));

  PG_RETURN_INT64(result);
}

   
                                  
   
                                               
                                                    
                                                                           
                                                                         
                                                                        
                                                                          
                                                                      
                                                                        
                                                                     
                                        
   
Datum
pg_relation_filenode(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  Oid result;
  HeapTuple tuple;
  Form_pg_class relform;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    PG_RETURN_NULL();
  }
  relform = (Form_pg_class)GETSTRUCT(tuple);

  switch (relform->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
  case RELKIND_INDEX:
  case RELKIND_SEQUENCE:
  case RELKIND_TOASTVALUE:
                                  
    if (relform->relfilenode)
    {
      result = relform->relfilenode;
    }
    else                                  
    {
      result = RelationMapOidToFilenode(relid, relform->relisshared);
    }
    break;

  default:
                                 
    result = InvalidOid;
    break;
  }

  ReleaseSysCache(tuple);

  if (!OidIsValid(result))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_OID(result);
}

   
                                                     
   
                                                                               
                                                                          
                                                                                
           
   
                                                              
   
                                                                      
               
   
Datum
pg_filenode_relation(PG_FUNCTION_ARGS)
{
  Oid reltablespace = PG_GETARG_OID(0);
  Oid relfilenode = PG_GETARG_OID(1);
  Oid heaprel;

                                                           
  if (!OidIsValid(relfilenode))
  {
    PG_RETURN_NULL();
  }

  heaprel = RelidByRelfilenode(reltablespace, relfilenode);

  if (!OidIsValid(heaprel))
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_OID(heaprel);
  }
}

   
                                                        
   
                                          
   
Datum
pg_relation_filepath(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  HeapTuple tuple;
  Form_pg_class relform;
  RelFileNode rnode;
  BackendId backend;
  char *path;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    PG_RETURN_NULL();
  }
  relform = (Form_pg_class)GETSTRUCT(tuple);

  switch (relform->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
  case RELKIND_INDEX:
  case RELKIND_SEQUENCE:
  case RELKIND_TOASTVALUE:
                                  

                                                          
    if (relform->reltablespace)
    {
      rnode.spcNode = relform->reltablespace;
    }
    else
    {
      rnode.spcNode = MyDatabaseTableSpace;
    }
    if (rnode.spcNode == GLOBALTABLESPACE_OID)
    {
      rnode.dbNode = InvalidOid;
    }
    else
    {
      rnode.dbNode = MyDatabaseId;
    }
    if (relform->relfilenode)
    {
      rnode.relNode = relform->relfilenode;
    }
    else                                  
    {
      rnode.relNode = RelationMapOidToFilenode(relid, relform->relisshared);
    }
    break;

  default:
                                 
    rnode.relNode = InvalidOid;
                                                                       
    rnode.dbNode = InvalidOid;
    rnode.spcNode = InvalidOid;
    break;
  }

  if (!OidIsValid(rnode.relNode))
  {
    ReleaseSysCache(tuple);
    PG_RETURN_NULL();
  }

                                 
  switch (relform->relpersistence)
  {
  case RELPERSISTENCE_UNLOGGED:
  case RELPERSISTENCE_PERMANENT:
    backend = InvalidBackendId;
    break;
  case RELPERSISTENCE_TEMP:
    if (isTempOrTempToastNamespace(relform->relnamespace))
    {
      backend = BackendIdForTempRelations();
    }
    else
    {
                               
      backend = GetTempNamespaceBackendId(relform->relnamespace);
      Assert(backend != InvalidBackendId);
    }
    break;
  default:
    elog(ERROR, "invalid relpersistence: %c", relform->relpersistence);
    backend = InvalidBackendId;                       
    break;
  }

  ReleaseSysCache(tuple);

  path = relpathbackend(rnode, backend, MAIN_FORKNUM);

  PG_RETURN_TEXT_P(cstring_to_text(path));
}
