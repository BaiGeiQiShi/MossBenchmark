                                                                            
   
              
                                             
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
   
                      
                                                               
                                                                      
                                                                 
                                                           
                                               
   
         
                                                                    
                
   
#include "postgres.h"

#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/tupdesc_details.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/partition.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_database.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_partitioned_table.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_shseclabel.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "catalog/schemapg.h"
#include "catalog/storage.h"
#include "commands/policy.h"
#include "commands/trigger.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rowsecurity.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/relmapper.h"
#include "utils/resowner_private.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

#define RELCACHE_INIT_FILEMAGIC 0x573266                       

   
                                                                      
                                                                        
                                                                            
   
#ifndef RECOVER_RELATION_BUILD_MEMORY
#if defined(CLOBBER_CACHE_ALWAYS) || defined(CLOBBER_CACHE_RECURSIVELY)
#define RECOVER_RELATION_BUILD_MEMORY 1
#else
#define RECOVER_RELATION_BUILD_MEMORY 0
#endif
#endif

   
                                                                 
   
static const FormData_pg_attribute Desc_pg_class[Natts_pg_class] = {Schema_pg_class};
static const FormData_pg_attribute Desc_pg_attribute[Natts_pg_attribute] = {Schema_pg_attribute};
static const FormData_pg_attribute Desc_pg_proc[Natts_pg_proc] = {Schema_pg_proc};
static const FormData_pg_attribute Desc_pg_type[Natts_pg_type] = {Schema_pg_type};
static const FormData_pg_attribute Desc_pg_database[Natts_pg_database] = {Schema_pg_database};
static const FormData_pg_attribute Desc_pg_authid[Natts_pg_authid] = {Schema_pg_authid};
static const FormData_pg_attribute Desc_pg_auth_members[Natts_pg_auth_members] = {Schema_pg_auth_members};
static const FormData_pg_attribute Desc_pg_index[Natts_pg_index] = {Schema_pg_index};
static const FormData_pg_attribute Desc_pg_shseclabel[Natts_pg_shseclabel] = {Schema_pg_shseclabel};
static const FormData_pg_attribute Desc_pg_subscription[Natts_pg_subscription] = {Schema_pg_subscription};

   
                                              
   
                                                                   
                             
   
typedef struct relidcacheent
{
  Oid reloid;
  Relation reldesc;
} RelIdCacheEnt;

static HTAB *RelationIdCache;

   
                                                                           
                                                                             
   
bool criticalRelcachesBuilt = false;

   
                                                                           
                                                                
   
bool criticalSharedRelcachesBuilt = false;

   
                                                                            
                                                                              
                                                                            
                              
   
static long relcacheInvalsReceived = 0L;

   
                                                                             
                                                                            
                                                                              
                                                                               
                                                                            
                   
   
typedef struct inprogressent
{
  Oid reloid;                                        
  bool invalidated;                                             
} InProgressEnt;

static InProgressEnt *in_progress_list;
static int in_progress_list_len;
static int in_progress_list_maxlen;

   
                                                                         
                                                                             
                                                                              
                                                                               
                                                                          
                                                                             
                                                                             
                                          
   
#define MAX_EOXACT_LIST 32
static Oid eoxact_list[MAX_EOXACT_LIST];
static int eoxact_list_len = 0;
static bool eoxact_list_overflowed = false;

#define EOXactListAdd(rel)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (eoxact_list_len < MAX_EOXACT_LIST)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      eoxact_list[eoxact_list_len++] = (rel)->rd_id;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      eoxact_list_overflowed = true;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

   
                                                                     
                                                                             
                                                              
   
static TupleDesc *EOXactTupleDescArray;
static int NextEOXactTupleDescNum = 0;
static int EOXactTupleDescArrayLen = 0;

   
                                              
   
#define RelationCacheInsert(RELATION, replace_allowed)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    RelIdCacheEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    bool found;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    hentry = (RelIdCacheEnt *)hash_search(RelationIdCache, (void *)&((RELATION)->rd_id), HASH_ENTER, &found);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    if (found)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      Relation _old_rel = hentry->reldesc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      Assert(replace_allowed);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      hentry->reldesc = (RELATION);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      if (RelationHasReferenceCountZero(_old_rel))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
        RelationDestroyRelation(_old_rel, false);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      else if (!IsBootstrapProcessingMode())                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
        elog(WARNING, "leaking still-referenced relcache entry for \"%s\"", RelationGetRelationName(_old_rel));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      hentry->reldesc = (RELATION);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  } while (0)

#define RelationIdCacheLookup(ID, RELATION)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    RelIdCacheEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    hentry = (RelIdCacheEnt *)hash_search(RelationIdCache, (void *)&(ID), HASH_FIND, NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    if (hentry)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      RELATION = hentry->reldesc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      RELATION = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

#define RelationCacheDelete(RELATION)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    RelIdCacheEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    hentry = (RelIdCacheEnt *)hash_search(RelationIdCache, (void *)&((RELATION)->rd_id), HASH_REMOVE, NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (hentry == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      elog(WARNING, "failed to delete relcache entry for OID %u", (RELATION)->rd_id);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

   
                                                 
   
                                                               
                                     
   
typedef struct opclasscacheent
{
  Oid opclassoid;                                             
  bool valid;                                                        
  StrategyNumber numSupport;                                           
  Oid opcfamily;                                           
  Oid opcintype;                                                        
  RegProcedure *supportProcs;                                 
} OpClassCacheEnt;

static HTAB *OpClassCache = NULL;

                                    

static void
RelationDestroyRelation(Relation relation, bool remember_tupdesc);
static void
RelationClearRelation(Relation relation, bool rebuild);

static void
RelationReloadIndexInfo(Relation relation);
static void
RelationReloadNailed(Relation relation);
static void
RelationFlushRelation(Relation relation);
static void
RememberToFreeTupleDescAtEOX(TupleDesc td);
static void
AtEOXact_cleanup(Relation relation, bool isCommit);
static void
AtEOSubXact_cleanup(Relation relation, bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid);
static bool
load_relcache_init_file(bool shared);
static void
write_relcache_init_file(bool shared);
static void
write_item(const void *data, Size len, FILE *fp);

static void
formrdesc(const char *relationName, Oid relationReltype, bool isshared, int natts, const FormData_pg_attribute *attrs);

static HeapTuple
ScanPgRelation(Oid targetRelId, bool indexOK, bool force_non_historic);
static Relation
AllocateRelationDesc(Form_pg_class relp);
static void
RelationParseRelOptions(Relation relation, HeapTuple tuple);
static void
RelationBuildTupleDesc(Relation relation);
static Relation
RelationBuildDesc(Oid targetRelId, bool insertIt);
static void
RelationInitPhysicalAddr(Relation relation);
static void
load_critical_index(Oid indexoid, Oid heapoid);
static TupleDesc
GetPgClassDescriptor(void);
static TupleDesc
GetPgIndexDescriptor(void);
static void
AttrDefaultFetch(Relation relation);
static void
CheckConstraintFetch(Relation relation);
static int
CheckConstraintCmp(const void *a, const void *b);
static List *
insert_ordered_oid(List *list, Oid datum);
static void
InitIndexAmRoutine(Relation relation);
static void
IndexSupportInitialize(oidvector *indclass, RegProcedure *indexSupport, Oid *opFamily, Oid *opcInType, StrategyNumber maxSupportNumber, AttrNumber maxAttributeNumber);
static OpClassCacheEnt *
LookupOpclassInfo(Oid operatorClassOid, StrategyNumber numSupport);
static void
RelationCacheInitFileRemoveInDir(const char *tblspcpath);
static void
unlink_initfile(const char *initfilename, int elevel);

   
                   
   
                                                         
                                                               
                                                                     
                                                                    
                                                                 
                                                                     
                                                    
   
                                                                 
                                                      
   
static HeapTuple
ScanPgRelation(Oid targetRelId, bool indexOK, bool force_non_historic)
{
  HeapTuple pg_class_tuple;
  Relation pg_class_desc;
  SysScanDesc pg_class_scan;
  ScanKeyData key[1];
  Snapshot snapshot = NULL;

     
                                                                             
                                                                           
                                                                            
                                                                       
     
  if (!OidIsValid(MyDatabaseId))
  {
    elog(FATAL, "cannot read pg_class without having selected a database");
  }

     
                     
     
  ScanKeyInit(&key[0], Anum_pg_class_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(targetRelId));

     
                                                                         
                                                                           
                                                                         
                                       
     
  pg_class_desc = table_open(RelationRelationId, AccessShareLock);

     
                                                                          
                                                                            
                                                                      
                                                                            
                                    
     
  if (force_non_historic)
  {
    snapshot = GetNonHistoricCatalogSnapshot(RelationRelationId);
  }

  pg_class_scan = systable_beginscan(pg_class_desc, ClassOidIndexId, indexOK && criticalRelcachesBuilt, snapshot, 1, key);

  pg_class_tuple = systable_getnext(pg_class_scan);

     
                                              
     
  if (HeapTupleIsValid(pg_class_tuple))
  {
    pg_class_tuple = heap_copytuple(pg_class_tuple);
  }

                
  systable_endscan(pg_class_scan);
  table_close(pg_class_desc, AccessShareLock);

  return pg_class_tuple;
}

   
                         
   
                                                                  
                                                                   
   
static Relation
AllocateRelationDesc(Form_pg_class relp)
{
  Relation relation;
  MemoryContext oldcxt;
  Form_pg_class relationForm;

                                                        
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

     
                                                         
     
  relation = (Relation)palloc0(sizeof(RelationData));

                                                               
  relation->rd_smgr = NULL;

     
                                  
     
                                                                            
                                                                       
                                                                         
                                                                           
                                                                             
                                                                          
                                                                            
                     
     
  relationForm = (Form_pg_class)palloc(CLASS_TUPLE_SIZE);

  memcpy(relationForm, relp, CLASS_TUPLE_SIZE);

                                      
  relation->rd_rel = relationForm;

                                                 
  relation->rd_att = CreateTemplateTupleDesc(relationForm->relnatts);
                                                    
  relation->rd_att->tdrefcount = 1;

  MemoryContextSwitchTo(oldcxt);

  return relation;
}

   
                           
                                                           
   
                                                               
   
                                                                 
   
static void
RelationParseRelOptions(Relation relation, HeapTuple tuple)
{
  bytea *options;
  amoptions_function amoptsfn;

  relation->rd_options = NULL;

     
                                                                            
                   
     
  switch (relation->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_TOASTVALUE:
  case RELKIND_VIEW:
  case RELKIND_MATVIEW:
  case RELKIND_PARTITIONED_TABLE:
    amoptsfn = NULL;
    break;
  case RELKIND_INDEX:
  case RELKIND_PARTITIONED_INDEX:
    amoptsfn = relation->rd_indam->amoptions;
    break;
  default:
    return;
  }

     
                                                                             
                                                                           
                               
     
  options = extractRelOptions(tuple, GetPgClassDescriptor(), amoptsfn);

     
                                                                     
                                                                           
                                                                      
                                        
     
  if (options)
  {
    relation->rd_options = MemoryContextAlloc(CacheMemoryContext, VARSIZE(options));
    memcpy(relation->rd_options, options, VARSIZE(options));
    pfree(options);
  }
}

   
                           
   
                                                             
                                                                  
   
static void
RelationBuildTupleDesc(Relation relation)
{
  HeapTuple pg_attribute_tuple;
  Relation pg_attribute_desc;
  SysScanDesc pg_attribute_scan;
  ScanKeyData skey[2];
  int need;
  TupleConstr *constr;
  AttrDefault *attrdef = NULL;
  AttrMissing *attrmiss = NULL;
  int ndef = 0;

                                                    
  relation->rd_att->tdtypeid = relation->rd_rel->reltype;
  relation->rd_att->tdtypmod = -1;                          

  constr = (TupleConstr *)MemoryContextAlloc(CacheMemoryContext, sizeof(TupleConstr));
  constr->has_not_null = false;
  constr->has_generated_stored = false;

     
                                                                     
                                                                          
                          
     
  ScanKeyInit(&skey[0], Anum_pg_attribute_attrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));
  ScanKeyInit(&skey[1], Anum_pg_attribute_attnum, BTGreaterStrategyNumber, F_INT2GT, Int16GetDatum(0));

     
                                                                            
                                                                           
                                       
     
  pg_attribute_desc = table_open(AttributeRelationId, AccessShareLock);
  pg_attribute_scan = systable_beginscan(pg_attribute_desc, AttributeRelidNumIndexId, criticalRelcachesBuilt, NULL, 2, skey);

     
                                            
     
  need = RelationGetNumberOfAttributes(relation);

  while (HeapTupleIsValid(pg_attribute_tuple = systable_getnext(pg_attribute_scan)))
  {
    Form_pg_attribute attp;
    int attnum;
    bool atthasmissing;

    attp = (Form_pg_attribute)GETSTRUCT(pg_attribute_tuple);

    attnum = attp->attnum;
    if (attnum <= 0 || attnum > RelationGetNumberOfAttributes(relation))
    {
      elog(ERROR, "invalid attribute number %d for %s", attp->attnum, RelationGetRelationName(relation));
    }

    memcpy(TupleDescAttr(relation->rd_att, attnum - 1), attp, ATTRIBUTE_FIXED_PART_SIZE);

       
                                                                   
                                                                           
                                                                          
                    
       
    atthasmissing = attp->atthasmissing;
    if (relation->rd_rel->relkind != RELKIND_RELATION && atthasmissing)
    {
      Form_pg_attribute nattp;

      atthasmissing = false;
      nattp = TupleDescAttr(relation->rd_att, attnum - 1);
      nattp->atthasmissing = false;
    }

                                        
    if (attp->attnotnull)
    {
      constr->has_not_null = true;
    }
    if (attp->attgenerated == ATTRIBUTE_GENERATED_STORED)
    {
      constr->has_generated_stored = true;
    }

                                                                     
    if (attp->atthasdef)
    {
      if (attrdef == NULL)
      {
        attrdef = (AttrDefault *)MemoryContextAllocZero(CacheMemoryContext, RelationGetNumberOfAttributes(relation) * sizeof(AttrDefault));
      }
      attrdef[ndef].adnum = attnum;
      attrdef[ndef].adbin = NULL;

      ndef++;
    }

                                      
    if (atthasmissing)
    {
      Datum missingval;
      bool missingNull;

                                       
      missingval = heap_getattr(pg_attribute_tuple, Anum_pg_attribute_attmissingval, pg_attribute_desc->rd_att, &missingNull);
      if (!missingNull)
      {
                                       
        MemoryContext oldcxt;
        bool is_null;
        int one = 1;
        Datum missval;

        if (attrmiss == NULL)
        {
          attrmiss = (AttrMissing *)MemoryContextAllocZero(CacheMemoryContext, relation->rd_rel->relnatts * sizeof(AttrMissing));
        }

        missval = array_get_element(missingval, 1, &one, -1, attp->attlen, attp->attbyval, attp->attalign, &is_null);
        Assert(!is_null);
        if (attp->attbyval)
        {
                                                          
          attrmiss[attnum - 1].am_value = missval;
        }
        else
        {
                                                     
          oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
          attrmiss[attnum - 1].am_value = datumCopy(missval, attp->attbyval, attp->attlen);
          MemoryContextSwitchTo(oldcxt);
        }
        attrmiss[attnum - 1].am_present = true;
      }
    }
    need--;
    if (need == 0)
    {
      break;
    }
  }

     
                                                   
     
  systable_endscan(pg_attribute_scan);
  table_close(pg_attribute_desc, AccessShareLock);

  if (need != 0)
  {
    elog(ERROR, "catalog is missing %d attribute(s) for relid %u", need, RelationGetRelid(relation));
  }

     
                                                                       
                                                                       
                                                      
     
#ifdef USE_ASSERT_CHECKING
  {
    int i;

    for (i = 0; i < RelationGetNumberOfAttributes(relation); i++)
    {
      Assert(TupleDescAttr(relation->rd_att, i)->attcacheoff == -1);
    }
  }
