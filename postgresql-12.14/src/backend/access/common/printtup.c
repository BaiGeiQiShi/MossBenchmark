                                                                            
   
              
                                                                    
                                                          
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/printtup.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "tcop/pquery.h"
#include "utils/lsyscache.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

static void
printtup_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static bool
printtup(TupleTableSlot *slot, DestReceiver *self);
static bool
printtup_20(TupleTableSlot *slot, DestReceiver *self);
static bool
printtup_internal_20(TupleTableSlot *slot, DestReceiver *self);
static void
printtup_shutdown(DestReceiver *self);
static void
printtup_destroy(DestReceiver *self);

static void
SendRowDescriptionCols_2(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats);
static void
SendRowDescriptionCols_3(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats);

                                                                    
                                
                                                                    
   

                    
                                                    
   
                                                                             
                                 
                    
   
typedef struct
{                                                   
  Oid typoutput;                                            
  Oid typsend;                                                
  bool typisvarlena;                                             
  int16 format;                                       
  FmgrInfo finfo;                                             
} PrinttupAttrInfo;

typedef struct
{
  DestReceiver pub;                                         
  Portal portal;                                           
  bool sendDescrip;                                        
  TupleDesc attrinfo;                                      
  int nattrs;
  PrinttupAttrInfo *myinfo;                                  
  StringInfoData buf;                                                
  MemoryContext tmpcontext;                                           
} DR_printtup;

                    
                                                   
                    
   
DestReceiver *
printtup_create_DR(CommandDest dest)
{
  DR_printtup *self = (DR_printtup *)palloc0(sizeof(DR_printtup));

  self->pub.receiveSlot = printtup;                              
  self->pub.rStartup = printtup_startup;
  self->pub.rShutdown = printtup_shutdown;
  self->pub.rDestroy = printtup_destroy;
  self->pub.mydest = dest;

     
                                                            
                       
     
  self->sendDescrip = (dest == DestRemote);

  self->attrinfo = NULL;
  self->nattrs = 0;
  self->myinfo = NULL;
  self->buf.data = NULL;
  self->tmpcontext = NULL;

  return (DestReceiver *)self;
}

   
                                                                   
   
void
SetRemoteDestReceiverParams(DestReceiver *self, Portal portal)
{
  DR_printtup *myState = (DR_printtup *)self;

  Assert(myState->pub.mydest == DestRemote || myState->pub.mydest == DestRemoteExecute);

  myState->portal = portal;

  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
       
                                                                           
                                                                           
                              
       
    if (portal->formats && portal->formats[0] != 0)
    {
      myState->pub.receiveSlot = printtup_internal_20;
    }
    else
    {
      myState->pub.receiveSlot = printtup_20;
    }
  }
}

