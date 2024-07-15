                                                                            
   
              
                              
   
                                                                           
                                                                        
                                                                       
                                                                           
                                                                            
   
                                                                           
                                                                            
                                                                             
                                                                          
                                                                            
                              
   
                                                                          
                                                                         
                                                                        
                                                                         
                                                                     
   
                                                                         
                                                                          
                                                                     
                                                                      
                                                                   
                                                        
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/parallel.h"
#include "access/relation.h"
#include "access/session.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_range.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "executor/executor.h"
#include "lib/dshash.h"
#include "optimizer/optimizer.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

                                                                 
static HTAB *TypeCacheHash = NULL;

                                                 
static TypeCacheEntry *firstDomainTypeEntry = NULL;

                                                         
#define TCFLAGS_CHECKED_BTREE_OPCLASS 0x000001
#define TCFLAGS_CHECKED_HASH_OPCLASS 0x000002
#define TCFLAGS_CHECKED_EQ_OPR 0x000004
#define TCFLAGS_CHECKED_LT_OPR 0x000008
#define TCFLAGS_CHECKED_GT_OPR 0x000010
#define TCFLAGS_CHECKED_CMP_PROC 0x000020
#define TCFLAGS_CHECKED_HASH_PROC 0x000040
#define TCFLAGS_CHECKED_HASH_EXTENDED_PROC 0x000080
#define TCFLAGS_CHECKED_ELEM_PROPERTIES 0x000100
#define TCFLAGS_HAVE_ELEM_EQUALITY 0x000200
#define TCFLAGS_HAVE_ELEM_COMPARE 0x000400
#define TCFLAGS_HAVE_ELEM_HASHING 0x000800
#define TCFLAGS_HAVE_ELEM_EXTENDED_HASHING 0x001000
#define TCFLAGS_CHECKED_FIELD_PROPERTIES 0x002000
#define TCFLAGS_HAVE_FIELD_EQUALITY 0x004000
#define TCFLAGS_HAVE_FIELD_COMPARE 0x008000
#define TCFLAGS_CHECKED_DOMAIN_CONSTRAINTS 0x010000
#define TCFLAGS_DOMAIN_BASE_IS_COMPOSITE 0x020000

   
                                                                              
                                                                            
                                        
   
                                                                           
                                                                            
                                                                          
                                                                        
                                                                     
                                                  
   
struct DomainConstraintCache
{
  List *constraints;                                                 
  MemoryContext dccContext;                                                 
  long dccRefCount;                                                  
};

                                                               
typedef struct
{
  Oid enum_oid;                                 
  float4 sort_order;                        
} EnumItem;

typedef struct TypeCacheEnumData
{
  Oid bitmap_base;                                                       
  Bitmapset *sorted_values;                                       
  int num_values;                                               
  EnumItem enum_values[FLEXIBLE_ARRAY_MEMBER];
} TypeCacheEnumData;

   
                                                                        
                                                                         
                                                                          
                                                                           
   
                                                                       
                                                                         
                                                           
   

typedef struct RecordCacheEntry
{
  TupleDesc tupdesc;
} RecordCacheEntry;

   
                                                                          
                                                                             
   
struct SharedRecordTypmodRegistry
{
                                                      
  dshash_table_handle record_table_handle;
                                                       
  dshash_table_handle typmod_table_handle;
                                              
  pg_atomic_uint32 next_typmod;
};

   
                                                                              
                                                                      
                                                                               
                                           
   
typedef struct SharedRecordTableKey
{
  union
  {
    TupleDesc local_tupdesc;
    dsa_pointer shared_tupdesc;
  } u;
  bool shared;
} SharedRecordTableKey;

   
                                                                          
                                                             
   
typedef struct SharedRecordTableEntry
{
  SharedRecordTableKey key;
} SharedRecordTableEntry;

   
                                                                             
                                                   
   
typedef struct SharedTypmodTableEntry
{
  uint32 typmod;
  dsa_pointer shared_tupdesc;
} SharedTypmodTableEntry;

   
                                                   
   
static int
shared_record_table_compare(const void *a, const void *b, size_t size, void *arg)
{
  dsa_area *area = (dsa_area *)arg;
  SharedRecordTableKey *k1 = (SharedRecordTableKey *)a;
  SharedRecordTableKey *k2 = (SharedRecordTableKey *)b;
  TupleDesc t1;
  TupleDesc t2;

  if (k1->shared)
  {
    t1 = (TupleDesc)dsa_get_address(area, k1->u.shared_tupdesc);
  }
  else
  {
    t1 = k1->u.local_tupdesc;
  }

  if (k2->shared)
  {
    t2 = (TupleDesc)dsa_get_address(area, k2->u.shared_tupdesc);
  }
  else
  {
    t2 = k2->u.local_tupdesc;
  }

  return equalTupleDescs(t1, t2) ? 0 : 1;
}

   
                                             
   
static uint32
shared_record_table_hash(const void *a, size_t size, void *arg)
{
  dsa_area *area = (dsa_area *)arg;
  SharedRecordTableKey *k = (SharedRecordTableKey *)a;
  TupleDesc t;

  if (k->shared)
  {
    t = (TupleDesc)dsa_get_address(area, k->u.shared_tupdesc);
  }
  else
  {
    t = k->u.local_tupdesc;
  }

  return hashTupleDesc(t);
}

                                                                  
static const dshash_parameters srtr_record_table_params = {sizeof(SharedRecordTableKey),             
    sizeof(SharedRecordTableEntry), shared_record_table_compare, shared_record_table_hash, LWTRANCHE_SESSION_RECORD_TABLE};

                                                                    
static const dshash_parameters srtr_typmod_table_params = {sizeof(uint32), sizeof(SharedTypmodTableEntry), dshash_memcmp, dshash_memhash, LWTRANCHE_SESSION_TYPMOD_TABLE};

                                                       
static HTAB *RecordCacheHash = NULL;

                                                                              
static TupleDesc *RecordCacheArray = NULL;
static uint64 *RecordIdentifierArray = NULL;
static int32 RecordCacheArrayLen = 0;                                       
static int32 NextRecordTypmod = 0;                                

   
                                                                     
                                                                           
                                                                            
   
static uint64 tupledesc_id_counter = INVALID_TUPLEDESC_IDENTIFIER;

