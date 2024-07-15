                                                                            
   
              
                                      
   
                                                                         
                                                                        
   
   
                  
                                        
   
         
                                                                 
                                                           
   
                                                      
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "catalog/indexing.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_db_role_setting.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_description.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_partitioned_table.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_publication_rel.h"
#include "catalog/pg_range.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_seclabel.h"
#include "catalog/pg_sequence.h"
#include "catalog/pg_shdepend.h"
#include "catalog/pg_shdescription.h"
#include "catalog/pg_shseclabel.h"
#include "catalog/pg_replication_origin.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_config_map.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "utils/rel.h"
#include "utils/catcache.h"
#include "utils/syscache.h"

                                                                              
 
                              
 
                                                                    
                                             
 
                                                                           
                                                                                
                                                                                
                      
 
                                                                             
                                                                                
                                   
 
                                                                            
                                                                          
                                                                                
                                                                             
                                                                      
                                                           
 
                                                               
                                                                              
                                                                              
 
                                                                             
  

   
                                                             
   
struct cachedesc
{
  Oid reloid;                                         
  Oid indoid;                                             
  int nkeys;                                           
  int key[4];                                       
  int nbuckets;                                            
};

static const struct cachedesc cacheinfo[] = {{AggregateRelationId,               
                                                 AggregateFnoidIndexId, 1, {Anum_pg_aggregate_aggfnoid, 0, 0, 0}, 16},
    {AccessMethodRelationId,             
        AmNameIndexId, 1, {Anum_pg_am_amname, 0, 0, 0}, 4},
    {AccessMethodRelationId,            
        AmOidIndexId, 1, {Anum_pg_am_oid, 0, 0, 0}, 4},
    {AccessMethodOperatorRelationId,               
        AccessMethodOperatorIndexId, 3, {Anum_pg_amop_amopopr, Anum_pg_amop_amoppurpose, Anum_pg_amop_amopfamily, 0}, 64},
    {AccessMethodOperatorRelationId,                   
        AccessMethodStrategyIndexId, 4, {Anum_pg_amop_amopfamily, Anum_pg_amop_amoplefttype, Anum_pg_amop_amoprighttype, Anum_pg_amop_amopstrategy}, 64},
    {AccessMethodProcedureRelationId,                
        AccessMethodProcedureIndexId, 4, {Anum_pg_amproc_amprocfamily, Anum_pg_amproc_amproclefttype, Anum_pg_amproc_amprocrighttype, Anum_pg_amproc_amprocnum}, 16},
    {AttributeRelationId,              
        AttributeRelidNameIndexId, 2, {Anum_pg_attribute_attrelid, Anum_pg_attribute_attname, 0, 0}, 32},
    {AttributeRelationId,             
        AttributeRelidNumIndexId, 2, {Anum_pg_attribute_attrelid, Anum_pg_attribute_attnum, 0, 0}, 128},
    {AuthMemRelationId,                     
        AuthMemMemRoleIndexId, 2, {Anum_pg_auth_members_member, Anum_pg_auth_members_roleid, 0, 0}, 8},
    {AuthMemRelationId,                     
        AuthMemRoleMemIndexId, 2, {Anum_pg_auth_members_roleid, Anum_pg_auth_members_member, 0, 0}, 8},
    {AuthIdRelationId,               
        AuthIdRolnameIndexId, 1, {Anum_pg_authid_rolname, 0, 0, 0}, 8},
    {AuthIdRelationId,              
        AuthIdOidIndexId, 1, {Anum_pg_authid_oid, 0, 0, 0}, 8},
    {CastRelationId,                       
        CastSourceTargetIndexId, 2, {Anum_pg_cast_castsource, Anum_pg_cast_casttarget, 0, 0}, 256},
    {OperatorClassRelationId,                   
        OpclassAmNameNspIndexId, 3, {Anum_pg_opclass_opcmethod, Anum_pg_opclass_opcname, Anum_pg_opclass_opcnamespace, 0}, 8},
    {OperatorClassRelationId,             
        OpclassOidIndexId, 1, {Anum_pg_opclass_oid, 0, 0, 0}, 8},
    {CollationRelationId,                     
        CollationNameEncNspIndexId, 3, {Anum_pg_collation_collname, Anum_pg_collation_collencoding, Anum_pg_collation_collnamespace, 0}, 8},
    {CollationRelationId,              
        CollationOidIndexId, 1, {Anum_pg_collation_oid, 0, 0, 0}, 8},
    {ConversionRelationId,                 
        ConversionDefaultIndexId, 4, {Anum_pg_conversion_connamespace, Anum_pg_conversion_conforencoding, Anum_pg_conversion_contoencoding, Anum_pg_conversion_oid}, 8},
    {ConversionRelationId,                 
        ConversionNameNspIndexId, 2, {Anum_pg_conversion_conname, Anum_pg_conversion_connamespace, 0, 0}, 8},
    {ConstraintRelationId,                
        ConstraintOidIndexId, 1, {Anum_pg_constraint_oid, 0, 0, 0}, 16},
    {ConversionRelationId,              
        ConversionOidIndexId, 1, {Anum_pg_conversion_oid, 0, 0, 0}, 8},
    {DatabaseRelationId,                  
        DatabaseOidIndexId, 1, {Anum_pg_database_oid, 0, 0, 0}, 4},
    {DefaultAclRelationId,                       
        DefaultAclRoleNspObjIndexId, 3, {Anum_pg_default_acl_defaclrole, Anum_pg_default_acl_defaclnamespace, Anum_pg_default_acl_defaclobjtype, 0}, 8},
    {EnumRelationId,              
        EnumOidIndexId, 1, {Anum_pg_enum_oid, 0, 0, 0}, 8},
    {EnumRelationId,                     
        EnumTypIdLabelIndexId, 2, {Anum_pg_enum_enumtypid, Anum_pg_enum_enumlabel, 0, 0}, 8},
    {EventTriggerRelationId,                       
        EventTriggerNameIndexId, 1, {Anum_pg_event_trigger_evtname, 0, 0, 0}, 8},
    {EventTriggerRelationId,                      
        EventTriggerOidIndexId, 1, {Anum_pg_event_trigger_oid, 0, 0, 0}, 8},
    {ForeignDataWrapperRelationId,                             
        ForeignDataWrapperNameIndexId, 1, {Anum_pg_foreign_data_wrapper_fdwname, 0, 0, 0}, 2},
    {ForeignDataWrapperRelationId,                            
        ForeignDataWrapperOidIndexId, 1, {Anum_pg_foreign_data_wrapper_oid, 0, 0, 0}, 2},
    {ForeignServerRelationId,                        
        ForeignServerNameIndexId, 1, {Anum_pg_foreign_server_srvname, 0, 0, 0}, 2},
    {ForeignServerRelationId,                       
        ForeignServerOidIndexId, 1, {Anum_pg_foreign_server_oid, 0, 0, 0}, 2},
    {ForeignTableRelationId,                      
        ForeignTableRelidIndexId, 1, {Anum_pg_foreign_table_ftrelid, 0, 0, 0}, 4},
    {IndexRelationId,                 
        IndexRelidIndexId, 1, {Anum_pg_index_indexrelid, 0, 0, 0}, 64},
    {LanguageRelationId,               
        LanguageNameIndexId, 1, {Anum_pg_language_lanname, 0, 0, 0}, 4},
    {LanguageRelationId,              
        LanguageOidIndexId, 1, {Anum_pg_language_oid, 0, 0, 0}, 4},
    {NamespaceRelationId,                    
        NamespaceNameIndexId, 1, {Anum_pg_namespace_nspname, 0, 0, 0}, 4},
    {NamespaceRelationId,                   
        NamespaceOidIndexId, 1, {Anum_pg_namespace_oid, 0, 0, 0}, 16},
    {OperatorRelationId,                  
        OperatorNameNspIndexId, 4, {Anum_pg_operator_oprname, Anum_pg_operator_oprleft, Anum_pg_operator_oprright, Anum_pg_operator_oprnamespace}, 256},
    {OperatorRelationId,              
        OperatorOidIndexId, 1, {Anum_pg_operator_oid, 0, 0, 0}, 32},
    {OperatorFamilyRelationId,                        
        OpfamilyAmNameNspIndexId, 3, {Anum_pg_opfamily_opfmethod, Anum_pg_opfamily_opfname, Anum_pg_opfamily_opfnamespace, 0}, 8},
    {OperatorFamilyRelationId,                  
        OpfamilyOidIndexId, 1, {Anum_pg_opfamily_oid, 0, 0, 0}, 8},
    {PartitionedRelationId,                
        PartitionedRelidIndexId, 1, {Anum_pg_partitioned_table_partrelid, 0, 0, 0}, 32},
    {ProcedureRelationId,                      
        ProcedureNameArgsNspIndexId, 3, {Anum_pg_proc_proname, Anum_pg_proc_proargtypes, Anum_pg_proc_pronamespace, 0}, 128},
    {ProcedureRelationId,              
        ProcedureOidIndexId, 1, {Anum_pg_proc_oid, 0, 0, 0}, 128},
    {PublicationRelationId,                      
        PublicationNameIndexId, 1, {Anum_pg_publication_pubname, 0, 0, 0}, 8},
    {PublicationRelationId,                     
        PublicationObjectIndexId, 1, {Anum_pg_publication_oid, 0, 0, 0}, 8},
    {PublicationRelRelationId,                     
        PublicationRelObjectIndexId, 1, {Anum_pg_publication_rel_oid, 0, 0, 0}, 64},
    {PublicationRelRelationId,                        
        PublicationRelPrrelidPrpubidIndexId, 2, {Anum_pg_publication_rel_prrelid, Anum_pg_publication_rel_prpubid, 0, 0}, 64},
    {RangeRelationId,                
        RangeTypidIndexId, 1, {Anum_pg_range_rngtypid, 0, 0, 0}, 4},
    {RelationRelationId,                 
        ClassNameNspIndexId, 2, {Anum_pg_class_relname, Anum_pg_class_relnamespace, 0, 0}, 128},
    {RelationRelationId,             
        ClassOidIndexId, 1, {Anum_pg_class_oid, 0, 0, 0}, 128},
    {ReplicationOriginRelationId,                    
        ReplicationOriginIdentIndex, 1, {Anum_pg_replication_origin_roident, 0, 0, 0}, 16},
    {ReplicationOriginRelationId,                   
        ReplicationOriginNameIndex, 1, {Anum_pg_replication_origin_roname, 0, 0, 0}, 16},
    {RewriteRelationId,                  
        RewriteRelRulenameIndexId, 2, {Anum_pg_rewrite_ev_class, Anum_pg_rewrite_rulename, 0, 0}, 8},
    {SequenceRelationId,               
        SequenceRelidIndexId, 1, {Anum_pg_sequence_seqrelid, 0, 0, 0}, 32},
    {StatisticExtDataRelationId,                        
        StatisticExtDataStxoidIndexId, 1, {Anum_pg_statistic_ext_data_stxoid, 0, 0, 0}, 4},
    {StatisticExtRelationId,                     
        StatisticExtNameIndexId, 2, {Anum_pg_statistic_ext_stxname, Anum_pg_statistic_ext_stxnamespace, 0, 0}, 4},
    {StatisticExtRelationId,                 
        StatisticExtOidIndexId, 1, {Anum_pg_statistic_ext_oid, 0, 0, 0}, 4},
    {StatisticRelationId,                    
        StatisticRelidAttnumInhIndexId, 3, {Anum_pg_statistic_starelid, Anum_pg_statistic_staattnum, Anum_pg_statistic_stainherit, 0}, 128},
    {SubscriptionRelationId,                       
        SubscriptionNameIndexId, 2, {Anum_pg_subscription_subdbid, Anum_pg_subscription_subname, 0, 0}, 4},
    {SubscriptionRelationId,                      
        SubscriptionObjectIndexId, 1, {Anum_pg_subscription_oid, 0, 0, 0}, 4},
    {SubscriptionRelRelationId,                         
        SubscriptionRelSrrelidSrsubidIndexId, 2, {Anum_pg_subscription_rel_srrelid, Anum_pg_subscription_rel_srsubid, 0, 0}, 64},
    {TableSpaceRelationId,                    
        TablespaceOidIndexId, 1,
        {
            Anum_pg_tablespace_oid,
            0,
            0,
            0,
        },
        4},
    {TransformRelationId,             
        TransformOidIndexId, 1,
        {
            Anum_pg_transform_oid,
            0,
            0,
            0,
        },
        16},
    {TransformRelationId,                  
        TransformTypeLangIndexId, 2,
        {
            Anum_pg_transform_trftype,
            Anum_pg_transform_trflang,
            0,
            0,
        },
        16},
    {TSConfigMapRelationId,                  
        TSConfigMapIndexId, 3, {Anum_pg_ts_config_map_mapcfg, Anum_pg_ts_config_map_maptokentype, Anum_pg_ts_config_map_mapseqno, 0}, 2},
    {TSConfigRelationId,                      
        TSConfigNameNspIndexId, 2, {Anum_pg_ts_config_cfgname, Anum_pg_ts_config_cfgnamespace, 0, 0}, 2},
    {TSConfigRelationId,                  
        TSConfigOidIndexId, 1, {Anum_pg_ts_config_oid, 0, 0, 0}, 2},
    {TSDictionaryRelationId,                    
        TSDictionaryNameNspIndexId, 2, {Anum_pg_ts_dict_dictname, Anum_pg_ts_dict_dictnamespace, 0, 0}, 2},
    {TSDictionaryRelationId,                
        TSDictionaryOidIndexId, 1, {Anum_pg_ts_dict_oid, 0, 0, 0}, 2},
    {TSParserRelationId,                      
        TSParserNameNspIndexId, 2, {Anum_pg_ts_parser_prsname, Anum_pg_ts_parser_prsnamespace, 0, 0}, 2},
    {TSParserRelationId,                  
        TSParserOidIndexId, 1, {Anum_pg_ts_parser_oid, 0, 0, 0}, 2},
    {TSTemplateRelationId,                        
        TSTemplateNameNspIndexId, 2, {Anum_pg_ts_template_tmplname, Anum_pg_ts_template_tmplnamespace, 0, 0}, 2},
    {TSTemplateRelationId,                    
        TSTemplateOidIndexId, 1, {Anum_pg_ts_template_oid, 0, 0, 0}, 2},
    {TypeRelationId,                  
        TypeNameNspIndexId, 2, {Anum_pg_type_typname, Anum_pg_type_typnamespace, 0, 0}, 64},
    {TypeRelationId,              
        TypeOidIndexId, 1, {Anum_pg_type_oid, 0, 0, 0}, 64},
    {UserMappingRelationId,                     
        UserMappingOidIndexId, 1, {Anum_pg_user_mapping_oid, 0, 0, 0}, 2},
    {UserMappingRelationId,                            
        UserMappingUserServerIndexId, 2, {Anum_pg_user_mapping_umuser, Anum_pg_user_mapping_umserver, 0, 0}, 2}};

