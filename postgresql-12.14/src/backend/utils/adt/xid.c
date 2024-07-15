                                                                            
   
         
                                                                       
   
                                                                         
                                                                        
   
   
                  
                                 
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/multixact.h"
#include "access/transam.h"
#include "access/xact.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"

#define PG_GETARG_TRANSACTIONID(n) DatumGetTransactionId(PG_GETARG_DATUM(n))
#define PG_RETURN_TRANSACTIONID(x) return TransactionIdGetDatum(x)

#define PG_GETARG_COMMANDID(n) DatumGetCommandId(PG_GETARG_DATUM(n))
#define PG_RETURN_COMMANDID(x) return CommandIdGetDatum(x)

Datum
xidin(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_TRANSACTIONID((TransactionId)strtoul(str, NULL, 0));
}

Datum
xidout(PG_FUNCTION_ARGS)
{
  TransactionId transactionId = PG_GETARG_TRANSACTIONID(0);
  char *result = (char *)palloc(16);

  snprintf(result, 16, "%lu", (unsigned long)transactionId);
  PG_RETURN_CSTRING(result);
}

   
                                                       
   
Datum
xidrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_TRANSACTIONID((TransactionId)pq_getmsgint(buf, sizeof(TransactionId)));
}

   
                                              
   
Datum
xidsend(PG_FUNCTION_ARGS)
{
  TransactionId arg1 = PG_GETARG_TRANSACTIONID(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                  
   
Datum
xideq(PG_FUNCTION_ARGS)
{
  TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
  TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

  PG_RETURN_BOOL(TransactionIdEquals(xid1, xid2));
}

   
                                       
   
Datum
xidneq(PG_FUNCTION_ARGS)
{
  TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
  TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

  PG_RETURN_BOOL(!TransactionIdEquals(xid1, xid2));
}

   
                                                                      
   
Datum
xid_age(PG_FUNCTION_ARGS)
{
  TransactionId xid = PG_GETARG_TRANSACTIONID(0);
  TransactionId now = GetStableLatestTransactionId();

                                                
  if (!TransactionIdIsNormal(xid))
  {
    PG_RETURN_INT32(INT_MAX);
  }

  PG_RETURN_INT32((int32)(now - xid));
}

   
                                                                             
   
Datum
mxid_age(PG_FUNCTION_ARGS)
{
  TransactionId xid = PG_GETARG_TRANSACTIONID(0);
  MultiXactId now = ReadNextMultiXactId();

  if (!MultiXactIdIsValid(xid))
  {
    PG_RETURN_INT32(INT_MAX);
  }

  PG_RETURN_INT32((int32)(now - xid));
}

   
                 
                                       
   
                                                                             
                                                         
   
int
xidComparator(const void *arg1, const void *arg2)
{
  TransactionId xid1 = *(const TransactionId *)arg1;
  TransactionId xid2 = *(const TransactionId *)arg2;

  if (xid1 > xid2)
  {
    return 1;
  }
  if (xid1 < xid2)
  {
    return -1;
  }
  return 0;
}

   
                        
                                       
   
                                                                            
                                                                            
                                      
   
int
xidLogicalComparator(const void *arg1, const void *arg2)
{
  TransactionId xid1 = *(const TransactionId *)arg1;
  TransactionId xid2 = *(const TransactionId *)arg2;

  Assert(TransactionIdIsNormal(xid1));
  Assert(TransactionIdIsNormal(xid2));

  if (TransactionIdPrecedes(xid1, xid2))
  {
    return -1;
  }

  if (TransactionIdPrecedes(xid2, xid1))
  {
    return 1;
  }

  return 0;
}

                                                                               
                                             
                                                                               

   
                                                           
   
Datum
cidin(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_COMMANDID((CommandId)strtoul(str, NULL, 0));
}

   
                                                        
   
Datum
cidout(PG_FUNCTION_ARGS)
{
  CommandId c = PG_GETARG_COMMANDID(0);
  char *result = (char *)palloc(16);

  snprintf(result, 16, "%lu", (unsigned long)c);
  PG_RETURN_CSTRING(result);
}

   
                                                       
   
Datum
cidrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_COMMANDID((CommandId)pq_getmsgint(buf, sizeof(CommandId)));
}

   
                                              
   
Datum
cidsend(PG_FUNCTION_ARGS)
{
  CommandId arg1 = PG_GETARG_COMMANDID(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, arg1);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum
cideq(PG_FUNCTION_ARGS)
{
  CommandId arg1 = PG_GETARG_COMMANDID(0);
  CommandId arg2 = PG_GETARG_COMMANDID(1);

  PG_RETURN_BOOL(arg1 == arg2);
}