static void
load_typcache_tupdesc(TypeCacheEntry *typentry);
static void
load_rangetype_info(TypeCacheEntry *typentry);
static void
load_domaintype_info(TypeCacheEntry *typentry);
static int
dcs_cmp(const void *a, const void *b);
static void
decr_dcc_refcount(DomainConstraintCache *dcc);
static void
dccref_deletion_callback(void *arg);
static List *
prep_domain_constraints(List *constraints, MemoryContext execctx);
static bool
array_element_has_equality(TypeCacheEntry *typentry);
static bool
array_element_has_compare(TypeCacheEntry *typentry);
static bool
array_element_has_hashing(TypeCacheEntry *typentry);
static bool
array_element_has_extended_hashing(TypeCacheEntry *typentry);
static void
cache_array_element_properties(TypeCacheEntry *typentry);
static bool
record_fields_have_equality(TypeCacheEntry *typentry);
static bool
record_fields_have_compare(TypeCacheEntry *typentry);
static void
cache_record_field_properties(TypeCacheEntry *typentry);
static bool
range_element_has_hashing(TypeCacheEntry *typentry);
static bool
range_element_has_extended_hashing(TypeCacheEntry *typentry);
static void
cache_range_element_properties(TypeCacheEntry *typentry);
static void
TypeCacheRelCallback(Datum arg, Oid relid);
static void
TypeCacheOpcCallback(Datum arg, int cacheid, uint32 hashvalue);
static void
TypeCacheConstrCallback(Datum arg, int cacheid, uint32 hashvalue);
static void
load_enum_cache_data(TypeCacheEntry *tcache);
static EnumItem *
find_enumitem(TypeCacheEnumData *enumdata, Oid arg);
static int
enum_oid_cmp(const void *left, const void *right);
static void
shared_record_typmod_registry_detach(dsm_segment *segment, Datum datum);
static TupleDesc
find_or_make_matching_shared_tupledesc(TupleDesc tupdesc);
static dsa_pointer
share_tupledesc(dsa_area *area, TupleDesc tupdesc, uint32 typmod);

   
                     
   
                                                                             
                                                          
   
                                                                            
                                                                      
                                                                             
                          
   
TypeCacheEntry *
lookup_type_cache(Oid type_id, int flags)
{
  TypeCacheEntry *typentry;
  bool found;

  if (TypeCacheHash == NULL)
  {
                                                       
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(Oid);
    ctl.entrysize = sizeof(TypeCacheEntry);
    TypeCacheHash = hash_create("Type information cache", 64, &ctl, HASH_ELEM | HASH_BLOBS);

                                                    
    CacheRegisterRelcacheCallback(TypeCacheRelCallback, (Datum)0);
    CacheRegisterSyscacheCallback(CLAOID, TypeCacheOpcCallback, (Datum)0);
    CacheRegisterSyscacheCallback(CONSTROID, TypeCacheConstrCallback, (Datum)0);
    CacheRegisterSyscacheCallback(TYPEOID, TypeCacheConstrCallback, (Datum)0);

                                                  
    if (!CacheMemoryContext)
    {
      CreateCacheMemoryContext();
    }
  }

                                        
  typentry = (TypeCacheEntry *)hash_search(TypeCacheHash, (void *)&type_id, HASH_FIND, NULL);
  if (typentry == NULL)
  {
       
                                                                          
                                                                         
                                                                  
                                                                          
                                                             
       
    HeapTuple tp;
    Form_pg_type typtup;

    tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type_id));
    if (!HeapTupleIsValid(tp))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type with OID %u does not exist", type_id)));
    }
    typtup = (Form_pg_type)GETSTRUCT(tp);
    if (!typtup->typisdefined)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type \"%s\" is only a shell", NameStr(typtup->typname))));
    }

                                     
    typentry = (TypeCacheEntry *)hash_search(TypeCacheHash, (void *)&type_id, HASH_ENTER, &found);
    Assert(!found);                                   

    MemSet(typentry, 0, sizeof(TypeCacheEntry));
    typentry->type_id = type_id;
    typentry->typlen = typtup->typlen;
    typentry->typbyval = typtup->typbyval;
    typentry->typalign = typtup->typalign;
    typentry->typstorage = typtup->typstorage;
    typentry->typtype = typtup->typtype;
    typentry->typrelid = typtup->typrelid;
    typentry->typelem = typtup->typelem;
    typentry->typcollation = typtup->typcollation;

                                                                            
    if (typentry->typtype == TYPTYPE_DOMAIN)
    {
      typentry->nextDomain = firstDomainTypeEntry;
      firstDomainTypeEntry = typentry;
    }

    ReleaseSysCache(tp);
  }

     
                                                                       
                
     
  if ((flags & (TYPECACHE_EQ_OPR | TYPECACHE_LT_OPR | TYPECACHE_GT_OPR | TYPECACHE_CMP_PROC | TYPECACHE_EQ_OPR_FINFO | TYPECACHE_CMP_PROC_FINFO | TYPECACHE_BTREE_OPFAMILY)) && !(typentry->flags & TCFLAGS_CHECKED_BTREE_OPCLASS))
  {
    Oid opclass;

    opclass = GetDefaultOpClass(type_id, BTREE_AM_OID);
    if (OidIsValid(opclass))
    {
      typentry->btree_opf = get_opclass_family(opclass);
      typentry->btree_opintype = get_opclass_input_type(opclass);
    }
    else
    {
      typentry->btree_opf = typentry->btree_opintype = InvalidOid;
    }

       
                                                                         
                                                                          
                                                                          
                                           
       
    typentry->flags &= ~(TCFLAGS_CHECKED_EQ_OPR | TCFLAGS_CHECKED_LT_OPR | TCFLAGS_CHECKED_GT_OPR | TCFLAGS_CHECKED_CMP_PROC);
    typentry->flags |= TCFLAGS_CHECKED_BTREE_OPCLASS;
  }

     
                                                                            
                                   
     
  if ((flags & (TYPECACHE_EQ_OPR | TYPECACHE_EQ_OPR_FINFO)) && !(typentry->flags & TCFLAGS_CHECKED_EQ_OPR) && typentry->btree_opf == InvalidOid)
  {
    flags |= TYPECACHE_HASH_OPFAMILY;
  }

  if ((flags & (TYPECACHE_HASH_PROC | TYPECACHE_HASH_PROC_FINFO | TYPECACHE_HASH_EXTENDED_PROC | TYPECACHE_HASH_EXTENDED_PROC_FINFO | TYPECACHE_HASH_OPFAMILY)) && !(typentry->flags & TCFLAGS_CHECKED_HASH_OPCLASS))
  {
    Oid opclass;

    opclass = GetDefaultOpClass(type_id, HASH_AM_OID);
    if (OidIsValid(opclass))
    {
      typentry->hash_opf = get_opclass_family(opclass);
      typentry->hash_opintype = get_opclass_input_type(opclass);
    }
    else
    {
      typentry->hash_opf = typentry->hash_opintype = InvalidOid;
    }

       
                                                                           
                                                                    
                               
       
    typentry->flags &= ~(TCFLAGS_CHECKED_HASH_PROC | TCFLAGS_CHECKED_HASH_EXTENDED_PROC);
    typentry->flags |= TCFLAGS_CHECKED_HASH_OPCLASS;
  }

     
                                                                        
     
  if ((flags & (TYPECACHE_EQ_OPR | TYPECACHE_EQ_OPR_FINFO)) && !(typentry->flags & TCFLAGS_CHECKED_EQ_OPR))
  {
    Oid eq_opr = InvalidOid;

    if (typentry->btree_opf != InvalidOid)
    {
      eq_opr = get_opfamily_member(typentry->btree_opf, typentry->btree_opintype, typentry->btree_opintype, BTEqualStrategyNumber);
    }
    if (eq_opr == InvalidOid && typentry->hash_opf != InvalidOid)
    {
      eq_opr = get_opfamily_member(typentry->hash_opf, typentry->hash_opintype, typentry->hash_opintype, HTEqualStrategyNumber);
    }

       
                                                                         
                                                                        
                                                                          
                                                                   
                                                                        
                                                      
       
    if (eq_opr == ARRAY_EQ_OP && !array_element_has_equality(typentry))
    {
      eq_opr = InvalidOid;
    }
    else if (eq_opr == RECORD_EQ_OP && !record_fields_have_equality(typentry))
    {
      eq_opr = InvalidOid;
    }

                                                                   
    if (typentry->eq_opr != eq_opr)
    {
      typentry->eq_opr_finfo.fn_oid = InvalidOid;
    }

    typentry->eq_opr = eq_opr;

       
                                                                          
                                                                  
                                     
       
    typentry->flags &= ~(TCFLAGS_CHECKED_HASH_PROC | TCFLAGS_CHECKED_HASH_EXTENDED_PROC);
    typentry->flags |= TCFLAGS_CHECKED_EQ_OPR;
  }
  if ((flags & TYPECACHE_LT_OPR) && !(typentry->flags & TCFLAGS_CHECKED_LT_OPR))
  {
    Oid lt_opr = InvalidOid;

    if (typentry->btree_opf != InvalidOid)
    {
      lt_opr = get_opfamily_member(typentry->btree_opf, typentry->btree_opintype, typentry->btree_opintype, BTLessStrategyNumber);
    }

       
                                                                           
                                            
       
    if (lt_opr == ARRAY_LT_OP && !array_element_has_compare(typentry))
    {
      lt_opr = InvalidOid;
    }
    else if (lt_opr == RECORD_LT_OP && !record_fields_have_compare(typentry))
    {
      lt_opr = InvalidOid;
    }

    typentry->lt_opr = lt_opr;
    typentry->flags |= TCFLAGS_CHECKED_LT_OPR;
  }
  if ((flags & TYPECACHE_GT_OPR) && !(typentry->flags & TCFLAGS_CHECKED_GT_OPR))
  {
    Oid gt_opr = InvalidOid;

    if (typentry->btree_opf != InvalidOid)
    {
      gt_opr = get_opfamily_member(typentry->btree_opf, typentry->btree_opintype, typentry->btree_opintype, BTGreaterStrategyNumber);
    }

       
                                                                           
                                            
       
    if (gt_opr == ARRAY_GT_OP && !array_element_has_compare(typentry))
    {
      gt_opr = InvalidOid;
    }
    else if (gt_opr == RECORD_GT_OP && !record_fields_have_compare(typentry))
    {
      gt_opr = InvalidOid;
    }

    typentry->gt_opr = gt_opr;
    typentry->flags |= TCFLAGS_CHECKED_GT_OPR;
  }
  if ((flags & (TYPECACHE_CMP_PROC | TYPECACHE_CMP_PROC_FINFO)) && !(typentry->flags & TCFLAGS_CHECKED_CMP_PROC))
  {
    Oid cmp_proc = InvalidOid;

    if (typentry->btree_opf != InvalidOid)
    {
      cmp_proc = get_opfamily_proc(typentry->btree_opf, typentry->btree_opintype, typentry->btree_opintype, BTORDER_PROC);
    }

       
                                                                           
                                            
       
    if (cmp_proc == F_BTARRAYCMP && !array_element_has_compare(typentry))
    {
      cmp_proc = InvalidOid;
    }
    else if (cmp_proc == F_BTRECORDCMP && !record_fields_have_compare(typentry))
    {
      cmp_proc = InvalidOid;
    }

                                                                     
    if (typentry->cmp_proc != cmp_proc)
    {
      typentry->cmp_proc_finfo.fn_oid = InvalidOid;
    }

    typentry->cmp_proc = cmp_proc;
    typentry->flags |= TCFLAGS_CHECKED_CMP_PROC;
  }
  if ((flags & (TYPECACHE_HASH_PROC | TYPECACHE_HASH_PROC_FINFO)) && !(typentry->flags & TCFLAGS_CHECKED_HASH_PROC))
  {
    Oid hash_proc = InvalidOid;

       
                                                                        
                                                            
       
    if (typentry->hash_opf != InvalidOid && (!OidIsValid(typentry->eq_opr) || typentry->eq_opr == get_opfamily_member(typentry->hash_opf, typentry->hash_opintype, typentry->hash_opintype, HTEqualStrategyNumber)))
    {
      hash_proc = get_opfamily_proc(typentry->hash_opf, typentry->hash_opintype, typentry->hash_opintype, HASHSTANDARD_PROC);
    }

       
                                                                        
                                                                       
                                               
       
    if (hash_proc == F_HASH_ARRAY && !array_element_has_hashing(typentry))
    {
      hash_proc = InvalidOid;
    }

       
                                
       
    if (hash_proc == F_HASH_RANGE && !range_element_has_hashing(typentry))
    {
      hash_proc = InvalidOid;
    }

                                                                      
    if (typentry->hash_proc != hash_proc)
    {
      typentry->hash_proc_finfo.fn_oid = InvalidOid;
    }

    typentry->hash_proc = hash_proc;
    typentry->flags |= TCFLAGS_CHECKED_HASH_PROC;
  }
  if ((flags & (TYPECACHE_HASH_EXTENDED_PROC | TYPECACHE_HASH_EXTENDED_PROC_FINFO)) && !(typentry->flags & TCFLAGS_CHECKED_HASH_EXTENDED_PROC))
  {
    Oid hash_extended_proc = InvalidOid;

       
                                                                        
                                                            
       
    if (typentry->hash_opf != InvalidOid && (!OidIsValid(typentry->eq_opr) || typentry->eq_opr == get_opfamily_member(typentry->hash_opf, typentry->hash_opintype, typentry->hash_opintype, HTEqualStrategyNumber)))
    {
      hash_extended_proc = get_opfamily_proc(typentry->hash_opf, typentry->hash_opintype, typentry->hash_opintype, HASHEXTENDED_PROC);
    }

       
                                                                       
                                                                      
                                                          
       
    if (hash_extended_proc == F_HASH_ARRAY_EXTENDED && !array_element_has_extended_hashing(typentry))
    {
      hash_extended_proc = InvalidOid;
    }

       
                                         
       
    if (hash_extended_proc == F_HASH_RANGE_EXTENDED && !range_element_has_extended_hashing(typentry))
    {
      hash_extended_proc = InvalidOid;
    }

                                                                 
    if (typentry->hash_extended_proc != hash_extended_proc)
    {
      typentry->hash_extended_proc_finfo.fn_oid = InvalidOid;
    }

    typentry->hash_extended_proc = hash_extended_proc;
    typentry->flags |= TCFLAGS_CHECKED_HASH_EXTENDED_PROC;
  }

     
                                          
     
                                                                         
                                                                          
                                                        
     
                                                                           
                                                                            
                                                                            
                                                     
     
  if ((flags & TYPECACHE_EQ_OPR_FINFO) && typentry->eq_opr_finfo.fn_oid == InvalidOid && typentry->eq_opr != InvalidOid)
  {
    Oid eq_opr_func;

    eq_opr_func = get_opcode(typentry->eq_opr);
    if (eq_opr_func != InvalidOid)
    {
      fmgr_info_cxt(eq_opr_func, &typentry->eq_opr_finfo, CacheMemoryContext);
    }
  }
  if ((flags & TYPECACHE_CMP_PROC_FINFO) && typentry->cmp_proc_finfo.fn_oid == InvalidOid && typentry->cmp_proc != InvalidOid)
  {
    fmgr_info_cxt(typentry->cmp_proc, &typentry->cmp_proc_finfo, CacheMemoryContext);
  }
  if ((flags & TYPECACHE_HASH_PROC_FINFO) && typentry->hash_proc_finfo.fn_oid == InvalidOid && typentry->hash_proc != InvalidOid)
  {
    fmgr_info_cxt(typentry->hash_proc, &typentry->hash_proc_finfo, CacheMemoryContext);
  }
  if ((flags & TYPECACHE_HASH_EXTENDED_PROC_FINFO) && typentry->hash_extended_proc_finfo.fn_oid == InvalidOid && typentry->hash_extended_proc != InvalidOid)
  {
    fmgr_info_cxt(typentry->hash_extended_proc, &typentry->hash_extended_proc_finfo, CacheMemoryContext);
  }

     
                                                                   
     
  if ((flags & TYPECACHE_TUPDESC) && typentry->tupDesc == NULL && typentry->typtype == TYPTYPE_COMPOSITE)
  {
    load_typcache_tupdesc(typentry);
  }

     
                                                      
     
  if ((flags & TYPECACHE_RANGE_INFO) && typentry->rngelemtype == NULL && typentry->typtype == TYPTYPE_RANGE)
  {
    load_rangetype_info(typentry);
  }

     
                                                       
     
  if ((flags & TYPECACHE_DOMAIN_BASE_INFO) && typentry->domainBaseType == InvalidOid && typentry->typtype == TYPTYPE_DOMAIN)
  {
    typentry->domainBaseTypmod = -1;
    typentry->domainBaseType = getBaseTypeAndTypmod(type_id, &typentry->domainBaseTypmod);
  }
  if ((flags & TYPECACHE_DOMAIN_CONSTR_INFO) && (typentry->flags & TCFLAGS_CHECKED_DOMAIN_CONSTRAINTS) == 0 && typentry->typtype == TYPTYPE_DOMAIN)
  {
    load_domaintype_info(typentry);
  }

  return typentry;
}

   
                                                                               
   
static void
load_typcache_tupdesc(TypeCacheEntry *typentry)
{
  Relation rel;

  if (!OidIsValid(typentry->typrelid))                        
  {
    elog(ERROR, "invalid typrelid for composite type %u", typentry->type_id);
  }
  rel = relation_open(typentry->typrelid, AccessShareLock);
  Assert(rel->rd_rel->reltype == typentry->type_id);

     
                                                                      
                                                                             
                                                                             
                                       
     
  typentry->tupDesc = RelationGetDescr(rel);

  Assert(typentry->tupDesc->tdrefcount > 0);
  typentry->tupDesc->tdrefcount++;

     
                                                                             
                                                                      
     
  typentry->tupDesc_identifier = ++tupledesc_id_counter;

  relation_close(rel, AccessShareLock);
}

   
                                                                           
   
static void
load_rangetype_info(TypeCacheEntry *typentry)
{
  Form_pg_range pg_range;
  HeapTuple tup;
  Oid subtypeOid;
  Oid opclassOid;
  Oid canonicalOid;
  Oid subdiffOid;
  Oid opfamilyOid;
  Oid opcintype;
  Oid cmpFnOid;

                                     
  tup = SearchSysCache1(RANGETYPE, ObjectIdGetDatum(typentry->type_id));
                                                             
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for range type %u", typentry->type_id);
  }
  pg_range = (Form_pg_range)GETSTRUCT(tup);

  subtypeOid = pg_range->rngsubtype;
  typentry->rng_collation = pg_range->rngcollation;
  opclassOid = pg_range->rngsubopc;
  canonicalOid = pg_range->rngcanonical;
  subdiffOid = pg_range->rngsubdiff;

  ReleaseSysCache(tup);

                                                                  
  opfamilyOid = get_opclass_family(opclassOid);
  opcintype = get_opclass_input_type(opclassOid);

  cmpFnOid = get_opfamily_proc(opfamilyOid, opcintype, opcintype, BTORDER_PROC);
  if (!RegProcedureIsValid(cmpFnOid))
  {
    elog(ERROR, "missing support function %d(%u,%u) in opfamily %u", BTORDER_PROC, opcintype, opcintype, opfamilyOid);
  }

                                      
  fmgr_info_cxt(cmpFnOid, &typentry->rng_cmp_proc_finfo, CacheMemoryContext);
  if (OidIsValid(canonicalOid))
  {
    fmgr_info_cxt(canonicalOid, &typentry->rng_canonical_finfo, CacheMemoryContext);
  }
  if (OidIsValid(subdiffOid))
  {
    fmgr_info_cxt(subdiffOid, &typentry->rng_subdiff_finfo, CacheMemoryContext);
  }

                                                                         
  typentry->rngelemtype = lookup_type_cache(subtypeOid, 0);
}

   
                                                                            
   
                                                                             
                                                                            
                                                                       
                                                                       
             
   
static void
load_domaintype_info(TypeCacheEntry *typentry)
{
  Oid typeOid = typentry->type_id;
  DomainConstraintCache *dcc;
  bool notNull = false;
  DomainConstraintState **ccons;
  int cconslen;
  Relation conRel;
  MemoryContext oldcxt;

     
                                                                          
                                                                            
     
  if (typentry->domainData)
  {
    dcc = typentry->domainData;
    typentry->domainData = NULL;
    decr_dcc_refcount(dcc);
  }

     
                                                                           
                                                                             
                                 
     
  dcc = NULL;
  ccons = NULL;
  cconslen = 0;

     
                                                                   
                                                                            
                                            
     
  conRel = table_open(ConstraintRelationId, AccessShareLock);

  for (;;)
  {
    HeapTuple tup;
    HeapTuple conTup;
    Form_pg_type typTup;
    int nccons = 0;
    ScanKeyData key[1];
    SysScanDesc scan;

    tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeOid));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for type %u", typeOid);
    }
    typTup = (Form_pg_type)GETSTRUCT(tup);

    if (typTup->typtype != TYPTYPE_DOMAIN)
    {
                                 
      ReleaseSysCache(tup);
      break;
    }

                                      
    if (typTup->typnotnull)
    {
      notNull = true;
    }

                                                   
    ScanKeyInit(&key[0], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(typeOid));

    scan = systable_beginscan(conRel, ConstraintTypidIndexId, true, NULL, 1, key);

    while (HeapTupleIsValid(conTup = systable_getnext(scan)))
    {
      Form_pg_constraint c = (Form_pg_constraint)GETSTRUCT(conTup);
      Datum val;
      bool isNull;
      char *constring;
      Expr *check_expr;
      DomainConstraintState *r;

                                                                      
      if (c->contype != CONSTRAINT_CHECK)
      {
        continue;
      }

                                                                         
      val = fastgetattr(conTup, Anum_pg_constraint_conbin, conRel->rd_att, &isNull);
      if (isNull)
      {
        elog(ERROR, "domain \"%s\" constraint \"%s\" has NULL conbin", NameStr(typTup->typname), NameStr(c->conname));
      }

                                                        
      constring = TextDatumGetCString(val);

                                                                         
      if (dcc == NULL)
      {
        MemoryContext cxt;

        cxt = AllocSetContextCreate(CurrentMemoryContext, "Domain constraints", ALLOCSET_SMALL_SIZES);
        dcc = (DomainConstraintCache *)MemoryContextAlloc(cxt, sizeof(DomainConstraintCache));
        dcc->constraints = NIL;
        dcc->dccContext = cxt;
        dcc->dccRefCount = 0;
      }

                                                                
      oldcxt = MemoryContextSwitchTo(dcc->dccContext);

      check_expr = (Expr *)stringToNode(constring);

         
                                                                   
         
                                                                      
                                                                     
                                                                 
                                                                      
                                                                      
                                    
         
      check_expr = expression_planner(check_expr);

      r = makeNode(DomainConstraintState);
      r->constrainttype = DOM_CONSTRAINT_CHECK;
      r->name = pstrdup(NameStr(c->conname));
      r->check_expr = check_expr;
      r->check_exprstate = NULL;

      MemoryContextSwitchTo(oldcxt);

                                                                 
      if (ccons == NULL)
      {
        cconslen = 8;
        ccons = (DomainConstraintState **)palloc(cconslen * sizeof(DomainConstraintState *));
      }
      else if (nccons >= cconslen)
      {
        cconslen *= 2;
        ccons = (DomainConstraintState **)repalloc(ccons, cconslen * sizeof(DomainConstraintState *));
      }
      ccons[nccons++] = r;
    }

    systable_endscan(scan);

    if (nccons > 0)
    {
         
                                                                         
                              
         
      if (nccons > 1)
      {
        qsort(ccons, nccons, sizeof(DomainConstraintState *), dcs_cmp);
      }

         
                                                                        
                                                                  
         
      oldcxt = MemoryContextSwitchTo(dcc->dccContext);
      while (nccons > 0)
      {
        dcc->constraints = lcons(ccons[--nccons], dcc->constraints);
      }
      MemoryContextSwitchTo(oldcxt);
    }

                                      
    typeOid = typTup->typbasetype;
    ReleaseSysCache(tup);
  }

  table_close(conRel, AccessShareLock);

     
                                                                           
                           
     
  if (notNull)
  {
    DomainConstraintState *r;

                                                                       
    if (dcc == NULL)
    {
      MemoryContext cxt;

      cxt = AllocSetContextCreate(CurrentMemoryContext, "Domain constraints", ALLOCSET_SMALL_SIZES);
      dcc = (DomainConstraintCache *)MemoryContextAlloc(cxt, sizeof(DomainConstraintCache));
      dcc->constraints = NIL;
      dcc->dccContext = cxt;
      dcc->dccRefCount = 0;
    }

                                                              
    oldcxt = MemoryContextSwitchTo(dcc->dccContext);

    r = makeNode(DomainConstraintState);

    r->constrainttype = DOM_CONSTRAINT_NOTNULL;
    r->name = pstrdup("NOT NULL");
    r->check_expr = NULL;
    r->check_exprstate = NULL;

                                                 
    dcc->constraints = lcons(r, dcc->constraints);

    MemoryContextSwitchTo(oldcxt);
  }

     
                                                                         
                                      
     
  if (dcc)
  {
    MemoryContextSetParent(dcc->dccContext, CacheMemoryContext);
    typentry->domainData = dcc;
    dcc->dccRefCount++;                                     
  }

                                                                  
  typentry->flags |= TCFLAGS_CHECKED_DOMAIN_CONSTRAINTS;
}

   
                                                                   
   
static int
dcs_cmp(const void *a, const void *b)
{
  const DomainConstraintState *const *ca = (const DomainConstraintState *const *)a;
  const DomainConstraintState *const *cb = (const DomainConstraintState *const *)b;

  return strcmp((*ca)->name, (*cb)->name);
}

   
                                                                       
                                       
   