static CatCache *SysCache[SysCacheSize];

static bool CacheInitialized = false;

                                                             
static Oid SysCacheRelationOid[SysCacheSize];
static int SysCacheRelationOidSize;

                                                               
static Oid SysCacheSupportingRelOid[SysCacheSize * 2];
static int SysCacheSupportingRelOidSize;

static int
oid_compare(const void *a, const void *b);

   
                                            
   
                                                                      
                                                                      
                                                                
                  
   
void
InitCatalogCache(void)
{
  int cacheId;
  int i, j;

  StaticAssertStmt(SysCacheSize == (int)lengthof(cacheinfo), "SysCacheSize does not match syscache.c's array");

  Assert(!CacheInitialized);

  SysCacheRelationOidSize = SysCacheSupportingRelOidSize = 0;

  for (cacheId = 0; cacheId < SysCacheSize; cacheId++)
  {
    SysCache[cacheId] = InitCatCache(cacheId, cacheinfo[cacheId].reloid, cacheinfo[cacheId].indoid, cacheinfo[cacheId].nkeys, cacheinfo[cacheId].key, cacheinfo[cacheId].nbuckets);
    if (!PointerIsValid(SysCache[cacheId]))
    {
      elog(ERROR, "could not initialize cache %u (%d)", cacheinfo[cacheId].reloid, cacheId);
    }
                                            
    SysCacheRelationOid[SysCacheRelationOidSize++] = cacheinfo[cacheId].reloid;
    SysCacheSupportingRelOid[SysCacheSupportingRelOidSize++] = cacheinfo[cacheId].reloid;
    SysCacheSupportingRelOid[SysCacheSupportingRelOidSize++] = cacheinfo[cacheId].indoid;
                                                           
    Assert(!RelationInvalidatesSnapshotsOnly(cacheinfo[cacheId].reloid));
  }

  Assert(SysCacheRelationOidSize <= lengthof(SysCacheRelationOid));
  Assert(SysCacheSupportingRelOidSize <= lengthof(SysCacheSupportingRelOid));

                                                                
  pg_qsort(SysCacheRelationOid, SysCacheRelationOidSize, sizeof(Oid), oid_compare);
  for (i = 1, j = 0; i < SysCacheRelationOidSize; i++)
  {
    if (SysCacheRelationOid[i] != SysCacheRelationOid[j])
    {
      SysCacheRelationOid[++j] = SysCacheRelationOid[i];
    }
  }
  SysCacheRelationOidSize = j + 1;

  pg_qsort(SysCacheSupportingRelOid, SysCacheSupportingRelOidSize, sizeof(Oid), oid_compare);
  for (i = 1, j = 0; i < SysCacheSupportingRelOidSize; i++)
  {
    if (SysCacheSupportingRelOid[i] != SysCacheSupportingRelOid[j])
    {
      SysCacheSupportingRelOid[++j] = SysCacheSupportingRelOid[i];
    }
  }
  SysCacheSupportingRelOidSize = j + 1;

  CacheInitialized = true;
}

   
                                                           
   
                                                                    
           
   
                                                                          
                                                                      
                                                                     
                                                                          
              
   
