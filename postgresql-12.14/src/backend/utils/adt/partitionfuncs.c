                                                                            
   
                    
                                                        
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/partition.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                                                                        
                                                                        
                                                                        
                   
   
static bool
check_rel_can_be_partition(Oid relid)
{
  char relkind;
  bool relispartition;

                                
  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(relid)))
  {
    return false;
  }

  relkind = get_rel_relkind(relid);
  relispartition = get_rel_relispartition(relid);

                                                                     
  if (!relispartition && relkind != RELKIND_PARTITIONED_TABLE && relkind != RELKIND_PARTITIONED_INDEX)
  {
    return false;
  }

  return true;
}

   
                     
   
                                                                         
                                                                         
                                                                    
                                                    
   
Datum
pg_partition_tree(PG_FUNCTION_ARGS)
{
#define PG_PARTITION_TREE_COLS 4
  Oid rootrelid = PG_GETARG_OID(0);
  FuncCallContext *funcctx;
  ListCell **next;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcxt;
    TupleDesc tupdesc;
    List *partitions;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

    if (!check_rel_can_be_partition(rootrelid))
    {
      SRF_RETURN_DONE(funcctx);
    }

                                                                          
    oldcxt = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

       
                                                                          
                                                             
       
    partitions = find_all_inheritors(rootrelid, AccessShareLock, NULL);

    tupdesc = CreateTemplateTupleDesc(PG_PARTITION_TREE_COLS);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "relid", REGCLASSOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "parentid", REGCLASSOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "isleaf", BOOLOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)4, "level", INT4OID, -1, 0);

    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

                                          
    next = (ListCell **)palloc(sizeof(ListCell *));
    *next = list_head(partitions);
    funcctx->user_fctx = (void *)next;

    MemoryContextSwitchTo(oldcxt);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();
  next = (ListCell **)funcctx->user_fctx;

  if (*next != NULL)
  {
    Datum result;
    Datum values[PG_PARTITION_TREE_COLS];
    bool nulls[PG_PARTITION_TREE_COLS];
    HeapTuple tuple;
    Oid parentid = InvalidOid;
    Oid relid = lfirst_oid(*next);
    char relkind = get_rel_relkind(relid);
    int level = 0;
    List *ancestors = get_partition_ancestors(lfirst_oid(*next));
    ListCell *lc;

       
                                         
       
    MemSet(nulls, 0, sizeof(nulls));
    MemSet(values, 0, sizeof(values));

               
    values[0] = ObjectIdGetDatum(relid);

                  
    if (ancestors != NIL)
    {
      parentid = linitial_oid(ancestors);
    }
    if (OidIsValid(parentid))
    {
      values[1] = ObjectIdGetDatum(parentid);
    }
    else
    {
      nulls[1] = true;
    }

                
    values[2] = BoolGetDatum(relkind != RELKIND_PARTITIONED_TABLE && relkind != RELKIND_PARTITIONED_INDEX);

               
    if (relid != rootrelid)
    {
      foreach (lc, ancestors)
      {
        level++;
        if (lfirst_oid(lc) == rootrelid)
        {
          break;
        }
      }
    }
    values[3] = Int32GetDatum(level);

    *next = lnext(*next);

    tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    result = HeapTupleGetDatum(tuple);
    SRF_RETURN_NEXT(funcctx, result);
  }

                                                 
  SRF_RETURN_DONE(funcctx);
}

   
                     
   
                                                                      
                                                                    
                   
   
Datum
pg_partition_root(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  Oid rootrelid;
  List *ancestors;

  if (!check_rel_can_be_partition(relid))
  {
    PG_RETURN_NULL();
  }

                                   
  ancestors = get_partition_ancestors(relid);

     
                                                                       
             
     
  if (ancestors == NIL)
  {
    PG_RETURN_OID(relid);
  }

  rootrelid = llast_oid(ancestors);
  list_free(ancestors);

     
                                                                            
                                                     
     
  Assert(OidIsValid(rootrelid));
  PG_RETURN_OID(rootrelid);
}

   
                          
   
                                                                     
                                        
   
Datum
pg_partition_ancestors(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  FuncCallContext *funcctx;
  ListCell **next;

  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcxt;
    List *ancestors;

    funcctx = SRF_FIRSTCALL_INIT();

    if (!check_rel_can_be_partition(relid))
    {
      SRF_RETURN_DONE(funcctx);
    }

    oldcxt = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    ancestors = get_partition_ancestors(relid);
    ancestors = lcons_oid(relid, ancestors);

    next = (ListCell **)palloc(sizeof(ListCell *));
    *next = list_head(ancestors);
    funcctx->user_fctx = (void *)next;

    MemoryContextSwitchTo(oldcxt);
  }

  funcctx = SRF_PERCALL_SETUP();
  next = (ListCell **)funcctx->user_fctx;

  if (*next != NULL)
  {
    Oid relid = lfirst_oid(*next);

    *next = lnext(*next);
    SRF_RETURN_NEXT(funcctx, ObjectIdGetDatum(relid));
  }

  SRF_RETURN_DONE(funcctx);
}