#endif

     
                                                                    
                                                                             
                                                                           
     
  if (RelationGetNumberOfAttributes(relation) > 0)
  {
    TupleDescAttr(relation->rd_att, 0)->attcacheoff = 0;
  }

     
                                    
     
  if (constr->has_not_null || ndef > 0 || attrmiss || relation->rd_rel->relchecks)
  {
    relation->rd_att->constr = constr;

    if (ndef > 0)               
    {
      if (ndef < RelationGetNumberOfAttributes(relation))
      {
        constr->defval = (AttrDefault *)repalloc(attrdef, ndef * sizeof(AttrDefault));
      }
      else
      {
        constr->defval = attrdef;
      }
      constr->num_defval = ndef;
      AttrDefaultFetch(relation);
    }
    else
    {
      constr->num_defval = 0;
    }

    constr->missing = attrmiss;

    if (relation->rd_rel->relchecks > 0)             
    {
      constr->num_check = relation->rd_rel->relchecks;
      constr->check = (ConstrCheck *)MemoryContextAllocZero(CacheMemoryContext, constr->num_check * sizeof(ConstrCheck));
      CheckConstraintFetch(relation);
    }
    else
    {
      constr->num_check = 0;
    }
  }
  else
  {
    pfree(constr);
    relation->rd_att->constr = NULL;
  }
}

   
                          
   
                                                          
                                   
   
                                                                           
                                                                        
                                                                         
                                                                       
                                                                         
                                                                         
                                                                       
                                          
   
static void
RelationBuildRuleLock(Relation relation)
{
  MemoryContext rulescxt;
  MemoryContext oldcxt;
  HeapTuple rewrite_tuple;
  Relation rewrite_desc;
  TupleDesc rewrite_tupdesc;
  SysScanDesc rewrite_scan;
  ScanKeyData key;
  RuleLock *rulelock;
  int numlocks;
  RewriteRule **rules;
  int maxlocks;

     
                                                                    
     
  rulescxt = AllocSetContextCreate(CacheMemoryContext, "relation rules", ALLOCSET_SMALL_SIZES);
  relation->rd_rulescxt = rulescxt;
  MemoryContextCopyAndSetIdentifier(rulescxt, RelationGetRelationName(relation));

     
                                                                           
                
     
  maxlocks = 4;
  rules = (RewriteRule **)MemoryContextAlloc(rulescxt, sizeof(RewriteRule *) * maxlocks);
  numlocks = 0;

     
                     
     
  ScanKeyInit(&key, Anum_pg_rewrite_ev_class, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

     
                                      
     
                                                                            
                                                                
                                                                           
                                                     
     
  rewrite_desc = table_open(RewriteRelationId, AccessShareLock);
  rewrite_tupdesc = RelationGetDescr(rewrite_desc);
  rewrite_scan = systable_beginscan(rewrite_desc, RewriteRelRulenameIndexId, true, NULL, 1, &key);

  while (HeapTupleIsValid(rewrite_tuple = systable_getnext(rewrite_scan)))
  {
    Form_pg_rewrite rewrite_form = (Form_pg_rewrite)GETSTRUCT(rewrite_tuple);
    bool isnull;
    Datum rule_datum;
    char *rule_str;
    RewriteRule *rule;

    rule = (RewriteRule *)MemoryContextAlloc(rulescxt, sizeof(RewriteRule));

    rule->ruleId = rewrite_form->oid;

    rule->event = rewrite_form->ev_type - '0';
    rule->enabled = rewrite_form->ev_enabled;
    rule->isInstead = rewrite_form->is_instead;

       
                                                                        
                                                                    
                                                                         
                                          
       
    rule_datum = heap_getattr(rewrite_tuple, Anum_pg_rewrite_ev_action, rewrite_tupdesc, &isnull);
    Assert(!isnull);
    rule_str = TextDatumGetCString(rule_datum);
    oldcxt = MemoryContextSwitchTo(rulescxt);
    rule->actions = (List *)stringToNode(rule_str);
    MemoryContextSwitchTo(oldcxt);
    pfree(rule_str);

    rule_datum = heap_getattr(rewrite_tuple, Anum_pg_rewrite_ev_qual, rewrite_tupdesc, &isnull);
    Assert(!isnull);
    rule_str = TextDatumGetCString(rule_datum);
    oldcxt = MemoryContextSwitchTo(rulescxt);
    rule->qual = (Node *)stringToNode(rule_str);
    MemoryContextSwitchTo(oldcxt);
    pfree(rule_str);

       
                                                                          
                                                                        
                                                                       
                                                                        
                          
       
                                                                           
                                                                       
                                                                          
                                                                  
                                                              
       
    setRuleCheckAsUser((Node *)rule->actions, relation->rd_rel->relowner);
    setRuleCheckAsUser(rule->qual, relation->rd_rel->relowner);

    if (numlocks >= maxlocks)
    {
      maxlocks *= 2;
      rules = (RewriteRule **)repalloc(rules, sizeof(RewriteRule *) * maxlocks);
    }
    rules[numlocks++] = rule;
  }

     
                                                   
     
  systable_endscan(rewrite_scan);
  table_close(rewrite_desc, AccessShareLock);

     
                                                                  
     
  if (numlocks == 0)
  {
    relation->rd_rules = NULL;
    relation->rd_rulescxt = NULL;
    MemoryContextDelete(rulescxt);
    return;
  }

     
                                              
     
  rulelock = (RuleLock *)MemoryContextAlloc(rulescxt, sizeof(RuleLock));
  rulelock->numLocks = numlocks;
  rulelock->rules = rules;

  relation->rd_rules = rulelock;
}

   
                   
   
                                                   
   
                                                           
   
static bool
equalRuleLocks(RuleLock *rlock1, RuleLock *rlock2)
{
  int i;

     
                                                                  
                                                                           
                                  
     
  if (rlock1 != NULL)
  {
    if (rlock2 == NULL)
    {
      return false;
    }
    if (rlock1->numLocks != rlock2->numLocks)
    {
      return false;
    }
    for (i = 0; i < rlock1->numLocks; i++)
    {
      RewriteRule *rule1 = rlock1->rules[i];
      RewriteRule *rule2 = rlock2->rules[i];

      if (rule1->ruleId != rule2->ruleId)
      {
        return false;
      }
      if (rule1->event != rule2->event)
      {
        return false;
      }
      if (rule1->enabled != rule2->enabled)
      {
        return false;
      }
      if (rule1->isInstead != rule2->isInstead)
      {
        return false;
      }
      if (!equal(rule1->qual, rule2->qual))
      {
        return false;
      }
      if (!equal(rule1->actions, rule2->actions))
      {
        return false;
      }
    }
  }
  else if (rlock2 != NULL)
  {
    return false;
  }
  return true;
}

   
                
   
                                                  
   
static bool
equalPolicy(RowSecurityPolicy *policy1, RowSecurityPolicy *policy2)
{
  int i;
  Oid *r1, *r2;

  if (policy1 != NULL)
  {
    if (policy2 == NULL)
    {
      return false;
    }

    if (policy1->polcmd != policy2->polcmd)
    {
      return false;
    }
    if (policy1->hassublinks != policy2->hassublinks)
    {
      return false;
    }
    if (strcmp(policy1->policy_name, policy2->policy_name) != 0)
    {
      return false;
    }
    if (ARR_DIMS(policy1->roles)[0] != ARR_DIMS(policy2->roles)[0])
    {
      return false;
    }

    r1 = (Oid *)ARR_DATA_PTR(policy1->roles);
    r2 = (Oid *)ARR_DATA_PTR(policy2->roles);

    for (i = 0; i < ARR_DIMS(policy1->roles)[0]; i++)
    {
      if (r1[i] != r2[i])
      {
        return false;
      }
    }

    if (!equal(policy1->qual, policy2->qual))
    {
      return false;
    }
    if (!equal(policy1->with_check_qual, policy2->with_check_qual))
    {
      return false;
    }
  }
  else if (policy2 != NULL)
  {
    return false;
  }

  return true;
}

   
                
   
                                                           
   
static bool
equalRSDesc(RowSecurityDesc *rsdesc1, RowSecurityDesc *rsdesc2)
{
  ListCell *lc, *rc;

  if (rsdesc1 == NULL && rsdesc2 == NULL)
  {
    return true;
  }

  if ((rsdesc1 != NULL && rsdesc2 == NULL) || (rsdesc1 == NULL && rsdesc2 != NULL))
  {
    return false;
  }

  if (list_length(rsdesc1->policies) != list_length(rsdesc2->policies))
  {
    return false;
  }

                                                               
  forboth(lc, rsdesc1->policies, rc, rsdesc2->policies)
  {
    RowSecurityPolicy *l = (RowSecurityPolicy *)lfirst(lc);
    RowSecurityPolicy *r = (RowSecurityPolicy *)lfirst(rc);

    if (!equalPolicy(l, r))
    {
      return false;
    }
  }

  return true;
}

   
                      
   
                                                                
                                         
   
                                                                            
   
                                                                       
                                                                  
                                          
   
static Relation
RelationBuildDesc(Oid targetRelId, bool insertIt)
{
  int in_progress_offset;
  Relation relation;
  Oid relid;
  HeapTuple pg_class_tuple;
  Form_pg_class relp;

     
                                                                             
                                                                         
                                                                          
                                                                             
                                                                             
                                                                          
                                                                            
                                                                          
                                                                            
                                                                        
     
#if RECOVER_RELATION_BUILD_MEMORY
  MemoryContext tmpcxt;
  MemoryContext oldcxt;

  tmpcxt = AllocSetContextCreate(CurrentMemoryContext, "RelationBuildDesc workspace", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(tmpcxt);
#endif

                                               
  if (in_progress_list_len >= in_progress_list_maxlen)
  {
    int allocsize;

    allocsize = in_progress_list_maxlen * 2;
    in_progress_list = repalloc(in_progress_list, allocsize * sizeof(*in_progress_list));
    in_progress_list_maxlen = allocsize;
  }
  in_progress_offset = in_progress_list_len++;
  in_progress_list[in_progress_offset].reloid = targetRelId;
retry:
  in_progress_list[in_progress_offset].invalidated = false;

     
                                                                       
     
  pg_class_tuple = ScanPgRelation(targetRelId, true, false);

     
                                          
     
  if (!HeapTupleIsValid(pg_class_tuple))
  {
#if RECOVER_RELATION_BUILD_MEMORY
                                                                         
    MemoryContextSwitchTo(oldcxt);
    MemoryContextDelete(tmpcxt);
#endif
    Assert(in_progress_offset + 1 == in_progress_list_len);
    in_progress_list_len--;
    return NULL;
  }

     
                                             
     
  relp = (Form_pg_class)GETSTRUCT(pg_class_tuple);
  relid = relp->oid;
  Assert(relid == targetRelId);

     
                                                                           
                          
     
  relation = AllocateRelationDesc(relp);

     
                                                             
     
  RelationGetRelid(relation) = relid;

     
                                                                            
                                                                            
                                                                           
     
  relation->rd_refcnt = 0;
  relation->rd_isnailed = false;
  relation->rd_createSubid = InvalidSubTransactionId;
  relation->rd_newRelfilenodeSubid = InvalidSubTransactionId;
  switch (relation->rd_rel->relpersistence)
  {
  case RELPERSISTENCE_UNLOGGED:
  case RELPERSISTENCE_PERMANENT:
    relation->rd_backend = InvalidBackendId;
    relation->rd_islocaltemp = false;
    break;
  case RELPERSISTENCE_TEMP:
    if (isTempOrTempToastNamespace(relation->rd_rel->relnamespace))
    {
      relation->rd_backend = BackendIdForTempRelations();
      relation->rd_islocaltemp = true;
    }
    else
    {
         
                                                                   
                                                                   
         
                                                                     
                                                                    
                                                                 
                                                                 
                                                              
                                                                   
                                                                     
                                      
         
      relation->rd_backend = GetTempNamespaceBackendId(relation->rd_rel->relnamespace);
      Assert(relation->rd_backend != InvalidBackendId);
      relation->rd_islocaltemp = false;
    }
    break;
  default:
    elog(ERROR, "invalid relpersistence: %c", relation->rd_rel->relpersistence);
    break;
  }

     
                                                         
     
  RelationBuildTupleDesc(relation);

     
                                                        
     
  if (relation->rd_rel->relhasrules)
  {
    RelationBuildRuleLock(relation);
  }
  else
  {
    relation->rd_rules = NULL;
    relation->rd_rulescxt = NULL;
  }

  if (relation->rd_rel->relhastriggers)
  {
    RelationBuildTriggers(relation);
  }
  else
  {
    relation->trigdesc = NULL;
  }

  if (relation->rd_rel->relrowsecurity)
  {
    RelationBuildRowSecurity(relation);
  }
  else
  {
    relation->rd_rsdesc = NULL;
  }

                                                     
  relation->rd_fkeylist = NIL;
  relation->rd_fkeyvalid = false;

                                                                            
  if (relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    RelationBuildPartitionKey(relation);
    RelationBuildPartitionDesc(relation);
  }
  else
  {
    relation->rd_partkey = NULL;
    relation->rd_partkeycxt = NULL;
    relation->rd_partdesc = NULL;
    relation->rd_pdcxt = NULL;
  }
                                                      
  relation->rd_partcheck = NIL;
  relation->rd_partcheckvalid = false;
  relation->rd_partcheckcxt = NULL;

     
                                          
     
  switch (relation->rd_rel->relkind)
  {
  case RELKIND_INDEX:
  case RELKIND_PARTITIONED_INDEX:
    Assert(relation->rd_rel->relam != InvalidOid);
    RelationInitIndexAccessInfo(relation);
    break;
  case RELKIND_RELATION:
  case RELKIND_TOASTVALUE:
  case RELKIND_MATVIEW:
    Assert(relation->rd_rel->relam != InvalidOid);
    RelationInitTableAccessMethod(relation);
    break;
  case RELKIND_SEQUENCE:
    Assert(relation->rd_rel->relam == InvalidOid);
    RelationInitTableAccessMethod(relation);
    break;
  case RELKIND_VIEW:
  case RELKIND_COMPOSITE_TYPE:
  case RELKIND_FOREIGN_TABLE:
  case RELKIND_PARTITIONED_TABLE:
    Assert(relation->rd_rel->relam == InvalidOid);
    break;
  }

                                 
  RelationParseRelOptions(relation, pg_class_tuple);

     
                                                      
     
  RelationInitLockInfo(relation);                 

     
                                                                 
     
  RelationInitPhysicalAddr(relation);

                                                               
  relation->rd_smgr = NULL;

     
                                                             
     
  heap_freetuple(pg_class_tuple);

     
                                                                             
                                                                             
                                                                            
                                                                           
                                                                             
     
  if (in_progress_list[in_progress_offset].invalidated)
  {
    RelationDestroyRelation(relation, false);
    goto retry;
  }
  Assert(in_progress_offset + 1 == in_progress_list_len);
  in_progress_list_len--;

     
                                                                           
     
                                                                            
                                                                             
                                                                             
                                                                             
                                                                         
                                                                             
                                                                            
                                                              
     
  if (insertIt)
  {
    RelationCacheInsert(relation, true);
  }

                        
  relation->rd_isvalid = true;

#if RECOVER_RELATION_BUILD_MEMORY
                                                                       
  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(tmpcxt);
#endif

  return relation;
}

   
                                                                              
   
                                                                           
                                                                         
                             
   
static void
RelationInitPhysicalAddr(Relation relation)
{
                                                
  if (!RELKIND_HAS_STORAGE(relation->rd_rel->relkind))
  {
    return;
  }

  if (relation->rd_rel->reltablespace)
  {
    relation->rd_node.spcNode = relation->rd_rel->reltablespace;
  }
  else
  {
    relation->rd_node.spcNode = MyDatabaseTableSpace;
  }
  if (relation->rd_node.spcNode == GLOBALTABLESPACE_OID)
  {
    relation->rd_node.dbNode = InvalidOid;
  }
  else
  {
    relation->rd_node.dbNode = MyDatabaseId;
  }

  if (relation->rd_rel->relfilenode)
  {
       
                                                                           
                                                                      
                                                                        
                                                                         
                                                                    
                                                                          
                                                                         
                                              
       
    if (HistoricSnapshotActive() && RelationIsAccessibleInLogicalDecoding(relation) && IsTransactionState())
    {
      HeapTuple phys_tuple;
      Form_pg_class physrel;

      phys_tuple = ScanPgRelation(RelationGetRelid(relation), RelationGetRelid(relation) != ClassOidIndexId, true);
      if (!HeapTupleIsValid(phys_tuple))
      {
        elog(ERROR, "could not find pg_class entry for %u", RelationGetRelid(relation));
      }
      physrel = (Form_pg_class)GETSTRUCT(phys_tuple);

      relation->rd_rel->reltablespace = physrel->reltablespace;
      relation->rd_rel->relfilenode = physrel->relfilenode;
      heap_freetuple(phys_tuple);
    }

    relation->rd_node.relNode = relation->rd_rel->relfilenode;
  }
  else
  {
                                     
    relation->rd_node.relNode = RelationMapOidToFilenode(relation->rd_id, relation->rd_rel->relisshared);
    if (!OidIsValid(relation->rd_node.relNode))
    {
      elog(ERROR, "could not find relation mapping for relation \"%s\", OID %u", RelationGetRelationName(relation), relation->rd_id);
    }
  }
}

   
                                                     
   
                                                                  
   
static void
InitIndexAmRoutine(Relation relation)
{
  IndexAmRoutine *cached, *tmp;

     
                                                                             
                                                                   
     
  tmp = GetIndexAmRoutine(relation->rd_amhandler);

                                                              
  cached = (IndexAmRoutine *)MemoryContextAlloc(relation->rd_indexcxt, sizeof(IndexAmRoutine));
  memcpy(cached, tmp, sizeof(IndexAmRoutine));
  relation->rd_indam = cached;

  pfree(tmp);
}

   
                                                                     
   
void
RelationInitIndexAccessInfo(Relation relation)
{
  HeapTuple tuple;
  Form_pg_am aform;
  Datum indcollDatum;
  Datum indclassDatum;
  Datum indoptionDatum;
  bool isnull;
  oidvector *indcoll;
  oidvector *indclass;
  int2vector *indoption;
  MemoryContext indexcxt;
  MemoryContext oldcontext;
  int indnatts;
  int indnkeyatts;
  uint16 amsupport;

     
                                                                      
                                                                           
                                                                      
     
  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(RelationGetRelid(relation)));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for index %u", RelationGetRelid(relation));
  }
  oldcontext = MemoryContextSwitchTo(CacheMemoryContext);
  relation->rd_indextuple = heap_copytuple(tuple);
  relation->rd_index = (Form_pg_index)GETSTRUCT(relation->rd_indextuple);
  MemoryContextSwitchTo(oldcontext);
  ReleaseSysCache(tuple);

     
                                                                             
     
  tuple = SearchSysCache1(AMOID, ObjectIdGetDatum(relation->rd_rel->relam));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for access method %u", relation->rd_rel->relam);
  }
  aform = (Form_pg_am)GETSTRUCT(tuple);
  relation->rd_amhandler = aform->amhandler;
  ReleaseSysCache(tuple);

  indnatts = RelationGetNumberOfAttributes(relation);
  if (indnatts != IndexRelationGetNumberOfAttributes(relation))
  {
    elog(ERROR, "relnatts disagrees with indnatts for index %u", RelationGetRelid(relation));
  }
  indnkeyatts = IndexRelationGetNumberOfKeyAttributes(relation);

     
                                                                             
                                                                           
                                                          
     
  indexcxt = AllocSetContextCreate(CacheMemoryContext, "index info", ALLOCSET_SMALL_SIZES);
  relation->rd_indexcxt = indexcxt;
  MemoryContextCopyAndSetIdentifier(indexcxt, RelationGetRelationName(relation));

     
                                                
     
  InitIndexAmRoutine(relation);

     
                                                                       
                                                     
     
  relation->rd_opfamily = (Oid *)MemoryContextAllocZero(indexcxt, indnkeyatts * sizeof(Oid));
  relation->rd_opcintype = (Oid *)MemoryContextAllocZero(indexcxt, indnkeyatts * sizeof(Oid));

  amsupport = relation->rd_indam->amsupport;
  if (amsupport > 0)
  {
    int nsupport = indnatts * amsupport;

    relation->rd_support = (RegProcedure *)MemoryContextAllocZero(indexcxt, nsupport * sizeof(RegProcedure));
    relation->rd_supportinfo = (FmgrInfo *)MemoryContextAllocZero(indexcxt, nsupport * sizeof(FmgrInfo));
  }
  else
  {
    relation->rd_support = NULL;
    relation->rd_supportinfo = NULL;
  }

  relation->rd_indcollation = (Oid *)MemoryContextAllocZero(indexcxt, indnkeyatts * sizeof(Oid));

  relation->rd_indoption = (int16 *)MemoryContextAllocZero(indexcxt, indnkeyatts * sizeof(int16));

     
                                                                      
                                                                           
                               
     
  indcollDatum = fastgetattr(relation->rd_indextuple, Anum_pg_index_indcollation, GetPgIndexDescriptor(), &isnull);
  Assert(!isnull);
  indcoll = (oidvector *)DatumGetPointer(indcollDatum);
  memcpy(relation->rd_indcollation, indcoll->values, indnkeyatts * sizeof(Oid));

     
                                                                             
                                                                          
                     
     
  indclassDatum = fastgetattr(relation->rd_indextuple, Anum_pg_index_indclass, GetPgIndexDescriptor(), &isnull);
  Assert(!isnull);
  indclass = (oidvector *)DatumGetPointer(indclassDatum);

     
                                                                     
                                                                           
                                                     
     
  IndexSupportInitialize(indclass, relation->rd_support, relation->rd_opfamily, relation->rd_opcintype, amsupport, indnkeyatts);

     
                                                                
     
  indoptionDatum = fastgetattr(relation->rd_indextuple, Anum_pg_index_indoption, GetPgIndexDescriptor(), &isnull);
  Assert(!isnull);
  indoption = (int2vector *)DatumGetPointer(indoptionDatum);
  memcpy(relation->rd_indoption, indoption->values, indnkeyatts * sizeof(int16));

     
                                                                   
     
  relation->rd_indexprs = NIL;
  relation->rd_indpred = NIL;
  relation->rd_exclops = NULL;
  relation->rd_exclprocs = NULL;
  relation->rd_exclstrats = NULL;
  relation->rd_amcache = NULL;
}

   
                          
                                                       
                                               
   
                                                                   
                                             
   
                                                                               
                                                                              
                                                                              
                                    
   
static void
IndexSupportInitialize(oidvector *indclass, RegProcedure *indexSupport, Oid *opFamily, Oid *opcInType, StrategyNumber maxSupportNumber, AttrNumber maxAttributeNumber)
{
  int attIndex;

  for (attIndex = 0; attIndex < maxAttributeNumber; attIndex++)
  {
    OpClassCacheEnt *opcentry;

    if (!OidIsValid(indclass->values[attIndex]))
    {
      elog(ERROR, "bogus pg_index tuple");
    }

                                                          
    opcentry = LookupOpclassInfo(indclass->values[attIndex], maxSupportNumber);

                                              
    opFamily[attIndex] = opcentry->opcfamily;
    opcInType[attIndex] = opcentry->opcintype;
    if (maxSupportNumber > 0)
    {
      memcpy(&indexSupport[attIndex * maxSupportNumber], opcentry->supportProcs, maxSupportNumber * sizeof(RegProcedure));
    }
  }
}

   
                     
   
                                                                        
                                                                        
                                                                          
                                               
   
                                                                       
                                                                         
                                
   
                                                                         
                                                                            
                                                                        
                                                                        
                                                                           
                                                                           
                
   