void
InitCatalogCachePhase2(void)
{
  int cacheId;

  Assert(CacheInitialized);

  for (cacheId = 0; cacheId < SysCacheSize; cacheId++)
  {
    InitCatCachePhase2(SysCache[cacheId], true);
  }
}

   
                  
   
                                                                     
                        
   
                                                                     
                                                           
   
                                                                   
                                                                        
                                                                   
                                                      
   
                                                                        
   
HeapTuple
SearchSysCache(int cacheId, Datum key1, Datum key2, Datum key3, Datum key4)
{
  Assert(cacheId >= 0 && cacheId < SysCacheSize && PointerIsValid(SysCache[cacheId]));

  return SearchCatCache(SysCache[cacheId], key1, key2, key3, key4);
}

HeapTuple
SearchSysCache1(int cacheId, Datum key1)
{
  Assert(cacheId >= 0 && cacheId < SysCacheSize && PointerIsValid(SysCache[cacheId]));
  Assert(SysCache[cacheId]->cc_nkeys == 1);

  return SearchCatCache1(SysCache[cacheId], key1);
}

HeapTuple
SearchSysCache2(int cacheId, Datum key1, Datum key2)
{
  Assert(cacheId >= 0 && cacheId < SysCacheSize && PointerIsValid(SysCache[cacheId]));
  Assert(SysCache[cacheId]->cc_nkeys == 2);

  return SearchCatCache2(SysCache[cacheId], key1, key2);
}

