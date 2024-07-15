                                                                            
   
                       
                                                       
   
                                                                     
                                                                           
                                                                       
                                                                  
                                                                            
                                                                  
   
                                                       
                                                               
                                                                      
                                                                         
                                                                          
                                                                        
                                                                       
                                                                    
                                                                   
                                                                      
                                                                           
                                                                        
                                                                        
                                                              
                                                                         
                   
   
                                                                         
                                                                          
                                                                              
   
   
                                    
   
                             
                                                             
                               
                                                              
                                      
                             
                                                      
                              
                                                                    
                               
                                                         
                                      
                                                      
                              
                                                                
                            
                             
   
                                                                         
                                                                        
   
                  
                                                 
   
                                                                            
   

#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/subtrans.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "storage/bufmgr.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/combocid.h"
#include "utils/snapmgr.h"

   
                 
   
                                                                       
   
                                                                          
                                                                              
                                                                               
                                                                              
                                                                       
                                                                            
                                                                          
                                       
   
                                                                          
                                     
   
                                                                             
                                                                          
                                                                      
                                                                         
                                                                            
                                                                       
                                                   
   
                                                                           
                                                              
   
                                                                         
                                               
   
static inline void
SetHintBits(HeapTupleHeader tuple, Buffer buffer, uint16 infomask, TransactionId xid)
{
  if (TransactionIdIsValid(xid))
  {
                                               
    XLogRecPtr commitLSN = TransactionIdGetCommitLSN(xid);

    if (BufferIsPermanent(buffer) && XLogNeedsFlush(commitLSN) && BufferGetLSNAtomic(buffer) < commitLSN)
    {
                                                               
      return;
    }
  }

  tuple->t_infomask |= infomask;
  MarkBufferDirtyHint(buffer, true);
}

   
                                                              
   
                                                                          
                               
   
void
HeapTupleSetHintBits(HeapTupleHeader tuple, Buffer buffer, uint16 infomask, TransactionId xid)
{
  SetHintBits(tuple, buffer, infomask, xid);
}

   
                          
                                               
   
                                                              
   
         
                                 
   
                                                        
   
                                                                                      
                                          
                                                                                
      
   
                                                                                  
                                                          
                                                                             
                                                             
   
static bool
HeapTupleSatisfiesSelf(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    if (HeapTupleHeaderXminInvalid(tuple))
    {
      return false;
    }

                                         
    if (tuple->t_infomask & HEAP_MOVED_OFF)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return false;
      }
      if (!TransactionIdIsInProgress(xvac))
      {
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
        SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
      }
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_IN)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (!TransactionIdIsCurrentTransactionId(xvac))
      {
        if (TransactionIdIsInProgress(xvac))
        {
          return false;
        }
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
        }
        else
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
      }
    }
    else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tuple)))
    {
      if (tuple->t_infomask & HEAP_XMAX_INVALID)                  
      {
        return true;
      }

      if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))                  
      {
        return true;
      }

      if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
      {
        TransactionId xmax;

        xmax = HeapTupleGetUpdateXid(tuple);

                                                        
        Assert(TransactionIdIsValid(xmax));

                                                       
        if (!TransactionIdIsCurrentTransactionId(xmax))
        {
          return true;
        }
        else
        {
          return false;
        }
      }

      if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
      {
                                                       
        SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
        return true;
      }

      return false;
    }
    else if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmin(tuple)))
    {
      return false;
    }
    else if (TransactionIdDidCommit(HeapTupleHeaderGetRawXmin(tuple)))
    {
      SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, HeapTupleHeaderGetRawXmin(tuple));
    }
    else
    {
                                           
      SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
      return false;
    }
  }

                                                        

  if (tuple->t_infomask & HEAP_XMAX_INVALID)                             
  {
    return true;
  }

  if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
  {
    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return true;
    }
    return false;                       
  }

  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    TransactionId xmax;

    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return true;
    }

    xmax = HeapTupleGetUpdateXid(tuple);

                                                    
    Assert(TransactionIdIsValid(xmax));

    if (TransactionIdIsCurrentTransactionId(xmax))
    {
      return false;
    }
    if (TransactionIdIsInProgress(xmax))
    {
      return true;
    }
    if (TransactionIdDidCommit(xmax))
    {
      return false;
    }
                                         
    return true;
  }

  if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
  {
    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return true;
    }
    return false;
  }

  if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmax(tuple)))
  {
    return true;
  }

  if (!TransactionIdDidCommit(HeapTupleHeaderGetRawXmax(tuple)))
  {
                                         
    SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    return true;
  }

                                  

  if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
    SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    return true;
  }

  SetHintBits(tuple, buffer, HEAP_XMAX_COMMITTED, HeapTupleHeaderGetRawXmax(tuple));
  return false;
}

   
                         
                                                                
   