static OpClassCacheEnt *
LookupOpclassInfo(Oid operatorClassOid, StrategyNumber numSupport)
{
  OpClassCacheEnt *opcentry;
  bool found;
  Relation rel;
  SysScanDesc scan;
  ScanKeyData skey[3];
  HeapTuple htup;
  bool indexOK;

  if (OpClassCache == NULL)
  {
                                                          
    HASHCTL ctl;

                                                  
    if (!CacheMemoryContext)
    {
      CreateCacheMemoryContext();
    }

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(Oid);
    ctl.entrysize = sizeof(OpClassCacheEnt);
    OpClassCache = hash_create("Operator class cache", 64, &ctl, HASH_ELEM | HASH_BLOBS);
  }

  opcentry = (OpClassCacheEnt *)hash_search(OpClassCache, (void *)&operatorClassOid, HASH_ENTER, &found);

  if (!found)
  {
                              
    opcentry->valid = false;                     
    opcentry->numSupport = numSupport;
    opcentry->supportProcs = NULL;                   
  }
  else
  {
    Assert(numSupport == opcentry->numSupport);
  }

     
                                                                            
                                                                            
                                                                           
                                                                           
                                                                          
                                                                      
                                                                   
     
#if defined(CLOBBER_CACHE_RECURSIVELY)
  opcentry->valid = false;
#endif

  if (opcentry->valid)
  {
    return opcentry;
  }

     
                                                                             
                                  
     
  if (opcentry->supportProcs == NULL && numSupport > 0)
  {
    opcentry->supportProcs = (RegProcedure *)MemoryContextAllocZero(CacheMemoryContext, numSupport * sizeof(RegProcedure));
  }

     
                                                                           
                                                                            
                     
     
  indexOK = criticalRelcachesBuilt || (operatorClassOid != OID_BTREE_OPS_OID && operatorClassOid != INT2_BTREE_OPS_OID);

     
                                                                       
                                                                             
                                                                            
                               
     
  ScanKeyInit(&skey[0], Anum_pg_opclass_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(operatorClassOid));
  rel = table_open(OperatorClassRelationId, AccessShareLock);
  scan = systable_beginscan(rel, OpclassOidIndexId, indexOK, NULL, 1, skey);

  if (HeapTupleIsValid(htup = systable_getnext(scan)))
  {
    Form_pg_opclass opclassform = (Form_pg_opclass)GETSTRUCT(htup);

    opcentry->opcfamily = opclassform->opcfamily;
    opcentry->opcintype = opclassform->opcintype;
  }
  else
  {
    elog(ERROR, "could not find tuple for opclass %u", operatorClassOid);
  }

  systable_endscan(scan);
  table_close(rel, AccessShareLock);

     
                                                                            
                                                                     
     
  if (numSupport > 0)
  {
    ScanKeyInit(&skey[0], Anum_pg_amproc_amprocfamily, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(opcentry->opcfamily));
    ScanKeyInit(&skey[1], Anum_pg_amproc_amproclefttype, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(opcentry->opcintype));
    ScanKeyInit(&skey[2], Anum_pg_amproc_amprocrighttype, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(opcentry->opcintype));
    rel = table_open(AccessMethodProcedureRelationId, AccessShareLock);
    scan = systable_beginscan(rel, AccessMethodProcedureIndexId, indexOK, NULL, 3, skey);

    while (HeapTupleIsValid(htup = systable_getnext(scan)))
    {
      Form_pg_amproc amprocform = (Form_pg_amproc)GETSTRUCT(htup);

      if (amprocform->amprocnum <= 0 || (StrategyNumber)amprocform->amprocnum > numSupport)
      {
        elog(ERROR, "invalid amproc number %d for opclass %u", amprocform->amprocnum, operatorClassOid);
      }

      opcentry->supportProcs[amprocform->amprocnum - 1] = amprocform->amproc;
    }

    systable_endscan(scan);
    table_close(rel, AccessShareLock);
  }

  opcentry->valid = true;
  return opcentry;
}

   
                                             
   
                                                  
   
static void
InitTableAmRoutine(Relation relation)
{
  relation->rd_tableam = GetTableAmRoutine(relation->rd_amhandler);
}

   
                                                                    
   
void
RelationInitTableAccessMethod(Relation relation)
{
  HeapTuple tuple;
  Form_pg_am aform;

  if (relation->rd_rel->relkind == RELKIND_SEQUENCE)
  {
       
                                                                         
                                                                      
             
       
    relation->rd_amhandler = HEAP_TABLE_AM_HANDLER_OID;
  }
  else if (IsCatalogRelation(relation))
  {
       
                                                         
       
    Assert(relation->rd_rel->relam == HEAP_TABLE_AM_OID);
    relation->rd_amhandler = HEAP_TABLE_AM_HANDLER_OID;
  }
  else
  {
       
                                                                    
                 
       
    Assert(relation->rd_rel->relam != InvalidOid);
    tuple = SearchSysCache1(AMOID, ObjectIdGetDatum(relation->rd_rel->relam));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for access method %u", relation->rd_rel->relam);
    }
    aform = (Form_pg_am)GETSTRUCT(tuple);
    relation->rd_amhandler = aform->amhandler;
    ReleaseSysCache(tuple);
  }

     
                                                
     
  InitTableAmRoutine(relation);
}

   
              
   
                                                               
                                          
                                                                        
                                                                    
                                                                  
              
   
                                                                             
                                                                             
                                                                           
                                                                              
                                    
   
                                                                    
   
static void
formrdesc(const char *relationName, Oid relationReltype, bool isshared, int natts, const FormData_pg_attribute *attrs)
{
  Relation relation;
  int i;
  bool has_not_null;

     
                                                             
     
  relation = (Relation)palloc0(sizeof(RelationData));

                                                               
  relation->rd_smgr = NULL;

     
                                                                 
     
  relation->rd_refcnt = 1;

     
                                                                           
                            
     
  relation->rd_isnailed = true;
  relation->rd_createSubid = InvalidSubTransactionId;
  relation->rd_newRelfilenodeSubid = InvalidSubTransactionId;
  relation->rd_backend = InvalidBackendId;
  relation->rd_islocaltemp = false;

     
                                    
     
                                                                            
                                                                          
                                                                   
                                                         
                                                                       
     
  relation->rd_rel = (Form_pg_class)palloc0(CLASS_TUPLE_SIZE);

  namestrcpy(&relation->rd_rel->relname, relationName);
  relation->rd_rel->relnamespace = PG_CATALOG_NAMESPACE;
  relation->rd_rel->reltype = relationReltype;

     
                                                                            
                                                                         
     
  relation->rd_rel->relisshared = isshared;
  if (isshared)
  {
    relation->rd_rel->reltablespace = GLOBALTABLESPACE_OID;
  }

                                                      
  relation->rd_rel->relpersistence = RELPERSISTENCE_PERMANENT;

                                             
  relation->rd_rel->relispopulated = true;

  relation->rd_rel->relreplident = REPLICA_IDENTITY_NOTHING;
  relation->rd_rel->relpages = 0;
  relation->rd_rel->reltuples = 0;
  relation->rd_rel->relallvisible = 0;
  relation->rd_rel->relkind = RELKIND_RELATION;
  relation->rd_rel->relnatts = (int16)natts;
  relation->rd_rel->relam = HEAP_TABLE_AM_OID;

     
                                     
     
                                                                            
                                                             
                                                 
     
  relation->rd_att = CreateTemplateTupleDesc(natts);
  relation->rd_att->tdrefcount = 1;                         

  relation->rd_att->tdtypeid = relationReltype;
  relation->rd_att->tdtypmod = -1;                          

     
                                
     
  has_not_null = false;
  for (i = 0; i < natts; i++)
  {
    memcpy(TupleDescAttr(relation->rd_att, i), &attrs[i], ATTRIBUTE_FIXED_PART_SIZE);
    has_not_null |= attrs[i].attnotnull;
                                        
    TupleDescAttr(relation->rd_att, i)->attcacheoff = -1;
  }

                                                                           
  TupleDescAttr(relation->rd_att, 0)->attcacheoff = 0;

                            
  if (has_not_null)
  {
    TupleConstr *constr = (TupleConstr *)palloc0(sizeof(TupleConstr));

    constr->has_not_null = true;
    relation->rd_att->constr = constr;
  }

     
                                                                      
     
  RelationGetRelid(relation) = TupleDescAttr(relation->rd_att, 0)->attrelid;

     
                                                                           
                                                                        
                                                                             
                                                                  
     
  relation->rd_rel->relfilenode = InvalidOid;
  if (IsBootstrapProcessingMode())
  {
    RelationMapUpdateMap(RelationGetRelid(relation), RelationGetRelid(relation), isshared, true);
  }

     
                                                      
     
  RelationInitLockInfo(relation);                 

     
                                                                 
     
  RelationInitPhysicalAddr(relation);

     
                                     
     
  relation->rd_rel->relam = HEAP_TABLE_AM_OID;
  relation->rd_tableam = GetHeapamTableAmRoutine();

     
                                                                  
     
  if (IsBootstrapProcessingMode())
  {
                                               
    relation->rd_rel->relhasindex = false;
  }
  else
  {
                                                                    
    relation->rd_rel->relhasindex = true;
  }

     
                                 
     
  RelationCacheInsert(relation, false);

                        
  relation->rd_isvalid = true;
}

                                                                    
                                            
                                                                    
   

   
                          
   
                                                               
   
                                                                       
                                                                  
                                          
   
                                                                   
                                                       
   
                                                                     
                                                         
                                              
   
Relation
RelationIdGetRelation(Oid relationId)
{
  Relation rd;

                                                                          
  Assert(IsTransactionState());

     
                                            
     
  RelationIdCacheLookup(relationId, rd);

  if (RelationIsValid(rd))
  {
    RelationIncrementReferenceCount(rd);
                                             
    if (!rd->rd_isvalid)
    {
         
                                                                        
                                                                        
                                                               
         
      if (rd->rd_rel->relkind == RELKIND_INDEX || rd->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
      {
        RelationReloadIndexInfo(rd);
      }
      else
      {
        RelationClearRelation(rd, true);
      }

         
                                                                         
                                                                   
                                                                         
                                                                  
                                                                 
         
      Assert(rd->rd_isvalid || (rd->rd_isnailed && !criticalRelcachesBuilt));
    }
    return rd;
  }

     
                                                                            
         
     
  rd = RelationBuildDesc(relationId, true);
  if (RelationIsValid(rd))
  {
    RelationIncrementReferenceCount(rd);
  }
  return rd;
}

                                                                    
                                          
                                                                    
   

   
                                   
                                         
   
                                                                        
                                                                   
                                                         
   
void
RelationIncrementReferenceCount(Relation rel)
{
  ResourceOwnerEnlargeRelationRefs(CurrentResourceOwner);
  rel->rd_refcnt += 1;
  if (!IsBootstrapProcessingMode())
  {
    ResourceOwnerRememberRelationRef(CurrentResourceOwner, rel);
  }
}

   
                                   
                                         
   
void
RelationDecrementReferenceCount(Relation rel)
{
  Assert(rel->rd_refcnt > 0);
  rel->rd_refcnt -= 1;
  if (!IsBootstrapProcessingMode())
  {
    ResourceOwnerForgetRelationRef(CurrentResourceOwner, rel);
  }
}

   
                                          
   
                                             
   
                                                                         
                                                                         
                                                                        
                                                                       
                                     
   
void
RelationClose(Relation relation)
{
                                             
  RelationDecrementReferenceCount(relation);

#ifdef RELCACHE_FORCE_RELEASE
  if (RelationHasReferenceCountZero(relation) && relation->rd_createSubid == InvalidSubTransactionId && relation->rd_newRelfilenodeSubid == InvalidSubTransactionId)
  {
    RelationClearRelation(relation, false);
  }
#endif
}

   
                                                                          
   
                                                                         
                                                                       
                                                                        
                                                                         
                                                                         
                                                                           
                                                                  
   
                                                                        
                                                                        
                                                                    
                                                                         
                   
   
                                                                              
                                                                         
                                                                      
   
                                                                             
                                                                         
                                                                            
                                                                          
                                                                           
   
static void
RelationReloadIndexInfo(Relation relation)
{
  bool indexOK;
  HeapTuple pg_class_tuple;
  Form_pg_class relp;

                                                     
  Assert((relation->rd_rel->relkind == RELKIND_INDEX || relation->rd_rel->relkind == RELKIND_PARTITIONED_INDEX) && !relation->rd_isvalid);

                                        
  RelationCloseSmgr(relation);

                                                        
  if (relation->rd_amcache)
  {
    pfree(relation->rd_amcache);
  }
  relation->rd_amcache = NULL;

     
                                                                           
                                                                         
                                                                           
                                                                           
                                                           
     
  if (relation->rd_rel->relisshared && !criticalRelcachesBuilt)
  {
    relation->rd_isvalid = true;
    return;
  }

     
                           
     
                                                                            
                                
     
  indexOK = (RelationGetRelid(relation) != ClassOidIndexId);
  pg_class_tuple = ScanPgRelation(RelationGetRelid(relation), indexOK, false);
  if (!HeapTupleIsValid(pg_class_tuple))
  {
    elog(ERROR, "could not find pg_class tuple for index %u", RelationGetRelid(relation));
  }
  relp = (Form_pg_class)GETSTRUCT(pg_class_tuple);
  memcpy(relation->rd_rel, relp, CLASS_TUPLE_SIZE);
                                              
  if (relation->rd_options)
  {
    pfree(relation->rd_options);
  }
  RelationParseRelOptions(relation, pg_class_tuple);
                                
  heap_freetuple(pg_class_tuple);
                                                               
  RelationInitPhysicalAddr(relation);

     
                                                                           
                                                                           
                                                                             
                                                                           
                                                                          
                                     
     
  if (!IsSystemRelation(relation))
  {
    HeapTuple tuple;
    Form_pg_index index;

    tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(RelationGetRelid(relation)));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for index %u", RelationGetRelid(relation));
    }
    index = (Form_pg_index)GETSTRUCT(tuple);

       
                                                                         
                                                                        
                                                                        
                                                       
       
    relation->rd_index->indisunique = index->indisunique;
    relation->rd_index->indisprimary = index->indisprimary;
    relation->rd_index->indisexclusion = index->indisexclusion;
    relation->rd_index->indimmediate = index->indimmediate;
    relation->rd_index->indisclustered = index->indisclustered;
    relation->rd_index->indisvalid = index->indisvalid;
    relation->rd_index->indcheckxmin = index->indcheckxmin;
    relation->rd_index->indisready = index->indisready;
    relation->rd_index->indislive = index->indislive;

                                                                        
    HeapTupleHeaderSetXmin(relation->rd_indextuple->t_data, HeapTupleHeaderGetXmin(tuple->t_data));

    ReleaseSysCache(tuple);
  }

                                  
  relation->rd_isvalid = true;
}

   
                                                                           
   
                                                                               
                                                                               
                                                                       
                                                                               
                     
   
static void
RelationReloadNailed(Relation relation)
{
  Assert(relation->rd_isnailed);

     
                                                                         
                      
     
  RelationInitPhysicalAddr(relation);

                                         
  relation->rd_isvalid = false;

     
                                                                            
                                                                 
                                                                       
                                                   
     
  if (!IsTransactionState() || relation->rd_refcnt <= 1)
  {
    return;
  }

  if (relation->rd_rel->relkind == RELKIND_INDEX)
  {
       
                                                                          
                                                       
       
    RelationReloadIndexInfo(relation);
  }
  else
  {
       
                                                                     
                                                                   
                                                                         
                                                                          
                                                                         
       
    if (criticalRelcachesBuilt)
    {
      HeapTuple pg_class_tuple;
      Form_pg_class relp;

         
                                                                       
                                                   
         
      relation->rd_isvalid = true;

      pg_class_tuple = ScanPgRelation(RelationGetRelid(relation), true, false);
      relp = (Form_pg_class)GETSTRUCT(pg_class_tuple);
      memcpy(relation->rd_rel, relp, CLASS_TUPLE_SIZE);
      heap_freetuple(pg_class_tuple);

         
                                                                       
                        
         
      relation->rd_isvalid = true;
    }
  }
}

   
                           
   
                                                                     
                                                                    
   