static void
decr_dcc_refcount(DomainConstraintCache *dcc)
{
  Assert(dcc->dccRefCount > 0);
  if (--(dcc->dccRefCount) <= 0)
  {
    MemoryContextDelete(dcc->dccContext);
  }
}

   
                                                           
   
static void
dccref_deletion_callback(void *arg)
{
  DomainConstraintRef *ref = (DomainConstraintRef *)arg;
  DomainConstraintCache *dcc = ref->dcc;

                                                                    
  if (dcc)
  {
    ref->constraints = NIL;
    ref->dcc = NULL;
    decr_dcc_refcount(dcc);
  }
}

   
                                                                        
   
                                                                       
                                                                     
   
static List *
prep_domain_constraints(List *constraints, MemoryContext execctx)
{
  List *result = NIL;
  MemoryContext oldcxt;
  ListCell *lc;

  oldcxt = MemoryContextSwitchTo(execctx);

  foreach (lc, constraints)
  {
    DomainConstraintState *r = (DomainConstraintState *)lfirst(lc);
    DomainConstraintState *newr;

    newr = makeNode(DomainConstraintState);
    newr->constrainttype = r->constrainttype;
    newr->name = r->name;
    newr->check_expr = r->check_expr;
    newr->check_exprstate = ExecInitExpr(r->check_expr, NULL);

    result = lappend(result, newr);
  }

  MemoryContextSwitchTo(oldcxt);

  return result;
}

   
                                                                       
   
                                                                          
                                                                          
   
                                                                          
                                                                     
                                                                             
   
