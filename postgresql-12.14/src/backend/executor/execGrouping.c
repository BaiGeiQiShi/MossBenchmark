                                                                            
   
                  
                                                                      
   
                                                                         
                                                                            
                                                                              
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/parallel.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "utils/lsyscache.h"
#include "utils/hashutils.h"
#include "utils/memutils.h"

static uint32
TupleHashTableHash(struct tuplehash_hash *tb, const MinimalTuple tuple);
static int
TupleHashTableMatch(struct tuplehash_hash *tb, const MinimalTuple tuple1, const MinimalTuple tuple2);

   
                                                                            
                                                                               
             
   
#define SH_PREFIX tuplehash
#define SH_ELEMENT_TYPE TupleHashEntryData
#define SH_KEY_TYPE MinimalTuple
#define SH_KEY firstTuple
#define SH_HASH_KEY(tb, key) TupleHashTableHash(tb, key)
#define SH_EQUAL(tb, a, b) TupleHashTableMatch(tb, a, b) == 0
#define SH_SCOPE extern
#define SH_STORE_HASH
#define SH_GET_HASH(tb, a) a->hash
#define SH_DEFINE
#include "lib/simplehash.h"

                                                                               
                                                  
                                                                               

   
                          
                                                                       
                                                                 
   
ExprState *
execTuplesMatchPrepare(TupleDesc desc, int numCols, const AttrNumber *keyColIdx, const Oid *eqOperators, const Oid *collations, PlanState *parent)
{
  Oid *eqFunctions = (Oid *)palloc(numCols * sizeof(Oid));
  int i;
  ExprState *expr;

  if (numCols == 0)
  {
    return NULL;
  }

                                 
  for (i = 0; i < numCols; i++)
  {
    eqFunctions[i] = get_opcode(eqOperators[i]);
  }

                               
  expr = ExecBuildGroupingEqual(desc, desc, NULL, NULL, numCols, keyColIdx, eqFunctions, collations, parent);

  return expr;
}

   
                         
                                                                            
   
                                                                           
                                                                            
                                                      
   
                                                                            
   
void
execTuplesHashPrepare(int numCols, const Oid *eqOperators, Oid **eqFuncOids, FmgrInfo **hashFunctions)
{
  int i;

  *eqFuncOids = (Oid *)palloc(numCols * sizeof(Oid));
  *hashFunctions = (FmgrInfo *)palloc(numCols * sizeof(FmgrInfo));

  for (i = 0; i < numCols; i++)
  {
    Oid eq_opr = eqOperators[i];
    Oid eq_function;
    Oid left_hash_function;
    Oid right_hash_function;

    eq_function = get_opcode(eq_opr);
    if (!get_op_hash_functions(eq_opr, &left_hash_function, &right_hash_function))
    {
      elog(ERROR, "could not find hash function for hash operator %u", eq_opr);
    }
                                                    
    Assert(left_hash_function == right_hash_function);
    (*eqFuncOids)[i] = eq_function;
    fmgr_info(right_hash_function, &(*hashFunctions)[i]);
  }
}

                                                                               
                                                   
   
                                                                          
                                                                              
              
                                                                               

   
                                     
   
                                                                      
                                                     
                                                             
                                                
                                                       
                                                                             
                                                            
                                                                             
   
                                                                            
                                                                         
                  
   
                                                                            
                                                         
   
TupleHashTable
BuildTupleHashTableExt(PlanState *parent, TupleDesc inputDesc, int numCols, AttrNumber *keyColIdx, const Oid *eqfuncoids, FmgrInfo *hashfunctions, Oid *collations, long nbuckets, Size additionalsize, MemoryContext metacxt, MemoryContext tablecxt, MemoryContext tempcxt, bool use_variable_hash_iv)
{
  TupleHashTable hashtable;
  Size entrysize = sizeof(TupleHashEntryData) + additionalsize;
  MemoryContext oldcontext;
  bool allow_jit;

  Assert(nbuckets > 0);

                                                                  
  nbuckets = Min(nbuckets, (long)((work_mem * 1024L) / entrysize));

  oldcontext = MemoryContextSwitchTo(metacxt);

  hashtable = (TupleHashTable)palloc(sizeof(TupleHashTableData));

  hashtable->numCols = numCols;
  hashtable->keyColIdx = keyColIdx;
  hashtable->tab_hash_funcs = hashfunctions;
  hashtable->tab_collations = collations;
  hashtable->tablecxt = tablecxt;
  hashtable->tempcxt = tempcxt;
  hashtable->entrysize = entrysize;
  hashtable->tableslot = NULL;                                   
  hashtable->inputslot = NULL;
  hashtable->in_hash_funcs = NULL;
  hashtable->cur_eq_func = NULL;

     
                                                                            
                                                                             
                                                                        
                                                                    
                                                              
                     
     
  if (use_variable_hash_iv)
  {
    hashtable->hash_iv = murmurhash32(ParallelWorkerNumber);
  }
  else
  {
    hashtable->hash_iv = 0;
  }

  hashtable->hashtab = tuplehash_create(metacxt, nbuckets, hashtable);

     
                                                                          
                                                    
     
  hashtable->tableslot = MakeSingleTupleTableSlot(CreateTupleDescCopy(inputDesc), &TTSOpsMinimalTuple);

     
                                                                          
                                                                            
                                                                             
                                                                             
                                                                        
                                
     
  allow_jit = metacxt != tablecxt;

                                        
                                                                    
  hashtable->tab_eq_func = ExecBuildGroupingEqual(inputDesc, inputDesc, &TTSOpsMinimalTuple, &TTSOpsMinimalTuple, numCols, keyColIdx, eqfuncoids, collations, allow_jit ? parent : NULL);

     
                                                                          
                                                           
                                                                           
                                                                           
     
  hashtable->exprcontext = CreateStandaloneExprContext();

  MemoryContextSwitchTo(oldcontext);

  return hashtable;
}

   
                                                               
                                                                        
                                                                             
                               
   