static void
printtup_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
  DR_printtup *myState = (DR_printtup *)self;
  Portal portal = myState->portal;

     
                                                                           
                                                         
     
  initStringInfo(&myState->buf);

     
                                                                         
                                                                          
                                                                        
             
     
  myState->tmpcontext = AllocSetContextCreate(CurrentMemoryContext, "printtup", ALLOCSET_DEFAULT_SIZES);

  if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
  {
       
                                                                        
       
                                                         
       
    const char *portalName = portal->name;

    if (portalName == NULL || portalName[0] == '\0')
    {
      portalName = "blank";
    }

    pq_puttextmessage('P', portalName);
  }

     
                                                                      
                               
     
  if (myState->sendDescrip)
  {
    SendRowDescriptionMessage(&myState->buf, typeinfo, FetchPortalTargetList(portal), portal->formats);
  }

                      
                                                                            
                                                      
                                                                      
                                
                                                                          
                                                                          
                              
                      
     
}

   
                                                                               
   
                                                                            
                                                                       
                                                                          
                                                                           
                                                                              
                                                                              
                                                  
   
void
SendRowDescriptionMessage(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats)
{
  int natts = typeinfo->natts;
  int proto = PG_PROTOCOL_MAJOR(FrontendProtocol);

                                     
  pq_beginmessage_reuse(buf, 'T');
                            
  pq_sendint16(buf, natts);

  if (proto >= 3)
  {
    SendRowDescriptionCols_3(buf, typeinfo, targetlist, formats);
  }
  else
  {
    SendRowDescriptionCols_2(buf, typeinfo, targetlist, formats);
  }

  pq_endmessage_reuse(buf);
}

   
                                                            
   
static void
SendRowDescriptionCols_3(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats)
{
  int natts = typeinfo->natts;
  int i;
  ListCell *tlist_item = list_head(targetlist);

     
                                                                          
                                                                           
                    
     
                                                                       
                             
     
  enlargeStringInfo(buf, (NAMEDATALEN * MAX_CONVERSION_GROWTH              
                             + sizeof(Oid)                                    
                             + sizeof(AttrNumber)                             
                             + sizeof(Oid)                                  
                             + sizeof(int16)                              
                             + sizeof(int32)                                
                             + sizeof(int16)                              
                             ) *
                             natts);

  for (i = 0; i < natts; ++i)
  {
    Form_pg_attribute att = TupleDescAttr(typeinfo, i);
    Oid atttypid = att->atttypid;
    int32 atttypmod = att->atttypmod;
    Oid resorigtbl;
    AttrNumber resorigcol;
    int16 format;

       
                                                                     
                                                       
       
    atttypid = getBaseTypeAndTypmod(atttypid, &atttypmod);

                                              
    while (tlist_item && ((TargetEntry *)lfirst(tlist_item))->resjunk)
    {
      tlist_item = lnext(tlist_item);
    }
    if (tlist_item)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(tlist_item);

      resorigtbl = tle->resorigtbl;
      resorigcol = tle->resorigcol;
      tlist_item = lnext(tlist_item);
    }
    else
    {
                                             
      resorigtbl = 0;
      resorigcol = 0;
    }

    if (formats)
    {
      format = formats[i];
    }
    else
    {
      format = 0;
    }

    pq_writestring(buf, NameStr(att->attname));
    pq_writeint32(buf, resorigtbl);
    pq_writeint16(buf, resorigcol);
    pq_writeint32(buf, atttypid);
    pq_writeint16(buf, att->attlen);
    pq_writeint32(buf, atttypmod);
    pq_writeint16(buf, format);
  }
}

   
                                                           
   
static void
SendRowDescriptionCols_2(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats)
{
  int natts = typeinfo->natts;
  int i;

  for (i = 0; i < natts; ++i)
  {
    Form_pg_attribute att = TupleDescAttr(typeinfo, i);
    Oid atttypid = att->atttypid;
    int32 atttypmod = att->atttypmod;

                                                                      
    atttypid = getBaseTypeAndTypmod(atttypid, &atttypmod);

    pq_sendstring(buf, NameStr(att->attname));
                                                            
    pq_sendint32(buf, atttypid);
    pq_sendint16(buf, att->attlen);
    pq_sendint32(buf, atttypmod);
                                                         
  }
}

   
                                             
   
static void
printtup_prepare_info(DR_printtup *myState, TupleDesc typeinfo, int numAttrs)
{
  int16 *formats = myState->portal->formats;
  int i;

                               
  if (myState->myinfo)
  {
    pfree(myState->myinfo);
  }
  myState->myinfo = NULL;

  myState->attrinfo = typeinfo;
  myState->nattrs = numAttrs;
  if (numAttrs <= 0)
  {
    return;
  }

  myState->myinfo = (PrinttupAttrInfo *)palloc0(numAttrs * sizeof(PrinttupAttrInfo));

  for (i = 0; i < numAttrs; i++)
  {
    PrinttupAttrInfo *thisState = myState->myinfo + i;
    int16 format = (formats ? formats[i] : 0);
    Form_pg_attribute attr = TupleDescAttr(typeinfo, i);

    thisState->format = format;
    if (format == 0)
    {
      getTypeOutputInfo(attr->atttypid, &thisState->typoutput, &thisState->typisvarlena);
      fmgr_info(thisState->typoutput, &thisState->finfo);
    }
    else if (format == 1)
    {
      getTypeBinaryOutputInfo(attr->atttypid, &thisState->typsend, &thisState->typisvarlena);
      fmgr_info(thisState->typsend, &thisState->finfo);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unsupported format code: %d", format)));
    }
  }
}

                    
                                               
                    
   