void
InitDomainConstraintRef(Oid type_id, DomainConstraintRef *ref, MemoryContext refctx, bool need_exprstate)
{
                                                                         
  ref->tcache = lookup_type_cache(type_id, TYPECACHE_DOMAIN_CONSTR_INFO);
  ref->need_exprstate = need_exprstate;
                                                                      
  ref->refctx = refctx;
  ref->dcc = NULL;
  ref->callback.func = dccref_deletion_callback;
  ref->callback.arg = (void *)ref;
  MemoryContextRegisterResetCallback(refctx, &ref->callback);
                                                                           
  if (ref->tcache->domainData)
  {
    ref->dcc = ref->tcache->domainData;
    ref->dcc->dccRefCount++;
    if (ref->need_exprstate)
    {
      ref->constraints = prep_domain_constraints(ref->dcc->constraints, ref->refctx);
    }
    else
    {
      ref->constraints = ref->dcc->constraints;
    }
  }
  else
  {
    ref->constraints = NIL;
  }
}

   
                                                                            
   
                                                                          
                                              
   
                                                                          
                                                                         
                           
   
void
UpdateDomainConstraintRef(DomainConstraintRef *ref)
{
  TypeCacheEntry *typentry = ref->tcache;

                                                     
  if ((typentry->flags & TCFLAGS_CHECKED_DOMAIN_CONSTRAINTS) == 0 && typentry->typtype == TYPTYPE_DOMAIN)
  {
    load_domaintype_info(typentry);
  }

                                                                       
  if (ref->dcc != typentry->domainData)
  {
                                                                      
    DomainConstraintCache *dcc = ref->dcc;

    if (dcc)
    {
         
                                                                   
                                                                     
                                                                     
                                                                     
                                                                     
                                                                         
         
      ref->constraints = NIL;
      ref->dcc = NULL;
      decr_dcc_refcount(dcc);
    }
    dcc = typentry->domainData;
    if (dcc)
    {
      ref->dcc = dcc;
      dcc->dccRefCount++;
      if (ref->need_exprstate)
      {
        ref->constraints = prep_domain_constraints(dcc->constraints, ref->refctx);
      }
      else
      {
        ref->constraints = dcc->constraints;
      }
    }
  }
}

   
                                                                                 
   
                                                                       
   
bool
DomainHasConstraints(Oid type_id)
{
  TypeCacheEntry *typentry;

     
                                                                          
                                                                           
     
  typentry = lookup_type_cache(type_id, TYPECACHE_DOMAIN_CONSTR_INFO);

  return (typentry->domainData != NULL);
}

   
                                                                       
                                                                           
                                              
   
                                                                           
                                                                          
                                                                           
                                                                     
                          
   