HeapTuple
SearchSysCache3(int cacheId, Datum key1, Datum key2, Datum key3)
{
  Assert(cacheId >= 0 && cacheId < SysCacheSize && PointerIsValid(SysCache[cacheId]));
  Assert(SysCache[cacheId]->cc_nkeys == 3);

  return SearchCatCache3(SysCache[cacheId], key1, key2, key3);
}

HeapTuple
SearchSysCache4(int cacheId, Datum key1, Datum key2, Datum key3, Datum key4)
{
  Assert(cacheId >= 0 && cacheId < SysCacheSize && PointerIsValid(SysCache[cacheId]));
  Assert(SysCache[cacheId]->cc_nkeys == 4);

  return SearchCatCache4(SysCache[cacheId], key1, key2, key3, key4);
}

   
                   
                                                          
   
void
ReleaseSysCache(HeapTuple tuple)
{
  ReleaseCatCache(tuple);
}

   
                      
   
                                                                      
                                                                  
                                                                   
                                                  
   
HeapTuple
SearchSysCacheCopy(int cacheId, Datum key1, Datum key2, Datum key3, Datum key4)
{
  HeapTuple tuple, newtuple;

  tuple = SearchSysCache(cacheId, key1, key2, key3, key4);
  if (!HeapTupleIsValid(tuple))
  {
    return tuple;
  }
  newtuple = heap_copytuple(tuple);
  ReleaseSysCache(tuple);
  return newtuple;
}

   
                        
   
                                                                          
                                              
   