static void
RelationDestroyRelation(Relation relation, bool remember_tupdesc)
{
  Assert(RelationHasReferenceCountZero(relation));

     
                                                                         
                                                                           
                         
     
  RelationCloseSmgr(relation);

     
                                                                             
                   
     
  if (relation->rd_rel)
  {
    pfree(relation->rd_rel);
  }
                                            
  Assert(relation->rd_att->tdrefcount > 0);
  if (--relation->rd_att->tdrefcount == 0)
  {
       
                                                                    
                                                                         
                                                                      
                                                                          
                                                                     
                                                            
       
    if (remember_tupdesc)
    {
      RememberToFreeTupleDescAtEOX(relation->rd_att);
    }
    else
    {
      FreeTupleDesc(relation->rd_att);
    }
  }
  FreeTriggerDesc(relation->trigdesc);
  list_free_deep(relation->rd_fkeylist);
  list_free(relation->rd_indexlist);
  list_free(relation->rd_statlist);
  bms_free(relation->rd_indexattr);
  bms_free(relation->rd_keyattr);
  bms_free(relation->rd_pkattr);
  bms_free(relation->rd_idattr);
  if (relation->rd_pubactions)
  {
    pfree(relation->rd_pubactions);
  }
  if (relation->rd_options)
  {
    pfree(relation->rd_options);
  }
  if (relation->rd_indextuple)
  {
    pfree(relation->rd_indextuple);
  }
  if (relation->rd_amcache)
  {
    pfree(relation->rd_amcache);
  }
  if (relation->rd_fdwroutine)
  {
    pfree(relation->rd_fdwroutine);
  }
  if (relation->rd_indexcxt)
  {
    MemoryContextDelete(relation->rd_indexcxt);
  }
  if (relation->rd_rulescxt)
  {
    MemoryContextDelete(relation->rd_rulescxt);
  }
  if (relation->rd_rsdesc)
  {
    MemoryContextDelete(relation->rd_rsdesc->rscxt);
  }
  if (relation->rd_partkeycxt)
  {
    MemoryContextDelete(relation->rd_partkeycxt);
  }
  if (relation->rd_pdcxt)
  {
    MemoryContextDelete(relation->rd_pdcxt);
  }
  if (relation->rd_partcheckcxt)
  {
    MemoryContextDelete(relation->rd_partcheckcxt);
  }
  pfree(relation);
}

   
                         
   
                                                                         
                                                                         
                                                                        
                   
   
                                                                     
                                                                      
                                                                           
                                                                        
                                                                          
                           
   
                                                                         
                                                                          
                                              
   
static void
RelationClearRelation(Relation relation, bool rebuild)
{
     
                                                                            
                                                                          
                                                                      
                                                                          
     
  Assert(rebuild ? !RelationHasReferenceCountZero(relation) : RelationHasReferenceCountZero(relation));

     
                                                                         
                                                                          
                                                                           
                                                                          
                 
     
  RelationCloseSmgr(relation);

                                   
  if (relation->rd_amcache)
  {
    pfree(relation->rd_amcache);
  }
  relation->rd_amcache = NULL;

     
                                                                         
                                             
     
  if (relation->rd_isnailed)
  {
    RelationReloadNailed(relation);
    return;
  }

     
                                                                           
                                                                             
                                                                       
                                                                            
                                                   
     
  if ((relation->rd_rel->relkind == RELKIND_INDEX || relation->rd_rel->relkind == RELKIND_PARTITIONED_INDEX) && relation->rd_refcnt > 0 && relation->rd_indexcxt != NULL)
  {
    relation->rd_isvalid = false;                              
    if (IsTransactionState())
    {
      RelationReloadIndexInfo(relation);
    }
    return;
  }

                                                    
  relation->rd_isvalid = false;

     
                                                                        
                                                                          
                                                                        
                   
     
  if (!rebuild)
  {
                                       
    RelationCacheDelete(relation);

                             
    RelationDestroyRelation(relation, false);
  }
  else if (!IsTransactionState())
  {
       
                                                                        
                                                                       
                                                                           
                    
       
                                                                          
                                                                        
                                                                        
                                                                         
                                                                        
                                                                  
                                                                         
                                                                    
                                                                          
                                                                          
                                                                          
                                           
       
    return;
  }
  else
  {
       
                                                                        
                                                                         
                                                                           
                                                                      
                                                                       
                                                                          
                                                                      
                                                                      
                                                                   
                                                                         
                                                                      
                                                                    
                                                                           
                                                                          
                
       
                                                                           
                                                                           
                                                                   
                                                                           
                                                                           
                                                                        
                                                                       
                                            
       
                                                                         
                                                                     
                                                  
       
    Relation newrel;
    Oid save_relid = RelationGetRelid(relation);
    bool keep_tupdesc;
    bool keep_rules;
    bool keep_policies;
    bool keep_partkey;
    bool keep_partdesc;

                                                                 
    newrel = RelationBuildDesc(save_relid, false);

       
                                                                         
                                                                       
                                                                           
                            
       

    if (newrel == NULL)
    {
         
                                                                        
                                                                      
                                                                  
                                                                      
                                                                        
                                                                         
                                                                
         
      if (HistoricSnapshotActive())
      {
        return;
      }

         
                                                                        
                                                                        
                                                                         
                                                                    
                                                                       
         
      elog(ERROR, "relation %u deleted while still in use", save_relid);
    }

    keep_tupdesc = equalTupleDescs(relation->rd_att, newrel->rd_att);
    keep_rules = equalRuleLocks(relation->rd_rules, newrel->rd_rules);
    keep_policies = equalRSDesc(relation->rd_rsdesc, newrel->rd_rsdesc);
                                                                    
    keep_partkey = (relation->rd_partkey != NULL);
    keep_partdesc = equalPartitionDescs(relation->rd_partkey, relation->rd_partdesc, newrel->rd_partdesc);

       
                                                                     
                                                                           
                                                                          
                                    
       
                                                                          
                                                                         
                            
       
#define SWAPFIELD(fldtype, fldname)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    fldtype _tmp = newrel->fldname;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    newrel->fldname = relation->fldname;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    relation->fldname = _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

                                         
    {
      RelationData tmpstruct;

      memcpy(&tmpstruct, newrel, sizeof(RelationData));
      memcpy(newrel, relation, sizeof(RelationData));
      memcpy(relation, &tmpstruct, sizeof(RelationData));
    }

                                                                        
    SWAPFIELD(SMgrRelation, rd_smgr);
                                     
    SWAPFIELD(int, rd_refcnt);
                                   
    Assert(newrel->rd_isnailed == relation->rd_isnailed);
                                             
    SWAPFIELD(SubTransactionId, rd_createSubid);
    SWAPFIELD(SubTransactionId, rd_newRelfilenodeSubid);
                                                        
    SWAPFIELD(Form_pg_class, rd_rel);
                                                                  
    memcpy(relation->rd_rel, newrel->rd_rel, CLASS_TUPLE_SIZE);
                                                                      
    if (keep_tupdesc)
    {
      SWAPFIELD(TupleDesc, rd_att);
    }
    if (keep_rules)
    {
      SWAPFIELD(RuleLock *, rd_rules);
      SWAPFIELD(MemoryContext, rd_rulescxt);
    }
    if (keep_policies)
    {
      SWAPFIELD(RowSecurityDesc *, rd_rsdesc);
    }
                                              
    SWAPFIELD(Oid, rd_toastoid);
                                       
    SWAPFIELD(struct PgStat_TableStatus *, pgstat_info);
                                                             
    if (keep_partkey)
    {
      SWAPFIELD(PartitionKey, rd_partkey);
      SWAPFIELD(MemoryContext, rd_partkeycxt);
    }
    if (keep_partdesc)
    {
      SWAPFIELD(PartitionDesc, rd_partdesc);
      SWAPFIELD(MemoryContext, rd_pdcxt);
    }
    else if (rebuild && newrel->rd_pdcxt != NULL)
    {
         
                                                                  
                                                                       
                                                                    
                                                                  
                                                                       
                                                                      
                                                                         
                
         
                                                                         
                                                                       
                 
         
      MemoryContextSetParent(newrel->rd_pdcxt, relation->rd_pdcxt);
      newrel->rd_partdesc = NULL;
      newrel->rd_pdcxt = NULL;
    }

#undef SWAPFIELD

                                                       
    RelationDestroyRelation(newrel, !keep_tupdesc);
  }
}

   
                         
   
                                                                          
                                                                         
   
static void
RelationFlushRelation(Relation relation)
{
  if (relation->rd_createSubid != InvalidSubTransactionId || relation->rd_newRelfilenodeSubid != InvalidSubTransactionId)
  {
       
                                                                       
                                                                  
                                                                    
       
                                                                         
                                                                         
                                                             
       
    RelationIncrementReferenceCount(relation);
    RelationClearRelation(relation, true);
    RelationDecrementReferenceCount(relation);
  }
  else
  {
       
                                                                       
       
    bool rebuild = !RelationHasReferenceCountZero(relation);

    RelationClearRelation(relation, rebuild);
  }
}

   
                                                                    
   
                                                                  
                          
   
void
RelationForgetRelation(Oid rid)
{
  Relation relation;

  RelationIdCacheLookup(rid, relation);

  if (!PointerIsValid(relation))
  {
    return;                                  
  }

  if (!RelationHasReferenceCountZero(relation))
  {
    elog(ERROR, "relation %u is still open", rid);
  }

                                                  
  RelationClearRelation(relation, false);
}

   
                                 
   
                                                         
   
                                                                             
                                                                            
              
   
                                                                   
                                                                    
                                                                    
                                                                 
                                 
   
void
RelationCacheInvalidateEntry(Oid relationId)
{
  Relation relation;

  RelationIdCacheLookup(relationId, relation);

  if (PointerIsValid(relation))
  {
    relcacheInvalsReceived++;
    RelationFlushRelation(relation);
  }
  else
  {
    int i;

    for (i = 0; i < in_progress_list_len; i++)
    {
      if (in_progress_list[i].reloid == relationId)
      {
        in_progress_list[i].invalidated = true;
      }
    }
  }
}

   
                           
                                                                           
                                                                           
                                                      
   
                                                                            
                                                                            
                                                                      
                                                                            
                                                                        
                                                            
   
                                                                          
                                                                          
                                                                           
                                                                           
                                                                          
                                                                         
                                                                         
                                                                             
                                                                           
                                                     
   
                                                                         
                                                                        
                                                                          
                                                                          
                                                                             
   
                                                                         
                                                                            
                                                                         
                                                       
   
void
RelationCacheInvalidate(bool debug_discard)
{
  HASH_SEQ_STATUS status;
  RelIdCacheEnt *idhentry;
  Relation relation;
  List *rebuildFirstList = NIL;
  List *rebuildList = NIL;
  ListCell *l;
  int i;

     
                                                                        
     
  RelationMapInvalidateAll();

               
  hash_seq_init(&status, RelationIdCache);

  while ((idhentry = (RelIdCacheEnt *)hash_seq_search(&status)) != NULL)
  {
    relation = idhentry->reldesc;

                                                                       
    RelationCloseSmgr(relation);

       
                                                                          
                                                                           
                                                                          
                              
       
    if (relation->rd_createSubid != InvalidSubTransactionId || relation->rd_newRelfilenodeSubid != InvalidSubTransactionId)
    {
      continue;
    }

    relcacheInvalsReceived++;

    if (RelationHasReferenceCountZero(relation))
    {
                                         
      Assert(!relation->rd_isnailed);
      RelationClearRelation(relation, false);
    }
    else
    {
         
                                                                      
                                                                       
                                                                   
                                                                      
                                                             
         
      if (RelationIsMapped(relation))
      {
        RelationInitPhysicalAddr(relation);
      }

         
                                                                    
                                                              
                                                                     
                                                                    
                                                                      
                                                                      
                              
         
      if (RelationGetRelid(relation) == RelationRelationId)
      {
        rebuildFirstList = lcons(relation, rebuildFirstList);
      }
      else if (RelationGetRelid(relation) == ClassOidIndexId)
      {
        rebuildFirstList = lappend(rebuildFirstList, relation);
      }
      else if (relation->rd_isnailed)
      {
        rebuildList = lcons(relation, rebuildList);
      }
      else
      {
        rebuildList = lappend(rebuildList, relation);
      }
    }
  }

     
                                                                           
                                                                            
                                 
     
  smgrcloseall();

                                                                   
  foreach (l, rebuildFirstList)
  {
    relation = (Relation)lfirst(l);
    RelationClearRelation(relation, true);
  }
  list_free(rebuildFirstList);
  foreach (l, rebuildList)
  {
    relation = (Relation)lfirst(l);
    RelationClearRelation(relation, true);
  }
  list_free(rebuildList);

  if (!debug_discard)
  {
                                                               
    for (i = 0; i < in_progress_list_len; i++)
    {
      in_progress_list[i].invalidated = true;
    }
  }
}

   
                                                               
   
                                                                             
                                                        
   
void
RelationCloseSmgrByOid(Oid relationId)
{
  Relation relation;

  RelationIdCacheLookup(relationId, relation);

  if (!PointerIsValid(relation))
  {
    return;                                  
  }

  RelationCloseSmgr(relation);
}

static void
RememberToFreeTupleDescAtEOX(TupleDesc td)
{
  if (EOXactTupleDescArray == NULL)
  {
    MemoryContext oldcxt;

    oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

    EOXactTupleDescArray = (TupleDesc *)palloc(16 * sizeof(TupleDesc));
    EOXactTupleDescArrayLen = 16;
    NextEOXactTupleDescNum = 0;
    MemoryContextSwitchTo(oldcxt);
  }
  else if (NextEOXactTupleDescNum >= EOXactTupleDescArrayLen)
  {
    int32 newlen = EOXactTupleDescArrayLen * 2;

    Assert(EOXactTupleDescArrayLen > 0);

    EOXactTupleDescArray = (TupleDesc *)repalloc(EOXactTupleDescArray, newlen * sizeof(TupleDesc));
    EOXactTupleDescArrayLen = newlen;
  }

  EOXactTupleDescArray[NextEOXactTupleDescNum++] = td;
}

   
                          
   
                                                              
   
                                                                        
                                                                         
                                                                          
                                                                
   
                                                                     
                                                                
                                                                         
                                                                          
                          
   
void
AtEOXact_RelationCache(bool isCommit)
{
  HASH_SEQ_STATUS status;
  RelIdCacheEnt *idhentry;
  int i;

     
                                                                           
                                          
     
  Assert(in_progress_list_len == 0 || !isCommit);
  in_progress_list_len = 0;

     
                                                                           
                                                                   
     
                                                                       
                                                                
                                                                            
                                                                         
                                                                            
                             
     
  if (eoxact_list_overflowed)
  {
    hash_seq_init(&status, RelationIdCache);
    while ((idhentry = (RelIdCacheEnt *)hash_seq_search(&status)) != NULL)
    {
      AtEOXact_cleanup(idhentry->reldesc, isCommit);
    }
  }
  else
  {
    for (i = 0; i < eoxact_list_len; i++)
    {
      idhentry = (RelIdCacheEnt *)hash_search(RelationIdCache, (void *)&eoxact_list[i], HASH_FIND, NULL);
      if (idhentry != NULL)
      {
        AtEOXact_cleanup(idhentry->reldesc, isCommit);
      }
    }
  }

  if (EOXactTupleDescArrayLen > 0)
  {
    Assert(EOXactTupleDescArray != NULL);
    for (i = 0; i < NextEOXactTupleDescNum; i++)
    {
      FreeTupleDesc(EOXactTupleDescArray[i]);
    }
    pfree(EOXactTupleDescArray);
    EOXactTupleDescArray = NULL;
  }

                                                                
  eoxact_list_len = 0;
  eoxact_list_overflowed = false;
  NextEOXactTupleDescNum = 0;
  EOXactTupleDescArrayLen = 0;
}

   
                    
   
                                                             
   
                                                                           
                                                         
   