static bool
array_element_has_equality(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_ELEM_PROPERTIES))
  {
    cache_array_element_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_ELEM_EQUALITY) != 0;
}

static bool
array_element_has_compare(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_ELEM_PROPERTIES))
  {
    cache_array_element_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_ELEM_COMPARE) != 0;
}

static bool
array_element_has_hashing(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_ELEM_PROPERTIES))
  {
    cache_array_element_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_ELEM_HASHING) != 0;
}

static bool
array_element_has_extended_hashing(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_ELEM_PROPERTIES))
  {
    cache_array_element_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_ELEM_EXTENDED_HASHING) != 0;
}

static void
cache_array_element_properties(TypeCacheEntry *typentry)
{
  Oid elem_type = get_base_element_type(typentry->type_id);

  if (OidIsValid(elem_type))
  {
    TypeCacheEntry *elementry;

    elementry = lookup_type_cache(elem_type, TYPECACHE_EQ_OPR | TYPECACHE_CMP_PROC | TYPECACHE_HASH_PROC | TYPECACHE_HASH_EXTENDED_PROC);
    if (OidIsValid(elementry->eq_opr))
    {
      typentry->flags |= TCFLAGS_HAVE_ELEM_EQUALITY;
    }
    if (OidIsValid(elementry->cmp_proc))
    {
      typentry->flags |= TCFLAGS_HAVE_ELEM_COMPARE;
    }
    if (OidIsValid(elementry->hash_proc))
    {
      typentry->flags |= TCFLAGS_HAVE_ELEM_HASHING;
    }
    if (OidIsValid(elementry->hash_extended_proc))
    {
      typentry->flags |= TCFLAGS_HAVE_ELEM_EXTENDED_HASHING;
    }
  }
  typentry->flags |= TCFLAGS_CHECKED_ELEM_PROPERTIES;
}

   
                                                        
   

static bool
record_fields_have_equality(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_FIELD_PROPERTIES))
  {
    cache_record_field_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_FIELD_EQUALITY) != 0;
}

static bool
record_fields_have_compare(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_FIELD_PROPERTIES))
  {
    cache_record_field_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_FIELD_COMPARE) != 0;
}

static void
cache_record_field_properties(TypeCacheEntry *typentry)
{
     
                                                                          
                                                                        
                                                           
     
  if (typentry->type_id == RECORDOID)
  {
    typentry->flags |= (TCFLAGS_HAVE_FIELD_EQUALITY | TCFLAGS_HAVE_FIELD_COMPARE);
  }
  else if (typentry->typtype == TYPTYPE_COMPOSITE)
  {
    TupleDesc tupdesc;
    int newflags;
    int i;

                                                                    
    if (typentry->tupDesc == NULL)
    {
      load_typcache_tupdesc(typentry);
    }
    tupdesc = typentry->tupDesc;

                                                                       
    IncrTupleDescRefCount(tupdesc);

                                                                        
    newflags = (TCFLAGS_HAVE_FIELD_EQUALITY | TCFLAGS_HAVE_FIELD_COMPARE);
    for (i = 0; i < tupdesc->natts; i++)
    {
      TypeCacheEntry *fieldentry;
      Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

      if (attr->attisdropped)
      {
        continue;
      }

      fieldentry = lookup_type_cache(attr->atttypid, TYPECACHE_EQ_OPR | TYPECACHE_CMP_PROC);
      if (!OidIsValid(fieldentry->eq_opr))
      {
        newflags &= ~TCFLAGS_HAVE_FIELD_EQUALITY;
      }
      if (!OidIsValid(fieldentry->cmp_proc))
      {
        newflags &= ~TCFLAGS_HAVE_FIELD_COMPARE;
      }

                                                                 
      if (newflags == 0)
      {
        break;
      }
    }
    typentry->flags |= newflags;

    DecrTupleDescRefCount(tupdesc);
  }
  else if (typentry->typtype == TYPTYPE_DOMAIN)
  {
                                                                    
    TypeCacheEntry *baseentry;

                                                    
    if (typentry->domainBaseType == InvalidOid)
    {
      typentry->domainBaseTypmod = -1;
      typentry->domainBaseType = getBaseTypeAndTypmod(typentry->type_id, &typentry->domainBaseTypmod);
    }
    baseentry = lookup_type_cache(typentry->domainBaseType, TYPECACHE_EQ_OPR | TYPECACHE_CMP_PROC);
    if (baseentry->typtype == TYPTYPE_COMPOSITE)
    {
      typentry->flags |= TCFLAGS_DOMAIN_BASE_IS_COMPOSITE;
      typentry->flags |= baseentry->flags & (TCFLAGS_HAVE_FIELD_EQUALITY | TCFLAGS_HAVE_FIELD_COMPARE);
    }
  }
  typentry->flags |= TCFLAGS_CHECKED_FIELD_PROPERTIES;
}

   
                                                    
   
                                                                             
                                                                        
                                
   

static bool
range_element_has_hashing(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_ELEM_PROPERTIES))
  {
    cache_range_element_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_ELEM_HASHING) != 0;
}

static bool
range_element_has_extended_hashing(TypeCacheEntry *typentry)
{
  if (!(typentry->flags & TCFLAGS_CHECKED_ELEM_PROPERTIES))
  {
    cache_range_element_properties(typentry);
  }
  return (typentry->flags & TCFLAGS_HAVE_ELEM_EXTENDED_HASHING) != 0;
}

static void
cache_range_element_properties(TypeCacheEntry *typentry)
{
                                                 
  if (typentry->rngelemtype == NULL && typentry->typtype == TYPTYPE_RANGE)
  {
    load_rangetype_info(typentry);
  }

  if (typentry->rngelemtype != NULL)
  {
    TypeCacheEntry *elementry;

                                                                    
    elementry = lookup_type_cache(typentry->rngelemtype->type_id, TYPECACHE_HASH_PROC | TYPECACHE_HASH_EXTENDED_PROC);
    if (OidIsValid(elementry->hash_proc))
    {
      typentry->flags |= TCFLAGS_HAVE_ELEM_HASHING;
    }
    if (OidIsValid(elementry->hash_extended_proc))
    {
      typentry->flags |= TCFLAGS_HAVE_ELEM_EXTENDED_HASHING;
    }
  }
  typentry->flags |= TCFLAGS_CHECKED_ELEM_PROPERTIES;
}

   
                                                                              
                      
   
static void
ensure_record_cache_typmod_slot_exists(int32 typmod)
{
  if (RecordCacheArray == NULL)
  {
    RecordCacheArray = (TupleDesc *)MemoryContextAllocZero(CacheMemoryContext, 64 * sizeof(TupleDesc));
    RecordIdentifierArray = (uint64 *)MemoryContextAllocZero(CacheMemoryContext, 64 * sizeof(uint64));
    RecordCacheArrayLen = 64;
  }

  if (typmod >= RecordCacheArrayLen)
  {
    int32 newlen = RecordCacheArrayLen * 2;

    while (typmod >= newlen)
    {
      newlen *= 2;
    }

    RecordCacheArray = (TupleDesc *)repalloc(RecordCacheArray, newlen * sizeof(TupleDesc));
    memset(RecordCacheArray + RecordCacheArrayLen, 0, (newlen - RecordCacheArrayLen) * sizeof(TupleDesc));
    RecordIdentifierArray = (uint64 *)repalloc(RecordIdentifierArray, newlen * sizeof(uint64));
    memset(RecordIdentifierArray + RecordCacheArrayLen, 0, (newlen - RecordCacheArrayLen) * sizeof(uint64));
    RecordCacheArrayLen = newlen;
  }
}

   
                                                                            
   
                                                                        
                                   
   