bool
SearchSysCacheExists(int cacheId, Datum key1, Datum key2, Datum key3, Datum key4)
{
  HeapTuple tuple;

  tuple = SearchSysCache(cacheId, key1, key2, key3, key4);
  if (!HeapTupleIsValid(tuple))
  {
    return false;
  }
  ReleaseSysCache(tuple);
  return true;
}

   
                  
   
                                                                             
                                                                               
                                              
   
Oid
GetSysCacheOid(int cacheId, AttrNumber oidcol, Datum key1, Datum key2, Datum key3, Datum key4)
{
  HeapTuple tuple;
  bool isNull;
  Oid result;

  tuple = SearchSysCache(cacheId, key1, key2, key3, key4);
  if (!HeapTupleIsValid(tuple))
  {
    return InvalidOid;
  }
  result = heap_getattr(tuple, oidcol, SysCache[cacheId]->cc_tupdesc, &isNull);
  Assert(!isNull);                                                
  ReleaseSysCache(tuple);
  return result;
}

   
                         
   
                                                                      
                                                                    
                                                                     
                                          
   
HeapTuple
SearchSysCacheAttName(Oid relid, const char *attname)
{
  HeapTuple tuple;

  tuple = SearchSysCache2(ATTNAME, ObjectIdGetDatum(relid), CStringGetDatum(attname));
  if (!HeapTupleIsValid(tuple))
  {
    return NULL;
  }
  if (((Form_pg_attribute)GETSTRUCT(tuple))->attisdropped)
  {
    ReleaseSysCache(tuple);
    return NULL;
  }
  return tuple;
}

   
                             
   
                                                                  
   
HeapTuple
SearchSysCacheCopyAttName(Oid relid, const char *attname)
{
  HeapTuple tuple, newtuple;

  tuple = SearchSysCacheAttName(relid, attname);
  if (!HeapTupleIsValid(tuple))
  {
    return tuple;
  }
  newtuple = heap_copytuple(tuple);
  ReleaseSysCache(tuple);
  return newtuple;
}

   
                               
   
                                                                    
   
bool
SearchSysCacheExistsAttName(Oid relid, const char *attname)
{
  HeapTuple tuple;

  tuple = SearchSysCacheAttName(relid, attname);
  if (!HeapTupleIsValid(tuple))
  {
    return false;
  }
  ReleaseSysCache(tuple);
  return true;
}

   
                        
   
                                                                     
                                                                    
                                                                     
                                          
   