static bool
HeapTupleSatisfiesAny(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  return true;
}

   
                           
                                                 
   
                                                               
   
                                                                               
                                                                            
                                                                             
                                                                             
                                                                              
                                                                      
   
                                                                          
          
   
static bool
HeapTupleSatisfiesToast(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    if (HeapTupleHeaderXminInvalid(tuple))
    {
      return false;
    }

                                         
    if (tuple->t_infomask & HEAP_MOVED_OFF)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return false;
      }
      if (!TransactionIdIsInProgress(xvac))
      {
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
        SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
      }
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_IN)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (!TransactionIdIsCurrentTransactionId(xvac))
      {
        if (TransactionIdIsInProgress(xvac))
        {
          return false;
        }
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
        }
        else
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
      }
    }

       
                                                                          
                                                                      
                                                          
       
    else if (!TransactionIdIsValid(HeapTupleHeaderGetXmin(tuple)))
    {
      return false;
    }
  }

                                                      
  return true;
}

   
                            
   
                                                                      
                                                                       
                                                                      
                                
   
                                  
   
                                                                              
                                     
   
                                                                
   
                                                                            
                             
   
                                                                           
                                                                   
   
                                                                 
   
                                                                              
                                                                            
                                                                            
                                                                             
                            
   
TM_Result
HeapTupleSatisfiesUpdate(HeapTuple htup, CommandId curcid, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    if (HeapTupleHeaderXminInvalid(tuple))
    {
      return TM_Invisible;
    }

                                         
    if (tuple->t_infomask & HEAP_MOVED_OFF)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return TM_Invisible;
      }
      if (!TransactionIdIsInProgress(xvac))
      {
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return TM_Invisible;
        }
        SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
      }
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_IN)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (!TransactionIdIsCurrentTransactionId(xvac))
      {
        if (TransactionIdIsInProgress(xvac))
        {
          return TM_Invisible;
        }
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
        }
        else
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return TM_Invisible;
        }
      }
    }
    else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tuple)))
    {
      if (HeapTupleHeaderGetCmin(tuple) >= curcid)
      {
        return TM_Invisible;                                  
      }

      if (tuple->t_infomask & HEAP_XMAX_INVALID)                  
      {
        return TM_Ok;
      }

      if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
      {
        TransactionId xmax;

        xmax = HeapTupleHeaderGetRawXmax(tuple);

           
                                                                       
                                                                     
                                                                     
               
           

        if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
        {
          if (MultiXactIdIsRunning(xmax, true))
          {
            return TM_BeingModified;
          }
          else
          {
            return TM_Ok;
          }
        }

           
                                                                    
                                                             
                           
           
        if (!TransactionIdIsInProgress(xmax))
        {
          return TM_Ok;
        }
        return TM_BeingModified;
      }

      if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
      {
        TransactionId xmax;

        xmax = HeapTupleGetUpdateXid(tuple);

                                                        
        Assert(TransactionIdIsValid(xmax));

                                                       
        if (!TransactionIdIsCurrentTransactionId(xmax))
        {
          if (MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), false))
          {
            return TM_BeingModified;
          }
          return TM_Ok;
        }
        else
        {
          if (HeapTupleHeaderGetCmax(tuple) >= curcid)
          {
            return TM_SelfModified;                                 
          }
          else
          {
            return TM_Invisible;                                  
          }
        }
      }

      if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
      {
                                                       
        SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
        return TM_Ok;
      }

      if (HeapTupleHeaderGetCmax(tuple) >= curcid)
      {
        return TM_SelfModified;                                 
      }
      else
      {
        return TM_Invisible;                                  
      }
    }
    else if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmin(tuple)))
    {
      return TM_Invisible;
    }
    else if (TransactionIdDidCommit(HeapTupleHeaderGetRawXmin(tuple)))
    {
      SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, HeapTupleHeaderGetRawXmin(tuple));
    }
    else
    {
                                           
      SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
      return TM_Invisible;
    }
  }

                                                        

  if (tuple->t_infomask & HEAP_XMAX_INVALID)                             
  {
    return TM_Ok;
  }

  if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
  {
    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return TM_Ok;
    }
    if (!ItemPointerEquals(&htup->t_self, &tuple->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(tuple))
    {
      return TM_Updated;                       
    }
    else
    {
      return TM_Deleted;                       
    }
  }

  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    TransactionId xmax;

    if (HEAP_LOCKED_UPGRADED(tuple->t_infomask))
    {
      return TM_Ok;
    }

    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      if (MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), true))
      {
        return TM_BeingModified;
      }

      SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
      return TM_Ok;
    }

    xmax = HeapTupleGetUpdateXid(tuple);
    if (!TransactionIdIsValid(xmax))
    {
      if (MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), false))
      {
        return TM_BeingModified;
      }
    }

                                                    
    Assert(TransactionIdIsValid(xmax));

    if (TransactionIdIsCurrentTransactionId(xmax))
    {
      if (HeapTupleHeaderGetCmax(tuple) >= curcid)
      {
        return TM_SelfModified;                                 
      }
      else
      {
        return TM_Invisible;                                  
      }
    }

    if (MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), false))
    {
      return TM_BeingModified;
    }

    if (TransactionIdDidCommit(xmax))
    {
      if (!ItemPointerEquals(&htup->t_self, &tuple->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(tuple))
      {
        return TM_Updated;
      }
      else
      {
        return TM_Deleted;
      }
    }

       
                                                                         
                                     
       

    if (!MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), false))
    {
         
                                                                         
                                   
         
      SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
      return TM_Ok;
    }
    else
    {
                                     
      return TM_BeingModified;
    }
  }

  if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
  {
    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return TM_BeingModified;
    }
    if (HeapTupleHeaderGetCmax(tuple) >= curcid)
    {
      return TM_SelfModified;                                 
    }
    else
    {
      return TM_Invisible;                                  
    }
  }

  if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmax(tuple)))
  {
    return TM_BeingModified;
  }

  if (!TransactionIdDidCommit(HeapTupleHeaderGetRawXmax(tuple)))
  {
                                         
    SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    return TM_Ok;
  }

                                  

  if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
    SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    return TM_Ok;
  }

  SetHintBits(tuple, buffer, HEAP_XMAX_COMMITTED, HeapTupleHeaderGetRawXmax(tuple));
  if (!ItemPointerEquals(&htup->t_self, &tuple->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(tuple))
  {
    return TM_Updated;                       
  }
  else
  {
    return TM_Deleted;                       
  }
}

   
                           
                                                                         
   
                                                               
   
                                                                        
                                                                      
                                                                          
   
                                                                      
                                                                            
                                                                        
                                                                           
                                                                   
                                                                        
                                                                           
                                                                           
                                                         
   
