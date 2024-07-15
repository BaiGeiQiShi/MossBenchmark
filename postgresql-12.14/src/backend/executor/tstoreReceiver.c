                                                                            
   
                    
                                                                        
                   
   
                                                                              
                                                                            
                                                 
   
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/tuptoaster.h"
#include "executor/tstoreReceiver.h"

typedef struct
{
  DestReceiver pub;
                   
  Tuplestorestate *tstore;                            
  MemoryContext cxt;                                      
  bool detoast;                                          
                  
  Datum *outvalues;                                    
  Datum *tofree;                                   
} TStoreState;

static bool
tstoreReceiveSlot_notoast(TupleTableSlot *slot, DestReceiver *self);
static bool
tstoreReceiveSlot_detoast(TupleTableSlot *slot, DestReceiver *self);

   
                                            
   
static void
tstoreStartupReceiver(DestReceiver *self, int operation, TupleDesc typeinfo)
{
  TStoreState *myState = (TStoreState *)self;
  bool needtoast = false;
  int natts = typeinfo->natts;
  int i;

                                                 
  if (myState->detoast)
  {
    for (i = 0; i < natts; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(typeinfo, i);

      if (attr->attisdropped)
      {
        continue;
      }
      if (attr->attlen == -1)
      {
        needtoast = true;
        break;
      }
    }
  }

                                   
  if (needtoast)
  {
    myState->pub.receiveSlot = tstoreReceiveSlot_detoast;
                          
    myState->outvalues = (Datum *)MemoryContextAlloc(myState->cxt, natts * sizeof(Datum));
    myState->tofree = (Datum *)MemoryContextAlloc(myState->cxt, natts * sizeof(Datum));
  }
  else
  {
    myState->pub.receiveSlot = tstoreReceiveSlot_notoast;
    myState->outvalues = NULL;
    myState->tofree = NULL;
  }
}

   
                                                                     
                                                             
   
static bool
tstoreReceiveSlot_notoast(TupleTableSlot *slot, DestReceiver *self)
{
  TStoreState *myState = (TStoreState *)self;

  tuplestore_puttupleslot(myState->tstore, slot);

  return true;
}

   
                                                                     
                                                                     
   
static bool
tstoreReceiveSlot_detoast(TupleTableSlot *slot, DestReceiver *self)
{
  TStoreState *myState = (TStoreState *)self;
  TupleDesc typeinfo = slot->tts_tupleDescriptor;
  int natts = typeinfo->natts;
  int nfree;
  int i;
  MemoryContext oldcxt;

                                                  
  slot_getallattrs(slot);

     
                                                                          
                                                                             
                                                     
     
  nfree = 0;
  for (i = 0; i < natts; i++)
  {
    Datum val = slot->tts_values[i];
    Form_pg_attribute attr = TupleDescAttr(typeinfo, i);

    if (!attr->attisdropped && attr->attlen == -1 && !slot->tts_isnull[i])
    {
      if (VARATT_IS_EXTERNAL(DatumGetPointer(val)))
      {
        val = PointerGetDatum(heap_tuple_fetch_attr((struct varlena *)DatumGetPointer(val)));
        myState->tofree[nfree++] = val;
      }
    }

    myState->outvalues[i] = val;
  }

     
                                                  
     
  oldcxt = MemoryContextSwitchTo(myState->cxt);
  tuplestore_putvalues(myState->tstore, typeinfo, myState->outvalues, slot->tts_isnull);
  MemoryContextSwitchTo(oldcxt);

                                                  
  for (i = 0; i < nfree; i++)
  {
    pfree(DatumGetPointer(myState->tofree[i]));
  }

  return true;
}

   
                                      
   
static void
tstoreShutdownReceiver(DestReceiver *self)
{
  TStoreState *myState = (TStoreState *)self;

                                
  if (myState->outvalues)
  {
    pfree(myState->outvalues);
  }
  myState->outvalues = NULL;
  if (myState->tofree)
  {
    pfree(myState->tofree);
  }
  myState->tofree = NULL;
}

   
                                      
   
static void
tstoreDestroyReceiver(DestReceiver *self)
{
  pfree(self);
}

   
                                           
   
DestReceiver *
CreateTuplestoreDestReceiver(void)
{
  TStoreState *self = (TStoreState *)palloc0(sizeof(TStoreState));

  self->pub.receiveSlot = tstoreReceiveSlot_notoast;                   
  self->pub.rStartup = tstoreStartupReceiver;
  self->pub.rShutdown = tstoreShutdownReceiver;
  self->pub.rDestroy = tstoreDestroyReceiver;
  self->pub.mydest = DestTuplestore;

                                                                     

  return (DestReceiver *)self;
}

   
                                               
   
void
SetTuplestoreDestReceiverParams(DestReceiver *self, Tuplestorestate *tStore, MemoryContext tContext, bool detoast)
{
  TStoreState *myState = (TStoreState *)self;

  Assert(myState->pub.mydest == DestTuplestore);
  myState->tstore = tStore;
  myState->cxt = tContext;
  myState->detoast = detoast;
}