TupleHashTable
BuildTupleHashTable(PlanState *parent, TupleDesc inputDesc, int numCols, AttrNumber *keyColIdx, const Oid *eqfuncoids, FmgrInfo *hashfunctions, Oid *collations, long nbuckets, Size additionalsize, MemoryContext tablecxt, MemoryContext tempcxt, bool use_variable_hash_iv)
{
  return BuildTupleHashTableExt(parent, inputDesc, numCols, keyColIdx, eqfuncoids, hashfunctions, collations, nbuckets, additionalsize, tablecxt, tablecxt, tempcxt, use_variable_hash_iv);
}

   
                                                                               
                                                                           
                                                 
   
void
ResetTupleHashTable(TupleHashTable hashtable)
{
  tuplehash_reset(hashtable->hashtab);
}

   
                                                                       
                                                                           
   
                                                                        
                   
   
                                                                         
                                                                      
                                                                        
                
   
TupleHashEntry
LookupTupleHashEntry(TupleHashTable hashtable, TupleTableSlot *slot, bool *isnew)
{
  TupleHashEntryData *entry;
  MemoryContext oldContext;
  bool found;
  MinimalTuple key;

                                                             
  oldContext = MemoryContextSwitchTo(hashtable->tempcxt);

                                                      
  hashtable->inputslot = slot;
  hashtable->in_hash_funcs = hashtable->tab_hash_funcs;
  hashtable->cur_eq_func = hashtable->tab_eq_func;

  key = NULL;                                  

  if (isnew)
  {
    entry = tuplehash_insert(hashtable->hashtab, key, &found);

    if (found)
    {
                                    
      *isnew = false;
    }
    else
    {
                             
      *isnew = true;
                            
      entry->additional = NULL;
      MemoryContextSwitchTo(hashtable->tablecxt);
                                                       
      entry->firstTuple = ExecCopySlotMinimalTuple(slot);
    }
  }
  else
  {
    entry = tuplehash_lookup(hashtable->hashtab, key);
  }

  MemoryContextSwitchTo(oldContext);

  return entry;
}

   
                                                                       
                                                                        
                                                                    
                                                                        
                                                                         
                                                                          
                                                  
   
TupleHashEntry
FindTupleHashEntry(TupleHashTable hashtable, TupleTableSlot *slot, ExprState *eqcomp, FmgrInfo *hashfunctions)
{
  TupleHashEntry entry;
  MemoryContext oldContext;
  MinimalTuple key;

                                                             
  oldContext = MemoryContextSwitchTo(hashtable->tempcxt);

                                                      
  hashtable->inputslot = slot;
  hashtable->in_hash_funcs = hashfunctions;
  hashtable->cur_eq_func = eqcomp;

                             
  key = NULL;                                  
  entry = tuplehash_lookup(hashtable->hashtab, key);
  MemoryContextSwitchTo(oldContext);

  return entry;
}

   
                                      
   
                                                                            
                                                                        
                                                                            
                                                                            
                                                                              
                                                    
   
                                                                          
                                                                         
   
static uint32
TupleHashTableHash(struct tuplehash_hash *tb, const MinimalTuple tuple)
{
  TupleHashTable hashtable = (TupleHashTable)tb->private_data;
  int numCols = hashtable->numCols;
  AttrNumber *keyColIdx = hashtable->keyColIdx;
  uint32 hashkey = hashtable->hash_iv;
  TupleTableSlot *slot;
  FmgrInfo *hashfunctions;
  int i;

  if (tuple == NULL)
  {
                                                       
    slot = hashtable->inputslot;
    hashfunctions = hashtable->in_hash_funcs;
  }
  else
  {
       
                                                    
       
                                                                       
                                                         
       
    slot = hashtable->tableslot;
    ExecStoreMinimalTuple(tuple, slot, false);
    hashfunctions = hashtable->tab_hash_funcs;
  }

  for (i = 0; i < numCols; i++)
  {
    AttrNumber att = keyColIdx[i];
    Datum attr;
    bool isNull;

                                                
    hashkey = (hashkey << 1) | ((hashkey & 0x80000000) ? 1 : 0);

    attr = slot_getattr(slot, att, &isNull);

    if (!isNull)                                       
    {
      uint32 hkey;

      hkey = DatumGetUInt32(FunctionCall1Coll(&hashfunctions[i], hashtable->tab_collations[i], attr));
      hashkey ^= hkey;
    }
  }

     
                                                                          
                                                                           
                                                                     
                                             
     
  return murmurhash32(hashkey);
}

   
                                                                    
   
                                                                     
   
static int
TupleHashTableMatch(struct tuplehash_hash *tb, const MinimalTuple tuple1, const MinimalTuple tuple2)
{
  TupleTableSlot *slot1;
  TupleTableSlot *slot2;
  TupleHashTable hashtable = (TupleHashTable)tb->private_data;
  ExprContext *econtext = hashtable->exprcontext;

     
                                                                       
                                                                         
                                                                           
                                                            
     
  Assert(tuple1 != NULL);
  slot1 = hashtable->tableslot;
  ExecStoreMinimalTuple(tuple1, slot1, false);
  Assert(tuple2 == NULL);
  slot2 = hashtable->inputslot;

                                                              
  econtext->ecxt_innertuple = slot2;
  econtext->ecxt_outertuple = slot1;
  return !ExecQualAndReset(hashtable->cur_eq_func, econtext);
}
