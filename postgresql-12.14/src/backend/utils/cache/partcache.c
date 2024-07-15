                                                                            
   
               
                                                                      
             
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/relation.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_partitioned_table.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "partitioning/partbounds.h"
#include "rewrite/rewriteHandler.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static List *
generate_partition_qual(Relation rel);

   
                             
                                                                 
   
                                                                               
                                                                               
                                                                              
                                                                              
                                                                              
                                                                             
                                                                              
                                                                               
                                                                               
                
   
void
RelationBuildPartitionKey(Relation relation)
{
  Form_pg_partitioned_table form;
  HeapTuple tuple;
  bool isnull;
  int i;
  PartitionKey key;
  AttrNumber *attrs;
  oidvector *opclass;
  oidvector *collation;
  ListCell *partexprs_item;
  Datum datum;
  MemoryContext partkeycxt, oldcxt;
  int16 procnum;

  tuple = SearchSysCache1(PARTRELID, ObjectIdGetDatum(RelationGetRelid(relation)));

     
                                                                           
                                         
     
  if (!HeapTupleIsValid(tuple))
  {
    return;
  }

  partkeycxt = AllocSetContextCreate(CurTransactionContext, "partition key", ALLOCSET_SMALL_SIZES);
  MemoryContextCopyAndSetIdentifier(partkeycxt, RelationGetRelationName(relation));

  key = (PartitionKey)MemoryContextAllocZero(partkeycxt, sizeof(PartitionKeyData));

                               
  form = (Form_pg_partitioned_table)GETSTRUCT(tuple);
  key->strategy = form->partstrat;
  key->partnatts = form->partnatts;

     
                                                                            
                                                                    
                                                   
     
  attrs = form->partattrs.values;

                                                                           
                      
  datum = SysCacheGetAttr(PARTRELID, tuple, Anum_pg_partitioned_table_partclass, &isnull);
  Assert(!isnull);
  opclass = (oidvector *)DatumGetPointer(datum);

                 
  datum = SysCacheGetAttr(PARTRELID, tuple, Anum_pg_partitioned_table_partcollation, &isnull);
  Assert(!isnull);
  collation = (oidvector *)DatumGetPointer(datum);

                   
  datum = SysCacheGetAttr(PARTRELID, tuple, Anum_pg_partitioned_table_partexprs, &isnull);
  if (!isnull)
  {
    char *exprString;
    Node *expr;

    exprString = TextDatumGetCString(datum);
    expr = stringToNode(exprString);
    pfree(exprString);

       
                                                                          
                                                                           
                                                                   
                                                            
                                                                           
                                                                         
                     
       
    expr = eval_const_expressions(NULL, expr);
    fix_opfuncids(expr);

    oldcxt = MemoryContextSwitchTo(partkeycxt);
    key->partexprs = (List *)copyObject(expr);
    MemoryContextSwitchTo(oldcxt);
  }

                                                                          
  oldcxt = MemoryContextSwitchTo(partkeycxt);
  key->partattrs = (AttrNumber *)palloc0(key->partnatts * sizeof(AttrNumber));
  key->partopfamily = (Oid *)palloc0(key->partnatts * sizeof(Oid));
  key->partopcintype = (Oid *)palloc0(key->partnatts * sizeof(Oid));
  key->partsupfunc = (FmgrInfo *)palloc0(key->partnatts * sizeof(FmgrInfo));

  key->partcollation = (Oid *)palloc0(key->partnatts * sizeof(Oid));
  key->parttypid = (Oid *)palloc0(key->partnatts * sizeof(Oid));
  key->parttypmod = (int32 *)palloc0(key->partnatts * sizeof(int32));
  key->parttyplen = (int16 *)palloc0(key->partnatts * sizeof(int16));
  key->parttypbyval = (bool *)palloc0(key->partnatts * sizeof(bool));
  key->parttypalign = (char *)palloc0(key->partnatts * sizeof(char));
  key->parttypcoll = (Oid *)palloc0(key->partnatts * sizeof(Oid));
  MemoryContextSwitchTo(oldcxt);

                                                       
  procnum = (key->strategy == PARTITION_STRATEGY_HASH) ? HASHEXTENDED_PROC : BTORDER_PROC;

                                                        
  memcpy(key->partattrs, attrs, key->partnatts * sizeof(int16));
  partexprs_item = list_head(key->partexprs);
  for (i = 0; i < key->partnatts; i++)
  {
    AttrNumber attno = key->partattrs[i];
    HeapTuple opclasstup;
    Form_pg_opclass opclassform;
    Oid funcid;

                                      
    opclasstup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass->values[i]));
    if (!HeapTupleIsValid(opclasstup))
    {
      elog(ERROR, "cache lookup failed for opclass %u", opclass->values[i]);
    }

    opclassform = (Form_pg_opclass)GETSTRUCT(opclasstup);
    key->partopfamily[i] = opclassform->opcfamily;
    key->partopcintype[i] = opclassform->opcintype;

                                                                         
    funcid = get_opfamily_proc(opclassform->opcfamily, opclassform->opcintype, opclassform->opcintype, procnum);
    if (!OidIsValid(funcid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator class \"%s\" of access method %s is missing support function %d for type %s", NameStr(opclassform->opcname), (key->strategy == PARTITION_STRATEGY_HASH) ? "hash" : "btree", procnum, format_type_be(opclassform->opcintype))));
    }

    fmgr_info_cxt(funcid, &key->partsupfunc[i], partkeycxt);

                   
    key->partcollation[i] = collation->values[i];

                                  
    if (attno != 0)
    {
      Form_pg_attribute att = TupleDescAttr(relation->rd_att, attno - 1);

      key->parttypid[i] = att->atttypid;
      key->parttypmod[i] = att->atttypmod;
      key->parttypcoll[i] = att->attcollation;
    }
    else
    {
      if (partexprs_item == NULL)
      {
        elog(ERROR, "wrong number of partition key expressions");
      }

      key->parttypid[i] = exprType(lfirst(partexprs_item));
      key->parttypmod[i] = exprTypmod(lfirst(partexprs_item));
      key->parttypcoll[i] = exprCollation(lfirst(partexprs_item));

      partexprs_item = lnext(partexprs_item);
    }
    get_typlenbyvalalign(key->parttypid[i], &key->parttyplen[i], &key->parttypbyval[i], &key->parttypalign[i]);

    ReleaseSysCache(opclasstup);
  }

  ReleaseSysCache(tuple);

                                                                           
  Assert(relation->rd_partkeycxt == NULL);
  Assert(relation->rd_partkey == NULL);

     
                                                                         
                           
     
  MemoryContextSetParent(partkeycxt, CacheMemoryContext);
  relation->rd_partkeycxt = partkeycxt;
  relation->rd_partkey = key;
}

   
                            
   
                                     
   
List *
RelationGetPartitionQual(Relation rel)
{
                  
  if (!rel->rd_rel->relispartition)
  {
    return NIL;
  }

  return generate_partition_qual(rel);
}

   
                            
   
                                                                            
               
   
                                                                       
                                                                           
                                                                            
                                                                           
                                    
   