static TupleDesc
lookup_rowtype_tupdesc_internal(Oid type_id, int32 typmod, bool noError)
{
  if (type_id != RECORDOID)
  {
       
                                                                 
       
    TypeCacheEntry *typentry;

    typentry = lookup_type_cache(type_id, TYPECACHE_TUPDESC);
    if (typentry->tupDesc == NULL && !noError)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(type_id))));
    }
    return typentry->tupDesc;
  }
  else
  {
       
                                                                       
       
    if (typmod >= 0)
    {
                                             
      if (typmod < RecordCacheArrayLen && RecordCacheArray[typmod] != NULL)
      {
        return RecordCacheArray[typmod];
      }

                                                               
      if (CurrentSession->shared_typmod_registry != NULL)
      {
        SharedTypmodTableEntry *entry;

                                                        
        entry = dshash_find(CurrentSession->shared_typmod_table, &typmod, false);
        if (entry != NULL)
        {
          TupleDesc tupdesc;

          tupdesc = (TupleDesc)dsa_get_address(CurrentSession->area, entry->shared_tupdesc);
          Assert(typmod == tupdesc->tdtypmod);

                                                                 
          ensure_record_cache_typmod_slot_exists(typmod);

             
                                                                     
                                                               
             
          RecordCacheArray[typmod] = tupdesc;
          Assert(tupdesc->tdrefcount == -1);

             
                                                                     
                                 
             
          RecordIdentifierArray[typmod] = ++tupledesc_id_counter;

          dshash_release_lock(CurrentSession->shared_typmod_table, entry);

          return RecordCacheArray[typmod];
        }
      }
    }

    if (!noError)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("record type has not been registered")));
    }
    return NULL;
  }
}

   
                          
   
                                                                      
                                                                       
                                                                    
                                  
   
                                                                          
                                                                      
                                                                          
   
TupleDesc
lookup_rowtype_tupdesc(Oid type_id, int32 typmod)
{
  TupleDesc tupDesc;

  tupDesc = lookup_rowtype_tupdesc_internal(type_id, typmod, false);
  PinTupleDesc(tupDesc);
  return tupDesc;
}

   
                                  
   
                                                                       
                                                                        
                                                     
   
TupleDesc
lookup_rowtype_tupdesc_noerror(Oid type_id, int32 typmod, bool noError)
{
  TupleDesc tupDesc;

  tupDesc = lookup_rowtype_tupdesc_internal(type_id, typmod, noError);
  if (tupDesc != NULL)
  {
    PinTupleDesc(tupDesc);
  }
  return tupDesc;
}

   
                               
   
                                                                      
                                                                      
   
TupleDesc
lookup_rowtype_tupdesc_copy(Oid type_id, int32 typmod)
{
  TupleDesc tmp;

  tmp = lookup_rowtype_tupdesc_internal(type_id, typmod, false);
  return CreateTupleDescCopyConstr(tmp);
}

   
                                 
   
                                                                              
                                                                              
                                                                         
                                  
   
                                                                              
                                                                          
                                                                              
                                                            
   
TupleDesc
lookup_rowtype_tupdesc_domain(Oid type_id, int32 typmod, bool noError)
{
  TupleDesc tupDesc;

  if (type_id != RECORDOID)
  {
       
                                                                        
                                 
       
    TypeCacheEntry *typentry;

    typentry = lookup_type_cache(type_id, TYPECACHE_TUPDESC | TYPECACHE_DOMAIN_BASE_INFO);
    if (typentry->typtype == TYPTYPE_DOMAIN)
    {
      return lookup_rowtype_tupdesc_noerror(typentry->domainBaseType, typentry->domainBaseTypmod, noError);
    }
    if (typentry->tupDesc == NULL && !noError)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(type_id))));
    }
    tupDesc = typentry->tupDesc;
  }
  else
  {
    tupDesc = lookup_rowtype_tupdesc_internal(type_id, typmod, noError);
  }
  if (tupDesc != NULL)
  {
    PinTupleDesc(tupDesc);
  }
  return tupDesc;
}

   
                                                         
   
static uint32
record_type_typmod_hash(const void *data, size_t size)
{
  RecordCacheEntry *entry = (RecordCacheEntry *)data;

  return hashTupleDesc(entry->tupdesc);
}

   
                                                          
   
static int
record_type_typmod_compare(const void *a, const void *b, size_t size)
{
  RecordCacheEntry *left = (RecordCacheEntry *)a;
  RecordCacheEntry *right = (RecordCacheEntry *)b;

  return equalTupleDescs(left->tupdesc, right->tupdesc) ? 0 : 1;
}

   
                             
   
                                                                            
                                                                           
                                                        
   
void
assign_record_type_typmod(TupleDesc tupDesc)
{
  RecordCacheEntry *recentry;
  TupleDesc entDesc;
  bool found;
  MemoryContext oldcxt;

  Assert(tupDesc->tdtypeid == RECORDOID);

  if (RecordCacheHash == NULL)
  {
                                                       
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(TupleDesc);                       
    ctl.entrysize = sizeof(RecordCacheEntry);
    ctl.hash = record_type_typmod_hash;
    ctl.match = record_type_typmod_compare;
    RecordCacheHash = hash_create("Record information cache", 64, &ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE);

                                                  
    if (!CacheMemoryContext)
    {
      CreateCacheMemoryContext();
    }
  }

     
                                                                    
                                                                            
                                                             
     
  recentry = (RecordCacheEntry *)hash_search(RecordCacheHash, (void *)&tupDesc, HASH_FIND, &found);
  if (found && recentry->tupdesc != NULL)
  {
    tupDesc->tdtypmod = recentry->tupdesc->tdtypmod;
    return;
  }

                                                    
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

                                                           
  entDesc = find_or_make_matching_shared_tupledesc(tupDesc);
  if (entDesc == NULL)
  {
       
                                                                         
                         
       
    ensure_record_cache_typmod_slot_exists(NextRecordTypmod);

                                             
    entDesc = CreateTupleDescCopy(tupDesc);
    entDesc->tdrefcount = 1;
    entDesc->tdtypmod = NextRecordTypmod++;
  }
  else
  {
    ensure_record_cache_typmod_slot_exists(entDesc->tdtypmod);
  }

  RecordCacheArray[entDesc->tdtypmod] = entDesc;

                                                
  RecordIdentifierArray[entDesc->tdtypmod] = ++tupledesc_id_counter;

                                                      
  recentry = (RecordCacheEntry *)hash_search(RecordCacheHash, (void *)&tupDesc, HASH_ENTER, NULL);
  recentry->tupdesc = entDesc;

                                             
  tupDesc->tdtypmod = entDesc->tdtypmod;

  MemoryContextSwitchTo(oldcxt);
}

   
                                 
   
                                                                             
                                                                              
                                                                              
                                                                            
                                                                           
                                                                      
   
uint64
assign_record_type_identifier(Oid type_id, int32 typmod)
{
  if (type_id != RECORDOID)
  {
       
                                                                 
       
    TypeCacheEntry *typentry;

    typentry = lookup_type_cache(type_id, TYPECACHE_TUPDESC);
    if (typentry->tupDesc == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not composite", format_type_be(type_id))));
    }
    Assert(typentry->tupDesc_identifier != 0);
    return typentry->tupDesc_identifier;
  }
  else
  {
       
                                                                       
       
    if (typmod >= 0 && typmod < RecordCacheArrayLen && RecordCacheArray[typmod] != NULL)
    {
      Assert(RecordIdentifierArray[typmod] != 0);
      return RecordIdentifierArray[typmod];
    }

                                                                      
    return ++tupledesc_id_counter;
  }
}

   
                                                                             
                                                         
                                           
   
size_t
SharedRecordTypmodRegistryEstimate(void)
{
  return sizeof(SharedRecordTypmodRegistry);
}

   
                                                                               
                                                                             
          
   
                                                                           
                                                                               
                                                                              
                                                                      
                                                                            
                                   
   
                                                                               
                                                                          
                                   
   