static bool
HeapTupleSatisfiesDirty(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

  snapshot->xmin = snapshot->xmax = InvalidTransactionId;
  snapshot->speculativeToken = 0;

  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    if (HeapTupleHeaderXminInvalid(tuple))
    {
      return false;
    }

                                         
    if (tuple->t_infomask & HEAP_MOVED_OFF)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return false;
      }
      if (!TransactionIdIsInProgress(xvac))
      {
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
        SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
      }
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_IN)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (!TransactionIdIsCurrentTransactionId(xvac))
      {
        if (TransactionIdIsInProgress(xvac))
        {
          return false;
        }
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
        }
        else
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
      }
    }
    else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tuple)))
    {
      if (tuple->t_infomask & HEAP_XMAX_INVALID)                  
      {
        return true;
      }

      if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))                  
      {
        return true;
      }

      if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
      {
        TransactionId xmax;

        xmax = HeapTupleGetUpdateXid(tuple);

                                                        
        Assert(TransactionIdIsValid(xmax));

                                                       
        if (!TransactionIdIsCurrentTransactionId(xmax))
        {
          return true;
        }
        else
        {
          return false;
        }
      }

      if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
      {
                                                       
        SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
        return true;
      }

      return false;
    }
    else if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmin(tuple)))
    {
         
                                                                         
                                                                        
                                                                
                   
         
      if (HeapTupleHeaderIsSpeculative(tuple))
      {
        snapshot->speculativeToken = HeapTupleHeaderGetSpeculativeToken(tuple);

        Assert(snapshot->speculativeToken != 0);
      }

      snapshot->xmin = HeapTupleHeaderGetRawXmin(tuple);
                                                          
      return true;                            
    }
    else if (TransactionIdDidCommit(HeapTupleHeaderGetRawXmin(tuple)))
    {
      SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, HeapTupleHeaderGetRawXmin(tuple));
    }
    else
    {
                                           
      SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
      return false;
    }
  }

                                                        

  if (tuple->t_infomask & HEAP_XMAX_INVALID)                             
  {
    return true;
  }

  if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
  {
    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return true;
    }
    return false;                       
  }

  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    TransactionId xmax;

    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return true;
    }

    xmax = HeapTupleGetUpdateXid(tuple);

                                                    
    Assert(TransactionIdIsValid(xmax));

    if (TransactionIdIsCurrentTransactionId(xmax))
    {
      return false;
    }
    if (TransactionIdIsInProgress(xmax))
    {
      snapshot->xmax = xmax;
      return true;
    }
    if (TransactionIdDidCommit(xmax))
    {
      return false;
    }
                                         
    return true;
  }

  if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
  {
    if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      return true;
    }
    return false;
  }

  if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmax(tuple)))
  {
    if (!HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
    {
      snapshot->xmax = HeapTupleHeaderGetRawXmax(tuple);
    }
    return true;
  }

  if (!TransactionIdDidCommit(HeapTupleHeaderGetRawXmax(tuple)))
  {
                                         
    SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    return true;
  }

                                  

  if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
    SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    return true;
  }

  SetHintBits(tuple, buffer, HEAP_XMAX_COMMITTED, HeapTupleHeaderGetRawXmax(tuple));
  return false;                       
}

   
                          
                                                              
   
                                                              
   
                                                                          
                                                                              
                                                                              
                                                                            
                                                                              
                                                                              
                                                                              
                                                                             
                                                                             
                                                                            
                                                                              
                                                                              
                                                                              
                                                                              
                                            
   
