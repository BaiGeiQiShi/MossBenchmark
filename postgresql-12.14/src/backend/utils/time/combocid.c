                                                                            
   
              
                                       
   
                                                                        
                                                                         
                                                                           
                                                                        
                                                                         
                                                                         
                                                                     
                                                                       
                                                                         
                
   
                                                                        
                                                                           
                                                                          
                                          
   
                                                                           
                                                                              
                                                                        
                                                                             
                                                                            
                                                      
   
                                                                       
                                             
   
   
                                                                         
                                                                        
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "storage/shmem.h"
#include "utils/combocid.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

                                                      
static HTAB *comboHash = NULL;

                                                 
typedef struct
{
  CommandId cmin;
  CommandId cmax;
} ComboCidKeyData;

typedef ComboCidKeyData *ComboCidKey;

typedef struct
{
  ComboCidKeyData key;
  CommandId combocid;
} ComboCidEntryData;

typedef ComboCidEntryData *ComboCidEntry;

                                    
#define CCID_HASH_SIZE 100

   
                                                             
                                                                          
   
static ComboCidKey comboCids = NULL;
static int usedComboCids = 0;                                      
static int sizeComboCids = 0;                              

                               
#define CCID_ARRAY_SIZE 100

                                       
static CommandId
GetComboCommandId(CommandId cmin, CommandId cmax);
static CommandId
GetRealCmin(CommandId combocid);
static CommandId
GetRealCmax(CommandId combocid);

                        

   
                                                                            
                                                                       
                                                                         
                                                                       
   

CommandId
HeapTupleHeaderGetCmin(HeapTupleHeader tup)
{
  CommandId cid = HeapTupleHeaderGetRawCommandId(tup);

  Assert(!(tup->t_infomask & HEAP_MOVED));
  Assert(TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tup)));

  if (tup->t_infomask & HEAP_COMBOCID)
  {
    return GetRealCmin(cid);
  }
  else
  {
    return cid;
  }
}

CommandId
HeapTupleHeaderGetCmax(HeapTupleHeader tup)
{
  CommandId cid = HeapTupleHeaderGetRawCommandId(tup);

  Assert(!(tup->t_infomask & HEAP_MOVED));

     
                                                                     
                                                                          
                                                                            
                      
     
  Assert(CritSectionCount > 0 || TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetUpdateXid(tup)));

  if (tup->t_infomask & HEAP_COMBOCID)
  {
    return GetRealCmax(cid);
  }
  else
  {
    return cid;
  }
}

   
                                                                              
                         
   
                                                                           
                                                                            
                   
   
                                                                        
                                                                             
                                                                         
                                        
   
void
HeapTupleHeaderAdjustCmax(HeapTupleHeader tup, CommandId *cmax, bool *iscombo)
{
     
                                                                
                                                                            
                                                                         
                                                      
     
  if (!HeapTupleHeaderXminCommitted(tup) && TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tup)))
  {
    CommandId cmin = HeapTupleHeaderGetCmin(tup);

    *cmax = GetComboCommandId(cmin, *cmax);
    *iscombo = true;
  }
  else
  {
    *iscombo = false;
  }
}

   
                                                                        
                                                                       
   
void
AtEOXact_ComboCid(void)
{
     
                                                                             
                                                                
     
  comboHash = NULL;

  comboCids = NULL;
  usedComboCids = 0;
  sizeComboCids = 0;
}

                             

   
                                                      
   
                                                        
   
static CommandId
GetComboCommandId(CommandId cmin, CommandId cmax)
{
  CommandId combocid;
  ComboCidKeyData key;
  ComboCidEntry entry;
  bool found;

     
                                                                         
                              
     
  if (comboHash == NULL)
  {
    HASHCTL hash_ctl;

                                                                        
    comboCids = (ComboCidKeyData *)MemoryContextAlloc(TopTransactionContext, sizeof(ComboCidKeyData) * CCID_ARRAY_SIZE);
    sizeComboCids = CCID_ARRAY_SIZE;
    usedComboCids = 0;

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(ComboCidKeyData);
    hash_ctl.entrysize = sizeof(ComboCidEntryData);
    hash_ctl.hcxt = TopTransactionContext;

    comboHash = hash_create("Combo CIDs", CCID_HASH_SIZE, &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
  }

     
                                                                            
                                                                     
                                                            
     
  if (usedComboCids >= sizeComboCids)
  {
    int newsize = sizeComboCids * 2;

    comboCids = (ComboCidKeyData *)repalloc(comboCids, sizeof(ComboCidKeyData) * newsize);
    sizeComboCids = newsize;
  }

                                                                

                                                                
  key.cmin = cmin;
  key.cmax = cmax;
  entry = (ComboCidEntry)hash_search(comboHash, (void *)&key, HASH_ENTER, &found);

  if (found)
  {
                                     
    return entry->combocid;
  }

                                                                            
  combocid = usedComboCids;

  comboCids[combocid].cmin = cmin;
  comboCids[combocid].cmax = cmax;
  usedComboCids++;

  entry->combocid = combocid;

  return combocid;
}

static CommandId
GetRealCmin(CommandId combocid)
{
  Assert(combocid < usedComboCids);
  return comboCids[combocid].cmin;
}

static CommandId
GetRealCmax(CommandId combocid)
{
  Assert(combocid < usedComboCids);
  return comboCids[combocid].cmax;
}

   
                                                                           
          
   
Size
EstimateComboCIDStateSpace(void)
{
  Size size;

                                                   
  size = sizeof(int);

                                                       
  size = add_size(size, mul_size(sizeof(ComboCidKeyData), usedComboCids));

  return size;
}

   
                                                                             
                                                                
                               
   
void
SerializeComboCIDState(Size maxsize, char *start_address)
{
  char *endptr;

                                                                   
  *(int *)start_address = usedComboCids;

                                                
  endptr = start_address + sizeof(int) + (sizeof(ComboCidKeyData) * usedComboCids);
  if (endptr < start_address || endptr > start_address + maxsize)
  {
    elog(ERROR, "not enough space to serialize ComboCID state");
  }

                                             
  if (usedComboCids > 0)
  {
    memcpy(start_address + sizeof(int), comboCids, (sizeof(ComboCidKeyData) * usedComboCids));
  }
}

   
                                                                        
                                                                          
                                                                             
                                        
   
void
RestoreComboCIDState(char *comboCIDstate)
{
  int num_elements;
  ComboCidKeyData *keydata;
  int i;
  CommandId cid;

  Assert(!comboCids && !comboHash);

                                                                        
  num_elements = *(int *)comboCIDstate;
  keydata = (ComboCidKeyData *)(comboCIDstate + sizeof(int));

                                                       
  for (i = 0; i < num_elements; i++)
  {
    cid = GetComboCommandId(keydata[i].cmin, keydata[i].cmax);

                                                 
    if (cid != i)
    {
      elog(ERROR, "unexpected command ID while restoring combo CIDs");
    }
  }
}
