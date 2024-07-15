                                                                            
   
               
                                                          
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/tupconvert.h"
#include "access/sysattr.h"
#include "catalog/indexing.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_partitioned_table.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "partitioning/partbounds.h"
#include "rewrite/rewriteManip.h"
#include "utils/fmgroids.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
get_partition_parent_worker(Relation inhRel, Oid relid);
static void
get_partition_ancestors_worker(Relation inhRel, Oid relid, List **ancestors);

   
                        
                                           
   
                                                                     
   
                                                                             
                                                                           
                                                      
   
Oid
get_partition_parent(Oid relid)
{
  Relation catalogRelation;
  Oid result;

  catalogRelation = table_open(InheritsRelationId, AccessShareLock);

  result = get_partition_parent_worker(catalogRelation, relid);

  if (!OidIsValid(result))
  {
    elog(ERROR, "could not find tuple for parent of relation %u", relid);
  }

  table_close(catalogRelation, AccessShareLock);

  return result;
}

   
                               
                                                                         
                   
   
static Oid
get_partition_parent_worker(Relation inhRel, Oid relid)
{
  SysScanDesc scan;
  ScanKeyData key[2];
  Oid result = InvalidOid;
  HeapTuple tuple;

  ScanKeyInit(&key[0], Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  ScanKeyInit(&key[1], Anum_pg_inherits_inhseqno, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(1));

  scan = systable_beginscan(inhRel, InheritsRelidSeqnoIndexId, true, NULL, 2, key);
  tuple = systable_getnext(scan);
  if (HeapTupleIsValid(tuple))
  {
    Form_pg_inherits form = (Form_pg_inherits)GETSTRUCT(tuple);

    result = form->inhparent;
  }

  systable_endscan(scan);

  return result;
}

   
                           
                                       
   
                                                      
   
                                                                             
                                                                              
                                                                     
   
List *
get_partition_ancestors(Oid relid)
{
  List *result = NIL;
  Relation inhRel;

  inhRel = table_open(InheritsRelationId, AccessShareLock);

  get_partition_ancestors_worker(inhRel, relid, &result);

  table_close(inhRel, AccessShareLock);

  return result;
}

   
                                  
                                                 
   
static void
get_partition_ancestors_worker(Relation inhRel, Oid relid, List **ancestors)
{
  Oid parentOid;

                                                                        
  parentOid = get_partition_parent_worker(inhRel, relid);
  if (parentOid == InvalidOid)
  {
    return;
  }

  *ancestors = lappend_oid(*ancestors, parentOid);
  get_partition_ancestors_worker(inhRel, parentOid, ancestors);
}

   
                       
                                                                   
                                                          
   
Oid
index_get_partition(Relation partition, Oid indexId)
{
  List *idxlist = RelationGetIndexList(partition);
  ListCell *l;

  foreach (l, idxlist)
  {
    Oid partIdx = lfirst_oid(l);
    HeapTuple tup;
    Form_pg_class classForm;
    bool ispartition;

    tup = SearchSysCache1(RELOID, ObjectIdGetDatum(partIdx));
    if (!HeapTupleIsValid(tup))
    {
      elog(ERROR, "cache lookup failed for relation %u", partIdx);
    }
    classForm = (Form_pg_class)GETSTRUCT(tup);
    ispartition = classForm->relispartition;
    ReleaseSysCache(tup);
    if (!ispartition)
    {
      continue;
    }
    if (get_partition_parent(partIdx) == indexId)
    {
      list_free(idxlist);
      return partIdx;
    }
  }

  list_free(idxlist);
  return InvalidOid;
}

   
                                                                        
                                                                             
                                                                            
                                                 
   
                                                                             
                                                                            
                 
   
                                                                      
                                                         
   
                                                                            
                                                                           
                                                                        
   
List *
map_partition_varattnos(List *expr, int fromrel_varno, Relation to_rel, Relation from_rel, bool *found_whole_row)
{
  bool my_found_whole_row = false;

  if (expr != NIL)
  {
    AttrNumber *part_attnos;

    part_attnos = convert_tuples_by_name_map(RelationGetDescr(to_rel), RelationGetDescr(from_rel), gettext_noop("could not convert row type"));
    expr = (List *)map_variable_attnos((Node *)expr, fromrel_varno, 0, part_attnos, RelationGetDescr(from_rel)->natts, RelationGetForm(to_rel)->reltype, &my_found_whole_row);
  }

  if (found_whole_row)
  {
    *found_whole_row = my_found_whole_row;
  }

  return expr;
}

   
                                                                       
   
                                                                                
                                                                         
                                                                             
                                                                       
                                                                            
         
   
bool
has_partition_attrs(Relation rel, Bitmapset *attnums, bool *used_in_expr)
{
  PartitionKey key;
  int partnatts;
  List *partexprs;
  ListCell *partexprs_item;
  int i;

  if (attnums == NULL || rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    return false;
  }

  key = RelationGetPartitionKey(rel);
  partnatts = get_partition_natts(key);
  partexprs = get_partition_exprs(key);

  partexprs_item = list_head(partexprs);
  for (i = 0; i < partnatts; i++)
  {
    AttrNumber partattno = get_partition_col_attnum(key, i);

    if (partattno != 0)
    {
      if (bms_is_member(partattno - FirstLowInvalidHeapAttributeNumber, attnums))
      {
        if (used_in_expr)
        {
          *used_in_expr = false;
        }
        return true;
      }
    }
    else
    {
                                
      Node *expr = (Node *)lfirst(partexprs_item);
      Bitmapset *expr_attrs = NULL;

                                          
      pull_varattnos(expr, 1, &expr_attrs);
      partexprs_item = lnext(partexprs_item);

      if (bms_overlap(attnums, expr_attrs))
      {
        if (used_in_expr)
        {
          *used_in_expr = true;
        }
        return true;
      }
    }
  }

  return false;
}

   
                             
   
                                                                         
                                                                  
               
   
Oid
get_default_partition_oid(Oid parentId)
{
  HeapTuple tuple;
  Oid defaultPartId = InvalidOid;

  tuple = SearchSysCache1(PARTRELID, ObjectIdGetDatum(parentId));

  if (HeapTupleIsValid(tuple))
  {
    Form_pg_partitioned_table part_table_form;

    part_table_form = (Form_pg_partitioned_table)GETSTRUCT(tuple);
    defaultPartId = part_table_form->partdefid;
    ReleaseSysCache(tuple);
  }

  return defaultPartId;
}

   
                                
   
                                                                           
   
void
update_default_partition_oid(Oid parentId, Oid defaultPartId)
{
  HeapTuple tuple;
  Relation pg_partitioned_table;
  Form_pg_partitioned_table part_table_form;

  pg_partitioned_table = table_open(PartitionedRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(PARTRELID, ObjectIdGetDatum(parentId));

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for partition key of relation %u", parentId);
  }

  part_table_form = (Form_pg_partitioned_table)GETSTRUCT(tuple);
  part_table_form->partdefid = defaultPartId;
  CatalogTupleUpdate(pg_partitioned_table, &tuple->t_self, tuple);

  heap_freetuple(tuple);
  table_close(pg_partitioned_table, RowExclusiveLock);
}

   
                                   
   
                                                                     
                                                                        
                                                                        
   
List *
get_proposed_default_constraint(List *new_part_constraints)
{
  Expr *defPartConstraint;

  defPartConstraint = make_ands_explicit(new_part_constraints);

     
                                                                           
                                                                           
                                                
     
  defPartConstraint = makeBoolExpr(NOT_EXPR, list_make1(defPartConstraint), -1);

                                                                   
  defPartConstraint = (Expr *)eval_const_expressions(NULL, (Node *)defPartConstraint);
  defPartConstraint = canonicalize_qual(defPartConstraint, true);

  return make_ands_implicit(defPartConstraint);
}