static void
AtEOXact_cleanup(Relation relation, bool isCommit)
{
     
                                                                 
                                                                
     
                                                                    
                                                                       
                                                                        
     
                                                                            
                                                                           
                                                                          
                                                                            
                                     
     
#ifdef USE_ASSERT_CHECKING
  if (!IsBootstrapProcessingMode())
  {
    int expected_refcnt;

    expected_refcnt = relation->rd_isnailed ? 1 : 0;
    Assert(relation->rd_refcnt == expected_refcnt);
  }
#endif

     
                                                          
     
                                                                        
                                                                           
                                          
     
  if (relation->rd_createSubid != InvalidSubTransactionId)
  {
    if (isCommit)
    {
      relation->rd_createSubid = InvalidSubTransactionId;
    }
    else if (RelationHasReferenceCountZero(relation))
    {
      RelationClearRelation(relation, false);
      return;
    }
    else
    {
         
                                                                       
                                                                 
                                                                         
                                                                  
                                                           
                                            
         
      relation->rd_createSubid = InvalidSubTransactionId;
      elog(WARNING, "cannot remove relcache entry for \"%s\" because it has nonzero refcount", RelationGetRelationName(relation));
    }
  }

     
                                                               
     
  relation->rd_newRelfilenodeSubid = InvalidSubTransactionId;
}

   
                             
   
                                                             
   
                                                                        
   
void
AtEOSubXact_RelationCache(bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid)
{
  HASH_SEQ_STATUS status;
  RelIdCacheEnt *idhentry;
  int i;

     
                                                                           
                                                                           
                                 
     
  Assert(in_progress_list_len == 0 || !isCommit);
  in_progress_list_len = 0;

     
                                                                           
                                                                         
                                         
     
  if (eoxact_list_overflowed)
  {
    hash_seq_init(&status, RelationIdCache);
    while ((idhentry = (RelIdCacheEnt *)hash_seq_search(&status)) != NULL)
    {
      AtEOSubXact_cleanup(idhentry->reldesc, isCommit, mySubid, parentSubid);
    }
  }
  else
  {
    for (i = 0; i < eoxact_list_len; i++)
    {
      idhentry = (RelIdCacheEnt *)hash_search(RelationIdCache, (void *)&eoxact_list[i], HASH_FIND, NULL);
      if (idhentry != NULL)
      {
        AtEOSubXact_cleanup(idhentry->reldesc, isCommit, mySubid, parentSubid);
      }
    }
  }

                                                              
}

   
                       
   
                                                           
   
                                                                           
                                                         
   
static void
AtEOSubXact_cleanup(Relation relation, bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid)
{
     
                                                             
     
                                                                           
                                                 
     
  if (relation->rd_createSubid == mySubid)
  {
    if (isCommit)
    {
      relation->rd_createSubid = parentSubid;
    }
    else if (RelationHasReferenceCountZero(relation))
    {
      RelationClearRelation(relation, false);
      return;
    }
    else
    {
         
                                                                       
                                                                 
                                                                       
                                                                        
                                                             
         
      relation->rd_createSubid = parentSubid;
      elog(WARNING, "cannot remove relcache entry for \"%s\" because it has nonzero refcount", RelationGetRelationName(relation));
    }
  }

     
                                                                          
     
  if (relation->rd_newRelfilenodeSubid == mySubid)
  {
    if (isCommit)
    {
      relation->rd_newRelfilenodeSubid = parentSubid;
    }
    else
    {
      relation->rd_newRelfilenodeSubid = InvalidSubTransactionId;
    }
  }
}

   
                               
                                                                 
                                     
   
Relation
RelationBuildLocalRelation(const char *relname, Oid relnamespace, TupleDesc tupDesc, Oid relid, Oid accessmtd, Oid relfilenode, Oid reltablespace, bool shared_relation, bool mapped_relation, char relpersistence, char relkind)
{
  Relation rel;
  MemoryContext oldcxt;
  int natts = tupDesc->natts;
  int i;
  bool has_not_null;
  bool nailit;

  AssertArg(natts >= 0);

     
                                                               
     
                                                                       
                                      
     
  switch (relid)
  {
  case DatabaseRelationId:
  case AuthIdRelationId:
  case AuthMemRelationId:
  case RelationRelationId:
  case AttributeRelationId:
  case ProcedureRelationId:
  case TypeRelationId:
    nailit = true;
    break;
  default:
    nailit = false;
    break;
  }

     
                                                                    
                                                                        
                                                                           
                                     
     
  if (shared_relation != IsSharedRelation(relid))
  {
    elog(ERROR, "shared_relation flag for \"%s\" does not match IsSharedRelation(%u)", relname, relid);
  }

                                                  
  Assert(mapped_relation || !shared_relation);

     
                                                               
     
  if (!CacheMemoryContext)
  {
    CreateCacheMemoryContext();
  }

  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

     
                                                                        
     
  rel = (Relation)palloc0(sizeof(RelationData));

                                                               
  rel->rd_smgr = NULL;

                                     
  rel->rd_isnailed = nailit;

  rel->rd_refcnt = nailit ? 1 : 0;

                                              
  rel->rd_createSubid = GetCurrentSubTransactionId();
  rel->rd_newRelfilenodeSubid = InvalidSubTransactionId;

     
                                                                       
                                                                          
                                                                          
                                                                             
                                                                  
     
  rel->rd_att = CreateTupleDescCopy(tupDesc);
  rel->rd_att->tdrefcount = 1;                         
  has_not_null = false;
  for (i = 0; i < natts; i++)
  {
    Form_pg_attribute satt = TupleDescAttr(tupDesc, i);
    Form_pg_attribute datt = TupleDescAttr(rel->rd_att, i);

    datt->attidentity = satt->attidentity;
    datt->attgenerated = satt->attgenerated;
    datt->attnotnull = satt->attnotnull;
    has_not_null |= satt->attnotnull;
  }

  if (has_not_null)
  {
    TupleConstr *constr = (TupleConstr *)palloc0(sizeof(TupleConstr));

    constr->has_not_null = true;
    rel->rd_att->constr = constr;
  }

     
                                                                         
     
  rel->rd_rel = (Form_pg_class)palloc0(CLASS_TUPLE_SIZE);

  namestrcpy(&rel->rd_rel->relname, relname);
  rel->rd_rel->relnamespace = relnamespace;

  rel->rd_rel->relkind = relkind;
  rel->rd_rel->relnatts = natts;
  rel->rd_rel->reltype = InvalidOid;
                                  
  rel->rd_rel->relowner = BOOTSTRAP_SUPERUSERID;

                                                              
  rel->rd_rel->relpersistence = relpersistence;
  switch (relpersistence)
  {
  case RELPERSISTENCE_UNLOGGED:
  case RELPERSISTENCE_PERMANENT:
    rel->rd_backend = InvalidBackendId;
    rel->rd_islocaltemp = false;
    break;
  case RELPERSISTENCE_TEMP:
    Assert(isTempOrTempToastNamespace(relnamespace));
    rel->rd_backend = BackendIdForTempRelations();
    rel->rd_islocaltemp = true;
    break;
  default:
    elog(ERROR, "invalid relpersistence: %c", relpersistence);
    break;
  }

                                                                 
  if (relkind == RELKIND_MATVIEW)
  {
    rel->rd_rel->relispopulated = false;
  }
  else
  {
    rel->rd_rel->relispopulated = true;
  }

                                                                             
  if (!IsCatalogNamespace(relnamespace) && (relkind == RELKIND_RELATION || relkind == RELKIND_MATVIEW || relkind == RELKIND_PARTITIONED_TABLE))
  {
    rel->rd_rel->relreplident = REPLICA_IDENTITY_DEFAULT;
  }
  else
  {
    rel->rd_rel->relreplident = REPLICA_IDENTITY_NOTHING;
  }

     
                                                                            
                                                                            
                                                  
     
  rel->rd_rel->relisshared = shared_relation;

  RelationGetRelid(rel) = relid;

  for (i = 0; i < natts; i++)
  {
    TupleDescAttr(rel->rd_att, i)->attrelid = relid;
  }

  rel->rd_rel->reltablespace = reltablespace;

  if (mapped_relation)
  {
    rel->rd_rel->relfilenode = InvalidOid;
                                                  
    RelationMapUpdateMap(relid, relfilenode, shared_relation, true);
  }
  else
  {
    rel->rd_rel->relfilenode = relfilenode;
  }

  RelationInitLockInfo(rel);                 

  RelationInitPhysicalAddr(rel);

  rel->rd_rel->relam = accessmtd;

     
                                                                           
                                                                           
                                           
     
  MemoryContextSwitchTo(oldcxt);

  if (relkind == RELKIND_RELATION || relkind == RELKIND_SEQUENCE || relkind == RELKIND_TOASTVALUE || relkind == RELKIND_MATVIEW)
  {
    RelationInitTableAccessMethod(rel);
  }

     
                                                  
     
                                                                          
                                                                          
                                                                        
                                                                           
           
     
  RelationCacheInsert(rel, nailit);

     
                                                                           
                                               
     
  EOXactListAdd(rel);

                        
  rel->rd_isvalid = true;

     
                                                  
     
  RelationIncrementReferenceCount(rel);

  return rel;
}

   
                             
   
                                                                     
                                         
   
                                                                            
                                                                            
                                                                         
                                                                              
                                                         
   
                                                            
   
void
RelationSetNewRelfilenode(Relation relation, char persistence)
{
  Oid newrelfilenode;
  Relation pg_class;
  HeapTuple tuple;
  Form_pg_class classform;
  MultiXactId minmulti = InvalidMultiXactId;
  TransactionId freezeXid = InvalidTransactionId;
  RelFileNode newrnode;

                                  
  newrelfilenode = GetNewRelFileNode(relation->rd_rel->reltablespace, NULL, persistence);

     
                                                                       
     
  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(RelationGetRelid(relation)));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for relation %u", RelationGetRelid(relation));
  }
  classform = (Form_pg_class)GETSTRUCT(tuple);

     
                                                                  
     
  RelationDropStorage(relation);

     
                                                                         
                                                                       
                                             
     
                                                                             
                                                                 
     
  newrnode = relation->rd_node;
  newrnode.relNode = newrelfilenode;

  switch (relation->rd_rel->relkind)
  {
  case RELKIND_INDEX:
  case RELKIND_SEQUENCE:
  {
                                                 
    SMgrRelation srel;

    srel = RelationCreateStorage(newrnode, persistence);
    smgrclose(srel);
  }
  break;

  case RELKIND_RELATION:
  case RELKIND_TOASTVALUE:
  case RELKIND_MATVIEW:
    table_relation_set_new_filenode(relation, &newrnode, persistence, &freezeXid, &minmulti);
    break;

  default:
                                                  
    elog(ERROR, "relation \"%s\" does not have storage", RelationGetRelationName(relation));
    break;
  }

     
                                                                        
                                                                        
     
                                                                             
                                                                             
                                                                            
            
     
  if (RelationIsMapped(relation))
  {
                                                 
    Assert(relation->rd_rel->relkind == RELKIND_INDEX);

                                                                        
    Assert(classform->relfrozenxid == freezeXid);
    Assert(classform->relminmxid == minmulti);
    Assert(classform->relpersistence == persistence);

       
                                                                   
                                                                        
                                                                        
                                            
       
    (void)GetCurrentTransactionId();

                     
    RelationMapUpdateMap(RelationGetRelid(relation), newrelfilenode, relation->rd_rel->relisshared, false);

                                                                        
    CacheInvalidateRelcache(relation);
  }
  else
  {
                                                
    classform->relfilenode = newrelfilenode;

                                                  
    if (relation->rd_rel->relkind != RELKIND_SEQUENCE)
    {
      classform->relpages = 0;                                      
      classform->reltuples = 0;
      classform->relallvisible = 0;
    }
    classform->relfrozenxid = freezeXid;
    classform->relminmxid = minmulti;
    classform->relpersistence = persistence;

    CatalogTupleUpdate(pg_class, &tuple->t_self, tuple);
  }

  heap_freetuple(tuple);

  table_close(pg_class, RowExclusiveLock);

     
                                                                             
                                                   
     
  CommandCounterIncrement();

     
                                                                        
                                                                           
                                                    
     
  relation->rd_newRelfilenodeSubid = GetCurrentSubTransactionId();

                                                                    
  EOXactListAdd(relation);
}

   
                            
   
                                                                 
                                                                  
                                                                   
                                                                  
                                                               
                                                                  
                                           
   

#define INITRELCACHESIZE 400

void
RelationCacheInitialize(void)
{
  HASHCTL ctl;
  int allocsize;

     
                                           
     
  if (!CacheMemoryContext)
  {
    CreateCacheMemoryContext();
  }

     
                                                
     
  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(RelIdCacheEnt);
  RelationIdCache = hash_create("Relcache by OID", INITRELCACHESIZE, &ctl, HASH_ELEM | HASH_BLOBS);

     
                                                          
     
  allocsize = 4;
  in_progress_list = MemoryContextAlloc(CacheMemoryContext, allocsize * sizeof(*in_progress_list));
  in_progress_list_maxlen = allocsize;

     
                                                 
     
  RelationMapInitialize();
}

   
                                  
   
                                                                            
                                                                        
                                                                           
                                                                          
                                                                       
                                               
                                                           
   
void
RelationCacheInitializePhase2(void)
{
  MemoryContext oldcxt;

     
                                           
     
  RelationMapInitializePhase2();

     
                                                                           
              
     
  if (IsBootstrapProcessingMode())
  {
    return;
  }

     
                                    
     
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

     
                                                                             
                                                                           
     
  if (!load_relcache_init_file(true))
  {
    formrdesc("pg_database", DatabaseRelation_Rowtype_Id, true, Natts_pg_database, Desc_pg_database);
    formrdesc("pg_authid", AuthIdRelation_Rowtype_Id, true, Natts_pg_authid, Desc_pg_authid);
    formrdesc("pg_auth_members", AuthMemRelation_Rowtype_Id, true, Natts_pg_auth_members, Desc_pg_auth_members);
    formrdesc("pg_shseclabel", SharedSecLabelRelation_Rowtype_Id, true, Natts_pg_shseclabel, Desc_pg_shseclabel);
    formrdesc("pg_subscription", SubscriptionRelation_Rowtype_Id, true, Natts_pg_subscription, Desc_pg_subscription);

#define NUM_CRITICAL_SHARED_RELS 5                                   
  }

  MemoryContextSwitchTo(oldcxt);
}

   
                                  
   
                                                                  
                                                                       
                                                                   
                                                                      
                                                                         
                                                                    
                                                                     
                                                                         
                                                                      
                                       
   
void
RelationCacheInitializePhase3(void)
{
  HASH_SEQ_STATUS status;
  RelIdCacheEnt *idhentry;
  MemoryContext oldcxt;
  bool needNewCacheFile = !criticalSharedRelcachesBuilt;

     
                                           
     
  RelationMapInitializePhase3();

     
                                    
     
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

     
                                                                            
                                                                             
               
     
  if (IsBootstrapProcessingMode() || !load_relcache_init_file(false))
  {
    needNewCacheFile = true;

    formrdesc("pg_class", RelationRelation_Rowtype_Id, false, Natts_pg_class, Desc_pg_class);
    formrdesc("pg_attribute", AttributeRelation_Rowtype_Id, false, Natts_pg_attribute, Desc_pg_attribute);
    formrdesc("pg_proc", ProcedureRelation_Rowtype_Id, false, Natts_pg_proc, Desc_pg_proc);
    formrdesc("pg_type", TypeRelation_Rowtype_Id, false, Natts_pg_type, Desc_pg_type);

#define NUM_CRITICAL_LOCAL_RELS 4                                   
  }

  MemoryContextSwitchTo(oldcxt);

                                                                        
  if (IsBootstrapProcessingMode())
  {
    return;
  }

     
                                                                           
                                                                           
                                                                             
                                                                      
                                                                             
                                                                             
                                                                             
                                                                             
                                                      
     
                                                                             
                                                                           
                                                                            
                                                                  
                                                                          
                                   
     
                                                                            
                                                                        
                                                                         
                                                                       
                                                                           
                                        
     
  if (!criticalRelcachesBuilt)
  {
    load_critical_index(ClassOidIndexId, RelationRelationId);
    load_critical_index(AttributeRelidNumIndexId, AttributeRelationId);
    load_critical_index(IndexRelidIndexId, IndexRelationId);
    load_critical_index(OpclassOidIndexId, OperatorClassRelationId);
    load_critical_index(AccessMethodProcedureIndexId, AccessMethodProcedureRelationId);
    load_critical_index(RewriteRelRulenameIndexId, RewriteRelationId);
    load_critical_index(TriggerRelidNameIndexId, TriggerRelationId);

#define NUM_CRITICAL_LOCAL_INDEXES 7                                   

    criticalRelcachesBuilt = true;
  }

     
                                          
     
                                                                             
                                                                        
                                                                       
                                                                         
                                                                           
                                                                      
                                                                     
                       
     
  if (!criticalSharedRelcachesBuilt)
  {
    load_critical_index(DatabaseNameIndexId, DatabaseRelationId);
    load_critical_index(DatabaseOidIndexId, DatabaseRelationId);
    load_critical_index(AuthIdRolnameIndexId, AuthIdRelationId);
    load_critical_index(AuthIdOidIndexId, AuthIdRelationId);
    load_critical_index(AuthMemMemRoleIndexId, AuthMemRelationId);
    load_critical_index(SharedSecLabelObjectIndexId, SharedSecLabelRelationId);

#define NUM_CRITICAL_SHARED_INDEXES 6                                   

    criticalSharedRelcachesBuilt = true;
  }

     
                                                                          
                                                                           
                                                                            
                                                                      
                                                                            
                                                                  
     
                                                                             
                                                                        
                                                                             
                                                                             
                                                                           
                                                                           
                                                              
     
  hash_seq_init(&status, RelationIdCache);

  while ((idhentry = (RelIdCacheEnt *)hash_seq_search(&status)) != NULL)
  {
    Relation relation = idhentry->reldesc;
    bool restart = false;

       
                                                                         
       
    RelationIncrementReferenceCount(relation);

       
                                                               
       
    if (relation->rd_rel->relowner == InvalidOid)
    {
      HeapTuple htup;
      Form_pg_class relp;

      htup = SearchSysCache1(RELOID, ObjectIdGetDatum(RelationGetRelid(relation)));
      if (!HeapTupleIsValid(htup))
      {
        elog(FATAL, "cache lookup failed for relation %u", RelationGetRelid(relation));
      }
      relp = (Form_pg_class)GETSTRUCT(htup);

         
                                                       
                                 
         
      memcpy((char *)relation->rd_rel, (char *)relp, CLASS_TUPLE_SIZE);

                                                     
      if (relation->rd_options)
      {
        pfree(relation->rd_options);
      }
      RelationParseRelOptions(relation, htup);

         
                                                                       
                                                                        
                                                                        
                                                    
         
      Assert(relation->rd_att->tdtypeid == relp->reltype);
      Assert(relation->rd_att->tdtypmod == -1);

      ReleaseSysCache(htup);

                                                                  
      if (relation->rd_rel->relowner == InvalidOid)
      {
        elog(ERROR, "invalid relowner in pg_class entry for \"%s\"", RelationGetRelationName(relation));
      }

      restart = true;
    }

       
                                                         
       
                                                                       
                                                                         
                                                                         
                                                                           
       
    if (relation->rd_rel->relhasrules && relation->rd_rules == NULL)
    {
      RelationBuildRuleLock(relation);
      if (relation->rd_rules == NULL)
      {
        relation->rd_rel->relhasrules = false;
      }
      restart = true;
    }
    if (relation->rd_rel->relhastriggers && relation->trigdesc == NULL)
    {
      RelationBuildTriggers(relation);
      if (relation->trigdesc == NULL)
      {
        relation->rd_rel->relhastriggers = false;
      }
      restart = true;
    }

       
                                                                         
                                                                        
                                                   
                                                                         
                                                   
       
    if (relation->rd_rel->relrowsecurity && relation->rd_rsdesc == NULL)
    {
      RelationBuildRowSecurity(relation);

      Assert(relation->rd_rsdesc != NULL);
      restart = true;
    }

       
                                                                        
       
    if (relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && relation->rd_partkey == NULL)
    {
      RelationBuildPartitionKey(relation);
      Assert(relation->rd_partkey != NULL);

      restart = true;
    }

    if (relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && relation->rd_partdesc == NULL)
    {
      RelationBuildPartitionDesc(relation);
      Assert(relation->rd_partdesc != NULL);

      restart = true;
    }

    if (relation->rd_tableam == NULL && (relation->rd_rel->relkind == RELKIND_RELATION || relation->rd_rel->relkind == RELKIND_SEQUENCE || relation->rd_rel->relkind == RELKIND_TOASTVALUE || relation->rd_rel->relkind == RELKIND_MATVIEW))
    {
      RelationInitTableAccessMethod(relation);
      Assert(relation->rd_tableam != NULL);

      restart = true;
    }

                                      
    RelationDecrementReferenceCount(relation);

                                                   
    if (restart)
    {
      hash_seq_term(&status);
      hash_seq_init(&status, RelationIdCache);
    }
  }

     
                                                                            
                                                                     
     
  if (needNewCacheFile)
  {
       
                                                                           
                                                                           
                                                                          
                                                                    
       
    InitCatalogCachePhase2();

                             
    write_relcache_init_file(true);
    write_relcache_init_file(false);
  }
}

   
                                                    
   
                                                                              
                  
   