HeapTuple
SearchSysCacheAttNum(Oid relid, int16 attnum)
{
  HeapTuple tuple;

  tuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(tuple))
  {
    return NULL;
  }
  if (((Form_pg_attribute)GETSTRUCT(tuple))->attisdropped)
  {
    ReleaseSysCache(tuple);
    return NULL;
  }
  return tuple;
}

   
                            
   
                                                                  
   
HeapTuple
SearchSysCacheCopyAttNum(Oid relid, int16 attnum)
{
  HeapTuple tuple, newtuple;

  tuple = SearchSysCacheAttNum(relid, attnum);
  if (!HeapTupleIsValid(tuple))
  {
    return NULL;
  }
  newtuple = heap_copytuple(tuple);
  ReleaseSysCache(tuple);
  return newtuple;
}

   
                   
   
                                                          
                                  
   
                                                                 
                                                                          
                                                                       
                                                                           
                                       
   
                                                                           
                                                                           
                                  
   
                                                                         
                                                                      
   
Datum
SysCacheGetAttr(int cacheId, HeapTuple tup, AttrNumber attributeNumber, bool *isNull)
{
     
                                                                           
                                                                           
                                                                        
                                                                             
     
  if (cacheId < 0 || cacheId >= SysCacheSize || !PointerIsValid(SysCache[cacheId]))
  {
    elog(ERROR, "invalid cache ID: %d", cacheId);
  }
  if (!PointerIsValid(SysCache[cacheId]->cc_tupdesc))
  {
    InitCatCachePhase2(SysCache[cacheId], false);
    Assert(PointerIsValid(SysCache[cacheId]->cc_tupdesc));
  }

  return heap_getattr(tup, attributeNumber, SysCache[cacheId]->cc_tupdesc, isNull);
}

   
                        
   
                                                                            
                               
   
                                                                             
                                                                             
                                                                  
   
