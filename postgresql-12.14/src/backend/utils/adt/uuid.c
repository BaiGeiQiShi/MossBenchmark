                                                                            
   
          
                                             
   
                                                                
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "port/pg_bswap.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/hashutils.h"
#include "utils/sortsupport.h"
#include "utils/uuid.h"

                          
typedef struct
{
  int64 input_count;                                     
  bool estimating;                                       

  hyperLogLogState abbr_card;                            
} uuid_sortsupport_state;

static void
string_to_uuid(const char *source, pg_uuid_t *uuid);
static int
uuid_internal_cmp(const pg_uuid_t *arg1, const pg_uuid_t *arg2);
static int
uuid_fast_cmp(Datum x, Datum y, SortSupport ssup);
static int
uuid_cmp_abbrev(Datum x, Datum y, SortSupport ssup);
static bool
uuid_abbrev_abort(int memtupcount, SortSupport ssup);
static Datum
uuid_abbrev_convert(Datum original, SortSupport ssup);

Datum
uuid_in(PG_FUNCTION_ARGS)
{
  char *uuid_str = PG_GETARG_CSTRING(0);
  pg_uuid_t *uuid;

  uuid = (pg_uuid_t *)palloc(sizeof(*uuid));
  string_to_uuid(uuid_str, uuid);
  PG_RETURN_UUID_P(uuid);
}

Datum
uuid_out(PG_FUNCTION_ARGS)
{
  pg_uuid_t *uuid = PG_GETARG_UUID_P(0);
  static const char hex_chars[] = "0123456789abcdef";
  StringInfoData buf;
  int i;

  initStringInfo(&buf);
  for (i = 0; i < UUID_LEN; i++)
  {
    int hi;
    int lo;

       
                                                                   
                                                                        
                                                                         
       
    if (i == 4 || i == 6 || i == 8 || i == 10)
    {
      appendStringInfoChar(&buf, '-');
    }

    hi = uuid->data[i] >> 4;
    lo = uuid->data[i] & 0x0F;

    appendStringInfoChar(&buf, hex_chars[hi]);
    appendStringInfoChar(&buf, hex_chars[lo]);
  }

  PG_RETURN_CSTRING(buf.data);
}

   
                                                                             
                                                                              
                                                                         
                                             
   
static void
string_to_uuid(const char *source, pg_uuid_t *uuid)
{
  const char *src = source;
  bool braces = false;
  int i;

  if (src[0] == '{')
  {
    src++;
    braces = true;
  }

  for (i = 0; i < UUID_LEN; i++)
  {
    char str_buf[3];

    if (src[0] == '\0' || src[1] == '\0')
    {
      goto syntax_error;
    }
    memcpy(str_buf, src, 2);
    if (!isxdigit((unsigned char)str_buf[0]) || !isxdigit((unsigned char)str_buf[1]))
    {
      goto syntax_error;
    }

    str_buf[2] = '\0';
    uuid->data[i] = (unsigned char)strtoul(str_buf, NULL, 16);
    src += 2;
    if (src[0] == '-' && (i % 2) == 1 && i < UUID_LEN - 1)
    {
      src++;
    }
  }

  if (braces)
  {
    if (*src != '}')
    {
      goto syntax_error;
    }
    src++;
  }

  if (*src != '\0')
  {
    goto syntax_error;
  }

  return;

syntax_error:
  ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "uuid", source)));
}

Datum
uuid_recv(PG_FUNCTION_ARGS)
{
  StringInfo buffer = (StringInfo)PG_GETARG_POINTER(0);
  pg_uuid_t *uuid;

  uuid = (pg_uuid_t *)palloc(UUID_LEN);
  memcpy(uuid->data, pq_getmsgbytes(buffer, UUID_LEN), UUID_LEN);
  PG_RETURN_POINTER(uuid);
}

Datum
uuid_send(PG_FUNCTION_ARGS)
{
  pg_uuid_t *uuid = PG_GETARG_UUID_P(0);
  StringInfoData buffer;

  pq_begintypsend(&buffer);
  pq_sendbytes(&buffer, (char *)uuid->data, UUID_LEN);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buffer));
}

                                    
static int
uuid_internal_cmp(const pg_uuid_t *arg1, const pg_uuid_t *arg2)
{
  return memcmp(arg1->data, arg2->data, UUID_LEN);
}

Datum
uuid_lt(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_BOOL(uuid_internal_cmp(arg1, arg2) < 0);
}

Datum
uuid_le(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_BOOL(uuid_internal_cmp(arg1, arg2) <= 0);
}

Datum
uuid_eq(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_BOOL(uuid_internal_cmp(arg1, arg2) == 0);
}

Datum
uuid_ge(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_BOOL(uuid_internal_cmp(arg1, arg2) >= 0);
}