static bool
HeapTupleSatisfiesMVCC(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    if (HeapTupleHeaderXminInvalid(tuple))
    {
      return false;
    }

                                         
    if (tuple->t_infomask & HEAP_MOVED_OFF)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return false;
      }
      if (!XidInMVCCSnapshot(xvac, snapshot))
      {
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
        SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
      }
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_IN)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (!TransactionIdIsCurrentTransactionId(xvac))
      {
        if (XidInMVCCSnapshot(xvac, snapshot))
        {
          return false;
        }
        if (TransactionIdDidCommit(xvac))
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
        }
        else
        {
          SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
          return false;
        }
      }
    }
    else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tuple)))
    {
      if (HeapTupleHeaderGetCmin(tuple) >= snapshot->curcid)
      {
        return false;                                  
      }

      if (tuple->t_infomask & HEAP_XMAX_INVALID)                  
      {
        return true;
      }

      if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))                  
      {
        return true;
      }

      if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
      {
        TransactionId xmax;

        xmax = HeapTupleGetUpdateXid(tuple);

                                                        
        Assert(TransactionIdIsValid(xmax));

                                                       
        if (!TransactionIdIsCurrentTransactionId(xmax))
        {
          return true;
        }
        else if (HeapTupleHeaderGetCmax(tuple) >= snapshot->curcid)
        {
          return true;                                 
        }
        else
        {
          return false;                                  
        }
      }

      if (!TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
      {
                                                       
        SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
        return true;
      }

      if (HeapTupleHeaderGetCmax(tuple) >= snapshot->curcid)
      {
        return true;                                 
      }
      else
      {
        return false;                                  
      }
    }
    else if (XidInMVCCSnapshot(HeapTupleHeaderGetRawXmin(tuple), snapshot))
    {
      return false;
    }
    else if (TransactionIdDidCommit(HeapTupleHeaderGetRawXmin(tuple)))
    {
      SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, HeapTupleHeaderGetRawXmin(tuple));
    }
    else
    {
                                           
      SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
      return false;
    }
  }
  else
  {
                                                                    
    if (!HeapTupleHeaderXminFrozen(tuple) && XidInMVCCSnapshot(HeapTupleHeaderGetRawXmin(tuple), snapshot))
    {
      return false;                                 
    }
  }

                                                        

  if (tuple->t_infomask & HEAP_XMAX_INVALID)                             
  {
    return true;
  }

  if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
    return true;
  }

  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    TransactionId xmax;

                               
    Assert(!HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask));

    xmax = HeapTupleGetUpdateXid(tuple);

                                                    
    Assert(TransactionIdIsValid(xmax));

    if (TransactionIdIsCurrentTransactionId(xmax))
    {
      if (HeapTupleHeaderGetCmax(tuple) >= snapshot->curcid)
      {
        return true;                                 
      }
      else
      {
        return false;                                  
      }
    }
    if (XidInMVCCSnapshot(xmax, snapshot))
    {
      return true;
    }
    if (TransactionIdDidCommit(xmax))
    {
      return false;                                     
    }
                                         
    return true;
  }

  if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
  {
    if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tuple)))
    {
      if (HeapTupleHeaderGetCmax(tuple) >= snapshot->curcid)
      {
        return true;                                 
      }
      else
      {
        return false;                                  
      }
    }

    if (XidInMVCCSnapshot(HeapTupleHeaderGetRawXmax(tuple), snapshot))
    {
      return true;
    }

    if (!TransactionIdDidCommit(HeapTupleHeaderGetRawXmax(tuple)))
    {
                                           
      SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
      return true;
    }

                                    
    SetHintBits(tuple, buffer, HEAP_XMAX_COMMITTED, HeapTupleHeaderGetRawXmax(tuple));
  }
  else
  {
                                                                    
    if (XidInMVCCSnapshot(HeapTupleHeaderGetRawXmax(tuple), snapshot))
    {
      return true;                                 
    }
  }

                                  

  return false;
}

   
                            
   
                                                                   
                                                                        
                                                                   
   
                                                                       
                                                                        
                                                                       
                                                               
   