static bool
printtup(TupleTableSlot *slot, DestReceiver *self)
{
  TupleDesc typeinfo = slot->tts_tupleDescriptor;
  DR_printtup *myState = (DR_printtup *)self;
  MemoryContext oldcontext;
  StringInfo buf = &myState->buf;
  int natts = typeinfo->natts;
  int i;

                                                          
  if (myState->attrinfo != typeinfo || myState->nattrs != natts)
  {
    printtup_prepare_info(myState, typeinfo, natts);
  }

                                                  
  slot_getallattrs(slot);

                                                                  
  oldcontext = MemoryContextSwitchTo(myState->tmpcontext);

     
                                                                   
     
  pq_beginmessage_reuse(buf, 'D');

  pq_sendint16(buf, natts);

     
                                       
     
  for (i = 0; i < natts; ++i)
  {
    PrinttupAttrInfo *thisState = myState->myinfo + i;
    Datum attr = slot->tts_values[i];

    if (slot->tts_isnull[i])
    {
      pq_sendint32(buf, -1);
      continue;
    }

       
                                                                        
                                                                         
                                                                  
                                                                         
                                
       
    if (thisState->typisvarlena)
    {
      VALGRIND_CHECK_MEM_IS_DEFINED(DatumGetPointer(attr), VARSIZE_ANY(attr));
    }

    if (thisState->format == 0)
    {
                       
      char *outputstr;

      outputstr = OutputFunctionCall(&thisState->finfo, attr);
      pq_sendcountedtext(buf, outputstr, strlen(outputstr), false);
    }
    else
    {
                         
      bytea *outputbytes;

      outputbytes = SendFunctionCall(&thisState->finfo, attr);
      pq_sendint32(buf, VARSIZE(outputbytes) - VARHDRSZ);
      pq_sendbytes(buf, VARDATA(outputbytes), VARSIZE(outputbytes) - VARHDRSZ);
    }
  }

  pq_endmessage_reuse(buf);

                                                                    
  MemoryContextSwitchTo(oldcontext);
  MemoryContextReset(myState->tmpcontext);

  return true;
}

                    
                                                  
                    
   
static bool
printtup_20(TupleTableSlot *slot, DestReceiver *self)
{
  TupleDesc typeinfo = slot->tts_tupleDescriptor;
  DR_printtup *myState = (DR_printtup *)self;
  MemoryContext oldcontext;
  StringInfo buf = &myState->buf;
  int natts = typeinfo->natts;
  int i, j, k;

                                                          
  if (myState->attrinfo != typeinfo || myState->nattrs != natts)
  {
    printtup_prepare_info(myState, typeinfo, natts);
  }

                                                  
  slot_getallattrs(slot);

                                                                  
  oldcontext = MemoryContextSwitchTo(myState->tmpcontext);

     
                                                                 
     
  pq_beginmessage_reuse(buf, 'D');

     
                                                    
     
  j = 0;
  k = 1 << 7;
  for (i = 0; i < natts; ++i)
  {
    if (!slot->tts_isnull[i])
    {
      j |= k;                          
    }
    k >>= 1;
    if (k == 0)                   
    {
      pq_sendint8(buf, j);
      j = 0;
      k = 1 << 7;
    }
  }
  if (k != (1 << 7))                              
  {
    pq_sendint8(buf, j);
  }

     
                                       
     
  for (i = 0; i < natts; ++i)
  {
    PrinttupAttrInfo *thisState = myState->myinfo + i;
    Datum attr = slot->tts_values[i];
    char *outputstr;

    if (slot->tts_isnull[i])
    {
      continue;
    }

    Assert(thisState->format == 0);

    outputstr = OutputFunctionCall(&thisState->finfo, attr);
    pq_sendcountedtext(buf, outputstr, strlen(outputstr), true);
  }

  pq_endmessage_reuse(buf);

                                                                    
  MemoryContextSwitchTo(oldcontext);
  MemoryContextReset(myState->tmpcontext);

  return true;
}

                    
                      
                    
   