Datum
uuid_gt(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_BOOL(uuid_internal_cmp(arg1, arg2) > 0);
}

Datum
uuid_ne(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_BOOL(uuid_internal_cmp(arg1, arg2) != 0);
}

                                      
Datum
uuid_cmp(PG_FUNCTION_ARGS)
{
  pg_uuid_t *arg1 = PG_GETARG_UUID_P(0);
  pg_uuid_t *arg2 = PG_GETARG_UUID_P(1);

  PG_RETURN_INT32(uuid_internal_cmp(arg1, arg2));
}

   
                                 
   
Datum
uuid_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = uuid_fast_cmp;
  ssup->ssup_extra = NULL;

  if (ssup->abbreviate)
  {
    uuid_sortsupport_state *uss;
    MemoryContext oldcontext;

    oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

    uss = palloc(sizeof(uuid_sortsupport_state));
    uss->input_count = 0;
    uss->estimating = true;
    initHyperLogLog(&uss->abbr_card, 10);

    ssup->ssup_extra = uss;

    ssup->comparator = uuid_cmp_abbrev;
    ssup->abbrev_converter = uuid_abbrev_convert;
    ssup->abbrev_abort = uuid_abbrev_abort;
    ssup->abbrev_full_comparator = uuid_fast_cmp;

    MemoryContextSwitchTo(oldcontext);
  }

  PG_RETURN_VOID();
}

   
                               
   
static int
uuid_fast_cmp(Datum x, Datum y, SortSupport ssup)
{
  pg_uuid_t *arg1 = DatumGetUUIDP(x);
  pg_uuid_t *arg2 = DatumGetUUIDP(y);

  return uuid_internal_cmp(arg1, arg2);
}

   
                                   
   
static int
uuid_cmp_abbrev(Datum x, Datum y, SortSupport ssup)
{
  if (x > y)
  {
    return 1;
  }
  else if (x == y)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}

   
                                                                          
   
                                                                               
                                                                        
   
static bool
uuid_abbrev_abort(int memtupcount, SortSupport ssup)
{
  uuid_sortsupport_state *uss = ssup->ssup_extra;
  double abbr_card;

  if (memtupcount < 10000 || uss->input_count < 10000 || !uss->estimating)
  {
    return false;
  }

  abbr_card = estimateHyperLogLog(&uss->abbr_card);

     
                                                                         
                                                                           
                                                                          
                             
     
  if (abbr_card > 100000.0)
  {
#ifdef TRACE_SORT
    if (trace_sort)
    {
      elog(LOG,
          "uuid_abbrev: estimation ends at cardinality %f"
          " after " INT64_FORMAT " values (%d rows)",
          abbr_card, uss->input_count, memtupcount);
    }
#endif
    uss->estimating = false;
    return false;
  }

     
                                                                          
                                                                            
                                                                   
                      
     
  if (abbr_card < uss->input_count / 2000.0 + 0.5)
  {
#ifdef TRACE_SORT
    if (trace_sort)
    {
      elog(LOG,
          "uuid_abbrev: aborting abbreviation at cardinality %f"
          " below threshold %f after " INT64_FORMAT " values (%d rows)",
          abbr_card, uss->input_count / 2000.0 + 0.5, uss->input_count, memtupcount);
    }
#endif
    return true;
  }

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "uuid_abbrev: cardinality %f after " INT64_FORMAT " values (%d rows)", abbr_card, uss->input_count, memtupcount);
  }
#endif

  return false;
}

   
                                                                              
                                                                               
                                                                               
                                                                        
                     
   
static Datum
uuid_abbrev_convert(Datum original, SortSupport ssup)
{
  uuid_sortsupport_state *uss = ssup->ssup_extra;
  pg_uuid_t *authoritative = DatumGetUUIDP(original);
  Datum res;

  memcpy(&res, authoritative->data, sizeof(Datum));
  uss->input_count += 1;

  if (uss->estimating)
  {
    uint32 tmp;

#if SIZEOF_DATUM == 8
    tmp = (uint32)res ^ (uint32)((uint64)res >> 32);
#else                        
    tmp = (uint32)res;
#endif

    addHyperLogLog(&uss->abbr_card, DatumGetUInt32(hash_uint32(tmp)));
  }

     
                                         
     
                                                                         
                                                                          
                                                                           
                                                              
     
  res = DatumBigEndianToNative(res);

  return res;
}

                        
Datum
uuid_hash(PG_FUNCTION_ARGS)
{
  pg_uuid_t *key = PG_GETARG_UUID_P(0);

  return hash_any(key->data, UUID_LEN);
}

Datum
uuid_hash_extended(PG_FUNCTION_ARGS)
{
  pg_uuid_t *key = PG_GETARG_UUID_P(0);

  return hash_any_extended(key->data, UUID_LEN, PG_GETARG_INT64(1));
}