HTSV_Result
HeapTupleSatisfiesVacuum(HeapTuple htup, TransactionId OldestXmin, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

     
                                          
     
                                                                            
                                                                
     
  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    if (HeapTupleHeaderXminInvalid(tuple))
    {
      return HEAPTUPLE_DEAD;
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_OFF)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return HEAPTUPLE_DELETE_IN_PROGRESS;
      }
      if (TransactionIdIsInProgress(xvac))
      {
        return HEAPTUPLE_DELETE_IN_PROGRESS;
      }
      if (TransactionIdDidCommit(xvac))
      {
        SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
        return HEAPTUPLE_DEAD;
      }
      SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
    }
                                         
    else if (tuple->t_infomask & HEAP_MOVED_IN)
    {
      TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

      if (TransactionIdIsCurrentTransactionId(xvac))
      {
        return HEAPTUPLE_INSERT_IN_PROGRESS;
      }
      if (TransactionIdIsInProgress(xvac))
      {
        return HEAPTUPLE_INSERT_IN_PROGRESS;
      }
      if (TransactionIdDidCommit(xvac))
      {
        SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, InvalidTransactionId);
      }
      else
      {
        SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
        return HEAPTUPLE_DEAD;
      }
    }
    else if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tuple)))
    {
      if (tuple->t_infomask & HEAP_XMAX_INVALID)                  
      {
        return HEAPTUPLE_INSERT_IN_PROGRESS;
      }
                                                                       
      if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask) || HeapTupleHeaderIsOnlyLocked(tuple))
      {
        return HEAPTUPLE_INSERT_IN_PROGRESS;
      }
                                                  
      if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetUpdateXid(tuple)))
      {
        return HEAPTUPLE_DELETE_IN_PROGRESS;
      }
                                                     
      return HEAPTUPLE_INSERT_IN_PROGRESS;
    }
    else if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmin(tuple)))
    {
         
                                                                       
                                                                        
                                                                     
                                                                   
                                                                    
                                                           
         
      return HEAPTUPLE_INSERT_IN_PROGRESS;
    }
    else if (TransactionIdDidCommit(HeapTupleHeaderGetRawXmin(tuple)))
    {
      SetHintBits(tuple, buffer, HEAP_XMIN_COMMITTED, HeapTupleHeaderGetRawXmin(tuple));
    }
    else
    {
         
                                                                      
         
      SetHintBits(tuple, buffer, HEAP_XMIN_INVALID, InvalidTransactionId);
      return HEAPTUPLE_DEAD;
    }

       
                                                                        
                                                                          
                 
       
  }

     
                                                                           
                                     
     
  if (tuple->t_infomask & HEAP_XMAX_INVALID)
  {
    return HEAPTUPLE_LIVE;
  }

  if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
       
                                                                          
                                                                         
                                                                           
                                             
       
    if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
    {
      if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
      {
           
                                                                
                                                         
           
        if (!HEAP_LOCKED_UPGRADED(tuple->t_infomask) && MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), true))
        {
          return HEAPTUPLE_LIVE;
        }
        SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
      }
      else
      {
        if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmax(tuple)))
        {
          return HEAPTUPLE_LIVE;
        }
        SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
      }
    }

       
                                                                        
                                                                        
                           
       

    return HEAPTUPLE_LIVE;
  }

  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    TransactionId xmax = HeapTupleGetUpdateXid(tuple);

                               
    Assert(!HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask));

                                                    
    Assert(TransactionIdIsValid(xmax));

    if (TransactionIdIsInProgress(xmax))
    {
      return HEAPTUPLE_DELETE_IN_PROGRESS;
    }
    else if (TransactionIdDidCommit(xmax))
    {
         
                                                                      
                                                                  
                                                                        
                                                                         
                                                                    
                                                                         
         
      if (!TransactionIdPrecedes(xmax, OldestXmin))
      {
        return HEAPTUPLE_RECENTLY_DEAD;
      }

      return HEAPTUPLE_DEAD;
    }
    else if (!MultiXactIdIsRunning(HeapTupleHeaderGetRawXmax(tuple), false))
    {
         
                                                                       
                                   
         
      SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    }

    return HEAPTUPLE_LIVE;
  }

  if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
  {
    if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmax(tuple)))
    {
      return HEAPTUPLE_DELETE_IN_PROGRESS;
    }
    else if (TransactionIdDidCommit(HeapTupleHeaderGetRawXmax(tuple)))
    {
      SetHintBits(tuple, buffer, HEAP_XMAX_COMMITTED, HeapTupleHeaderGetRawXmax(tuple));
    }
    else
    {
         
                                                                      
         
      SetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
      return HEAPTUPLE_LIVE;
    }

       
                                                                        
                                                                          
                 
       
  }

     
                                                                        
                                             
     
  if (!TransactionIdPrecedes(HeapTupleHeaderGetRawXmax(tuple), OldestXmin))
  {
    return HEAPTUPLE_RECENTLY_DEAD;
  }

                                          
  return HEAPTUPLE_DEAD;
}

   
                                   
   
                                                                     
                                            
   
                                                               
   
                                                                        
                                                                     
                                                                      
   