static void
load_critical_index(Oid indexoid, Oid heapoid)
{
  Relation ird;

     
                                                                           
                                                                            
                                                                            
                                
     
  LockRelationOid(heapoid, AccessShareLock);
  LockRelationOid(indexoid, AccessShareLock);
  ird = RelationBuildDesc(indexoid, true);
  if (ird == NULL)
  {
    elog(PANIC, "could not open critical system index %u", indexoid);
  }
  ird->rd_isnailed = true;
  ird->rd_refcnt = 1;
  UnlockRelationOid(indexoid, AccessShareLock);
  UnlockRelationOid(heapoid, AccessShareLock);
}

   
                                                                          
                                                                          
   
                                                                           
                                                                              
                                                                            
                                                                            
                                                                              
                                                                              
                      
   
static TupleDesc
BuildHardcodedDescriptor(int natts, const FormData_pg_attribute *attrs)
{
  TupleDesc result;
  MemoryContext oldcxt;
  int i;

  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

  result = CreateTemplateTupleDesc(natts);
  result->tdtypeid = RECORDOID;                                   
  result->tdtypmod = -1;

  for (i = 0; i < natts; i++)
  {
    memcpy(TupleDescAttr(result, i), &attrs[i], ATTRIBUTE_FIXED_PART_SIZE);
                                        
    TupleDescAttr(result, i)->attcacheoff = -1;
  }

                                                                           
  TupleDescAttr(result, 0)->attcacheoff = 0;

                                                           

  MemoryContextSwitchTo(oldcxt);

  return result;
}

static TupleDesc
GetPgClassDescriptor(void)
{
  static TupleDesc pgclassdesc = NULL;

                     
  if (pgclassdesc == NULL)
  {
    pgclassdesc = BuildHardcodedDescriptor(Natts_pg_class, Desc_pg_class);
  }

  return pgclassdesc;
}

static TupleDesc
GetPgIndexDescriptor(void)
{
  static TupleDesc pgindexdesc = NULL;

                     
  if (pgindexdesc == NULL)
  {
    pgindexdesc = BuildHardcodedDescriptor(Natts_pg_index, Desc_pg_index);
  }

  return pgindexdesc;
}

   
                                                                  
   
static void
AttrDefaultFetch(Relation relation)
{
  AttrDefault *attrdef = relation->rd_att->constr->defval;
  int ndef = relation->rd_att->constr->num_defval;
  Relation adrel;
  SysScanDesc adscan;
  ScanKeyData skey;
  HeapTuple htup;
  Datum val;
  bool isnull;
  int i;

  ScanKeyInit(&skey, Anum_pg_attrdef_adrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

  adrel = table_open(AttrDefaultRelationId, AccessShareLock);
  adscan = systable_beginscan(adrel, AttrDefaultIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(htup = systable_getnext(adscan)))
  {
    Form_pg_attrdef adform = (Form_pg_attrdef)GETSTRUCT(htup);
    Form_pg_attribute attr = TupleDescAttr(relation->rd_att, adform->adnum - 1);

    for (i = 0; i < ndef; i++)
    {
      if (adform->adnum != attrdef[i].adnum)
      {
        continue;
      }
      if (attrdef[i].adbin != NULL)
      {
        elog(WARNING, "multiple attrdef records found for attr %s of rel %s", NameStr(attr->attname), RelationGetRelationName(relation));
      }

      val = fastgetattr(htup, Anum_pg_attrdef_adbin, adrel->rd_att, &isnull);
      if (isnull)
      {
        elog(WARNING, "null adbin for attr %s of rel %s", NameStr(attr->attname), RelationGetRelationName(relation));
      }
      else
      {
                                                                
        char *s = TextDatumGetCString(val);

        attrdef[i].adbin = MemoryContextStrdup(CacheMemoryContext, s);
        pfree(s);
      }
      break;
    }

    if (i >= ndef)
    {
      elog(WARNING, "unexpected attrdef record found for attr %d of rel %s", adform->adnum, RelationGetRelationName(relation));
    }
  }

  systable_endscan(adscan);
  table_close(adrel, AccessShareLock);
}

   
                                                
   
static void
CheckConstraintFetch(Relation relation)
{
  ConstrCheck *check = relation->rd_att->constr->check;
  int ncheck = relation->rd_att->constr->num_check;
  Relation conrel;
  SysScanDesc conscan;
  ScanKeyData skey[1];
  HeapTuple htup;
  int found = 0;

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

  conrel = table_open(ConstraintRelationId, AccessShareLock);
  conscan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 1, skey);

  while (HeapTupleIsValid(htup = systable_getnext(conscan)))
  {
    Form_pg_constraint conform = (Form_pg_constraint)GETSTRUCT(htup);
    Datum val;
    bool isnull;
    char *s;

                                        
    if (conform->contype != CONSTRAINT_CHECK)
    {
      continue;
    }

    if (found >= ncheck)
    {
      elog(ERROR, "unexpected constraint record found for rel %s", RelationGetRelationName(relation));
    }

    check[found].ccvalid = conform->convalidated;
    check[found].ccnoinherit = conform->connoinherit;
    check[found].ccname = MemoryContextStrdup(CacheMemoryContext, NameStr(conform->conname));

                                              
    val = fastgetattr(htup, Anum_pg_constraint_conbin, conrel->rd_att, &isnull);
    if (isnull)
    {
      elog(ERROR, "null conbin for rel %s", RelationGetRelationName(relation));
    }

                                                            
    s = TextDatumGetCString(val);
    check[found].ccbin = MemoryContextStrdup(CacheMemoryContext, s);
    pfree(s);

    found++;
  }

  systable_endscan(conscan);
  table_close(conrel, AccessShareLock);

  if (found != ncheck)
  {
    elog(ERROR, "%d constraint record(s) missing for rel %s", ncheck - found, RelationGetRelationName(relation));
  }

                                                                            
  if (ncheck > 1)
  {
    qsort(check, ncheck, sizeof(ConstrCheck), CheckConstraintCmp);
  }
}

   
                                                        
   
static int
CheckConstraintCmp(const void *a, const void *b)
{
  const ConstrCheck *ca = (const ConstrCheck *)a;
  const ConstrCheck *cb = (const ConstrCheck *)b;

  return strcmp(ca->ccname, cb->ccname);
}

   
                                                                          
   
                                                                          
                                                                           
                                                              
   
                                                                        
                                                                      
                                                                   
                                                                  
                                                                     
                                                                         
   