Expr *
get_partition_qual_relid(Oid relid)
{
  Expr *result = NULL;

                                                                    
  if (get_rel_relispartition(relid))
  {
    Relation rel = relation_open(relid, AccessShareLock);
    List *and_args;

    and_args = generate_partition_qual(rel);

                                                                
    if (and_args == NIL)
    {
      result = NULL;
    }
    else if (list_length(and_args) > 1)
    {
      result = makeBoolExpr(AND_EXPR, and_args, -1);
    }
    else
    {
      result = linitial(and_args);
    }

                                                                           
    relation_close(rel, NoLock);
  }

  return result;
}

   
                           
   
                                                                           
                                                         
   
                                                                           
                                                                         
                                                                          
   
static List *
generate_partition_qual(Relation rel)
{
  HeapTuple tuple;
  MemoryContext oldcxt;
  Datum boundDatum;
  bool isnull;
  List *my_qual = NIL, *result = NIL;
  Relation parent;
  bool found_whole_row;

                                                                      
  check_stack_depth();

                                                           
  if (rel->rd_partcheckvalid)
  {
    return copyObject(rel->rd_partcheck);
  }

                                                            
  parent = relation_open(get_partition_parent(RelationGetRelid(rel)), AccessShareLock);

                                 
  tuple = SearchSysCache1(RELOID, RelationGetRelid(rel));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", RelationGetRelid(rel));
  }

  boundDatum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relpartbound, &isnull);
  if (!isnull)
  {
    PartitionBoundSpec *bound;

    bound = castNode(PartitionBoundSpec, stringToNode(TextDatumGetCString(boundDatum)));

    my_qual = get_qual_from_partbound(rel, parent, bound);
  }

  ReleaseSysCache(tuple);

                                                   
  if (parent->rd_rel->relispartition)
  {
    result = list_concat(generate_partition_qual(parent), my_qual);
  }
  else
  {
    result = my_qual;
  }

     
                                                                           
                                                                             
                                                                         
           
     
  result = map_partition_varattnos(result, 1, rel, parent, &found_whole_row);
                                                     
  if (found_whole_row)
  {
    elog(ERROR, "unexpected whole-row reference found in partition key");
  }

                                                                           
  Assert(rel->rd_partcheckcxt == NULL);
  Assert(rel->rd_partcheck == NIL);

     
                                                                           
                                                                             
                                                           
     
                                                                          
                                            
     
  if (result != NIL)
  {
    rel->rd_partcheckcxt = AllocSetContextCreate(CacheMemoryContext, "partition constraint", ALLOCSET_SMALL_SIZES);
    MemoryContextCopyAndSetIdentifier(rel->rd_partcheckcxt, RelationGetRelationName(rel));
    oldcxt = MemoryContextSwitchTo(rel->rd_partcheckcxt);
    rel->rd_partcheck = copyObject(result);
    MemoryContextSwitchTo(oldcxt);
  }
  else
  {
    rel->rd_partcheck = NIL;
  }
  rel->rd_partcheckvalid = true;

                                           
  relation_close(parent, NoLock);

                                             
  return result;
}