static bool
HeapTupleSatisfiesNonVacuumable(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  return HeapTupleSatisfiesVacuum(htup, snapshot->xmin, buffer) != HEAPTUPLE_DEAD;
}

   
                         
   
                                                                      
                                                                      
                                                                     
                                                                         
                                                                            
                                                                        
                                                                       
                                                                         
                              
   
bool
HeapTupleIsSurelyDead(HeapTuple htup, TransactionId OldestXmin)
{
  HeapTupleHeader tuple = htup->t_data;

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

     
                                                                          
                                                                         
                                                                             
                                                        
     
  if (!HeapTupleHeaderXminCommitted(tuple))
  {
    return HeapTupleHeaderXminInvalid(tuple) ? true : false;
  }

     
                                                                          
                                        
     
  if (tuple->t_infomask & HEAP_XMAX_INVALID)
  {
    return false;
  }

     
                                                           
     
  if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
    return false;
  }

     
                                                                          
                                         
     
  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    return false;
  }

                                                                            
  if (!(tuple->t_infomask & HEAP_XMAX_COMMITTED))
  {
    return false;
  }

                                                                     
  return TransactionIdPrecedes(HeapTupleHeaderGetRawXmax(tuple), OldestXmin);
}

   
                                                                 
   
                                                                           
                                                                              
   
                                                                               
                            
   
bool
HeapTupleHeaderIsOnlyLocked(HeapTupleHeader tuple)
{
  TransactionId xmax;

                                                                         
  if (tuple->t_infomask & HEAP_XMAX_INVALID)
  {
    return true;
  }

  if (tuple->t_infomask & HEAP_XMAX_LOCK_ONLY)
  {
    return true;
  }

                                    
  if (!TransactionIdIsValid(HeapTupleHeaderGetRawXmax(tuple)))
  {
    return true;
  }

     
                                                                       
                                   
     
  if (!(tuple->t_infomask & HEAP_XMAX_IS_MULTI))
  {
    return false;
  }

                                                                       
  xmax = HeapTupleGetUpdateXid(tuple);

                                                  
  Assert(TransactionIdIsValid(xmax));

  if (TransactionIdIsCurrentTransactionId(xmax))
  {
    return false;
  }
  if (TransactionIdIsInProgress(xmax))
  {
    return false;
  }
  if (TransactionIdDidCommit(xmax))
  {
    return false;
  }

     
                                                                         
             
     
  return true;
}

   
                                                                            
   
static bool
TransactionIdInArray(TransactionId xid, TransactionId *xip, Size num)
{
  return num > 0 && bsearch(&xid, xip, num, sizeof(TransactionId), xidComparator) != NULL;
}

   
                                                                               
          
   
                                              
   
                                                                                
                                                                               
   
                                                                               
                                                                       
                                                                              
                                                          
   