List *
RelationGetFKeyList(Relation relation)
{
  List *result;
  Relation conrel;
  SysScanDesc conscan;
  ScanKeyData skey;
  HeapTuple htup;
  List *oldlist;
  MemoryContext oldcxt;

                                                   
  if (relation->rd_fkeyvalid)
  {
    return relation->rd_fkeylist;
  }

                                                                         
  if (!relation->rd_rel->relhastriggers && relation->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    return NIL;
  }

     
                                                                           
                                                                           
                                                                             
                                                   
     
  result = NIL;

                                                                             
  ScanKeyInit(&skey, Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

  conrel = table_open(ConstraintRelationId, AccessShareLock);
  conscan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(htup = systable_getnext(conscan)))
  {
    Form_pg_constraint constraint = (Form_pg_constraint)GETSTRUCT(htup);
    ForeignKeyCacheInfo *info;

                                    
    if (constraint->contype != CONSTRAINT_FOREIGN)
    {
      continue;
    }

    info = makeNode(ForeignKeyCacheInfo);
    info->conoid = constraint->oid;
    info->conrelid = constraint->conrelid;
    info->confrelid = constraint->confrelid;

    DeconstructFkConstraintRow(htup, &info->nkeys, info->conkey, info->confkey, info->conpfeqop, NULL, NULL);

                                          
    result = lappend(result, info);
  }

  systable_endscan(conscan);
  table_close(conrel, AccessShareLock);

                                                                    
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
  oldlist = relation->rd_fkeylist;
  relation->rd_fkeylist = copyObject(result);
  relation->rd_fkeyvalid = true;
  MemoryContextSwitchTo(oldcxt);

                                                
  list_free_deep(oldlist);

  return result;
}

   
                                                                          
   
                                                                            
                                                                            
                                                                         
                                                                           
                                                                           
                                     
   
                                                                             
                                                                          
                                                  
   
                                                                          
                                                                          
                                                                           
                                                                         
                                                              
   
                                                                               
                                                                              
                                                                          
                                                                             
                                                                         
   
                                                                          
                                                                             
                                                                      
                                                                        
   
List *
RelationGetIndexList(Relation relation)
{
  Relation indrel;
  SysScanDesc indscan;
  ScanKeyData skey;
  HeapTuple htup;
  List *result;
  List *oldlist;
  char replident = relation->rd_rel->relreplident;
  Oid pkeyIndex = InvalidOid;
  Oid candidateIndex = InvalidOid;
  MemoryContext oldcxt;

                                                   
  if (relation->rd_indexvalid)
  {
    return list_copy(relation->rd_indexlist);
  }

     
                                                                           
                                                                           
                                                                             
                                                   
     
  result = NIL;

                                                                        
  ScanKeyInit(&skey, Anum_pg_index_indrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

  indrel = table_open(IndexRelationId, AccessShareLock);
  indscan = systable_beginscan(indrel, IndexIndrelidIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(htup = systable_getnext(indscan)))
  {
    Form_pg_index index = (Form_pg_index)GETSTRUCT(htup);

       
                                                                       
                                                                         
                                                                        
                                                                 
       
    if (!index->indislive)
    {
      continue;
    }

                                                            
    result = insert_ordered_oid(result, index->indexrelid);

       
                                                                      
                                                                           
                            
       
    if (!index->indisvalid || !index->indisunique || !index->indimmediate || !heap_attisnull(htup, Anum_pg_index_indpred, NULL))
    {
      continue;
    }

                                           
    if (index->indisprimary)
    {
      pkeyIndex = index->indexrelid;
    }

                                                  
    if (index->indisreplident)
    {
      candidateIndex = index->indexrelid;
    }
  }

  systable_endscan(indscan);

  table_close(indrel, AccessShareLock);

                                                                    
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
  oldlist = relation->rd_indexlist;
  relation->rd_indexlist = list_copy(result);
  relation->rd_pkindex = pkeyIndex;
  if (replident == REPLICA_IDENTITY_DEFAULT && OidIsValid(pkeyIndex))
  {
    relation->rd_replidindex = pkeyIndex;
  }
  else if (replident == REPLICA_IDENTITY_INDEX && OidIsValid(candidateIndex))
  {
    relation->rd_replidindex = candidateIndex;
  }
  else
  {
    relation->rd_replidindex = InvalidOid;
  }
  relation->rd_indexvalid = true;
  MemoryContextSwitchTo(oldcxt);

                                                
  list_free(oldlist);

  return result;
}

   
                          
                                                              
   
                                                                        
                                                                        
                                                                          
                                                                      
                                                                      
                                                                        
                                                        
   
                                                                          
                                 
   
                                                                               
                                                                              
                                                                          
                                                                             
                                                                            
   
List *
RelationGetStatExtList(Relation relation)
{
  Relation indrel;
  SysScanDesc indscan;
  ScanKeyData skey;
  HeapTuple htup;
  List *result;
  List *oldlist;
  MemoryContext oldcxt;

                                                   
  if (relation->rd_statvalid != 0)
  {
    return list_copy(relation->rd_statlist);
  }

     
                                                                           
                                                                           
                                                                             
                                                   
     
  result = NIL;

     
                                                                         
          
     
  ScanKeyInit(&skey, Anum_pg_statistic_ext_stxrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

  indrel = table_open(StatisticExtRelationId, AccessShareLock);
  indscan = systable_beginscan(indrel, StatisticExtRelidIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(htup = systable_getnext(indscan)))
  {
    Oid oid = ((Form_pg_statistic_ext)GETSTRUCT(htup))->oid;

    result = insert_ordered_oid(result, oid);
  }

  systable_endscan(indscan);

  table_close(indrel, AccessShareLock);

                                                                    
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
  oldlist = relation->rd_statlist;
  relation->rd_statlist = list_copy(result);

  relation->rd_statvalid = true;
  MemoryContextSwitchTo(oldcxt);

                                                
  list_free(oldlist);

  return result;
}

   
                      
                                                                     
   
                                                                         
                                                                        
                                                                          
              
   
static List *
insert_ordered_oid(List *list, Oid datum)
{
  ListCell *prev;

                                           
  if (list == NIL || datum < linitial_oid(list))
  {
    return lcons_oid(datum, list);
  }
                                              
  prev = list_head(list);
  for (;;)
  {
    ListCell *curr = lnext(prev);

    if (curr == NULL || datum < lfirst_oid(curr))
    {
      break;                                             
    }

    prev = curr;
  }
                                           
  lappend_cell_oid(list, prev, datum);
  return list;
}

   
                                                                             
   
                                                 
   
Oid
RelationGetPrimaryKeyIndex(Relation relation)
{
  List *ilist;

  if (!relation->rd_indexvalid)
  {
                                                      
    ilist = RelationGetIndexList(relation);
    list_free(ilist);
    Assert(relation->rd_indexvalid);
  }

  return relation->rd_pkindex;
}

   
                                                                               
   
                                                 
   
Oid
RelationGetReplicaIndex(Relation relation)
{
  List *ilist;

  if (!relation->rd_indexvalid)
  {
                                                      
    ilist = RelationGetIndexList(relation);
    list_free(ilist);
    Assert(relation->rd_indexvalid);
  }

  return relation->rd_replidindex;
}

   
                                                                         
   
                                                                           
                                                                             
                                                                            
                                                                           
                                            
   
List *
RelationGetIndexExpressions(Relation relation)
{
  List *result;
  Datum exprsDatum;
  bool isnull;
  char *exprsString;
  MemoryContext oldcxt;

                                                     
  if (relation->rd_indexprs)
  {
    return copyObject(relation->rd_indexprs);
  }

                                             
  if (relation->rd_indextuple == NULL || heap_attisnull(relation->rd_indextuple, Anum_pg_index_indexprs, NULL))
  {
    return NIL;
  }

     
                                                                          
                                                                           
                                                                        
     
  exprsDatum = heap_getattr(relation->rd_indextuple, Anum_pg_index_indexprs, GetPgIndexDescriptor(), &isnull);
  Assert(!isnull);
  exprsString = TextDatumGetCString(exprsDatum);
  result = (List *)stringToNode(exprsString);
  pfree(exprsString);

     
                                                                             
                                                                           
                                                                            
                                                                        
                                          
     
  result = (List *)eval_const_expressions(NULL, (Node *)result);

                                     
  fix_opfuncids((Node *)result);

                                                                    
  oldcxt = MemoryContextSwitchTo(relation->rd_indexcxt);
  relation->rd_indexprs = copyObject(result);
  MemoryContextSwitchTo(oldcxt);

  return result;
}

   
                                                                          
   
                                                                       
                                                                      
                                                                          
   
List *
RelationGetDummyIndexExpressions(Relation relation)
{
  List *result;
  Datum exprsDatum;
  bool isnull;
  char *exprsString;
  List *rawExprs;
  ListCell *lc;

                                             
  if (relation->rd_indextuple == NULL || heap_attisnull(relation->rd_indextuple, Anum_pg_index_indexprs, NULL))
  {
    return NIL;
  }

                                                  
  exprsDatum = heap_getattr(relation->rd_indextuple, Anum_pg_index_indexprs, GetPgIndexDescriptor(), &isnull);
  Assert(!isnull);
  exprsString = TextDatumGetCString(exprsDatum);
  rawExprs = (List *)stringToNode(exprsString);
  pfree(exprsString);

                                                                     
  result = NIL;
  foreach (lc, rawExprs)
  {
    Node *rawExpr = (Node *)lfirst(lc);

    result = lappend(result, makeConst(exprType(rawExpr), exprTypmod(rawExpr), exprCollation(rawExpr), 1, (Datum)0, true, true));
  }

  return result;
}

   
                                                                     
   
                                                                             
                                             
                                                                  
                                                                            
                                                                           
                                            
   
List *
RelationGetIndexPredicate(Relation relation)
{
  List *result;
  Datum predDatum;
  bool isnull;
  char *predString;
  MemoryContext oldcxt;

                                                     
  if (relation->rd_indpred)
  {
    return copyObject(relation->rd_indpred);
  }

                                             
  if (relation->rd_indextuple == NULL || heap_attisnull(relation->rd_indextuple, Anum_pg_index_indpred, NULL))
  {
    return NIL;
  }

     
                                                                          
                                                                           
                                                                        
     
  predDatum = heap_getattr(relation->rd_indextuple, Anum_pg_index_indpred, GetPgIndexDescriptor(), &isnull);
  Assert(!isnull);
  predString = TextDatumGetCString(predDatum);
  result = (List *)stringToNode(predString);
  pfree(predString);

     
                                                                           
                                                                             
                                                                            
                                                                           
                                                                        
                                                                            
                  
     
  result = (List *)eval_const_expressions(NULL, (Node *)result);

  result = (List *)canonicalize_qual((Expr *)result, false);

                                           
  result = make_ands_implicit((Expr *)result);

                                     
  fix_opfuncids((Node *)result);

                                                                    
  oldcxt = MemoryContextSwitchTo(relation->rd_indexcxt);
  relation->rd_indpred = copyObject(result);
  MemoryContextSwitchTo(oldcxt);

  return result;
}

   
                                                                         
   
                                                                          
                                                                             
                                                                           
                
   
                                                                               
                                                                               
                                       
   
                                                                              
                                                                              
   
                                                                           
                                                                               
                                                                            
                                                                            
                                                                        
   
                                                                             
                                          
   
Bitmapset *
RelationGetIndexAttrBitmap(Relation relation, IndexAttrBitmapKind attrKind)
{
  Bitmapset *indexattrs;                        
  Bitmapset *uindexattrs;                                 
  Bitmapset *pkindexattrs;                                   
  Bitmapset *idindexattrs;                                      
  List *indexoidlist;
  List *newindexoidlist;
  Oid relpkindex;
  Oid relreplindex;
  ListCell *l;
  MemoryContext oldcxt;

                                                     
  if (relation->rd_indexattr != NULL)
  {
    switch (attrKind)
    {
    case INDEX_ATTR_BITMAP_ALL:
      return bms_copy(relation->rd_indexattr);
    case INDEX_ATTR_BITMAP_KEY:
      return bms_copy(relation->rd_keyattr);
    case INDEX_ATTR_BITMAP_PRIMARY_KEY:
      return bms_copy(relation->rd_pkattr);
    case INDEX_ATTR_BITMAP_IDENTITY_KEY:
      return bms_copy(relation->rd_idattr);
    default:
      elog(ERROR, "unknown attrKind %u", attrKind);
    }
  }

                                          
  if (!RelationGetForm(relation)->relhasindex)
  {
    return NULL;
  }

     
                                                                             
     
restart:
  indexoidlist = RelationGetIndexList(relation);

                                                        
  if (indexoidlist == NIL)
  {
    return NULL;
  }

     
                                                               
                                                                       
                                                                       
                                                                          
                                    
     
  relpkindex = relation->rd_pkindex;
  relreplindex = relation->rd_replidindex;

     
                                                              
     
                                                                             
                                                                          
                                                                        
                                                                         
                                                                          
                                                       
     
  indexattrs = NULL;
  uindexattrs = NULL;
  pkindexattrs = NULL;
  idindexattrs = NULL;
  foreach (l, indexoidlist)
  {
    Oid indexOid = lfirst_oid(l);
    Relation indexDesc;
    Datum datum;
    bool isnull;
    Node *indexExpressions;
    Node *indexPredicate;
    int i;
    bool isKey;                      
    bool isPK;                     
    bool isIDKey;                             

    indexDesc = index_open(indexOid, AccessShareLock);

       
                                                                       
                                                                          
                                                                      
                                                                          
                                                                          
                                                                        
       

    datum = heap_getattr(indexDesc->rd_indextuple, Anum_pg_index_indexprs, GetPgIndexDescriptor(), &isnull);
    if (!isnull)
    {
      indexExpressions = stringToNode(TextDatumGetCString(datum));
    }
    else
    {
      indexExpressions = NULL;
    }

    datum = heap_getattr(indexDesc->rd_indextuple, Anum_pg_index_indpred, GetPgIndexDescriptor(), &isnull);
    if (!isnull)
    {
      indexPredicate = stringToNode(TextDatumGetCString(datum));
    }
    else
    {
      indexPredicate = NULL;
    }

                                                        
    isKey = indexDesc->rd_index->indisunique && indexExpressions == NULL && indexPredicate == NULL;

                                
    isPK = (indexOid == relpkindex);

                                                                     
    isIDKey = (indexOid == relreplindex);

                                             
    for (i = 0; i < indexDesc->rd_index->indnatts; i++)
    {
      int attrnum = indexDesc->rd_index->indkey.values[i];

         
                                                                      
                                                                         
                                                                       
                                                                         
                                                                        
                                                             
         
      if (attrnum != 0)
      {
        indexattrs = bms_add_member(indexattrs, attrnum - FirstLowInvalidHeapAttributeNumber);

        if (isKey && i < indexDesc->rd_index->indnkeyatts)
        {
          uindexattrs = bms_add_member(uindexattrs, attrnum - FirstLowInvalidHeapAttributeNumber);
        }

        if (isPK && i < indexDesc->rd_index->indnkeyatts)
        {
          pkindexattrs = bms_add_member(pkindexattrs, attrnum - FirstLowInvalidHeapAttributeNumber);
        }

        if (isIDKey && i < indexDesc->rd_index->indnkeyatts)
        {
          idindexattrs = bms_add_member(idindexattrs, attrnum - FirstLowInvalidHeapAttributeNumber);
        }
      }
    }

                                                         
    pull_varattnos(indexExpressions, 1, &indexattrs);

                                                            
    pull_varattnos(indexPredicate, 1, &indexattrs);

    index_close(indexDesc, AccessShareLock);
  }

     
                                                                             
                                                                          
                                                                           
                                                             
     
  newindexoidlist = RelationGetIndexList(relation);
  if (equal(indexoidlist, newindexoidlist) && relpkindex == relation->rd_pkindex && relreplindex == relation->rd_replidindex)
  {
                                              
    list_free(newindexoidlist);
    list_free(indexoidlist);
  }
  else
  {
                                                            
    list_free(newindexoidlist);
    list_free(indexoidlist);
    bms_free(uindexattrs);
    bms_free(pkindexattrs);
    bms_free(idindexattrs);
    bms_free(indexattrs);

    goto restart;
  }

                                                          
  bms_free(relation->rd_indexattr);
  relation->rd_indexattr = NULL;
  bms_free(relation->rd_keyattr);
  relation->rd_keyattr = NULL;
  bms_free(relation->rd_pkattr);
  relation->rd_pkattr = NULL;
  bms_free(relation->rd_idattr);
  relation->rd_idattr = NULL;

     
                                                                             
                                                                            
                                                                           
                                                                        
            
     
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
  relation->rd_keyattr = bms_copy(uindexattrs);
  relation->rd_pkattr = bms_copy(pkindexattrs);
  relation->rd_idattr = bms_copy(idindexattrs);
  relation->rd_indexattr = bms_copy(indexattrs);
  MemoryContextSwitchTo(oldcxt);

                                                                   
  switch (attrKind)
  {
  case INDEX_ATTR_BITMAP_ALL:
    return indexattrs;
  case INDEX_ATTR_BITMAP_KEY:
    return uindexattrs;
  case INDEX_ATTR_BITMAP_PRIMARY_KEY:
    return pkindexattrs;
  case INDEX_ATTR_BITMAP_IDENTITY_KEY:
    return idindexattrs;
  default:
    elog(ERROR, "unknown attrKind %u", attrKind);
    return NULL;
  }
}

   
                                                                           
   
                                                                    
                                                                             
                                                                        
                                                                        
                                                                        
   
void
RelationGetExclusionInfo(Relation indexRelation, Oid **operators, Oid **procs, uint16 **strategies)
{
  int indnkeyatts;
  Oid *ops;
  Oid *funcs;
  uint16 *strats;
  Relation conrel;
  SysScanDesc conscan;
  ScanKeyData skey[1];
  HeapTuple htup;
  bool found;
  MemoryContext oldcxt;
  int i;

  indnkeyatts = IndexRelationGetNumberOfKeyAttributes(indexRelation);

                                               
  *operators = ops = (Oid *)palloc(sizeof(Oid) * indnkeyatts);
  *procs = funcs = (Oid *)palloc(sizeof(Oid) * indnkeyatts);
  *strategies = strats = (uint16 *)palloc(sizeof(uint16) * indnkeyatts);

                                                     
  if (indexRelation->rd_exclstrats != NULL)
  {
    memcpy(ops, indexRelation->rd_exclops, sizeof(Oid) * indnkeyatts);
    memcpy(funcs, indexRelation->rd_exclprocs, sizeof(Oid) * indnkeyatts);
    memcpy(strats, indexRelation->rd_exclstrats, sizeof(uint16) * indnkeyatts);
    return;
  }

     
                                                                           
                                                                          
                                                                  
     
                                                                            
                                                                         
                                                                  
     
  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(indexRelation->rd_index->indrelid));

  conrel = table_open(ConstraintRelationId, AccessShareLock);
  conscan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 1, skey);
  found = false;

  while (HeapTupleIsValid(htup = systable_getnext(conscan)))
  {
    Form_pg_constraint conform = (Form_pg_constraint)GETSTRUCT(htup);
    Datum val;
    bool isnull;
    ArrayType *arr;
    int nelem;

                                                           
    if (conform->contype != CONSTRAINT_EXCLUSION || conform->conindid != RelationGetRelid(indexRelation))
    {
      continue;
    }

                                  
    if (found)
    {
      elog(ERROR, "unexpected exclusion constraint record found for rel %s", RelationGetRelationName(indexRelation));
    }
    found = true;

                                                  
    val = fastgetattr(htup, Anum_pg_constraint_conexclop, conrel->rd_att, &isnull);
    if (isnull)
    {
      elog(ERROR, "null conexclop for rel %s", RelationGetRelationName(indexRelation));
    }

    arr = DatumGetArrayTypeP(val);                         
    nelem = ARR_DIMS(arr)[0];
    if (ARR_NDIM(arr) != 1 || nelem != indnkeyatts || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
    {
      elog(ERROR, "conexclop is not a 1-D Oid array");
    }

    memcpy(ops, ARR_DATA_PTR(arr), sizeof(Oid) * indnkeyatts);
  }

  systable_endscan(conscan);
  table_close(conrel, AccessShareLock);

  if (!found)
  {
    elog(ERROR, "exclusion constraint record missing for rel %s", RelationGetRelationName(indexRelation));
  }

                                                      
  for (i = 0; i < indnkeyatts; i++)
  {
    funcs[i] = get_opcode(ops[i]);
    strats[i] = get_op_opfamily_strategy(ops[i], indexRelation->rd_opfamily[i]);
                                                                
    if (strats[i] == InvalidStrategy)
    {
      elog(ERROR, "could not find strategy for operator %u in family %u", ops[i], indexRelation->rd_opfamily[i]);
    }
  }

                                                         
  oldcxt = MemoryContextSwitchTo(indexRelation->rd_indexcxt);
  indexRelation->rd_exclops = (Oid *)palloc(sizeof(Oid) * indnkeyatts);
  indexRelation->rd_exclprocs = (Oid *)palloc(sizeof(Oid) * indnkeyatts);
  indexRelation->rd_exclstrats = (uint16 *)palloc(sizeof(uint16) * indnkeyatts);
  memcpy(indexRelation->rd_exclops, ops, sizeof(Oid) * indnkeyatts);
  memcpy(indexRelation->rd_exclprocs, funcs, sizeof(Oid) * indnkeyatts);
  memcpy(indexRelation->rd_exclstrats, strats, sizeof(uint16) * indnkeyatts);
  MemoryContextSwitchTo(oldcxt);
}

   
                                                   
   
struct PublicationActions *
GetRelationPublicationActions(Relation relation)
{
  List *puboids;
  ListCell *lc;
  MemoryContext oldcxt;
  PublicationActions *pubactions = palloc0(sizeof(PublicationActions));

     
                                                                           
                 
     
  if (!is_publishable_relation(relation))
  {
    return pubactions;
  }

  if (relation->rd_pubactions)
  {
    return memcpy(pubactions, relation->rd_pubactions, sizeof(PublicationActions));
  }

                                              
  puboids = GetRelationPublications(RelationGetRelid(relation));
  puboids = list_concat_unique_oid(puboids, GetAllTablesPublications());

  foreach (lc, puboids)
  {
    Oid pubid = lfirst_oid(lc);
    HeapTuple tup;
    Form_pg_publication pubform;

    tup = SearchSysCache1(PUBLICATIONOID, ObjectIdGetDatum(pubid));

    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for publication %u", pubid);
    }

    pubform = (Form_pg_publication)GETSTRUCT(tup);

    pubactions->pubinsert |= pubform->pubinsert;
    pubactions->pubupdate |= pubform->pubupdate;
    pubactions->pubdelete |= pubform->pubdelete;
    pubactions->pubtruncate |= pubform->pubtruncate;

    ReleaseSysCache(tup);

       
                                                                           
                           
       
    if (pubactions->pubinsert && pubactions->pubupdate && pubactions->pubdelete && pubactions->pubtruncate)
    {
      break;
    }
  }

  if (relation->rd_pubactions)
  {
    pfree(relation->rd_pubactions);
    relation->rd_pubactions = NULL;
  }

                                                           
  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
  relation->rd_pubactions = palloc(sizeof(PublicationActions));
  memcpy(relation->rd_pubactions, pubactions, sizeof(PublicationActions));
  MemoryContextSwitchTo(oldcxt);

  return pubactions;
}

   
                                                                    
   
                                                                              
                                                                          
                                                                       
   

   
                                                             
                                 
   
int
errtable(Relation rel)
{
  err_generic_string(PG_DIAG_SCHEMA_NAME, get_namespace_name(RelationGetNamespace(rel)));
  err_generic_string(PG_DIAG_TABLE_NAME, RelationGetRelationName(rel));

  return 0;                                   
}

   
                                                                  
                                                   
   
                                                                             
                                                                            
   
int
errtablecol(Relation rel, int attnum)
{
  TupleDesc reldesc = RelationGetDescr(rel);
  const char *colname;

                                                                       
  if (attnum > 0 && attnum <= reldesc->natts)
  {
    colname = NameStr(TupleDescAttr(reldesc, attnum - 1)->attname);
  }
  else
  {
    colname = get_attname(RelationGetRelid(rel), attnum, false);
  }

  return errtablecolname(rel, colname);
}

   
                                                                      
                                                                            
                                                                          
   
                                                                         
                                                                              
                        
   
int
errtablecolname(Relation rel, const char *colname)
{
  errtable(rel);
  err_generic_string(PG_DIAG_COLUMN_NAME, colname);

  return 0;                                   
}

   
                                                                             
                                                               
   
int
errtableconstraint(Relation rel, const char *conname)
{
  errtable(rel);
  err_generic_string(PG_DIAG_CONSTRAINT_NAME, conname);

  return 0;                                   
}

   
                                                     
   
                                                                       
                                                                        
                                                           
   
                                                                     
                                                                      
                                                                    
                                                 
   
                                                             
   
                                                                       
                                                   
   
                                                                          
                                                                    
                                                                   
   
                                                                     
                                                                    
                                                               
   
                                                                       
                                                                       
   
                                                                       
                                                                      
                                           
   
                                                                   
                                                                        
                                                                         
                                                                      
                                                         
   
                                                                       
                                                                     
                                                                          
                                                                    
   

   
                                                                    
                            
   
                                                                
                                         
                                    
   
                                                                    
   