void
SharedRecordTypmodRegistryInit(SharedRecordTypmodRegistry *registry, dsm_segment *segment, dsa_area *area)
{
  MemoryContext old_context;
  dshash_table *record_table;
  dshash_table *typmod_table;
  int32 typmod;

  Assert(!IsParallelWorker());

                                                          
  Assert(CurrentSession->shared_typmod_registry == NULL);
  Assert(CurrentSession->shared_record_table == NULL);
  Assert(CurrentSession->shared_typmod_table == NULL);

  old_context = MemoryContextSwitchTo(TopMemoryContext);

                                                                         
  record_table = dshash_create(area, &srtr_record_table_params, area);

                                                                     
  typmod_table = dshash_create(area, &srtr_typmod_table_params, NULL);

  MemoryContextSwitchTo(old_context);

                                                  
  registry->record_table_handle = dshash_get_hash_table_handle(record_table);
  registry->typmod_table_handle = dshash_get_hash_table_handle(typmod_table);
  pg_atomic_init_u32(&registry->next_typmod, NextRecordTypmod);

     
                                                                           
               
     
  for (typmod = 0; typmod < NextRecordTypmod; ++typmod)
  {
    SharedTypmodTableEntry *typmod_table_entry;
    SharedRecordTableEntry *record_table_entry;
    SharedRecordTableKey record_table_key;
    dsa_pointer shared_dp;
    TupleDesc tupdesc;
    bool found;

    tupdesc = RecordCacheArray[typmod];
    if (tupdesc == NULL)
    {
      continue;
    }

                                                
    shared_dp = share_tupledesc(area, tupdesc, typmod);

                                       
    typmod_table_entry = dshash_find_or_insert(typmod_table, &tupdesc->tdtypmod, &found);
    if (found)
    {
      elog(ERROR, "cannot create duplicate shared record typmod");
    }
    typmod_table_entry->typmod = tupdesc->tdtypmod;
    typmod_table_entry->shared_tupdesc = shared_dp;
    dshash_release_lock(typmod_table, typmod_table_entry);

                                       
    record_table_key.shared = false;
    record_table_key.u.local_tupdesc = tupdesc;
    record_table_entry = dshash_find_or_insert(record_table, &record_table_key, &found);
    if (!found)
    {
      record_table_entry->key.shared = true;
      record_table_entry->key.u.shared_tupdesc = shared_dp;
    }
    dshash_release_lock(record_table, record_table_entry);
  }

     
                                                                          
                                                                
     
  CurrentSession->shared_record_table = record_table;
  CurrentSession->shared_typmod_table = typmod_table;
  CurrentSession->shared_typmod_registry = registry;

     
                                                                           
                                                                            
                                                                        
            
     
  on_dsm_detach(segment, shared_record_typmod_registry_detach, (Datum)0);
}

   
                                                                             
                                                           
                                                                          
                                
   
void
SharedRecordTypmodRegistryAttach(SharedRecordTypmodRegistry *registry)
{
  MemoryContext old_context;
  dshash_table *record_table;
  dshash_table *typmod_table;

  Assert(IsParallelWorker());

                                                          
  Assert(CurrentSession != NULL);
  Assert(CurrentSession->segment != NULL);
  Assert(CurrentSession->area != NULL);
  Assert(CurrentSession->shared_typmod_registry == NULL);
  Assert(CurrentSession->shared_record_table == NULL);
  Assert(CurrentSession->shared_typmod_table == NULL);

     
                                                                            
                                                                            
                                                                   
                                                                      
                                                                       
                                                                           
                                                                           
                                      
     
  Assert(NextRecordTypmod == 0);

  old_context = MemoryContextSwitchTo(TopMemoryContext);

                                      
  record_table = dshash_attach(CurrentSession->area, &srtr_record_table_params, registry->record_table_handle, CurrentSession->area);
  typmod_table = dshash_attach(CurrentSession->area, &srtr_typmod_table_params, registry->typmod_table_handle, NULL);

  MemoryContextSwitchTo(old_context);

     
                                                                           
                                                                      
                
     
  on_dsm_detach(CurrentSession->segment, shared_record_typmod_registry_detach, PointerGetDatum(registry));

     
                                                                           
                                                                
     
  CurrentSession->shared_typmod_registry = registry;
  CurrentSession->shared_record_table = record_table;
  CurrentSession->shared_typmod_table = typmod_table;
}

   
                        
                                     
   
                                                                             
                                                                        
                                                                          
   
                                                                          
                                                                        
                                                                             
                                                                         
                                                                           
                                                                            
                                                                          
                                                                             
              
   
                                                                            
                                                                             
                                                    
   
static void
TypeCacheRelCallback(Datum arg, Oid relid)
{
  HASH_SEQ_STATUS status;
  TypeCacheEntry *typentry;

                                                                           
  hash_seq_init(&status, TypeCacheHash);
  while ((typentry = (TypeCacheEntry *)hash_seq_search(&status)) != NULL)
  {
    if (typentry->typtype == TYPTYPE_COMPOSITE)
    {
                                                                      
      if (relid != typentry->typrelid && relid != InvalidOid)
      {
        continue;
      }

                                        
      if (typentry->tupDesc != NULL)
      {
           
                                                                      
                                                                      
                                                  
           
        Assert(typentry->tupDesc->tdrefcount > 0);
        if (--typentry->tupDesc->tdrefcount == 0)
        {
          FreeTupleDesc(typentry->tupDesc);
        }
        typentry->tupDesc = NULL;

           
                                                                    
                                                                    
                                                                    
                                                                      
                                                                       
                                           
           
        typentry->tupDesc_identifier = 0;
      }

                                                                  
      typentry->flags = 0;
    }
    else if (typentry->typtype == TYPTYPE_DOMAIN)
    {
         
                                                                       
                                                                    
                                                                      
                                                             
         
      if (typentry->flags & TCFLAGS_DOMAIN_BASE_IS_COMPOSITE)
      {
        typentry->flags = 0;
      }
    }
  }
}

   
                        
                                     
   
                                                                               
                                                                              
                                                                              
                                                                             
            
   
                                                                           
                                                                                
                                                                             
                                                                            
                                                
   
static void
TypeCacheOpcCallback(Datum arg, int cacheid, uint32 hashvalue)
{
  HASH_SEQ_STATUS status;
  TypeCacheEntry *typentry;

                                                                           
  hash_seq_init(&status, TypeCacheHash);
  while ((typentry = (TypeCacheEntry *)hash_seq_search(&status)) != NULL)
  {
                                                                
    typentry->flags = 0;
  }
}

   
                           
                                     
   
                                                                    
                                                                    
                                  
   
                                                                               
                                                                             
                                                                             
                                                                          
                                   
   