static bool
HeapTupleSatisfiesHistoricMVCC(HeapTuple htup, Snapshot snapshot, Buffer buffer)
{
  HeapTupleHeader tuple = htup->t_data;
  TransactionId xmin = HeapTupleHeaderGetXmin(tuple);
  TransactionId xmax = HeapTupleHeaderGetRawXmax(tuple);

  Assert(ItemPointerIsValid(&htup->t_self));
  Assert(htup->t_tableOid != InvalidOid);

                                     
  if (HeapTupleHeaderXminInvalid(tuple))
  {
    Assert(!TransactionIdDidCommit(xmin));
    return false;
  }
                                                                 
  else if (TransactionIdInArray(xmin, snapshot->subxip, snapshot->subxcnt))
  {
    bool resolved;
    CommandId cmin = HeapTupleHeaderGetRawCommandId(tuple);
    CommandId cmax = InvalidCommandId;

       
                                                                      
                                                                           
                          
       
    resolved = ResolveCminCmaxDuringDecoding(HistoricSnapshotGetTupleCids(), snapshot, htup, buffer, &cmin, &cmax);

    if (!resolved)
    {
      elog(ERROR, "could not resolve cmin/cmax of catalog tuple");
    }

    Assert(cmin != InvalidCommandId);

    if (cmin >= snapshot->curcid)
    {
      return false;                                  
    }
                      
  }
                                                                        
  else if (TransactionIdPrecedes(xmin, snapshot->xmin))
  {
    Assert(!(HeapTupleHeaderXminCommitted(tuple) && !TransactionIdDidCommit(xmin)));

                                                           
    if (!HeapTupleHeaderXminCommitted(tuple) && !TransactionIdDidCommit(xmin))
    {
      return false;
    }
                      
  }
                                               
  else if (TransactionIdFollowsOrEquals(xmin, snapshot->xmax))
  {
    return false;
  }
                                                             
  else if (TransactionIdInArray(xmin, snapshot->xip, snapshot->xcnt))
  {
                      
  }

     
                                                                             
                
     
  else
  {
    return false;
  }

                                                                  

                              
  if (tuple->t_infomask & HEAP_XMAX_INVALID)
  {
    return true;
  }
                                        
  else if (HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask))
  {
    return true;
  }

     
                                                                           
                                                 
     
  else if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    xmax = HeapTupleGetUpdateXid(tuple);
  }

                                                                 
  if (TransactionIdInArray(xmax, snapshot->subxip, snapshot->subxcnt))
  {
    bool resolved;
    CommandId cmin;
    CommandId cmax = HeapTupleHeaderGetRawCommandId(tuple);

                                        
    resolved = ResolveCminCmaxDuringDecoding(HistoricSnapshotGetTupleCids(), snapshot, htup, buffer, &cmin, &cmax);

    if (!resolved)
    {
      elog(ERROR, "could not resolve combocid to cmax");
    }

    Assert(cmax != InvalidCommandId);

    if (cmax >= snapshot->curcid)
    {
      return true;                                 
    }
    else
    {
      return false;                                  
    }
  }
                                                             
  else if (TransactionIdPrecedes(xmax, snapshot->xmin))
  {
    Assert(!(tuple->t_infomask & HEAP_XMAX_COMMITTED && !TransactionIdDidCommit(xmax)));

                              
    if (tuple->t_infomask & HEAP_XMAX_COMMITTED)
    {
      return false;
    }

                    
    return !TransactionIdDidCommit(xmax);
  }
                                                                           
  else if (TransactionIdFollowsOrEquals(xmax, snapshot->xmax))
  {
    return true;
  }
                                                                 
  else if (TransactionIdInArray(xmax, snapshot->xip, snapshot->xcnt))
  {
    return false;
  }
                                                                         
  else
  {
    return true;
  }
}

   
                                
                                               
   
          
                                                                  
   
                                                                            
                                                
   
bool
HeapTupleSatisfiesVisibility(HeapTuple tup, Snapshot snapshot, Buffer buffer)
{
  switch (snapshot->snapshot_type)
  {
  case SNAPSHOT_MVCC:
    return HeapTupleSatisfiesMVCC(tup, snapshot, buffer);
    break;
  case SNAPSHOT_SELF:
    return HeapTupleSatisfiesSelf(tup, snapshot, buffer);
    break;
  case SNAPSHOT_ANY:
    return HeapTupleSatisfiesAny(tup, snapshot, buffer);
    break;
  case SNAPSHOT_TOAST:
    return HeapTupleSatisfiesToast(tup, snapshot, buffer);
    break;
  case SNAPSHOT_DIRTY:
    return HeapTupleSatisfiesDirty(tup, snapshot, buffer);
    break;
  case SNAPSHOT_HISTORIC_MVCC:
    return HeapTupleSatisfiesHistoricMVCC(tup, snapshot, buffer);
    break;
  case SNAPSHOT_NON_VACUUMABLE:
    return HeapTupleSatisfiesNonVacuumable(tup, snapshot, buffer);
    break;
  }

  return false;                          
}