uint32
GetSysCacheHashValue(int cacheId, Datum key1, Datum key2, Datum key3, Datum key4)
{
  if (cacheId < 0 || cacheId >= SysCacheSize || !PointerIsValid(SysCache[cacheId]))
  {
    elog(ERROR, "invalid cache ID: %d", cacheId);
  }

  return GetCatCacheHashValue(SysCache[cacheId], key1, key2, key3, key4);
}

   
                         
   
struct catclist *
SearchSysCacheList(int cacheId, int nkeys, Datum key1, Datum key2, Datum key3)
{
  if (cacheId < 0 || cacheId >= SysCacheSize || !PointerIsValid(SysCache[cacheId]))
  {
    elog(ERROR, "invalid cache ID: %d", cacheId);
  }

  return SearchCatCacheList(SysCache[cacheId], nkeys, key1, key2, key3);
}

   
                      
   
                                                                  
                                           
   
                                                                         
   
void
SysCacheInvalidate(int cacheId, uint32 hashValue)
{
  if (cacheId < 0 || cacheId >= SysCacheSize)
  {
    elog(ERROR, "invalid cache ID: %d", cacheId);
  }

                                                                   
  if (!PointerIsValid(SysCache[cacheId]))
  {
    return;
  }

  CatCacheInvalidate(SysCache[cacheId], hashValue);
}

   
                                                                               
                                                                      
                                                                         
                                                                           
                                   
   
                                                                              
                                                                              
                                                                  
   
bool
RelationInvalidatesSnapshotsOnly(Oid relid)
{
  switch (relid)
  {
  case DbRoleSettingRelationId:
  case DependRelationId:
  case SharedDependRelationId:
  case DescriptionRelationId:
  case SharedDescriptionRelationId:
  case SecLabelRelationId:
  case SharedSecLabelRelationId:
    return true;
  default:
    break;
  }

  return false;
}

   
                                               
   
bool
RelationHasSysCache(Oid relid)
{
  int low = 0, high = SysCacheRelationOidSize - 1;

  while (low <= high)
  {
    int middle = low + (high - low) / 2;

    if (SysCacheRelationOid[middle] == relid)
    {
      return true;
    }
    if (SysCacheRelationOid[middle] < relid)
    {
      low = middle + 1;
    }
    else
    {
      high = middle - 1;
    }
  }

  return false;
}

   
                                                                      
                                               
   
bool
RelationSupportsSysCache(Oid relid)
{
  int low = 0, high = SysCacheSupportingRelOidSize - 1;

  while (low <= high)
  {
    int middle = low + (high - low) / 2;

    if (SysCacheSupportingRelOid[middle] == relid)
    {
      return true;
    }
    if (SysCacheSupportingRelOid[middle] < relid)
    {
      low = middle + 1;
    }
    else
    {
      high = middle - 1;
    }
  }

  return false;
}

   
                               
   
static int
oid_compare(const void *a, const void *b)
{
  Oid oa = *((const Oid *)a);
  Oid ob = *((const Oid *)b);

  if (oa == ob)
  {
    return 0;
  }
  return (oa > ob) ? 1 : -1;
}
