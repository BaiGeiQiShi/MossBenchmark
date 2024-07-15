                                                                            
   
                 
                                                                     
                                                                 
                                                                        
                                                                   
                                                                        
                                                                        
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/printsimple.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"

   
                                                   
   
void
printsimple_startup(DestReceiver *self, int operation, TupleDesc tupdesc)
{
  StringInfoData buf;
  int i;

  pq_beginmessage(&buf, 'T');                     
  pq_sendint16(&buf, tupdesc->natts);

  for (i = 0; i < tupdesc->natts; ++i)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

    pq_sendstring(&buf, NameStr(attr->attname));
    pq_sendint32(&buf, 0);                
    pq_sendint16(&buf, 0);             
    pq_sendint32(&buf, (int)attr->atttypid);
    pq_sendint16(&buf, attr->attlen);
    pq_sendint32(&buf, attr->atttypmod);
    pq_sendint16(&buf, 0);                  
  }

  pq_endmessage(&buf);
}

   
                                           
   
bool
printsimple(TupleTableSlot *slot, DestReceiver *self)
{
  TupleDesc tupdesc = slot->tts_tupleDescriptor;
  StringInfoData buf;
  int i;

                                                  
  slot_getallattrs(slot);

                                
  pq_beginmessage(&buf, 'D');
  pq_sendint16(&buf, tupdesc->natts);

  for (i = 0; i < tupdesc->natts; ++i)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, i);
    Datum value;

    if (slot->tts_isnull[i])
    {
      pq_sendint32(&buf, -1);
      continue;
    }

    value = slot->tts_values[i];

       
                                                                       
                                                                  
                                        
       
    switch (attr->atttypid)
    {
    case TEXTOID:
    {
      text *t = DatumGetTextPP(value);

      pq_sendcountedtext(&buf, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t), false);
    }
    break;

    case INT4OID:
    {
      int32 num = DatumGetInt32(value);
      char str[12];                               

      pg_ltoa(num, str);
      pq_sendcountedtext(&buf, str, strlen(str), false);
    }
    break;

    case INT8OID:
    {
      int64 num = DatumGetInt64(value);
      char str[23];                               

      pg_lltoa(num, str);
      pq_sendcountedtext(&buf, str, strlen(str), false);
    }
    break;

    default:
      elog(ERROR, "unsupported type OID: %u", attr->atttypid);
    }
  }

  pq_endmessage(&buf);

  return true;
}