static void
TypeCacheConstrCallback(Datum arg, int cacheid, uint32 hashvalue)
{
  TypeCacheEntry *typentry;

     
                                                                           
                                                                          
                                                                           
                         
     
  for (typentry = firstDomainTypeEntry; typentry != NULL; typentry = typentry->nextDomain)
  {
                                                      
    typentry->flags &= ~TCFLAGS_CHECKED_DOMAIN_CONSTRAINTS;
  }
}

   
                                                                           
   
static inline bool
enum_known_sorted(TypeCacheEnumData *enumdata, Oid arg)
{
  Oid offset;

  if (arg < enumdata->bitmap_base)
  {
    return false;
  }
  offset = arg - enumdata->bitmap_base;
  if (offset > (Oid)INT_MAX)
  {
    return false;
  }
  return bms_is_member((int)offset, enumdata->sorted_values);
}

   
                          
                                         
                                                           
   
                                                                         
                                                                            
                                                                               
                                                                          
                                                          
   
                                                                           
                                                                     
                                                             
   
int
compare_values_of_enum(TypeCacheEntry *tcache, Oid arg1, Oid arg2)
{
  TypeCacheEnumData *enumdata;
  EnumItem *item1;
  EnumItem *item2;

     
                                                                          
                                           
     
  if (arg1 == arg2)
  {
    return 0;
  }

                                               
  if (tcache->enumData == NULL)
  {
    load_enum_cache_data(tcache);
  }
  enumdata = tcache->enumData;

     
                                                                       
     
  if (enum_known_sorted(enumdata, arg1) && enum_known_sorted(enumdata, arg2))
  {
    if (arg1 < arg2)
    {
      return -1;
    }
    else
    {
      return 1;
    }
  }

     
                                                                       
     
  item1 = find_enumitem(enumdata, arg1);
  item2 = find_enumitem(enumdata, arg2);

  if (item1 == NULL || item2 == NULL)
  {
       
                                                                     
                                                                      
                                                                 
       
    load_enum_cache_data(tcache);
    enumdata = tcache->enumData;

    item1 = find_enumitem(enumdata, arg1);
    item2 = find_enumitem(enumdata, arg2);

       
                                                                         
             
       
    if (item1 == NULL)
    {
      elog(ERROR, "enum value %u not found in cache for enum %s", arg1, format_type_be(tcache->type_id));
    }
    if (item2 == NULL)
    {
      elog(ERROR, "enum value %u not found in cache for enum %s", arg2, format_type_be(tcache->type_id));
    }
  }

  if (item1->sort_order < item2->sort_order)
  {
    return -1;
  }
  else if (item1->sort_order > item2->sort_order)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

   
                                                                
   
static void
load_enum_cache_data(TypeCacheEntry *tcache)
{
  TypeCacheEnumData *enumdata;
  Relation enum_rel;
  SysScanDesc enum_scan;
  HeapTuple enum_tuple;
  ScanKeyData skey;
  EnumItem *items;
  int numitems;
  int maxitems;
  Oid bitmap_base;
  Bitmapset *bitmap;
  MemoryContext oldcxt;
  int bm_size, start_pos;

                                           
  if (tcache->typtype != TYPTYPE_ENUM)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("%s is not an enum", format_type_be(tcache->type_id))));
  }

     
                                                                            
                                                                             
                                                                         
                                                                             
              
     
  maxitems = 64;
  items = (EnumItem *)palloc(sizeof(EnumItem) * maxitems);
  numitems = 0;

                                                             
  ScanKeyInit(&skey, Anum_pg_enum_enumtypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(tcache->type_id));

  enum_rel = table_open(EnumRelationId, AccessShareLock);
  enum_scan = systable_beginscan(enum_rel, EnumTypIdLabelIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(enum_tuple = systable_getnext(enum_scan)))
  {
    Form_pg_enum en = (Form_pg_enum)GETSTRUCT(enum_tuple);

    if (numitems >= maxitems)
    {
      maxitems *= 2;
      items = (EnumItem *)repalloc(items, sizeof(EnumItem) * maxitems);
    }
    items[numitems].enum_oid = en->oid;
    items[numitems].sort_order = en->enumsortorder;
    numitems++;
  }

  systable_endscan(enum_scan);
  table_close(enum_rel, AccessShareLock);

                                     
  qsort(items, numitems, sizeof(EnumItem), enum_oid_cmp);

     
                                                                           
                                                                             
     
                                                                         
                                                                             
                                                           
     
                                                                          
                                                                           
                                 
     
  bitmap_base = InvalidOid;
  bitmap = NULL;
  bm_size = 1;                                        

  for (start_pos = 0; start_pos < numitems - 1; start_pos++)
  {
       
                                                                 
       
    Bitmapset *this_bitmap = bms_make_singleton(0);
    int this_bm_size = 1;
    Oid start_oid = items[start_pos].enum_oid;
    float4 prev_order = items[start_pos].sort_order;
    int i;

    for (i = start_pos + 1; i < numitems; i++)
    {
      Oid offset;

      offset = items[i].enum_oid - start_oid;
                                                                  
      if (offset >= 8192)
      {
        break;
      }
                                             
      if (items[i].sort_order > prev_order)
      {
        prev_order = items[i].sort_order;
        this_bitmap = bms_add_member(this_bitmap, (int)offset);
        this_bm_size++;
      }
    }

                                                  
    if (this_bm_size > bm_size)
    {
      bms_free(bitmap);
      bitmap_base = start_oid;
      bitmap = this_bitmap;
      bm_size = this_bm_size;
    }
    else
    {
      bms_free(this_bitmap);
    }

       
                                                                          
                                                                 
                                                                           
                                          
       
    if (bm_size >= (numitems - start_pos - 1))
    {
      break;
    }
  }

                                                 
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
  enumdata = (TypeCacheEnumData *)palloc(offsetof(TypeCacheEnumData, enum_values) + numitems * sizeof(EnumItem));
  enumdata->bitmap_base = bitmap_base;
  enumdata->sorted_values = bms_copy(bitmap);
  enumdata->num_values = numitems;
  memcpy(enumdata->enum_values, items, numitems * sizeof(EnumItem));
  MemoryContextSwitchTo(oldcxt);

  pfree(items);
  bms_free(bitmap);

                                                            
  if (tcache->enumData != NULL)
  {
    pfree(tcache->enumData);
  }
  tcache->enumData = enumdata;
}

   
                                                      
   
static EnumItem *
find_enumitem(TypeCacheEnumData *enumdata, Oid arg)
{
  EnumItem srch;

                                                                     
  if (enumdata->num_values <= 0)
  {
    return NULL;
  }

  srch.enum_oid = arg;
  return bsearch(&srch, enumdata->enum_values, enumdata->num_values, sizeof(EnumItem), enum_oid_cmp);
}

   
                                                       
   
static int
enum_oid_cmp(const void *left, const void *right)
{
  const EnumItem *l = (const EnumItem *)left;
  const EnumItem *r = (const EnumItem *)right;

  if (l->enum_oid < r->enum_oid)
  {
    return -1;
  }
  else if (l->enum_oid > r->enum_oid)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

   
                                                                               
                                                
   
static dsa_pointer
share_tupledesc(dsa_area *area, TupleDesc tupdesc, uint32 typmod)
{
  dsa_pointer shared_dp;
  TupleDesc shared;

  shared_dp = dsa_allocate(area, TupleDescSize(tupdesc));
  shared = (TupleDesc)dsa_get_address(area, shared_dp);
  TupleDescCopy(shared, tupdesc);
  shared->tdtypmod = typmod;

  return shared_dp;
}

   
                                                                         
                                                                             
                                                                              
                                                                               
                    
   
static TupleDesc
find_or_make_matching_shared_tupledesc(TupleDesc tupdesc)
{
  TupleDesc result;
  SharedRecordTableKey key;
  SharedRecordTableEntry *record_table_entry;
  SharedTypmodTableEntry *typmod_table_entry;
  dsa_pointer shared_dp;
  bool found;
  uint32 typmod;

                                            
  if (CurrentSession->shared_typmod_registry == NULL)
  {
    return NULL;
  }

                                                                    
  key.shared = false;
  key.u.local_tupdesc = tupdesc;
  record_table_entry = (SharedRecordTableEntry *)dshash_find(CurrentSession->shared_record_table, &key, false);
  if (record_table_entry)
  {
    Assert(record_table_entry->key.shared);
    dshash_release_lock(CurrentSession->shared_record_table, record_table_entry);
    result = (TupleDesc)dsa_get_address(CurrentSession->area, record_table_entry->key.u.shared_tupdesc);
    Assert(result->tdrefcount == -1);

    return result;
  }

                                                                           
  typmod = (int)pg_atomic_fetch_add_u32(&CurrentSession->shared_typmod_registry->next_typmod, 1);

                                              
  shared_dp = share_tupledesc(CurrentSession->area, tupdesc, typmod);

     
                                                                             
                    
     
  PG_TRY();
  {
    typmod_table_entry = (SharedTypmodTableEntry *)dshash_find_or_insert(CurrentSession->shared_typmod_table, &typmod, &found);
    if (found)
    {
      elog(ERROR, "cannot create duplicate shared record typmod");
    }
  }
  PG_CATCH();
  {
    dsa_free(CurrentSession->area, shared_dp);
    PG_RE_THROW();
  }
  PG_END_TRY();
  typmod_table_entry->typmod = typmod;
  typmod_table_entry->shared_tupdesc = shared_dp;
  dshash_release_lock(CurrentSession->shared_typmod_table, typmod_table_entry);

     
                                                                         
                                             
     
  record_table_entry = (SharedRecordTableEntry *)dshash_find_or_insert(CurrentSession->shared_record_table, &key, &found);
  if (found)
  {
       
                                                                           
                                                     
       
    dshash_release_lock(CurrentSession->shared_record_table, record_table_entry);

                                                                     
    found = dshash_delete_key(CurrentSession->shared_typmod_table, &typmod);
    Assert(found);
    dsa_free(CurrentSession->area, shared_dp);

                                  
    Assert(record_table_entry->key.shared);
    result = (TupleDesc)dsa_get_address(CurrentSession->area, record_table_entry->key.u.shared_tupdesc);
    Assert(result->tdrefcount == -1);

    return result;
  }

                               
  record_table_entry->key.shared = true;
  record_table_entry->key.u.shared_tupdesc = shared_dp;
  dshash_release_lock(CurrentSession->shared_record_table, record_table_entry);
  result = (TupleDesc)dsa_get_address(CurrentSession->area, shared_dp);
  Assert(result->tdrefcount == -1);

  return result;
}

   
                                                                       
                                                                       
   
static void
shared_record_typmod_registry_detach(dsm_segment *segment, Datum datum)
{
                                                              
  if (CurrentSession->shared_record_table != NULL)
  {
    dshash_detach(CurrentSession->shared_record_table);
    CurrentSession->shared_record_table = NULL;
  }
  if (CurrentSession->shared_typmod_table != NULL)
  {
    dshash_detach(CurrentSession->shared_typmod_table);
    CurrentSession->shared_typmod_table = NULL;
  }
  CurrentSession->shared_typmod_registry = NULL;
}