static void
printtup_shutdown(DestReceiver *self)
{
  DR_printtup *myState = (DR_printtup *)self;

  if (myState->myinfo)
  {
    pfree(myState->myinfo);
  }
  myState->myinfo = NULL;

  myState->attrinfo = NULL;

  if (myState->buf.data)
  {
    pfree(myState->buf.data);
  }
  myState->buf.data = NULL;

  if (myState->tmpcontext)
  {
    MemoryContextDelete(myState->tmpcontext);
  }
  myState->tmpcontext = NULL;
}

                    
                     
                    
   
static void
printtup_destroy(DestReceiver *self)
{
  pfree(self);
}

                    
             
                    
   
static void
printatt(unsigned attributeId, Form_pg_attribute attributeP, char *value)
{
  printf("\t%2d: %s%s%s%s\t(typeid = %u, len = %d, typmod = %d, byval = %c)\n", attributeId, NameStr(attributeP->attname), value != NULL ? " = \"" : "", value != NULL ? value : "", value != NULL ? "\"" : "", (unsigned int)(attributeP->atttypid), attributeP->attlen, attributeP->atttypmod, attributeP->attbyval ? 't' : 'f');
}

                    
                                                                      
                    
   
void
debugStartup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
  int natts = typeinfo->natts;
  int i;

     
                                        
     
  for (i = 0; i < natts; ++i)
  {
    printatt((unsigned)i + 1, TupleDescAttr(typeinfo, i), NULL);
  }
  printf("\t----\n");
}

                    
                                                          
                    
   
bool
debugtup(TupleTableSlot *slot, DestReceiver *self)
{
  TupleDesc typeinfo = slot->tts_tupleDescriptor;
  int natts = typeinfo->natts;
  int i;
  Datum attr;
  char *value;
  bool isnull;
  Oid typoutput;
  bool typisvarlena;

  for (i = 0; i < natts; ++i)
  {
    attr = slot_getattr(slot, i + 1, &isnull);
    if (isnull)
    {
      continue;
    }
    getTypeOutputInfo(TupleDescAttr(typeinfo, i)->atttypid, &typoutput, &typisvarlena);

    value = OidOutputFunctionCall(typoutput, attr);

    printatt((unsigned)i + 1, TupleDescAttr(typeinfo, i), value);
  }
  printf("\t----\n");

  return true;
}

                    
                                                                  
   
                                                               
                                               
   
                                                                         
                    
   
static bool
printtup_internal_20(TupleTableSlot *slot, DestReceiver *self)
{
  TupleDesc typeinfo = slot->tts_tupleDescriptor;
  DR_printtup *myState = (DR_printtup *)self;
  MemoryContext oldcontext;
  StringInfo buf = &myState->buf;
  int natts = typeinfo->natts;
  int i, j, k;

                                                          
  if (myState->attrinfo != typeinfo || myState->nattrs != natts)
  {
    printtup_prepare_info(myState, typeinfo, natts);
  }

                                                  
  slot_getallattrs(slot);

                                                                  
  oldcontext = MemoryContextSwitchTo(myState->tmpcontext);

     
                                                                  
     
  pq_beginmessage_reuse(buf, 'B');

     
                                                    
     
  j = 0;
  k = 1 << 7;
  for (i = 0; i < natts; ++i)
  {
    if (!slot->tts_isnull[i])
    {
      j |= k;                          
    }
    k >>= 1;
    if (k == 0)                   
    {
      pq_sendint8(buf, j);
      j = 0;
      k = 1 << 7;
    }
  }
  if (k != (1 << 7))                              
  {
    pq_sendint8(buf, j);
  }

     
                                       
     
  for (i = 0; i < natts; ++i)
  {
    PrinttupAttrInfo *thisState = myState->myinfo + i;
    Datum attr = slot->tts_values[i];
    bytea *outputbytes;

    if (slot->tts_isnull[i])
    {
      continue;
    }

    Assert(thisState->format == 1);

    outputbytes = SendFunctionCall(&thisState->finfo, attr);
    pq_sendint32(buf, VARSIZE(outputbytes) - VARHDRSZ);
    pq_sendbytes(buf, VARDATA(outputbytes), VARSIZE(outputbytes) - VARHDRSZ);
  }

  pq_endmessage_reuse(buf);

                                                                    
  MemoryContextSwitchTo(oldcontext);
  MemoryContextReset(myState->tmpcontext);

  return true;
}
