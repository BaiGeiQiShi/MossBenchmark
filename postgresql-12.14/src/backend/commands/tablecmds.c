                                                                            
   
               
                                                                      
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/multixact.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/tupconvert.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/partition.h"
#include "catalog/pg_am.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "catalog/toasting.h"
#include "commands/cluster.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/policy.h"
#include "commands/sequence.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "commands/typecmds.h"
#include "commands/user.h"
#include "executor/executor.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "optimizer/optimizer.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "parser/parser.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "pgstat.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/lock.h"
#include "storage/predicate.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/relcache.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"

   
                         
   
typedef struct OnCommitItem
{
  Oid relid;                                      
  OnCommitAction oncommit;                                

     
                                                               
                                                                             
                                                                         
                                                                          
                                                          
     
  SubTransactionId creating_subid;
  SubTransactionId deleting_subid;
} OnCommitItem;

static List *on_commits = NIL;

   
                                     
   
                                                                           
                                                                          
                                                                           
                                                                             
                                                                          
                                                                         
   
                                                                        
                                         
   

#define AT_PASS_UNSET -1                                 
#define AT_PASS_DROP 0                               
#define AT_PASS_ALTER_TYPE 1                        
#define AT_PASS_OLD_INDEX 2                               
#define AT_PASS_OLD_CONSTR 3                                  
                                                                        
#define AT_PASS_ADD_COL 4                    
#define AT_PASS_COL_ATTRS 5                                   
#define AT_PASS_ADD_INDEX 6                   
#define AT_PASS_ADD_CONSTR 7                                
#define AT_PASS_MISC 8                        
#define AT_NUM_PASSES 9

typedef struct AlteredTableInfo
{
                                                    
  Oid relid;                                  
  char relkind;                       
  TupleDesc oldDesc;                                        
                                                 
  List *subcmds[AT_NUM_PASSES];                             
                                                    
  List *constraints;                                     
  List *newvals;                                          
  bool verify_new_notnull;                                         
  int rewrite;                                                       
  Oid newTableSpace;                                                 
  bool chgPersistence;                                              
  char newrelpersistence;                           
  Expr *partition_constraint;                                      
                                                                   
  bool validate_default;
                                                                 
  List *changedConstraintOids;                                     
  List *changedConstraintDefs;                                 
  List *changedIndexOids;                                      
  List *changedIndexDefs;                                      
  char *replicaIdentityIndex;                                          
  char *clusterOnIndex;                                      
} AlteredTableInfo;

                                                                   
                                                          
typedef struct NewConstraint
{
  char *name;                                                 
  ConstrType contype;                         
  Oid refrelid;                                 
  Oid refindid;                                            
  Oid conid;                                                        
  Node *qual;                                                        
  ExprState *qualstate;                                     
} NewConstraint;

   
                                                                           
                                                                               
                                                                             
                                                                            
                                                                           
                                                                      
   
typedef struct NewColumnValue
{
  AttrNumber attnum;                      
  Expr *expr;                                      
  ExprState *exprstate;                      
  bool is_generated;                                       
} NewColumnValue;

   
                                               
   
struct dropmsgstrings
{
  char kind;
  int nonexistent_code;
  const char *nonexistent_msg;
  const char *skipping_msg;
  const char *nota_msg;
  const char *drophint_msg;
};

static const struct dropmsgstrings dropmsgstringarray[] = {{RELKIND_RELATION, ERRCODE_UNDEFINED_TABLE, gettext_noop("table \"%s\" does not exist"), gettext_noop("table \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a table"), gettext_noop("Use DROP TABLE to remove a table.")}, {RELKIND_SEQUENCE, ERRCODE_UNDEFINED_TABLE, gettext_noop("sequence \"%s\" does not exist"), gettext_noop("sequence \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a sequence"), gettext_noop("Use DROP SEQUENCE to remove a sequence.")}, {RELKIND_VIEW, ERRCODE_UNDEFINED_TABLE, gettext_noop("view \"%s\" does not exist"), gettext_noop("view \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a view"), gettext_noop("Use DROP VIEW to remove a view.")},
    {RELKIND_MATVIEW, ERRCODE_UNDEFINED_TABLE, gettext_noop("materialized view \"%s\" does not exist"), gettext_noop("materialized view \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a materialized view"), gettext_noop("Use DROP MATERIALIZED VIEW to remove a materialized view.")}, {RELKIND_INDEX, ERRCODE_UNDEFINED_OBJECT, gettext_noop("index \"%s\" does not exist"), gettext_noop("index \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not an index"), gettext_noop("Use DROP INDEX to remove an index.")}, {RELKIND_COMPOSITE_TYPE, ERRCODE_UNDEFINED_OBJECT, gettext_noop("type \"%s\" does not exist"), gettext_noop("type \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a type"), gettext_noop("Use DROP TYPE to remove a type.")},
    {RELKIND_FOREIGN_TABLE, ERRCODE_UNDEFINED_OBJECT, gettext_noop("foreign table \"%s\" does not exist"), gettext_noop("foreign table \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a foreign table"), gettext_noop("Use DROP FOREIGN TABLE to remove a foreign table.")}, {RELKIND_PARTITIONED_TABLE, ERRCODE_UNDEFINED_TABLE, gettext_noop("table \"%s\" does not exist"), gettext_noop("table \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not a table"), gettext_noop("Use DROP TABLE to remove a table.")}, {RELKIND_PARTITIONED_INDEX, ERRCODE_UNDEFINED_OBJECT, gettext_noop("index \"%s\" does not exist"), gettext_noop("index \"%s\" does not exist, skipping"), gettext_noop("\"%s\" is not an index"), gettext_noop("Use DROP INDEX to remove an index.")}, {'\0', 0, NULL, NULL, NULL, NULL}};

                                                                               
struct DropRelationCallbackState
{
                                                
  char expected_relkind;
  LOCKMODE heap_lockmode;
                                                                        
  Oid heapOid;
  Oid partParentOid;
                                                                        
  char actual_relkind;
  char actual_relpersistence;
};

                                                           
#define ATT_TABLE 0x0001
#define ATT_VIEW 0x0002
#define ATT_MATVIEW 0x0004
#define ATT_INDEX 0x0008
#define ATT_COMPOSITE_TYPE 0x0010
#define ATT_FOREIGN_TABLE 0x0020
#define ATT_PARTITIONED_INDEX 0x0040

   
                                                                           
                                                                      
                                                             
   
#define child_dependency_type(child_is_partition) ((child_is_partition) ? DEPENDENCY_AUTO : DEPENDENCY_NORMAL)

static void
truncate_check_rel(Oid relid, Form_pg_class reltuple);
static void
truncate_check_activity(Relation rel);
static void
RangeVarCallbackForTruncate(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg);
static List *
MergeAttributes(List *schema, List *supers, char relpersistence, bool is_partition, List **supconstr);
static bool
MergeCheckConstraint(List *constraints, char *name, Node *expr);
static void
MergeAttributesIntoExisting(Relation child_rel, Relation parent_rel);
static void
MergeConstraintsIntoExisting(Relation child_rel, Relation parent_rel);
static void
StoreCatalogInheritance(Oid relationId, List *supers, bool child_is_partition);
static void
StoreCatalogInheritance1(Oid relationId, Oid parentOid, int32 seqNumber, Relation inhRelation, bool child_is_partition);
static int
findAttrByName(const char *attributeName, List *schema);
static void
AlterIndexNamespaces(Relation classRel, Relation rel, Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved);
static void
AlterSeqNamespaces(Relation classRel, Relation rel, Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved, LOCKMODE lockmode);
static ObjectAddress
ATExecAlterConstraint(Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing, LOCKMODE lockmode);
static bool
ATExecAlterConstrRecurse(Constraint *cmdcon, Relation conrel, Relation tgrel, Relation rel, HeapTuple contuple, List **otherrelids, LOCKMODE lockmode);
static ObjectAddress
ATExecValidateConstraint(List **wqueue, Relation rel, char *constrName, bool recurse, bool recursing, LOCKMODE lockmode);
static int
transformColumnNameList(Oid relId, List *colList, int16 *attnums, Oid *atttypids);
static int
transformFkeyGetPrimaryKey(Relation pkrel, Oid *indexOid, List **attnamelist, int16 *attnums, Oid *atttypids, Oid *opclasses);
static Oid
transformFkeyCheckAttrs(Relation pkrel, int numattrs, int16 *attnums, Oid *opclasses);
static void
checkFkeyPermissions(Relation rel, int16 *attnums, int natts);
static CoercionPathType
findFkeyCast(Oid targetTypeId, Oid sourceTypeId, Oid *funcid);
static void
validateForeignKeyConstraint(char *conname, Relation rel, Relation pkrel, Oid pkindOid, Oid constraintOid);
static void
ATController(AlterTableStmt *parsetree, Relation rel, List *cmds, bool recurse, LOCKMODE lockmode);
static void
ATPrepCmd(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing, LOCKMODE lockmode);
static void
ATRewriteCatalogs(List **wqueue, LOCKMODE lockmode);
static void
ATExecCmd(List **wqueue, AlteredTableInfo *tab, Relation rel, AlterTableCmd *cmd, LOCKMODE lockmode);
static void
ATRewriteTables(AlterTableStmt *parsetree, List **wqueue, LOCKMODE lockmode);
static void
ATRewriteTable(AlteredTableInfo *tab, Oid OIDNewHeap, LOCKMODE lockmode);
static AlteredTableInfo *
ATGetQueueEntry(List **wqueue, Relation rel);
static void
ATSimplePermissions(Relation rel, int allowed_targets);
static void
ATWrongRelkindError(Relation rel, int allowed_targets);
static void
ATSimpleRecursion(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, LOCKMODE lockmode);
static void
ATCheckPartitionsNotInUse(Relation rel, LOCKMODE lockmode);
static void
ATTypedTableRecursion(List **wqueue, Relation rel, AlterTableCmd *cmd, LOCKMODE lockmode);
static List *
find_typed_table_dependencies(Oid typeOid, const char *typeName, DropBehavior behavior);
static void
ATPrepAddColumn(List **wqueue, Relation rel, bool recurse, bool recursing, bool is_view, AlterTableCmd *cmd, LOCKMODE lockmode);
static ObjectAddress
ATExecAddColumn(List **wqueue, AlteredTableInfo *tab, Relation rel, ColumnDef *colDef, bool recurse, bool recursing, bool if_not_exists, LOCKMODE lockmode);
static bool
check_for_column_name_collision(Relation rel, const char *colname, bool if_not_exists);
static void
add_column_datatype_dependency(Oid relid, int32 attnum, Oid typid);
static void
add_column_collation_dependency(Oid relid, int32 attnum, Oid collid);
static void
ATPrepDropNotNull(Relation rel, bool recurse, bool recursing);
static ObjectAddress
ATExecDropNotNull(Relation rel, const char *colName, LOCKMODE lockmode);
static void
ATPrepSetNotNull(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing, LOCKMODE lockmode);
static ObjectAddress
ATExecSetNotNull(AlteredTableInfo *tab, Relation rel, const char *colName, LOCKMODE lockmode);
static void
ATExecCheckNotNull(AlteredTableInfo *tab, Relation rel, const char *colName, LOCKMODE lockmode);
static bool
NotNullImpliedByRelConstraints(Relation rel, Form_pg_attribute attr);
static bool
ConstraintImpliedByRelConstraint(Relation scanrel, List *testConstraint, List *provenConstraint);
static ObjectAddress
ATExecColumnDefault(Relation rel, const char *colName, Node *newDefault, LOCKMODE lockmode);
static ObjectAddress
ATExecCookedColumnDefault(Relation rel, AttrNumber attnum, Node *newDefault);
static ObjectAddress
ATExecAddIdentity(Relation rel, const char *colName, Node *def, LOCKMODE lockmode);
static ObjectAddress
ATExecSetIdentity(Relation rel, const char *colName, Node *def, LOCKMODE lockmode);
static ObjectAddress
ATExecDropIdentity(Relation rel, const char *colName, bool missing_ok, LOCKMODE lockmode);
static void
ATPrepSetStatistics(Relation rel, const char *colName, int16 colNum, Node *newValue, LOCKMODE lockmode);
static ObjectAddress
ATExecSetStatistics(Relation rel, const char *colName, int16 colNum, Node *newValue, LOCKMODE lockmode);
static ObjectAddress
ATExecSetOptions(Relation rel, const char *colName, Node *options, bool isReset, LOCKMODE lockmode);
static ObjectAddress
ATExecSetStorage(Relation rel, const char *colName, Node *newValue, LOCKMODE lockmode);
static void
ATPrepDropColumn(List **wqueue, Relation rel, bool recurse, bool recursing, AlterTableCmd *cmd, LOCKMODE lockmode);
static ObjectAddress
ATExecDropColumn(List **wqueue, Relation rel, const char *colName, DropBehavior behavior, bool recurse, bool recursing, bool missing_ok, LOCKMODE lockmode, ObjectAddresses *addrs);
static ObjectAddress
ATExecAddIndex(AlteredTableInfo *tab, Relation rel, IndexStmt *stmt, bool is_rebuild, LOCKMODE lockmode);
static ObjectAddress
ATExecAddConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel, Constraint *newConstraint, bool recurse, bool is_readd, LOCKMODE lockmode);
static char *
ChooseForeignKeyConstraintNameAddition(List *colnames);
static ObjectAddress
ATExecAddIndexConstraint(AlteredTableInfo *tab, Relation rel, IndexStmt *stmt, LOCKMODE lockmode);
static ObjectAddress
ATAddCheckConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel, Constraint *constr, bool recurse, bool recursing, bool is_readd, LOCKMODE lockmode);
static ObjectAddress
ATAddForeignKeyConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel, Constraint *fkconstraint, Oid parentConstr, bool recurse, bool recursing, LOCKMODE lockmode);
static ObjectAddress
addFkRecurseReferenced(List **wqueue, Constraint *fkconstraint, Relation rel, Relation pkrel, Oid indexOid, Oid parentConstr, int numfks, int16 *pkattnum, int16 *fkattnum, Oid *pfeqoperators, Oid *ppeqoperators, Oid *ffeqoperators, bool old_check_ok);
static void
addFkRecurseReferencing(List **wqueue, Constraint *fkconstraint, Relation rel, Relation pkrel, Oid indexOid, Oid parentConstr, int numfks, int16 *pkattnum, int16 *fkattnum, Oid *pfeqoperators, Oid *ppeqoperators, Oid *ffeqoperators, bool old_check_ok, LOCKMODE lockmode);
static void
CloneForeignKeyConstraints(List **wqueue, Relation parentRel, Relation partitionRel);
static void
CloneFkReferenced(Relation parentRel, Relation partitionRel);
static void
CloneFkReferencing(List **wqueue, Relation parentRel, Relation partRel);
static void
createForeignKeyCheckTriggers(Oid myRelOid, Oid refRelOid, Constraint *fkconstraint, Oid constraintOid, Oid indexOid);
static void
createForeignKeyActionTriggers(Relation rel, Oid refRelOid, Constraint *fkconstraint, Oid constraintOid, Oid indexOid);
static bool
tryAttachPartitionForeignKey(ForeignKeyCacheInfo *fk, Oid partRelid, Oid parentConstrOid, int numfks, AttrNumber *mapped_conkey, AttrNumber *confkey, Oid *conpfeqop);
static void
ATExecDropConstraint(Relation rel, const char *constrName, DropBehavior behavior, bool recurse, bool recursing, bool missing_ok, LOCKMODE lockmode);
static void
ATPrepAlterColumnType(List **wqueue, AlteredTableInfo *tab, Relation rel, bool recurse, bool recursing, AlterTableCmd *cmd, LOCKMODE lockmode);
static bool
ATColumnChangeRequiresRewrite(Node *expr, AttrNumber varattno);
static ObjectAddress
ATExecAlterColumnType(AlteredTableInfo *tab, Relation rel, AlterTableCmd *cmd, LOCKMODE lockmode);
static void
RememberConstraintForRebuilding(Oid conoid, AlteredTableInfo *tab);
static void
RememberIndexForRebuilding(Oid indoid, AlteredTableInfo *tab);
static void
ATPostAlterTypeCleanup(List **wqueue, AlteredTableInfo *tab, LOCKMODE lockmode);
static void
ATPostAlterTypeParse(Oid oldId, Oid oldRelId, Oid refRelId, char *cmd, List **wqueue, LOCKMODE lockmode, bool rewrite);
static void
RebuildConstraintComment(AlteredTableInfo *tab, int pass, Oid objid, Relation rel, List *domname, const char *conname);
static void
TryReuseIndex(Oid oldId, IndexStmt *stmt);
static void
TryReuseForeignKey(Oid oldId, Constraint *con);
static ObjectAddress
ATExecAlterColumnGenericOptions(Relation rel, const char *colName, List *options, LOCKMODE lockmode);
static void
change_owner_fix_column_acls(Oid relationOid, Oid oldOwnerId, Oid newOwnerId);
static void
change_owner_recurse_to_sequences(Oid relationOid, Oid newOwnerId, LOCKMODE lockmode);
static ObjectAddress
ATExecClusterOn(Relation rel, const char *indexName, LOCKMODE lockmode);
static void
ATExecDropCluster(Relation rel, LOCKMODE lockmode);
static bool
ATPrepChangePersistence(Relation rel, bool toLogged);
static void
ATPrepSetTableSpace(AlteredTableInfo *tab, Relation rel, const char *tablespacename, LOCKMODE lockmode);
static void
ATExecSetTableSpace(Oid tableOid, Oid newTableSpace, LOCKMODE lockmode);
static void
ATExecSetTableSpaceNoStorage(Relation rel, Oid newTableSpace);
static void
ATExecSetRelOptions(Relation rel, List *defList, AlterTableType operation, LOCKMODE lockmode);
static void
ATExecEnableDisableTrigger(Relation rel, const char *trigname, char fires_when, bool skip_system, bool recurse, LOCKMODE lockmode);
static void
ATExecEnableDisableRule(Relation rel, const char *rulename, char fires_when, LOCKMODE lockmode);
static void
ATPrepAddInherit(Relation child_rel);
static ObjectAddress
ATExecAddInherit(Relation child_rel, RangeVar *parent, LOCKMODE lockmode);
static ObjectAddress
ATExecDropInherit(Relation rel, RangeVar *parent, LOCKMODE lockmode);
static void
drop_parent_dependency(Oid relid, Oid refclassid, Oid refobjid, DependencyType deptype);
static ObjectAddress
ATExecAddOf(Relation rel, const TypeName *ofTypename, LOCKMODE lockmode);
static void
ATExecDropOf(Relation rel, LOCKMODE lockmode);
static void
ATExecReplicaIdentity(Relation rel, ReplicaIdentityStmt *stmt, LOCKMODE lockmode);
static void
ATExecGenericOptions(Relation rel, List *options);
static void
ATExecEnableRowSecurity(Relation rel);
static void
ATExecDisableRowSecurity(Relation rel);
static void
ATExecForceNoForceRowSecurity(Relation rel, bool force_rls);

static void
index_copy_data(Relation rel, RelFileNode newrnode);
static const char *
storage_name(char c);

static void
RangeVarCallbackForDropRelation(const RangeVar *rel, Oid relOid, Oid oldRelOid, void *arg);
static void
RangeVarCallbackForAlterRelation(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg);
static PartitionSpec *
transformPartitionSpec(Relation rel, PartitionSpec *partspec, char *strategy);
static void
ComputePartitionAttrs(ParseState *pstate, Relation rel, List *partParams, AttrNumber *partattrs, List **partexprs, Oid *partopclass, Oid *partcollation, char strategy);
static void
CreateInheritance(Relation child_rel, Relation parent_rel);
static void
RemoveInheritance(Relation child_rel, Relation parent_rel);
static ObjectAddress
ATExecAttachPartition(List **wqueue, Relation rel, PartitionCmd *cmd);
static void
AttachPartitionEnsureIndexes(Relation rel, Relation attachrel);
static void
QueuePartitionConstraintValidation(List **wqueue, Relation scanrel, List *partConstraint, bool validate_default);
static void
CloneRowTriggersToPartition(Relation parent, Relation partition);
static void
DropClonedTriggersFromPartition(Oid partitionId);
static ObjectAddress
ATExecDetachPartition(Relation rel, RangeVar *name);
static ObjectAddress
ATExecAttachPartitionIdx(List **wqueue, Relation rel, RangeVar *name);
static void
validatePartitionedIndex(Relation partedIdx, Relation partedTbl);
static void
refuseDupeIndexAttach(Relation parentIdx, Relation partIdx, Relation partitionTbl);
static List *
GetParentedForeignKeyRefs(Relation partition);
static void
ATDetachCheckNoForeignKeyRefs(Relation partition);

                                                                    
                   
                              
   
                                                                               
                                                                        
                                                  
                                                                     
                                                                     
                                    
   
                                                                            
                                                                           
                                                                              
                             
   
                                                           
                                                                    
   
ObjectAddress
DefineRelation(CreateStmt *stmt, char relkind, Oid ownerId, ObjectAddress *typaddress, const char *queryString)
{
  char relname[NAMEDATALEN];
  Oid namespaceId;
  Oid relationId;
  Oid tablespaceId;
  Relation rel;
  TupleDesc descriptor;
  List *inheritOids;
  List *old_constraints;
  List *rawDefaults;
  List *cookedDefaults;
  Datum reloptions;
  ListCell *listptr;
  AttrNumber attnum;
  bool partitioned;
  static char *validnsps[] = HEAP_RELOPT_NAMESPACES;
  Oid ofTypeId;
  ObjectAddress address;
  LOCKMODE parentLockmode;
  const char *accessMethod = NULL;
  Oid accessMethodId = InvalidOid;

     
                                                                          
                                            
     
  StrNCpy(relname, stmt->relation->relname, NAMEDATALEN);

     
                                    
     
  if (stmt->oncommit != ONCOMMIT_NOOP && stmt->relation->relpersistence != RELPERSISTENCE_TEMP)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("ON COMMIT can only be used on temporary tables")));
  }

  if (stmt->partspec != NULL)
  {
    if (relkind != RELKIND_RELATION)
    {
      elog(ERROR, "unexpected relkind: %d", (int)relkind);
    }

    relkind = RELKIND_PARTITIONED_TABLE;
    partitioned = true;
  }
  else
  {
    partitioned = false;
  }

     
                                                                            
                                                                          
                                                                         
                            
     
  namespaceId = RangeVarGetAndCheckCreationNamespace(stmt->relation, NoLock, NULL);

     
                                                                            
                                                                           
                                                                  
     
  if (stmt->relation->relpersistence == RELPERSISTENCE_TEMP && InSecurityRestrictedOperation())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("cannot create temporary table within security-restricted operation")));
  }

     
                                                                            
                          
     
                                                                             
                                                                     
                                                                            
                                                                         
                                                                        
                                                                        
                                                            
     
                                                                          
                                                                            
                                    
     
  parentLockmode = (stmt->partbound != NULL ? AccessExclusiveLock : ShareUpdateExclusiveLock);

                                                  
  inheritOids = NIL;
  foreach (listptr, stmt->inhRelations)
  {
    RangeVar *rv = (RangeVar *)lfirst(listptr);
    Oid parentOid;

    parentOid = RangeVarGetRelid(rv, parentLockmode, false);

       
                                                   
       
    if (list_member_oid(inheritOids, parentOid))
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" would be inherited from more than once", get_rel_name(parentOid))));
    }

    inheritOids = lappend_oid(inheritOids, parentOid);
  }

     
                                                                            
                                                          
     
  if (stmt->tablespacename)
  {
    tablespaceId = get_tablespace_oid(stmt->tablespacename, false);

    if (partitioned && tablespaceId == MyDatabaseTableSpace)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot specify default tablespace for partitioned relations")));
    }
  }
  else if (stmt->partbound)
  {
       
                                                                         
                                                         
       
    Assert(list_length(inheritOids) == 1);
    tablespaceId = get_rel_tablespace(linitial_oid(inheritOids));
  }
  else
  {
    tablespaceId = InvalidOid;
  }

                                      
  if (!OidIsValid(tablespaceId))
  {
    tablespaceId = GetDefaultTablespace(stmt->relation->relpersistence, partitioned);
  }

                                                              
  if (OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
  {
    AclResult aclresult;

    aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_TABLESPACE, get_tablespace_name(tablespaceId));
    }
  }

                                                                 
  if (tablespaceId == GLOBALTABLESPACE_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("only shared relations can be placed in pg_global tablespace")));
  }

                                                
  if (!OidIsValid(ownerId))
  {
    ownerId = GetUserId();
  }

     
                                            
     
  reloptions = transformRelOptions((Datum)0, stmt->options, NULL, validnsps, true, false);

  if (relkind == RELKIND_VIEW)
  {
    (void)view_reloptions(reloptions, true);
  }
  else
  {
    (void)heap_reloptions(relkind, reloptions, true);
  }

  if (stmt->ofTypename)
  {
    AclResult aclresult;

    ofTypeId = typenameTypeId(NULL, stmt->ofTypename);

    aclresult = pg_type_aclcheck(ofTypeId, GetUserId(), ACL_USAGE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error_type(aclresult, ofTypeId);
    }
  }
  else
  {
    ofTypeId = InvalidOid;
  }

     
                                                                           
                                                                        
                                   
     
  stmt->tableElts = MergeAttributes(stmt->tableElts, inheritOids, stmt->relation->relpersistence, stmt->partbound != NULL, &old_constraints);

     
                                                                         
                                                                       
                                                                 
     
  descriptor = BuildDescForRelation(stmt->tableElts);

     
                                                                       
                                                                           
                                                                           
                                                                             
                                                                         
                                                 
     
                                                                           
                                                                         
                        
     
  rawDefaults = NIL;
  cookedDefaults = NIL;
  attnum = 0;

  foreach (listptr, stmt->tableElts)
  {
    ColumnDef *colDef = lfirst(listptr);
    Form_pg_attribute attr;

    attnum++;
    attr = TupleDescAttr(descriptor, attnum - 1);

    if (colDef->raw_default != NULL)
    {
      RawColumnDefault *rawEnt;

      Assert(colDef->cooked_default == NULL);

      rawEnt = (RawColumnDefault *)palloc(sizeof(RawColumnDefault));
      rawEnt->attnum = attnum;
      rawEnt->raw_default = colDef->raw_default;
      rawEnt->missingMode = false;
      rawEnt->generated = colDef->generated;
      rawDefaults = lappend(rawDefaults, rawEnt);
      attr->atthasdef = true;
    }
    else if (colDef->cooked_default != NULL)
    {
      CookedConstraint *cooked;

      cooked = (CookedConstraint *)palloc(sizeof(CookedConstraint));
      cooked->contype = CONSTR_DEFAULT;
      cooked->conoid = InvalidOid;                    
      cooked->name = NULL;
      cooked->attnum = attnum;
      cooked->expr = colDef->cooked_default;
      cooked->skip_validation = false;
      cooked->is_local = true;                            
      cooked->inhcount = 0;               
      cooked->is_no_inherit = false;
      cookedDefaults = lappend(cookedDefaults, cooked);
      attr->atthasdef = true;
    }

    if (colDef->identity)
    {
      attr->attidentity = colDef->identity;
    }

    if (colDef->generated)
    {
      attr->attgenerated = colDef->generated;
    }
  }

     
                                                                            
                                                         
     
  if (stmt->accessMethod != NULL)
  {
    accessMethod = stmt->accessMethod;

    if (partitioned)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("specifying a table access method is not supported on a partitioned table")));
    }
  }
  else if (relkind == RELKIND_RELATION || relkind == RELKIND_TOASTVALUE || relkind == RELKIND_MATVIEW)
  {
    accessMethod = default_table_access_method;
  }

                                                           
  if (accessMethod != NULL)
  {
    accessMethodId = get_table_am_oid(accessMethod, false);
  }

     
                                                                            
                                                                           
                         
     
  relationId = heap_create_with_catalog(relname, namespaceId, tablespaceId, InvalidOid, InvalidOid, ofTypeId, ownerId, accessMethodId, descriptor, list_concat(cookedDefaults, old_constraints), relkind, stmt->relation->relpersistence, false, false, stmt->oncommit, reloptions, true, allowSystemTableMods, false, InvalidOid, typaddress);

     
                                                                         
                                
     
  CommandCounterIncrement();

     
                                                                         
                                                                           
                                                                             
                                       
     
  rel = relation_open(relationId, AccessExclusiveLock);

     
                                                                           
                                                                     
                                                                          
                                                                        
                                                                         
                                                                             
                                                         
     
                                                                         
                                             
     
  if (rawDefaults)
  {
    AddRelationNewConstraints(rel, rawDefaults, NIL, true, true, false, queryString);
  }

     
                                                                         
     
  CommandCounterIncrement();

                                                  
  if (stmt->partbound)
  {
    PartitionBoundSpec *bound;
    ParseState *pstate;
    Oid parentId = linitial_oid(inheritOids), defaultPartOid;
    Relation parent, defaultRel = NULL;
    RangeTblEntry *rte;

                                                       
    parent = table_open(parentId, NoLock);

       
                                                                         
                                                                      
       
    if (parent->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("\"%s\" is not partitioned", RelationGetRelationName(parent))));
    }

       
                                                                        
                                                                      
                                                                        
                                                                       
                                                                          
                                                                          
                                                                           
                                                     
       
                                                                      
                                                           
                                                                           
                                                                        
                                                                           
                                                                          
                                                                         
                                                     
       
    defaultPartOid = get_default_oid_from_partdesc(RelationGetPartitionDesc(parent));
    if (OidIsValid(defaultPartOid))
    {
      defaultRel = table_open(defaultPartOid, AccessExclusiveLock);
    }

                                    
    pstate = make_parsestate(NULL);
    pstate->p_sourcetext = queryString;

       
                                                                         
                                                                       
                       
       
    rte = addRangeTableEntryForRelation(pstate, rel, AccessShareLock, NULL, false, false);
    addRTEtoQuery(pstate, rte, false, true, true);
    bound = transformPartitionBound(pstate, parent, stmt->partbound);

       
                                                                        
                                                              
       
    check_new_partition_bound(relname, parent, bound);

       
                                                                       
                                                                          
                                                                           
                                                                           
                                                     
       
    if (OidIsValid(defaultPartOid))
    {
      check_default_partition_contents(parent, defaultRel, bound);
                                       
      table_close(defaultRel, NoLock);
    }

                                    
    StorePartitionBound(rel, parent, bound);

    table_close(parent, NoLock);
  }

                                                  
  StoreCatalogInheritance(relationId, inheritOids, stmt->partbound != NULL);

     
                                                                             
                                       
     
  if (partitioned)
  {
    ParseState *pstate;
    char strategy;
    int partnatts;
    AttrNumber partattrs[PARTITION_MAX_KEYS];
    Oid partopclass[PARTITION_MAX_KEYS];
    Oid partcollation[PARTITION_MAX_KEYS];
    List *partexprs = NIL;

    pstate = make_parsestate(NULL);
    pstate->p_sourcetext = queryString;

    partnatts = list_length(stmt->partspec->partParams);

                                                        
    if (partnatts > PARTITION_MAX_KEYS)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("cannot partition using more than %d columns", PARTITION_MAX_KEYS)));
    }

       
                                                                          
                                                                           
                                                                        
                
       
    stmt->partspec = transformPartitionSpec(rel, stmt->partspec, &strategy);

    ComputePartitionAttrs(pstate, rel, stmt->partspec->partParams, partattrs, &partexprs, partopclass, partcollation, strategy);

    StorePartitionKey(rel, strategy, partnatts, partattrs, partexprs, partopclass, partcollation);

                             
    CommandCounterIncrement();
  }

     
                                                                          
                                
     
                                                                             
                               
     
  if (stmt->partbound)
  {
    Oid parentId = linitial_oid(inheritOids);
    Relation parent;
    List *idxlist;
    ListCell *cell;

                                                       
    parent = table_open(parentId, NoLock);
    idxlist = RelationGetIndexList(parent);

       
                                                                       
       
    foreach (cell, idxlist)
    {
      Relation idxRel = index_open(lfirst_oid(cell), AccessShareLock);
      AttrNumber *attmap;
      IndexStmt *idxstmt;
      Oid constraintOid;

      if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
      {
        if (idxRel->rd_index->indisunique)
        {
          ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot create foreign partition of partitioned table \"%s\"", RelationGetRelationName(parent)), errdetail("Table \"%s\" contains indexes that are unique.", RelationGetRelationName(parent))));
        }
        else
        {
          index_close(idxRel, AccessShareLock);
          continue;
        }
      }

      attmap = convert_tuples_by_name_map(RelationGetDescr(rel), RelationGetDescr(parent), gettext_noop("could not convert row type"));
      idxstmt = generateClonedIndexStmt(NULL, idxRel, attmap, RelationGetDescr(parent)->natts, &constraintOid);
      DefineIndex(RelationGetRelid(rel), idxstmt, InvalidOid, RelationGetRelid(idxRel), constraintOid, false, false, false, false, false);

      index_close(idxRel, AccessShareLock);
    }

    list_free(idxlist);

       
                                                                  
                  
       
    if (parent->trigdesc != NULL)
    {
      CloneRowTriggersToPartition(parent, rel);
    }

       
                                                                           
                                                                
       
    CloneForeignKeyConstraints(NULL, parent, rel);

    table_close(parent, NoLock);
  }

     
                                                                             
                                                                             
         
     
  if (stmt->constraints)
  {
    AddRelationNewConstraints(rel, NIL, stmt->constraints, true, true, false, queryString);
  }

  ObjectAddressSet(address, RelationRelationId, relationId);

     
                                                                       
                                                   
     
  relation_close(rel, NoLock);

  return address;
}

   
                                                                            
                         
   
static void
DropErrorMsgNonExistent(RangeVar *rel, char rightkind, bool missing_ok)
{
  const struct dropmsgstrings *rentry;

  if (rel->schemaname != NULL && !OidIsValid(LookupNamespaceNoError(rel->schemaname)))
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_SCHEMA), errmsg("schema \"%s\" does not exist", rel->schemaname)));
    }
    else
    {
      ereport(NOTICE, (errmsg("schema \"%s\" does not exist, skipping", rel->schemaname)));
    }
    return;
  }

  for (rentry = dropmsgstringarray; rentry->kind != '\0'; rentry++)
  {
    if (rentry->kind == rightkind)
    {
      if (!missing_ok)
      {
        ereport(ERROR, (errcode(rentry->nonexistent_code), errmsg(rentry->nonexistent_msg, rel->relname)));
      }
      else
      {
        ereport(NOTICE, (errmsg(rentry->skipping_msg, rel->relname)));
        break;
      }
    }
  }

  Assert(rentry->kind != '\0');                           
}

   
                                                                 
                              
   
static void
DropErrorMsgWrongType(const char *relname, char wrongkind, char rightkind)
{
  const struct dropmsgstrings *rentry;
  const struct dropmsgstrings *wentry;

  for (rentry = dropmsgstringarray; rentry->kind != '\0'; rentry++)
  {
    if (rentry->kind == rightkind)
    {
      break;
    }
  }
  Assert(rentry->kind != '\0');

  for (wentry = dropmsgstringarray; wentry->kind != '\0'; wentry++)
  {
    if (wentry->kind == wrongkind)
    {
      break;
    }
  }
                                                                  

  ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg(rentry->nota_msg, relname), (wentry->kind != '\0') ? errhint("%s", _(wentry->drophint_msg)) : 0));
}

   
                   
                                                                 
                                               
   
void
RemoveRelations(DropStmt *drop)
{
  ObjectAddresses *objects;
  char relkind;
  ListCell *cell;
  int flags = 0;
  LOCKMODE lockmode = AccessExclusiveLock;

                                                                       
  if (drop->concurrent)
  {
       
                                                                    
                                                                
                                        
       
    lockmode = ShareUpdateExclusiveLock;
    Assert(drop->removeType == OBJECT_INDEX);
    if (list_length(drop->objects) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DROP INDEX CONCURRENTLY does not support dropping multiple objects")));
    }
    if (drop->behavior == DROP_CASCADE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DROP INDEX CONCURRENTLY does not support CASCADE")));
    }
  }

     
                                                                          
                                                                      
                                                                 
     

                                  
  switch (drop->removeType)
  {
  case OBJECT_TABLE:
    relkind = RELKIND_RELATION;
    break;

  case OBJECT_INDEX:
    relkind = RELKIND_INDEX;
    break;

  case OBJECT_SEQUENCE:
    relkind = RELKIND_SEQUENCE;
    break;

  case OBJECT_VIEW:
    relkind = RELKIND_VIEW;
    break;

  case OBJECT_MATVIEW:
    relkind = RELKIND_MATVIEW;
    break;

  case OBJECT_FOREIGN_TABLE:
    relkind = RELKIND_FOREIGN_TABLE;
    break;

  default:
    elog(ERROR, "unrecognized drop object type: %d", (int)drop->removeType);
    relkind = 0;                          
    break;
  }

                                                                         
  objects = new_object_addresses();

  foreach (cell, drop->objects)
  {
    RangeVar *rel = makeRangeVarFromNameList((List *)lfirst(cell));
    Oid relOid;
    ObjectAddress obj;
    struct DropRelationCallbackState state;

       
                                                                          
                                                                      
       
                                                                         
                                                                  
                                                                      
                                                                           
                                                                   
       
    AcceptInvalidationMessages();

                                                                  
    state.expected_relkind = relkind;
    state.heap_lockmode = drop->concurrent ? ShareUpdateExclusiveLock : AccessExclusiveLock;
                                                                         
    state.heapOid = InvalidOid;
    state.partParentOid = InvalidOid;

    relOid = RangeVarGetRelidExtended(rel, lockmode, RVR_MISSING_OK, RangeVarCallbackForDropRelation, (void *)&state);

                    
    if (!OidIsValid(relOid))
    {
      DropErrorMsgNonExistent(rel, relkind, drop->missing_ok);
      continue;
    }

       
                                                                    
                                                        
       
    if (drop->concurrent && state.actual_relpersistence != RELPERSISTENCE_TEMP)
    {
      Assert(list_length(drop->objects) == 1 && drop->removeType == OBJECT_INDEX);
      flags |= PERFORM_DELETION_CONCURRENTLY;
    }

       
                                                                      
               
       
    if ((flags & PERFORM_DELETION_CONCURRENTLY) != 0 && state.actual_relkind == RELKIND_PARTITIONED_INDEX)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot drop partitioned index \"%s\" concurrently", rel->relname)));
    }

       
                                                                          
                                                                           
                                                                          
                                                                         
                                                   
       
    if (state.actual_relkind == RELKIND_PARTITIONED_INDEX)
    {
      (void)find_all_inheritors(state.heapOid, state.heap_lockmode, NULL);
    }

                                            
    obj.classId = RelationRelationId;
    obj.objectId = relOid;
    obj.objectSubId = 0;

    add_exact_object_address(&obj, objects);
  }

  performMultipleDeletions(objects, drop->behavior, flags);

  free_object_addresses(objects);
}

   
                                                                           
                                                                           
                                                                              
          
   
static void
RangeVarCallbackForDropRelation(const RangeVar *rel, Oid relOid, Oid oldRelOid, void *arg)
{
  HeapTuple tuple;
  struct DropRelationCallbackState *state;
  char expected_relkind;
  bool is_partition;
  Form_pg_class classform;
  LOCKMODE heap_lockmode;
  bool invalid_system_index = false;

  state = (struct DropRelationCallbackState *)arg;
  heap_lockmode = state->heap_lockmode;

     
                                                                         
                                                                           
           
     
  if (relOid != oldRelOid && OidIsValid(state->heapOid))
  {
    UnlockRelationOid(state->heapOid, heap_lockmode);
    state->heapOid = InvalidOid;
  }

     
                                                                             
                                                                          
                       
     
  if (relOid != oldRelOid && OidIsValid(state->partParentOid))
  {
    UnlockRelationOid(state->partParentOid, AccessExclusiveLock);
    state->partParentOid = InvalidOid;
  }

                                                                            
  if (!OidIsValid(relOid))
  {
    return;
  }

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relOid));
  if (!HeapTupleIsValid(tuple))
  {
    return;                                             
  }
  classform = (Form_pg_class)GETSTRUCT(tuple);
  is_partition = classform->relispartition;

                                                              
  state->actual_relkind = classform->relkind;
  state->actual_relpersistence = classform->relpersistence;

     
                                                                           
                                                                           
                                                                          
                                                                           
                                                                       
                          
     
  if (classform->relkind == RELKIND_PARTITIONED_TABLE)
  {
    expected_relkind = RELKIND_RELATION;
  }
  else if (classform->relkind == RELKIND_PARTITIONED_INDEX)
  {
    expected_relkind = RELKIND_INDEX;
  }
  else
  {
    expected_relkind = classform->relkind;
  }

  if (state->expected_relkind != expected_relkind)
  {
    DropErrorMsgWrongType(rel->relname, classform->relkind, state->expected_relkind);
  }

                                                        
  if (!pg_class_ownercheck(relOid, GetUserId()) && !pg_namespace_ownercheck(classform->relnamespace, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(classform->relkind), rel->relname);
  }

     
                                                                            
                                                                            
                                                                           
                                   
     
  if (IsSystemClass(relOid, classform) && classform->relkind == RELKIND_INDEX)
  {
    HeapTuple locTuple;
    Form_pg_index indexform;
    bool indisvalid;

    locTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(relOid));
    if (!HeapTupleIsValid(locTuple))
    {
      ReleaseSysCache(tuple);
      return;
    }

    indexform = (Form_pg_index)GETSTRUCT(locTuple);
    indisvalid = indexform->indisvalid;
    ReleaseSysCache(locTuple);

                                                                  
    if (!indisvalid)
    {
      invalid_system_index = true;
    }
  }

                                                                        
  if (!invalid_system_index && !allowSystemTableMods && IsSystemClass(relOid, classform))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", rel->relname)));
  }

  ReleaseSysCache(tuple);

     
                                                                       
                                                                       
                                                                           
                                                                          
                                                                           
                                                                
     
  if (expected_relkind == RELKIND_INDEX && relOid != oldRelOid)
  {
    state->heapOid = IndexGetRelation(relOid, true);
    if (OidIsValid(state->heapOid))
    {
      LockRelationOid(state->heapOid, heap_lockmode);
    }
  }

     
                                                                            
                                                                           
                                                                             
                 
     
  if (is_partition && relOid != oldRelOid)
  {
    state->partParentOid = get_partition_parent(relOid);
    if (OidIsValid(state->partParentOid))
    {
      LockRelationOid(state->partParentOid, AccessExclusiveLock);
    }
  }
}

   
                   
                                 
   
                                                                        
                                                                      
                                                                       
                                                                              
                                                                             
                                                                            
                                
   
void
ExecuteTruncate(TruncateStmt *stmt)
{
  List *rels = NIL;
  List *relids = NIL;
  List *relids_logged = NIL;
  ListCell *cell;

     
                                                                            
     
  foreach (cell, stmt->relations)
  {
    RangeVar *rv = lfirst(cell);
    Relation rel;
    bool recurse = rv->inh;
    Oid myrelid;
    LOCKMODE lockmode = AccessExclusiveLock;

    myrelid = RangeVarGetRelidExtended(rv, lockmode, 0, RangeVarCallbackForTruncate, NULL);

                                                         
    rel = table_open(myrelid, NoLock);

                                                   
    if (list_member_oid(relids, myrelid))
    {
      table_close(rel, lockmode);
      continue;
    }

       
                                                                          
                                                             
       
    truncate_check_activity(rel);

    rels = lappend(rels, rel);
    relids = lappend_oid(relids, myrelid);
                                                               
    if (RelationIsLogicallyLogged(rel))
    {
      relids_logged = lappend_oid(relids_logged, myrelid);
    }

    if (recurse)
    {
      ListCell *child;
      List *children;

      children = find_all_inheritors(myrelid, lockmode, NULL);

      foreach (child, children)
      {
        Oid childrelid = lfirst_oid(child);

        if (list_member_oid(relids, childrelid))
        {
          continue;
        }

                                                  
        rel = table_open(childrelid, NoLock);

           
                                                                      
                                                                   
                                                                   
                                                                   
                                                          
                                                                    
                                
           
        if (RELATION_IS_OTHER_TEMP(rel))
        {
          table_close(rel, lockmode);
          continue;
        }

        truncate_check_rel(RelationGetRelid(rel), rel->rd_rel);
        truncate_check_activity(rel);

        rels = lappend(rels, rel);
        relids = lappend_oid(relids, childrelid);
                                                                   
        if (RelationIsLogicallyLogged(rel))
        {
          relids_logged = lappend_oid(relids_logged, childrelid);
        }
      }
    }
    else if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot truncate only a partitioned table"), errhint("Do not specify the ONLY keyword, or use TRUNCATE ONLY on the partitions directly.")));
    }
  }

  ExecuteTruncateGuts(rels, relids, relids_logged, stmt->behavior, stmt->restart_seqs);

                          
  foreach (cell, rels)
  {
    Relation rel = (Relation)lfirst(cell);

    table_close(rel, NoLock);
  }
}

   
                       
   
                                                                               
                                                                         
                               
   
                                                                       
                                                                          
                                                                       
                                                                            
                                        
   
void
ExecuteTruncateGuts(List *explicit_rels, List *relids, List *relids_logged, DropBehavior behavior, bool restart_seqs)
{
  List *rels;
  List *seq_relids = NIL;
  EState *estate;
  ResultRelInfo *resultRelInfos;
  ResultRelInfo *resultRelInfo;
  SubTransactionId mySubid;
  ListCell *cell;
  Oid *logrelids;

     
                                               
     
                                                                       
                                                                             
                                                                             
                                                                             
                                                                             
                                               
     
  rels = list_copy(explicit_rels);
  if (behavior == DROP_CASCADE)
  {
    for (;;)
    {
      List *newrelids;

      newrelids = heap_truncate_find_FKs(relids);
      if (newrelids == NIL)
      {
        break;                          
      }

      foreach (cell, newrelids)
      {
        Oid relid = lfirst_oid(cell);
        Relation rel;

        rel = table_open(relid, AccessExclusiveLock);
        ereport(NOTICE, (errmsg("truncate cascades to table \"%s\"", RelationGetRelationName(rel))));
        truncate_check_rel(relid, rel->rd_rel);
        truncate_check_activity(rel);
        rels = lappend(rels, rel);
        relids = lappend_oid(relids, relid);
                                                                   
        if (RelationIsLogicallyLogged(rel))
        {
          relids_logged = lappend_oid(relids_logged, relid);
        }
      }
    }
  }

     
                                                                    
                                                                      
                                                              
     
#ifdef USE_ASSERT_CHECKING
  heap_truncate_check_FKs(rels, false);
#else
  if (behavior == DROP_RESTRICT)
  {
    heap_truncate_check_FKs(rels, false);
  }
#endif

     
                                                                             
                                                                             
                                                                            
                                                
     
  if (restart_seqs)
  {
    foreach (cell, rels)
    {
      Relation rel = (Relation)lfirst(cell);
      List *seqlist = getOwnedSequences(RelationGetRelid(rel), 0);
      ListCell *seqcell;

      foreach (seqcell, seqlist)
      {
        Oid seq_relid = lfirst_oid(seqcell);
        Relation seq_rel;

        seq_rel = relation_open(seq_relid, AccessExclusiveLock);

                                                  
        if (!pg_class_ownercheck(seq_relid, GetUserId()))
        {
          aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_SEQUENCE, RelationGetRelationName(seq_rel));
        }

        seq_relids = lappend_oid(seq_relids, seq_relid);

        relation_close(seq_rel, NoLock);
      }
    }
  }

                                        
  AfterTriggerBeginQuery();

     
                                                                           
                                                                    
     
  estate = CreateExecutorState();
  resultRelInfos = (ResultRelInfo *)palloc(list_length(rels) * sizeof(ResultRelInfo));
  resultRelInfo = resultRelInfos;
  foreach (cell, rels)
  {
    Relation rel = (Relation)lfirst(cell);

    InitResultRelInfo(resultRelInfo, rel, 0,                             
        NULL, 0);
    resultRelInfo++;
  }
  estate->es_result_relations = resultRelInfos;
  estate->es_num_result_relations = list_length(rels);

     
                                                                    
                                                                             
                                                                           
                         
     
  resultRelInfo = resultRelInfos;
  foreach (cell, rels)
  {
    estate->es_result_relation_info = resultRelInfo;
    ExecBSTruncateTriggers(estate, resultRelInfo);
    resultRelInfo++;
  }

     
                              
     
  mySubid = GetCurrentSubTransactionId();

  foreach (cell, rels)
  {
    Relation rel = (Relation)lfirst(cell);

                                                           
    if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      continue;
    }

       
                                                                          
                                                                           
                                                                           
                                                                      
                                                                    
       
    if (rel->rd_createSubid == mySubid || rel->rd_newRelfilenodeSubid == mySubid)
    {
                                                        
      heap_truncate_one_rel(rel);
    }
    else
    {
      Oid heap_relid;
      Oid toast_relid;

         
                                                                         
                                                                       
                                                                  
                                                
         
      CheckTableForSerializableConflictIn(rel);

         
                                                 
         
                                                                         
                                                                         
                             
         
      RelationSetNewRelfilenode(rel, rel->rd_rel->relpersistence);

      heap_relid = RelationGetRelid(rel);

         
                                               
         
      toast_relid = rel->rd_rel->reltoastrelid;
      if (OidIsValid(toast_relid))
      {
        Relation toastrel = relation_open(toast_relid, AccessExclusiveLock);

        RelationSetNewRelfilenode(toastrel, toastrel->rd_rel->relpersistence);
        table_close(toastrel, NoLock);
      }

         
                                                           
         
      reindex_relation(heap_relid, REINDEX_REL_PROCESS_TOAST, 0);
    }

    pgstat_count_truncate(rel);
  }

     
                                                  
     
  foreach (cell, seq_relids)
  {
    Oid seq_relid = lfirst_oid(cell);

    ResetSequence(seq_relid);
  }

     
                                                                     
              
     
                                                                             
                   
     
  if (list_length(relids_logged) > 0)
  {
    xl_heap_truncate xlrec;
    int i = 0;

                                                      
    Assert(XLogLogicalInfoActive());

    logrelids = palloc(list_length(relids_logged) * sizeof(Oid));
    foreach (cell, relids_logged)
    {
      logrelids[i++] = lfirst_oid(cell);
    }

    xlrec.dbId = MyDatabaseId;
    xlrec.nrelids = list_length(relids_logged);
    xlrec.flags = 0;
    if (behavior == DROP_CASCADE)
    {
      xlrec.flags |= XLH_TRUNCATE_CASCADE;
    }
    if (restart_seqs)
    {
      xlrec.flags |= XLH_TRUNCATE_RESTART_SEQS;
    }

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHeapTruncate);
    XLogRegisterData((char *)logrelids, list_length(relids_logged) * sizeof(Oid));

    XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

    (void)XLogInsert(RM_HEAP_ID, XLOG_HEAP_TRUNCATE);
  }

     
                                                    
     
  resultRelInfo = resultRelInfos;
  foreach (cell, rels)
  {
    estate->es_result_relation_info = resultRelInfo;
    ExecASTruncateTriggers(estate, resultRelInfo);
    resultRelInfo++;
  }

                                    
  AfterTriggerEndQuery(estate);

                                      
  FreeExecutorState(estate);

     
                                                                        
                 
     
  rels = list_difference_ptr(rels, explicit_rels);
  foreach (cell, rels)
  {
    Relation rel = (Relation)lfirst(cell);

    table_close(rel, NoLock);
  }
}

   
                                                                    
                                                        
   
static void
truncate_check_rel(Oid relid, Form_pg_class reltuple)
{
  AclResult aclresult;
  char *relname = NameStr(reltuple->relname);

     
                                                                             
                                                                          
                                                    
     
  if (reltuple->relkind != RELKIND_RELATION && reltuple->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table", relname)));
  }

                          
  aclresult = pg_class_aclcheck(relid, GetUserId(), ACL_TRUNCATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, get_relkind_objtype(reltuple->relkind), relname);
  }

  if (!allowSystemTableMods && IsSystemClass(relid, reltuple))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", relname)));
  }
}

   
                                                                      
                                                         
                                                             
   
static void
truncate_check_activity(Relation rel)
{
     
                                                                           
                                          
     
  if (RELATION_IS_OTHER_TEMP(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot truncate temporary tables of other sessions")));
  }

     
                                                                            
                                                            
     
  CheckTableNotInUse(rel, "TRUNCATE");
}

   
                
                                                                          
   
static const char *
storage_name(char c)
{
  switch (c)
  {
  case 'p':
    return "PLAIN";
  case 'm':
    return "MAIN";
  case 'x':
    return "EXTENDED";
  case 'e':
    return "EXTERNAL";
  default:
    return "???";
  }
}

             
                   
                                                              
   
                    
                                                                           
                                                  
                                                                             
                                                          
                                                     
   
                     
                                                                        
                                                    
   
                 
                          
   
          
                                                                        
                                                                         
                                                                       
                                                                      
   
                        
   
                                                               
                                                                   
                                                         
                                                                  
   
                                                 
   
                                            
            \
                                                    
              
                                
   
                                                                         
                                                                           
   
                                                                       
                                                                            
                         
   
                                                          
                                                                     
                                                                      
                                         
                                                                     
                                                            
                                                 
                                                                   
                                                            
             
   
static List *
MergeAttributes(List *schema, List *supers, char relpersistence, bool is_partition, List **supconstr)
{
  ListCell *entry;
  List *inhSchema = NIL;
  List *constraints = NIL;
  bool have_bogus_defaults = false;
  int child_attno;
  static Node bogus_marker = {0};                                 
  List *saved_schema = NIL;

     
                                                                        
                                                                          
                                                                             
                                                                         
                                                                     
                           
     
                                                                             
                                                 
     
  if (list_length(schema) > MaxHeapAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("tables can have at most %d columns", MaxHeapAttributeNumber)));
  }

     
                                                                   
     
                                                                             
                                                                           
                                                
     
  foreach (entry, schema)
  {
    ColumnDef *coldef = lfirst(entry);
    ListCell *rest = lnext(entry);
    ListCell *prev = entry;

    if (!is_partition && coldef->typeName == NULL)
    {
         
                                                                         
                                                                      
                                                                      
                                                       
         
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" does not exist", coldef->colname)));
    }

    while (rest != NULL)
    {
      ColumnDef *restdef = lfirst(rest);
      ListCell *next = lnext(rest);                               
                                                   

      if (strcmp(coldef->colname, restdef->colname) == 0)
      {
        if (coldef->is_from_type)
        {
             
                                                                    
             
          coldef->is_not_null = restdef->is_not_null;
          coldef->raw_default = restdef->raw_default;
          coldef->cooked_default = restdef->cooked_default;
          coldef->constraints = restdef->constraints;
          coldef->is_from_type = false;
          schema = list_delete_cell(schema, rest, prev);

             
                                                               
                                                     
             
          Assert(schema != NIL);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" specified more than once", coldef->colname)));
        }
      }
      prev = rest;
      rest = next;
    }
  }

     
                                                                             
                                                                            
                              
     
  if (is_partition)
  {
    saved_schema = schema;
    schema = NIL;
  }

     
                                                                          
                                                                             
                               
     
  child_attno = 0;
  foreach (entry, supers)
  {
    Oid parent = lfirst_oid(entry);
    Relation relation;
    TupleDesc tupleDesc;
    TupleConstr *constr;
    AttrNumber *newattno;
    List *inherited_defaults;
    List *cols_with_defaults;
    AttrNumber parent_attno;
    ListCell *lc1;
    ListCell *lc2;

                                 
    relation = table_open(parent, NoLock);

       
                                                                    
                                                                    
                          
       
    if (is_partition)
    {
      CheckTableNotInUse(relation, "CREATE TABLE .. PARTITION OF");
    }

       
                                                                           
                            
       
    if (relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && !is_partition)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit from partitioned table \"%s\"", RelationGetRelationName(relation))));
    }
    if (relation->rd_rel->relispartition && !is_partition)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit from partition \"%s\"", RelationGetRelationName(relation))));
    }

    if (relation->rd_rel->relkind != RELKIND_RELATION && relation->rd_rel->relkind != RELKIND_FOREIGN_TABLE && relation->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("inherited relation \"%s\" is not a table or foreign table", RelationGetRelationName(relation))));
    }

       
                                                                           
                                          
       
    if (is_partition && relation->rd_rel->relpersistence != RELPERSISTENCE_TEMP && relpersistence == RELPERSISTENCE_TEMP)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot create a temporary relation as partition of permanent relation \"%s\"", RelationGetRelationName(relation))));
    }

                                                           
    if (relpersistence != RELPERSISTENCE_TEMP && relation->rd_rel->relpersistence == RELPERSISTENCE_TEMP)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg(!is_partition ? "cannot inherit from temporary relation \"%s\"" : "cannot create a permanent relation as partition of temporary relation \"%s\"", RelationGetRelationName(relation))));
    }

                                                                 
    if (relation->rd_rel->relpersistence == RELPERSISTENCE_TEMP && !relation->rd_islocaltemp)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg(!is_partition ? "cannot inherit from temporary relation of another session" : "cannot create as partition of temporary relation of another session")));
    }

       
                                                                      
                                                            
       
    if (!pg_class_ownercheck(RelationGetRelid(relation), GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(relation->rd_rel->relkind), RelationGetRelationName(relation));
    }

    tupleDesc = RelationGetDescr(relation);
    constr = tupleDesc->constr;

       
                                                                         
                                                                    
                                                                     
       
    newattno = (AttrNumber *)palloc0(tupleDesc->natts * sizeof(AttrNumber));

                                                                           
    inherited_defaults = cols_with_defaults = NIL;

    for (parent_attno = 1; parent_attno <= tupleDesc->natts; parent_attno++)
    {
      Form_pg_attribute attribute = TupleDescAttr(tupleDesc, parent_attno - 1);
      char *attributeName = NameStr(attribute->attname);
      int exist_attno;
      ColumnDef *def;

         
                                               
         
      if (attribute->attisdropped)
      {
        continue;                                   
      }

         
                                                                 
         
      exist_attno = findAttrByName(attributeName, inhSchema);
      if (exist_attno > 0)
      {
        Oid defTypeId;
        int32 deftypmod;
        Oid defCollId;

           
                                                                   
                                                      
           
        ereport(NOTICE, (errmsg("merging multiple inherited definitions of column \"%s\"", attributeName)));
        def = (ColumnDef *)list_nth(inhSchema, exist_attno - 1);
        typenameTypeIdAndMod(NULL, def->typeName, &defTypeId, &deftypmod);
        if (defTypeId != attribute->atttypid || deftypmod != attribute->atttypmod)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("inherited column \"%s\" has a type conflict", attributeName), errdetail("%s versus %s", format_type_with_typemod(defTypeId, deftypmod), format_type_with_typemod(attribute->atttypid, attribute->atttypmod))));
        }
        defCollId = GetColumnDefCollation(NULL, def, defTypeId);
        if (defCollId != attribute->attcollation)
        {
          ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("inherited column \"%s\" has a collation conflict", attributeName), errdetail("\"%s\" versus \"%s\"", get_collation_name(defCollId), get_collation_name(attribute->attcollation))));
        }

                                          
        if (def->storage == 0)
        {
          def->storage = attribute->attstorage;
        }
        else if (def->storage != attribute->attstorage)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("inherited column \"%s\" has a storage parameter conflict", attributeName), errdetail("%s versus %s", storage_name(def->storage), storage_name(attribute->attstorage))));
        }

        def->inhcount++;
                                                             
        def->is_not_null |= attribute->attnotnull;
                                                             
        newattno[parent_attno - 1] = exist_attno;

                                           
        if (def->generated != attribute->attgenerated)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("inherited column \"%s\" has a generation conflict", attributeName)));
        }
      }
      else
      {
           
                                             
           
        def = makeNode(ColumnDef);
        def->colname = pstrdup(attributeName);
        def->typeName = makeTypeNameFromOid(attribute->atttypid, attribute->atttypmod);
        def->inhcount = 1;
        def->is_local = false;
        def->is_not_null = attribute->attnotnull;
        def->is_from_type = false;
        def->storage = attribute->attstorage;
        def->raw_default = NULL;
        def->cooked_default = NULL;
        def->generated = attribute->attgenerated;
        def->collClause = NULL;
        def->collOid = attribute->attcollation;
        def->constraints = NIL;
        def->location = -1;
        inhSchema = lappend(inhSchema, def);
        newattno[parent_attno - 1] = ++child_attno;
      }

         
                               
         
      if (attribute->atthasdef)
      {
        Node *this_default = NULL;
        AttrDefault *attrdef;
        int i;

                                                  
        Assert(constr != NULL);
        attrdef = constr->defval;
        for (i = 0; i < constr->num_defval; i++)
        {
          if (attrdef[i].adnum == parent_attno)
          {
            this_default = stringToNode(attrdef[i].adbin);
            break;
          }
        }
        Assert(this_default != NULL);

           
                                                                   
                                                                      
                                                                       
                                                                 
           
        inherited_defaults = lappend(inherited_defaults, this_default);
        cols_with_defaults = lappend(cols_with_defaults, def);
      }
    }

       
                                                                       
                                           
       
    forboth(lc1, inherited_defaults, lc2, cols_with_defaults)
    {
      Node *this_default = (Node *)lfirst(lc1);
      ColumnDef *def = (ColumnDef *)lfirst(lc2);
      bool found_whole_row;

                                                             
      this_default = map_variable_attnos(this_default, 1, 0, newattno, tupleDesc->natts, InvalidOid, &found_whole_row);

         
                                                                         
                                                                        
                                                                       
                                                                  
         
      if (found_whole_row)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert whole-row table reference"), errdetail("Generation expression for column \"%s\" contains a whole-row reference to table \"%s\".", def->colname, RelationGetRelationName(relation))));
      }

         
                                                                      
                                                                        
                                                                       
                                                                 
         
      Assert(def->raw_default == NULL);
      if (def->cooked_default == NULL)
      {
        def->cooked_default = this_default;
      }
      else if (!equal(def->cooked_default, this_default))
      {
        def->cooked_default = &bogus_marker;
        have_bogus_defaults = true;
      }
    }

       
                                                                       
                                                                          
                                                    
       
    if (constr && constr->num_check > 0)
    {
      ConstrCheck *check = constr->check;
      int i;

      for (i = 0; i < constr->num_check; i++)
      {
        char *name = check[i].ccname;
        Node *expr;
        bool found_whole_row;

                                                         
        if (check[i].ccnoinherit)
        {
          continue;
        }

                                                               
        expr = map_variable_attnos(stringToNode(check[i].ccbin), 1, 0, newattno, tupleDesc->natts, InvalidOid, &found_whole_row);

           
                                                                    
                                                                       
                                              
           
        if (found_whole_row)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert whole-row table reference"), errdetail("Constraint \"%s\" contains a whole-row reference to table \"%s\".", name, RelationGetRelationName(relation))));
        }

                                 
        if (!MergeCheckConstraint(constraints, name, expr))
        {
                                       
          CookedConstraint *cooked;

          cooked = (CookedConstraint *)palloc(sizeof(CookedConstraint));
          cooked->contype = CONSTR_CHECK;
          cooked->conoid = InvalidOid;                    
          cooked->name = pstrdup(name);
          cooked->attnum = 0;                               
          cooked->expr = expr;
          cooked->skip_validation = false;
          cooked->is_local = false;
          cooked->inhcount = 1;
          cooked->is_no_inherit = false;
          constraints = lappend(constraints, cooked);
        }
      }
    }

    pfree(newattno);

       
                                                                        
                                                                           
                                      
       
    table_close(relation, NoLock);
  }

     
                                                                      
                                                                            
                                                                          
                                                              
     
  if (inhSchema != NIL)
  {
    int schema_attno = 0;

    foreach (entry, schema)
    {
      ColumnDef *newdef = lfirst(entry);
      char *attributeName = newdef->colname;
      int exist_attno;

      schema_attno++;

         
                                                                 
         
      exist_attno = findAttrByName(attributeName, inhSchema);
      if (exist_attno > 0)
      {
        ColumnDef *def;
        Oid defTypeId, newTypeId;
        int32 deftypmod, newtypmod;
        Oid defcollid, newcollid;

           
                                                              
                                                                     
           
        Assert(!is_partition);

           
                                                                   
                                                      
           
        if (exist_attno == schema_attno)
        {
          ereport(NOTICE, (errmsg("merging column \"%s\" with inherited definition", attributeName)));
        }
        else
        {
          ereport(NOTICE, (errmsg("moving and merging column \"%s\" with inherited definition", attributeName), errdetail("User-specified column moved to the position of the inherited column.")));
        }
        def = (ColumnDef *)list_nth(inhSchema, exist_attno - 1);
        typenameTypeIdAndMod(NULL, def->typeName, &defTypeId, &deftypmod);
        typenameTypeIdAndMod(NULL, newdef->typeName, &newTypeId, &newtypmod);
        if (defTypeId != newTypeId || deftypmod != newtypmod)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("column \"%s\" has a type conflict", attributeName), errdetail("%s versus %s", format_type_with_typemod(defTypeId, deftypmod), format_type_with_typemod(newTypeId, newtypmod))));
        }
        defcollid = GetColumnDefCollation(NULL, def, defTypeId);
        newcollid = GetColumnDefCollation(NULL, newdef, newTypeId);
        if (defcollid != newcollid)
        {
          ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("column \"%s\" has a collation conflict", attributeName), errdetail("\"%s\" versus \"%s\"", get_collation_name(defcollid), get_collation_name(newcollid))));
        }

           
                                                                    
                                                                 
           
        def->identity = newdef->identity;

                                    
        if (def->storage == 0)
        {
          def->storage = newdef->storage;
        }
        else if (newdef->storage != 0 && def->storage != newdef->storage)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("column \"%s\" has a storage parameter conflict", attributeName), errdetail("%s versus %s", storage_name(def->storage), storage_name(newdef->storage))));
        }

                                                
        def->is_local = true;
                                                             
        def->is_not_null |= newdef->is_not_null;

           
                                                             
           
                                                                       
                                                                     
                                                                      
                                                              
                                                                       
                                                                      
                                                                   
                                   
           
        if (def->generated)
        {
          if (newdef->generated)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_DEFINITION), errmsg("child column \"%s\" specifies generation expression", def->colname), errhint("Omit the generation expression in the definition of the child table column to inherit the generation expression from the parent table.")));
          }
          if (newdef->raw_default && !newdef->generated)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_DEFINITION), errmsg("column \"%s\" inherits from generated column but specifies default", def->colname)));
          }
          if (newdef->identity)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_DEFINITION), errmsg("column \"%s\" inherits from generated column but specifies identity", def->colname)));
          }
        }
           
                                                                     
                                             
           
        else
        {
          if (newdef->generated)
          {
            def->generated = newdef->generated;
          }
        }

                                                                 
        if (newdef->raw_default != NULL)
        {
          def->raw_default = newdef->raw_default;
          def->cooked_default = newdef->cooked_default;
        }
      }
      else
      {
           
                                                  
           
        inhSchema = lappend(inhSchema, newdef);
      }
    }

    schema = inhSchema;

       
                                                                           
                             
       
    if (list_length(schema) > MaxHeapAttributeNumber)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("tables can have at most %d columns", MaxHeapAttributeNumber)));
    }
  }

     
                                                                         
                                                                         
                                                                     
                                      
     
  if (is_partition)
  {
    foreach (entry, saved_schema)
    {
      ColumnDef *restdef = lfirst(entry);
      bool found = false;
      ListCell *l;

      foreach (l, schema)
      {
        ColumnDef *coldef = lfirst(l);

        if (strcmp(coldef->colname, restdef->colname) == 0)
        {
          found = true;
          coldef->is_not_null |= restdef->is_not_null;

             
                                                                 
                                                                 
                                                                   
                                                                     
                                                             
                                                                    
                              
             
          Assert(restdef->cooked_default == NULL);
          Assert(coldef->raw_default == NULL);
          if (restdef->raw_default)
          {
            coldef->raw_default = restdef->raw_default;
            coldef->cooked_default = NULL;
          }
        }
      }

                                                             
      if (!found)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" does not exist", restdef->colname)));
      }
    }
  }

     
                                                                           
                                        
     
  if (have_bogus_defaults)
  {
    foreach (entry, schema)
    {
      ColumnDef *def = lfirst(entry);

      if (def->cooked_default == &bogus_marker)
      {
        if (def->generated)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_DEFINITION), errmsg("column \"%s\" inherits conflicting generation expressions", def->colname)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_DEFINITION), errmsg("column \"%s\" inherits conflicting default values", def->colname), errhint("To resolve the conflict, specify a default explicitly.")));
        }
      }
    }
  }

  *supconstr = constraints;
  return schema;
}

   
                        
                                                                  
   
                                                                              
                                                                           
   
                                                                               
   
                                                                        
                                                          
   
static bool
MergeCheckConstraint(List *constraints, char *name, Node *expr)
{
  ListCell *lc;

  foreach (lc, constraints)
  {
    CookedConstraint *ccon = (CookedConstraint *)lfirst(lc);

    Assert(ccon->contype == CONSTR_CHECK);

                                           
    if (strcmp(ccon->name, name) != 0)
    {
      continue;
    }

    if (equal(expr, ccon->expr))
    {
                       
      ccon->inhcount++;
      return true;
    }

    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("check constraint name \"%s\" appears multiple times but with different expressions", name)));
  }

  return false;
}

   
                           
                                                                     
   
                                                                        
   
static void
StoreCatalogInheritance(Oid relationId, List *supers, bool child_is_partition)
{
  Relation relation;
  int32 seqNumber;
  ListCell *entry;

     
                   
     
  AssertArg(OidIsValid(relationId));

  if (supers == NIL)
  {
    return;
  }

     
                                                                            
                                                                             
                                        
     
                                                                           
                                                                     
                                                               
     
  relation = table_open(InheritsRelationId, RowExclusiveLock);

  seqNumber = 1;
  foreach (entry, supers)
  {
    Oid parentOid = lfirst_oid(entry);

    StoreCatalogInheritance1(relationId, parentOid, seqNumber, relation, child_is_partition);
    seqNumber++;
  }

  table_close(relation, RowExclusiveLock);
}

   
                                                                         
                                                                         
   
static void
StoreCatalogInheritance1(Oid relationId, Oid parentOid, int32 seqNumber, Relation inhRelation, bool child_is_partition)
{
  ObjectAddress childobject, parentobject;

                                 
  StoreSingleInheritance(relationId, parentOid, seqNumber);

     
                            
     
  parentobject.classId = RelationRelationId;
  parentobject.objectId = parentOid;
  parentobject.objectSubId = 0;
  childobject.classId = RelationRelationId;
  childobject.objectId = relationId;
  childobject.objectSubId = 0;

  recordDependencyOn(&childobject, &parentobject, child_dependency_type(child_is_partition));

     
                                                                      
                                                                      
                                           
     
  InvokeObjectPostAlterHookArg(InheritsRelationId, relationId, 0, parentOid, false);

     
                                           
     
  SetRelationHasSubclass(parentOid, true);
}

   
                                                          
   
                                                                              
                    
   
static int
findAttrByName(const char *attributeName, List *schema)
{
  ListCell *s;
  int i = 1;

  foreach (s, schema)
  {
    ColumnDef *def = lfirst(s);

    if (strcmp(attributeName, def->colname) == 0)
    {
      return i;
    }

    i++;
  }
  return 0;
}

   
                          
                                                                      
   
                                                                     
                                           
   
                                                                               
                                                                          
                                                                         
                                                                             
        
   
void
SetRelationHasSubclass(Oid relationId, bool relhassubclass)
{
  Relation relationRelation;
  HeapTuple tuple;
  Form_pg_class classtuple;

     
                                                                       
     
  relationRelation = table_open(RelationRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relationId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relationId);
  }
  classtuple = (Form_pg_class)GETSTRUCT(tuple);

  if (classtuple->relhassubclass != relhassubclass)
  {
    classtuple->relhassubclass = relhassubclass;
    CatalogTupleUpdate(relationRelation, &tuple->t_self, tuple);
  }
  else
  {
                                                                    
    CacheInvalidateRelcacheByTuple(tuple);
  }

  heap_freetuple(tuple);
  table_close(relationRelation, RowExclusiveLock);
}

   
                                                                    
   
static void
renameatt_check(Oid myrelid, Form_pg_class classform, bool recursing)
{
  char relkind = classform->relkind;

  if (classform->reloftype && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot rename column of typed table")));
  }

     
                                                                        
                                                                    
                                                                            
                                                                          
                  
     
  if (relkind != RELKIND_RELATION && relkind != RELKIND_VIEW && relkind != RELKIND_MATVIEW && relkind != RELKIND_COMPOSITE_TYPE && relkind != RELKIND_INDEX && relkind != RELKIND_PARTITIONED_INDEX && relkind != RELKIND_FOREIGN_TABLE && relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, view, materialized view, composite type, index, or foreign table", NameStr(classform->relname))));
  }

     
                                                                             
     
  if (!pg_class_ownercheck(myrelid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(myrelid)), NameStr(classform->relname));
  }
  if (!allowSystemTableMods && IsSystemClass(myrelid, classform))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", NameStr(classform->relname))));
  }
}

   
                                                  
   
                                                                   
   
static AttrNumber
renameatt_internal(Oid myrelid, const char *oldattname, const char *newattname, bool recurse, bool recursing, int expected_parents, DropBehavior behavior)
{
  Relation targetrelation;
  Relation attrelation;
  HeapTuple atttup;
  Form_pg_attribute attform;
  AttrNumber attnum;

     
                                                                           
                               
     
  targetrelation = relation_open(myrelid, AccessExclusiveLock);
  renameatt_check(myrelid, RelationGetForm(targetrelation), recursing);

     
                                                                      
                                                                         
                 
     
                                                                          
                                                                          
     
  if (recurse)
  {
    List *child_oids, *child_numparents;
    ListCell *lo, *li;

       
                                                                          
                                                                        
                                                          
       
    child_oids = find_all_inheritors(myrelid, AccessExclusiveLock, &child_numparents);

       
                                                                        
                                                                           
                             
       
    forboth(lo, child_oids, li, child_numparents)
    {
      Oid childrelid = lfirst_oid(lo);
      int numparents = lfirst_int(li);

      if (childrelid == myrelid)
      {
        continue;
      }
                                          
      renameatt_internal(childrelid, oldattname, newattname, false, true, numparents, behavior);
    }
  }
  else
  {
       
                                                                        
                                                           
       
                                                                        
       
    if (expected_parents == 0 && find_inheritance_children(myrelid, NoLock) != NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("inherited column \"%s\" must be renamed in child tables too", oldattname)));
    }
  }

                                                           
  if (targetrelation->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
  {
    List *child_oids;
    ListCell *lo;

    child_oids = find_typed_table_dependencies(targetrelation->rd_rel->reltype, RelationGetRelationName(targetrelation), behavior);

    foreach (lo, child_oids)
    {
      renameatt_internal(lfirst_oid(lo), oldattname, newattname, true, true, 0, behavior);
    }
  }

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);

  atttup = SearchSysCacheCopyAttName(myrelid, oldattname);
  if (!HeapTupleIsValid(atttup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" does not exist", oldattname)));
  }
  attform = (Form_pg_attribute)GETSTRUCT(atttup);

  attnum = attform->attnum;
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot rename system column \"%s\"", oldattname)));
  }

     
                                                                       
                                                                            
                                                                           
                                                                       
                                                                             
                                                                             
                                                                   
     
  if (attform->attinhcount > expected_parents)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot rename inherited column \"%s\"", oldattname)));
  }

                                         
  (void)check_for_column_name_collision(targetrelation, newattname, false);

                        
  namestrcpy(&(attform->attname), newattname);

  CatalogTupleUpdate(attrelation, &atttup->t_self, atttup);

  InvokeObjectPostAlterHook(RelationRelationId, myrelid, attnum);

  heap_freetuple(atttup);

  table_close(attrelation, RowExclusiveLock);

  relation_close(targetrelation, NoLock);                              

  return attnum;
}

   
                                                                              
   
static void
RangeVarCallbackForRenameAttribute(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{
  HeapTuple tuple;
  Form_pg_class form;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    return;                           
  }
  form = (Form_pg_class)GETSTRUCT(tuple);
  renameatt_check(relid, form, false);
  ReleaseSysCache(tuple);
}

   
                                                                
   
                                                             
   
ObjectAddress
renameatt(RenameStmt *stmt)
{
  Oid relid;
  AttrNumber attnum;
  ObjectAddress address;

                                                             
  relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock, stmt->missing_ok ? RVR_MISSING_OK : 0, RangeVarCallbackForRenameAttribute, NULL);

  if (!OidIsValid(relid))
  {
    ereport(NOTICE, (errmsg("relation \"%s\" does not exist, skipping", stmt->relation->relname)));
    return InvalidObjectAddress;
  }

  attnum = renameatt_internal(relid, stmt->subname,                   
      stmt->newname,                                                  
      stmt->relation->inh,                                          
      false,                                                        
      0,                                                                   
      stmt->behavior);

  ObjectAddressSubSet(address, RelationRelationId, relid, attnum);

  return address;
}

   
                                    
   
static ObjectAddress
rename_constraint_internal(Oid myrelid, Oid mytypid, const char *oldconname, const char *newconname, bool recurse, bool recursing, int expected_parents)
{
  Relation targetrelation = NULL;
  Oid constraintOid;
  HeapTuple tuple;
  Form_pg_constraint con;
  ObjectAddress address;

  AssertArg(!myrelid || !mytypid);

  if (mytypid)
  {
    constraintOid = get_domain_constraint_oid(mytypid, oldconname, false);
  }
  else
  {
    targetrelation = relation_open(myrelid, AccessExclusiveLock);

       
                                                                      
                   
       
    renameatt_check(myrelid, RelationGetForm(targetrelation), false);

    constraintOid = get_relation_constraint_oid(myrelid, oldconname, false);
  }

  tuple = SearchSysCache1(CONSTROID, ObjectIdGetDatum(constraintOid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for constraint %u", constraintOid);
  }
  con = (Form_pg_constraint)GETSTRUCT(tuple);

  if (myrelid && con->contype == CONSTRAINT_CHECK && !con->connoinherit)
  {
    if (recurse)
    {
      List *child_oids, *child_numparents;
      ListCell *lo, *li;

      child_oids = find_all_inheritors(myrelid, AccessExclusiveLock, &child_numparents);

      forboth(lo, child_oids, li, child_numparents)
      {
        Oid childrelid = lfirst_oid(lo);
        int numparents = lfirst_int(li);

        if (childrelid == myrelid)
        {
          continue;
        }

        rename_constraint_internal(childrelid, InvalidOid, oldconname, newconname, false, true, numparents);
      }
    }
    else
    {
      if (expected_parents == 0 && find_inheritance_children(myrelid, NoLock) != NIL)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("inherited constraint \"%s\" must be renamed in child tables too", oldconname)));
      }
    }

    if (con->coninhcount > expected_parents)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot rename inherited constraint \"%s\"", oldconname)));
    }
  }

  if (con->conindid && (con->contype == CONSTRAINT_PRIMARY || con->contype == CONSTRAINT_UNIQUE || con->contype == CONSTRAINT_EXCLUSION))
  {
                                                               
    RenameRelationInternal(con->conindid, newconname, false, true);
  }
  else
  {
    RenameConstraintById(constraintOid, newconname);
  }

  ObjectAddressSet(address, ConstraintRelationId, constraintOid);

  ReleaseSysCache(tuple);

  if (targetrelation)
  {
       
                                                                         
       
    CacheInvalidateRelcache(targetrelation);

    relation_close(targetrelation, NoLock);                              
  }

  return address;
}

ObjectAddress
RenameConstraint(RenameStmt *stmt)
{
  Oid relid = InvalidOid;
  Oid typid = InvalidOid;

  if (stmt->renameType == OBJECT_DOMCONSTRAINT)
  {
    Relation rel;
    HeapTuple tup;

    typid = typenameTypeId(NULL, makeTypeNameFromNameList(castNode(List, stmt->object)));
    rel = table_open(TypeRelationId, RowExclusiveLock);
    tup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for type %u", typid);
    }
    checkDomainOwner(tup);
    ReleaseSysCache(tup);
    table_close(rel, NoLock);
  }
  else
  {
                                                                       
    relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock, stmt->missing_ok ? RVR_MISSING_OK : 0, RangeVarCallbackForRenameAttribute, NULL);
    if (!OidIsValid(relid))
    {
      ereport(NOTICE, (errmsg("relation \"%s\" does not exist, skipping", stmt->relation->relname)));
      return InvalidObjectAddress;
    }
  }

  return rename_constraint_internal(relid, typid, stmt->subname, stmt->newname, (stmt->relation && stmt->relation->inh),                 
      false,                                                                                                                             
      0                        );
}

   
                                                                           
          
   
ObjectAddress
RenameRelation(RenameStmt *stmt)
{
  bool is_index_stmt = stmt->renameType == OBJECT_INDEX;
  Oid relid;
  ObjectAddress address;

     
                                                                        
                                                                          
                         
     
                                                                             
                                                                             
                                                    
     
  for (;;)
  {
    LOCKMODE lockmode;
    char relkind;
    bool obj_is_index;

    lockmode = is_index_stmt ? ShareUpdateExclusiveLock : AccessExclusiveLock;

    relid = RangeVarGetRelidExtended(stmt->relation, lockmode, stmt->missing_ok ? RVR_MISSING_OK : 0, RangeVarCallbackForAlterRelation, (void *)stmt);

    if (!OidIsValid(relid))
    {
      ereport(NOTICE, (errmsg("relation \"%s\" does not exist, skipping", stmt->relation->relname)));
      return InvalidObjectAddress;
    }

       
                                                                         
                                                                          
                                                                         
                                                                          
       
    relkind = get_rel_relkind(relid);
    obj_is_index = (relkind == RELKIND_INDEX || relkind == RELKIND_PARTITIONED_INDEX);
    if (obj_is_index || is_index_stmt == obj_is_index)
    {
      break;
    }

    UnlockRelationOid(relid, lockmode);
    is_index_stmt = obj_is_index;
  }

                   
  RenameRelationInternal(relid, stmt->newname, false, is_index_stmt);

  ObjectAddressSet(address, RelationRelationId, relid);

  return address;
}

   
                                                           
   
void
RenameRelationInternal(Oid myrelid, const char *newrelname, bool is_internal, bool is_index)
{
  Relation targetrelation;
  Relation relrelation;                            
  HeapTuple reltup;
  Form_pg_class relform;
  Oid namespaceId;

     
                                                                             
                                                                     
                                                                        
                                                                       
                                                                         
                                                                             
                                                                          
                
     
  targetrelation = relation_open(myrelid, is_index ? ShareUpdateExclusiveLock : AccessExclusiveLock);
  namespaceId = RelationGetNamespace(targetrelation);

     
                                                                            
     
  relrelation = table_open(RelationRelationId, RowExclusiveLock);

  reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(myrelid));
  if (!HeapTupleIsValid(reltup))                       
  {
    elog(ERROR, "cache lookup failed for relation %u", myrelid);
  }
  relform = (Form_pg_class)GETSTRUCT(reltup);

  if (get_relname_relid(newrelname, namespaceId) != InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" already exists", newrelname)));
  }

     
                                                                       
                                                                          
                                                                       
                                                 
     
  Assert(!is_index || is_index == (targetrelation->rd_rel->relkind == RELKIND_INDEX || targetrelation->rd_rel->relkind == RELKIND_PARTITIONED_INDEX));

     
                                                                          
                             
     
  namestrcpy(&(relform->relname), newrelname);

  CatalogTupleUpdate(relrelation, &reltup->t_self, reltup);

  InvokeObjectPostAlterHookArg(RelationRelationId, myrelid, 0, InvalidOid, is_internal);

  heap_freetuple(reltup);
  table_close(relrelation, RowExclusiveLock);

     
                                              
     
  if (OidIsValid(targetrelation->rd_rel->reltype))
  {
    RenameTypeInternal(targetrelation->rd_rel->reltype, newrelname, namespaceId);
  }

     
                                                    
     
  if (targetrelation->rd_rel->relkind == RELKIND_INDEX || targetrelation->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
  {
    Oid constraintId = get_index_constraint(myrelid);

    if (OidIsValid(constraintId))
    {
      RenameConstraintById(constraintId, newrelname);
    }
  }

     
                               
     
  relation_close(targetrelation, NoLock);
}

   
                                       
   
void
ResetRelRewrite(Oid myrelid)
{
  Relation relrelation;                            
  HeapTuple reltup;
  Form_pg_class relform;

     
                                     
     
  relrelation = table_open(RelationRelationId, RowExclusiveLock);

  reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(myrelid));
  if (!HeapTupleIsValid(reltup))                       
  {
    elog(ERROR, "cache lookup failed for relation %u", myrelid);
  }
  relform = (Form_pg_class)GETSTRUCT(reltup);

     
                            
     
  relform->relrewrite = InvalidOid;

  CatalogTupleUpdate(relrelation, &reltup->t_self, reltup);

  heap_freetuple(reltup);
  table_close(relrelation, RowExclusiveLock);
}

   
                                                                            
                                                                           
                                                                            
                                                                           
                                              
   
                                                                          
                                                                            
                                                                           
                                                                           
                                  
   
                                                                               
                                                                           
                                                                          
                                                                            
                                                                             
   
                                                                           
                                                                           
                                                                           
   
                                                                               
   
void
CheckTableNotInUse(Relation rel, const char *stmt)
{
  int expected_refcnt;

  expected_refcnt = rel->rd_isnailed ? 2 : 1;
  if (rel->rd_refcnt != expected_refcnt)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE),
                                                                                  
                       errmsg("cannot %s \"%s\" because it is being used by active queries in this session", stmt, RelationGetRelationName(rel))));
  }

  if (rel->rd_rel->relkind != RELKIND_INDEX && rel->rd_rel->relkind != RELKIND_PARTITIONED_INDEX && AfterTriggerPendingOnRel(RelationGetRelid(rel)))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE),
                                                                                  
                       errmsg("cannot %s \"%s\" because it has pending trigger events", stmt, RelationGetRelationName(rel))));
  }
}

   
                            
                                                                        
               
   
Oid
AlterTableLookupRelation(AlterTableStmt *stmt, LOCKMODE lockmode)
{
  return RangeVarGetRelidExtended(stmt->relation, lockmode, stmt->missing_ok ? RVR_MISSING_OK : 0, RangeVarCallbackForAlterRelation, (void *)stmt);
}

   
              
                                                            
   
                                             
                                                                    
                               
                                                                     
                                   
                                                                           
                                                                      
                                                                           
         
   
                                                                    
                                                                           
                                                                             
                                                                          
                                                                       
           
   
                                                               
                                                                   
                                                                      
                                                                        
                                                  
   
                                                                   
   
                                                                       
                                                                         
                                                                        
                                                                              
                                                                             
   
                                                                           
                                                                          
   
                                                                     
                                                                           
                                          
                                                                          
                                                                           
                                                                           
                                            
   
void
AlterTable(Oid relid, LOCKMODE lockmode, AlterTableStmt *stmt)
{
  Relation rel;

                                                       
  rel = relation_open(relid, NoLock);

  CheckTableNotInUse(rel, "ALTER TABLE");

  ATController(stmt, rel, stmt->cmds, stmt->relation->inh, lockmode);
}

   
                      
   
                                            
   
                                                                        
                                                                         
                                                                      
                                                                       
                                                        
   
void
AlterTableInternal(Oid relid, List *cmds, bool recurse)
{
  Relation rel;
  LOCKMODE lockmode = AlterTableGetLockLevel(cmds);

  rel = relation_open(relid, lockmode);

  EventTriggerAlterTableRelid(relid);

  ATController(NULL, rel, cmds, recurse, lockmode);
}

   
                          
   
                                                                              
                                                                     
                                           
   
                                                                     
                                                                   
                                                                         
                                                                              
   
                                                                              
                                             
   
                                                                             
                                                                               
                                                                                  
                                                                                
                                      
   
                                                                             
                                                                            
                                                                             
                                  
   
                                                                              
                                                                                 
                                                                              
                                                                            
                                                                           
   
LOCKMODE
AlterTableGetLockLevel(List *cmds)
{
     
                                                                     
     
  ListCell *lcmd;
  LOCKMODE lockmode = ShareUpdateExclusiveLock;

  foreach (lcmd, cmds)
  {
    AlterTableCmd *cmd = (AlterTableCmd *)lfirst(lcmd);
    LOCKMODE cmd_lockmode = AccessExclusiveLock;                           

    switch (cmd->subtype)
    {
         
                                                                    
         
    case AT_AddColumn:                                                      
                                            
    case AT_SetTableSpace:                          
    case AT_AlterColumnType:                        
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                                    
                                                                     
                                                                    
                              
         
    case AT_SetStorage:                              
                                                 
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                                
                                                                
                            
         
    case AT_DropConstraint:                    
    case AT_DropNotNull:                                   
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                               
         
    case AT_DropColumn:                                      
    case AT_AddColumnToView:                    
    case AT_DropOids:                                           
    case AT_EnableAlwaysRule:                               
    case AT_EnableReplicaRule:                              
    case AT_EnableRule:                                     
    case AT_DisableRule:                                    
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                              
         
    case AT_ChangeOwner:                               
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                                 
         
    case AT_GenericOptions:
    case AT_AlterColumnGenericOptions:
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                         
         
    case AT_EnableTrig:
    case AT_EnableAlwaysTrig:
    case AT_EnableReplicaTrig:
    case AT_EnableTrigAll:
    case AT_EnableTrigUser:
    case AT_DisableTrig:
    case AT_DisableTrigAll:
    case AT_DisableTrigUser:
      cmd_lockmode = ShareRowExclusiveLock;
      break;

         
                                                             
                                                              
         
    case AT_ColumnDefault:
    case AT_CookedColumnDefault:
    case AT_AlterConstraint:
    case AT_AddIndex:                          
    case AT_AddIndexConstraint:
    case AT_ReplicaIdentity:
    case AT_SetNotNull:
    case AT_EnableRowSecurity:
    case AT_DisableRowSecurity:
    case AT_ForceRowSecurity:
    case AT_NoForceRowSecurity:
    case AT_AddIdentity:
    case AT_DropIdentity:
    case AT_SetIdentity:
      cmd_lockmode = AccessExclusiveLock;
      break;

    case AT_AddConstraint:
    case AT_ProcessedConstraint:                                 
    case AT_AddConstraintRecurse:                                
    case AT_ReAddConstraint:                                     
    case AT_ReAddDomainConstraint:                               
      if (IsA(cmd->def, Constraint))
      {
        Constraint *con = (Constraint *)cmd->def;

        switch (con->contype)
        {
        case CONSTR_EXCLUSION:
        case CONSTR_PRIMARY:
        case CONSTR_UNIQUE:

             
                                                            
                                                            
                                                             
                                               
                                                        
                       
             
          cmd_lockmode = AccessExclusiveLock;
          break;
        case CONSTR_FOREIGN:

             
                                                          
                                                             
                                          
             
          cmd_lockmode = ShareRowExclusiveLock;
          break;

        default:
          cmd_lockmode = AccessExclusiveLock;
        }
      }
      break;

         
                                                                 
                                                                    
                                                                   
                                                                  
                                                                   
                                                 
         
    case AT_AddInherit:
    case AT_DropInherit:
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                                     
                                                                    
                                                                  
                                                           
         
    case AT_AddOf:
    case AT_DropOf:
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                                 
                                                   
         
    case AT_ReplaceRelOptions:
      cmd_lockmode = AccessExclusiveLock;
      break;

         
                                                                     
                                                                   
                                                                    
                                                                    
                                                                
                                                                  
                                                              
                  
         
    case AT_SetStatistics:                                   
    case AT_ClusterOn:                                    
    case AT_DropCluster:                                  
    case AT_SetOptions:                                      
    case AT_ResetOptions:                                    
      cmd_lockmode = ShareUpdateExclusiveLock;
      break;

    case AT_SetLogged:
    case AT_SetUnLogged:
      cmd_lockmode = AccessExclusiveLock;
      break;

    case AT_ValidateConstraint:                                    
      cmd_lockmode = ShareUpdateExclusiveLock;
      break;

         
                                                                  
                                                                    
                                                                     
                                                      
         
    case AT_SetRelOptions:                                    
                                              
    case AT_ResetRelOptions:                                  
                                              
      cmd_lockmode = AlterTableGetRelOptionsLockLevel((List *)cmd->def);
      break;

    case AT_AttachPartition:
      cmd_lockmode = ShareUpdateExclusiveLock;
      break;

    case AT_DetachPartition:
      cmd_lockmode = AccessExclusiveLock;
      break;

    case AT_CheckNotNull:

         
                                                                 
                                                            
         
      cmd_lockmode = AccessShareLock;
      break;

    default:           
      elog(ERROR, "unrecognized alter table type: %d", (int)cmd->subtype);
      break;
    }

       
                                                      
       
    if (cmd_lockmode > lockmode)
    {
      lockmode = cmd_lockmode;
    }
  }

  return lockmode;
}

   
                                                            
   
                                                                     
                   
   
static void
ATController(AlterTableStmt *parsetree, Relation rel, List *cmds, bool recurse, LOCKMODE lockmode)
{
  List *wqueue = NIL;
  ListCell *lcmd;

                                                                       
  foreach (lcmd, cmds)
  {
    AlterTableCmd *cmd = (AlterTableCmd *)lfirst(lcmd);

    ATPrepCmd(&wqueue, rel, cmd, recurse, false, lockmode);
  }

                                                      
  relation_close(rel, NoLock);

                                       
  ATRewriteCatalogs(&wqueue, lockmode);

                                              
  ATRewriteTables(parsetree, &wqueue, lockmode);
}

   
             
   
                                                                    
                                    
   
                                                                        
                                          
   
static void
ATPrepCmd(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing, LOCKMODE lockmode)
{
  AlteredTableInfo *tab;
  int pass = AT_PASS_UNSET;

                                                      
  tab = ATGetQueueEntry(wqueue, rel);

     
                                                                            
                                                                    
                                                                            
                                                      
     
  cmd = copyObject(cmd);

     
                                                                           
                                           
     
  switch (cmd->subtype)
  {
  case AT_AddColumn:                 
    ATSimplePermissions(rel, ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE);
    ATPrepAddColumn(wqueue, rel, recurse, recursing, false, cmd, lockmode);
                                                 
    pass = AT_PASS_ADD_COL;
    break;
  case AT_AddColumnToView:                                            
    ATSimplePermissions(rel, ATT_VIEW);
    ATPrepAddColumn(wqueue, rel, recurse, recursing, true, cmd, lockmode);
                                                 
    pass = AT_PASS_ADD_COL;
    break;
  case AT_ColumnDefault:                           

       
                                                                      
                                                              
                                                                 
              
       
    ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE);
    ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
                                         
    pass = cmd->def ? AT_PASS_ADD_CONSTR : AT_PASS_DROP;
    break;
  case AT_CookedColumnDefault:                               
                                                     
                                                          
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                     
    pass = AT_PASS_ADD_CONSTR;
    break;
  case AT_AddIdentity:
    ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE);
                                     
    pass = AT_PASS_ADD_CONSTR;
    break;
  case AT_SetIdentity:
    ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE);
                                     
    pass = AT_PASS_COL_ATTRS;
    break;
  case AT_DropIdentity:
    ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE);
                                     
    pass = AT_PASS_DROP;
    break;
  case AT_DropNotNull:                                 
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
    ATPrepDropNotNull(rel, recurse, recursing);
    ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
    pass = AT_PASS_DROP;
    break;
  case AT_SetNotNull:                                
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                                  
    ATPrepSetNotNull(wqueue, rel, cmd, recurse, recursing, lockmode);
    pass = AT_PASS_COL_ATTRS;
    break;
  case AT_CheckNotNull:                                              
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
    ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
                                         
    pass = AT_PASS_COL_ATTRS;
    break;
  case AT_SetStatistics:                                  
    ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
                                        
    ATPrepSetStatistics(rel, cmd->name, cmd->num, cmd->def, lockmode);
    pass = AT_PASS_MISC;
    break;
  case AT_SetOptions:                                     
  case AT_ResetOptions:                                     
    ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW | ATT_INDEX | ATT_FOREIGN_TABLE);
                                     
    pass = AT_PASS_MISC;
    break;
  case AT_SetStorage:                               
    ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW | ATT_FOREIGN_TABLE);
    ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_DropColumn:                  
    ATSimplePermissions(rel, ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE);
    ATPrepDropColumn(wqueue, rel, recurse, recursing, cmd, lockmode);
                                                 
    pass = AT_PASS_DROP;
    break;
  case AT_AddIndex:                
    ATSimplePermissions(rel, ATT_TABLE);
                                     
                                         
    pass = AT_PASS_ADD_INDEX;
    break;
  case AT_AddConstraint:                     
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                                 
                                                                    
    if (recurse)
    {
      cmd->subtype = AT_AddConstraintRecurse;
    }
    pass = AT_PASS_ADD_CONSTR;
    break;
  case AT_AddIndexConstraint:                                 
    ATSimplePermissions(rel, ATT_TABLE);
                                     
                                         
    pass = AT_PASS_ADD_CONSTR;
    break;
  case AT_DropConstraint:                      
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
    ATCheckPartitionsNotInUse(rel, lockmode);
                                                       
                                                                    
    if (recurse)
    {
      cmd->subtype = AT_DropConstraintRecurse;
    }
    pass = AT_PASS_DROP;
    break;
  case AT_AlterColumnType:                        
    ATSimplePermissions(rel, ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE);
                                
    ATPrepAlterColumnType(wqueue, tab, rel, recurse, recursing, cmd, lockmode);
    pass = AT_PASS_ALTER_TYPE;
    break;
  case AT_AlterColumnGenericOptions:
    ATSimplePermissions(rel, ATT_FOREIGN_TABLE);
                                     
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_ChangeOwner:                  
                                     
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_ClusterOn:                   
  case AT_DropCluster:                          
    ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW);
                                      
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_SetLogged:                 
    ATSimplePermissions(rel, ATT_TABLE);
    tab->chgPersistence = ATPrepChangePersistence(rel, true);
                                                                    
    if (tab->chgPersistence)
    {
      tab->rewrite |= AT_REWRITE_ALTER_PERSISTENCE;
      tab->newrelpersistence = RELPERSISTENCE_PERMANENT;
    }
    pass = AT_PASS_MISC;
    break;
  case AT_SetUnLogged:                   
    ATSimplePermissions(rel, ATT_TABLE);
    tab->chgPersistence = ATPrepChangePersistence(rel, false);
                                                                    
    if (tab->chgPersistence)
    {
      tab->rewrite |= AT_REWRITE_ALTER_PERSISTENCE;
      tab->newrelpersistence = RELPERSISTENCE_UNLOGGED;
    }
    pass = AT_PASS_MISC;
    break;
  case AT_DropOids:                       
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
    pass = AT_PASS_DROP;
    break;
  case AT_SetTableSpace:                     
    ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW | ATT_INDEX | ATT_PARTITIONED_INDEX);
                                     
    ATPrepSetTableSpace(tab, rel, cmd->name, lockmode);
    pass = AT_PASS_MISC;                              
    break;
  case AT_SetRelOptions:                    
  case AT_ResetRelOptions:                    
  case AT_ReplaceRelOptions:                                          
    ATSimplePermissions(rel, ATT_TABLE | ATT_VIEW | ATT_MATVIEW | ATT_INDEX);
                                     
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_AddInherit:              
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                     
    ATPrepAddInherit(rel);
    pass = AT_PASS_MISC;
    break;
  case AT_DropInherit:                 
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                     
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_AlterConstraint:                       
    ATSimplePermissions(rel, ATT_TABLE);
                                                 
    pass = AT_PASS_MISC;
    break;
  case AT_ValidateConstraint:                          
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                                 
                                                                    
    if (recurse)
    {
      cmd->subtype = AT_ValidateConstraintRecurse;
    }
    pass = AT_PASS_MISC;
    break;
  case AT_ReplicaIdentity:                           
    ATSimplePermissions(rel, ATT_TABLE | ATT_MATVIEW);
    pass = AT_PASS_MISC;
                                     
                                         
    break;
  case AT_EnableTrig:                              
  case AT_EnableAlwaysTrig:
  case AT_EnableReplicaTrig:
  case AT_EnableTrigAll:
  case AT_EnableTrigUser:
  case AT_DisableTrig:                               
  case AT_DisableTrigAll:
  case AT_DisableTrigUser:
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
                                                            
    if (recurse)
    {
      cmd->recurse = true;
    }
    pass = AT_PASS_MISC;
    break;
  case AT_EnableRule:                                   
  case AT_EnableAlwaysRule:
  case AT_EnableReplicaRule:
  case AT_DisableRule:
  case AT_AddOf:          
  case AT_DropOf:             
  case AT_EnableRowSecurity:
  case AT_DisableRowSecurity:
  case AT_ForceRowSecurity:
  case AT_NoForceRowSecurity:
    ATSimplePermissions(rel, ATT_TABLE);
                                      
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_GenericOptions:
    ATSimplePermissions(rel, ATT_FOREIGN_TABLE);
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_AttachPartition:
    ATSimplePermissions(rel, ATT_TABLE | ATT_PARTITIONED_INDEX);
                                         
    pass = AT_PASS_MISC;
    break;
  case AT_DetachPartition:
    ATSimplePermissions(rel, ATT_TABLE);
                                         
    pass = AT_PASS_MISC;
    break;
  default:           
    elog(ERROR, "unrecognized alter table type: %d", (int)cmd->subtype);
    pass = AT_PASS_UNSET;                          
    break;
  }
  Assert(pass > AT_PASS_UNSET);

                                                              
  tab->subcmds[pass] = lappend(tab->subcmds[pass], cmd);
}

   
                     
   
                                                                    
                                                                         
               
   
static void
ATRewriteCatalogs(List **wqueue, LOCKMODE lockmode)
{
  int pass;
  ListCell *ltab;

     
                                                                           
                                                                            
                                                                         
                                                                            
                                                    
     
  for (pass = 0; pass < AT_NUM_PASSES; pass++)
  {
                                                          
    foreach (ltab, *wqueue)
    {
      AlteredTableInfo *tab = (AlteredTableInfo *)lfirst(ltab);
      List *subcmds = tab->subcmds[pass];
      Relation rel;
      ListCell *lcmd;

      if (subcmds == NIL)
      {
        continue;
      }

         
                                                                        
         
      rel = relation_open(tab->relid, NoLock);

      foreach (lcmd, subcmds)
      {
        ATExecCmd(wqueue, tab, rel, castNode(AlterTableCmd, lfirst(lcmd)), lockmode);
      }

         
                                                                         
                                                                    
                                                   
         
      if (pass == AT_PASS_ALTER_TYPE)
      {
        ATPostAlterTypeCleanup(wqueue, tab, lockmode);
      }

      relation_close(rel, NoLock);
    }
  }

                                                    
  foreach (ltab, *wqueue)
  {
    AlteredTableInfo *tab = (AlteredTableInfo *)lfirst(ltab);

       
                                                                        
                                                                  
                                         
       
    if (((tab->relkind == RELKIND_RELATION || tab->relkind == RELKIND_PARTITIONED_TABLE) && tab->partition_constraint == NULL) || tab->relkind == RELKIND_MATVIEW)
    {
      AlterTableCreateToastTable(tab->relid, (Datum)0, lockmode);
    }
  }
}

   
                                                                     
   
static void
ATExecCmd(List **wqueue, AlteredTableInfo *tab, Relation rel, AlterTableCmd *cmd, LOCKMODE lockmode)
{
  ObjectAddress address = InvalidObjectAddress;

  switch (cmd->subtype)
  {
  case AT_AddColumn:                       
  case AT_AddColumnToView:                                            
    address = ATExecAddColumn(wqueue, tab, rel, (ColumnDef *)cmd->def, false, false, cmd->missing_ok, lockmode);
    break;
  case AT_AddColumnRecurse:
    address = ATExecAddColumn(wqueue, tab, rel, (ColumnDef *)cmd->def, true, false, cmd->missing_ok, lockmode);
    break;
  case AT_ColumnDefault:                           
    address = ATExecColumnDefault(rel, cmd->name, cmd->def, lockmode);
    break;
  case AT_CookedColumnDefault:                               
    address = ATExecCookedColumnDefault(rel, cmd->num, cmd->def);
    break;
  case AT_AddIdentity:
    address = ATExecAddIdentity(rel, cmd->name, cmd->def, lockmode);
    break;
  case AT_SetIdentity:
    address = ATExecSetIdentity(rel, cmd->name, cmd->def, lockmode);
    break;
  case AT_DropIdentity:
    address = ATExecDropIdentity(rel, cmd->name, cmd->missing_ok, lockmode);
    break;
  case AT_DropNotNull:                                 
    address = ATExecDropNotNull(rel, cmd->name, lockmode);
    break;
  case AT_SetNotNull:                                
    address = ATExecSetNotNull(tab, rel, cmd->name, lockmode);
    break;
  case AT_CheckNotNull:                                              
    ATExecCheckNotNull(tab, rel, cmd->name, lockmode);
    break;
  case AT_SetStatistics:                                  
    address = ATExecSetStatistics(rel, cmd->name, cmd->num, cmd->def, lockmode);
    break;
  case AT_SetOptions:                                   
    address = ATExecSetOptions(rel, cmd->name, cmd->def, false, lockmode);
    break;
  case AT_ResetOptions:                                     
    address = ATExecSetOptions(rel, cmd->name, cmd->def, true, lockmode);
    break;
  case AT_SetStorage:                               
    address = ATExecSetStorage(rel, cmd->name, cmd->def, lockmode);
    break;
  case AT_DropColumn:                  
    address = ATExecDropColumn(wqueue, rel, cmd->name, cmd->behavior, false, false, cmd->missing_ok, lockmode, NULL);
    break;
  case AT_DropColumnRecurse:                                 
    address = ATExecDropColumn(wqueue, rel, cmd->name, cmd->behavior, true, false, cmd->missing_ok, lockmode, NULL);
    break;
  case AT_AddIndex:                
    address = ATExecAddIndex(tab, rel, (IndexStmt *)cmd->def, false, lockmode);
    break;
  case AT_ReAddIndex:                
    address = ATExecAddIndex(tab, rel, (IndexStmt *)cmd->def, true, lockmode);
    break;
  case AT_AddConstraint:                     
    address = ATExecAddConstraint(wqueue, tab, rel, (Constraint *)cmd->def, false, false, lockmode);
    break;
  case AT_AddConstraintRecurse:                                    
    address = ATExecAddConstraint(wqueue, tab, rel, (Constraint *)cmd->def, true, false, lockmode);
    break;
  case AT_ReAddConstraint:                                           
    address = ATExecAddConstraint(wqueue, tab, rel, (Constraint *)cmd->def, true, true, lockmode);
    break;
  case AT_ReAddDomainConstraint:                                     
                                                 
    address = AlterDomainAddConstraint(((AlterDomainStmt *)cmd->def)->typeName, ((AlterDomainStmt *)cmd->def)->def, NULL);
    break;
  case AT_ReAddComment:                              
    address = CommentObject((CommentStmt *)cmd->def);
    break;
  case AT_AddIndexConstraint:                                 
    address = ATExecAddIndexConstraint(tab, rel, (IndexStmt *)cmd->def, lockmode);
    break;
  case AT_AlterConstraint:                       
    address = ATExecAlterConstraint(rel, cmd, false, false, lockmode);
    break;
  case AT_ValidateConstraint:                          
    address = ATExecValidateConstraint(wqueue, rel, cmd->name, false, false, lockmode);
    break;
  case AT_ValidateConstraintRecurse:                             
                                                    
    address = ATExecValidateConstraint(wqueue, rel, cmd->name, true, false, lockmode);
    break;
  case AT_DropConstraint:                      
    ATExecDropConstraint(rel, cmd->name, cmd->behavior, false, false, cmd->missing_ok, lockmode);
    break;
  case AT_DropConstraintRecurse:                                     
    ATExecDropConstraint(rel, cmd->name, cmd->behavior, true, false, cmd->missing_ok, lockmode);
    break;
  case AT_AlterColumnType:                        
    address = ATExecAlterColumnType(tab, rel, cmd, lockmode);
    break;
  case AT_AlterColumnGenericOptions:                           
    address = ATExecAlterColumnGenericOptions(rel, cmd->name, (List *)cmd->def, lockmode);
    break;
  case AT_ChangeOwner:                  
    ATExecChangeOwner(RelationGetRelid(rel), get_rolespec_oid(cmd->newowner, false), false, lockmode);
    break;
  case AT_ClusterOn:                 
    address = ATExecClusterOn(rel, cmd->name, lockmode);
    break;
  case AT_DropCluster:                          
    ATExecDropCluster(rel, lockmode);
    break;
  case AT_SetLogged:                   
  case AT_SetUnLogged:                   
    break;
  case AT_DropOids:                       
                                                             
    break;
  case AT_SetTableSpace:                     

       
                                                                       
                                                                  
                                       
       
    if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE || rel->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
    {
      ATExecSetTableSpaceNoStorage(rel, tab->newTableSpace);
    }

    break;
  case AT_SetRelOptions:                    
  case AT_ResetRelOptions:                    
  case AT_ReplaceRelOptions:                                 
    ATExecSetRelOptions(rel, (List *)cmd->def, cmd->subtype, lockmode);
    break;
  case AT_EnableTrig:                          
    ATExecEnableDisableTrigger(rel, cmd->name, TRIGGER_FIRES_ON_ORIGIN, false, cmd->recurse, lockmode);
    break;
  case AT_EnableAlwaysTrig:                                 
    ATExecEnableDisableTrigger(rel, cmd->name, TRIGGER_FIRES_ALWAYS, false, cmd->recurse, lockmode);
    break;
  case AT_EnableReplicaTrig:                                  
    ATExecEnableDisableTrigger(rel, cmd->name, TRIGGER_FIRES_ON_REPLICA, false, cmd->recurse, lockmode);
    break;
  case AT_DisableTrig:                           
    ATExecEnableDisableTrigger(rel, cmd->name, TRIGGER_DISABLED, false, cmd->recurse, lockmode);
    break;
  case AT_EnableTrigAll:                         
    ATExecEnableDisableTrigger(rel, NULL, TRIGGER_FIRES_ON_ORIGIN, false, cmd->recurse, lockmode);
    break;
  case AT_DisableTrigAll:                          
    ATExecEnableDisableTrigger(rel, NULL, TRIGGER_DISABLED, false, cmd->recurse, lockmode);
    break;
  case AT_EnableTrigUser:                          
    ATExecEnableDisableTrigger(rel, NULL, TRIGGER_FIRES_ON_ORIGIN, true, cmd->recurse, lockmode);
    break;
  case AT_DisableTrigUser:                           
    ATExecEnableDisableTrigger(rel, NULL, TRIGGER_DISABLED, true, cmd->recurse, lockmode);
    break;

  case AT_EnableRule:                       
    ATExecEnableDisableRule(rel, cmd->name, RULE_FIRES_ON_ORIGIN, lockmode);
    break;
  case AT_EnableAlwaysRule:                              
    ATExecEnableDisableRule(rel, cmd->name, RULE_FIRES_ALWAYS, lockmode);
    break;
  case AT_EnableReplicaRule:                               
    ATExecEnableDisableRule(rel, cmd->name, RULE_FIRES_ON_REPLICA, lockmode);
    break;
  case AT_DisableRule:                        
    ATExecEnableDisableRule(rel, cmd->name, RULE_DISABLED, lockmode);
    break;

  case AT_AddInherit:
    address = ATExecAddInherit(rel, (RangeVar *)cmd->def, lockmode);
    break;
  case AT_DropInherit:
    address = ATExecDropInherit(rel, (RangeVar *)cmd->def, lockmode);
    break;
  case AT_AddOf:
    address = ATExecAddOf(rel, (TypeName *)cmd->def, lockmode);
    break;
  case AT_DropOf:
    ATExecDropOf(rel, lockmode);
    break;
  case AT_ReplicaIdentity:
    ATExecReplicaIdentity(rel, (ReplicaIdentityStmt *)cmd->def, lockmode);
    break;
  case AT_EnableRowSecurity:
    ATExecEnableRowSecurity(rel);
    break;
  case AT_DisableRowSecurity:
    ATExecDisableRowSecurity(rel);
    break;
  case AT_ForceRowSecurity:
    ATExecForceNoForceRowSecurity(rel, true);
    break;
  case AT_NoForceRowSecurity:
    ATExecForceNoForceRowSecurity(rel, false);
    break;
  case AT_GenericOptions:
    ATExecGenericOptions(rel, (List *)cmd->def);
    break;
  case AT_AttachPartition:
    if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      ATExecAttachPartition(wqueue, rel, (PartitionCmd *)cmd->def);
    }
    else
    {
      ATExecAttachPartitionIdx(wqueue, rel, ((PartitionCmd *)cmd->def)->name);
    }
    break;
  case AT_DetachPartition:
                                              
    Assert(rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);
    ATExecDetachPartition(rel, ((PartitionCmd *)cmd->def)->name);
    break;
  default:           
    elog(ERROR, "unrecognized alter table type: %d", (int)cmd->subtype);
    break;
  }

     
                                                         
     
  EventTriggerCollectAlterTableSubcmd((Node *)cmd, address);

     
                                                                            
                                
     
  CommandCounterIncrement();
}

   
                                        
   
static void
ATRewriteTables(AlterTableStmt *parsetree, List **wqueue, LOCKMODE lockmode)
{
  ListCell *ltab;

                                                                   
  foreach (ltab, *wqueue)
  {
    AlteredTableInfo *tab = (AlteredTableInfo *)lfirst(ltab);

                                                       
    if (!RELKIND_HAS_STORAGE(tab->relkind))
    {
      continue;
    }

       
                                                                        
                                                                         
                                                                          
                                                                       
                                                                    
                 
       
                                                                   
                                                                      
                                                                        
                                                                           
                      
       
    if (tab->newvals != NIL || tab->rewrite > 0)
    {
      Relation rel;

      rel = table_open(tab->relid, NoLock);
      find_composite_type_dependencies(rel->rd_rel->reltype, rel, NULL);
      table_close(rel, NoLock);
    }

       
                                                                         
                                                                       
                                 
       
                                                                   
                                                                    
                                                                         
                                                                          
                                                                         
                                                  
       
    if (tab->rewrite > 0)
    {
                                                    
      Relation OldHeap;
      Oid OIDNewHeap;
      Oid NewTableSpace;
      char persistence;

      OldHeap = table_open(tab->relid, NoLock);

         
                                                                      
                                                                       
                                                             
         
      if (IsSystemRelation(OldHeap))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot rewrite system relation \"%s\"", RelationGetRelationName(OldHeap))));
      }

      if (RelationIsUsedAsCatalogTable(OldHeap))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot rewrite table \"%s\" used as a catalog table", RelationGetRelationName(OldHeap))));
      }

         
                                                                        
                                                    
         
      if (RELATION_IS_OTHER_TEMP(OldHeap))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot rewrite temporary tables of other sessions")));
      }

         
                                                                     
                             
         
      if (tab->newTableSpace)
      {
        NewTableSpace = tab->newTableSpace;
      }
      else
      {
        NewTableSpace = OldHeap->rd_rel->reltablespace;
      }

         
                                                                        
                                  
         
      persistence = tab->chgPersistence ? tab->newrelpersistence : OldHeap->rd_rel->relpersistence;

      table_close(OldHeap, NoLock);

         
                                                                      
                
         
                                                                      
                                                                     
                             
         
                                
         
      if (parsetree)
      {
        EventTriggerTableRewrite((Node *)parsetree, tab->relid, tab->rewrite);
      }

         
                                                                     
         
                                                                       
                                                                      
                                                                     
                                                                        
                                                                        
                                                                      
                                                                 
         
                                                                    
                                                                         
                          
         
      OIDNewHeap = make_new_heap(tab->relid, NewTableSpace, persistence, lockmode);

         
                                                                
                                                                   
                                                                    
         
      ATRewriteTable(tab, OIDNewHeap, lockmode);

         
                                                                        
                                                                      
                                                                        
                                                                         
                                                                        
                                                                
         
      finish_heap_swap(tab->relid, OIDNewHeap, false, false, true, !OidIsValid(tab->newTableSpace), RecentXmin, ReadNextMultiXactId(), persistence);
    }
    else
    {
         
                                                                         
                                                                  
                       
         
      if (tab->constraints != NIL || tab->verify_new_notnull || tab->partition_constraint != NULL)
      {
        ATRewriteTable(tab, InvalidOid, lockmode);
      }

         
                                                                       
                                        
         
      if (tab->newTableSpace)
      {
        ATExecSetTableSpace(tab->relid, tab->newTableSpace, lockmode);
      }
    }
  }

     
                                                                         
                                                                          
                                                                       
                                                                            
                         
     
  foreach (ltab, *wqueue)
  {
    AlteredTableInfo *tab = (AlteredTableInfo *)lfirst(ltab);
    Relation rel = NULL;
    ListCell *lcon;

                                                           
    if (!RELKIND_HAS_STORAGE(tab->relkind))
    {
      continue;
    }

    foreach (lcon, tab->constraints)
    {
      NewConstraint *con = lfirst(lcon);

      if (con->contype == CONSTR_FOREIGN)
      {
        Constraint *fkconstraint = (Constraint *)con->qual;
        Relation refrel;

        if (rel == NULL)
        {
                                                      
          rel = table_open(tab->relid, NoLock);
        }

        refrel = table_open(con->refrelid, RowShareLock);

        validateForeignKeyConstraint(fkconstraint->conname, rel, refrel, con->refindid, con->conid);

           
                                                                   
                                                  
           

        table_close(refrel, NoLock);
      }
    }

    if (rel)
    {
      table_close(rel, NoLock);
    }
  }
}

   
                                             
   
                                                        
   
static void
ATRewriteTable(AlteredTableInfo *tab, Oid OIDNewHeap, LOCKMODE lockmode)
{
  Relation oldrel;
  Relation newrel;
  TupleDesc oldTupDesc;
  TupleDesc newTupDesc;
  bool needscan = false;
  List *notnull_attrs;
  int i;
  ListCell *l;
  EState *estate;
  CommandId mycid;
  BulkInsertState bistate;
  int ti_options;
  ExprState *partqualstate = NULL;

     
                                                                       
            
     
  oldrel = table_open(tab->relid, NoLock);
  oldTupDesc = tab->oldDesc;
  newTupDesc = RelationGetDescr(oldrel);                        

  if (OidIsValid(OIDNewHeap))
  {
    newrel = table_open(OIDNewHeap, lockmode);
  }
  else
  {
    newrel = NULL;
  }

     
                                                                           
                                                                             
                                                                         
                                                                             
     
  if (newrel)
  {
    mycid = GetCurrentCommandId(true);
    bistate = GetBulkInsertState();

    ti_options = TABLE_INSERT_SKIP_FSM;
    if (!XLogIsNeeded())
    {
      ti_options |= TABLE_INSERT_SKIP_WAL;
    }
  }
  else
  {
                                                             
    mycid = 0;
    bistate = NULL;
    ti_options = 0;
  }

     
                                                          
     

  estate = CreateExecutorState();

                                                    
  foreach (l, tab->constraints)
  {
    NewConstraint *con = lfirst(l);

    switch (con->contype)
    {
    case CONSTR_CHECK:
      needscan = true;
      con->qualstate = ExecPrepareExpr((Expr *)con->qual, estate);
      break;
    case CONSTR_FOREIGN:
                              
      break;
    default:
      elog(ERROR, "unrecognized constraint type: %d", (int)con->contype);
    }
  }

                                                                   
  if (tab->partition_constraint)
  {
    needscan = true;
    partqualstate = ExecPrepareExpr(tab->partition_constraint, estate);
  }

  foreach (l, tab->newvals)
  {
    NewColumnValue *ex = lfirst(l);

                              
    ex->exprstate = ExecInitExpr((Expr *)ex->expr, NULL);
  }

  notnull_attrs = NIL;
  if (newrel || tab->verify_new_notnull)
  {
       
                                                                      
                                                                           
                                                               
                                                     
       
    for (i = 0; i < newTupDesc->natts; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(newTupDesc, i);

      if (attr->attnotnull && !attr->attisdropped)
      {
        notnull_attrs = lappend_int(notnull_attrs, i);
      }
    }
    if (notnull_attrs)
    {
      needscan = true;
    }
  }

  if (newrel || needscan)
  {
    ExprContext *econtext;
    TupleTableSlot *oldslot;
    TupleTableSlot *newslot;
    TableScanDesc scan;
    MemoryContext oldCxt;
    List *dropped_attrs = NIL;
    ListCell *lc;
    Snapshot snapshot;

    if (newrel)
    {
      ereport(DEBUG1, (errmsg("rewriting table \"%s\"", RelationGetRelationName(oldrel))));
    }
    else
    {
      ereport(DEBUG1, (errmsg("verifying table \"%s\"", RelationGetRelationName(oldrel))));
    }

    if (newrel)
    {
         
                                                                         
                                                                  
                         
         
      TransferPredicateLocksToHeapRelation(oldrel);
    }

    econtext = GetPerTupleExprContext(estate);

       
                                                                           
                                                                       
                                                                        
                                                                        
                                                                  
                                                
       
    if (tab->rewrite)
    {
      Assert(newrel != NULL);
      oldslot = MakeSingleTupleTableSlot(oldTupDesc, table_slot_callbacks(oldrel));
      newslot = MakeSingleTupleTableSlot(newTupDesc, table_slot_callbacks(newrel));

         
                                                                      
                                                                       
                                                               
                                                                        
                                                                        
                                      
         
      ExecStoreAllNullTuple(newslot);
    }
    else
    {
      oldslot = MakeSingleTupleTableSlot(newTupDesc, table_slot_callbacks(oldrel));
      newslot = NULL;
    }

       
                                                                  
                                                                        
                                                                   
       
    for (i = 0; i < newTupDesc->natts; i++)
    {
      if (TupleDescAttr(newTupDesc, i)->attisdropped)
      {
        dropped_attrs = lappend_int(dropped_attrs, i);
      }
    }

       
                                                                      
                                     
       
    snapshot = RegisterSnapshot(GetLatestSnapshot());
    scan = table_beginscan(oldrel, snapshot, 0, NULL);

       
                                                                      
                                          
       
    oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

    while (table_scan_getnextslot(scan, ForwardScanDirection, oldslot))
    {
      TupleTableSlot *insertslot;

      if (tab->rewrite > 0)
      {
                                         
        slot_getallattrs(oldslot);
        ExecClearTuple(newslot);

                             
        memcpy(newslot->tts_values, oldslot->tts_values, sizeof(Datum) * oldslot->tts_nvalid);
        memcpy(newslot->tts_isnull, oldslot->tts_isnull, sizeof(bool) * oldslot->tts_nvalid);

                                                         
        foreach (lc, dropped_attrs)
        {
          newslot->tts_isnull[lfirst_int(lc)] = true;
        }

           
                                                                     
                                                                  
                                                               
                                                          
           
        newslot->tts_tableOid = RelationGetRelid(oldrel);

           
                                                                     
           
                                                                      
                  
           
        econtext->ecxt_scantuple = oldslot;

        foreach (l, tab->newvals)
        {
          NewColumnValue *ex = lfirst(l);

          if (ex->is_generated)
          {
            continue;
          }

          newslot->tts_values[ex->attnum - 1] = ExecEvalExpr(ex->exprstate, econtext, &newslot->tts_isnull[ex->attnum - 1]);
        }

        ExecStoreVirtualTuple(newslot);

           
                                                                    
                                                                    
                                                          
           
        econtext->ecxt_scantuple = newslot;

        foreach (l, tab->newvals)
        {
          NewColumnValue *ex = lfirst(l);

          if (!ex->is_generated)
          {
            continue;
          }

          newslot->tts_values[ex->attnum - 1] = ExecEvalExpr(ex->exprstate, econtext, &newslot->tts_isnull[ex->attnum - 1]);
        }

        insertslot = newslot;
      }
      else
      {
           
                                                                      
                                                                       
                                
           
        insertslot = oldslot;
      }

                                                                   
      econtext->ecxt_scantuple = insertslot;

      foreach (l, notnull_attrs)
      {
        int attn = lfirst_int(l);

        if (slot_attisnull(insertslot, attn + 1))
        {
          Form_pg_attribute attr = TupleDescAttr(newTupDesc, attn);

          ereport(ERROR, (errcode(ERRCODE_NOT_NULL_VIOLATION), errmsg("column \"%s\" contains null values", NameStr(attr->attname)), errtablecol(oldrel, attn + 1)));
        }
      }

      foreach (l, tab->constraints)
      {
        NewConstraint *con = lfirst(l);

        switch (con->contype)
        {
        case CONSTR_CHECK:
          if (!ExecCheck(con->qualstate, econtext))
          {
            ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("check constraint \"%s\" is violated by some row", con->name), errtableconstraint(oldrel, con->name)));
          }
          break;
        case CONSTR_FOREIGN:
                                  
          break;
        default:
          elog(ERROR, "unrecognized constraint type: %d", (int)con->contype);
        }
      }

      if (partqualstate && !ExecCheck(partqualstate, econtext))
      {
        if (tab->validate_default)
        {
          ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("updated partition constraint for default partition would be violated by some row")));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("partition constraint is violated by some row")));
        }
      }

                                                   
      if (newrel)
      {
        table_tuple_insert(newrel, insertslot, mycid, ti_options, bistate);
      }

      ResetExprContext(econtext);

      CHECK_FOR_INTERRUPTS();
    }

    MemoryContextSwitchTo(oldCxt);
    table_endscan(scan);
    UnregisterSnapshot(snapshot);

    ExecDropSingleTupleTableSlot(oldslot);
    if (newslot)
    {
      ExecDropSingleTupleTableSlot(newslot);
    }
  }

  FreeExecutorState(estate);

  table_close(oldrel, NoLock);
  if (newrel)
  {
    FreeBulkInsertState(bistate);

    table_finish_bulk_insert(newrel, ti_options);

    table_close(newrel, NoLock);
  }
}

   
                                                                          
   
static AlteredTableInfo *
ATGetQueueEntry(List **wqueue, Relation rel)
{
  Oid relid = RelationGetRelid(rel);
  AlteredTableInfo *tab;
  ListCell *ltab;

  foreach (ltab, *wqueue)
  {
    tab = (AlteredTableInfo *)lfirst(ltab);
    if (tab->relid == relid)
    {
      return tab;
    }
  }

     
                                                                       
                                                                       
     
  tab = (AlteredTableInfo *)palloc0(sizeof(AlteredTableInfo));
  tab->relid = relid;
  tab->relkind = rel->rd_rel->relkind;
  tab->oldDesc = CreateTupleDescCopyConstr(RelationGetDescr(rel));
  tab->newrelpersistence = RELPERSISTENCE_PERMANENT;
  tab->chgPersistence = false;

  *wqueue = lappend(*wqueue, tab);

  return tab;
}

   
                       
   
                                                       
                                   
                                          
   
static void
ATSimplePermissions(Relation rel, int allowed_targets)
{
  int actual_target;

  switch (rel->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_PARTITIONED_TABLE:
    actual_target = ATT_TABLE;
    break;
  case RELKIND_VIEW:
    actual_target = ATT_VIEW;
    break;
  case RELKIND_MATVIEW:
    actual_target = ATT_MATVIEW;
    break;
  case RELKIND_INDEX:
    actual_target = ATT_INDEX;
    break;
  case RELKIND_PARTITIONED_INDEX:
    actual_target = ATT_PARTITIONED_INDEX;
    break;
  case RELKIND_COMPOSITE_TYPE:
    actual_target = ATT_COMPOSITE_TYPE;
    break;
  case RELKIND_FOREIGN_TABLE:
    actual_target = ATT_FOREIGN_TABLE;
    break;
  default:
    actual_target = 0;
    break;
  }

                          
  if ((actual_target & allowed_targets) == 0)
  {
    ATWrongRelkindError(rel, allowed_targets);
  }

                          
  if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(rel->rd_rel->relkind), RelationGetRelationName(rel));
  }

  if (!allowSystemTableMods && IsSystemRelation(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", RelationGetRelationName(rel))));
  }
}

   
                       
   
                                                                         
         
   
static void
ATWrongRelkindError(Relation rel, int allowed_targets)
{
  char *msg;

  switch (allowed_targets)
  {
  case ATT_TABLE:
    msg = _("\"%s\" is not a table");
    break;
  case ATT_TABLE | ATT_VIEW:
    msg = _("\"%s\" is not a table or view");
    break;
  case ATT_TABLE | ATT_VIEW | ATT_FOREIGN_TABLE:
    msg = _("\"%s\" is not a table, view, or foreign table");
    break;
  case ATT_TABLE | ATT_VIEW | ATT_MATVIEW | ATT_INDEX:
    msg = _("\"%s\" is not a table, view, materialized view, or index");
    break;
  case ATT_TABLE | ATT_MATVIEW:
    msg = _("\"%s\" is not a table or materialized view");
    break;
  case ATT_TABLE | ATT_MATVIEW | ATT_INDEX:
    msg = _("\"%s\" is not a table, materialized view, or index");
    break;
  case ATT_TABLE | ATT_MATVIEW | ATT_INDEX | ATT_PARTITIONED_INDEX:
    msg = _("\"%s\" is not a table, materialized view, index, or partitioned index");
    break;
  case ATT_TABLE | ATT_MATVIEW | ATT_FOREIGN_TABLE:
    msg = _("\"%s\" is not a table, materialized view, or foreign table");
    break;
  case ATT_TABLE | ATT_FOREIGN_TABLE:
    msg = _("\"%s\" is not a table or foreign table");
    break;
  case ATT_TABLE | ATT_COMPOSITE_TYPE | ATT_FOREIGN_TABLE:
    msg = _("\"%s\" is not a table, composite type, or foreign table");
    break;
  case ATT_TABLE | ATT_MATVIEW | ATT_INDEX | ATT_FOREIGN_TABLE:
    msg = _("\"%s\" is not a table, materialized view, index, or foreign table");
    break;
  case ATT_TABLE | ATT_PARTITIONED_INDEX:
    msg = _("\"%s\" is not a table or partitioned index");
    break;
  case ATT_VIEW:
    msg = _("\"%s\" is not a view");
    break;
  case ATT_FOREIGN_TABLE:
    msg = _("\"%s\" is not a foreign table");
    break;
  default:
                                                           
    msg = _("\"%s\" is of the wrong type");
    break;
  }

  ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg(msg, RelationGetRelationName(rel))));
}

   
                     
   
                                                                      
                                                                           
                                                                      
                                                    
   
static void
ATSimpleRecursion(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, LOCKMODE lockmode)
{
     
                                                                          
               
     
  if (recurse && rel->rd_rel->relhassubclass)
  {
    Oid relid = RelationGetRelid(rel);
    ListCell *child;
    List *children;

    children = find_all_inheritors(relid, lockmode, NULL);

       
                                                                        
                                                                           
                             
       
    foreach (child, children)
    {
      Oid childrelid = lfirst_oid(child);
      Relation childrel;

      if (childrelid == relid)
      {
        continue;
      }
                                                
      childrel = relation_open(childrelid, NoLock);
      CheckTableNotInUse(childrel, "ALTER TABLE");
      ATPrepCmd(wqueue, childrel, cmd, false, true, lockmode);
      relation_close(childrel, NoLock);
    }
  }
}

   
                                                                               
                                                                
   
                                                                              
                                                                       
   
static void
ATCheckPartitionsNotInUse(Relation rel, LOCKMODE lockmode)
{
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    List *inh;
    ListCell *cell;

    inh = find_all_inheritors(RelationGetRelid(rel), lockmode, NULL);
                                                         
    for_each_cell(cell, lnext(list_head(inh)))
    {
      Relation childrel;

                                                
      childrel = table_open(lfirst_oid(cell), NoLock);
      CheckTableNotInUse(childrel, "ALTER TABLE");
      table_close(childrel, NoLock);
    }
    list_free(inh);
  }
}

   
                         
   
                                                                     
                                                                         
                                                          
   
static void
ATTypedTableRecursion(List **wqueue, Relation rel, AlterTableCmd *cmd, LOCKMODE lockmode)
{
  ListCell *child;
  List *children;

  Assert(rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE);

  children = find_typed_table_dependencies(rel->rd_rel->reltype, RelationGetRelationName(rel), cmd->behavior);

  foreach (child, children)
  {
    Oid childrelid = lfirst_oid(child);
    Relation childrel;

    childrel = relation_open(childrelid, lockmode);
    CheckTableNotInUse(childrel, "ALTER TABLE");
    ATPrepCmd(wqueue, childrel, cmd, true, true, lockmode);
    relation_close(childrel, NoLock);
  }
}

   
                                    
   
                                                                              
                                                                           
                                                                     
                                                                 
   
                                                                      
                                                                   
   
                                                                             
                                                                             
                                                                              
                                
   
                                                                            
                                                    
   
void
find_composite_type_dependencies(Oid typeOid, Relation origRelation, const char *origTypeName)
{
  Relation depRel;
  ScanKeyData key[2];
  SysScanDesc depScan;
  HeapTuple depTup;

                                                                          
  check_stack_depth();

     
                                                                           
                                                       
     
  depRel = table_open(DependRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_depend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(TypeRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(typeOid));

  depScan = systable_beginscan(depRel, DependReferenceIndexId, true, NULL, 2, key);

  while (HeapTupleIsValid(depTup = systable_getnext(depScan)))
  {
    Form_pg_depend pg_depend = (Form_pg_depend)GETSTRUCT(depTup);
    Relation rel;
    Form_pg_attribute att;

                                            
    if (pg_depend->classid == TypeRelationId)
    {
         
                                                                      
                                                                      
                                                                  
                                         
         
      find_composite_type_dependencies(pg_depend->objid, origRelation, origTypeName);
      continue;
    }

                                                                      
                                                                   
    if (pg_depend->classid != RelationRelationId || pg_depend->objsubid <= 0)
    {
      continue;
    }

    rel = relation_open(pg_depend->objid, AccessShareLock);
    att = TupleDescAttr(rel->rd_att, pg_depend->objsubid - 1);

    if (rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_MATVIEW || rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      if (origTypeName)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter type \"%s\" because column \"%s.%s\" uses it", origTypeName, RelationGetRelationName(rel), NameStr(att->attname))));
      }
      else if (origRelation->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter type \"%s\" because column \"%s.%s\" uses it", RelationGetRelationName(origRelation), RelationGetRelationName(rel), NameStr(att->attname))));
      }
      else if (origRelation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter foreign table \"%s\" because column \"%s.%s\" uses its row type", RelationGetRelationName(origRelation), RelationGetRelationName(rel), NameStr(att->attname))));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter table \"%s\" because column \"%s.%s\" uses its row type", RelationGetRelationName(origRelation), RelationGetRelationName(rel), NameStr(att->attname))));
      }
    }
    else if (OidIsValid(rel->rd_rel->reltype))
    {
         
                                                                      
                                                                      
         
      find_composite_type_dependencies(rel->rd_rel->reltype, origRelation, origTypeName);
    }

    relation_close(rel, AccessShareLock);
  }

  systable_endscan(depScan);

  relation_close(depRel, AccessShareLock);
}

   
                                 
   
                                                                   
                                                                  
                                   
   
static List *
find_typed_table_dependencies(Oid typeOid, const char *typeName, DropBehavior behavior)
{
  Relation classRel;
  ScanKeyData key[1];
  TableScanDesc scan;
  HeapTuple tuple;
  List *result = NIL;

  classRel = table_open(RelationRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_class_reloftype, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(typeOid));

  scan = table_beginscan_catalog(classRel, 1, key);

  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class classform = (Form_pg_class)GETSTRUCT(tuple);

    if (behavior == DROP_RESTRICT)
    {
      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot alter type \"%s\" because it is the type of a typed table", typeName), errhint("Use ALTER ... CASCADE to alter the typed tables too.")));
    }
    else
    {
      result = lappend_oid(result, classform->oid);
    }
  }

  table_endscan(scan);
  table_close(classRel, AccessShareLock);

  return result;
}

   
                 
   
                                                                               
                                                                        
                                                                                
                                                                              
                                                                            
                                                                             
   
void
check_of_type(HeapTuple typetuple)
{
  Form_pg_type typ = (Form_pg_type)GETSTRUCT(typetuple);
  bool typeOk = false;

  if (typ->typtype == TYPTYPE_COMPOSITE)
  {
    Relation typeRelation;

    Assert(OidIsValid(typ->typrelid));
    typeRelation = relation_open(typ->typrelid, AccessShareLock);
    typeOk = (typeRelation->rd_rel->relkind == RELKIND_COMPOSITE_TYPE);

       
                                                                           
                                                                         
                                                                    
       
    relation_close(typeRelation, NoLock);
  }
  if (!typeOk)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("type %s is not a composite type", format_type_be(typ->oid))));
  }
}

   
                          
   
                                                                         
                                                                         
                                                                          
                    
   
                                                                                
                                                                               
                                                                             
                                                                                
                          
   
static void
ATPrepAddColumn(List **wqueue, Relation rel, bool recurse, bool recursing, bool is_view, AlterTableCmd *cmd, LOCKMODE lockmode)
{
  if (rel->rd_rel->reloftype && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot add column to typed table")));
  }

  if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
  {
    ATTypedTableRecursion(wqueue, rel, cmd, lockmode);
  }

  if (recurse && !is_view)
  {
    cmd->subtype = AT_AddColumnRecurse;
  }
}

   
                                                                    
                                      
   
static ObjectAddress
ATExecAddColumn(List **wqueue, AlteredTableInfo *tab, Relation rel, ColumnDef *colDef, bool recurse, bool recursing, bool if_not_exists, LOCKMODE lockmode)
{
  Oid myrelid = RelationGetRelid(rel);
  Relation pgclass, attrdesc;
  HeapTuple reltup;
  FormData_pg_attribute attribute;
  int newattnum;
  char relkind;
  HeapTuple typeTuple;
  Oid typeOid;
  int32 typmod;
  Oid collOid;
  Form_pg_type tform;
  Expr *defval;
  List *children;
  ListCell *child;
  AclResult aclresult;
  ObjectAddress address;

                                                                        
  if (recursing)
  {
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
  }

  if (rel->rd_rel->relispartition && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot add column to a partition")));
  }

  attrdesc = table_open(AttributeRelationId, RowExclusiveLock);

     
                                                                             
                                                                           
                                                                             
                                          
     
  if (colDef->inhcount > 0)
  {
    HeapTuple tuple;

                                                        
    tuple = SearchSysCacheCopyAttName(myrelid, colDef->colname);
    if (HeapTupleIsValid(tuple))
    {
      Form_pg_attribute childatt = (Form_pg_attribute)GETSTRUCT(tuple);
      Oid ctypeId;
      int32 ctypmod;
      Oid ccollid;

                                                                  
      typenameTypeIdAndMod(NULL, colDef->typeName, &ctypeId, &ctypmod);
      if (ctypeId != childatt->atttypid || ctypmod != childatt->atttypmod)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("child table \"%s\" has different type for column \"%s\"", RelationGetRelationName(rel), colDef->colname)));
      }
      ccollid = GetColumnDefCollation(NULL, colDef, ctypeId);
      if (ccollid != childatt->attcollation)
      {
        ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("child table \"%s\" has different collation for column \"%s\"", RelationGetRelationName(rel), colDef->colname), errdetail("\"%s\" versus \"%s\"", get_collation_name(ccollid), get_collation_name(childatt->attcollation))));
      }

                                                  
      childatt->attinhcount++;
      CatalogTupleUpdate(attrdesc, &tuple->t_self, tuple);

      heap_freetuple(tuple);

                                           
      ereport(NOTICE, (errmsg("merging definition of column \"%s\" for child \"%s\"", colDef->colname, RelationGetRelationName(rel))));

      table_close(attrdesc, RowExclusiveLock);
      return InvalidObjectAddress;
    }
  }

  pgclass = table_open(RelationRelationId, RowExclusiveLock);

  reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(myrelid));
  if (!HeapTupleIsValid(reltup))
  {
    elog(ERROR, "cache lookup failed for relation %u", myrelid);
  }
  relkind = ((Form_pg_class)GETSTRUCT(reltup))->relkind;

     
                                                                             
                                                                      
     
  if (colDef->identity && recurse && find_inheritance_children(myrelid, NoLock) != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot recursively add identity column to table that has child tables")));
  }

                                                                 
  if (!check_for_column_name_collision(rel, colDef->colname, if_not_exists))
  {
    table_close(attrdesc, RowExclusiveLock);
    heap_freetuple(reltup);
    table_close(pgclass, RowExclusiveLock);
    return InvalidObjectAddress;
  }

                                            
  newattnum = ((Form_pg_class)GETSTRUCT(reltup))->relnatts + 1;
  if (newattnum > MaxHeapAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("tables can have at most %d columns", MaxHeapAttributeNumber)));
  }

  typeTuple = typenameType(NULL, colDef->typeName, &typmod);
  tform = (Form_pg_type)GETSTRUCT(typeTuple);
  typeOid = tform->oid;

  aclresult = pg_type_aclcheck(typeOid, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, typeOid);
  }

  collOid = GetColumnDefCollation(NULL, colDef, typeOid);

                                                
  CheckAttributeType(colDef->colname, typeOid, collOid, list_make1_oid(rel->rd_rel->reltype), 0);

                                                    
  attribute.attrelid = myrelid;
  namestrcpy(&(attribute.attname), colDef->colname);
  attribute.atttypid = typeOid;
  attribute.attstattarget = (newattnum > 0) ? -1 : 0;
  attribute.attlen = tform->typlen;
  attribute.atttypmod = typmod;
  attribute.attnum = newattnum;
  attribute.attbyval = tform->typbyval;
  attribute.attndims = list_length(colDef->typeName->arrayBounds);
  attribute.attstorage = tform->typstorage;
  attribute.attalign = tform->typalign;
  attribute.attnotnull = colDef->is_not_null;
  attribute.atthasdef = false;
  attribute.atthasmissing = false;
  attribute.attidentity = colDef->identity;
  attribute.attgenerated = colDef->generated;
  attribute.attisdropped = false;
  attribute.attislocal = colDef->is_local;
  attribute.attinhcount = colDef->inhcount;
  attribute.attcollation = collOid;
                                                             

  ReleaseSysCache(typeTuple);

  InsertPgAttributeTuple(attrdesc, &attribute, NULL);

  table_close(attrdesc, RowExclusiveLock);

     
                                          
     
  ((Form_pg_class)GETSTRUCT(reltup))->relnatts = newattnum;

  CatalogTupleUpdate(pgclass, &reltup->t_self, reltup);

  heap_freetuple(reltup);

                                            
  InvokeObjectPostCreateHook(RelationRelationId, myrelid, newattnum);

  table_close(pgclass, RowExclusiveLock);

                                                  
  CommandCounterIncrement();

     
                                                
     
  if (colDef->raw_default)
  {
    RawColumnDefault *rawEnt;

    rawEnt = (RawColumnDefault *)palloc(sizeof(RawColumnDefault));
    rawEnt->attnum = attribute.attnum;
    rawEnt->raw_default = copyObject(colDef->raw_default);

       
                                                                         
                                                                       
                                                                        
       
    rawEnt->missingMode = (!colDef->generated);

    rawEnt->generated = colDef->generated;

       
                                                                     
                                               
       
    AddRelationNewConstraints(rel, list_make1(rawEnt), NIL, false, true, false, NULL);

                                                     
    CommandCounterIncrement();

       
                                                                           
               
       
    if (!rawEnt->missingMode)
    {
      tab->rewrite |= AT_REWRITE_DEFAULT_VAL;
    }
  }

     
                                                                      
     
                                                                          
                                                                             
                                                                             
                                                                           
                                                  
     
                                                                             
                                                                             
                                                                           
                                                                
                                                                             
                                                                           
                                                                          
                                     
     
                                                                        
                                                                            
                                        
     
                                                                             
                                                                          
                                                                             
                                                                           
                      
     
  if (relkind != RELKIND_VIEW && relkind != RELKIND_COMPOSITE_TYPE && relkind != RELKIND_FOREIGN_TABLE && attribute.attnum > 0)
  {
       
                                                                    
                                                                         
       
    if (colDef->identity)
    {
      NextValueExpr *nve = makeNode(NextValueExpr);

      nve->seqid = RangeVarGetRelid(colDef->identitySequence, NoLock, false);
      nve->typeId = typeOid;

      defval = (Expr *)nve;

                                                  
      tab->rewrite |= AT_REWRITE_DEFAULT_VAL;
    }
    else
    {
      defval = (Expr *)build_column_default(rel, attribute.attnum);
    }

    if (!defval && DomainHasConstraints(typeOid))
    {
      Oid baseTypeId;
      int32 baseTypeMod;
      Oid baseTypeColl;

      baseTypeMod = typmod;
      baseTypeId = getBaseTypeAndTypmod(typeOid, &baseTypeMod);
      baseTypeColl = get_typcollation(baseTypeId);
      defval = (Expr *)makeNullConst(baseTypeId, baseTypeMod, baseTypeColl);
      defval = (Expr *)coerce_to_target_type(NULL, (Node *)defval, baseTypeId, typeOid, typmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
      if (defval == NULL)                        
      {
        elog(ERROR, "failed to coerce base type to domain");
      }
    }

    if (defval)
    {
      NewColumnValue *newval;

      newval = (NewColumnValue *)palloc0(sizeof(NewColumnValue));
      newval->attnum = attribute.attnum;
      newval->expr = expression_planner(defval);
      newval->is_generated = (colDef->generated != '\0');

      tab->newvals = lappend(tab->newvals, newval);
    }

    if (DomainHasConstraints(typeOid))
    {
      tab->rewrite |= AT_REWRITE_DEFAULT_VAL;
    }

    if (!TupleDescAttr(rel->rd_att, attribute.attnum - 1)->atthasmissing)
    {
         
                                                                       
                                                   
         
      tab->verify_new_notnull |= colDef->is_not_null;
    }
  }

     
                                                       
     
  add_column_datatype_dependency(myrelid, newattnum, attribute.atttypid);
  add_column_collation_dependency(myrelid, newattnum, attribute.attcollation);

     
                                                                    
                                                                             
                                                   
     
  children = find_inheritance_children(RelationGetRelid(rel), lockmode);

     
                                                                      
                                                           
     
  if (children && !recurse)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column must be added to child tables too")));
  }

                                                      
  if (!recursing)
  {
    colDef = copyObject(colDef);
    colDef->inhcount = 1;
    colDef->is_local = false;
  }

  foreach (child, children)
  {
    Oid childrelid = lfirst_oid(child);
    Relation childrel;
    AlteredTableInfo *childtab;

                                                    
    childrel = table_open(childrelid, NoLock);
    CheckTableNotInUse(childrel, "ALTER TABLE");

                                                        
    childtab = ATGetQueueEntry(wqueue, childrel);

                                                   
    ATExecAddColumn(wqueue, childtab, childrel, colDef, recurse, true, if_not_exists, lockmode);

    table_close(childrel, NoLock);
  }

  ObjectAddressSubSet(address, RelationRelationId, myrelid, newattnum);
  return address;
}

   
                                                                        
                                                                      
   
static bool
check_for_column_name_collision(Relation rel, const char *colname, bool if_not_exists)
{
  HeapTuple attTuple;
  int attnum;

     
                                                                             
                                                                          
     
  attTuple = SearchSysCache2(ATTNAME, ObjectIdGetDatum(RelationGetRelid(rel)), PointerGetDatum(colname));
  if (!HeapTupleIsValid(attTuple))
  {
    return true;
  }

  attnum = ((Form_pg_attribute)GETSTRUCT(attTuple))->attnum;
  ReleaseSysCache(attTuple);

     
                                                                         
                                                                           
                                                    
     
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column name \"%s\" conflicts with a system column name", colname)));
  }
  else
  {
    if (if_not_exists)
    {
      ereport(NOTICE, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" of relation \"%s\" already exists, skipping", colname, RelationGetRelationName(rel))));
      return false;
    }

    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" of relation \"%s\" already exists", colname, RelationGetRelationName(rel))));
  }

  return true;
}

   
                                                  
   
static void
add_column_datatype_dependency(Oid relid, int32 attnum, Oid typid)
{
  ObjectAddress myself, referenced;

  myself.classId = RelationRelationId;
  myself.objectId = relid;
  myself.objectSubId = attnum;
  referenced.classId = TypeRelationId;
  referenced.objectId = typid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
}

   
                                                   
   
static void
add_column_collation_dependency(Oid relid, int32 attnum, Oid collid)
{
  ObjectAddress myself, referenced;

                                                                             
  if (OidIsValid(collid) && collid != DEFAULT_COLLATION_OID)
  {
    myself.classId = RelationRelationId;
    myself.objectId = relid;
    myself.objectSubId = attnum;
    referenced.classId = CollationRelationId;
    referenced.objectId = collid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }
}

   
                                          
   

static void
ATPrepDropNotNull(Relation rel, bool recurse, bool recursing)
{
     
                                                                             
                                                           
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    PartitionDesc partdesc = RelationGetPartitionDesc(rel);

    Assert(partdesc != NULL);
    if (partdesc->nparts > 0 && !recurse && !recursing)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot remove constraint from only the partitioned table when partitions exist"), errhint("Do not specify the ONLY keyword.")));
    }
  }
}

   
                                                                         
                                               
   
static ObjectAddress
ATExecDropNotNull(Relation rel, const char *colName, LOCKMODE lockmode)
{
  HeapTuple tuple;
  Form_pg_attribute attTup;
  AttrNumber attnum;
  Relation attr_rel;
  List *indexoidlist;
  ListCell *indexoidscan;
  ObjectAddress address;

     
                          
     
  attr_rel = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }
  attTup = (Form_pg_attribute)GETSTRUCT(tuple);
  attnum = attTup->attnum;

                                                     
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

  if (attTup->attidentity)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("column \"%s\" of relation \"%s\" is an identity column", colName, RelationGetRelationName(rel))));
  }

     
                                                                             
                         
     
                                                                  
     

                                             
  indexoidlist = RelationGetIndexList(rel);

  foreach (indexoidscan, indexoidlist)
  {
    Oid indexoid = lfirst_oid(indexoidscan);
    HeapTuple indexTuple;
    Form_pg_index indexStruct;
    int i;

    indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexoid);
    }
    indexStruct = (Form_pg_index)GETSTRUCT(indexTuple);

       
                                                                     
                                 
       
    if (indexStruct->indisprimary || indexStruct->indisreplident)
    {
         
                                                                       
                                                                     
                    
         
      for (i = 0; i < indexStruct->indnkeyatts; i++)
      {
        if (indexStruct->indkey.values[i] == attnum)
        {
          if (indexStruct->indisprimary)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" is in a primary key", colName)));
          }
          else
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" is in index used as replica identity", colName)));
          }
        }
      }
    }

    ReleaseSysCache(indexTuple);
  }

  list_free(indexoidlist);

                                                                           
  if (rel->rd_rel->relispartition)
  {
    Oid parentId = get_partition_parent(RelationGetRelid(rel));
    Relation parent = table_open(parentId, AccessShareLock);
    TupleDesc tupDesc = RelationGetDescr(parent);
    AttrNumber parent_attnum;

    parent_attnum = get_attnum(parentId, colName);
    if (TupleDescAttr(tupDesc, parent_attnum - 1)->attnotnull)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" is marked NOT NULL in parent table", colName)));
    }
    table_close(parent, AccessShareLock);
  }

     
                                                             
     
  if (attTup->attnotnull)
  {
    attTup->attnotnull = false;

    CatalogTupleUpdate(attr_rel, &tuple->t_self, tuple);

    ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  }
  else
  {
    address = InvalidObjectAddress;
  }

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attnum);

  table_close(attr_rel, RowExclusiveLock);

  return address;
}

   
                                         
   

static void
ATPrepSetNotNull(List **wqueue, Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing, LOCKMODE lockmode)
{
     
                                                                    
                                                                   
     
  if (recursing)
  {
    return;
  }

     
                                                                            
                                                                             
                                                                          
                                                                           
                                                                       
                                                                        
                         
     
                                                                       
                                                                        
                                                                          
                         
     
  if (rel->rd_rel->relhassubclass && rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    HeapTuple tuple;
    bool attnotnull;

    tuple = SearchSysCacheAttName(RelationGetRelid(rel), cmd->name);

                                                           
    if (!HeapTupleIsValid(tuple))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", cmd->name, RelationGetRelationName(rel))));
    }

    attnotnull = ((Form_pg_attribute)GETSTRUCT(tuple))->attnotnull;
    ReleaseSysCache(tuple);
    if (attnotnull)
    {
      return;
    }
  }

     
                                                                          
                                                                          
                             
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && !recurse)
  {
    AlterTableCmd *newcmd = makeNode(AlterTableCmd);

    newcmd->subtype = AT_CheckNotNull;
    newcmd->name = pstrdup(cmd->name);
    ATSimpleRecursion(wqueue, rel, newcmd, true, lockmode);
  }
  else
  {
    ATSimpleRecursion(wqueue, rel, cmd, recurse, lockmode);
  }
}

   
                                                                             
                                           
   
static ObjectAddress
ATExecSetNotNull(AlteredTableInfo *tab, Relation rel, const char *colName, LOCKMODE lockmode)
{
  HeapTuple tuple;
  AttrNumber attnum;
  Relation attr_rel;
  ObjectAddress address;

     
                          
     
  attr_rel = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }

  attnum = ((Form_pg_attribute)GETSTRUCT(tuple))->attnum;

                                                     
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

     
                                                             
     
  if (!((Form_pg_attribute)GETSTRUCT(tuple))->attnotnull)
  {
    ((Form_pg_attribute)GETSTRUCT(tuple))->attnotnull = true;

    CatalogTupleUpdate(attr_rel, &tuple->t_self, tuple);

       
                                                                          
                                                                           
                                                                       
                                                                         
       
    if (!tab->verify_new_notnull && !NotNullImpliedByRelConstraints(rel, (Form_pg_attribute)GETSTRUCT(tuple)))
    {
                                                        
      tab->verify_new_notnull = true;
    }

    ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  }
  else
  {
    address = InvalidObjectAddress;
  }

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attnum);

  table_close(attr_rel, RowExclusiveLock);

  return address;
}

   
                                           
   
                                                                      
                                                                      
                                                                      
                                                                    
                                                                     
                                                                    
                                                             
   
                                                                        
                                                                       
                                                            
   
static void
ATExecCheckNotNull(AlteredTableInfo *tab, Relation rel, const char *colName, LOCKMODE lockmode)
{
  HeapTuple tuple;

  tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);

  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }

  if (!((Form_pg_attribute)GETSTRUCT(tuple))->attnotnull)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraint must be added to child tables too"), errdetail("Column \"%s\" of relation \"%s\" is not already NOT NULL.", colName, RelationGetRelationName(rel)), errhint("Do not specify the ONLY keyword.")));
  }

  ReleaseSysCache(tuple);
}

   
                                  
                                                                            
   
static bool
NotNullImpliedByRelConstraints(Relation rel, Form_pg_attribute attr)
{
  NullTest *nnulltest = makeNode(NullTest);

  nnulltest->arg = (Expr *)makeVar(1, attr->attnum, attr->atttypid, attr->atttypmod, attr->attcollation, 0);
  nnulltest->nulltesttype = IS_NOT_NULL;

     
                                                                      
                                                                         
                                       
     
  nnulltest->argisrow = false;
  nnulltest->location = -1;

  if (ConstraintImpliedByRelConstraint(rel, list_make1(nnulltest), NIL))
  {
    ereport(DEBUG1, (errmsg("existing constraints on column \"%s\".\"%s\" are sufficient to prove that it does not contain nulls", RelationGetRelationName(rel), NameStr(attr->attname))));
    return true;
  }

  return false;
}

   
                                             
   
                                              
   
static ObjectAddress
ATExecColumnDefault(Relation rel, const char *colName, Node *newDefault, LOCKMODE lockmode)
{
  TupleDesc tupdesc = RelationGetDescr(rel);
  AttrNumber attnum;
  ObjectAddress address;

     
                                     
     
  attnum = get_attnum(RelationGetRelid(rel), colName);
  if (attnum == InvalidAttrNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }

                                                     
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

  if (TupleDescAttr(tupdesc, attnum - 1)->attidentity)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("column \"%s\" of relation \"%s\" is an identity column", colName, RelationGetRelationName(rel)), newDefault ? 0 : errhint("Use ALTER TABLE ... ALTER COLUMN ... DROP IDENTITY instead.")));
  }

  if (TupleDescAttr(tupdesc, attnum - 1)->attgenerated)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("column \"%s\" of relation \"%s\" is a generated column", colName, RelationGetRelationName(rel))));
  }

     
                                                                      
                                                                       
              
     
                                                                             
                                                                     
                                               
     
  RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, false, newDefault == NULL ? false : true);

  if (newDefault)
  {
                     
    RawColumnDefault *rawEnt;

    rawEnt = (RawColumnDefault *)palloc(sizeof(RawColumnDefault));
    rawEnt->attnum = attnum;
    rawEnt->raw_default = newDefault;
    rawEnt->missingMode = false;
    rawEnt->generated = '\0';

       
                                                                     
                                               
       
    AddRelationNewConstraints(rel, list_make1(rawEnt), NIL, false, true, false, NULL);
  }

  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  return address;
}

   
                                        
   
                                              
   
static ObjectAddress
ATExecCookedColumnDefault(Relation rel, AttrNumber attnum, Node *newDefault)
{
  ObjectAddress address;

                                         

     
                                                                      
                                                                       
                                                                         
                                                                      
     
  RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, false, true);

  (void)StoreAttrDefault(rel, attnum, newDefault, true, false);

  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  return address;
}

   
                                         
   
                                              
   
static ObjectAddress
ATExecAddIdentity(Relation rel, const char *colName, Node *def, LOCKMODE lockmode)
{
  Relation attrelation;
  HeapTuple tuple;
  Form_pg_attribute attTup;
  AttrNumber attnum;
  ObjectAddress address;
  ColumnDef *cdef = castNode(ColumnDef, def);

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }
  attTup = (Form_pg_attribute)GETSTRUCT(tuple);
  attnum = attTup->attnum;

                                      
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

     
                                                                            
                                                                          
                                               
     
  if (!attTup->attnotnull)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("column \"%s\" of relation \"%s\" must be declared NOT NULL before identity can be added", colName, RelationGetRelationName(rel))));
  }

  if (attTup->attidentity)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("column \"%s\" of relation \"%s\" is already an identity column", colName, RelationGetRelationName(rel))));
  }

  if (attTup->atthasdef)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("column \"%s\" of relation \"%s\" already has a default value", colName, RelationGetRelationName(rel))));
  }

  attTup->attidentity = cdef->identity;
  CatalogTupleUpdate(attrelation, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attTup->attnum);
  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  heap_freetuple(tuple);

  table_close(attrelation, RowExclusiveLock);

  return address;
}

   
                                                                  
   
                                              
   
static ObjectAddress
ATExecSetIdentity(Relation rel, const char *colName, Node *def, LOCKMODE lockmode)
{
  ListCell *option;
  DefElem *generatedEl = NULL;
  HeapTuple tuple;
  Form_pg_attribute attTup;
  AttrNumber attnum;
  Relation attrelation;
  ObjectAddress address;

  foreach (option, castNode(List, def))
  {
    DefElem *defel = lfirst_node(DefElem, option);

    if (strcmp(defel->defname, "generated") == 0)
    {
      if (generatedEl)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }
      generatedEl = defel;
    }
    else
    {
      elog(ERROR, "option \"%s\" not recognized", defel->defname);
    }
  }

     
                                                                            
                                                                         
            
     

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }

  attTup = (Form_pg_attribute)GETSTRUCT(tuple);
  attnum = attTup->attnum;

  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

  if (!attTup->attidentity)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("column \"%s\" of relation \"%s\" is not an identity column", colName, RelationGetRelationName(rel))));
  }

  if (generatedEl)
  {
    attTup->attidentity = defGetInt32(generatedEl);
    CatalogTupleUpdate(attrelation, &tuple->t_self, tuple);

    InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attTup->attnum);
    ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  }
  else
  {
    address = InvalidObjectAddress;
  }

  heap_freetuple(tuple);
  table_close(attrelation, RowExclusiveLock);

  return address;
}

   
                                          
   
                                              
   
static ObjectAddress
ATExecDropIdentity(Relation rel, const char *colName, bool missing_ok, LOCKMODE lockmode)
{
  HeapTuple tuple;
  Form_pg_attribute attTup;
  AttrNumber attnum;
  Relation attrelation;
  ObjectAddress address;
  Oid seqid;
  ObjectAddress seqaddress;

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }

  attTup = (Form_pg_attribute)GETSTRUCT(tuple);
  attnum = attTup->attnum;

  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

  if (!attTup->attidentity)
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("column \"%s\" of relation \"%s\" is not an identity column", colName, RelationGetRelationName(rel))));
    }
    else
    {
      ereport(NOTICE, (errmsg("column \"%s\" of relation \"%s\" is not an identity column, skipping", colName, RelationGetRelationName(rel))));
      heap_freetuple(tuple);
      table_close(attrelation, RowExclusiveLock);
      return InvalidObjectAddress;
    }
  }

  attTup->attidentity = '\0';
  CatalogTupleUpdate(attrelation, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attTup->attnum);
  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  heap_freetuple(tuple);

  table_close(attrelation, RowExclusiveLock);

                                  
  seqid = getOwnedSequence(RelationGetRelid(rel), attnum);
  deleteDependencyRecordsForClass(RelationRelationId, seqid, RelationRelationId, DEPENDENCY_INTERNAL);
  CommandCounterIncrement();
  seqaddress.classId = RelationRelationId;
  seqaddress.objectId = seqid;
  seqaddress.objectSubId = 0;
  performDeletion(&seqaddress, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);

  return address;
}

   
                                           
   
static void
ATPrepSetStatistics(Relation rel, const char *colName, int16 colNum, Node *newValue, LOCKMODE lockmode)
{
     
                                                                        
                                                                             
                                                                  
                                           
     
  if (rel->rd_rel->relkind != RELKIND_RELATION && rel->rd_rel->relkind != RELKIND_MATVIEW && rel->rd_rel->relkind != RELKIND_INDEX && rel->rd_rel->relkind != RELKIND_PARTITIONED_INDEX && rel->rd_rel->relkind != RELKIND_FOREIGN_TABLE && rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, materialized view, index, or foreign table", RelationGetRelationName(rel))));
  }

     
                                                                           
                                                                     
     
  if (rel->rd_rel->relkind != RELKIND_INDEX && rel->rd_rel->relkind != RELKIND_PARTITIONED_INDEX && !colName)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot refer to non-index column by number")));
  }

                          
  if (!pg_class_ownercheck(RelationGetRelid(rel), GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(rel->rd_rel->relkind), RelationGetRelationName(rel));
  }
}

   
                                                      
   
static ObjectAddress
ATExecSetStatistics(Relation rel, const char *colName, int16 colNum, Node *newValue, LOCKMODE lockmode)
{
  int newtarget;
  Relation attrelation;
  HeapTuple tuple;
  Form_pg_attribute attrtuple;
  AttrNumber attnum;
  ObjectAddress address;

  Assert(IsA(newValue, Integer));
  newtarget = intVal(newValue);

     
                                  
     
  if (newtarget < -1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("statistics target %d is too low", newtarget)));
  }
  else if (newtarget > 10000)
  {
    newtarget = 10000;
    ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("lowering statistics target to %d", newtarget)));
  }

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);

  if (colName)
  {
    tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

    if (!HeapTupleIsValid(tuple))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
    }
  }
  else
  {
    tuple = SearchSysCacheCopyAttNum(RelationGetRelid(rel), colNum);

    if (!HeapTupleIsValid(tuple))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column number %d of relation \"%s\" does not exist", colNum, RelationGetRelationName(rel))));
    }
  }

  attrtuple = (Form_pg_attribute)GETSTRUCT(tuple);

  attnum = attrtuple->attnum;
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

  if (rel->rd_rel->relkind == RELKIND_INDEX || rel->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
  {
    if (attnum > rel->rd_index->indnkeyatts)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter statistics on included column \"%s\" of index \"%s\"", NameStr(attrtuple->attname), RelationGetRelationName(rel))));
    }
    else if (rel->rd_index->indkey.values[attnum - 1] != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter statistics on non-expression column \"%s\" of index \"%s\"", NameStr(attrtuple->attname), RelationGetRelationName(rel)), errhint("Alter statistics on table column instead.")));
    }
  }

  attrtuple->attstattarget = newtarget;

  CatalogTupleUpdate(attrelation, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attrtuple->attnum);
  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  heap_freetuple(tuple);

  table_close(attrelation, RowExclusiveLock);

  return address;
}

   
                                                      
   
static ObjectAddress
ATExecSetOptions(Relation rel, const char *colName, Node *options, bool isReset, LOCKMODE lockmode)
{
  Relation attrelation;
  HeapTuple tuple, newtuple;
  Form_pg_attribute attrtuple;
  AttrNumber attnum;
  Datum datum, newOptions;
  bool isnull;
  ObjectAddress address;
  Datum repl_val[Natts_pg_attribute];
  bool repl_null[Natts_pg_attribute];
  bool repl_repl[Natts_pg_attribute];

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);

  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }
  attrtuple = (Form_pg_attribute)GETSTRUCT(tuple);

  attnum = attrtuple->attnum;
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

                                                     
  datum = SysCacheGetAttr(ATTNAME, tuple, Anum_pg_attribute_attoptions, &isnull);
  newOptions = transformRelOptions(isnull ? (Datum)0 : datum, castNode(List, options), NULL, NULL, false, isReset);
                            
  (void)attribute_reloptions(newOptions, true);

                        
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));
  if (newOptions != (Datum)0)
  {
    repl_val[Anum_pg_attribute_attoptions - 1] = newOptions;
  }
  else
  {
    repl_null[Anum_pg_attribute_attoptions - 1] = true;
  }
  repl_repl[Anum_pg_attribute_attoptions - 1] = true;
  newtuple = heap_modify_tuple(tuple, RelationGetDescr(attrelation), repl_val, repl_null, repl_repl);

                              
  CatalogTupleUpdate(attrelation, &newtuple->t_self, newtuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attrtuple->attnum);
  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);

  heap_freetuple(newtuple);

  ReleaseSysCache(tuple);

  table_close(attrelation, RowExclusiveLock);

  return address;
}

   
                                        
   
                                                      
   
static ObjectAddress
ATExecSetStorage(Relation rel, const char *colName, Node *newValue, LOCKMODE lockmode)
{
  char *storagemode;
  char newstorage;
  Relation attrelation;
  HeapTuple tuple;
  Form_pg_attribute attrtuple;
  AttrNumber attnum;
  ObjectAddress address;
  ListCell *lc;

  Assert(IsA(newValue, String));
  storagemode = strVal(newValue);

  if (pg_strcasecmp(storagemode, "plain") == 0)
  {
    newstorage = 'p';
  }
  else if (pg_strcasecmp(storagemode, "external") == 0)
  {
    newstorage = 'e';
  }
  else if (pg_strcasecmp(storagemode, "extended") == 0)
  {
    newstorage = 'x';
  }
  else if (pg_strcasecmp(storagemode, "main") == 0)
  {
    newstorage = 'm';
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid storage type \"%s\"", storagemode)));
    newstorage = 0;                          
  }

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);

  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }
  attrtuple = (Form_pg_attribute)GETSTRUCT(tuple);

  attnum = attrtuple->attnum;
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

     
                                                                             
                     
     
  if (newstorage == 'p' || TypeIsToastable(attrtuple->atttypid))
  {
    attrtuple->attstorage = newstorage;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("column data type %s can only have storage PLAIN", format_type_be(attrtuple->atttypid))));
  }

  CatalogTupleUpdate(attrelation, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attrtuple->attnum);

  heap_freetuple(tuple);

     
                                                                         
                                                               
     
  foreach (lc, RelationGetIndexList(rel))
  {
    Oid indexoid = lfirst_oid(lc);
    Relation indrel;
    AttrNumber indattnum = 0;

    indrel = index_open(indexoid, lockmode);

    for (int i = 0; i < indrel->rd_index->indnatts; i++)
    {
      if (indrel->rd_index->indkey.values[i] == attnum)
      {
        indattnum = i + 1;
        break;
      }
    }

    if (indattnum == 0)
    {
      index_close(indrel, lockmode);
      continue;
    }

    tuple = SearchSysCacheCopyAttNum(RelationGetRelid(indrel), indattnum);

    if (HeapTupleIsValid(tuple))
    {
      attrtuple = (Form_pg_attribute)GETSTRUCT(tuple);
      attrtuple->attstorage = newstorage;

      CatalogTupleUpdate(attrelation, &tuple->t_self, tuple);

      InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attrtuple->attnum);

      heap_freetuple(tuple);
    }

    index_close(indrel, lockmode);
  }

  table_close(attrelation, RowExclusiveLock);

  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);
  return address;
}

   
                           
   
                                                                      
                                                                            
                                                                          
                                                                           
               
   
static void
ATPrepDropColumn(List **wqueue, Relation rel, bool recurse, bool recursing, AlterTableCmd *cmd, LOCKMODE lockmode)
{
  if (rel->rd_rel->reloftype && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot drop column from typed table")));
  }

  if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
  {
    ATTypedTableRecursion(wqueue, rel, cmd, lockmode);
  }

  if (recurse)
  {
    cmd->subtype = AT_DropColumnRecurse;
  }
}

   
                                                                             
                                                                       
                                                                              
   
                                                                            
                                                                              
                                                                         
                                                                         
                        
   
static ObjectAddress
ATExecDropColumn(List **wqueue, Relation rel, const char *colName, DropBehavior behavior, bool recurse, bool recursing, bool missing_ok, LOCKMODE lockmode, ObjectAddresses *addrs)
{
  HeapTuple tuple;
  Form_pg_attribute targetatt;
  AttrNumber attnum;
  List *children;
  ObjectAddress object;
  bool is_expr;

                                                                        
  if (recursing)
  {
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
  }

                                                
  Assert(!recursing || addrs != NULL);
  if (!recursing)
  {
    addrs = new_object_addresses();
  }

     
                                     
     
  tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
    }
    else
    {
      ereport(NOTICE, (errmsg("column \"%s\" of relation \"%s\" does not exist, skipping", colName, RelationGetRelationName(rel))));
      return InvalidObjectAddress;
    }
  }
  targetatt = (Form_pg_attribute)GETSTRUCT(tuple);

  attnum = targetatt->attnum;

                                     
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot drop system column \"%s\"", colName)));
  }

     
                                                                            
                           
     
  if (targetatt->attinhcount > 0 && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot drop inherited column \"%s\"", colName)));
  }

     
                                                                            
                                                                           
                                                                      
     
  if (has_partition_attrs(rel, bms_make_singleton(attnum - FirstLowInvalidHeapAttributeNumber), &is_expr))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot drop column \"%s\" because it is part of the partition key of relation \"%s\"", colName, RelationGetRelationName(rel))));
  }

  ReleaseSysCache(tuple);

     
                                                                    
                                                                             
                                                   
     
  children = find_inheritance_children(RelationGetRelid(rel), lockmode);

  if (children)
  {
    Relation attr_rel;
    ListCell *child;

       
                                                                           
                           
       
    if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && !recurse)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot drop column from only the partitioned table when partitions exist"), errhint("Do not specify the ONLY keyword.")));
    }

    attr_rel = table_open(AttributeRelationId, RowExclusiveLock);
    foreach (child, children)
    {
      Oid childrelid = lfirst_oid(child);
      Relation childrel;
      Form_pg_attribute childatt;

                                                      
      childrel = table_open(childrelid, NoLock);
      CheckTableNotInUse(childrel, "ALTER TABLE");

      tuple = SearchSysCacheCopyAttName(childrelid, colName);
      if (!HeapTupleIsValid(tuple))                       
      {
        elog(ERROR, "cache lookup failed for attribute \"%s\" of relation %u", colName, childrelid);
      }
      childatt = (Form_pg_attribute)GETSTRUCT(tuple);

      if (childatt->attinhcount <= 0)                       
      {
        elog(ERROR, "relation %u has non-inherited attribute \"%s\"", childrelid, colName);
      }

      if (recurse)
      {
           
                                                                  
                                                                      
               
           
        if (childatt->attinhcount == 1 && !childatt->attislocal)
        {
                                                     
          ATExecDropColumn(wqueue, childrel, colName, behavior, true, true, false, lockmode, addrs);
        }
        else
        {
                                                     
          childatt->attinhcount--;

          CatalogTupleUpdate(attr_rel, &tuple->t_self, tuple);

                                   
          CommandCounterIncrement();
        }
      }
      else
      {
           
                                                                      
                                                                 
                                          
           
        childatt->attinhcount--;
        childatt->attislocal = true;

        CatalogTupleUpdate(attr_rel, &tuple->t_self, tuple);

                                 
        CommandCounterIncrement();
      }

      heap_freetuple(tuple);

      table_close(childrel, NoLock);
    }
    table_close(attr_rel, RowExclusiveLock);
  }

                            
  object.classId = RelationRelationId;
  object.objectId = RelationGetRelid(rel);
  object.objectSubId = attnum;
  add_exact_object_address(&object, addrs);

  if (!recursing)
  {
                                                                 
    performMultipleDeletions(addrs, behavior, 0);
    free_object_addresses(addrs);
  }

  return object;
}

   
                         
   
                                                                         
                                                                               
                                                                           
   
                                                 
   
static ObjectAddress
ATExecAddIndex(AlteredTableInfo *tab, Relation rel, IndexStmt *stmt, bool is_rebuild, LOCKMODE lockmode)
{
  bool check_rights;
  bool skip_build;
  bool quiet;
  ObjectAddress address;

  Assert(IsA(stmt, IndexStmt));
  Assert(!stmt->concurrent);

                                                                 
  Assert(stmt->transformed);

                                                                   
  check_rights = !is_rebuild;
                                                                          
  skip_build = tab->rewrite > 0 || OidIsValid(stmt->oldNode);
                                                       
  quiet = is_rebuild;

  address = DefineIndex(RelationGetRelid(rel), stmt, InvalidOid,                        
      InvalidOid,                                                                     
      InvalidOid,                                                                          
      true,                                                                          
      check_rights, false,                                                                                 
      skip_build, quiet);

     
                                                                             
                                                                             
                                                                           
                                   
     
  if (OidIsValid(stmt->oldNode))
  {
    Relation irel = index_open(address.objectId, NoLock);

    RelationPreserveStorage(irel->rd_node, true);
    index_close(irel, NoLock);
  }

  return address;
}

   
                                          
   
                                              
   
static ObjectAddress
ATExecAddIndexConstraint(AlteredTableInfo *tab, Relation rel, IndexStmt *stmt, LOCKMODE lockmode)
{
  Oid index_oid = stmt->indexOid;
  Relation indexRel;
  char *indexName;
  IndexInfo *indexInfo;
  char *constraintName;
  char constraintType;
  ObjectAddress address;
  bits16 flags;

  Assert(IsA(stmt, IndexStmt));
  Assert(OidIsValid(index_oid));
  Assert(stmt->isconstraint);

     
                                                                            
                            
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ALTER TABLE / ADD CONSTRAINT USING INDEX is not supported on partitioned tables")));
  }

  indexRel = index_open(index_oid, AccessShareLock);

  indexName = pstrdup(RelationGetRelationName(indexRel));

  indexInfo = BuildIndexInfo(indexRel);

                                                   
  if (!indexInfo->ii_Unique)
  {
    elog(ERROR, "index \"%s\" is not unique", indexName);
  }

     
                                                                         
                                                                            
                                                                   
                                                                           
               
     
  constraintName = stmt->idxname;
  if (constraintName == NULL)
  {
    constraintName = indexName;
  }
  else if (strcmp(constraintName, indexName) != 0)
  {
    ereport(NOTICE, (errmsg("ALTER TABLE / ADD CONSTRAINT USING INDEX will rename index \"%s\" to \"%s\"", indexName, constraintName)));
    RenameRelationInternal(index_oid, constraintName, false, true);
  }

                                                 
  if (stmt->primary)
  {
    index_check_primary_key(rel, indexInfo, true, stmt);
  }

                                                                  
  if (stmt->primary)
  {
    constraintType = CONSTRAINT_PRIMARY;
  }
  else
  {
    constraintType = CONSTRAINT_UNIQUE;
  }

                                                     
  flags = INDEX_CONSTR_CREATE_UPDATE_INDEX | INDEX_CONSTR_CREATE_REMOVE_OLD_DEPS | (stmt->initdeferred ? INDEX_CONSTR_CREATE_INIT_DEFERRED : 0) | (stmt->deferrable ? INDEX_CONSTR_CREATE_DEFERRABLE : 0) | (stmt->primary ? INDEX_CONSTR_CREATE_MARK_AS_PRIMARY : 0);

  address = index_constraint_create(rel, index_oid, InvalidOid, indexInfo, constraintName, constraintType, flags, allowSystemTableMods, false);                  

  index_close(indexRel, NoLock);

  return address;
}

   
                              
   
                                                                           
                                            
   
static ObjectAddress
ATExecAddConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel, Constraint *newConstraint, bool recurse, bool is_readd, LOCKMODE lockmode)
{
  ObjectAddress address = InvalidObjectAddress;

  Assert(IsA(newConstraint, Constraint));

     
                                                                            
                                                                           
                                                             
     
  switch (newConstraint->contype)
  {
  case CONSTR_CHECK:
    address = ATAddCheckConstraint(wqueue, tab, rel, newConstraint, recurse, false, is_readd, lockmode);
    break;

  case CONSTR_FOREIGN:

       
                                          
       
    if (newConstraint->conname)
    {
      if (ConstraintNameIsUsed(CONSTRAINT_RELATION, RelationGetRelid(rel), newConstraint->conname))
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("constraint \"%s\" for relation \"%s\" already exists", newConstraint->conname, RelationGetRelationName(rel))));
      }
    }
    else
    {
      newConstraint->conname = ChooseConstraintName(RelationGetRelationName(rel), ChooseForeignKeyConstraintNameAddition(newConstraint->fk_attrs), "fkey", RelationGetNamespace(rel), NIL);
    }

    address = ATAddForeignKeyConstraint(wqueue, tab, rel, newConstraint, InvalidOid, recurse, false, lockmode);
    break;

  default:
    elog(ERROR, "unrecognized constraint type: %d", (int)newConstraint->contype);
  }

  return address;
}

   
                                                                             
                                                                    
                                                                             
                                     
   
                                                                              
                                                           
   
                                                        
                            
   
static char *
ChooseForeignKeyConstraintNameAddition(List *colnames)
{
  char buf[NAMEDATALEN * 2];
  int buflen = 0;
  ListCell *lc;

  buf[0] = '\0';
  foreach (lc, colnames)
  {
    const char *name = strVal(lfirst(lc));

    if (buflen > 0)
    {
      buf[buflen++] = '_';                             
    }

       
                                                                         
                                                               
       
    strlcpy(buf + buflen, name, NAMEDATALEN);
    buflen += strlen(buf + buflen);
    if (buflen >= NAMEDATALEN)
    {
      break;
    }
  }
  return pstrdup(buf);
}

   
                                                                           
                                                                              
                                      
   
                                       
   
                                                                       
                                                                         
                                                                         
                                                                      
                                                                          
                                                                         
                                        
   
static ObjectAddress
ATAddCheckConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel, Constraint *constr, bool recurse, bool recursing, bool is_readd, LOCKMODE lockmode)
{
  List *newcons;
  ListCell *lcon;
  List *children;
  ListCell *child;
  ObjectAddress address = InvalidObjectAddress;

                                                                        
  if (recursing)
  {
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
  }

     
                                                                            
                                                                             
                                           
     
                                                                            
                                                                           
                                                                       
                                                         
     
  newcons = AddRelationNewConstraints(rel, NIL, list_make1(copyObject(constr)), recursing | is_readd,                  
      !recursing,                                                                                                   
      is_readd,                                                                                                        
      NULL);                                                                                                                       
                                                                                                                

                                                     
  Assert(list_length(newcons) <= 1);

                                                              
  foreach (lcon, newcons)
  {
    CookedConstraint *ccon = (CookedConstraint *)lfirst(lcon);

    if (!ccon->skip_validation)
    {
      NewConstraint *newcon;

      newcon = (NewConstraint *)palloc0(sizeof(NewConstraint));
      newcon->name = ccon->name;
      newcon->contype = ccon->contype;
      newcon->qual = ccon->expr;

      tab->constraints = lappend(tab->constraints, newcon);
    }

                                                             
    if (constr->conname == NULL)
    {
      constr->conname = ccon->name;
    }

    ObjectAddressSet(address, ConstraintRelationId, ccon->conoid);
  }

                                                            
  Assert(constr->conname != NULL);

                                                                            
  CommandCounterIncrement();

     
                                                                           
                                                                      
                                                                          
                                      
     
  if (newcons == NIL)
  {
    return address;
  }

     
                                                                      
     
  if (constr->is_no_inherit)
  {
    return address;
  }

     
                                                                    
                                                                             
                                                   
     
  children = find_inheritance_children(RelationGetRelid(rel), lockmode);

     
                                                                     
                                                                             
                
     
  if (!recurse && children != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraint must be added to child tables too")));
  }

  foreach (child, children)
  {
    Oid childrelid = lfirst_oid(child);
    Relation childrel;
    AlteredTableInfo *childtab;

                                                    
    childrel = table_open(childrelid, NoLock);
    CheckTableNotInUse(childrel, "ALTER TABLE");

                                                        
    childtab = ATGetQueueEntry(wqueue, childrel);

                          
    ATAddCheckConstraint(wqueue, childtab, childrel, constr, recurse, true, is_readd, lockmode);

    table_close(childrel, NoLock);
  }

  return address;
}

   
                                                                               
            
   
                                                                    
                                                                      
                                           
   
                                                                        
                                                                              
                                                                               
                                                                         
                                                                     
                     
   
static ObjectAddress
ATAddForeignKeyConstraint(List **wqueue, AlteredTableInfo *tab, Relation rel, Constraint *fkconstraint, Oid parentConstr, bool recurse, bool recursing, LOCKMODE lockmode)
{
  Relation pkrel;
  int16 pkattnum[INDEX_MAX_KEYS];
  int16 fkattnum[INDEX_MAX_KEYS];
  Oid pktypoid[INDEX_MAX_KEYS];
  Oid fktypoid[INDEX_MAX_KEYS];
  Oid opclasses[INDEX_MAX_KEYS];
  Oid pfeqoperators[INDEX_MAX_KEYS];
  Oid ppeqoperators[INDEX_MAX_KEYS];
  Oid ffeqoperators[INDEX_MAX_KEYS];
  int i;
  int numfks, numpks;
  Oid indexOid;
  bool old_check_ok;
  ObjectAddress address;
  ListCell *old_pfeqop_item = list_head(fkconstraint->old_conpfeqop);

     
                                                                         
                                    
     
  if (OidIsValid(fkconstraint->old_pktable_oid))
  {
    pkrel = table_open(fkconstraint->old_pktable_oid, ShareRowExclusiveLock);
  }
  else
  {
    pkrel = table_openrv(fkconstraint->pktable, ShareRowExclusiveLock);
  }

     
                                                                     
              
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    if (!recurse)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot use ONLY for foreign key on partitioned table \"%s\" referencing relation \"%s\"", RelationGetRelationName(rel), RelationGetRelationName(pkrel))));
    }
    if (fkconstraint->skip_validation && !fkconstraint->initially_valid)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot add NOT VALID foreign key on partitioned table \"%s\" referencing relation \"%s\"", RelationGetRelationName(rel), RelationGetRelationName(pkrel)), errdetail("This feature is not yet supported on partitioned tables.")));
    }
  }

  if (pkrel->rd_rel->relkind != RELKIND_RELATION && pkrel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("referenced relation \"%s\" is not a table", RelationGetRelationName(pkrel))));
  }

  if (!allowSystemTableMods && IsSystemRelation(pkrel))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", RelationGetRelationName(pkrel))));
  }

     
                                                                           
                                                                     
                                                                         
                                                                       
                                                                            
                                                                       
     
  switch (rel->rd_rel->relpersistence)
  {
  case RELPERSISTENCE_PERMANENT:
    if (pkrel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraints on permanent tables may reference only permanent tables")));
    }
    break;
  case RELPERSISTENCE_UNLOGGED:
    if (pkrel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT && pkrel->rd_rel->relpersistence != RELPERSISTENCE_UNLOGGED)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraints on unlogged tables may reference only permanent or unlogged tables")));
    }
    break;
  case RELPERSISTENCE_TEMP:
    if (pkrel->rd_rel->relpersistence != RELPERSISTENCE_TEMP)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraints on temporary tables may reference only temporary tables")));
    }
    if (!pkrel->rd_islocaltemp || !rel->rd_islocaltemp)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraints on temporary tables must involve temporary tables of this session")));
    }
    break;
  }

     
                                                                            
                                  
     
  MemSet(pkattnum, 0, sizeof(pkattnum));
  MemSet(fkattnum, 0, sizeof(fkattnum));
  MemSet(pktypoid, 0, sizeof(pktypoid));
  MemSet(fktypoid, 0, sizeof(fktypoid));
  MemSet(opclasses, 0, sizeof(opclasses));
  MemSet(pfeqoperators, 0, sizeof(pfeqoperators));
  MemSet(ppeqoperators, 0, sizeof(ppeqoperators));
  MemSet(ffeqoperators, 0, sizeof(ffeqoperators));

  numfks = transformColumnNameList(RelationGetRelid(rel), fkconstraint->fk_attrs, fkattnum, fktypoid);

     
                                                                            
                                                                        
                                                                          
                                                                       
     
  if (fkconstraint->pk_attrs == NIL)
  {
    numpks = transformFkeyGetPrimaryKey(pkrel, &indexOid, &fkconstraint->pk_attrs, pkattnum, pktypoid, opclasses);
  }
  else
  {
    numpks = transformColumnNameList(RelationGetRelid(pkrel), fkconstraint->pk_attrs, pkattnum, pktypoid);
                                                    
    indexOid = transformFkeyCheckAttrs(pkrel, numpks, pkattnum, opclasses);
  }

     
                                   
     
  checkFkeyPermissions(pkrel, pkattnum, numpks);

     
                                              
     
  for (i = 0; i < numfks; i++)
  {
    char attgenerated = TupleDescAttr(RelationGetDescr(rel), fkattnum[i] - 1)->attgenerated;

    if (attgenerated)
    {
         
                                                                       
         
      if (fkconstraint->fk_upd_action == FKCONSTR_ACTION_SETNULL || fkconstraint->fk_upd_action == FKCONSTR_ACTION_SETDEFAULT || fkconstraint->fk_upd_action == FKCONSTR_ACTION_CASCADE)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid %s action for foreign key constraint containing generated column", "ON UPDATE")));
      }
      if (fkconstraint->fk_del_action == FKCONSTR_ACTION_SETNULL || fkconstraint->fk_del_action == FKCONSTR_ACTION_SETDEFAULT)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid %s action for foreign key constraint containing generated column", "ON DELETE")));
      }
    }
  }

     
                                                              
     
                                                                             
                                                                         
                                                                          
                                  
     
  if (numfks != numpks)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FOREIGN_KEY), errmsg("number of referencing and referenced columns for foreign key disagree")));
  }

     
                                                                       
                                              
     
  old_check_ok = (fkconstraint->old_conpfeqop != NIL);
  Assert(!old_check_ok || numfks == list_length(fkconstraint->old_conpfeqop));

  for (i = 0; i < numpks; i++)
  {
    Oid pktype = pktypoid[i];
    Oid fktype = fktypoid[i];
    Oid fktyped;
    HeapTuple cla_ht;
    Form_pg_opclass cla_tup;
    Oid amid;
    Oid opfamily;
    Oid opcintype;
    Oid pfeqop;
    Oid ppeqop;
    Oid ffeqop;
    int16 eqstrategy;
    Oid pfeqop_right;

                                                            
    cla_ht = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclasses[i]));
    if (!HeapTupleIsValid(cla_ht))
    {
      elog(ERROR, "cache lookup failed for opclass %u", opclasses[i]);
    }
    cla_tup = (Form_pg_opclass)GETSTRUCT(cla_ht);
    amid = cla_tup->opcmethod;
    opfamily = cla_tup->opcfamily;
    opcintype = cla_tup->opcintype;
    ReleaseSysCache(cla_ht);

       
                                                                        
                                                                          
                                                                      
                                                                      
                                                             
       
    if (amid != BTREE_AM_OID)
    {
      elog(ERROR, "only b-tree indexes are supported for foreign keys");
    }
    eqstrategy = BTEqualStrategyNumber;

       
                                                                      
                                             
       
    ppeqop = get_opfamily_member(opfamily, opcintype, opcintype, eqstrategy);

    if (!OidIsValid(ppeqop))
    {
      elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", eqstrategy, opcintype, opcintype, opfamily);
    }

       
                                                                          
                                               
       
    fktyped = getBaseType(fktype);

    pfeqop = get_opfamily_member(opfamily, opcintype, fktyped, eqstrategy);
    if (OidIsValid(pfeqop))
    {
      pfeqop_right = fktyped;
      ffeqop = get_opfamily_member(opfamily, fktyped, fktyped, eqstrategy);
    }
    else
    {
                               
      pfeqop_right = InvalidOid;
      ffeqop = InvalidOid;
    }

    if (!(OidIsValid(pfeqop) && OidIsValid(ffeqop)))
    {
         
                                                                      
                                                                     
                                                                       
                                                                      
                                                                         
                                                                       
                                
         
      Oid input_typeids[2];
      Oid target_typeids[2];

      input_typeids[0] = pktype;
      input_typeids[1] = fktype;
      target_typeids[0] = opcintype;
      target_typeids[1] = opcintype;
      if (can_coerce_type(2, input_typeids, target_typeids, COERCION_IMPLICIT))
      {
        pfeqop = ffeqop = ppeqop;
        pfeqop_right = opcintype;
      }
    }

    if (!(OidIsValid(pfeqop) && OidIsValid(ffeqop)))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("foreign key constraint \"%s\" cannot be implemented", fkconstraint->conname),
                         errdetail("Key columns \"%s\" and \"%s\" "
                                   "are of incompatible types: %s and %s.",
                             strVal(list_nth(fkconstraint->fk_attrs, i)), strVal(list_nth(fkconstraint->pk_attrs, i)), format_type_be(fktype), format_type_be(pktype))));
    }

    if (old_check_ok)
    {
         
                                                                     
                                                                        
                                                                   
                                                                         
         
      old_check_ok = (pfeqop == lfirst_oid(old_pfeqop_item));
      old_pfeqop_item = lnext(old_pfeqop_item);
    }
    if (old_check_ok)
    {
      Oid old_fktype;
      Oid new_fktype;
      CoercionPathType old_pathtype;
      CoercionPathType new_pathtype;
      Oid old_castfunc;
      Oid new_castfunc;
      Form_pg_attribute attr = TupleDescAttr(tab->oldDesc, fkattnum[i] - 1);

         
                                                                         
                                                                         
                                                                  
         
      old_fktype = attr->atttypid;
      new_fktype = fktype;
      old_pathtype = findFkeyCast(pfeqop_right, old_fktype, &old_castfunc);
      new_pathtype = findFkeyCast(pfeqop_right, new_fktype, &new_castfunc);

         
                                                                    
                                                                     
                                                                      
                                                                    
                                                                   
                                             
         
                                                                      
                                                                       
                                                                 
                                                                       
                                                                         
                                             
         
                                                                         
                                                                     
                                                                        
                                                                         
                                                                       
                                                                      
                                 
         
                                                                       
                                                                    
                                                     
         
                                                                      
                                                                       
                                                                     
                                                                      
         
      old_check_ok = (new_pathtype == old_pathtype && new_castfunc == old_castfunc && (!IsPolymorphicType(pfeqop_right) || new_fktype == old_fktype));
    }

    pfeqoperators[i] = pfeqop;
    ppeqoperators[i] = ppeqop;
    ffeqoperators[i] = ffeqop;
  }

     
                                                                            
                                                      
     
  address = addFkRecurseReferenced(wqueue, fkconstraint, rel, pkrel, indexOid, InvalidOid,                           
      numfks, pkattnum, fkattnum, pfeqoperators, ppeqoperators, ffeqoperators, old_check_ok);

                                        
  addFkRecurseReferencing(wqueue, fkconstraint, rel, pkrel, indexOid, address.objectId, numfks, pkattnum, fkattnum, pfeqoperators, ppeqoperators, ffeqoperators, old_check_ok, lockmode);

     
                                                                 
     
  table_close(pkrel, NoLock);

  return address;
}

   
                          
                                                                         
                           
   
                                                                        
                                                                               
                                                                            
              
   
                                                                              
                               
                                               
                                         
                                                                         
                                                                             
                                                                           
                         
                                                      
                                                          
                                                           
                                                                   
                                                                           
                                                                  
   
static ObjectAddress
addFkRecurseReferenced(List **wqueue, Constraint *fkconstraint, Relation rel, Relation pkrel, Oid indexOid, Oid parentConstr, int numfks, int16 *pkattnum, int16 *fkattnum, Oid *pfeqoperators, Oid *ppeqoperators, Oid *ffeqoperators, bool old_check_ok)
{
  ObjectAddress address;
  Oid constrOid;
  char *conname;
  bool conislocal;
  int coninhcount;
  bool connoinherit;

     
                                                                           
                                                                        
     
  if (pkrel->rd_rel->relkind != RELKIND_RELATION && pkrel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("referenced relation \"%s\" is not a table", RelationGetRelationName(pkrel))));
  }

     
                                                                           
                                                                   
     
  if (ConstraintNameIsUsed(CONSTRAINT_RELATION, RelationGetRelid(rel), fkconstraint->conname))
  {
    conname = ChooseConstraintName(RelationGetRelationName(rel), ChooseForeignKeyConstraintNameAddition(fkconstraint->fk_attrs), "fkey", RelationGetNamespace(rel), NIL);
  }
  else
  {
    conname = fkconstraint->conname;
  }

  if (OidIsValid(parentConstr))
  {
    conislocal = false;
    coninhcount = 1;
    connoinherit = false;
  }
  else
  {
    conislocal = true;
    coninhcount = 0;

       
                                                                           
       
    connoinherit = rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE;
  }

     
                                                
     
  constrOid = CreateConstraintEntry(conname, RelationGetNamespace(rel), CONSTRAINT_FOREIGN, fkconstraint->deferrable, fkconstraint->initdeferred, fkconstraint->initially_valid, parentConstr, RelationGetRelid(rel), fkattnum, numfks, numfks, InvalidOid,                              
      indexOid, RelationGetRelid(pkrel), pkattnum, pfeqoperators, ppeqoperators, ffeqoperators, numfks, fkconstraint->fk_upd_action, fkconstraint->fk_del_action, fkconstraint->fk_matchtype, NULL,                                                                                      
      NULL,                                                                                                                                                                                                                                                                          
      NULL, conislocal,                                                                                                                                                                                                                                                  
      coninhcount,                                                                                                                                                                                                                                                        
      connoinherit,                                                                                                                                                                                                                                                           
      false);                                                                                                                                                                                                                                                                

  ObjectAddressSet(address, ConstraintRelationId, constrOid);

     
                                                                             
                                                                            
                                                                        
                                     
     
  if (OidIsValid(parentConstr))
  {
    ObjectAddress referenced;

    ObjectAddressSet(referenced, ConstraintRelationId, parentConstr);
    recordDependencyOn(&address, &referenced, DEPENDENCY_INTERNAL);
  }

                                                        
  CommandCounterIncrement();

     
                                                                             
                                  
     
  if (pkrel->rd_rel->relkind == RELKIND_RELATION)
  {
    createForeignKeyActionTriggers(rel, RelationGetRelid(pkrel), fkconstraint, constrOid, indexOid);
  }

     
                                                                            
                                                                     
                                                                          
     
  if (pkrel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    PartitionDesc pd = RelationGetPartitionDesc(pkrel);

    for (int i = 0; i < pd->nparts; i++)
    {
      Relation partRel;
      AttrNumber *map;
      AttrNumber *mapped_pkattnum;
      Oid partIndexId;

      partRel = table_open(pd->oids[i], ShareRowExclusiveLock);

         
                                                                    
                                                            
         
      map = convert_tuples_by_name_map_if_req(RelationGetDescr(partRel), RelationGetDescr(pkrel), gettext_noop("could not convert row type"));
      if (map)
      {
        mapped_pkattnum = palloc(sizeof(AttrNumber) * numfks);
        for (int j = 0; j < numfks; j++)
        {
          mapped_pkattnum[j] = map[pkattnum[j] - 1];
        }
      }
      else
      {
        mapped_pkattnum = pkattnum;
      }

                       
      partIndexId = index_get_partition(partRel, indexOid);
      if (!OidIsValid(partIndexId))
      {
        elog(ERROR, "index for %u not found in partition %s", indexOid, RelationGetRelationName(partRel));
      }
      addFkRecurseReferenced(wqueue, fkconstraint, rel, partRel, partIndexId, constrOid, numfks, mapped_pkattnum, fkattnum, pfeqoperators, ppeqoperators, ffeqoperators, old_check_ok);

                                                
      table_close(partRel, NoLock);
      if (map)
      {
        pfree(mapped_pkattnum);
        pfree(map);
      }
    }
  }

  return address;
}

   
                           
                                                                    
   
                                                                               
                                                                             
                                                                           
                                                                             
              
   
                                                                       
                                                                          
           
   
                                                                              
                               
                                               
                                                                        
                                          
                                                                             
                                                                           
                                                      
                                                          
                                                           
                                                                   
                                                                           
                                                                   
                                                                     
   
static void
addFkRecurseReferencing(List **wqueue, Constraint *fkconstraint, Relation rel, Relation pkrel, Oid indexOid, Oid parentConstr, int numfks, int16 *pkattnum, int16 *fkattnum, Oid *pfeqoperators, Oid *ppeqoperators, Oid *ffeqoperators, bool old_check_ok, LOCKMODE lockmode)
{
  AssertArg(OidIsValid(parentConstr));

  if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("foreign key constraints are not supported on foreign tables")));
  }

     
                                                                             
                                                                 
     
                                                                            
     
  if (rel->rd_rel->relkind == RELKIND_RELATION)
  {
    createForeignKeyCheckTriggers(RelationGetRelid(rel), RelationGetRelid(pkrel), fkconstraint, parentConstr, indexOid);

       
                                                                          
                                                                    
                                                                         
                                                                        
                                                   
       
    if (wqueue && !old_check_ok && !fkconstraint->skip_validation)
    {
      NewConstraint *newcon;
      AlteredTableInfo *tab;

      tab = ATGetQueueEntry(wqueue, rel);

      newcon = (NewConstraint *)palloc0(sizeof(NewConstraint));
      newcon->name = get_constraint_name(parentConstr);
      newcon->contype = CONSTR_FOREIGN;
      newcon->refrelid = RelationGetRelid(pkrel);
      newcon->refindid = indexOid;
      newcon->conid = parentConstr;
      newcon->qual = (Node *)fkconstraint;

      tab->constraints = lappend(tab->constraints, newcon);
    }
  }
  else if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    PartitionDesc pd = RelationGetPartitionDesc(rel);

       
                                                                       
                                                                           
            
       
    for (int i = 0; i < pd->nparts; i++)
    {
      Oid partitionId = pd->oids[i];
      Relation partition = table_open(partitionId, lockmode);
      List *partFKs;
      AttrNumber *attmap;
      AttrNumber mapped_fkattnum[INDEX_MAX_KEYS];
      bool attached;
      char *conname;
      Oid constrOid;
      ObjectAddress address, referenced;
      ListCell *cell;

      CheckTableNotInUse(partition, "ALTER TABLE");

      attmap = convert_tuples_by_name_map(RelationGetDescr(partition), RelationGetDescr(rel), gettext_noop("could not convert row type"));
      for (int j = 0; j < numfks; j++)
      {
        mapped_fkattnum[j] = attmap[fkattnum[j] - 1];
      }

                                                                  
      partFKs = copyObject(RelationGetFKeyList(partition));
      attached = false;
      foreach (cell, partFKs)
      {
        ForeignKeyCacheInfo *fk;

        fk = lfirst_node(ForeignKeyCacheInfo, cell);
        if (tryAttachPartitionForeignKey(fk, partitionId, parentConstr, numfks, mapped_fkattnum, pkattnum, pfeqoperators))
        {
          attached = true;
          break;
        }
      }
      if (attached)
      {
        table_close(partition, NoLock);
        continue;
      }

         
                                                                     
         
      if (ConstraintNameIsUsed(CONSTRAINT_RELATION, RelationGetRelid(partition), fkconstraint->conname))
      {
        conname = ChooseConstraintName(RelationGetRelationName(partition), ChooseForeignKeyConstraintNameAddition(fkconstraint->fk_attrs), "fkey", RelationGetNamespace(partition), NIL);
      }
      else
      {
        conname = fkconstraint->conname;
      }
      constrOid = CreateConstraintEntry(conname, RelationGetNamespace(partition), CONSTRAINT_FOREIGN, fkconstraint->deferrable, fkconstraint->initdeferred, fkconstraint->initially_valid, parentConstr, partitionId, mapped_fkattnum, numfks, numfks, InvalidOid, indexOid, RelationGetRelid(pkrel), pkattnum, pfeqoperators, ppeqoperators, ffeqoperators, numfks, fkconstraint->fk_upd_action, fkconstraint->fk_del_action, fkconstraint->fk_matchtype, NULL, NULL, NULL, false, 1, false, false);

         
                                                                        
                                          
         
      ObjectAddressSet(address, ConstraintRelationId, constrOid);
      ObjectAddressSet(referenced, ConstraintRelationId, parentConstr);
      recordDependencyOn(&address, &referenced, DEPENDENCY_PARTITION_PRI);
      ObjectAddressSet(referenced, RelationRelationId, partitionId);
      recordDependencyOn(&address, &referenced, DEPENDENCY_PARTITION_SEC);

                                                  
      CommandCounterIncrement();

                                                                  
      addFkRecurseReferencing(wqueue, fkconstraint, partition, pkrel, indexOid, constrOid, numfks, pkattnum, mapped_fkattnum, pfeqoperators, ppeqoperators, ffeqoperators, old_check_ok, lockmode);

      table_close(partition, NoLock);
    }
  }
}

   
                              
                                                                    
               
   
                                                                              
                                                                              
                  
   
                                                                           
                                                                              
                  
   
static void
CloneForeignKeyConstraints(List **wqueue, Relation parentRel, Relation partitionRel)
{
                                                    
  Assert(parentRel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);

     
                                                                       
     
  CloneFkReferenced(parentRel, partitionRel);

     
                                                                        
     
  CloneFkReferencing(wqueue, parentRel, partitionRel);
}

   
                     
                                              
   
                                                                          
                                                                         
                                                    
   
                                                                               
   
                                                                               
                                                        
   
static void
CloneFkReferenced(Relation parentRel, Relation partitionRel)
{
  Relation pg_constraint;
  AttrNumber *attmap;
  ListCell *cell;
  SysScanDesc scan;
  ScanKeyData key[2];
  HeapTuple tuple;
  List *clone = NIL;

     
                                                                        
                                                                       
                                                                            
                                                                           
                                                                             
                                                                         
                                        
     
  pg_constraint = table_open(ConstraintRelationId, RowShareLock);
  ScanKeyInit(&key[0], Anum_pg_constraint_confrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(parentRel)));
  ScanKeyInit(&key[1], Anum_pg_constraint_contype, BTEqualStrategyNumber, F_CHAREQ, CharGetDatum(CONSTRAINT_FOREIGN));
                                                              
  scan = systable_beginscan(pg_constraint, InvalidOid, true, NULL, 2, key);
  while ((tuple = systable_getnext(scan)) != NULL)
  {
    Form_pg_constraint constrForm = (Form_pg_constraint)GETSTRUCT(tuple);

    clone = lappend_oid(clone, constrForm->oid);
  }
  systable_endscan(scan);
  table_close(pg_constraint, RowShareLock);

  attmap = convert_tuples_by_name_map(RelationGetDescr(partitionRel), RelationGetDescr(parentRel), gettext_noop("could not convert row type"));
  foreach (cell, clone)
  {
    Oid constrOid = lfirst_oid(cell);
    Form_pg_constraint constrForm;
    Relation fkRel;
    Oid indexOid;
    Oid partIndexId;
    int numfks;
    AttrNumber conkey[INDEX_MAX_KEYS];
    AttrNumber mapped_confkey[INDEX_MAX_KEYS];
    AttrNumber confkey[INDEX_MAX_KEYS];
    Oid conpfeqop[INDEX_MAX_KEYS];
    Oid conppeqop[INDEX_MAX_KEYS];
    Oid conffeqop[INDEX_MAX_KEYS];
    Constraint *fkconstraint;

    tuple = SearchSysCache1(CONSTROID, constrOid);
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for constraint %u", constrOid);
    }
    constrForm = (Form_pg_constraint)GETSTRUCT(tuple);

       
                                                                           
                                  
       
    if (list_member_oid(clone, constrForm->conparentid))
    {
      ReleaseSysCache(tuple);
      continue;
    }

       
                                                                      
                                                    
       
    if (constrForm->conrelid == RelationGetRelid(parentRel) || constrForm->conrelid == RelationGetRelid(partitionRel))
    {
      ReleaseSysCache(tuple);
      continue;
    }

       
                                                                          
                                                                           
                                                                      
                      
       
    fkRel = table_open(constrForm->conrelid, AccessShareLock);

    indexOid = constrForm->conindid;
    DeconstructFkConstraintRow(tuple, &numfks, conkey, confkey, conpfeqop, conppeqop, conffeqop);
    for (int i = 0; i < numfks; i++)
    {
      mapped_confkey[i] = attmap[confkey[i] - 1];
    }

    fkconstraint = makeNode(Constraint);
    fkconstraint->contype = CONSTRAINT_FOREIGN;
    fkconstraint->conname = NameStr(constrForm->conname);
    fkconstraint->deferrable = constrForm->condeferrable;
    fkconstraint->initdeferred = constrForm->condeferred;
    fkconstraint->location = -1;
    fkconstraint->pktable = NULL;
                                     
    fkconstraint->pk_attrs = NIL;
    fkconstraint->fk_matchtype = constrForm->confmatchtype;
    fkconstraint->fk_upd_action = constrForm->confupdtype;
    fkconstraint->fk_del_action = constrForm->confdeltype;
    fkconstraint->old_conpfeqop = NIL;
    fkconstraint->old_pktable_oid = InvalidOid;
    fkconstraint->skip_validation = false;
    fkconstraint->initially_valid = true;

                                                                       
    for (int i = 0; i < numfks; i++)
    {
      Form_pg_attribute att;

      att = TupleDescAttr(RelationGetDescr(fkRel), conkey[i] - 1);
      fkconstraint->fk_attrs = lappend(fkconstraint->fk_attrs, makeString(NameStr(att->attname)));
    }

       
                                                                         
                                                                        
                                                              
       
    partIndexId = index_get_partition(partitionRel, indexOid);
    if (!OidIsValid(partIndexId))
    {
      elog(ERROR, "index for %u not found in partition %s", indexOid, RelationGetRelationName(partitionRel));
    }
    addFkRecurseReferenced(NULL, fkconstraint, fkRel, partitionRel, partIndexId, constrOid, numfks, mapped_confkey, conkey, conpfeqop, conppeqop, conffeqop, true);

    table_close(fkRel, NoLock);
    ReleaseSysCache(tuple);
  }
}

   
                      
                                              
   
                                                                            
                                                                           
                                                                           
          
   
                                                                          
                                                                          
                                                          
   
static void
CloneFkReferencing(List **wqueue, Relation parentRel, Relation partRel)
{
  AttrNumber *attmap;
  List *partFKs;
  List *clone = NIL;
  ListCell *cell;

                                                          
  foreach (cell, RelationGetFKeyList(parentRel))
  {
    ForeignKeyCacheInfo *fk = lfirst(cell);

    clone = lappend_oid(clone, fk->conoid);
  }

     
                                                                        
                                                          
     
  if (clone == NIL)
  {
    return;
  }

  if (partRel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("foreign key constraints are not supported on foreign tables")));
  }

     
                                                                        
                                                   
     
  attmap = convert_tuples_by_name_map(RelationGetDescr(partRel), RelationGetDescr(parentRel), gettext_noop("could not convert row type"));

  partFKs = copyObject(RelationGetFKeyList(partRel));

  foreach (cell, clone)
  {
    Oid parentConstrOid = lfirst_oid(cell);
    Form_pg_constraint constrForm;
    Relation pkrel;
    HeapTuple tuple;
    int numfks;
    AttrNumber conkey[INDEX_MAX_KEYS];
    AttrNumber mapped_conkey[INDEX_MAX_KEYS];
    AttrNumber confkey[INDEX_MAX_KEYS];
    Oid conpfeqop[INDEX_MAX_KEYS];
    Oid conppeqop[INDEX_MAX_KEYS];
    Oid conffeqop[INDEX_MAX_KEYS];
    Constraint *fkconstraint;
    bool attached;
    Oid indexOid;
    Oid constrOid;
    ObjectAddress address, referenced;
    ListCell *cell;

    tuple = SearchSysCache1(CONSTROID, parentConstrOid);
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for constraint %u", parentConstrOid);
    }
    constrForm = (Form_pg_constraint)GETSTRUCT(tuple);

                                                                
    if (list_member_oid(clone, constrForm->conparentid))
    {
      ReleaseSysCache(tuple);
      continue;
    }

       
                                                                        
                                                    
       
    pkrel = table_open(constrForm->confrelid, ShareRowExclusiveLock);
    if (pkrel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      (void)find_all_inheritors(RelationGetRelid(pkrel), ShareRowExclusiveLock, NULL);
    }

    DeconstructFkConstraintRow(tuple, &numfks, conkey, confkey, conpfeqop, conppeqop, conffeqop);
    for (int i = 0; i < numfks; i++)
    {
      mapped_conkey[i] = attmap[conkey[i] - 1];
    }

       
                                                                          
                                                                        
                                                                      
                                                                      
                                                                
       
    attached = false;
    foreach (cell, partFKs)
    {
      ForeignKeyCacheInfo *fk = lfirst_node(ForeignKeyCacheInfo, cell);

      if (tryAttachPartitionForeignKey(fk, RelationGetRelid(partRel), parentConstrOid, numfks, mapped_conkey, confkey, conpfeqop))
      {
        attached = true;
        table_close(pkrel, NoLock);
        break;
      }
    }
    if (attached)
    {
      ReleaseSysCache(tuple);
      continue;
    }

                                                       
    fkconstraint = makeNode(Constraint);
    fkconstraint->contype = CONSTRAINT_FOREIGN;
                                    
    fkconstraint->deferrable = constrForm->condeferrable;
    fkconstraint->initdeferred = constrForm->condeferred;
    fkconstraint->location = -1;
    fkconstraint->pktable = NULL;
                                     
    fkconstraint->pk_attrs = NIL;
    fkconstraint->fk_matchtype = constrForm->confmatchtype;
    fkconstraint->fk_upd_action = constrForm->confupdtype;
    fkconstraint->fk_del_action = constrForm->confdeltype;
    fkconstraint->old_conpfeqop = NIL;
    fkconstraint->old_pktable_oid = InvalidOid;
    fkconstraint->skip_validation = false;
    fkconstraint->initially_valid = true;
    for (int i = 0; i < numfks; i++)
    {
      Form_pg_attribute att;

      att = TupleDescAttr(RelationGetDescr(partRel), mapped_conkey[i] - 1);
      fkconstraint->fk_attrs = lappend(fkconstraint->fk_attrs, makeString(NameStr(att->attname)));
    }
    if (ConstraintNameIsUsed(CONSTRAINT_RELATION, RelationGetRelid(partRel), NameStr(constrForm->conname)))
    {
      fkconstraint->conname = ChooseConstraintName(RelationGetRelationName(partRel), ChooseForeignKeyConstraintNameAddition(fkconstraint->fk_attrs), "fkey", RelationGetNamespace(partRel), NIL);
    }
    else
    {
      fkconstraint->conname = pstrdup(NameStr(constrForm->conname));
    }

    indexOid = constrForm->conindid;
    constrOid = CreateConstraintEntry(fkconstraint->conname, constrForm->connamespace, CONSTRAINT_FOREIGN, fkconstraint->deferrable, fkconstraint->initdeferred, constrForm->convalidated, parentConstrOid, RelationGetRelid(partRel), mapped_conkey, numfks, numfks, InvalidOid,                              
        indexOid, constrForm->confrelid,                                                                                                                                                                                                                                                                
        confkey, conpfeqop, conppeqop, conffeqop, numfks, fkconstraint->fk_upd_action, fkconstraint->fk_del_action, fkconstraint->fk_matchtype, NULL, NULL, NULL, false,                                                                                                                       
        1,                                                                                                                                                                                                                                                                                      
        false,                                                                                                                                                                                                                                                                                      
        true);

                                                              
    ObjectAddressSet(address, ConstraintRelationId, constrOid);
    ObjectAddressSet(referenced, ConstraintRelationId, parentConstrOid);
    recordDependencyOn(&address, &referenced, DEPENDENCY_PARTITION_PRI);
    ObjectAddressSet(referenced, RelationRelationId, RelationGetRelid(partRel));
    recordDependencyOn(&address, &referenced, DEPENDENCY_PARTITION_SEC);

                                                 
    ReleaseSysCache(tuple);

                                                
    CommandCounterIncrement();

    addFkRecurseReferencing(wqueue, fkconstraint, partRel, pkrel, indexOid, constrOid, numfks, confkey, mapped_conkey, conpfeqop, conppeqop, conffeqop, false,                          
        AccessExclusiveLock);
    table_close(pkrel, NoLock);
  }
}

   
                                                                               
                                                                           
                                                                        
                                                                               
                                                                             
                                        
   
                                                                         
                 
   
static bool
tryAttachPartitionForeignKey(ForeignKeyCacheInfo *fk, Oid partRelid, Oid parentConstrOid, int numfks, AttrNumber *mapped_conkey, AttrNumber *confkey, Oid *conpfeqop)
{
  HeapTuple parentConstrTup;
  Form_pg_constraint parentConstr;
  HeapTuple partcontup;
  Form_pg_constraint partConstr;
  Relation trigrel;
  ScanKeyData key;
  SysScanDesc scan;
  HeapTuple trigtup;

  parentConstrTup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(parentConstrOid));
  if (!HeapTupleIsValid(parentConstrTup))
  {
    elog(ERROR, "cache lookup failed for constraint %u", parentConstrOid);
  }
  parentConstr = (Form_pg_constraint)GETSTRUCT(parentConstrTup);

     
                                                                           
                          
     
  if (fk->confrelid != parentConstr->confrelid || fk->nkeys != numfks)
  {
    ReleaseSysCache(parentConstrTup);
    return false;
  }
  for (int i = 0; i < numfks; i++)
  {
    if (fk->conkey[i] != mapped_conkey[i] || fk->confkey[i] != confkey[i] || fk->conpfeqop[i] != conpfeqop[i])
    {
      ReleaseSysCache(parentConstrTup);
      return false;
    }
  }

     
                                                                             
                                                                           
                                         
     
  partcontup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(fk->conoid));
  if (!HeapTupleIsValid(partcontup))
  {
    elog(ERROR, "cache lookup failed for constraint %u", fk->conoid);
  }
  partConstr = (Form_pg_constraint)GETSTRUCT(partcontup);
  if (OidIsValid(partConstr->conparentid) || !partConstr->convalidated || partConstr->condeferrable != parentConstr->condeferrable || partConstr->condeferred != parentConstr->condeferred || partConstr->confupdtype != parentConstr->confupdtype || partConstr->confdeltype != parentConstr->confdeltype || partConstr->confmatchtype != parentConstr->confmatchtype)
  {
    ReleaseSysCache(parentConstrTup);
    ReleaseSysCache(partcontup);
    return false;
  }

  ReleaseSysCache(partcontup);
  ReleaseSysCache(parentConstrTup);

     
                                                                          
                                                                           
                                                                           
                                                                          
                                                  
     
  trigrel = table_open(TriggerRelationId, RowExclusiveLock);
  ScanKeyInit(&key, Anum_pg_trigger_tgconstraint, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(fk->conoid));

  scan = systable_beginscan(trigrel, TriggerConstraintIndexId, true, NULL, 1, &key);
  while ((trigtup = systable_getnext(scan)) != NULL)
  {
    Form_pg_trigger trgform = (Form_pg_trigger)GETSTRUCT(trigtup);
    ObjectAddress trigger;

    if (trgform->tgconstrrelid != fk->conrelid)
    {
      continue;
    }
    if (trgform->tgrelid != fk->confrelid)
    {
      continue;
    }

       
                                                                         
                                                                        
                                                                          
                                                                         
                                      
       
    deleteDependencyRecordsFor(TriggerRelationId, trgform->oid, false);
                                                             
    CommandCounterIncrement();
    ObjectAddressSet(trigger, TriggerRelationId, trgform->oid);
    performDeletion(&trigger, DROP_RESTRICT, 0);
                                                              
    CommandCounterIncrement();
  }

  systable_endscan(scan);
  table_close(trigrel, RowExclusiveLock);

  ConstraintSetParentConstraint(fk->conoid, parentConstrOid, partRelid);
  CommandCounterIncrement();
  return true;
}

   
                                
   
                                          
   
                                                     
   
                                                                         
                         
   
static ObjectAddress
ATExecAlterConstraint(Relation rel, AlterTableCmd *cmd, bool recurse, bool recursing, LOCKMODE lockmode)
{
  Constraint *cmdcon;
  Relation conrel;
  Relation tgrel;
  SysScanDesc scan;
  ScanKeyData skey[3];
  HeapTuple contuple;
  Form_pg_constraint currcon;
  ObjectAddress address;
  List *otherrelids = NIL;
  ListCell *lc;

  cmdcon = castNode(Constraint, cmd->def);

  conrel = table_open(ConstraintRelationId, RowExclusiveLock);
  tgrel = table_open(TriggerRelationId, RowExclusiveLock);

     
                                          
     
  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(cmdcon->conname));
  scan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (!HeapTupleIsValid(contuple = systable_getnext(scan)))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" of relation \"%s\" does not exist", cmdcon->conname, RelationGetRelationName(rel))));
  }

  currcon = (Form_pg_constraint)GETSTRUCT(contuple);
  if (currcon->contype != CONSTRAINT_FOREIGN)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("constraint \"%s\" of relation \"%s\" is not a foreign key constraint", cmdcon->conname, RelationGetRelationName(rel))));
  }

     
                                                         
     
                                                                             
                                                                             
                                                                            
                                                                            
                                                                            
     
  if (OidIsValid(currcon->conparentid))
  {
    HeapTuple tp;
    Oid parent = currcon->conparentid;
    char *ancestorname = NULL;
    char *ancestortable = NULL;

                                             
    while (HeapTupleIsValid(tp = SearchSysCache1(CONSTROID, ObjectIdGetDatum(parent))))
    {
      Form_pg_constraint contup = (Form_pg_constraint)GETSTRUCT(tp);

                                                        
      if (!OidIsValid(contup->conparentid))
      {
        ancestorname = pstrdup(NameStr(contup->conname));
        ancestortable = get_rel_name(contup->conrelid);
        ReleaseSysCache(tp);
        break;
      }

      parent = contup->conparentid;
      ReleaseSysCache(tp);
    }

    ereport(ERROR, (errmsg("cannot alter constraint \"%s\" on relation \"%s\"", cmdcon->conname, RelationGetRelationName(rel)), ancestorname && ancestortable ? errdetail("Constraint \"%s\" is derived from constraint \"%s\" of relation \"%s\".", cmdcon->conname, ancestorname, ancestortable) : 0, errhint("You may alter the constraint it derives from, instead.")));
  }

     
                                                                         
                                                                          
                                                                            
     
  address = InvalidObjectAddress;
  if (currcon->condeferrable != cmdcon->deferrable || currcon->condeferred != cmdcon->initdeferred || rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    if (ATExecAlterConstrRecurse(cmdcon, conrel, tgrel, rel, contuple, &otherrelids, lockmode))
    {
      ObjectAddressSet(address, ConstraintRelationId, currcon->oid);
    }
  }

     
                                                                        
                                                                         
                                                             
     
  foreach (lc, otherrelids)
  {
    CacheInvalidateRelcacheByRelid(lfirst_oid(lc));
  }

  systable_endscan(scan);

  table_close(tgrel, RowExclusiveLock);
  table_close(conrel, RowExclusiveLock);

  return address;
}

   
                                                                       
                          
   
                                                                            
   
                                                                       
                                                                    
                                                                           
                                         
   
static bool
ATExecAlterConstrRecurse(Constraint *cmdcon, Relation conrel, Relation tgrel, Relation rel, HeapTuple contuple, List **otherrelids, LOCKMODE lockmode)
{
  Form_pg_constraint currcon;
  Oid conoid;
  Oid refrelid;
  bool changed = false;

  currcon = (Form_pg_constraint)GETSTRUCT(contuple);
  conoid = currcon->oid;
  refrelid = currcon->confrelid;

     
                                                      
     
                                                                           
                          
     
  if (currcon->condeferrable != cmdcon->deferrable || currcon->condeferred != cmdcon->initdeferred)
  {
    HeapTuple copyTuple;
    Form_pg_constraint copy_con;
    HeapTuple tgtuple;
    ScanKeyData tgkey;
    SysScanDesc tgscan;

    copyTuple = heap_copytuple(contuple);
    copy_con = (Form_pg_constraint)GETSTRUCT(copyTuple);
    copy_con->condeferrable = cmdcon->deferrable;
    copy_con->condeferred = cmdcon->initdeferred;
    CatalogTupleUpdate(conrel, &copyTuple->t_self, copyTuple);

    InvokeObjectPostAlterHook(ConstraintRelationId, conoid, 0);

    heap_freetuple(copyTuple);
    changed = true;

                                                     
    CacheInvalidateRelcache(rel);

       
                                                                     
                                 
       
    ScanKeyInit(&tgkey, Anum_pg_trigger_tgconstraint, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(conoid));
    tgscan = systable_beginscan(tgrel, TriggerConstraintIndexId, true, NULL, 1, &tgkey);
    while (HeapTupleIsValid(tgtuple = systable_getnext(tgscan)))
    {
      Form_pg_trigger tgform = (Form_pg_trigger)GETSTRUCT(tgtuple);
      Form_pg_trigger copy_tg;
      HeapTuple copyTuple;

         
                                                                       
                                                                        
                                                                   
                                             
         
      if (tgform->tgrelid != RelationGetRelid(rel))
      {
        *otherrelids = list_append_unique_oid(*otherrelids, tgform->tgrelid);
      }

         
                                                       
                                                                       
                                                                      
                                   
         
      if (tgform->tgfoid != F_RI_FKEY_NOACTION_DEL && tgform->tgfoid != F_RI_FKEY_NOACTION_UPD && tgform->tgfoid != F_RI_FKEY_CHECK_INS && tgform->tgfoid != F_RI_FKEY_CHECK_UPD)
      {
        continue;
      }

      copyTuple = heap_copytuple(tgtuple);
      copy_tg = (Form_pg_trigger)GETSTRUCT(copyTuple);

      copy_tg->tgdeferrable = cmdcon->deferrable;
      copy_tg->tginitdeferred = cmdcon->initdeferred;
      CatalogTupleUpdate(tgrel, &copyTuple->t_self, copyTuple);

      InvokeObjectPostAlterHook(TriggerRelationId, tgform->oid, 0);

      heap_freetuple(copyTuple);
    }

    systable_endscan(tgscan);
  }

     
                                                                             
                                                                      
     
                                                                         
                                                                           
                 
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE || get_rel_relkind(refrelid) == RELKIND_PARTITIONED_TABLE)
  {
    ScanKeyData pkey;
    SysScanDesc pscan;
    HeapTuple childtup;

    ScanKeyInit(&pkey, Anum_pg_constraint_conparentid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(conoid));

    pscan = systable_beginscan(conrel, ConstraintParentIndexId, true, NULL, 1, &pkey);

    while (HeapTupleIsValid(childtup = systable_getnext(pscan)))
    {
      Form_pg_constraint childcon = (Form_pg_constraint)GETSTRUCT(childtup);
      Relation childrel;

      childrel = table_open(childcon->conrelid, lockmode);
      ATExecAlterConstrRecurse(cmdcon, conrel, tgrel, childrel, childtup, otherrelids, lockmode);
      table_close(childrel, NoLock);
    }

    systable_endscan(pscan);
  }

  return changed;
}

   
                                   
   
                                                                             
                                                                              
                                                                           
                           
   
                                                                               
                                                            
   
static ObjectAddress
ATExecValidateConstraint(List **wqueue, Relation rel, char *constrName, bool recurse, bool recursing, LOCKMODE lockmode)
{
  Relation conrel;
  SysScanDesc scan;
  ScanKeyData skey[3];
  HeapTuple tuple;
  Form_pg_constraint con;
  ObjectAddress address;

  conrel = table_open(ConstraintRelationId, RowExclusiveLock);

     
                                          
     
  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(constrName));
  scan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (!HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" of relation \"%s\" does not exist", constrName, RelationGetRelationName(rel))));
  }

  con = (Form_pg_constraint)GETSTRUCT(tuple);
  if (con->contype != CONSTRAINT_FOREIGN && con->contype != CONSTRAINT_CHECK)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("constraint \"%s\" of relation \"%s\" is not a foreign key or check constraint", constrName, RelationGetRelationName(rel))));
  }

  if (!con->convalidated)
  {
    AlteredTableInfo *tab;
    HeapTuple copyTuple;
    Form_pg_constraint copy_con;

    if (con->contype == CONSTRAINT_FOREIGN)
    {
      NewConstraint *newcon;
      Constraint *fkconstraint;

                                        
      fkconstraint = makeNode(Constraint);
                                       
      fkconstraint->conname = constrName;

      newcon = (NewConstraint *)palloc0(sizeof(NewConstraint));
      newcon->name = constrName;
      newcon->contype = CONSTR_FOREIGN;
      newcon->refrelid = con->confrelid;
      newcon->refindid = con->conindid;
      newcon->conid = con->oid;
      newcon->qual = (Node *)fkconstraint;

                                                          
      tab = ATGetQueueEntry(wqueue, rel);
      tab->constraints = lappend(tab->constraints, newcon);

         
                                                              
                                                                    
         
    }
    else if (con->contype == CONSTRAINT_CHECK)
    {
      List *children = NIL;
      ListCell *child;
      NewConstraint *newcon;
      bool isnull;
      Datum val;
      char *conbin;

         
                                                                       
                                                                     
                                                       
         
      if (!recursing && !con->connoinherit)
      {
        children = find_all_inheritors(RelationGetRelid(rel), lockmode, NULL);
      }

         
                                                                     
                                                                         
                          
         
                                                                       
                    
         
      foreach (child, children)
      {
        Oid childoid = lfirst_oid(child);
        Relation childrel;

        if (childoid == RelationGetRelid(rel))
        {
          continue;
        }

           
                                                                      
                                                                     
                                                                 
           
        if (!recurse)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("constraint must be validated on child tables too")));
        }

                                                  
        childrel = table_open(childoid, NoLock);

        ATExecValidateConstraint(wqueue, childrel, constrName, false, true, lockmode);
        table_close(childrel, NoLock);
      }

                                        
      newcon = (NewConstraint *)palloc0(sizeof(NewConstraint));
      newcon->name = constrName;
      newcon->contype = CONSTR_CHECK;
      newcon->refrelid = InvalidOid;
      newcon->refindid = InvalidOid;
      newcon->conid = con->oid;

      val = SysCacheGetAttr(CONSTROID, tuple, Anum_pg_constraint_conbin, &isnull);
      if (isnull)
      {
        elog(ERROR, "null conbin for constraint %u", con->oid);
      }

      conbin = TextDatumGetCString(val);
      newcon->qual = (Node *)stringToNode(conbin);

                                                          
      tab = ATGetQueueEntry(wqueue, rel);
      tab->constraints = lappend(tab->constraints, newcon);

         
                                                                  
                     
         
      CacheInvalidateRelcache(rel);
    }

       
                                                            
       
    copyTuple = heap_copytuple(tuple);
    copy_con = (Form_pg_constraint)GETSTRUCT(copyTuple);
    copy_con->convalidated = true;
    CatalogTupleUpdate(conrel, &copyTuple->t_self, copyTuple);

    InvokeObjectPostAlterHook(ConstraintRelationId, con->oid, 0);

    heap_freetuple(copyTuple);

    ObjectAddressSet(address, ConstraintRelationId, con->oid);
  }
  else
  {
    address = InvalidObjectAddress;                        
  }

  systable_endscan(scan);

  table_close(conrel, RowExclusiveLock);

  return address;
}

   
                                                            
   
                                                       
   
static int
transformColumnNameList(Oid relId, List *colList, int16 *attnums, Oid *atttypids)
{
  ListCell *l;
  int attnum;

  attnum = 0;
  foreach (l, colList)
  {
    char *attname = strVal(lfirst(l));
    HeapTuple atttuple;

    atttuple = SearchSysCacheAttName(relId, attname);
    if (!HeapTupleIsValid(atttuple))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" referenced in foreign key constraint does not exist", attname)));
    }
    if (attnum >= INDEX_MAX_KEYS)
    {
      ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("cannot have more than %d keys in a foreign key", INDEX_MAX_KEYS)));
    }
    attnums[attnum] = ((Form_pg_attribute)GETSTRUCT(atttuple))->attnum;
    atttypids[attnum] = ((Form_pg_attribute)GETSTRUCT(atttuple))->atttypid;
    ReleaseSysCache(atttuple);
    attnum++;
  }

  return attnum;
}

   
                                
   
                                                                       
                                                                        
                                     
   
                                                                          
                                                                
   
                                                                         
   
static int
transformFkeyGetPrimaryKey(Relation pkrel, Oid *indexOid, List **attnamelist, int16 *attnums, Oid *atttypids, Oid *opclasses)
{
  List *indexoidlist;
  ListCell *indexoidscan;
  HeapTuple indexTuple = NULL;
  Form_pg_index indexStruct = NULL;
  Datum indclassDatum;
  bool isnull;
  oidvector *indclass;
  int i;

     
                                                                             
                                                                            
                                                                          
     
  *indexOid = InvalidOid;

  indexoidlist = RelationGetIndexList(pkrel);

  foreach (indexoidscan, indexoidlist)
  {
    Oid indexoid = lfirst_oid(indexoidscan);

    indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexoid);
    }
    indexStruct = (Form_pg_index)GETSTRUCT(indexTuple);
    if (indexStruct->indisprimary && indexStruct->indisvalid)
    {
         
                                                                        
                                                                         
                            
         
      if (!indexStruct->indimmediate)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot use a deferrable primary key for referenced table \"%s\"", RelationGetRelationName(pkrel))));
      }

      *indexOid = indexoid;
      break;
    }
    ReleaseSysCache(indexTuple);
  }

  list_free(indexoidlist);

     
                            
     
  if (!OidIsValid(*indexOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("there is no primary key for referenced table \"%s\"", RelationGetRelationName(pkrel))));
  }

                                      
  indclassDatum = SysCacheGetAttr(INDEXRELID, indexTuple, Anum_pg_index_indclass, &isnull);
  Assert(!isnull);
  indclass = (oidvector *)DatumGetPointer(indclassDatum);

     
                                                                        
                                                             
     
  *attnamelist = NIL;
  for (i = 0; i < indexStruct->indnkeyatts; i++)
  {
    int pkattno = indexStruct->indkey.values[i];

    attnums[i] = pkattno;
    atttypids[i] = attnumTypeId(pkrel, pkattno);
    opclasses[i] = indclass->values[i];
    *attnamelist = lappend(*attnamelist, makeString(pstrdup(NameStr(*attnumAttName(pkrel, pkattno)))));
  }

  ReleaseSysCache(indexTuple);

  return i;
}

   
                             
   
                                                                          
                                                                        
                                                                      
            
   
static Oid
transformFkeyCheckAttrs(Relation pkrel, int numattrs, int16 *attnums, Oid *opclasses)                       
{
  Oid indexoid = InvalidOid;
  bool found = false;
  bool found_deferrable = false;
  List *indexoidlist;
  ListCell *indexoidscan;
  int i, j;

     
                                                                             
                                                                             
                                                                         
                                                                        
                                         
     
  for (i = 0; i < numattrs; i++)
  {
    for (j = i + 1; j < numattrs; j++)
    {
      if (attnums[i] == attnums[j])
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FOREIGN_KEY), errmsg("foreign key referenced-columns list must not contain duplicates")));
      }
    }
  }

     
                                                                             
                                                                             
                              
     
  indexoidlist = RelationGetIndexList(pkrel);

  foreach (indexoidscan, indexoidlist)
  {
    HeapTuple indexTuple;
    Form_pg_index indexStruct;

    indexoid = lfirst_oid(indexoidscan);
    indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexoid);
    }
    indexStruct = (Form_pg_index)GETSTRUCT(indexTuple);

       
                                                                       
                                                                           
                                
       
    if (indexStruct->indnkeyatts == numattrs && indexStruct->indisunique && indexStruct->indisvalid && heap_attisnull(indexTuple, Anum_pg_index_indpred, NULL) && heap_attisnull(indexTuple, Anum_pg_index_indexprs, NULL))
    {
      Datum indclassDatum;
      bool isnull;
      oidvector *indclass;

                                          
      indclassDatum = SysCacheGetAttr(INDEXRELID, indexTuple, Anum_pg_index_indclass, &isnull);
      Assert(!isnull);
      indclass = (oidvector *)DatumGetPointer(indclassDatum);

         
                                                                         
                                                                        
                      
         
                                                                      
                                                                         
                                                                        
                                                                   
         
      for (i = 0; i < numattrs; i++)
      {
        found = false;
        for (j = 0; j < numattrs; j++)
        {
          if (attnums[i] == indexStruct->indkey.values[j])
          {
            opclasses[i] = indclass->values[j];
            found = true;
            break;
          }
        }
        if (!found)
        {
          break;
        }
      }

         
                                                                         
                                                                         
                                  
         
      if (found && !indexStruct->indimmediate)
      {
           
                                                                       
                                                             
           
        found_deferrable = true;
        found = false;
      }
    }
    ReleaseSysCache(indexTuple);
    if (found)
    {
      break;
    }
  }

  if (!found)
  {
    if (found_deferrable)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot use a deferrable unique constraint for referenced table \"%s\"", RelationGetRelationName(pkrel))));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FOREIGN_KEY), errmsg("there is no unique constraint matching given keys for referenced table \"%s\"", RelationGetRelationName(pkrel))));
    }
  }

  list_free(indexoidlist);

  return indexoid;
}

   
                  
   
                                                                           
                                                                           
   
static CoercionPathType
findFkeyCast(Oid targetTypeId, Oid sourceTypeId, Oid *funcid)
{
  CoercionPathType ret;

  if (targetTypeId == sourceTypeId)
  {
    ret = COERCION_PATH_RELABELTYPE;
    *funcid = InvalidOid;
  }
  else
  {
    ret = find_coercion_pathway(targetTypeId, sourceTypeId, COERCION_IMPLICIT, funcid);
    if (ret == COERCION_PATH_NONE)
    {
                                                      
      elog(ERROR, "could not find cast from %u to %u", sourceTypeId, targetTypeId);
    }
  }

  return ret;
}

   
                                                                  
   
                                                                           
                                                                               
   
static void
checkFkeyPermissions(Relation rel, int16 *attnums, int natts)
{
  Oid roleid = GetUserId();
  AclResult aclresult;
  int i;

                                                            
  aclresult = pg_class_aclcheck(RelationGetRelid(rel), roleid, ACL_REFERENCES);
  if (aclresult == ACLCHECK_OK)
  {
    return;
  }
                                                   
  for (i = 0; i < natts; i++)
  {
    aclresult = pg_attribute_aclcheck(RelationGetRelid(rel), attnums[i], roleid, ACL_REFERENCES);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, get_relkind_objtype(rel->rd_rel->relkind), RelationGetRelationName(rel));
    }
  }
}

   
                                                                       
               
   
                                                                    
   
static void
validateForeignKeyConstraint(char *conname, Relation rel, Relation pkrel, Oid pkindOid, Oid constraintOid)
{
  TupleTableSlot *slot;
  TableScanDesc scan;
  Trigger trig;
  Snapshot snapshot;
  MemoryContext oldcxt;
  MemoryContext perTupCxt;

  ereport(DEBUG1, (errmsg("validating foreign key constraint \"%s\"", conname)));

     
                                                               
     
  MemSet(&trig, 0, sizeof(trig));
  trig.tgoid = InvalidOid;
  trig.tgname = conname;
  trig.tgenabled = TRIGGER_FIRES_ON_ORIGIN;
  trig.tgisinternal = true;
  trig.tgconstrrelid = RelationGetRelid(pkrel);
  trig.tgconstrindid = pkindOid;
  trig.tgconstraint = constraintOid;
  trig.tgdeferrable = false;
  trig.tginitdeferred = false;
                                           

     
                                                                        
                                                                 
     
  if (RI_Initial_Check(&trig, rel, pkrel))
  {
    return;
  }

     
                                                                            
                                                                            
                                     
     
  snapshot = RegisterSnapshot(GetLatestSnapshot());
  slot = table_slot_create(rel, NULL);
  scan = table_beginscan(rel, snapshot, 0, NULL);

  perTupCxt = AllocSetContextCreate(CurrentMemoryContext, "validateForeignKeyConstraint", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(perTupCxt);

  while (table_scan_getnextslot(scan, ForwardScanDirection, slot))
  {
    LOCAL_FCINFO(fcinfo, 0);
    TriggerData trigdata;

    CHECK_FOR_INTERRUPTS();

       
                                           
       
                                                         
       
    MemSet(fcinfo, 0, SizeForFunctionCallInfo(0));

       
                                                           
       
    trigdata.type = T_TriggerData;
    trigdata.tg_event = TRIGGER_EVENT_INSERT | TRIGGER_EVENT_ROW;
    trigdata.tg_relation = rel;
    trigdata.tg_trigtuple = ExecFetchSlotHeapTuple(slot, false, NULL);
    trigdata.tg_trigslot = slot;
    trigdata.tg_newtuple = NULL;
    trigdata.tg_newslot = NULL;
    trigdata.tg_trigger = &trig;

    fcinfo->context = (Node *)&trigdata;

    RI_FKey_check_ins(fcinfo);

    MemoryContextReset(perTupCxt);
  }

  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(perTupCxt);
  table_endscan(scan);
  UnregisterSnapshot(snapshot);
  ExecDropSingleTupleTableSlot(slot);
}

static void
CreateFKCheckTrigger(Oid myRelOid, Oid refRelOid, Constraint *fkconstraint, Oid constraintOid, Oid indexOid, bool on_insert)
{
  CreateTrigStmt *fk_trigger;

     
                                                                            
                                                                           
                                                                           
                                                                            
                                                                          
                                                                            
                                                               
     
  fk_trigger = makeNode(CreateTrigStmt);
  fk_trigger->trigname = "RI_ConstraintTrigger_c";
  fk_trigger->relation = NULL;
  fk_trigger->row = true;
  fk_trigger->timing = TRIGGER_TYPE_AFTER;

                                     
  if (on_insert)
  {
    fk_trigger->funcname = SystemFuncName("RI_FKey_check_ins");
    fk_trigger->events = TRIGGER_TYPE_INSERT;
  }
  else
  {
    fk_trigger->funcname = SystemFuncName("RI_FKey_check_upd");
    fk_trigger->events = TRIGGER_TYPE_UPDATE;
  }

  fk_trigger->columns = NIL;
  fk_trigger->transitionRels = NIL;
  fk_trigger->whenClause = NULL;
  fk_trigger->isconstraint = true;
  fk_trigger->deferrable = fkconstraint->deferrable;
  fk_trigger->initdeferred = fkconstraint->initdeferred;
  fk_trigger->constrrel = NULL;
  fk_trigger->args = NIL;

  (void)CreateTrigger(fk_trigger, NULL, myRelOid, refRelOid, constraintOid, indexOid, InvalidOid, InvalidOid, NULL, true, false);

                                   
  CommandCounterIncrement();
}

   
                                  
                                                                          
         
   
static void
createForeignKeyActionTriggers(Relation rel, Oid refRelOid, Constraint *fkconstraint, Oid constraintOid, Oid indexOid)
{
  CreateTrigStmt *fk_trigger;

     
                                                                        
                                            
     
  fk_trigger = makeNode(CreateTrigStmt);
  fk_trigger->trigname = "RI_ConstraintTrigger_a";
  fk_trigger->relation = NULL;
  fk_trigger->row = true;
  fk_trigger->timing = TRIGGER_TYPE_AFTER;
  fk_trigger->events = TRIGGER_TYPE_DELETE;
  fk_trigger->columns = NIL;
  fk_trigger->transitionRels = NIL;
  fk_trigger->whenClause = NULL;
  fk_trigger->isconstraint = true;
  fk_trigger->constrrel = NULL;
  switch (fkconstraint->fk_del_action)
  {
  case FKCONSTR_ACTION_NOACTION:
    fk_trigger->deferrable = fkconstraint->deferrable;
    fk_trigger->initdeferred = fkconstraint->initdeferred;
    fk_trigger->funcname = SystemFuncName("RI_FKey_noaction_del");
    break;
  case FKCONSTR_ACTION_RESTRICT:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_restrict_del");
    break;
  case FKCONSTR_ACTION_CASCADE:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_cascade_del");
    break;
  case FKCONSTR_ACTION_SETNULL:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_setnull_del");
    break;
  case FKCONSTR_ACTION_SETDEFAULT:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_setdefault_del");
    break;
  default:
    elog(ERROR, "unrecognized FK action type: %d", (int)fkconstraint->fk_del_action);
    break;
  }
  fk_trigger->args = NIL;

  (void)CreateTrigger(fk_trigger, NULL, refRelOid, RelationGetRelid(rel), constraintOid, indexOid, InvalidOid, InvalidOid, NULL, true, false);

                                   
  CommandCounterIncrement();

     
                                                                        
                                            
     
  fk_trigger = makeNode(CreateTrigStmt);
  fk_trigger->trigname = "RI_ConstraintTrigger_a";
  fk_trigger->relation = NULL;
  fk_trigger->row = true;
  fk_trigger->timing = TRIGGER_TYPE_AFTER;
  fk_trigger->events = TRIGGER_TYPE_UPDATE;
  fk_trigger->columns = NIL;
  fk_trigger->transitionRels = NIL;
  fk_trigger->whenClause = NULL;
  fk_trigger->isconstraint = true;
  fk_trigger->constrrel = NULL;
  switch (fkconstraint->fk_upd_action)
  {
  case FKCONSTR_ACTION_NOACTION:
    fk_trigger->deferrable = fkconstraint->deferrable;
    fk_trigger->initdeferred = fkconstraint->initdeferred;
    fk_trigger->funcname = SystemFuncName("RI_FKey_noaction_upd");
    break;
  case FKCONSTR_ACTION_RESTRICT:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_restrict_upd");
    break;
  case FKCONSTR_ACTION_CASCADE:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_cascade_upd");
    break;
  case FKCONSTR_ACTION_SETNULL:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_setnull_upd");
    break;
  case FKCONSTR_ACTION_SETDEFAULT:
    fk_trigger->deferrable = false;
    fk_trigger->initdeferred = false;
    fk_trigger->funcname = SystemFuncName("RI_FKey_setdefault_upd");
    break;
  default:
    elog(ERROR, "unrecognized FK action type: %d", (int)fkconstraint->fk_upd_action);
    break;
  }
  fk_trigger->args = NIL;

  (void)CreateTrigger(fk_trigger, NULL, refRelOid, RelationGetRelid(rel), constraintOid, indexOid, InvalidOid, InvalidOid, NULL, true, false);
}

   
                                 
                                                                          
         
   
static void
createForeignKeyCheckTriggers(Oid myRelOid, Oid refRelOid, Constraint *fkconstraint, Oid constraintOid, Oid indexOid)
{
  CreateFKCheckTrigger(myRelOid, refRelOid, fkconstraint, constraintOid, indexOid, true);
  CreateFKCheckTrigger(myRelOid, refRelOid, fkconstraint, constraintOid, indexOid, false);
}

   
                               
   
                                                                              
   
static void
ATExecDropConstraint(Relation rel, const char *constrName, DropBehavior behavior, bool recurse, bool recursing, bool missing_ok, LOCKMODE lockmode)
{
  List *children;
  ListCell *child;
  Relation conrel;
  Form_pg_constraint con;
  SysScanDesc scan;
  ScanKeyData skey[3];
  HeapTuple tuple;
  bool found = false;
  bool is_no_inherit_constraint = false;
  char contype;

                                                                        
  if (recursing)
  {
    ATSimplePermissions(rel, ATT_TABLE | ATT_FOREIGN_TABLE);
  }

  conrel = table_open(ConstraintRelationId, RowExclusiveLock);

     
                                         
     
  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(constrName));
  scan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    ObjectAddress conobj;

    con = (Form_pg_constraint)GETSTRUCT(tuple);

                                          
    if (con->coninhcount > 0 && !recursing)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot drop inherited constraint \"%s\" of relation \"%s\"", constrName, RelationGetRelationName(rel))));
    }

    is_no_inherit_constraint = con->connoinherit;
    contype = con->contype;

       
                                                                         
                                                                          
                                                                           
                                                                   
                              
       
    if (contype == CONSTRAINT_FOREIGN && con->confrelid != RelationGetRelid(rel))
    {
      Relation frel;

                                                       
      frel = table_open(con->confrelid, AccessExclusiveLock);
      CheckTableNotInUse(frel, "ALTER TABLE");
      table_close(frel, NoLock);
    }

       
                                              
       
    conobj.classId = ConstraintRelationId;
    conobj.objectId = con->oid;
    conobj.objectSubId = 0;

    performDeletion(&conobj, behavior, 0);

    found = true;
  }

  systable_endscan(scan);

  if (!found)
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" of relation \"%s\" does not exist", constrName, RelationGetRelationName(rel))));
    }
    else
    {
      ereport(NOTICE, (errmsg("constraint \"%s\" of relation \"%s\" does not exist, skipping", constrName, RelationGetRelationName(rel))));
      table_close(conrel, RowExclusiveLock);
      return;
    }
  }

     
                                                                             
                                                   
     
  if (contype != CONSTRAINT_CHECK && rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    table_close(conrel, RowExclusiveLock);
    return;
  }

     
                                                                    
                                                                             
                                                   
     
  if (!is_no_inherit_constraint)
  {
    children = find_inheritance_children(RelationGetRelid(rel), lockmode);
  }
  else
  {
    children = NIL;
  }

     
                                                                         
                                                                             
                                                                            
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && children != NIL && !recurse)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot remove constraint from only the partitioned table when partitions exist"), errhint("Do not specify the ONLY keyword.")));
  }

  foreach (child, children)
  {
    Oid childrelid = lfirst_oid(child);
    Relation childrel;
    HeapTuple copy_tuple;

                                                    
    childrel = table_open(childrelid, NoLock);
    CheckTableNotInUse(childrel, "ALTER TABLE");

    ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(childrelid));
    ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
    ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(constrName));
    scan = systable_beginscan(conrel, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                               
    if (!HeapTupleIsValid(tuple = systable_getnext(scan)))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" of relation \"%s\" does not exist", constrName, RelationGetRelationName(childrel))));
    }

    copy_tuple = heap_copytuple(tuple);

    systable_endscan(scan);

    con = (Form_pg_constraint)GETSTRUCT(copy_tuple);

                                                           
    if (con->contype != CONSTRAINT_CHECK)
    {
      elog(ERROR, "inherited constraint is not a CHECK constraint");
    }

    if (con->coninhcount <= 0)                       
    {
      elog(ERROR, "relation %u has non-inherited constraint \"%s\"", childrelid, constrName);
    }

    if (recurse)
    {
         
                                                                    
                                                                        
         
      if (con->coninhcount == 1 && !con->conislocal)
      {
                                                       
        ATExecDropConstraint(childrel, constrName, behavior, true, true, false, lockmode);
      }
      else
      {
                                                       
        con->coninhcount--;
        CatalogTupleUpdate(conrel, &copy_tuple->t_self, copy_tuple);

                                 
        CommandCounterIncrement();
      }
    }
    else
    {
         
                                                                       
                                                                     
                                
         
      con->coninhcount--;
      con->conislocal = true;

      CatalogTupleUpdate(conrel, &copy_tuple->t_self, copy_tuple);

                               
      CommandCounterIncrement();
    }

    heap_freetuple(copy_tuple);

    table_close(childrel, NoLock);
  }

  table_close(conrel, RowExclusiveLock);
}

   
                     
   
static void
ATPrepAlterColumnType(List **wqueue, AlteredTableInfo *tab, Relation rel, bool recurse, bool recursing, AlterTableCmd *cmd, LOCKMODE lockmode)
{
  char *colName = cmd->name;
  ColumnDef *def = (ColumnDef *)cmd->def;
  TypeName *typeName = def->typeName;
  Node *transform = def->cooked_default;
  HeapTuple tuple;
  Form_pg_attribute attTup;
  AttrNumber attnum;
  Oid targettype;
  int32 targettypmod;
  Oid targetcollid;
  NewColumnValue *newval;
  ParseState *pstate = make_parsestate(NULL);
  AclResult aclresult;
  bool is_expr;

  if (rel->rd_rel->reloftype && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot alter column type of typed table")));
  }

                                                               
  tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }
  attTup = (Form_pg_attribute)GETSTRUCT(tuple);
  attnum = attTup->attnum;

                                      
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

     
                                                                             
                                                                             
                                   
     
  if (attTup->attinhcount > 0 && !recursing)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot alter inherited column \"%s\"", colName)));
  }

                                                     
  if (has_partition_attrs(rel, bms_make_singleton(attnum - FirstLowInvalidHeapAttributeNumber), &is_expr))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot alter column \"%s\" because it is part of the partition key of relation \"%s\"", colName, RelationGetRelationName(rel))));
  }

                               
  typenameTypeIdAndMod(NULL, typeName, &targettype, &targettypmod);

  aclresult = pg_type_aclcheck(targettype, GetUserId(), ACL_USAGE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error_type(aclresult, targettype);
  }

                         
  targetcollid = GetColumnDefCollation(NULL, def, targettype);

                                                
  CheckAttributeType(colName, targettype, targetcollid, list_make1_oid(rel->rd_rel->reltype), 0);

  if (tab->relkind == RELKIND_RELATION || tab->relkind == RELKIND_PARTITIONED_TABLE)
  {
       
                                                                       
                                                                
                                                                      
                                                                  
                                                                           
                                                                          
             
       
    if (!transform)
    {
      transform = (Node *)makeVar(1, attnum, attTup->atttypid, attTup->atttypmod, attTup->attcollation, 0);
    }

    transform = coerce_to_target_type(pstate, transform, exprType(transform), targettype, targettypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (transform == NULL)
    {
                                                                    
      if (def->cooked_default != NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                           errmsg("result of USING clause for column \"%s\""
                                  " cannot be cast automatically to type %s",
                               colName, format_type_be(targettype)),
                           errhint("You might need to add an explicit cast.")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("column \"%s\" cannot be cast automatically to type %s", colName, format_type_be(targettype)),
                                                                             
                           errhint("You might need to specify \"USING %s::%s\".", quote_identifier(colName), format_type_with_typemod(targettype, targettypmod))));
      }
    }

                                       
    assign_expr_collations(pstate, transform);

                                                                            
    transform = (Node *)expression_planner((Expr *)transform);

       
                                                                      
                 
       
    newval = (NewColumnValue *)palloc0(sizeof(NewColumnValue));
    newval->attnum = attnum;
    newval->expr = (Expr *)transform;
    newval->is_generated = false;

    tab->newvals = lappend(tab->newvals, newval);
    if (ATColumnChangeRequiresRewrite(transform, attnum))
    {
      tab->rewrite |= AT_REWRITE_COLUMN_REWRITE;
    }
  }
  else if (transform)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table", RelationGetRelationName(rel))));
  }

  if (!RELKIND_HAS_STORAGE(tab->relkind))
  {
       
                                                                         
                                                              
       
    find_composite_type_dependencies(rel->rd_rel->reltype, rel, NULL);
  }

  ReleaseSysCache(tuple);

     
                                                                   
                                                                          
                                                              
     
                                                                      
                                                        
     
  if (recurse)
  {
    Oid relid = RelationGetRelid(rel);
    List *child_oids, *child_numparents;
    ListCell *lo, *li;

    child_oids = find_all_inheritors(relid, lockmode, &child_numparents);

       
                                                                        
                                                                           
                             
       
    forboth(lo, child_oids, li, child_numparents)
    {
      Oid childrelid = lfirst_oid(lo);
      int numparents = lfirst_int(li);
      Relation childrel;
      HeapTuple childtuple;
      Form_pg_attribute childattTup;

      if (childrelid == relid)
      {
        continue;
      }

                                                
      childrel = relation_open(childrelid, NoLock);
      CheckTableNotInUse(childrel, "ALTER TABLE");

         
                                                                         
                                                                        
                                                                    
                                                        
         
      childtuple = SearchSysCacheAttName(RelationGetRelid(childrel), colName);
      if (!HeapTupleIsValid(childtuple))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(childrel))));
      }
      childattTup = (Form_pg_attribute)GETSTRUCT(childtuple);

      if (childattTup->attinhcount > numparents)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot alter inherited column \"%s\" of relation \"%s\"", colName, RelationGetRelationName(childrel))));
      }

      ReleaseSysCache(childtuple);

         
                                                                  
                                                    
         
      if (def->cooked_default)
      {
        AttrNumber *attmap;
        bool found_whole_row;

                                          
        cmd = copyObject(cmd);

        attmap = convert_tuples_by_name_map(RelationGetDescr(childrel), RelationGetDescr(rel), gettext_noop("could not convert row type"));
        ((ColumnDef *)cmd->def)->cooked_default = map_variable_attnos(def->cooked_default, 1, 0, attmap, RelationGetDescr(rel)->natts, InvalidOid, &found_whole_row);
        if (found_whole_row)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert whole-row table reference"), errdetail("USING expression contains a whole-row table reference.")));
        }
        pfree(attmap);
      }
      ATPrepCmd(wqueue, childrel, cmd, false, true, lockmode);
      relation_close(childrel, NoLock);
    }
  }
  else if (!recursing && find_inheritance_children(RelationGetRelid(rel), NoLock) != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("type of inherited column \"%s\" must be changed in child tables too", colName)));
  }

  if (tab->relkind == RELKIND_COMPOSITE_TYPE)
  {
    ATTypedTableRecursion(wqueue, rel, cmd, lockmode);
  }
}

   
                                                                              
                                                                           
                                                                          
                           
   
                                                      
                                                               
                                                                               
   
                                                                          
                                                                               
                                   
   
static bool
ATColumnChangeRequiresRewrite(Node *expr, AttrNumber varattno)
{
  Assert(expr != NULL);

  for (;;)
  {
                                                  
    if (IsA(expr, Var) && ((Var *)expr)->varattno == varattno)
    {
      return false;
    }
    else if (IsA(expr, RelabelType))
    {
      expr = (Node *)((RelabelType *)expr)->arg;
    }
    else if (IsA(expr, CoerceToDomain))
    {
      CoerceToDomain *d = (CoerceToDomain *)expr;

      if (DomainHasConstraints(d->resulttype))
      {
        return true;
      }
      expr = (Node *)d->arg;
    }
    else if (IsA(expr, FuncExpr))
    {
      FuncExpr *f = (FuncExpr *)expr;

      switch (f->funcid)
      {
      case F_TIMESTAMPTZ_TIMESTAMP:
      case F_TIMESTAMP_TIMESTAMPTZ:
        if (TimestampTimestampTzRequiresRewrite())
        {
          return true;
        }
        else
        {
          expr = linitial(f->args);
        }
        break;
      default:
        return true;
      }
    }
    else
    {
      return true;
    }
  }
}

   
                                 
   
                                              
   
static ObjectAddress
ATExecAlterColumnType(AlteredTableInfo *tab, Relation rel, AlterTableCmd *cmd, LOCKMODE lockmode)
{
  char *colName = cmd->name;
  ColumnDef *def = (ColumnDef *)cmd->def;
  TypeName *typeName = def->typeName;
  HeapTuple heapTup;
  Form_pg_attribute attTup, attOldTup;
  AttrNumber attnum;
  HeapTuple typeTuple;
  Form_pg_type tform;
  Oid targettype;
  int32 targettypmod;
  Oid targetcollid;
  Node *defaultexpr;
  Relation attrelation;
  Relation depRel;
  ScanKeyData key[3];
  SysScanDesc scan;
  HeapTuple depTup;
  ObjectAddress address;

     
                                                                           
                             
     
  if (tab->rewrite)
  {
    Relation newrel;

    newrel = table_open(RelationGetRelid(rel), NoLock);
    RelationClearMissing(newrel);
    relation_close(newrel, NoLock);
                                                                        
    CommandCounterIncrement();
  }

  attrelation = table_open(AttributeRelationId, RowExclusiveLock);

                                 
  heapTup = SearchSysCacheCopyAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(heapTup))                       
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }
  attTup = (Form_pg_attribute)GETSTRUCT(heapTup);
  attnum = attTup->attnum;
  attOldTup = TupleDescAttr(tab->oldDesc, attnum - 1);

                                                                   
  if (attTup->atttypid != attOldTup->atttypid || attTup->atttypmod != attOldTup->atttypmod)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter type of column \"%s\" twice", colName)));
  }

                                                                      
  typeTuple = typenameType(NULL, typeName, &targettypmod);
  tform = (Form_pg_type)GETSTRUCT(typeTuple);
  targettype = tform->oid;
                         
  targetcollid = GetColumnDefCollation(NULL, def, targettype);

     
                                                                           
                                                                          
                                                                      
                                                                        
     
                                                                       
                                                                          
                                                                             
                                                                          
                                                   
     
  if (attTup->atthasdef)
  {
    defaultexpr = build_column_default(rel, attnum);
    Assert(defaultexpr);
    defaultexpr = strip_implicit_coercions(defaultexpr);
    defaultexpr = coerce_to_target_type(NULL,                        
        defaultexpr, exprType(defaultexpr), targettype, targettypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (defaultexpr == NULL)
    {
      if (attTup->attgenerated)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("generation expression for column \"%s\" cannot be cast automatically to type %s", colName, format_type_be(targettype))));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("default for column \"%s\" cannot be cast automatically to type %s", colName, format_type_be(targettype))));
      }
    }
  }
  else
  {
    defaultexpr = NULL;
  }

     
                                                                             
                                                                   
     
                                                                        
                                                                          
                                                                          
                   
     
  depRel = table_open(DependRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_depend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  ScanKeyInit(&key[2], Anum_pg_depend_refobjsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum((int32)attnum));

  scan = systable_beginscan(depRel, DependReferenceIndexId, true, NULL, 3, key);

  while (HeapTupleIsValid(depTup = systable_getnext(scan)))
  {
    Form_pg_depend foundDep = (Form_pg_depend)GETSTRUCT(depTup);
    ObjectAddress foundObject;

                                                         
    if (foundDep->deptype == DEPENDENCY_PIN)
    {
      elog(ERROR, "cannot alter type of a pinned column");
    }

    foundObject.classId = foundDep->classid;
    foundObject.objectId = foundDep->objid;
    foundObject.objectSubId = foundDep->objsubid;

    switch (getObjectClass(&foundObject))
    {
    case OCLASS_CLASS:
    {
      char relKind = get_rel_relkind(foundObject.objectId);

      if (relKind == RELKIND_INDEX || relKind == RELKIND_PARTITIONED_INDEX)
      {
        Assert(foundObject.objectSubId == 0);
        RememberIndexForRebuilding(foundObject.objectId, tab);
      }
      else if (relKind == RELKIND_SEQUENCE)
      {
           
                                                             
                                  
           
        Assert(foundObject.objectSubId == 0);
      }
      else if (relKind == RELKIND_RELATION && foundObject.objectSubId != 0 && get_attgenerated(foundObject.objectId, foundObject.objectSubId))
      {
           
                                                           
                                                               
                                                          
           
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot alter type of a column used by a generated column"), errdetail("Column \"%s\" is used by generated column \"%s\".", colName, get_attname(foundObject.objectId, foundObject.objectSubId, false))));
      }
      else
      {
                                                            
        elog(ERROR, "unexpected object depending on column: %s", getObjectDescription(&foundObject));
      }
      break;
    }

    case OCLASS_CONSTRAINT:
      Assert(foundObject.objectSubId == 0);
      RememberConstraintForRebuilding(foundObject.objectId, tab);
      break;

    case OCLASS_REWRITE:
                                                              
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter type of a column used by a view or rule"), errdetail("%s depends on column \"%s\"", getObjectDescription(&foundObject), colName)));
      break;

    case OCLASS_TRIGGER:

         
                                                                
                                                                 
                                                                     
                                                               
                                                                 
                                                                     
                                                               
         
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter type of a column used in a trigger definition"), errdetail("%s depends on column \"%s\"", getObjectDescription(&foundObject), colName)));
      break;

    case OCLASS_POLICY:

         
                                                               
                                                            
                                                                   
                                                                  
                                                                     
                  
         
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter type of a column used in a policy definition"), errdetail("%s depends on column \"%s\"", getObjectDescription(&foundObject), colName)));
      break;

    case OCLASS_DEFAULT:

         
                                                                   
                   
         
      Assert(defaultexpr);
      break;

    case OCLASS_STATISTIC_EXT:

         
                                                                    
                                                   
         
      UpdateStatisticsForTypeChange(foundObject.objectId, RelationGetRelid(rel), attnum, attTup->atttypid, targettype);
      break;

    case OCLASS_PROC:
    case OCLASS_TYPE:
    case OCLASS_CAST:
    case OCLASS_COLLATION:
    case OCLASS_CONVERSION:
    case OCLASS_LANGUAGE:
    case OCLASS_LARGEOBJECT:
    case OCLASS_OPERATOR:
    case OCLASS_OPCLASS:
    case OCLASS_OPFAMILY:
    case OCLASS_AM:
    case OCLASS_AMOP:
    case OCLASS_AMPROC:
    case OCLASS_SCHEMA:
    case OCLASS_TSPARSER:
    case OCLASS_TSDICT:
    case OCLASS_TSTEMPLATE:
    case OCLASS_TSCONFIG:
    case OCLASS_ROLE:
    case OCLASS_DATABASE:
    case OCLASS_TBLSPACE:
    case OCLASS_FDW:
    case OCLASS_FOREIGN_SERVER:
    case OCLASS_USER_MAPPING:
    case OCLASS_DEFACL:
    case OCLASS_EXTENSION:
    case OCLASS_EVENT_TRIGGER:
    case OCLASS_PUBLICATION:
    case OCLASS_PUBLICATION_REL:
    case OCLASS_SUBSCRIPTION:
    case OCLASS_TRANSFORM:

         
                                                                    
                   
         
      elog(ERROR, "unexpected object depending on column: %s", getObjectDescription(&foundObject));
      break;

         
                                                                  
                                                                     
         
    }
  }

  systable_endscan(scan);

     
                                                                         
                                                                             
                                                                          
                                                
     
  ScanKeyInit(&key[0], Anum_pg_depend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  ScanKeyInit(&key[2], Anum_pg_depend_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum((int32)attnum));

  scan = systable_beginscan(depRel, DependDependerIndexId, true, NULL, 3, key);

  while (HeapTupleIsValid(depTup = systable_getnext(scan)))
  {
    Form_pg_depend foundDep = (Form_pg_depend)GETSTRUCT(depTup);
    ObjectAddress foundObject;

    foundObject.classId = foundDep->refclassid;
    foundObject.objectId = foundDep->refobjid;
    foundObject.objectSubId = foundDep->refobjsubid;

    if (foundDep->deptype != DEPENDENCY_NORMAL && foundDep->deptype != DEPENDENCY_AUTO)
    {
      elog(ERROR, "found unexpected dependency type '%c'", foundDep->deptype);
    }
    if (!(foundDep->refclassid == TypeRelationId && foundDep->refobjid == attTup->atttypid) && !(foundDep->refclassid == CollationRelationId && foundDep->refobjid == attTup->attcollation) && !(foundDep->refclassid == RelationRelationId && foundDep->refobjid == RelationGetRelid(rel) && foundDep->refobjsubid != 0))
    {
      elog(ERROR, "found unexpected dependency for column: %s", getObjectDescription(&foundObject));
    }

    CatalogTupleDelete(depRel, &depTup->t_self);
  }

  systable_endscan(scan);

  table_close(depRel, RowExclusiveLock);

     
                                                                          
                                                                             
                                                                            
                                                                      
     
  if (rel->rd_rel->relkind == RELKIND_RELATION && attTup->atthasmissing)
  {
    Datum missingval;
    bool missingNull;

                                                                        
    Assert(tab->rewrite == 0);

                                     
    missingval = heap_getattr(heapTup, Anum_pg_attribute_attmissingval, attrelation->rd_att, &missingNull);

                                                     

    if (!missingNull)
    {
         
                                                                     
                                                                      
                                                                     
                                           
         

      int one = 1;
      bool isNull;
      Datum valuesAtt[Natts_pg_attribute];
      bool nullsAtt[Natts_pg_attribute];
      bool replacesAtt[Natts_pg_attribute];
      HeapTuple newTup;

      MemSet(valuesAtt, 0, sizeof(valuesAtt));
      MemSet(nullsAtt, false, sizeof(nullsAtt));
      MemSet(replacesAtt, false, sizeof(replacesAtt));

      missingval = array_get_element(missingval, 1, &one, 0, attTup->attlen, attTup->attbyval, attTup->attalign, &isNull);
      missingval = PointerGetDatum(construct_array(&missingval, 1, targettype, tform->typlen, tform->typbyval, tform->typalign));

      valuesAtt[Anum_pg_attribute_attmissingval - 1] = missingval;
      replacesAtt[Anum_pg_attribute_attmissingval - 1] = true;
      nullsAtt[Anum_pg_attribute_attmissingval - 1] = false;

      newTup = heap_modify_tuple(heapTup, RelationGetDescr(attrelation), valuesAtt, nullsAtt, replacesAtt);
      heap_freetuple(heapTup);
      heapTup = newTup;
      attTup = (Form_pg_attribute)GETSTRUCT(heapTup);
    }
  }

  attTup->atttypid = targettype;
  attTup->atttypmod = targettypmod;
  attTup->attcollation = targetcollid;
  attTup->attndims = list_length(typeName->arrayBounds);
  attTup->attlen = tform->typlen;
  attTup->attbyval = tform->typbyval;
  attTup->attalign = tform->typalign;
  attTup->attstorage = tform->typstorage;

  ReleaseSysCache(typeTuple);

  CatalogTupleUpdate(attrelation, &heapTup->t_self, heapTup);

  table_close(attrelation, RowExclusiveLock);

                                                          
  add_column_datatype_dependency(RelationGetRelid(rel), attnum, targettype);
  add_column_collation_dependency(RelationGetRelid(rel), attnum, targetcollid);

     
                                                                           
     
  RemoveStatistics(RelationGetRelid(rel), attnum);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), attnum);

     
                                                                          
                                                                            
                                                                        
                                                                          
                                    
     
  if (defaultexpr)
  {
                                                                  
    CommandCounterIncrement();

       
                                                                        
                                          
       
    RemoveAttrDefault(RelationGetRelid(rel), attnum, DROP_RESTRICT, true, true);

    StoreAttrDefault(rel, attnum, defaultexpr, true, false);
  }

  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);

               
  heap_freetuple(heapTup);

  return address;
}

   
                                                                          
                      
   
static void
RememberReplicaIdentityForRebuilding(Oid indoid, AlteredTableInfo *tab)
{
  if (!get_index_isreplident(indoid))
  {
    return;
  }

  if (tab->replicaIdentityIndex)
  {
    elog(ERROR, "relation %u has multiple indexes marked as replica identity", tab->relid);
  }

  tab->replicaIdentityIndex = get_rel_name(indoid);
}

   
                                                                       
   
static void
RememberClusterOnForRebuilding(Oid indoid, AlteredTableInfo *tab)
{
  if (!get_index_isclustered(indoid))
  {
    return;
  }

  if (tab->clusterOnIndex)
  {
    elog(ERROR, "relation %u has multiple clustered indexes", tab->relid);
  }

  tab->clusterOnIndex = get_rel_name(indoid);
}

   
                                                                          
                                                
   
static void
RememberConstraintForRebuilding(Oid conoid, AlteredTableInfo *tab)
{
     
                                                                           
                                                                            
                                                                          
                                                                          
                                                                    
     
  if (!list_member_oid(tab->changedConstraintOids, conoid))
  {
                                                                 
    char *defstring = pg_get_constraintdef_command(conoid);
    Oid indoid;

    tab->changedConstraintOids = lappend_oid(tab->changedConstraintOids, conoid);
    tab->changedConstraintDefs = lappend(tab->changedConstraintDefs, defstring);

       
                                                                         
                                                                           
                                                                           
                         
       
    indoid = get_constraint_index(conoid);
    if (OidIsValid(indoid))
    {
      RememberReplicaIdentityForRebuilding(indoid, tab);
      RememberClusterOnForRebuilding(indoid, tab);
    }
  }
}

   
                                                                      
                                                
   
static void
RememberIndexForRebuilding(Oid indoid, AlteredTableInfo *tab)
{
     
                                                                           
                                                                           
                                                                          
                                                                           
                                                          
     
  if (!list_member_oid(tab->changedIndexOids, indoid))
  {
       
                                                                      
                                                                          
                                                                       
                                                                          
                                                                        
                                                        
       
    Oid conoid = get_index_constraint(indoid);

    if (OidIsValid(conoid))
    {
      RememberConstraintForRebuilding(conoid, tab);
    }
    else
    {
                                                              
      char *defstring = pg_get_indexdef_string(indoid);

      tab->changedIndexOids = lappend_oid(tab->changedIndexOids, indoid);
      tab->changedIndexDefs = lappend(tab->changedIndexDefs, defstring);

         
                                                                         
                                                                         
                                                                      
         
      RememberReplicaIdentityForRebuilding(indoid, tab);
      RememberClusterOnForRebuilding(indoid, tab);
    }
  }
}

   
                                                                    
                                                                      
                                                                  
                                                                   
                                          
   
static void
ATPostAlterTypeCleanup(List **wqueue, AlteredTableInfo *tab, LOCKMODE lockmode)
{
  ObjectAddress obj;
  ObjectAddresses *objects;
  ListCell *def_item;
  ListCell *oid_item;

     
                                                                            
                                                                           
                 
     
  objects = new_object_addresses();

     
                                                                           
                                                                            
                                                                           
                                                                         
                                     
     
                                                                           
                                                                      
                                                                    
                                                                          
                 
     
  forboth(oid_item, tab->changedConstraintOids, def_item, tab->changedConstraintDefs)
  {
    Oid oldId = lfirst_oid(oid_item);
    HeapTuple tup;
    Form_pg_constraint con;
    Oid relid;
    Oid confrelid;
    char contype;
    bool conislocal;

    tup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(oldId));
    if (!HeapTupleIsValid(tup))                        
    {
      elog(ERROR, "cache lookup failed for constraint %u", oldId);
    }
    con = (Form_pg_constraint)GETSTRUCT(tup);
    if (OidIsValid(con->conrelid))
    {
      relid = con->conrelid;
    }
    else
    {
                                       
      relid = get_typ_typrelid(getBaseType(con->contypid));
      if (!OidIsValid(relid))
      {
        elog(ERROR, "could not identify relation associated with constraint %u", oldId);
      }
    }
    confrelid = con->confrelid;
    contype = con->contype;
    conislocal = con->conislocal;
    ReleaseSysCache(tup);

    ObjectAddressSet(obj, ConstraintRelationId, oldId);
    add_exact_object_address(&obj, objects);

       
                                                                        
                                                                          
                                                                          
                                                                         
       
    if (!conislocal)
    {
      continue;
    }

       
                                                                        
                                                                           
                                                                        
                                                                
       
    if (relid != tab->relid && contype == CONSTRAINT_FOREIGN)
    {
      LockRelationOid(relid, AccessExclusiveLock);
    }

    ATPostAlterTypeParse(oldId, relid, confrelid, (char *)lfirst(def_item), wqueue, lockmode, tab->rewrite);
  }
  forboth(oid_item, tab->changedIndexOids, def_item, tab->changedIndexDefs)
  {
    Oid oldId = lfirst_oid(oid_item);
    Oid relid;

    relid = IndexGetRelation(oldId, false);
    ATPostAlterTypeParse(oldId, relid, InvalidOid, (char *)lfirst(def_item), wqueue, lockmode, tab->rewrite);

    ObjectAddressSet(obj, RelationRelationId, oldId);
    add_exact_object_address(&obj, objects);
  }

     
                                                                
     
  if (tab->replicaIdentityIndex)
  {
    AlterTableCmd *cmd = makeNode(AlterTableCmd);
    ReplicaIdentityStmt *subcmd = makeNode(ReplicaIdentityStmt);

    subcmd->identity_type = REPLICA_IDENTITY_INDEX;
    subcmd->name = tab->replicaIdentityIndex;
    cmd->subtype = AT_ReplicaIdentity;
    cmd->def = (Node *)subcmd;

                                             
    tab->subcmds[AT_PASS_OLD_CONSTR] = lappend(tab->subcmds[AT_PASS_OLD_CONSTR], cmd);
  }

     
                                                                    
     
  if (tab->clusterOnIndex)
  {
    AlterTableCmd *cmd = makeNode(AlterTableCmd);

    cmd->subtype = AT_ClusterOn;
    cmd->name = tab->clusterOnIndex;

                                             
    tab->subcmds[AT_PASS_OLD_CONSTR] = lappend(tab->subcmds[AT_PASS_OLD_CONSTR], cmd);
  }

     
                                                                            
                                    
     
  performMultipleDeletions(objects, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);

  free_object_addresses(objects);

     
                                                                           
            
     
}

   
                                                                          
                                                                       
                                               
   
                                                                         
                                                          
   
static void
ATPostAlterTypeParse(Oid oldId, Oid oldRelId, Oid refRelId, char *cmd, List **wqueue, LOCKMODE lockmode, bool rewrite)
{
  List *raw_parsetree_list;
  List *querytree_list;
  ListCell *list_item;
  Relation rel;

     
                                                                  
                                                              
                                                                       
                                                               
     
  raw_parsetree_list = raw_parser(cmd);
  querytree_list = NIL;
  foreach (list_item, raw_parsetree_list)
  {
    RawStmt *rs = lfirst_node(RawStmt, list_item);
    Node *stmt = rs->stmt;

    if (IsA(stmt, IndexStmt))
    {
      querytree_list = lappend(querytree_list, transformIndexStmt(oldRelId, (IndexStmt *)stmt, cmd));
    }
    else if (IsA(stmt, AlterTableStmt))
    {
      querytree_list = list_concat(querytree_list, transformAlterTableStmt(oldRelId, (AlterTableStmt *)stmt, cmd));
    }
    else
    {
      querytree_list = lappend(querytree_list, stmt);
    }
  }

                                                                  
  rel = relation_open(oldRelId, NoLock);

     
                                                                          
                                                                            
     
                                                                            
                                                                      
                                        
     
  foreach (list_item, querytree_list)
  {
    Node *stm = (Node *)lfirst(list_item);
    AlteredTableInfo *tab;

    tab = ATGetQueueEntry(wqueue, rel);

    if (IsA(stm, IndexStmt))
    {
      IndexStmt *stmt = (IndexStmt *)stm;
      AlterTableCmd *newcmd;

      if (!rewrite)
      {
        TryReuseIndex(oldId, stmt);
      }
      stmt->reset_default_tblspc = true;
                                    
      stmt->idxcomment = GetComment(oldId, RelationRelationId, 0);

      newcmd = makeNode(AlterTableCmd);
      newcmd->subtype = AT_ReAddIndex;
      newcmd->def = (Node *)stmt;
      tab->subcmds[AT_PASS_OLD_INDEX] = lappend(tab->subcmds[AT_PASS_OLD_INDEX], newcmd);
    }
    else if (IsA(stm, AlterTableStmt))
    {
      AlterTableStmt *stmt = (AlterTableStmt *)stm;
      ListCell *lcmd;

      foreach (lcmd, stmt->cmds)
      {
        AlterTableCmd *cmd = castNode(AlterTableCmd, lfirst(lcmd));

        if (cmd->subtype == AT_AddIndex)
        {
          IndexStmt *indstmt;
          Oid indoid;

          indstmt = castNode(IndexStmt, cmd->def);
          indoid = get_constraint_index(oldId);

          if (!rewrite)
          {
            TryReuseIndex(indoid, indstmt);
          }
                                             
          indstmt->idxcomment = GetComment(indoid, RelationRelationId, 0);
          indstmt->reset_default_tblspc = true;

          cmd->subtype = AT_ReAddIndex;
          tab->subcmds[AT_PASS_OLD_INDEX] = lappend(tab->subcmds[AT_PASS_OLD_INDEX], cmd);

                                                      
          RebuildConstraintComment(tab, AT_PASS_OLD_INDEX, oldId, rel, NIL, indstmt->idxname);
        }
        else if (cmd->subtype == AT_AddConstraint)
        {
          Constraint *con = castNode(Constraint, cmd->def);

          con->old_pktable_oid = refRelId;
                                              
          if (con->contype == CONSTR_FOREIGN && !rewrite && tab->rewrite == 0)
          {
            TryReuseForeignKey(oldId, con);
          }
          con->reset_default_tblspc = true;
          cmd->subtype = AT_ReAddConstraint;
          tab->subcmds[AT_PASS_OLD_CONSTR] = lappend(tab->subcmds[AT_PASS_OLD_CONSTR], cmd);

                                                      
          RebuildConstraintComment(tab, AT_PASS_OLD_CONSTR, oldId, rel, NIL, con->conname);
        }
        else if (cmd->subtype == AT_SetNotNull)
        {
             
                                                                  
                                                                     
                                                                  
                                                                   
                                       
             
        }
        else
        {
          elog(ERROR, "unexpected statement subtype: %d", (int)cmd->subtype);
        }
      }
    }
    else if (IsA(stm, AlterDomainStmt))
    {
      AlterDomainStmt *stmt = (AlterDomainStmt *)stm;

      if (stmt->subtype == 'C')                     
      {
        Constraint *con = castNode(Constraint, stmt->def);
        AlterTableCmd *cmd = makeNode(AlterTableCmd);

        cmd->subtype = AT_ReAddDomainConstraint;
        cmd->def = (Node *)stmt;
        tab->subcmds[AT_PASS_OLD_CONSTR] = lappend(tab->subcmds[AT_PASS_OLD_CONSTR], cmd);

                                                    
        RebuildConstraintComment(tab, AT_PASS_OLD_CONSTR, oldId, NULL, stmt->typeName, con->conname);
      }
      else
      {
        elog(ERROR, "unexpected statement subtype: %d", (int)stmt->subtype);
      }
    }
    else
    {
      elog(ERROR, "unexpected statement type: %d", (int)nodeTag(stm));
    }
  }

  relation_close(rel, NoLock);
}

   
                                                                          
                                                           
   
                                       
                                                                            
                                              
                                                                             
                                                                     
   
static void
RebuildConstraintComment(AlteredTableInfo *tab, int pass, Oid objid, Relation rel, List *domname, const char *conname)
{
  CommentStmt *cmd;
  char *comment_str;
  AlterTableCmd *newcmd;

                                                             
  comment_str = GetComment(objid, ConstraintRelationId, 0);
  if (comment_str == NULL)
  {
    return;
  }

                                                                 
  cmd = makeNode(CommentStmt);
  if (rel)
  {
    cmd->objtype = OBJECT_TABCONSTRAINT;
    cmd->object = (Node *)list_make3(makeString(get_namespace_name(RelationGetNamespace(rel))), makeString(pstrdup(RelationGetRelationName(rel))), makeString(pstrdup(conname)));
  }
  else
  {
    cmd->objtype = OBJECT_DOMCONSTRAINT;
    cmd->object = (Node *)list_make2(makeTypeNameFromNameList(copyObject(domname)), makeString(pstrdup(conname)));
  }
  cmd->comment = comment_str;

                                     
  newcmd = makeNode(AlterTableCmd);
  newcmd->subtype = AT_ReAddComment;
  newcmd->def = (Node *)cmd;
  tab->subcmds[pass] = lappend(tab->subcmds[pass], newcmd);
}

   
                                                                               
                                                                            
   
static void
TryReuseIndex(Oid oldId, IndexStmt *stmt)
{
  if (CheckIndexCompatible(oldId, stmt->accessMethod, stmt->indexParams, stmt->excludeOpNames))
  {
    Relation irel = index_open(oldId, NoLock);

                                                                    
    if (irel->rd_rel->relkind != RELKIND_PARTITIONED_INDEX)
    {
      stmt->oldNode = irel->rd_node.relNode;
    }
    index_close(irel, NoLock);
  }
}

   
                                          
   
                                                                              
                                                                             
                                   
   
static void
TryReuseForeignKey(Oid oldId, Constraint *con)
{
  HeapTuple tup;
  Datum adatum;
  bool isNull;
  ArrayType *arr;
  Oid *rawarr;
  int numkeys;
  int i;

  Assert(con->contype == CONSTR_FOREIGN);
  Assert(con->old_conpfeqop == NIL);                                 

  tup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(oldId));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for constraint %u", oldId);
  }

  adatum = SysCacheGetAttr(CONSTROID, tup, Anum_pg_constraint_conpfeqop, &isNull);
  if (isNull)
  {
    elog(ERROR, "null conpfeqop for constraint %u", oldId);
  }
  arr = DatumGetArrayTypeP(adatum);                         
  numkeys = ARR_DIMS(arr)[0];
                                                        
  if (ARR_NDIM(arr) != 1 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
  {
    elog(ERROR, "conpfeqop is not a 1-D Oid array");
  }
  rawarr = (Oid *)ARR_DATA_PTR(arr);

                                                                
  for (i = 0; i < numkeys; i++)
  {
    con->old_conpfeqop = lappend_oid(con->old_conpfeqop, rawarr[i]);
  }

  ReleaseSysCache(tup);
}

   
                                   
   
                                              
   
static ObjectAddress
ATExecAlterColumnGenericOptions(Relation rel, const char *colName, List *options, LOCKMODE lockmode)
{
  Relation ftrel;
  Relation attrel;
  ForeignServer *server;
  ForeignDataWrapper *fdw;
  HeapTuple tuple;
  HeapTuple newtuple;
  bool isnull;
  Datum repl_val[Natts_pg_attribute];
  bool repl_null[Natts_pg_attribute];
  bool repl_repl[Natts_pg_attribute];
  Datum datum;
  Form_pg_foreign_table fttableform;
  Form_pg_attribute atttableform;
  AttrNumber attnum;
  ObjectAddress address;

  if (options == NIL)
  {
    return InvalidObjectAddress;
  }

                                                                       
  ftrel = table_open(ForeignTableRelationId, AccessShareLock);
  tuple = SearchSysCache1(FOREIGNTABLEREL, rel->rd_id);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign table \"%s\" does not exist", RelationGetRelationName(rel))));
  }
  fttableform = (Form_pg_foreign_table)GETSTRUCT(tuple);
  server = GetForeignServer(fttableform->ftserver);
  fdw = GetForeignDataWrapper(server->fdwid);

  table_close(ftrel, AccessShareLock);
  ReleaseSysCache(tuple);

  attrel = table_open(AttributeRelationId, RowExclusiveLock);
  tuple = SearchSysCacheAttName(RelationGetRelid(rel), colName);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", colName, RelationGetRelationName(rel))));
  }

                                                     
  atttableform = (Form_pg_attribute)GETSTRUCT(tuple);
  attnum = atttableform->attnum;
  if (attnum <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot alter system column \"%s\"", colName)));
  }

                                               
  memset(repl_val, 0, sizeof(repl_val));
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));

                                   
  datum = SysCacheGetAttr(ATTNAME, tuple, Anum_pg_attribute_attfdwoptions, &isnull);
  if (isnull)
  {
    datum = PointerGetDatum(NULL);
  }

                             
  datum = transformGenericOptions(AttributeRelationId, datum, options, fdw->fdwvalidator);

  if (PointerIsValid(DatumGetPointer(datum)))
  {
    repl_val[Anum_pg_attribute_attfdwoptions - 1] = datum;
  }
  else
  {
    repl_null[Anum_pg_attribute_attfdwoptions - 1] = true;
  }

  repl_repl[Anum_pg_attribute_attfdwoptions - 1] = true;

                                                

  newtuple = heap_modify_tuple(tuple, RelationGetDescr(attrel), repl_val, repl_null, repl_repl);

  CatalogTupleUpdate(attrel, &newtuple->t_self, newtuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), atttableform->attnum);
  ObjectAddressSubSet(address, RelationRelationId, RelationGetRelid(rel), attnum);

  ReleaseSysCache(tuple);

  table_close(attrel, RowExclusiveLock);

  heap_freetuple(newtuple);

  return address;
}

   
                     
   
                                                                      
                                                                               
                                                                              
                                                                         
                                  
   
                                                                        
                                 
   
void
ATExecChangeOwner(Oid relationOid, Oid newOwnerId, bool recursing, LOCKMODE lockmode)
{
  Relation target_rel;
  Relation class_rel;
  HeapTuple tuple;
  Form_pg_class tuple_class;

     
                                                                         
                                                                 
     
  target_rel = relation_open(relationOid, lockmode);

                                   
  class_rel = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relationOid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relationOid);
  }
  tuple_class = (Form_pg_class)GETSTRUCT(tuple);

                                                  
  switch (tuple_class->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_VIEW:
  case RELKIND_MATVIEW:
  case RELKIND_FOREIGN_TABLE:
  case RELKIND_PARTITIONED_TABLE:
                            
    break;
  case RELKIND_INDEX:
    if (!recursing)
    {
         
                                                                   
                                                                    
                                                                  
                                                                  
                                                                
         
      if (tuple_class->relowner != newOwnerId)
      {
        ereport(WARNING, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change owner of index \"%s\"", NameStr(tuple_class->relname)), errhint("Change the ownership of the index's table, instead.")));
      }
                                                 
      newOwnerId = tuple_class->relowner;
    }
    break;
  case RELKIND_PARTITIONED_INDEX:
    if (recursing)
    {
      break;
    }
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change owner of index \"%s\"", NameStr(tuple_class->relname)), errhint("Change the ownership of the index's table, instead.")));
    break;
  case RELKIND_SEQUENCE:
    if (!recursing && tuple_class->relowner != newOwnerId)
    {
                                                                     
      Oid tableId;
      int32 colId;

      if (sequenceIsOwned(relationOid, DEPENDENCY_AUTO, &tableId, &colId) || sequenceIsOwned(relationOid, DEPENDENCY_INTERNAL, &tableId, &colId))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot change owner of sequence \"%s\"", NameStr(tuple_class->relname)), errdetail("Sequence \"%s\" is linked to table \"%s\".", NameStr(tuple_class->relname), get_rel_name(tableId))));
      }
    }
    break;
  case RELKIND_COMPOSITE_TYPE:
    if (recursing)
    {
      break;
    }
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a composite type", NameStr(tuple_class->relname)), errhint("Use ALTER TYPE instead.")));
    break;
  case RELKIND_TOASTVALUE:
    if (recursing)
    {
      break;
    }
                   
  default:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, view, sequence, or foreign table", NameStr(tuple_class->relname))));
  }

     
                                                                      
                                                                        
     
  if (tuple_class->relowner != newOwnerId)
  {
    Datum repl_val[Natts_pg_class];
    bool repl_null[Natts_pg_class];
    bool repl_repl[Natts_pg_class];
    Acl *newAcl;
    Datum aclDatum;
    bool isNull;
    HeapTuple newtuple;

                                                                       
    if (!recursing)
    {
                                       
      if (!superuser())
      {
        Oid namespaceOid = tuple_class->relnamespace;
        AclResult aclresult;

                                                             
        if (!pg_class_ownercheck(relationOid, GetUserId()))
        {
          aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relationOid)), RelationGetRelationName(target_rel));
        }

                                              
        check_is_member_of_role(GetUserId(), newOwnerId);

                                                               
        aclresult = pg_namespace_aclcheck(namespaceOid, newOwnerId, ACL_CREATE);
        if (aclresult != ACLCHECK_OK)
        {
          aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceOid));
        }
      }
    }

    memset(repl_null, false, sizeof(repl_null));
    memset(repl_repl, false, sizeof(repl_repl));

    repl_repl[Anum_pg_class_relowner - 1] = true;
    repl_val[Anum_pg_class_relowner - 1] = ObjectIdGetDatum(newOwnerId);

       
                                                                   
                                           
       
    aclDatum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relacl, &isNull);
    if (!isNull)
    {
      newAcl = aclnewowner(DatumGetAclP(aclDatum), tuple_class->relowner, newOwnerId);
      repl_repl[Anum_pg_class_relacl - 1] = true;
      repl_val[Anum_pg_class_relacl - 1] = PointerGetDatum(newAcl);
    }

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(class_rel), repl_val, repl_null, repl_repl);

    CatalogTupleUpdate(class_rel, &newtuple->t_self, newtuple);

    heap_freetuple(newtuple);

       
                                                                       
                                                                     
       
    change_owner_fix_column_acls(relationOid, tuple_class->relowner, newOwnerId);

       
                                                                        
                                                                         
                                                                     
       
    if (tuple_class->relkind != RELKIND_COMPOSITE_TYPE && tuple_class->relkind != RELKIND_INDEX && tuple_class->relkind != RELKIND_PARTITIONED_INDEX && tuple_class->relkind != RELKIND_TOASTVALUE)
    {
      changeDependencyOnOwner(RelationRelationId, relationOid, newOwnerId);
    }

       
                                                                        
       
    if (tuple_class->relkind != RELKIND_INDEX && tuple_class->relkind != RELKIND_PARTITIONED_INDEX)
    {
      AlterTypeOwnerInternal(tuple_class->reltype, newOwnerId);
    }

       
                                                                        
                                                                     
                                                             
       
    if (tuple_class->relkind == RELKIND_RELATION || tuple_class->relkind == RELKIND_PARTITIONED_TABLE || tuple_class->relkind == RELKIND_MATVIEW || tuple_class->relkind == RELKIND_TOASTVALUE)
    {
      List *index_oid_list;
      ListCell *i;

                                                           
      index_oid_list = RelationGetIndexList(target_rel);

                                                            
      foreach (i, index_oid_list)
      {
        ATExecChangeOwner(lfirst_oid(i), newOwnerId, true, lockmode);
      }

      list_free(index_oid_list);
    }

                                                                  
    if (tuple_class->reltoastrelid != InvalidOid)
    {
      ATExecChangeOwner(tuple_class->reltoastrelid, newOwnerId, true, lockmode);
    }

                                                                   
    change_owner_recurse_to_sequences(relationOid, newOwnerId, lockmode);
  }

  InvokeObjectPostAlterHook(RelationRelationId, relationOid, 0);

  ReleaseSysCache(tuple);
  table_close(class_rel, RowExclusiveLock);
  relation_close(target_rel, NoLock);
}

   
                                
   
                                                                         
                                                              
   
static void
change_owner_fix_column_acls(Oid relationOid, Oid oldOwnerId, Oid newOwnerId)
{
  Relation attRelation;
  SysScanDesc scan;
  ScanKeyData key[1];
  HeapTuple attributeTuple;

  attRelation = table_open(AttributeRelationId, RowExclusiveLock);
  ScanKeyInit(&key[0], Anum_pg_attribute_attrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relationOid));
  scan = systable_beginscan(attRelation, AttributeRelidNumIndexId, true, NULL, 1, key);
  while (HeapTupleIsValid(attributeTuple = systable_getnext(scan)))
  {
    Form_pg_attribute att = (Form_pg_attribute)GETSTRUCT(attributeTuple);
    Datum repl_val[Natts_pg_attribute];
    bool repl_null[Natts_pg_attribute];
    bool repl_repl[Natts_pg_attribute];
    Acl *newAcl;
    Datum aclDatum;
    bool isNull;
    HeapTuple newtuple;

                                
    if (att->attisdropped)
    {
      continue;
    }

    aclDatum = heap_getattr(attributeTuple, Anum_pg_attribute_attacl, RelationGetDescr(attRelation), &isNull);
                                          
    if (isNull)
    {
      continue;
    }

    memset(repl_null, false, sizeof(repl_null));
    memset(repl_repl, false, sizeof(repl_repl));

    newAcl = aclnewowner(DatumGetAclP(aclDatum), oldOwnerId, newOwnerId);
    repl_repl[Anum_pg_attribute_attacl - 1] = true;
    repl_val[Anum_pg_attribute_attacl - 1] = PointerGetDatum(newAcl);

    newtuple = heap_modify_tuple(attributeTuple, RelationGetDescr(attRelation), repl_val, repl_null, repl_repl);

    CatalogTupleUpdate(attRelation, &newtuple->t_self, newtuple);

    heap_freetuple(newtuple);
  }
  systable_endscan(scan);
  table_close(attRelation, RowExclusiveLock);
}

   
                                     
   
                                                                        
                                                                         
              
   
static void
change_owner_recurse_to_sequences(Oid relationOid, Oid newOwnerId, LOCKMODE lockmode)
{
  Relation depRel;
  SysScanDesc scan;
  ScanKeyData key[2];
  HeapTuple tup;

     
                                                                        
                                                              
     
  depRel = table_open(DependRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_depend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relationOid));
                                        

  scan = systable_beginscan(depRel, DependReferenceIndexId, true, NULL, 2, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_depend depForm = (Form_pg_depend)GETSTRUCT(tup);
    Relation seqRel;

                                                                   
    if (depForm->refobjsubid == 0 || depForm->classid != RelationRelationId || depForm->objsubid != 0 || !(depForm->deptype == DEPENDENCY_AUTO || depForm->deptype == DEPENDENCY_INTERNAL))
    {
      continue;
    }

                                                      
    seqRel = relation_open(depForm->objid, lockmode);

                                     
    if (RelationGetForm(seqRel)->relkind != RELKIND_SEQUENCE)
    {
                                    
      relation_close(seqRel, lockmode);
      continue;
    }

                                                                
    ATExecChangeOwner(depForm->objid, newOwnerId, true, lockmode);

                                                                      
    relation_close(seqRel, NoLock);
  }

  systable_endscan(scan);

  relation_close(depRel, AccessShareLock);
}

   
                          
   
                                                                      
   
                                                   
   
static ObjectAddress
ATExecClusterOn(Relation rel, const char *indexName, LOCKMODE lockmode)
{
  Oid indexOid;
  ObjectAddress address;

  indexOid = get_relname_relid(indexName, rel->rd_rel->relnamespace);

  if (!OidIsValid(indexOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("index \"%s\" for table \"%s\" does not exist", indexName, RelationGetRelationName(rel))));
  }

                                          
  check_index_is_clusterable(rel, indexOid, false, lockmode);

                       
  mark_index_clustered(rel, indexOid, false);

  ObjectAddressSet(address, RelationRelationId, indexOid);

  return address;
}

   
                                   
   
                                                                         
                        
   
static void
ATExecDropCluster(Relation rel, LOCKMODE lockmode)
{
  mark_index_clustered(rel, InvalidOid, false);
}

   
                              
   
static void
ATPrepSetTableSpace(AlteredTableInfo *tab, Relation rel, const char *tablespacename, LOCKMODE lockmode)
{
  Oid tablespaceId;

                                        
  tablespaceId = get_tablespace_oid(tablespacename, false);

                                                                  
  if (OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
  {
    AclResult aclresult;

    aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_TABLESPACE, tablespacename);
    }
  }

                                                 
  if (OidIsValid(tab->newTableSpace))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot have multiple SET TABLESPACE subcommands")));
  }

  tab->newTableSpace = tablespaceId;
}

   
                                      
   
static void
ATExecSetRelOptions(Relation rel, List *defList, AlterTableType operation, LOCKMODE lockmode)
{
  Oid relid;
  Relation pgclass;
  HeapTuple tuple;
  HeapTuple newtuple;
  Datum datum;
  bool isnull;
  Datum newOptions;
  Datum repl_val[Natts_pg_class];
  bool repl_null[Natts_pg_class];
  bool repl_repl[Natts_pg_class];
  static char *validnsps[] = HEAP_RELOPT_NAMESPACES;

  if (defList == NIL && operation != AT_ReplaceRelOptions)
  {
    return;                    
  }

  pgclass = table_open(RelationRelationId, RowExclusiveLock);

                        
  relid = RelationGetRelid(rel);
  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }

  if (operation == AT_ReplaceRelOptions)
  {
       
                                                                         
                               
       
    datum = (Datum)0;
    isnull = true;
  }
  else
  {
                                
    datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions, &isnull);
  }

                                                     
  newOptions = transformRelOptions(isnull ? (Datum)0 : datum, defList, NULL, validnsps, false, operation == AT_ResetRelOptions);

                
  switch (rel->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_TOASTVALUE:
  case RELKIND_MATVIEW:
  case RELKIND_PARTITIONED_TABLE:
    (void)heap_reloptions(rel->rd_rel->relkind, newOptions, true);
    break;
  case RELKIND_VIEW:
    (void)view_reloptions(newOptions, true);
    break;
  case RELKIND_INDEX:
  case RELKIND_PARTITIONED_INDEX:
    (void)index_reloptions(rel->rd_indam->amoptions, newOptions, true);
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, view, materialized view, index, or TOAST table", RelationGetRelationName(rel))));
    break;
  }

                                               
  if (rel->rd_rel->relkind == RELKIND_VIEW)
  {
    Query *view_query = get_view_query(rel);
    List *view_options = untransformRelOptions(newOptions);
    ListCell *cell;
    bool check_option = false;

    foreach (cell, view_options)
    {
      DefElem *defel = (DefElem *)lfirst(cell);

      if (strcmp(defel->defname, "check_option") == 0)
      {
        check_option = true;
      }
    }

       
                                                                    
                                       
       
    if (check_option)
    {
      const char *view_updatable_error = view_query_is_auto_updatable(view_query, true);

      if (view_updatable_error)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("WITH CHECK OPTION is supported only on automatically updatable views"), errhint("%s", _(view_updatable_error))));
      }
    }
  }

     
                                                                             
                                                               
     
  memset(repl_val, 0, sizeof(repl_val));
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));

  if (newOptions != (Datum)0)
  {
    repl_val[Anum_pg_class_reloptions - 1] = newOptions;
  }
  else
  {
    repl_null[Anum_pg_class_reloptions - 1] = true;
  }

  repl_repl[Anum_pg_class_reloptions - 1] = true;

  newtuple = heap_modify_tuple(tuple, RelationGetDescr(pgclass), repl_val, repl_null, repl_repl);

  CatalogTupleUpdate(pgclass, &newtuple->t_self, newtuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), 0);

  heap_freetuple(newtuple);

  ReleaseSysCache(tuple);

                                                                     
  if (OidIsValid(rel->rd_rel->reltoastrelid))
  {
    Relation toastrel;
    Oid toastid = rel->rd_rel->reltoastrelid;

    toastrel = table_open(toastid, lockmode);

                          
    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(toastid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", toastid);
    }

    if (operation == AT_ReplaceRelOptions)
    {
         
                                                                   
                                         
         
      datum = (Datum)0;
      isnull = true;
    }
    else
    {
                                  
      datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions, &isnull);
    }

    newOptions = transformRelOptions(isnull ? (Datum)0 : datum, defList, "toast", validnsps, false, operation == AT_ResetRelOptions);

    (void)heap_reloptions(RELKIND_TOASTVALUE, newOptions, true);

    memset(repl_val, 0, sizeof(repl_val));
    memset(repl_null, false, sizeof(repl_null));
    memset(repl_repl, false, sizeof(repl_repl));

    if (newOptions != (Datum)0)
    {
      repl_val[Anum_pg_class_reloptions - 1] = newOptions;
    }
    else
    {
      repl_null[Anum_pg_class_reloptions - 1] = true;
    }

    repl_repl[Anum_pg_class_reloptions - 1] = true;

    newtuple = heap_modify_tuple(tuple, RelationGetDescr(pgclass), repl_val, repl_null, repl_repl);

    CatalogTupleUpdate(pgclass, &newtuple->t_self, newtuple);

    InvokeObjectPostAlterHookArg(RelationRelationId, RelationGetRelid(toastrel), 0, InvalidOid, true);

    heap_freetuple(newtuple);

    ReleaseSysCache(tuple);

    table_close(toastrel, NoLock);
  }

  table_close(pgclass, RowExclusiveLock);
}

   
                                                                        
                                                                               
   
static void
ATExecSetTableSpace(Oid tableOid, Oid newTableSpace, LOCKMODE lockmode)
{
  Relation rel;
  Oid oldTableSpace;
  Oid reltoastrelid;
  Oid newrelfilenode;
  RelFileNode newrnode;
  Relation pg_class;
  HeapTuple tuple;
  Form_pg_class rd_rel;
  List *reltoastidxids = NIL;
  ListCell *lc;

     
                                                                     
     
  rel = relation_open(tableOid, lockmode);

     
                                         
     
  oldTableSpace = rel->rd_rel->reltablespace;
  if (newTableSpace == oldTableSpace || (newTableSpace == MyDatabaseTableSpace && oldTableSpace == 0))
  {
    InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), 0);

    relation_close(rel, NoLock);
    return;
  }

     
                                                                           
                                                          
     
  if (RelationIsMapped(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot move system relation \"%s\"", RelationGetRelationName(rel))));
  }

                                                       
  if (newTableSpace == GLOBALTABLESPACE_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("only shared relations can be placed in pg_global tablespace")));
  }

     
                                                                             
                                   
     
  if (RELATION_IS_OTHER_TEMP(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot move temporary tables of other sessions")));
  }

  reltoastrelid = rel->rd_rel->reltoastrelid;
                                                                
  if (OidIsValid(reltoastrelid))
  {
    Relation toastRel = relation_open(reltoastrelid, lockmode);

    reltoastidxids = RelationGetIndexList(toastRel);
    relation_close(toastRel, lockmode);
  }

                                                            
  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(tableOid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", tableOid);
  }
  rd_rel = (Form_pg_class)GETSTRUCT(tuple);

     
                                                                             
                                                  
     
  newrelfilenode = GetNewRelFileNode(newTableSpace, NULL, rel->rd_rel->relpersistence);

                                 
  newrnode = rel->rd_node;
  newrnode.relNode = newrelfilenode;
  newrnode.spcNode = newTableSpace;

                                                                            
  if (rel->rd_rel->relkind == RELKIND_INDEX)
  {
    index_copy_data(rel, newrnode);
  }
  else
  {
    Assert(rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_MATVIEW || rel->rd_rel->relkind == RELKIND_TOASTVALUE);
    table_relation_copy_data(rel, &newrnode);
  }

     
                              
     
                                                                        
                                                                          
                                                              
     
  rd_rel->reltablespace = (newTableSpace == MyDatabaseTableSpace) ? InvalidOid : newTableSpace;
  rd_rel->relfilenode = newrelfilenode;
  CatalogTupleUpdate(pg_class, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(RelationRelationId, RelationGetRelid(rel), 0);

  heap_freetuple(tuple);

  table_close(pg_class, RowExclusiveLock);

  relation_close(rel, NoLock);

                                                     
  CommandCounterIncrement();

                                                          
  if (OidIsValid(reltoastrelid))
  {
    ATExecSetTableSpace(reltoastrelid, newTableSpace, lockmode);
  }
  foreach (lc, reltoastidxids)
  {
    ATExecSetTableSpace(lfirst_oid(lc), newTableSpace, lockmode);
  }

                
  list_free(reltoastidxids);
}

   
                                                                        
                                                           
   
                                                                           
                                                     
   
static void
ATExecSetTableSpaceNoStorage(Relation rel, Oid newTableSpace)
{
  HeapTuple tuple;
  Oid oldTableSpace;
  Relation pg_class;
  Form_pg_class rd_rel;
  Oid reloid = RelationGetRelid(rel);

     
                                                                             
              
     
  Assert(!RELKIND_HAS_STORAGE(rel->rd_rel->relkind));

                                                      
  if (newTableSpace == GLOBALTABLESPACE_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("only shared relations can be placed in pg_global tablespace")));
  }

     
                                         
     
  oldTableSpace = rel->rd_rel->reltablespace;
  if (newTableSpace == oldTableSpace || (newTableSpace == MyDatabaseTableSpace && oldTableSpace == 0))
  {
    InvokeObjectPostAlterHook(RelationRelationId, reloid, 0);
    return;
  }

                                                            
  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(reloid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", reloid);
  }
  rd_rel = (Form_pg_class)GETSTRUCT(tuple);

                               
  rd_rel->reltablespace = (newTableSpace == MyDatabaseTableSpace) ? InvalidOid : newTableSpace;
  CatalogTupleUpdate(pg_class, &tuple->t_self, tuple);

                                       
  changeDependencyOnTablespace(RelationRelationId, reloid, rd_rel->reltablespace);

  InvokeObjectPostAlterHook(RelationRelationId, reloid, 0);

  heap_freetuple(tuple);

  table_close(pg_class, RowExclusiveLock);

                                                     
  CommandCounterIncrement();
}

   
                                      
   
                                                                               
                                                                               
                                                                        
                                                                                
                                                                        
   
                                                                            
                                                  
   
Oid
AlterTableMoveAll(AlterTableMoveAllStmt *stmt)
{
  List *relations = NIL;
  ListCell *l;
  ScanKeyData key[1];
  Relation rel;
  TableScanDesc scan;
  HeapTuple tuple;
  Oid orig_tablespaceoid;
  Oid new_tablespaceoid;
  List *role_oids = roleSpecsToIds(stmt->roles);

                                                           
  if (stmt->objtype != OBJECT_TABLE && stmt->objtype != OBJECT_INDEX && stmt->objtype != OBJECT_MATVIEW)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("only tables, indexes, and materialized views exist in tablespaces")));
  }

                                            
  orig_tablespaceoid = get_tablespace_oid(stmt->orig_tablespacename, false);
  new_tablespaceoid = get_tablespace_oid(stmt->new_tablespacename, false);

                                                             
                                                                             
  if (orig_tablespaceoid == GLOBALTABLESPACE_OID || new_tablespaceoid == GLOBALTABLESPACE_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot move relations in to or out of pg_global tablespace")));
  }

     
                                                                     
                                                                         
                 
     
  if (OidIsValid(new_tablespaceoid) && new_tablespaceoid != MyDatabaseTableSpace)
  {
    AclResult aclresult;

    aclresult = pg_tablespace_aclcheck(new_tablespaceoid, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_TABLESPACE, get_tablespace_name(new_tablespaceoid));
    }
  }

     
                                                                    
                                                                 
     
  if (orig_tablespaceoid == MyDatabaseTableSpace)
  {
    orig_tablespaceoid = InvalidOid;
  }

  if (new_tablespaceoid == MyDatabaseTableSpace)
  {
    new_tablespaceoid = InvalidOid;
  }

             
  if (orig_tablespaceoid == new_tablespaceoid)
  {
    return new_tablespaceoid;
  }

     
                                                                         
                                                   
     
  ScanKeyInit(&key[0], Anum_pg_class_reltablespace, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(orig_tablespaceoid));

  rel = table_open(RelationRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 1, key);
  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class relForm = (Form_pg_class)GETSTRUCT(tuple);
    Oid relOid = relForm->oid;

       
                                                                      
                                                                   
                          
       
                                                                       
                                                  
       
    if (IsCatalogNamespace(relForm->relnamespace) || relForm->relisshared || isAnyTempNamespace(relForm->relnamespace) || IsToastNamespace(relForm->relnamespace))
    {
      continue;
    }

                                             
    if ((stmt->objtype == OBJECT_TABLE && relForm->relkind != RELKIND_RELATION && relForm->relkind != RELKIND_PARTITIONED_TABLE) || (stmt->objtype == OBJECT_INDEX && relForm->relkind != RELKIND_INDEX && relForm->relkind != RELKIND_PARTITIONED_INDEX) || (stmt->objtype == OBJECT_MATVIEW && relForm->relkind != RELKIND_MATVIEW))
    {
      continue;
    }

                                                                    
    if (role_oids != NIL && !list_member_oid(role_oids, relForm->relowner))
    {
      continue;
    }

       
                                                                        
                                                                           
                                                                      
       
                                                                   
       
    if (!pg_class_ownercheck(relOid, GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relOid)), NameStr(relForm->relname));
    }

    if (stmt->nowait && !ConditionalLockRelationOid(relOid, AccessExclusiveLock))
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_IN_USE), errmsg("aborting because lock on relation \"%s.%s\" is not available", get_namespace_name(relForm->relnamespace), NameStr(relForm->relname))));
    }
    else
    {
      LockRelationOid(relOid, AccessExclusiveLock);
    }

                                            
    relations = lappend_oid(relations, relOid);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  if (relations == NIL)
  {
    ereport(NOTICE, (errcode(ERRCODE_NO_DATA_FOUND), errmsg("no matching relations in tablespace \"%s\" found", orig_tablespaceoid == InvalidOid ? "(database default)" : get_tablespace_name(orig_tablespaceoid))));
  }

                                                                         
  foreach (l, relations)
  {
    List *cmds = NIL;
    AlterTableCmd *cmd = makeNode(AlterTableCmd);

    cmd->subtype = AT_SetTableSpace;
    cmd->name = stmt->new_tablespacename;

    cmds = lappend(cmds, cmd);

    EventTriggerAlterTableStart((Node *)stmt);
                                          
    AlterTableInternal(lfirst_oid(l), cmds, false);
    EventTriggerAlterTableEnd();
  }

  return new_tablespaceoid;
}

static void
index_copy_data(Relation rel, RelFileNode newrnode)
{
  SMgrRelation dstrel;

  dstrel = smgropen(newrnode, rel->rd_backend);

     
                                                                            
                                                                           
                                                                            
                                        
     
  FlushRelationBuffers(rel);

     
                                                                          
                         
     
                                                               
                              
     
  RelationCreateStorage(newrnode, rel->rd_rel->relpersistence);

                      
  RelationCopyStorage(RelationGetSmgr(rel), dstrel, MAIN_FORKNUM, rel->rd_rel->relpersistence);

                                         
  for (ForkNumber forkNum = MAIN_FORKNUM + 1; forkNum <= MAX_FORKNUM; forkNum++)
  {
    if (smgrexists(RelationGetSmgr(rel), forkNum))
    {
      smgrcreate(dstrel, forkNum, false);

         
                                                                        
                                            
         
      if (rel->rd_rel->relpersistence == RELPERSISTENCE_PERMANENT || (rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED && forkNum == INIT_FORKNUM))
      {
        log_smgrcreate(&newrnode, forkNum);
      }
      RelationCopyStorage(RelationGetSmgr(rel), dstrel, forkNum, rel->rd_rel->relpersistence);
    }
  }

                                            
  RelationDropStorage(rel);
  smgrclose(dstrel);
}

   
                                      
   
                                       
   
static void
ATExecEnableDisableTrigger(Relation rel, const char *trigname, char fires_when, bool skip_system, bool recurse, LOCKMODE lockmode)
{
  EnableDisableTriggerNew(rel, trigname, fires_when, skip_system, recurse, lockmode);
}

   
                                   
   
                                             
   
static void
ATExecEnableDisableRule(Relation rel, const char *rulename, char fires_when, LOCKMODE lockmode)
{
  EnableDisableRule(rel, rulename, fires_when);
}

   
                       
   
                                                                               
                                                                              
                                    
   
static void
ATPrepAddInherit(Relation child_rel)
{
  if (child_rel->rd_rel->reloftype)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change inheritance of typed table")));
  }

  if (child_rel->rd_rel->relispartition)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change inheritance of a partition")));
  }

  if (child_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change inheritance of partitioned table")));
  }
}

   
                                                  
   
static ObjectAddress
ATExecAddInherit(Relation child_rel, RangeVar *parent, LOCKMODE lockmode)
{
  Relation parent_rel;
  List *children;
  ObjectAddress address;
  const char *trigger_name;

     
                                                                    
                                               
     
  parent_rel = table_openrv(parent, ShareUpdateExclusiveLock);

     
                                                                    
                                           
     
  ATSimplePermissions(parent_rel, ATT_TABLE | ATT_FOREIGN_TABLE);

                                                         
  if (parent_rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP && child_rel->rd_rel->relpersistence != RELPERSISTENCE_TEMP)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit from temporary relation \"%s\"", RelationGetRelationName(parent_rel))));
  }

                                                             
  if (parent_rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP && !parent_rel->rd_islocaltemp)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit from temporary relation of another session")));
  }

                           
  if (child_rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP && !child_rel->rd_islocaltemp)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit to temporary relation of another session")));
  }

                                                                    
  if (parent_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit from partitioned table \"%s\"", parent->relname)));
  }

                               
  if (parent_rel->rd_rel->relispartition)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot inherit from a partition")));
  }

     
                                                                           
                                                                       
     
                                                                       
                                                                       
                                                                            
                                                                          
                                                                           
                                                                         
                                         
     
                                                                             
     
  children = find_all_inheritors(RelationGetRelid(child_rel), AccessShareLock, NULL);

  if (list_member_oid(children, RelationGetRelid(parent_rel)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("circular inheritance not allowed"), errdetail("\"%s\" is already a child of \"%s\".", parent->relname, RelationGetRelationName(child_rel))));
  }

     
                                                                    
                                                                        
                                                                  
     
  trigger_name = FindTriggerIncompatibleWithInheritance(child_rel->trigdesc);
  if (trigger_name != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("trigger \"%s\" prevents table \"%s\" from becoming an inheritance child", trigger_name, RelationGetRelationName(child_rel)), errdetail("ROW triggers with transition tables are not supported in inheritance hierarchies.")));
  }

                                
  CreateInheritance(child_rel, parent_rel);

  ObjectAddressSet(address, RelationRelationId, RelationGetRelid(parent_rel));

                                                         
  table_close(parent_rel, NoLock);

  return address;
}

   
                     
                                                                         
                              
   
                                                             
   
static void
CreateInheritance(Relation child_rel, Relation parent_rel)
{
  Relation catalogRelation;
  SysScanDesc scan;
  ScanKeyData key;
  HeapTuple inheritsTuple;
  int32 inhseqno;

                                                                           
  catalogRelation = table_open(InheritsRelationId, RowExclusiveLock);

     
                                                                            
                                                                          
                                                                  
                 
     
                                                                           
                                                                          
     
  ScanKeyInit(&key, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(child_rel)));
  scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId, true, NULL, 1, &key);

                                     
  inhseqno = 0;
  while (HeapTupleIsValid(inheritsTuple = systable_getnext(scan)))
  {
    Form_pg_inherits inh = (Form_pg_inherits)GETSTRUCT(inheritsTuple);

    if (inh->inhparent == RelationGetRelid(parent_rel))
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" would be inherited from more than once", RelationGetRelationName(parent_rel))));
    }

    if (inh->inhseqno > inhseqno)
    {
      inhseqno = inh->inhseqno;
    }
  }
  systable_endscan(scan);

                                                           
  MergeAttributesIntoExisting(child_rel, parent_rel);

                                                               
  MergeConstraintsIntoExisting(child_rel, parent_rel);

     
                                                                          
     
  StoreCatalogInheritance1(RelationGetRelid(child_rel), RelationGetRelid(parent_rel), inhseqno + 1, catalogRelation, parent_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);

                                       
  table_close(catalogRelation, RowExclusiveLock);
}

   
                                                                        
                                             
   
static char *
decompile_conbin(HeapTuple contup, TupleDesc tupdesc)
{
  Form_pg_constraint con;
  bool isnull;
  Datum attr;
  Datum expr;

  con = (Form_pg_constraint)GETSTRUCT(contup);
  attr = heap_getattr(contup, Anum_pg_constraint_conbin, tupdesc, &isnull);
  if (isnull)
  {
    elog(ERROR, "null conbin for constraint %u", con->oid);
  }

  expr = DirectFunctionCall2(pg_get_expr, attr, ObjectIdGetDatum(con->conrelid));
  return TextDatumGetCString(expr);
}

   
                                                                       
   
                                                                        
                                                                         
                                                                        
   
static bool
constraints_equivalent(HeapTuple a, HeapTuple b, TupleDesc tupleDesc)
{
  Form_pg_constraint acon = (Form_pg_constraint)GETSTRUCT(a);
  Form_pg_constraint bcon = (Form_pg_constraint)GETSTRUCT(b);

  if (acon->condeferrable != bcon->condeferrable || acon->condeferred != bcon->condeferred || strcmp(decompile_conbin(a, tupleDesc), decompile_conbin(b, tupleDesc)) != 0)
  {
    return false;
  }
  else
  {
    return true;
  }
}

   
                                                                               
                      
   
                               
   
                                                                               
                                                                            
                                                                          
                                       
   
                                                                           
                                                                  
   
static void
MergeAttributesIntoExisting(Relation child_rel, Relation parent_rel)
{
  Relation attrrel;
  AttrNumber parent_attno;
  int parent_natts;
  TupleDesc tupleDesc;
  HeapTuple tuple;
  bool child_is_partition = false;

  attrrel = table_open(AttributeRelationId, RowExclusiveLock);

  tupleDesc = RelationGetDescr(parent_rel);
  parent_natts = tupleDesc->natts;

                                                                           
  if (parent_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    child_is_partition = true;
  }

  for (parent_attno = 1; parent_attno <= parent_natts; parent_attno++)
  {
    Form_pg_attribute attribute = TupleDescAttr(tupleDesc, parent_attno - 1);
    char *attributeName = NameStr(attribute->attname);

                                               
    if (attribute->attisdropped)
    {
      continue;
    }

                                                              
    tuple = SearchSysCacheCopyAttName(RelationGetRelid(child_rel), attributeName);
    if (HeapTupleIsValid(tuple))
    {
                                                           
      Form_pg_attribute childatt = (Form_pg_attribute)GETSTRUCT(tuple);

      if (attribute->atttypid != childatt->atttypid || attribute->atttypmod != childatt->atttypmod)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("child table \"%s\" has different type for column \"%s\"", RelationGetRelationName(child_rel), attributeName)));
      }

      if (attribute->attcollation != childatt->attcollation)
      {
        ereport(ERROR, (errcode(ERRCODE_COLLATION_MISMATCH), errmsg("child table \"%s\" has different collation for column \"%s\"", RelationGetRelationName(child_rel), attributeName)));
      }

         
                                                                
                                             
         
      if (attribute->attnotnull && !childatt->attnotnull)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("column \"%s\" in child table must be marked NOT NULL", attributeName)));
      }

         
                                                                   
         
      if (attribute->attgenerated && !childatt->attgenerated)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("column \"%s\" in child table must be a generated column", attributeName)));
      }

         
                                                       
         
                                                                         
                                                                         
                                                                        
                                                                
         
      if (attribute->attgenerated && childatt->attgenerated)
      {
        TupleConstr *child_constr = child_rel->rd_att->constr;
        TupleConstr *parent_constr = parent_rel->rd_att->constr;
        char *child_expr = NULL;
        char *parent_expr = NULL;

        Assert(child_constr != NULL);
        Assert(parent_constr != NULL);

        for (int i = 0; i < child_constr->num_defval; i++)
        {
          if (child_constr->defval[i].adnum == childatt->attnum)
          {
            child_expr = TextDatumGetCString(DirectFunctionCall2(pg_get_expr, CStringGetTextDatum(child_constr->defval[i].adbin), ObjectIdGetDatum(child_rel->rd_id)));
            break;
          }
        }
        Assert(child_expr != NULL);

        for (int i = 0; i < parent_constr->num_defval; i++)
        {
          if (parent_constr->defval[i].adnum == attribute->attnum)
          {
            parent_expr = TextDatumGetCString(DirectFunctionCall2(pg_get_expr, CStringGetTextDatum(parent_constr->defval[i].adbin), ObjectIdGetDatum(parent_rel->rd_id)));
            break;
          }
        }
        Assert(parent_expr != NULL);

        if (strcmp(child_expr, parent_expr) != 0)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("column \"%s\" in child table has a conflicting generation expression", attributeName)));
        }
      }

         
                                                                     
                                                     
         
      childatt->attinhcount++;

         
                                                                         
                                                                    
                                   
         
      if (child_is_partition)
      {
        Assert(childatt->attinhcount == 1);
        childatt->attislocal = false;
      }

      CatalogTupleUpdate(attrrel, &tuple->t_self, tuple);
      heap_freetuple(tuple);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("child table is missing column \"%s\"", attributeName)));
    }
  }

  table_close(attrrel, RowExclusiveLock);
}

   
                                                                         
                                    
   
                                                               
   
                               
   
                                                                                
                                                               
   
                                                                         
                                                                                
                                                                             
   
                                                                    
   
static void
MergeConstraintsIntoExisting(Relation child_rel, Relation parent_rel)
{
  Relation catalog_relation;
  TupleDesc tuple_desc;
  SysScanDesc parent_scan;
  ScanKeyData parent_key;
  HeapTuple parent_tuple;
  bool child_is_partition = false;

  catalog_relation = table_open(ConstraintRelationId, RowExclusiveLock);
  tuple_desc = RelationGetDescr(catalog_relation);

                                                                           
  if (parent_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    child_is_partition = true;
  }

                                                                    
  ScanKeyInit(&parent_key, Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(parent_rel)));
  parent_scan = systable_beginscan(catalog_relation, ConstraintRelidTypidNameIndexId, true, NULL, 1, &parent_key);

  while (HeapTupleIsValid(parent_tuple = systable_getnext(parent_scan)))
  {
    Form_pg_constraint parent_con = (Form_pg_constraint)GETSTRUCT(parent_tuple);
    SysScanDesc child_scan;
    ScanKeyData child_key;
    HeapTuple child_tuple;
    bool found = false;

    if (parent_con->contype != CONSTRAINT_CHECK)
    {
      continue;
    }

                                                                             
    if (parent_con->connoinherit)
    {
      continue;
    }

                                                         
    ScanKeyInit(&child_key, Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(child_rel)));
    child_scan = systable_beginscan(catalog_relation, ConstraintRelidTypidNameIndexId, true, NULL, 1, &child_key);

    while (HeapTupleIsValid(child_tuple = systable_getnext(child_scan)))
    {
      Form_pg_constraint child_con = (Form_pg_constraint)GETSTRUCT(child_tuple);
      HeapTuple child_copy;

      if (child_con->contype != CONSTRAINT_CHECK)
      {
        continue;
      }

      if (strcmp(NameStr(parent_con->conname), NameStr(child_con->conname)) != 0)
      {
        continue;
      }

      if (!constraints_equivalent(parent_tuple, child_tuple, tuple_desc))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("child table \"%s\" has different definition for check constraint \"%s\"", RelationGetRelationName(child_rel), NameStr(parent_con->conname))));
      }

                                                                     
      if (child_con->connoinherit)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("constraint \"%s\" conflicts with non-inherited constraint on child table \"%s\"", NameStr(child_con->conname), RelationGetRelationName(child_rel))));
      }

         
                                                                         
                                 
         
      if (parent_con->convalidated && !child_con->convalidated)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("constraint \"%s\" conflicts with NOT VALID constraint on child table \"%s\"", NameStr(child_con->conname), RelationGetRelationName(child_rel))));
      }

         
                                                                         
                                                     
         
      child_copy = heap_copytuple(child_tuple);
      child_con = (Form_pg_constraint)GETSTRUCT(child_copy);
      child_con->coninhcount++;

         
                                                                
                                                                       
                                       
         
      if (child_is_partition)
      {
        Assert(child_con->coninhcount == 1);
        child_con->conislocal = false;
      }

      CatalogTupleUpdate(catalog_relation, &child_copy->t_self, child_copy);
      heap_freetuple(child_copy);

      found = true;
      break;
    }

    systable_endscan(child_scan);

    if (!found)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("child table is missing constraint \"%s\"", NameStr(parent_con->conname))));
    }
  }

  systable_endscan(parent_scan);
  table_close(catalog_relation, RowExclusiveLock);
}

   
                          
   
                                                                         
   
static ObjectAddress
ATExecDropInherit(Relation rel, RangeVar *parent, LOCKMODE lockmode)
{
  ObjectAddress address;
  Relation parent_rel;

  if (rel->rd_rel->relispartition)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change inheritance of a partition")));
  }

     
                                                                        
                                                                             
                                        
     
  parent_rel = table_openrv(parent, AccessShareLock);

     
                                                                             
                                          
     

                                                                 
  RemoveInheritance(rel, parent_rel);

  ObjectAddressSet(address, RelationRelationId, RelationGetRelid(parent_rel));

                                                         
  table_close(parent_rel, NoLock);

  return address;
}

   
                     
   
                                                                             
                                                                          
            
   
                                                                              
                                                                           
                                                                         
                                                                           
                                                                      
   
                                                                        
                         
   
                                                              
   
static void
RemoveInheritance(Relation child_rel, Relation parent_rel)
{
  Relation catalogRelation;
  SysScanDesc scan;
  ScanKeyData key[3];
  HeapTuple attributeTuple, constraintTuple;
  List *connames;
  bool found;
  bool child_is_partition = false;

                                                                           
  if (parent_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    child_is_partition = true;
  }

  found = DeleteInheritsTuple(RelationGetRelid(child_rel), RelationGetRelid(parent_rel));
  if (!found)
  {
    if (child_is_partition)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s\" is not a partition of relation \"%s\"", RelationGetRelationName(child_rel), RelationGetRelationName(parent_rel))));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("relation \"%s\" is not a parent of relation \"%s\"", RelationGetRelationName(parent_rel), RelationGetRelationName(child_rel))));
    }
  }

     
                                                                       
     
  catalogRelation = table_open(AttributeRelationId, RowExclusiveLock);
  ScanKeyInit(&key[0], Anum_pg_attribute_attrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(child_rel)));
  scan = systable_beginscan(catalogRelation, AttributeRelidNumIndexId, true, NULL, 1, key);
  while (HeapTupleIsValid(attributeTuple = systable_getnext(scan)))
  {
    Form_pg_attribute att = (Form_pg_attribute)GETSTRUCT(attributeTuple);

                                            
    if (att->attisdropped)
    {
      continue;
    }
    if (att->attinhcount <= 0)
    {
      continue;
    }

    if (SearchSysCacheExistsAttName(RelationGetRelid(parent_rel), NameStr(att->attname)))
    {
                                                               
      HeapTuple copyTuple = heap_copytuple(attributeTuple);
      Form_pg_attribute copy_att = (Form_pg_attribute)GETSTRUCT(copyTuple);

      copy_att->attinhcount--;
      if (copy_att->attinhcount == 0)
      {
        copy_att->attislocal = true;
      }

      CatalogTupleUpdate(catalogRelation, &copyTuple->t_self, copyTuple);
      heap_freetuple(copyTuple);
    }
  }
  systable_endscan(scan);
  table_close(catalogRelation, RowExclusiveLock);

     
                                                                           
                                                                   
                                                                      
                                                
     
  catalogRelation = table_open(ConstraintRelationId, RowExclusiveLock);
  ScanKeyInit(&key[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(parent_rel)));
  scan = systable_beginscan(catalogRelation, ConstraintRelidTypidNameIndexId, true, NULL, 1, key);

  connames = NIL;

  while (HeapTupleIsValid(constraintTuple = systable_getnext(scan)))
  {
    Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(constraintTuple);

    if (con->contype == CONSTRAINT_CHECK)
    {
      connames = lappend(connames, pstrdup(NameStr(con->conname)));
    }
  }

  systable_endscan(scan);

                                        
  ScanKeyInit(&key[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(child_rel)));
  scan = systable_beginscan(catalogRelation, ConstraintRelidTypidNameIndexId, true, NULL, 1, key);

  while (HeapTupleIsValid(constraintTuple = systable_getnext(scan)))
  {
    Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(constraintTuple);
    bool match;
    ListCell *lc;

    if (con->contype != CONSTRAINT_CHECK)
    {
      continue;
    }

    match = false;
    foreach (lc, connames)
    {
      if (strcmp(NameStr(con->conname), (char *)lfirst(lc)) == 0)
      {
        match = true;
        break;
      }
    }

    if (match)
    {
                                                               
      HeapTuple copyTuple = heap_copytuple(constraintTuple);
      Form_pg_constraint copy_con = (Form_pg_constraint)GETSTRUCT(copyTuple);

      if (copy_con->coninhcount <= 0)                       
      {
        elog(ERROR, "relation %u has non-inherited constraint \"%s\"", RelationGetRelid(child_rel), NameStr(copy_con->conname));
      }

      copy_con->coninhcount--;
      if (copy_con->coninhcount == 0)
      {
        copy_con->conislocal = true;
      }

      CatalogTupleUpdate(catalogRelation, &copyTuple->t_self, copyTuple);
      heap_freetuple(copyTuple);
    }
  }

  systable_endscan(scan);
  table_close(catalogRelation, RowExclusiveLock);

  drop_parent_dependency(RelationGetRelid(child_rel), RelationRelationId, RelationGetRelid(parent_rel), child_dependency_type(child_is_partition));

     
                                                                             
                                                                        
                            
     
  InvokeObjectPostAlterHookArg(InheritsRelationId, RelationGetRelid(child_rel), 0, RelationGetRelid(parent_rel), false);
}

   
                                                                         
                                                                             
                                                                               
                                                                             
                      
   
static void
drop_parent_dependency(Oid relid, Oid refclassid, Oid refobjid, DependencyType deptype)
{
  Relation catalogRelation;
  SysScanDesc scan;
  ScanKeyData key[3];
  HeapTuple depTuple;

  catalogRelation = table_open(DependRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_depend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  ScanKeyInit(&key[2], Anum_pg_depend_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(0));

  scan = systable_beginscan(catalogRelation, DependDependerIndexId, true, NULL, 3, key);

  while (HeapTupleIsValid(depTuple = systable_getnext(scan)))
  {
    Form_pg_depend dep = (Form_pg_depend)GETSTRUCT(depTuple);

    if (dep->refclassid == refclassid && dep->refobjid == refobjid && dep->refobjsubid == 0 && dep->deptype == deptype)
    {
      CatalogTupleDelete(catalogRelation, &depTuple->t_self);
    }
  }

  systable_endscan(scan);
  table_close(catalogRelation, RowExclusiveLock);
}

   
                  
   
                                                                                 
                                                                                 
                                                                               
                                                                                 
   
                                        
   
static ObjectAddress
ATExecAddOf(Relation rel, const TypeName *ofTypename, LOCKMODE lockmode)
{
  Oid relid = RelationGetRelid(rel);
  Type typetuple;
  Form_pg_type typeform;
  Oid typeid;
  Relation inheritsRelation, relationRelation;
  SysScanDesc scan;
  ScanKeyData key;
  AttrNumber table_attno, type_attno;
  TupleDesc typeTupleDesc, tableTupleDesc;
  ObjectAddress tableobj, typeobj;
  HeapTuple classtuple;

                          
  typetuple = typenameType(NULL, ofTypename, NULL);
  check_of_type(typetuple);
  typeform = (Form_pg_type)GETSTRUCT(typetuple);
  typeid = typeform->oid;

                                                      
  inheritsRelation = table_open(InheritsRelationId, AccessShareLock);
  ScanKeyInit(&key, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  scan = systable_beginscan(inheritsRelation, InheritsRelidSeqnoIndexId, true, NULL, 1, &key);
  if (HeapTupleIsValid(systable_getnext(scan)))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("typed tables cannot inherit")));
  }
  systable_endscan(scan);
  table_close(inheritsRelation, AccessShareLock);

     
                                                                            
                                                                             
     
  typeTupleDesc = lookup_rowtype_tupdesc(typeid, -1);
  tableTupleDesc = RelationGetDescr(rel);
  table_attno = 1;
  for (type_attno = 1; type_attno <= typeTupleDesc->natts; type_attno++)
  {
    Form_pg_attribute type_attr, table_attr;
    const char *type_attname, *table_attname;

                                                  
    type_attr = TupleDescAttr(typeTupleDesc, type_attno - 1);
    if (type_attr->attisdropped)
    {
      continue;
    }
    type_attname = NameStr(type_attr->attname);

                                                   
    do
    {
      if (table_attno > tableTupleDesc->natts)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table is missing column \"%s\"", type_attname)));
      }
      table_attr = TupleDescAttr(tableTupleDesc, table_attno - 1);
      table_attno++;
    } while (table_attr->attisdropped);
    table_attname = NameStr(table_attr->attname);

                       
    if (strncmp(table_attname, type_attname, NAMEDATALEN) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table has column \"%s\" where type requires \"%s\"", table_attname, type_attname)));
    }

                       
    if (table_attr->atttypid != type_attr->atttypid || table_attr->atttypmod != type_attr->atttypmod || table_attr->attcollation != type_attr->attcollation)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table \"%s\" has different type for column \"%s\"", RelationGetRelationName(rel), type_attname)));
    }
  }
  DecrTupleDescRefCount(typeTupleDesc);

                                                                            
  for (; table_attno <= tableTupleDesc->natts; table_attno++)
  {
    Form_pg_attribute table_attr = TupleDescAttr(tableTupleDesc, table_attno - 1);

    if (!table_attr->attisdropped)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table has extra column \"%s\"", NameStr(table_attr->attname))));
    }
  }

                                                                     
  if (rel->rd_rel->reloftype)
  {
    drop_parent_dependency(relid, TypeRelationId, rel->rd_rel->reloftype, DEPENDENCY_NORMAL);
  }

                                            
  tableobj.classId = RelationRelationId;
  tableobj.objectId = relid;
  tableobj.objectSubId = 0;
  typeobj.classId = TypeRelationId;
  typeobj.objectId = typeid;
  typeobj.objectSubId = 0;
  recordDependencyOn(&tableobj, &typeobj, DEPENDENCY_NORMAL);

                                 
  relationRelation = table_open(RelationRelationId, RowExclusiveLock);
  classtuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(classtuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  ((Form_pg_class)GETSTRUCT(classtuple))->reloftype = typeid;
  CatalogTupleUpdate(relationRelation, &classtuple->t_self, classtuple);

  InvokeObjectPostAlterHook(RelationRelationId, relid, 0);

  heap_freetuple(classtuple);
  table_close(relationRelation, RowExclusiveLock);

  ReleaseSysCache(typetuple);

  return typeobj;
}

   
                      
   
                                                                             
                          
   
static void
ATExecDropOf(Relation rel, LOCKMODE lockmode)
{
  Oid relid = RelationGetRelid(rel);
  Relation relationRelation;
  HeapTuple tuple;

  if (!OidIsValid(rel->rd_rel->reloftype))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a typed table", RelationGetRelationName(rel))));
  }

     
                                                                         
                                                                             
     

  drop_parent_dependency(relid, TypeRelationId, rel->rd_rel->reloftype, DEPENDENCY_NORMAL);

                                
  relationRelation = table_open(RelationRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  ((Form_pg_class)GETSTRUCT(tuple))->reloftype = InvalidOid;
  CatalogTupleUpdate(relationRelation, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(RelationRelationId, relid, 0);

  heap_freetuple(tuple);
  table_close(relationRelation, RowExclusiveLock);
}

   
                                                                     
   
                                                                                
                                            
   
                                                                            
                                                            
   
static void
relation_mark_replica_identity(Relation rel, char ri_type, Oid indexOid, bool is_internal)
{
  Relation pg_index;
  Relation pg_class;
  HeapTuple pg_class_tuple;
  HeapTuple pg_index_tuple;
  Form_pg_class pg_class_form;
  Form_pg_index pg_index_form;
  ListCell *index;

     
                                                                  
     
  pg_class = table_open(RelationRelationId, RowExclusiveLock);
  pg_class_tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(RelationGetRelid(rel)));
  if (!HeapTupleIsValid(pg_class_tuple))
  {
    elog(ERROR, "cache lookup failed for relation \"%s\"", RelationGetRelationName(rel));
  }
  pg_class_form = (Form_pg_class)GETSTRUCT(pg_class_tuple);
  if (pg_class_form->relreplident != ri_type)
  {
    pg_class_form->relreplident = ri_type;
    CatalogTupleUpdate(pg_class, &pg_class_tuple->t_self, pg_class_tuple);
  }
  table_close(pg_class, RowExclusiveLock);
  heap_freetuple(pg_class_tuple);

     
                                                          
     
  pg_index = table_open(IndexRelationId, RowExclusiveLock);
  foreach (index, RelationGetIndexList(rel))
  {
    Oid thisIndexOid = lfirst_oid(index);
    bool dirty = false;

    pg_index_tuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(thisIndexOid));
    if (!HeapTupleIsValid(pg_index_tuple))
    {
      elog(ERROR, "cache lookup failed for index %u", thisIndexOid);
    }
    pg_index_form = (Form_pg_index)GETSTRUCT(pg_index_tuple);

    if (thisIndexOid == indexOid)
    {
                                           
      if (!pg_index_form->indisreplident)
      {
        dirty = true;
        pg_index_form->indisreplident = true;
      }
    }
    else
    {
                                 
      if (pg_index_form->indisreplident)
      {
        dirty = true;
        pg_index_form->indisreplident = false;
      }
    }

    if (dirty)
    {
      CatalogTupleUpdate(pg_index, &pg_index_tuple->t_self, pg_index_tuple);
      InvokeObjectPostAlterHookArg(IndexRelationId, thisIndexOid, 0, InvalidOid, is_internal);
         
                                                                        
                                                                      
                                                                      
                                                                       
                                                                
         
      CacheInvalidateRelcache(rel);
    }
    heap_freetuple(pg_index_tuple);
  }

  table_close(pg_index, RowExclusiveLock);
}

   
                                           
   
static void
ATExecReplicaIdentity(Relation rel, ReplicaIdentityStmt *stmt, LOCKMODE lockmode)
{
  Oid indexOid;
  Relation indexRel;
  int key;

  if (stmt->identity_type == REPLICA_IDENTITY_DEFAULT)
  {
    relation_mark_replica_identity(rel, stmt->identity_type, InvalidOid, true);
    return;
  }
  else if (stmt->identity_type == REPLICA_IDENTITY_FULL)
  {
    relation_mark_replica_identity(rel, stmt->identity_type, InvalidOid, true);
    return;
  }
  else if (stmt->identity_type == REPLICA_IDENTITY_NOTHING)
  {
    relation_mark_replica_identity(rel, stmt->identity_type, InvalidOid, true);
    return;
  }
  else if (stmt->identity_type == REPLICA_IDENTITY_INDEX)
  {
                     ;
  }
  else
  {
    elog(ERROR, "unexpected identity type %u", stmt->identity_type);
  }

                                   
  indexOid = get_relname_relid(stmt->name, rel->rd_rel->relnamespace);
  if (!OidIsValid(indexOid))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("index \"%s\" for table \"%s\" does not exist", stmt->name, RelationGetRelationName(rel))));
  }

  indexRel = index_open(indexOid, ShareLock);

                                                               
  if (indexRel->rd_index == NULL || indexRel->rd_index->indrelid != RelationGetRelid(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not an index for table \"%s\"", RelationGetRelationName(indexRel), RelationGetRelationName(rel))));
  }
                                                                             
  if (!indexRel->rd_indam->amcanunique || !indexRel->rd_index->indisunique)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot use non-unique index \"%s\" as replica identity", RelationGetRelationName(indexRel))));
  }
                                                                
  if (!indexRel->rd_index->indimmediate)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot use non-immediate index \"%s\" as replica identity", RelationGetRelationName(indexRel))));
  }
                                            
  if (RelationGetIndexExpressions(indexRel) != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot use expression index \"%s\" as replica identity", RelationGetRelationName(indexRel))));
  }
                                           
  if (RelationGetIndexPredicate(indexRel) != NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot use partial index \"%s\" as replica identity", RelationGetRelationName(indexRel))));
  }

                                         
  for (key = 0; key < IndexRelationGetNumberOfKeyAttributes(indexRel); key++)
  {
    int16 attno = indexRel->rd_index->indkey.values[key];
    Form_pg_attribute attr;

       
                                                                        
                                                                          
                                 
       
    if (attno <= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("index \"%s\" cannot be used as replica identity because column %d is a system column", RelationGetRelationName(indexRel), attno)));
    }

    attr = TupleDescAttr(rel->rd_att, attno - 1);
    if (!attr->attnotnull)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("index \"%s\" cannot be used as replica identity because column \"%s\" is nullable", RelationGetRelationName(indexRel), NameStr(attr->attname))));
    }
  }

                                                                      
  relation_mark_replica_identity(rel, stmt->identity_type, indexOid, true);

  index_close(indexRel, NoLock);
}

   
                                                 
   
static void
ATExecEnableRowSecurity(Relation rel)
{
  Relation pg_class;
  Oid relid;
  HeapTuple tuple;

  relid = RelationGetRelid(rel);

  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }

  ((Form_pg_class)GETSTRUCT(tuple))->relrowsecurity = true;
  CatalogTupleUpdate(pg_class, &tuple->t_self, tuple);

  table_close(pg_class, RowExclusiveLock);
  heap_freetuple(tuple);
}

static void
ATExecDisableRowSecurity(Relation rel)
{
  Relation pg_class;
  Oid relid;
  HeapTuple tuple;

  relid = RelationGetRelid(rel);

                                                       
  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }

  ((Form_pg_class)GETSTRUCT(tuple))->relrowsecurity = false;
  CatalogTupleUpdate(pg_class, &tuple->t_self, tuple);

  table_close(pg_class, RowExclusiveLock);
  heap_freetuple(tuple);
}

   
                                                 
   
static void
ATExecForceNoForceRowSecurity(Relation rel, bool force_rls)
{
  Relation pg_class;
  Oid relid;
  HeapTuple tuple;

  relid = RelationGetRelid(rel);

  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }

  ((Form_pg_class)GETSTRUCT(tuple))->relforcerowsecurity = force_rls;
  CatalogTupleUpdate(pg_class, &tuple->t_self, tuple);

  table_close(pg_class, RowExclusiveLock);
  heap_freetuple(tuple);
}

   
                                            
   
static void
ATExecGenericOptions(Relation rel, List *options)
{
  Relation ftrel;
  ForeignServer *server;
  ForeignDataWrapper *fdw;
  HeapTuple tuple;
  bool isnull;
  Datum repl_val[Natts_pg_foreign_table];
  bool repl_null[Natts_pg_foreign_table];
  bool repl_repl[Natts_pg_foreign_table];
  Datum datum;
  Form_pg_foreign_table tableform;

  if (options == NIL)
  {
    return;
  }

  ftrel = table_open(ForeignTableRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(FOREIGNTABLEREL, rel->rd_id);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("foreign table \"%s\" does not exist", RelationGetRelationName(rel))));
  }
  tableform = (Form_pg_foreign_table)GETSTRUCT(tuple);
  server = GetForeignServer(tableform->ftserver);
  fdw = GetForeignDataWrapper(server->fdwid);

  memset(repl_val, 0, sizeof(repl_val));
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));

                                   
  datum = SysCacheGetAttr(FOREIGNTABLEREL, tuple, Anum_pg_foreign_table_ftoptions, &isnull);
  if (isnull)
  {
    datum = PointerGetDatum(NULL);
  }

                             
  datum = transformGenericOptions(ForeignTableRelationId, datum, options, fdw->fdwvalidator);

  if (PointerIsValid(DatumGetPointer(datum)))
  {
    repl_val[Anum_pg_foreign_table_ftoptions - 1] = datum;
  }
  else
  {
    repl_null[Anum_pg_foreign_table_ftoptions - 1] = true;
  }

  repl_repl[Anum_pg_foreign_table_ftoptions - 1] = true;

                                                

  tuple = heap_modify_tuple(tuple, RelationGetDescr(ftrel), repl_val, repl_null, repl_repl);

  CatalogTupleUpdate(ftrel, &tuple->t_self, tuple);

     
                                                                            
                                           
     
  CacheInvalidateRelcache(rel);

  InvokeObjectPostAlterHook(ForeignTableRelationId, RelationGetRelid(rel), 0);

  table_close(ftrel, RowExclusiveLock);

  heap_freetuple(tuple);
}

   
                                             
   
                                                                      
                                                                        
                                                 
   
                                                                        
                                        
   
static bool
ATPrepChangePersistence(Relation rel, bool toLogged)
{
  Relation pg_constraint;
  HeapTuple tuple;
  SysScanDesc scan;
  ScanKeyData skey[1];

     
                                                                            
                                                                         
                           
     
  switch (rel->rd_rel->relpersistence)
  {
  case RELPERSISTENCE_TEMP:
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot change logged status of table \"%s\" because it is temporary", RelationGetRelationName(rel)), errtable(rel)));
    break;
  case RELPERSISTENCE_PERMANENT:
    if (toLogged)
    {
                         
      return false;
    }
    break;
  case RELPERSISTENCE_UNLOGGED:
    if (!toLogged)
    {
                         
      return false;
    }
    break;
  }

     
                                                                       
                                                     
     
  if (!toLogged && list_length(GetRelationPublications(RelationGetRelid(rel))) > 0)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot change table \"%s\" to unlogged because it is part of a publication", RelationGetRelationName(rel)), errdetail("Unlogged relations cannot be replicated.")));
  }

     
                                                                           
                                                                        
                                         
     
  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);

     
                                                                        
                                               
     
  ScanKeyInit(&skey[0], toLogged ? Anum_pg_constraint_conrelid : Anum_pg_constraint_confrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  scan = systable_beginscan(pg_constraint, toLogged ? ConstraintRelidTypidNameIndexId : InvalidOid, true, NULL, 1, skey);

  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(tuple);

    if (con->contype == CONSTRAINT_FOREIGN)
    {
      Oid foreignrelid;
      Relation foreignrel;

                                                       
      foreignrelid = toLogged ? con->confrelid : con->conrelid;

                                      
      if (RelationGetRelid(rel) == foreignrelid)
      {
        continue;
      }

      foreignrel = relation_open(foreignrelid, AccessShareLock);

      if (toLogged)
      {
        if (foreignrel->rd_rel->relpersistence != RELPERSISTENCE_PERMANENT)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("could not change table \"%s\" to logged because it references unlogged table \"%s\"", RelationGetRelationName(rel), RelationGetRelationName(foreignrel)), errtableconstraint(rel, NameStr(con->conname))));
        }
      }
      else
      {
        if (foreignrel->rd_rel->relpersistence == RELPERSISTENCE_PERMANENT)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("could not change table \"%s\" to unlogged because it references logged table \"%s\"", RelationGetRelationName(rel), RelationGetRelationName(foreignrel)), errtableconstraint(rel, NameStr(con->conname))));
        }
      }

      relation_close(foreignrel, AccessShareLock);
    }
  }

  systable_endscan(scan);

  table_close(pg_constraint, AccessShareLock);

  return true;
}

   
                                  
   
ObjectAddress
AlterTableNamespace(AlterObjectSchemaStmt *stmt, Oid *oldschema)
{
  Relation rel;
  Oid relid;
  Oid oldNspOid;
  Oid nspOid;
  RangeVar *newrv;
  ObjectAddresses *objsMoved;
  ObjectAddress myself;

  relid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock, stmt->missing_ok ? RVR_MISSING_OK : 0, RangeVarCallbackForAlterRelation, (void *)stmt);

  if (!OidIsValid(relid))
  {
    ereport(NOTICE, (errmsg("relation \"%s\" does not exist, skipping", stmt->relation->relname)));
    return InvalidObjectAddress;
  }

  rel = relation_open(relid, NoLock);

  oldNspOid = RelationGetNamespace(rel);

                                                                
  if (rel->rd_rel->relkind == RELKIND_SEQUENCE)
  {
    Oid tableId;
    int32 colId;

    if (sequenceIsOwned(relid, DEPENDENCY_AUTO, &tableId, &colId) || sequenceIsOwned(relid, DEPENDENCY_INTERNAL, &tableId, &colId))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot move an owned sequence into another schema"), errdetail("Sequence \"%s\" is linked to table \"%s\".", RelationGetRelationName(rel), get_rel_name(tableId))));
    }
  }

                                                          
  newrv = makeRangeVar(stmt->newschema, RelationGetRelationName(rel), -1);
  nspOid = RangeVarGetAndCheckCreationNamespace(newrv, NoLock, NULL);

                                             
  CheckSetNamespace(oldNspOid, nspOid);

  objsMoved = new_object_addresses();
  AlterTableNamespaceInternal(rel, oldNspOid, nspOid, objsMoved);
  free_object_addresses(objsMoved);

  ObjectAddressSet(myself, RelationRelationId, relid);

  if (oldschema)
  {
    *oldschema = oldNspOid;
  }

                                             
  relation_close(rel, NoLock);

  return myself;
}

   
                                                                             
                                                                              
                   
   
void
AlterTableNamespaceInternal(Relation rel, Oid oldNspOid, Oid nspOid, ObjectAddresses *objsMoved)
{
  Relation classRel;

  Assert(objsMoved != NULL);

                                                       
  classRel = table_open(RelationRelationId, RowExclusiveLock);

  AlterRelationNamespaceInternal(classRel, RelationGetRelid(rel), oldNspOid, nspOid, true, objsMoved);

                                    
  AlterTypeNamespaceInternal(rel->rd_rel->reltype, nspOid, false, false, objsMoved);

                                 
  if (rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_MATVIEW || rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    AlterIndexNamespaces(classRel, rel, oldNspOid, nspOid, objsMoved);
    AlterSeqNamespaces(classRel, rel, oldNspOid, nspOid, objsMoved, AccessExclusiveLock);
    AlterConstraintNamespaces(RelationGetRelid(rel), oldNspOid, nspOid, false, objsMoved);
  }

  table_close(classRel, RowExclusiveLock);
}

   
                                                                            
                                                                    
                                     
   
void
AlterRelationNamespaceInternal(Relation classRel, Oid relOid, Oid oldNspOid, Oid newNspOid, bool hasDependEntry, ObjectAddresses *objsMoved)
{
  HeapTuple classTup;
  Form_pg_class classForm;
  ObjectAddress thisobj;
  bool already_done = false;

  classTup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relOid));
  if (!HeapTupleIsValid(classTup))
  {
    elog(ERROR, "cache lookup failed for relation %u", relOid);
  }
  classForm = (Form_pg_class)GETSTRUCT(classTup);

  Assert(classForm->relnamespace == oldNspOid);

  thisobj.classId = RelationRelationId;
  thisobj.objectId = relOid;
  thisobj.objectSubId = 0;

     
                                                                         
                                                                          
                  
     
  already_done = object_address_present(&thisobj, objsMoved);
  if (!already_done && oldNspOid != newNspOid)
  {
                                                                            
    if (get_relname_relid(NameStr(classForm->relname), newNspOid) != InvalidOid)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" already exists in schema \"%s\"", NameStr(classForm->relname), get_namespace_name(newNspOid))));
    }

                                                  
    classForm->relnamespace = newNspOid;

    CatalogTupleUpdate(classRel, &classTup->t_self, classTup);

                                                       
    if (hasDependEntry && changeDependencyFor(RelationRelationId, relOid, NamespaceRelationId, oldNspOid, newNspOid) != 1)
    {
      elog(ERROR, "failed to change schema dependency for relation \"%s\"", NameStr(classForm->relname));
    }
  }
  if (!already_done)
  {
    add_exact_object_address(&thisobj, objsMoved);

    InvokeObjectPostAlterHook(RelationRelationId, relOid, 0);
  }

  heap_freetuple(classTup);
}

   
                                                                     
   
                                                                        
                                                                   
   
static void
AlterIndexNamespaces(Relation classRel, Relation rel, Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved)
{
  List *indexList;
  ListCell *l;

  indexList = RelationGetIndexList(rel);

  foreach (l, indexList)
  {
    Oid indexOid = lfirst_oid(l);
    ObjectAddress thisobj;

    thisobj.classId = RelationRelationId;
    thisobj.objectId = indexOid;
    thisobj.objectSubId = 0;

       
                                                                          
                                                                           
                                    
       
                                                                           
                                                      
       
    if (!object_address_present(&thisobj, objsMoved))
    {
      AlterRelationNamespaceInternal(classRel, indexOid, oldNspOid, newNspOid, false, objsMoved);
      add_exact_object_address(&thisobj, objsMoved);
    }
  }

  list_free(indexList);
}

   
                                                                                      
              
   
                                                                        
                                                                   
   
static void
AlterSeqNamespaces(Relation classRel, Relation rel, Oid oldNspOid, Oid newNspOid, ObjectAddresses *objsMoved, LOCKMODE lockmode)
{
  Relation depRel;
  SysScanDesc scan;
  ScanKeyData key[2];
  HeapTuple tup;

     
                                                                        
                                                              
     
  depRel = table_open(DependRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_depend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
                                        

  scan = systable_beginscan(depRel, DependReferenceIndexId, true, NULL, 2, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_depend depForm = (Form_pg_depend)GETSTRUCT(tup);
    Relation seqRel;

                                                                   
    if (depForm->refobjsubid == 0 || depForm->classid != RelationRelationId || depForm->objsubid != 0 || !(depForm->deptype == DEPENDENCY_AUTO || depForm->deptype == DEPENDENCY_INTERNAL))
    {
      continue;
    }

                                                      
    seqRel = relation_open(depForm->objid, lockmode);

                                     
    if (RelationGetForm(seqRel)->relkind != RELKIND_SEQUENCE)
    {
                                    
      relation_close(seqRel, lockmode);
      continue;
    }

                                                
    AlterRelationNamespaceInternal(classRel, depForm->objid, oldNspOid, newNspOid, true, objsMoved);

       
                                                                        
                                       
       
    AlterTypeNamespaceInternal(RelationGetForm(seqRel)->reltype, newNspOid, false, false, objsMoved);

                                                                      
    relation_close(seqRel, NoLock);
  }

  systable_endscan(scan);

  relation_close(depRel, AccessShareLock);
}

   
                      
                                                                          
   
                                                                             
                                                
   

   
                                                         
   
void
register_on_commit_action(Oid relid, OnCommitAction action)
{
  OnCommitItem *oc;
  MemoryContext oldcxt;

     
                                                                             
                             
     
  if (action == ONCOMMIT_NOOP || action == ONCOMMIT_PRESERVE_ROWS)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

  oc = (OnCommitItem *)palloc(sizeof(OnCommitItem));
  oc->relid = relid;
  oc->oncommit = action;
  oc->creating_subid = GetCurrentSubTransactionId();
  oc->deleting_subid = InvalidSubTransactionId;

  on_commits = lcons(oc, on_commits);

  MemoryContextSwitchTo(oldcxt);
}

   
                                                               
   
                                                                                
   
void
remove_on_commit_action(Oid relid)
{
  ListCell *l;

  foreach (l, on_commits)
  {
    OnCommitItem *oc = (OnCommitItem *)lfirst(l);

    if (oc->relid == relid)
    {
      oc->deleting_subid = GetCurrentSubTransactionId();
      break;
    }
  }
}

   
                              
   
                                                                        
                        
   
void
PreCommit_on_commit_actions(void)
{
  ListCell *l;
  List *oids_to_truncate = NIL;
  List *oids_to_drop = NIL;

  foreach (l, on_commits)
  {
    OnCommitItem *oc = (OnCommitItem *)lfirst(l);

                                                      
    if (oc->deleting_subid != InvalidSubTransactionId)
    {
      continue;
    }

    switch (oc->oncommit)
    {
    case ONCOMMIT_NOOP:
    case ONCOMMIT_PRESERVE_ROWS:
                                                                  
      break;
    case ONCOMMIT_DELETE_ROWS:

         
                                                           
                                                                 
                                              
         
      if ((MyXactFlags & XACT_FLAGS_ACCESSEDTEMPNAMESPACE))
      {
        oids_to_truncate = lappend_oid(oids_to_truncate, oc->relid);
      }
      break;
    case ONCOMMIT_DROP:
      oids_to_drop = lappend_oid(oids_to_drop, oc->relid);
      break;
    }
  }

     
                                                                         
                                                                         
                                                                             
                                                                           
                                                                          
                                
     
  if (oids_to_truncate != NIL)
  {
    heap_truncate(oids_to_truncate);
  }

  if (oids_to_drop != NIL)
  {
    ObjectAddresses *targetObjects = new_object_addresses();
    ListCell *l;

    foreach (l, oids_to_drop)
    {
      ObjectAddress object;

      object.classId = RelationRelationId;
      object.objectId = lfirst_oid(l);
      object.objectSubId = 0;

      Assert(!object_address_present(&object, targetObjects));

      add_exact_object_address(&object, targetObjects);
    }

       
                                                                           
                                                                
       
    performMultipleDeletions(targetObjects, DROP_CASCADE, PERFORM_DELETION_INTERNAL | PERFORM_DELETION_QUIETLY);

#ifdef USE_ASSERT_CHECKING

       
                                                                          
                                           
       
    foreach (l, on_commits)
    {
      OnCommitItem *oc = (OnCommitItem *)lfirst(l);

      if (oc->oncommit != ONCOMMIT_DROP)
      {
        continue;
      }

      Assert(oc->deleting_subid != InvalidSubTransactionId);
    }
#endif
  }
}

   
                                                               
   
                                                                   
   
                                                                            
                                                               
   
void
AtEOXact_on_commit_actions(bool isCommit)
{
  ListCell *cur_item;
  ListCell *prev_item;

  prev_item = NULL;
  cur_item = list_head(on_commits);

  while (cur_item != NULL)
  {
    OnCommitItem *oc = (OnCommitItem *)lfirst(cur_item);

    if (isCommit ? oc->deleting_subid != InvalidSubTransactionId : oc->creating_subid != InvalidSubTransactionId)
    {
                                    
      on_commits = list_delete_cell(on_commits, cur_item, prev_item);
      pfree(oc);
      if (prev_item)
      {
        cur_item = lnext(prev_item);
      }
      else
      {
        cur_item = list_head(on_commits);
      }
    }
    else
    {
                                      
      oc->creating_subid = InvalidSubTransactionId;
      oc->deleting_subid = InvalidSubTransactionId;
      prev_item = cur_item;
      cur_item = lnext(prev_item);
    }
  }
}

   
                                                                     
   
                                                                          
                                                                         
                                                             
   
void
AtEOSubXact_on_commit_actions(bool isCommit, SubTransactionId mySubid, SubTransactionId parentSubid)
{
  ListCell *cur_item;
  ListCell *prev_item;

  prev_item = NULL;
  cur_item = list_head(on_commits);

  while (cur_item != NULL)
  {
    OnCommitItem *oc = (OnCommitItem *)lfirst(cur_item);

    if (!isCommit && oc->creating_subid == mySubid)
    {
                                    
      on_commits = list_delete_cell(on_commits, cur_item, prev_item);
      pfree(oc);
      if (prev_item)
      {
        cur_item = lnext(prev_item);
      }
      else
      {
        cur_item = list_head(on_commits);
      }
    }
    else
    {
                                      
      if (oc->creating_subid == mySubid)
      {
        oc->creating_subid = parentSubid;
      }
      if (oc->deleting_subid == mySubid)
      {
        oc->deleting_subid = isCommit ? parentSubid : InvalidSubTransactionId;
      }
      prev_item = cur_item;
      cur_item = lnext(prev_item);
    }
  }
}

   
                                                                             
                                                                          
                                                                      
                                                                             
                                                                             
                
   
void
RangeVarCallbackOwnsTable(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg)
{
  char relkind;

                                                    
  if (!OidIsValid(relId))
  {
    return;
  }

     
                                                                             
                                                                           
                                                           
     
  relkind = get_rel_relkind(relId);
  if (!relkind)
  {
    return;
  }
  if (relkind != RELKIND_RELATION && relkind != RELKIND_TOASTVALUE && relkind != RELKIND_MATVIEW && relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table or materialized view", relation->relname)));
  }

                         
  if (!pg_class_ownercheck(relId, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relId)), relation->relname);
  }
}

   
                                                                   
   
static void
RangeVarCallbackForTruncate(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg)
{
  HeapTuple tuple;

                                                    
  if (!OidIsValid(relId))
  {
    return;
  }

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relId));
  if (!HeapTupleIsValid(tuple))                        
  {
    elog(ERROR, "cache lookup failed for relation %u", relId);
  }

  truncate_check_rel(relId, (Form_pg_class)GETSTRUCT(tuple));

  ReleaseSysCache(tuple);
}

   
                                                      
                                                                               
   
void
RangeVarCallbackOwnsRelation(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg)
{
  HeapTuple tuple;

                                                    
  if (!OidIsValid(relId))
  {
    return;
  }

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relId));
  if (!HeapTupleIsValid(tuple))                        
  {
    elog(ERROR, "cache lookup failed for relation %u", relId);
  }

  if (!pg_class_ownercheck(relId, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relId)), relation->relname);
  }

  if (!allowSystemTableMods && IsSystemClass(relId, (Form_pg_class)GETSTRUCT(tuple)))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", relation->relname)));
  }

  ReleaseSysCache(tuple);
}

   
                                                                            
               
   
static void
RangeVarCallbackForAlterRelation(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{
  Node *stmt = (Node *)arg;
  ObjectType reltype;
  HeapTuple tuple;
  Form_pg_class classform;
  AclResult aclresult;
  char relkind;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    return;                           
  }
  classform = (Form_pg_class)GETSTRUCT(tuple);
  relkind = classform->relkind;

                          
  if (!pg_class_ownercheck(relid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relid)), rv->relname);
  }

                                                                
  if (!allowSystemTableMods && IsSystemClass(relid, classform))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", rv->relname)));
  }

     
                                                                        
     
                                                                         
                                                     
     
  if (IsA(stmt, RenameStmt))
  {
    aclresult = pg_namespace_aclcheck(classform->relnamespace, GetUserId(), ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(classform->relnamespace));
    }
    reltype = ((RenameStmt *)stmt)->renameType;
  }
  else if (IsA(stmt, AlterObjectSchemaStmt))
  {
    reltype = ((AlterObjectSchemaStmt *)stmt)->objectType;
  }

  else if (IsA(stmt, AlterTableStmt))
  {
    reltype = ((AlterTableStmt *)stmt)->relkind;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(stmt));
    reltype = OBJECT_TABLE;                       
  }

     
                                                                            
                                                                            
                                                                        
                                                                         
                                        
     
  if (reltype == OBJECT_SEQUENCE && relkind != RELKIND_SEQUENCE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a sequence", rv->relname)));
  }

  if (reltype == OBJECT_VIEW && relkind != RELKIND_VIEW)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a view", rv->relname)));
  }

  if (reltype == OBJECT_MATVIEW && relkind != RELKIND_MATVIEW)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a materialized view", rv->relname)));
  }

  if (reltype == OBJECT_FOREIGN_TABLE && relkind != RELKIND_FOREIGN_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a foreign table", rv->relname)));
  }

  if (reltype == OBJECT_TYPE && relkind != RELKIND_COMPOSITE_TYPE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a composite type", rv->relname)));
  }

  if (reltype == OBJECT_INDEX && relkind != RELKIND_INDEX && relkind != RELKIND_PARTITIONED_INDEX && !IsA(stmt, RenameStmt))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not an index", rv->relname)));
  }

     
                                                                             
                    
     
  if (reltype != OBJECT_TYPE && relkind == RELKIND_COMPOSITE_TYPE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a composite type", rv->relname), errhint("Use ALTER TYPE instead.")));
  }

     
                                                                            
                                                              
     
  if (IsA(stmt, AlterObjectSchemaStmt) && relkind != RELKIND_RELATION && relkind != RELKIND_VIEW && relkind != RELKIND_MATVIEW && relkind != RELKIND_SEQUENCE && relkind != RELKIND_FOREIGN_TABLE && relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, view, materialized view, sequence, or foreign table", rv->relname)));
  }

  ReleaseSysCache(tuple);
}

   
                                                          
   
                                                                     
   
static PartitionSpec *
transformPartitionSpec(Relation rel, PartitionSpec *partspec, char *strategy)
{
  PartitionSpec *newspec;
  ParseState *pstate;
  RangeTblEntry *rte;
  ListCell *l;

  newspec = makeNode(PartitionSpec);

  newspec->strategy = partspec->strategy;
  newspec->partParams = NIL;
  newspec->location = partspec->location;

                                        
  if (pg_strcasecmp(partspec->strategy, "hash") == 0)
  {
    *strategy = PARTITION_STRATEGY_HASH;
  }
  else if (pg_strcasecmp(partspec->strategy, "list") == 0)
  {
    *strategy = PARTITION_STRATEGY_LIST;
  }
  else if (pg_strcasecmp(partspec->strategy, "range") == 0)
  {
    *strategy = PARTITION_STRATEGY_RANGE;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized partitioning strategy \"%s\"", partspec->strategy)));
  }

                                                  
  if (*strategy == PARTITION_STRATEGY_LIST && list_length(partspec->partParams) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use \"list\" partition strategy with more than one column")));
  }

     
                                                                          
                                                                
     
  pstate = make_parsestate(NULL);
  rte = addRangeTableEntryForRelation(pstate, rel, AccessShareLock, NULL, false, true);
  addRTEtoQuery(pstate, rte, true, true, true);

                                              
  foreach (l, partspec->partParams)
  {
    PartitionElem *pelem = castNode(PartitionElem, lfirst(l));

    if (pelem->expr)
    {
                                                  
      pelem = copyObject(pelem);

                                                         
      pelem->expr = transformExpr(pstate, pelem->expr, EXPR_KIND_PARTITION_EXPRESSION);

                                             
      assign_expr_collations(pstate, pelem->expr);
    }

    newspec->partParams = lappend(newspec->partParams, pelem);
  }

  return newspec;
}

   
                                                                           
                                                                     
   
static void
ComputePartitionAttrs(ParseState *pstate, Relation rel, List *partParams, AttrNumber *partattrs, List **partexprs, Oid *partopclass, Oid *partcollation, char strategy)
{
  int attn;
  ListCell *lc;
  Oid am_oid;

  attn = 0;
  foreach (lc, partParams)
  {
    PartitionElem *pelem = castNode(PartitionElem, lfirst(lc));
    Oid atttype;
    Oid attcollation;

    if (pelem->name != NULL)
    {
                                      
      HeapTuple atttuple;
      Form_pg_attribute attform;

      atttuple = SearchSysCacheAttName(RelationGetRelid(rel), pelem->name);
      if (!HeapTupleIsValid(atttuple))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" named in partition key does not exist", pelem->name), parser_errposition(pstate, pelem->location)));
      }
      attform = (Form_pg_attribute)GETSTRUCT(atttuple);

      if (attform->attnum <= 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use system column \"%s\" in partition key", pelem->name), parser_errposition(pstate, pelem->location)));
      }

         
                                                                       
                                                                      
         
      if (attform->attgenerated)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use generated column in partition key"), errdetail("Column \"%s\" is a generated column.", pelem->name), parser_errposition(pstate, pelem->location)));
      }

      partattrs[attn] = attform->attnum;
      atttype = attform->atttypid;
      attcollation = attform->attcollation;
      ReleaseSysCache(atttuple);
    }
    else
    {
                      
      Node *expr = pelem->expr;
      char partattname[16];

      Assert(expr != NULL);
      atttype = exprType(expr);
      attcollation = exprCollation(expr);

         
                                                                       
                                                                         
                                                                    
                
         
      snprintf(partattname, sizeof(partattname), "%d", attn + 1);
      CheckAttributeType(partattname, atttype, attcollation, NIL, CHKATYPE_IS_PARTKEY);

         
                                                                         
                                                  
         
      while (IsA(expr, CollateExpr))
      {
        expr = (Node *)((CollateExpr *)expr)->arg;
      }

      if (IsA(expr, Var) && ((Var *)expr)->varattno > 0)
      {
           
                                                                  
                                                  
           
        partattrs[attn] = ((Var *)expr)->varattno;
      }
      else
      {
        Bitmapset *expr_attrs = NULL;
        int i;

        partattrs[attn] = 0;                                     
        *partexprs = lappend(*partexprs, expr);

           
                                                              
                                                                     
                                                                      
                                                                      
                                                
           
                                                                      
                                                                  
                                                           
           
        expr = (Node *)expression_planner((Expr *)expr);

           
                                                                  
                                                                     
                                                                   
                      
           
        if (contain_mutable_functions(expr))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("functions in partition key expression must be marked IMMUTABLE")));
        }

           
                                                                 
                                                                     
                                                        
           

           
                                                                      
                                     
           
        pull_varattnos(expr, 1, &expr_attrs);
        if (bms_is_member(0 - FirstLowInvalidHeapAttributeNumber, expr_attrs))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("partition key expressions cannot contain whole-row references")));
        }
        for (i = FirstLowInvalidHeapAttributeNumber; i < 0; i++)
        {
          if (bms_is_member(i - FirstLowInvalidHeapAttributeNumber, expr_attrs))
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("partition key expressions cannot contain system column references")));
          }
        }

           
                                                                  
                                                                     
                     
           
        i = -1;
        while ((i = bms_next_member(expr_attrs, i)) >= 0)
        {
          AttrNumber attno = i + FirstLowInvalidHeapAttributeNumber;

          if (TupleDescAttr(RelationGetDescr(rel), attno - 1)->attgenerated)
          {
            ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use generated column in partition key"), errdetail("Column \"%s\" is a generated column.", get_attname(RelationGetRelid(rel), attno, false)), parser_errposition(pstate, pelem->location)));
          }
        }

           
                                                                      
                                                                  
           
        if (IsA(expr, Const))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use constant expression as partition key")));
        }
      }
    }

       
                                       
       
    if (pelem->collation)
    {
      attcollation = get_collation_oid(pelem->collation, false);
    }

       
                                                                       
                                                                         
                                                                          
                                                                      
       
    if (type_is_collatable(atttype))
    {
      if (!OidIsValid(attcollation))
      {
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for partition expression"), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }
    }
    else
    {
      if (OidIsValid(attcollation))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be(atttype))));
      }
    }

    partcollation[attn] = attcollation;

       
                                                                    
                                                                           
                              
       
    if (strategy == PARTITION_STRATEGY_HASH)
    {
      am_oid = HASH_AM_OID;
    }
    else
    {
      am_oid = BTREE_AM_OID;
    }

    if (!pelem->opclass)
    {
      partopclass[attn] = GetDefaultOpClass(atttype, am_oid);

      if (!OidIsValid(partopclass[attn]))
      {
        if (strategy == PARTITION_STRATEGY_HASH)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("data type %s has no default operator class for access method \"%s\"", format_type_be(atttype), "hash"), errhint("You must specify a hash operator class or define a default hash operator class for the data type.")));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("data type %s has no default operator class for access method \"%s\"", format_type_be(atttype), "btree"), errhint("You must specify a btree operator class or define a default btree operator class for the data type.")));
        }
      }
    }
    else
    {
      partopclass[attn] = ResolveOpClass(pelem->opclass, atttype, am_oid == HASH_AM_OID ? "hash" : "btree", am_oid);
    }

    attn++;
  }
}

   
                                        
                                                                      
   
                                                                         
                                                                             
                         
   
bool
PartConstraintImpliedByRelConstraint(Relation scanrel, List *partConstraint)
{
  List *existConstraint = NIL;
  TupleConstr *constr = RelationGetDescr(scanrel)->constr;
  int i;

  if (constr && constr->has_not_null)
  {
    int natts = scanrel->rd_att->natts;

    for (i = 1; i <= natts; i++)
    {
      Form_pg_attribute att = TupleDescAttr(scanrel->rd_att, i - 1);

      if (att->attnotnull && !att->attisdropped)
      {
        NullTest *ntest = makeNode(NullTest);

        ntest->arg = (Expr *)makeVar(1, i, att->atttypid, att->atttypmod, att->attcollation, 0);
        ntest->nulltesttype = IS_NOT_NULL;

           
                                                                  
                                                                   
                                                                 
           
        ntest->argisrow = false;
        ntest->location = -1;
        existConstraint = lappend(existConstraint, ntest);
      }
    }
  }

  return ConstraintImpliedByRelConstraint(scanrel, partConstraint, existConstraint);
}

   
                                    
                                                                  
   
                                                                       
                                                                     
                                                                   
                                                                    
                                     
   
bool
ConstraintImpliedByRelConstraint(Relation scanrel, List *testConstraint, List *provenConstraint)
{
  List *existConstraint = list_copy(provenConstraint);
  TupleConstr *constr = RelationGetDescr(scanrel)->constr;
  int num_check, i;

  num_check = (constr != NULL) ? constr->num_check : 0;
  for (i = 0; i < num_check; i++)
  {
    Node *cexpr;

       
                                                                          
                
       
    if (!constr->check[i].ccvalid)
    {
      continue;
    }

    cexpr = stringToNode(constr->check[i].ccbin);

       
                                                            
                                                                           
                                                                        
                                                  
       
    cexpr = eval_const_expressions(NULL, cexpr);
    cexpr = (Node *)canonicalize_qual((Expr *)cexpr, true);

    existConstraint = list_concat(existConstraint, make_ands_implicit((Expr *)cexpr));
  }

     
                                                                          
                                                                      
                                                             
     
                                                                        
                                                                            
                                            
     
  return predicate_implied_by(testConstraint, existConstraint, true);
}

   
                                      
   
                                                                              
                                                          
   
                                                                           
                                                                        
                                          
   
static void
QueuePartitionConstraintValidation(List **wqueue, Relation scanrel, List *partConstraint, bool validate_default)
{
     
                                                                            
                                  
     
  if (PartConstraintImpliedByRelConstraint(scanrel, partConstraint))
  {
    if (!validate_default)
    {
      ereport(DEBUG1, (errmsg("partition constraint for table \"%s\" is implied by existing constraints", RelationGetRelationName(scanrel))));
    }
    else
    {
      ereport(DEBUG1, (errmsg("updated partition constraint for default partition \"%s\" is implied by existing constraints", RelationGetRelationName(scanrel))));
    }
    return;
  }

     
                                                                   
                                                                          
                
     
  if (scanrel->rd_rel->relkind == RELKIND_RELATION)
  {
    AlteredTableInfo *tab;

                                  
    tab = ATGetQueueEntry(wqueue, scanrel);
    Assert(tab->partition_constraint == NULL);
    tab->partition_constraint = (Expr *)linitial(partConstraint);
    tab->validate_default = validate_default;
  }
  else if (scanrel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    PartitionDesc partdesc = RelationGetPartitionDesc(scanrel);
    int i;

    for (i = 0; i < partdesc->nparts; i++)
    {
      Relation part_rel;
      bool found_whole_row;
      List *thisPartConstraint;

         
                                                                
         
      part_rel = table_open(partdesc->oids[i], AccessExclusiveLock);

         
                                                                   
                                        
         
      thisPartConstraint = map_partition_varattnos(partConstraint, 1, part_rel, scanrel, &found_whole_row);
                                                         
      if (found_whole_row)
      {
        elog(ERROR, "unexpected whole-row reference found in partition constraint");
      }

      QueuePartitionConstraintValidation(wqueue, part_rel, thisPartConstraint, validate_default);
      table_close(part_rel, NoLock);                            
    }
  }
}

   
                                                                   
   
                                                       
   
static ObjectAddress
ATExecAttachPartition(List **wqueue, Relation rel, PartitionCmd *cmd)
{
  Relation attachrel, catalog;
  List *attachrel_children;
  List *partConstraint;
  SysScanDesc scan;
  ScanKeyData skey;
  AttrNumber attno;
  int natts;
  TupleDesc tupleDesc;
  ObjectAddress address;
  const char *trigger_name;
  bool found_whole_row;
  Oid defaultPartOid;
  List *partBoundConstraint;

     
                                                                           
                                                         
     
  defaultPartOid = get_default_oid_from_partdesc(RelationGetPartitionDesc(rel));
  if (OidIsValid(defaultPartOid))
  {
    LockRelationOid(defaultPartOid, AccessExclusiveLock);
  }

  attachrel = table_openrv(cmd->name, AccessExclusiveLock);

     
                                                                            
                                
     

     
                                                                            
                                           
     
  ATSimplePermissions(attachrel, ATT_TABLE | ATT_FOREIGN_TABLE);

                                            
  if (attachrel->rd_rel->relispartition)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is already a partition", RelationGetRelationName(attachrel))));
  }

  if (OidIsValid(attachrel->rd_rel->reloftype))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach a typed table as partition")));
  }

     
                                                                            
                         
     
  catalog = table_open(InheritsRelationId, AccessShareLock);
  ScanKeyInit(&skey, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(attachrel)));
  scan = systable_beginscan(catalog, InheritsRelidSeqnoIndexId, true, NULL, 1, &skey);
  if (HeapTupleIsValid(systable_getnext(scan)))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach inheritance child as partition")));
  }
  systable_endscan(scan);

                                                                        
  ScanKeyInit(&skey, Anum_pg_inherits_inhparent, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(attachrel)));
  scan = systable_beginscan(catalog, InheritsParentIndexId, true, NULL, 1, &skey);
  if (HeapTupleIsValid(systable_getnext(scan)) && attachrel->rd_rel->relkind == RELKIND_RELATION)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach inheritance parent as partition")));
  }
  systable_endscan(scan);
  table_close(catalog, AccessShareLock);

     
                                                                           
                                                                     
     
                                                                          
                                                                             
                                                                           
                                                                           
                                                                            
                                                                            
                                                                       
                                                                       
                                                                           
                                                            
     
  attachrel_children = find_all_inheritors(RelationGetRelid(attachrel), AccessExclusiveLock, NULL);
  if (list_member_oid(attachrel_children, RelationGetRelid(rel)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("circular inheritance not allowed"), errdetail("\"%s\" is already a child of \"%s\".", RelationGetRelationName(rel), RelationGetRelationName(attachrel))));
  }

                                                                     
  if (rel->rd_rel->relpersistence != RELPERSISTENCE_TEMP && attachrel->rd_rel->relpersistence == RELPERSISTENCE_TEMP)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach a temporary relation as partition of permanent relation \"%s\"", RelationGetRelationName(rel))));
  }

                                                                     
  if (rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP && attachrel->rd_rel->relpersistence != RELPERSISTENCE_TEMP)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach a permanent relation as partition of temporary relation \"%s\"", RelationGetRelationName(rel))));
  }

                                                             
  if (rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP && !rel->rd_islocaltemp)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach as partition of temporary relation of another session")));
  }

                               
  if (attachrel->rd_rel->relpersistence == RELPERSISTENCE_TEMP && !attachrel->rd_islocaltemp)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach temporary relation of another session as partition")));
  }

                                                                             
  tupleDesc = RelationGetDescr(attachrel);
  natts = tupleDesc->natts;
  for (attno = 1; attno <= natts; attno++)
  {
    Form_pg_attribute attribute = TupleDescAttr(tupleDesc, attno - 1);
    char *attributeName = NameStr(attribute->attname);

                        
    if (attribute->attisdropped)
    {
      continue;
    }

                                                                    
    if (!SearchSysCacheExists2(ATTNAME, ObjectIdGetDatum(RelationGetRelid(rel)), CStringGetDatum(attributeName)))
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table \"%s\" contains column \"%s\" not found in parent \"%s\"", RelationGetRelationName(attachrel), attributeName, RelationGetRelationName(rel)), errdetail("The new partition may contain only the columns present in parent.")));
    }
  }

     
                                                                    
                                                                            
                                                
     
  trigger_name = FindTriggerIncompatibleWithInheritance(attachrel->trigdesc);
  if (trigger_name != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("trigger \"%s\" prevents table \"%s\" from becoming a partition", trigger_name, RelationGetRelationName(attachrel)), errdetail("ROW triggers with transition tables are not supported on partitions")));
  }

     
                                                                            
                                                                            
            
     
  check_new_partition_bound(RelationGetRelationName(attachrel), rel, cmd->bound);

                                                                     
  CreateInheritance(attachrel, rel);

                                  
  StorePartitionBound(attachrel, rel, cmd->bound);

                                                                      
  AttachPartitionEnsureIndexes(rel, attachrel);

                    
  CloneRowTriggersToPartition(rel, attachrel);

     
                                                                          
                                          
     
  CloneForeignKeyConstraints(wqueue, rel, attachrel);

     
                                                                           
                                                                   
                         
     
  partBoundConstraint = get_qual_from_partbound(attachrel, rel, cmd->bound);
  partConstraint = list_concat(partBoundConstraint, RelationGetPartitionQual(rel));

                                                                
  if (partConstraint)
  {
       
                                                                       
                                                                      
                                                            
       
    partConstraint = (List *)eval_const_expressions(NULL, (Node *)partConstraint);

                                   
    partConstraint = list_make1(make_ands_explicit(partConstraint));

       
                                                                           
                
       
    partConstraint = map_partition_varattnos(partConstraint, 1, attachrel, rel, &found_whole_row);
                                                       
    if (found_whole_row)
    {
      elog(ERROR, "unexpected whole-row reference found in partition key");
    }

                                                                          
    QueuePartitionConstraintValidation(wqueue, attachrel, partConstraint, false);
  }

     
                                                                           
                                                                             
                                                                             
                                                                           
                    
     
  if (OidIsValid(defaultPartOid))
  {
    Relation defaultrel;
    List *defPartConstraint;

    Assert(!cmd->bound->is_default);

                                                         
    defaultrel = table_open(defaultPartOid, NoLock);
    defPartConstraint = get_proposed_default_constraint(partBoundConstraint);

       
                                                                      
                     
       
    defPartConstraint = map_partition_varattnos(defPartConstraint, 1, defaultrel, rel, NULL);
    QueuePartitionConstraintValidation(wqueue, defaultrel, defPartConstraint, true);

                                     
    table_close(defaultrel, NoLock);
  }

  ObjectAddressSet(address, RelationRelationId, RelationGetRelid(attachrel));

     
                                                                         
                                                                     
                                                                          
                                        
     
  if (attachrel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ListCell *l;

    foreach (l, attachrel_children)
    {
      CacheInvalidateRelcacheByRelid(lfirst_oid(l));
    }
  }

                                  
  table_close(attachrel, NoLock);

  return address;
}

   
                                
                                                                 
   
                                                                                
                                                                               
                      
   
static void
AttachPartitionEnsureIndexes(Relation rel, Relation attachrel)
{
  List *idxes;
  List *attachRelIdxs;
  Relation *attachrelIdxRels;
  IndexInfo **attachInfos;
  int i;
  ListCell *cell;
  MemoryContext cxt;
  MemoryContext oldcxt;

  cxt = AllocSetContextCreate(CurrentMemoryContext, "AttachPartitionEnsureIndexes", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(cxt);

  idxes = RelationGetIndexList(rel);
  attachRelIdxs = RelationGetIndexList(attachrel);
  attachrelIdxRels = palloc(sizeof(Relation) * list_length(attachRelIdxs));
  attachInfos = palloc(sizeof(IndexInfo *) * list_length(attachRelIdxs));

                                                                 
  i = 0;
  foreach (cell, attachRelIdxs)
  {
    Oid cldIdxId = lfirst_oid(cell);

    attachrelIdxRels[i] = index_open(cldIdxId, AccessShareLock);
    attachInfos[i] = BuildIndexInfo(attachrelIdxRels[i]);
    i++;
  }

     
                                                                            
                                                                            
                                                                         
                                                           
     
  if (attachrel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    foreach (cell, idxes)
    {
      Oid idx = lfirst_oid(cell);
      Relation idxRel = index_open(idx, AccessShareLock);

      if (idxRel->rd_index->indisunique || idxRel->rd_index->indisprimary)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot attach foreign table \"%s\" as partition of partitioned table \"%s\"", RelationGetRelationName(attachrel), RelationGetRelationName(rel)), errdetail("Table \"%s\" contains unique indexes.", RelationGetRelationName(rel))));
      }
      index_close(idxRel, AccessShareLock);
    }

    goto out;
  }

     
                                                                         
                                                       
     
  foreach (cell, idxes)
  {
    Oid idx = lfirst_oid(cell);
    Relation idxRel = index_open(idx, AccessShareLock);
    IndexInfo *info;
    AttrNumber *attmap;
    bool found = false;
    Oid constraintOid;

       
                                                                      
                
       
    if (idxRel->rd_rel->relkind != RELKIND_PARTITIONED_INDEX)
    {
      index_close(idxRel, AccessShareLock);
      continue;
    }

                                                                    
    info = BuildIndexInfo(idxRel);
    attmap = convert_tuples_by_name_map(RelationGetDescr(attachrel), RelationGetDescr(rel), gettext_noop("could not convert row type"));
    constraintOid = get_relation_idx_constraint_oid(RelationGetRelid(rel), idx);

       
                                                                          
                                                                           
                                                      
       
    for (i = 0; i < list_length(attachRelIdxs); i++)
    {
      Oid cldIdxId = RelationGetRelid(attachrelIdxRels[i]);
      Oid cldConstrOid = InvalidOid;

                                                               
      if (attachrelIdxRels[i]->rd_rel->relispartition)
      {
        continue;
      }

      if (CompareIndexInfo(attachInfos[i], info, attachrelIdxRels[i]->rd_indcollation, idxRel->rd_indcollation, attachrelIdxRels[i]->rd_opfamily, idxRel->rd_opfamily, attmap, RelationGetDescr(rel)->natts))
      {
           
                                                                     
                                                                       
                                                                  
                                              
           
        if (OidIsValid(constraintOid))
        {
          cldConstrOid = get_relation_idx_constraint_oid(RelationGetRelid(attachrel), cldIdxId);
                       
          if (!OidIsValid(cldConstrOid))
          {
            continue;
          }
        }

                    
        IndexSetParentIndex(attachrelIdxRels[i], idx);
        if (OidIsValid(constraintOid))
        {
          ConstraintSetParentConstraint(cldConstrOid, constraintOid, RelationGetRelid(attachrel));
        }
        found = true;

        CommandCounterIncrement();
        break;
      }
    }

       
                                                                         
            
       
    if (!found)
    {
      IndexStmt *stmt;
      Oid constraintOid;

      stmt = generateClonedIndexStmt(NULL, idxRel, attmap, RelationGetDescr(rel)->natts, &constraintOid);
      DefineIndex(RelationGetRelid(attachrel), stmt, InvalidOid, RelationGetRelid(idxRel), constraintOid, true, false, false, false, false);
    }

    index_close(idxRel, AccessShareLock);
  }

out:
                 
  for (i = 0; i < list_length(attachRelIdxs); i++)
  {
    index_close(attachrelIdxRels[i], AccessShareLock);
  }
  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(cxt);
}

   
                      
                                                                  
                                                        
   
                                                                        
                                                                      
                                                                              
   
static bool
isPartitionTrigger(Oid trigger_oid)
{
  Relation pg_depend;
  ScanKeyData key[2];
  SysScanDesc scan;
  HeapTuple tup;
  bool found = false;

  pg_depend = table_open(DependRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_depend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(TriggerRelationId));
  ScanKeyInit(&key[1], Anum_pg_depend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(trigger_oid));

  scan = systable_beginscan(pg_depend, DependDependerIndexId, true, NULL, 2, key);
  while ((tup = systable_getnext(scan)) != NULL)
  {
    Form_pg_depend dep = (Form_pg_depend)GETSTRUCT(tup);

    if (dep->refclassid == TriggerRelationId)
    {
      found = true;
      break;
    }
  }

  systable_endscan(scan);
  table_close(pg_depend, AccessShareLock);

  return found;
}

   
                               
                                                                      
                           
   
static void
CloneRowTriggersToPartition(Relation parent, Relation partition)
{
  Relation pg_trigger;
  ScanKeyData key;
  SysScanDesc scan;
  HeapTuple tuple;
  MemoryContext perTupCxt;

  ScanKeyInit(&key, Anum_pg_trigger_tgrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(parent)));
  pg_trigger = table_open(TriggerRelationId, RowExclusiveLock);
  scan = systable_beginscan(pg_trigger, TriggerRelidNameIndexId, true, NULL, 1, &key);

  perTupCxt = AllocSetContextCreate(CurrentMemoryContext, "clone trig", ALLOCSET_SMALL_SIZES);

  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    Form_pg_trigger trigForm = (Form_pg_trigger)GETSTRUCT(tuple);
    CreateTrigStmt *trigStmt;
    Node *qual = NULL;
    Datum value;
    bool isnull;
    List *cols = NIL;
    List *trigargs = NIL;
    MemoryContext oldcxt;

       
                                                              
       
    if (!TRIGGER_FOR_ROW(trigForm->tgtype))
    {
      continue;
    }

       
                                                                         
                   
       
                                                                        
                                                                   
                                                                          
       
                                                                    
                                                                      
                      
       
    if (trigForm->tgisinternal && (!parent->rd_rel->relispartition || !isPartitionTrigger(trigForm->oid)))
    {
      continue;
    }

       
                                                       
       
    if (!TRIGGER_FOR_AFTER(trigForm->tgtype))
    {
      elog(ERROR, "unexpected trigger \"%s\" found", NameStr(trigForm->tgname));
    }

                                                    
    oldcxt = MemoryContextSwitchTo(perTupCxt);

       
                                                                           
                                      
       
    value = heap_getattr(tuple, Anum_pg_trigger_tgqual, RelationGetDescr(pg_trigger), &isnull);
    if (!isnull)
    {
      bool found_whole_row;

      qual = stringToNode(TextDatumGetCString(value));
      qual = (Node *)map_partition_varattnos((List *)qual, PRS2_OLD_VARNO, partition, parent, &found_whole_row);
      if (found_whole_row)
      {
        elog(ERROR, "unexpected whole-row reference found in trigger WHEN clause");
      }
      qual = (Node *)map_partition_varattnos((List *)qual, PRS2_NEW_VARNO, partition, parent, &found_whole_row);
      if (found_whole_row)
      {
        elog(ERROR, "unexpected whole-row reference found in trigger WHEN clause");
      }
    }

       
                                                                          
                                                          
       
    if (trigForm->tgattr.dim1 > 0)
    {
      int i;

      for (i = 0; i < trigForm->tgattr.dim1; i++)
      {
        Form_pg_attribute col;

        col = TupleDescAttr(parent->rd_att, trigForm->tgattr.values[i] - 1);
        cols = lappend(cols, makeString(pstrdup(NameStr(col->attname))));
      }
    }

                                             
    if (trigForm->tgnargs > 0)
    {
      char *p;

      value = heap_getattr(tuple, Anum_pg_trigger_tgargs, RelationGetDescr(pg_trigger), &isnull);
      if (isnull)
      {
        elog(ERROR, "tgargs is null for trigger \"%s\" in partition \"%s\"", NameStr(trigForm->tgname), RelationGetRelationName(partition));
      }

      p = (char *)VARDATA_ANY(DatumGetByteaPP(value));

      for (int i = 0; i < trigForm->tgnargs; i++)
      {
        trigargs = lappend(trigargs, makeString(pstrdup(p)));
        p += strlen(p) + 1;
      }
    }

    trigStmt = makeNode(CreateTrigStmt);
    trigStmt->trigname = NameStr(trigForm->tgname);
    trigStmt->relation = NULL;
    trigStmt->funcname = NULL;                        
    trigStmt->args = trigargs;
    trigStmt->row = true;
    trigStmt->timing = trigForm->tgtype & TRIGGER_TYPE_TIMING_MASK;
    trigStmt->events = trigForm->tgtype & TRIGGER_TYPE_EVENT_MASK;
    trigStmt->columns = cols;
    trigStmt->whenClause = NULL;                        
    trigStmt->isconstraint = OidIsValid(trigForm->tgconstraint);
    trigStmt->transitionRels = NIL;                               
    trigStmt->deferrable = trigForm->tgdeferrable;
    trigStmt->initdeferred = trigForm->tginitdeferred;
    trigStmt->constrrel = NULL;                        

    CreateTriggerFiringOn(trigStmt, NULL, RelationGetRelid(partition), trigForm->tgconstrrelid, InvalidOid, InvalidOid, trigForm->tgfoid, trigForm->oid, qual, false, true, trigForm->tgenabled);

    MemoryContextSwitchTo(oldcxt);
    MemoryContextReset(perTupCxt);
  }

  MemoryContextDelete(perTupCxt);

  systable_endscan(scan);
  table_close(pg_trigger, RowExclusiveLock);
}

   
                                
   
                                                                            
   
static ObjectAddress
ATExecDetachPartition(Relation rel, RangeVar *name)
{
  Relation partRel, classRel;
  HeapTuple tuple, newtuple;
  Datum new_val[Natts_pg_class];
  bool new_null[Natts_pg_class], new_repl[Natts_pg_class];
  ObjectAddress address;
  Oid defaultPartOid;
  List *indexes;
  List *fks;
  ListCell *cell;

     
                                                                          
                                           
     
  defaultPartOid = get_default_oid_from_partdesc(RelationGetPartitionDesc(rel));
  if (OidIsValid(defaultPartOid))
  {
    LockRelationOid(defaultPartOid, AccessExclusiveLock);
  }

  partRel = table_openrv(name, ShareUpdateExclusiveLock);

                                                             
  ATDetachCheckNoForeignKeyRefs(partRel);

                                                                        
  RemoveInheritance(partRel, rel);

                             
  classRel = table_open(RelationRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(RelationGetRelid(partRel)));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", RelationGetRelid(partRel));
  }
  Assert(((Form_pg_class)GETSTRUCT(tuple))->relispartition);

                                                   
  memset(new_val, 0, sizeof(new_val));
  memset(new_null, false, sizeof(new_null));
  memset(new_repl, false, sizeof(new_repl));
  new_val[Anum_pg_class_relpartbound - 1] = (Datum)0;
  new_null[Anum_pg_class_relpartbound - 1] = true;
  new_repl[Anum_pg_class_relpartbound - 1] = true;
  newtuple = heap_modify_tuple(tuple, RelationGetDescr(classRel), new_val, new_null, new_repl);

  ((Form_pg_class)GETSTRUCT(newtuple))->relispartition = false;
  CatalogTupleUpdate(classRel, &newtuple->t_self, newtuple);
  heap_freetuple(newtuple);

  if (OidIsValid(defaultPartOid))
  {
       
                                                                       
                                                               
       
                                                                         
                                                                         
                                               
       
    if (RelationGetRelid(partRel) == defaultPartOid)
    {
      update_default_partition_oid(RelationGetRelid(rel), InvalidOid);
    }
    else
    {
      CacheInvalidateRelcacheByRelid(defaultPartOid);
    }
  }

                          
  indexes = RelationGetIndexList(partRel);
  foreach (cell, indexes)
  {
    Oid idxid = lfirst_oid(cell);
    Relation idx;
    Oid constrOid;

    if (!has_superclass(idxid))
    {
      continue;
    }

    Assert((IndexGetRelation(get_partition_parent(idxid), false) == RelationGetRelid(rel)));

    idx = index_open(idxid, AccessExclusiveLock);
    IndexSetParentIndex(idx, InvalidOid);

                                                                          
    constrOid = get_relation_idx_constraint_oid(RelationGetRelid(partRel), idxid);
    if (OidIsValid(constrOid))
    {
      ConstraintSetParentConstraint(constrOid, InvalidOid, InvalidOid);
    }

    index_close(idx, NoLock);
  }
  table_close(classRel, RowExclusiveLock);

                                                              
  DropClonedTriggersFromPartition(RelationGetRelid(partRel));

     
                                                                         
                                 
     
  fks = copyObject(RelationGetFKeyList(partRel));
  foreach (cell, fks)
  {
    ForeignKeyCacheInfo *fk = lfirst(cell);
    HeapTuple contup;
    Form_pg_constraint conform;
    Constraint *fkconstraint;

    contup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(fk->conoid));
    if (!HeapTupleIsValid(contup))
    {
      elog(ERROR, "cache lookup failed for constraint %u", fk->conoid);
    }
    conform = (Form_pg_constraint)GETSTRUCT(contup);

                                                  
    if (conform->contype != CONSTRAINT_FOREIGN || !OidIsValid(conform->conparentid))
    {
      ReleaseSysCache(contup);
      continue;
    }

                                                                    
    ConstraintSetParentConstraint(fk->conoid, InvalidOid, InvalidOid);

       
                                                                           
                                                                       
                                                            
       
    fkconstraint = makeNode(Constraint);
    fkconstraint->contype = CONSTRAINT_FOREIGN;
    fkconstraint->conname = pstrdup(NameStr(conform->conname));
    fkconstraint->deferrable = conform->condeferrable;
    fkconstraint->initdeferred = conform->condeferred;
    fkconstraint->location = -1;
    fkconstraint->pktable = NULL;
    fkconstraint->fk_attrs = NIL;
    fkconstraint->pk_attrs = NIL;
    fkconstraint->fk_matchtype = conform->confmatchtype;
    fkconstraint->fk_upd_action = conform->confupdtype;
    fkconstraint->fk_del_action = conform->confdeltype;
    fkconstraint->old_conpfeqop = NIL;
    fkconstraint->old_pktable_oid = InvalidOid;
    fkconstraint->skip_validation = false;
    fkconstraint->initially_valid = true;

    createForeignKeyActionTriggers(partRel, conform->confrelid, fkconstraint, fk->conoid, conform->conindid);

    ReleaseSysCache(contup);
  }
  list_free_deep(fks);

     
                                                                    
                                                                             
                                  
     
  foreach (cell, GetParentedForeignKeyRefs(partRel))
  {
    Oid constrOid = lfirst_oid(cell);
    ObjectAddress constraint;

    ConstraintSetParentConstraint(constrOid, InvalidOid, InvalidOid);
    deleteDependencyRecordsForClass(ConstraintRelationId, constrOid, ConstraintRelationId, DEPENDENCY_INTERNAL);
    CommandCounterIncrement();

    ObjectAddressSet(constraint, ConstraintRelationId, constrOid);
    performDeletion(&constraint, DROP_RESTRICT, 0);
  }
  CommandCounterIncrement();

     
                                                                         
                                           
     
  CacheInvalidateRelcache(rel);

     
                                                                         
                                                                     
                                                                     
                                                                       
                                
     
  if (partRel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    List *children;

    children = find_all_inheritors(RelationGetRelid(partRel), AccessExclusiveLock, NULL);
    foreach (cell, children)
    {
      CacheInvalidateRelcacheByRelid(lfirst_oid(cell));
    }
  }

  ObjectAddressSet(address, RelationRelationId, RelationGetRelid(partRel));

                                  
  table_close(partRel, NoLock);

  return address;
}

   
                                   
                                                                          
                                                                          
                                                      
   
static void
DropClonedTriggersFromPartition(Oid partitionId)
{
  ScanKeyData skey;
  SysScanDesc scan;
  HeapTuple trigtup;
  Relation tgrel;
  ObjectAddresses *objects;

  objects = new_object_addresses();

     
                                                             
     
  ScanKeyInit(&skey, Anum_pg_trigger_tgrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(partitionId));
  tgrel = table_open(TriggerRelationId, RowExclusiveLock);
  scan = systable_beginscan(tgrel, TriggerRelidNameIndexId, true, NULL, 1, &skey);
  while (HeapTupleIsValid(trigtup = systable_getnext(scan)))
  {
    Form_pg_trigger pg_trigger = (Form_pg_trigger)GETSTRUCT(trigtup);
    ObjectAddress trig;

                                             
    if (!isPartitionTrigger(pg_trigger->oid))
    {
      continue;
    }

       
                                                                          
                                          
       
    deleteDependencyRecordsForClass(TriggerRelationId, pg_trigger->oid, TriggerRelationId, DEPENDENCY_PARTITION_PRI);
    deleteDependencyRecordsForClass(TriggerRelationId, pg_trigger->oid, RelationRelationId, DEPENDENCY_PARTITION_SEC);

                                                  
    ObjectAddressSet(trig, TriggerRelationId, pg_trigger->oid);
    add_exact_object_address(&trig, objects);
  }

                                                                 
  CommandCounterIncrement();
  performMultipleDeletions(objects, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);

            
  free_object_addresses(objects);
  systable_endscan(scan);
  table_close(tgrel, RowExclusiveLock);
}

   
                                                                          
          
   
struct AttachIndexCallbackState
{
  Oid partitionOid;
  Oid parentTblOid;
  bool lockedParentTbl;
};

static void
RangeVarCallbackForAttachIndex(const RangeVar *rv, Oid relOid, Oid oldRelOid, void *arg)
{
  struct AttachIndexCallbackState *state;
  Form_pg_class classform;
  HeapTuple tuple;

  state = (struct AttachIndexCallbackState *)arg;

  if (!state->lockedParentTbl)
  {
    LockRelationOid(state->parentTblOid, AccessShareLock);
    state->lockedParentTbl = true;
  }

     
                                                                            
                                                                            
                                                                            
                                                  
     
  if (relOid != oldRelOid && OidIsValid(state->partitionOid))
  {
    UnlockRelationOid(state->partitionOid, AccessShareLock);
    state->partitionOid = InvalidOid;
  }

                                                                            
  if (!OidIsValid(relOid))
  {
    return;
  }

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relOid));
  if (!HeapTupleIsValid(tuple))
  {
    return;                                             
  }
  classform = (Form_pg_class)GETSTRUCT(tuple);
  if (classform->relkind != RELKIND_PARTITIONED_INDEX && classform->relkind != RELKIND_INDEX)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("\"%s\" is not an index", rv->relname)));
  }
  ReleaseSysCache(tuple);

     
                                                                           
                                               
     
  state->partitionOid = IndexGetRelation(relOid, false);
  LockRelationOid(state->partitionOid, AccessShareLock);
}

   
                                      
   
static ObjectAddress
ATExecAttachPartitionIdx(List **wqueue, Relation parentIdx, RangeVar *name)
{
  Relation partIdx;
  Relation partTbl;
  Relation parentTbl;
  ObjectAddress address;
  Oid partIdxId;
  Oid currParent;
  struct AttachIndexCallbackState state;

     
                                                                          
                                                                            
                                                                            
                                                                         
                                      
     
  state.partitionOid = InvalidOid;
  state.parentTblOid = parentIdx->rd_index->indrelid;
  state.lockedParentTbl = false;
  partIdxId = RangeVarGetRelidExtended(name, AccessExclusiveLock, 0, RangeVarCallbackForAttachIndex, (void *)&state);
                  
  if (!OidIsValid(partIdxId))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("index \"%s\" does not exist", name->relname)));
  }

                                                                            
  partIdx = relation_open(partIdxId, AccessExclusiveLock);

                                                              
  parentTbl = relation_open(parentIdx->rd_index->indrelid, AccessShareLock);
  partTbl = relation_open(partIdx->rd_index->indrelid, NoLock);

  ObjectAddressSet(address, RelationRelationId, RelationGetRelid(partIdx));

                                                         
  currParent = partIdx->rd_rel->relispartition ? get_partition_parent(partIdxId) : InvalidOid;
  if (currParent != RelationGetRelid(parentIdx))
  {
    IndexInfo *childInfo;
    IndexInfo *parentInfo;
    AttrNumber *attmap;
    bool found;
    int i;
    PartitionDesc partDesc;
    Oid constraintOid, cldConstrId = InvalidOid;

       
                                                                   
                  
       
    refuseDupeIndexAttach(parentIdx, partIdx, partTbl);

    if (OidIsValid(currParent))
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot attach index \"%s\" as a partition of index \"%s\"", RelationGetRelationName(partIdx), RelationGetRelationName(parentIdx)), errdetail("Index \"%s\" is already attached to another index.", RelationGetRelationName(partIdx))));
    }

                                                                     
    partDesc = RelationGetPartitionDesc(parentTbl);
    found = false;
    for (i = 0; i < partDesc->nparts; i++)
    {
      if (partDesc->oids[i] == state.partitionOid)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot attach index \"%s\" as a partition of index \"%s\"", RelationGetRelationName(partIdx), RelationGetRelationName(parentIdx)), errdetail("Index \"%s\" is not an index on any partition of table \"%s\".", RelationGetRelationName(partIdx), RelationGetRelationName(parentTbl))));
    }

                                           
    childInfo = BuildIndexInfo(partIdx);
    parentInfo = BuildIndexInfo(parentIdx);
    attmap = convert_tuples_by_name_map(RelationGetDescr(partTbl), RelationGetDescr(parentTbl), gettext_noop("could not convert row type"));
    if (!CompareIndexInfo(childInfo, parentInfo, partIdx->rd_indcollation, parentIdx->rd_indcollation, partIdx->rd_opfamily, parentIdx->rd_opfamily, attmap, RelationGetDescr(parentTbl)->natts))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot attach index \"%s\" as a partition of index \"%s\"", RelationGetRelationName(partIdx), RelationGetRelationName(parentIdx)), errdetail("The index definitions do not match.")));
    }

       
                                                                         
                      
       
    constraintOid = get_relation_idx_constraint_oid(RelationGetRelid(parentTbl), RelationGetRelid(parentIdx));

    if (OidIsValid(constraintOid))
    {
      cldConstrId = get_relation_idx_constraint_oid(RelationGetRelid(partTbl), partIdxId);
      if (!OidIsValid(cldConstrId))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot attach index \"%s\" as a partition of index \"%s\"", RelationGetRelationName(partIdx), RelationGetRelationName(parentIdx)), errdetail("The index \"%s\" belongs to a constraint in table \"%s\" but no constraint exists for index \"%s\".", RelationGetRelationName(parentIdx), RelationGetRelationName(parentTbl), RelationGetRelationName(partIdx))));
      }
    }

                           
    IndexSetParentIndex(partIdx, RelationGetRelid(parentIdx));
    if (OidIsValid(constraintOid))
    {
      ConstraintSetParentConstraint(cldConstrId, constraintOid, RelationGetRelid(partTbl));
    }

    pfree(attmap);

    validatePartitionedIndex(parentIdx, parentTbl);
  }

  relation_close(parentTbl, AccessShareLock);
                                    
  relation_close(partTbl, NoLock);
  relation_close(partIdx, NoLock);

  return address;
}

   
                                                                         
                                                           
   
static void
refuseDupeIndexAttach(Relation parentIdx, Relation partIdx, Relation partitionTbl)
{
  Oid existingIdx;

  existingIdx = index_get_partition(partitionTbl, RelationGetRelid(parentIdx));
  if (OidIsValid(existingIdx))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot attach index \"%s\" as a partition of index \"%s\"", RelationGetRelationName(partIdx), RelationGetRelationName(parentIdx)), errdetail("Another index is already attached for partition \"%s\".", RelationGetRelationName(partitionTbl))));
  }
}

   
                                                                             
                                                                            
   
                                                                  
   
static void
validatePartitionedIndex(Relation partedIdx, Relation partedTbl)
{
  Relation inheritsRel;
  SysScanDesc scan;
  ScanKeyData key;
  int tuples = 0;
  HeapTuple inhTup;
  bool updated = false;

  Assert(partedIdx->rd_rel->relkind == RELKIND_PARTITIONED_INDEX);

     
                                                                             
                                                                        
                                                               
     
  inheritsRel = table_open(InheritsRelationId, AccessShareLock);
  ScanKeyInit(&key, Anum_pg_inherits_inhparent, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(partedIdx)));
  scan = systable_beginscan(inheritsRel, InheritsParentIndexId, true, NULL, 1, &key);
  while ((inhTup = systable_getnext(scan)) != NULL)
  {
    Form_pg_inherits inhForm = (Form_pg_inherits)GETSTRUCT(inhTup);
    HeapTuple indTup;
    Form_pg_index indexForm;

    indTup = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(inhForm->inhrelid));
    if (!HeapTupleIsValid(indTup))
    {
      elog(ERROR, "cache lookup failed for index %u", inhForm->inhrelid);
    }
    indexForm = (Form_pg_index)GETSTRUCT(indTup);
    if (indexForm->indisvalid)
    {
      tuples += 1;
    }
    ReleaseSysCache(indTup);
  }

                             
  systable_endscan(scan);
  table_close(inheritsRel, AccessShareLock);

     
                                                                        
                                                                
     
  if (tuples == RelationGetPartitionDesc(partedTbl)->nparts)
  {
    Relation idxRel;
    HeapTuple newtup;

    idxRel = table_open(IndexRelationId, RowExclusiveLock);

    newtup = heap_copytuple(partedIdx->rd_indextuple);
    ((Form_pg_index)GETSTRUCT(newtup))->indisvalid = true;
    updated = true;

    CatalogTupleUpdate(idxRel, &partedIdx->rd_indextuple->t_self, newtup);

    table_close(idxRel, RowExclusiveLock);
  }

     
                                                                           
                                                             
     
  if (updated && partedIdx->rd_rel->relispartition)
  {
    Oid parentIdxId, parentTblId;
    Relation parentIdx, parentTbl;

                                                     
    CommandCounterIncrement();

    parentIdxId = get_partition_parent(RelationGetRelid(partedIdx));
    parentTblId = get_partition_parent(RelationGetRelid(partedTbl));
    parentIdx = relation_open(parentIdxId, AccessExclusiveLock);
    parentTbl = relation_open(parentTblId, AccessExclusiveLock);
    Assert(!parentIdx->rd_index->indisvalid);

    validatePartitionedIndex(parentIdx, parentTbl);

    relation_close(parentIdx, AccessExclusiveLock);
    relation_close(parentTbl, AccessExclusiveLock);
  }
}

   
                                                                       
                                                   
   
static List *
GetParentedForeignKeyRefs(Relation partition)
{
  Relation pg_constraint;
  HeapTuple tuple;
  SysScanDesc scan;
  ScanKeyData key[2];
  List *constraints = NIL;

     
                                                                             
           
     
  if (RelationGetIndexList(partition) == NIL || bms_is_empty(RelationGetIndexAttrBitmap(partition, INDEX_ATTR_BITMAP_KEY)))
  {
    return NIL;
  }

                                                     
  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);
  ScanKeyInit(&key[0], Anum_pg_constraint_confrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(partition)));
  ScanKeyInit(&key[1], Anum_pg_constraint_contype, BTEqualStrategyNumber, F_CHAREQ, CharGetDatum(CONSTRAINT_FOREIGN));

                                                              
  scan = systable_beginscan(pg_constraint, InvalidOid, true, NULL, 2, key);
  while ((tuple = systable_getnext(scan)) != NULL)
  {
    Form_pg_constraint constrForm = (Form_pg_constraint)GETSTRUCT(tuple);

       
                                                                         
       
    if (!OidIsValid(constrForm->conparentid))
    {
      continue;
    }

    constraints = lappend_oid(constraints, constrForm->oid);
  }

  systable_endscan(scan);
  table_close(pg_constraint, AccessShareLock);

  return constraints;
}

   
                                                                         
                                                                          
                            
   
static void
ATDetachCheckNoForeignKeyRefs(Relation partition)
{
  List *constraints;
  ListCell *cell;

  constraints = GetParentedForeignKeyRefs(partition);

  foreach (cell, constraints)
  {
    Oid constrOid = lfirst_oid(cell);
    HeapTuple tuple;
    Form_pg_constraint constrForm;
    Relation rel;
    Trigger trig;

    tuple = SearchSysCache1(CONSTROID, ObjectIdGetDatum(constrOid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for constraint %u", constrOid);
    }
    constrForm = (Form_pg_constraint)GETSTRUCT(tuple);

    Assert(OidIsValid(constrForm->conparentid));
    Assert(constrForm->confrelid == RelationGetRelid(partition));

                                                                      
    rel = table_open(constrForm->conrelid, ShareLock);

    MemSet(&trig, 0, sizeof(trig));
    trig.tgoid = InvalidOid;
    trig.tgname = NameStr(constrForm->conname);
    trig.tgenabled = TRIGGER_FIRES_ON_ORIGIN;
    trig.tgisinternal = true;
    trig.tgconstrrelid = RelationGetRelid(partition);
    trig.tgconstrindid = constrForm->conindid;
    trig.tgconstraint = constrForm->oid;
    trig.tgdeferrable = false;
    trig.tginitdeferred = false;
                                             

    RI_PartitionRemove_Check(&trig, rel, partition);

    ReleaseSysCache(tuple);

    table_close(rel, NoLock);
  }
}