static bool
load_relcache_init_file(bool shared)
{
  FILE *fp;
  char initfilename[MAXPGPATH];
  Relation *rels;
  int relno, num_rels, max_rels, nailed_rels, nailed_indexes, magic;
  int i;

  if (shared)
  {
    snprintf(initfilename, sizeof(initfilename), "global/%s", RELCACHE_INIT_FILENAME);
  }
  else
  {
    snprintf(initfilename, sizeof(initfilename), "%s/%s", DatabasePath, RELCACHE_INIT_FILENAME);
  }

  fp = AllocateFile(initfilename, PG_BINARY_R);
  if (fp == NULL)
  {
    return false;
  }

     
                                                                            
                                                                        
                                               
     
  max_rels = 100;
  rels = (Relation *)palloc(max_rels * sizeof(Relation));
  num_rels = 0;
  nailed_rels = nailed_indexes = 0;

                                                           
  if (fread(&magic, 1, sizeof(magic), fp) != sizeof(magic))
  {
    goto read_failed;
  }
  if (magic != RELCACHE_INIT_FILEMAGIC)
  {
    goto read_failed;
  }

  for (relno = 0;; relno++)
  {
    Size len;
    size_t nread;
    Relation rel;
    Form_pg_class relform;
    bool has_not_null;

                                                   
    nread = fread(&len, 1, sizeof(len), fp);
    if (nread != sizeof(len))
    {
      if (nread == 0)
      {
        break;                  
      }
      goto read_failed;
    }

                                                       
    if (len != sizeof(RelationData))
    {
      goto read_failed;
    }

                                          
    if (num_rels >= max_rels)
    {
      max_rels *= 2;
      rels = (Relation *)repalloc(rels, max_rels * sizeof(Relation));
    }

    rel = rels[num_rels++] = (Relation)palloc(len);

                                           
    if (fread(rel, 1, len, fp) != len)
    {
      goto read_failed;
    }

                                           
    if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
    {
      goto read_failed;
    }

    relform = (Form_pg_class)palloc(len);
    if (fread(relform, 1, len, fp) != len)
    {
      goto read_failed;
    }

    rel->rd_rel = relform;

                                          
    rel->rd_att = CreateTemplateTupleDesc(relform->relnatts);
    rel->rd_att->tdrefcount = 1;                         

    rel->rd_att->tdtypeid = relform->reltype;
    rel->rd_att->tdtypmod = -1;                          

                                                             
    has_not_null = false;
    for (i = 0; i < relform->relnatts; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(rel->rd_att, i);

      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }
      if (len != ATTRIBUTE_FIXED_PART_SIZE)
      {
        goto read_failed;
      }
      if (fread(attr, 1, len, fp) != len)
      {
        goto read_failed;
      }

      has_not_null |= attr->attnotnull;
    }

                                                    
    if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
    {
      goto read_failed;
    }
    if (len > 0)
    {
      rel->rd_options = palloc(len);
      if (fread(rel->rd_options, 1, len, fp) != len)
      {
        goto read_failed;
      }
      if (len != VARSIZE(rel->rd_options))
      {
        goto read_failed;                   
      }
    }
    else
    {
      rel->rd_options = NULL;
    }

                              
    if (has_not_null)
    {
      TupleConstr *constr = (TupleConstr *)palloc0(sizeof(TupleConstr));

      constr->has_not_null = true;
      rel->rd_att->constr = constr;
    }

       
                                                                        
                                 
       
    if (rel->rd_rel->relkind == RELKIND_INDEX)
    {
      MemoryContext indexcxt;
      Oid *opfamily;
      Oid *opcintype;
      RegProcedure *support;
      int nsupport;
      int16 *indoption;
      Oid *indcollation;

                                                          
      if (rel->rd_isnailed)
      {
        nailed_indexes++;
      }

                                         
      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }

      rel->rd_indextuple = (HeapTuple)palloc(len);
      if (fread(rel->rd_indextuple, 1, len, fp) != len)
      {
        goto read_failed;
      }

                                                                       
      rel->rd_indextuple->t_data = (HeapTupleHeader)((char *)rel->rd_indextuple + HEAPTUPLESIZE);
      rel->rd_index = (Form_pg_index)GETSTRUCT(rel->rd_indextuple);

         
                                                                
                                     
         
      indexcxt = AllocSetContextCreate(CacheMemoryContext, "index info", ALLOCSET_SMALL_SIZES);
      rel->rd_indexcxt = indexcxt;
      MemoryContextCopyAndSetIdentifier(indexcxt, RelationGetRelationName(rel));

         
                                                                      
                                                                         
                                                                         
                                                                       
         
      InitIndexAmRoutine(rel);

                                                  
      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }

      opfamily = (Oid *)MemoryContextAlloc(indexcxt, len);
      if (fread(opfamily, 1, len, fp) != len)
      {
        goto read_failed;
      }

      rel->rd_opfamily = opfamily;

                                                   
      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }

      opcintype = (Oid *)MemoryContextAlloc(indexcxt, len);
      if (fread(opcintype, 1, len, fp) != len)
      {
        goto read_failed;
      }

      rel->rd_opcintype = opcintype;

                                                           
      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }
      support = (RegProcedure *)MemoryContextAlloc(indexcxt, len);
      if (fread(support, 1, len, fp) != len)
      {
        goto read_failed;
      }

      rel->rd_support = support;

                                                   
      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }

      indcollation = (Oid *)MemoryContextAlloc(indexcxt, len);
      if (fread(indcollation, 1, len, fp) != len)
      {
        goto read_failed;
      }

      rel->rd_indcollation = indcollation;

                                                        
      if (fread(&len, 1, sizeof(len), fp) != sizeof(len))
      {
        goto read_failed;
      }

      indoption = (int16 *)MemoryContextAlloc(indexcxt, len);
      if (fread(indoption, 1, len, fp) != len)
      {
        goto read_failed;
      }

      rel->rd_indoption = indoption;

                                          
      nsupport = relform->relnatts * rel->rd_indam->amsupport;
      rel->rd_supportinfo = (FmgrInfo *)MemoryContextAllocZero(indexcxt, nsupport * sizeof(FmgrInfo));
    }
    else
    {
                                                       
      if (rel->rd_isnailed)
      {
        nailed_rels++;
      }

                              
      if (rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_SEQUENCE || rel->rd_rel->relkind == RELKIND_TOASTVALUE || rel->rd_rel->relkind == RELKIND_MATVIEW)
      {
        RelationInitTableAccessMethod(rel);
      }

      Assert(rel->rd_index == NULL);
      Assert(rel->rd_indextuple == NULL);
      Assert(rel->rd_indexcxt == NULL);
      Assert(rel->rd_indam == NULL);
      Assert(rel->rd_opfamily == NULL);
      Assert(rel->rd_opcintype == NULL);
      Assert(rel->rd_support == NULL);
      Assert(rel->rd_supportinfo == NULL);
      Assert(rel->rd_indoption == NULL);
      Assert(rel->rd_indcollation == NULL);
    }

       
                                                                     
                                                                          
                                                                         
                                                                           
                                                                           
                                     
       
    rel->rd_rules = NULL;
    rel->rd_rulescxt = NULL;
    rel->trigdesc = NULL;
    rel->rd_rsdesc = NULL;
    rel->rd_partkey = NULL;
    rel->rd_partkeycxt = NULL;
    rel->rd_partdesc = NULL;
    rel->rd_pdcxt = NULL;
    rel->rd_partcheck = NIL;
    rel->rd_partcheckvalid = false;
    rel->rd_partcheckcxt = NULL;
    rel->rd_indexprs = NIL;
    rel->rd_indpred = NIL;
    rel->rd_exclops = NULL;
    rel->rd_exclprocs = NULL;
    rel->rd_exclstrats = NULL;
    rel->rd_fdwroutine = NULL;

       
                                                          
       
    rel->rd_smgr = NULL;
    if (rel->rd_isnailed)
    {
      rel->rd_refcnt = 1;
    }
    else
    {
      rel->rd_refcnt = 0;
    }
    rel->rd_indexvalid = false;
    rel->rd_indexlist = NIL;
    rel->rd_pkindex = InvalidOid;
    rel->rd_replidindex = InvalidOid;
    rel->rd_indexattr = NULL;
    rel->rd_keyattr = NULL;
    rel->rd_pkattr = NULL;
    rel->rd_idattr = NULL;
    rel->rd_pubactions = NULL;
    rel->rd_statvalid = false;
    rel->rd_statlist = NIL;
    rel->rd_fkeyvalid = false;
    rel->rd_fkeylist = NIL;
    rel->rd_createSubid = InvalidSubTransactionId;
    rel->rd_newRelfilenodeSubid = InvalidSubTransactionId;
    rel->rd_amcache = NULL;
    MemSet(&rel->pgstat_info, 0, sizeof(rel->pgstat_info));

       
                                                                       
                                                                          
                           
       
    RelationInitLockInfo(rel);
    RelationInitPhysicalAddr(rel);
  }

     
                                                                           
                                                                           
                                                                             
                                                                         
     
                                                                            
                                                                         
                                                                          
                                                                            
                             
     
  if (shared)
  {
    if (nailed_rels != NUM_CRITICAL_SHARED_RELS || nailed_indexes != NUM_CRITICAL_SHARED_INDEXES)
    {
      elog(WARNING, "found %d nailed shared rels and %d nailed shared indexes in init file, but expected %d and %d respectively", nailed_rels, nailed_indexes, NUM_CRITICAL_SHARED_RELS, NUM_CRITICAL_SHARED_INDEXES);
                                                             
      Assert(false);
                                                                       
      goto read_failed;
    }
  }
  else
  {
    if (nailed_rels != NUM_CRITICAL_LOCAL_RELS || nailed_indexes != NUM_CRITICAL_LOCAL_INDEXES)
    {
      elog(WARNING, "found %d nailed rels and %d nailed indexes in init file, but expected %d and %d respectively", nailed_rels, nailed_indexes, NUM_CRITICAL_LOCAL_RELS, NUM_CRITICAL_LOCAL_INDEXES);
                                                  
      goto read_failed;
    }
  }

     
                           
     
                                                             
     
  for (relno = 0; relno < num_rels; relno++)
  {
    RelationCacheInsert(rels[relno], false);
  }

  pfree(rels);
  FreeFile(fp);

  if (shared)
  {
    criticalSharedRelcachesBuilt = true;
  }
  else
  {
    criticalRelcachesBuilt = true;
  }
  return true;

     
                                                                            
                                                                        
                 
     
read_failed:
  pfree(rels);
  FreeFile(fp);

  return false;
}

   
                                                                 
                                                                     
   
static void
write_relcache_init_file(bool shared)
{
  FILE *fp;
  char tempfilename[MAXPGPATH];
  char finalfilename[MAXPGPATH];
  int magic;
  HASH_SEQ_STATUS status;
  RelIdCacheEnt *idhentry;
  int i;

     
                                                                       
                                                                  
     
  if (relcacheInvalsReceived != 0L)
  {
    return;
  }

     
                                                                         
                                                                           
                                       
     
  if (shared)
  {
    snprintf(tempfilename, sizeof(tempfilename), "global/%s.%d", RELCACHE_INIT_FILENAME, MyProcPid);
    snprintf(finalfilename, sizeof(finalfilename), "global/%s", RELCACHE_INIT_FILENAME);
  }
  else
  {
    snprintf(tempfilename, sizeof(tempfilename), "%s/%s.%d", DatabasePath, RELCACHE_INIT_FILENAME, MyProcPid);
    snprintf(finalfilename, sizeof(finalfilename), "%s/%s", DatabasePath, RELCACHE_INIT_FILENAME);
  }

  unlink(tempfilename);                                            

  fp = AllocateFile(tempfilename, PG_BINARY_W);
  if (fp == NULL)
  {
       
                                                                    
                                         
       
    ereport(WARNING, (errcode_for_file_access(), errmsg("could not create relation-cache initialization file \"%s\": %m", tempfilename), errdetail("Continuing anyway, but there's something wrong.")));
    return;
  }

     
                                                                         
                                                                   
     
  magic = RELCACHE_INIT_FILEMAGIC;
  if (fwrite(&magic, 1, sizeof(magic), fp) != sizeof(magic))
  {
    elog(FATAL, "could not write init file");
  }

     
                                                                  
     
  hash_seq_init(&status, RelationIdCache);

  while ((idhentry = (RelIdCacheEnt *)hash_seq_search(&status)) != NULL)
  {
    Relation rel = idhentry->reldesc;
    Form_pg_class relform = rel->rd_rel;

                                     
    if (relform->relisshared != shared)
    {
      continue;
    }

       
                                                                           
                                                                         
                                                                       
                                                                          
                                                                         
                                                                     
                                                                          
                                                  
       
    if (!shared && !RelationIdIsInInitFile(RelationGetRelid(rel)))
    {
                                              
      Assert(!rel->rd_isnailed);
      continue;
    }

                                               
    write_item(rel, sizeof(RelationData), fp);

                                            
    write_item(relform, CLASS_TUPLE_SIZE, fp);

                                                            
    for (i = 0; i < relform->relnatts; i++)
    {
      write_item(TupleDescAttr(rel->rd_att, i), ATTRIBUTE_FIXED_PART_SIZE, fp);
    }

                                                   
    write_item(rel->rd_options, (rel->rd_options ? VARSIZE(rel->rd_options) : 0), fp);

       
                                                                       
                                 
       
    if (rel->rd_rel->relkind == RELKIND_INDEX)
    {
                                    
                                                         
      write_item(rel->rd_indextuple, HEAPTUPLESIZE + rel->rd_indextuple->t_len, fp);

                                                   
      write_item(rel->rd_opfamily, relform->relnatts * sizeof(Oid), fp);

                                                    
      write_item(rel->rd_opcintype, relform->relnatts * sizeof(Oid), fp);

                                                            
      write_item(rel->rd_support, relform->relnatts * (rel->rd_indam->amsupport * sizeof(RegProcedure)), fp);

                                                    
      write_item(rel->rd_indcollation, relform->relnatts * sizeof(Oid), fp);

                                                         
      write_item(rel->rd_indoption, relform->relnatts * sizeof(int16), fp);
    }
  }

  if (FreeFile(fp))
  {
    elog(FATAL, "could not write init file");
  }

     
                                                                  
                                                                          
                                                                           
                                                                       
                                                                            
                                
     
                                                                           
                                                                           
     
  LWLockAcquire(RelCacheInitLock, LW_EXCLUSIVE);

                                                       
  AcceptInvalidationMessages();

     
                                                                            
                                           
     
  if (relcacheInvalsReceived == 0L)
  {
       
                                                                
                                      
       
                                                                    
                                                                          
                                                                         
                        
       
    if (rename(tempfilename, finalfilename) < 0)
    {
      unlink(tempfilename);
    }
  }
  else
  {
                                               
    unlink(tempfilename);
  }

  LWLockRelease(RelCacheInitLock);
}

                                                  
static void
write_item(const void *data, Size len, FILE *fp)
{
  if (fwrite(&len, 1, sizeof(len), fp) != sizeof(len))
  {
    elog(FATAL, "could not write init file");
  }
  if (len > 0 && fwrite(data, 1, len, fp) != len)
  {
    elog(FATAL, "could not write init file");
  }
}

   
                                                                             
                                            
   
                                                                               
                                                                              
                                                        
                                                                               
                                      
   
bool
RelationIdIsInInitFile(Oid relationId)
{
  if (relationId == SharedSecLabelRelationId || relationId == TriggerRelidNameIndexId || relationId == DatabaseNameIndexId || relationId == SharedSecLabelObjectIndexId)
  {
       
                                                                       
                
       
    Assert(!RelationSupportsSysCache(relationId));
    return true;
  }
  return RelationSupportsSysCache(relationId);
}

   
                                                                         
                                                                          
                    
   
                                                                           
                                                                           
                                                                            
                                                                       
                                                                           
                                                                      
                                                                              
                                                                              
                                                                           
                                                                             
                                                                              
          
   
                                                                             
                                                                              
                                                     
   
void
RelationCacheInitFilePreInvalidate(void)
{
  char localinitfname[MAXPGPATH];
  char sharedinitfname[MAXPGPATH];

  if (DatabasePath)
  {
    snprintf(localinitfname, sizeof(localinitfname), "%s/%s", DatabasePath, RELCACHE_INIT_FILENAME);
  }
  snprintf(sharedinitfname, sizeof(sharedinitfname), "global/%s", RELCACHE_INIT_FILENAME);

  LWLockAcquire(RelCacheInitLock, LW_EXCLUSIVE);

     
                                                                           
                                                                       
                                                                           
                                                       
     
  if (DatabasePath)
  {
    unlink_initfile(localinitfname, ERROR);
  }
  unlink_initfile(sharedinitfname, ERROR);
}

void
RelationCacheInitFilePostInvalidate(void)
{
  LWLockRelease(RelCacheInitLock);
}

   
                                                    
   
                                                                              
                                                                            
                                                                           
                                                                             
                                                                         
   
void
RelationCacheInitFileRemove(void)
{
  const char *tblspcdir = "pg_tblspc";
  DIR *dir;
  struct dirent *de;
  char path[MAXPGPATH + 10 + sizeof(TABLESPACE_VERSION_DIRECTORY)];

  snprintf(path, sizeof(path), "global/%s", RELCACHE_INIT_FILENAME);
  unlink_initfile(path, LOG);

                                                 
  RelationCacheInitFileRemoveInDir("base");

                                                                          
  dir = AllocateDir(tblspcdir);

  while ((de = ReadDirExtended(dir, tblspcdir, LOG)) != NULL)
  {
    if (strspn(de->d_name, "0123456789") == strlen(de->d_name))
    {
                                                         
      snprintf(path, sizeof(path), "%s/%s/%s", tblspcdir, de->d_name, TABLESPACE_VERSION_DIRECTORY);
      RelationCacheInitFileRemoveInDir(path);
    }
  }

  FreeDir(dir);
}

                                                                          
static void
RelationCacheInitFileRemoveInDir(const char *tblspcpath)
{
  DIR *dir;
  struct dirent *de;
  char initfilename[MAXPGPATH * 2];

                                                                      
  dir = AllocateDir(tblspcpath);

  while ((de = ReadDirExtended(dir, tblspcpath, LOG)) != NULL)
  {
    if (strspn(de->d_name, "0123456789") == strlen(de->d_name))
    {
                                                        
      snprintf(initfilename, sizeof(initfilename), "%s/%s/%s", tblspcpath, de->d_name, RELCACHE_INIT_FILENAME);
      unlink_initfile(initfilename, LOG);
    }
  }

  FreeDir(dir);
}

static void
unlink_initfile(const char *initfilename, int elevel)
{
  if (unlink(initfilename) < 0)
  {
                                                                    
    if (errno != ENOENT)
    {
      ereport(elevel, (errcode_for_file_access(), errmsg("could not remove cache file \"%s\": %m", initfilename)));
    }
  }
}
