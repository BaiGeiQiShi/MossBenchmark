                                                                            
   
               
                                                                      
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <sys/time.h>

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "common/int128.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "parser/scansup.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/float.h"

   
                                                                           
                                                                      
   
#ifdef __FAST_MATH__
#error -ffast-math is known to break this code
#endif

#define SAMESIGN(a, b) (((a) < 0) == ((b) < 0))

                             
TimestampTz PgStartTime;

                                 
TimestampTz PgReloadTime;

typedef struct
{
  Timestamp current;
  Timestamp finish;
  Interval step;
  int step_sign;
} generate_series_timestamp_fctx;

typedef struct
{
  TimestampTz current;
  TimestampTz finish;
  Interval step;
  int step_sign;
} generate_series_timestamptz_fctx;

static TimeOffset
time2t(const int hour, const int min, const int sec, const fsec_t fsec);
static Timestamp
dt2local(Timestamp dt, int timezone);
static void
AdjustTimestampForTypmod(Timestamp *time, int32 typmod);
static void
AdjustIntervalForTypmod(Interval *interval, int32 typmod);
static TimestampTz
timestamp2timestamptz(Timestamp timestamp);
static Timestamp
timestamptz2timestamp(TimestampTz timestamp);

                                                               
static int32
anytimestamp_typmodin(bool istz, ArrayType *ta)
{
  int32 *tl;
  int n;

  tl = ArrayGetIntegerTypmods(ta, &n);

     
                                                                       
                                                             
     
  if (n != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid type modifier")));
  }

  return anytimestamp_typmod_check(istz, tl[0]);
}

                                         
int32
anytimestamp_typmod_check(bool istz, int32 typmod)
{
  if (typmod < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("TIMESTAMP(%d)%s precision must not be negative", typmod, (istz ? " WITH TIME ZONE" : ""))));
  }
  if (typmod > MAX_TIMESTAMP_PRECISION)
  {
    ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("TIMESTAMP(%d)%s precision reduced to maximum allowed, %d", typmod, (istz ? " WITH TIME ZONE" : ""), MAX_TIMESTAMP_PRECISION)));
    typmod = MAX_TIMESTAMP_PRECISION;
  }

  return typmod;
}

                                                                 
static char *
anytimestamp_typmodout(bool istz, int32 typmod)
{
  const char *tz = istz ? " with time zone" : " without time zone";

  if (typmod >= 0)
  {
    return psprintf("(%d)%s", (int)typmod, tz);
  }
  else
  {
    return psprintf("%s", tz);
  }
}

                                                                               
                                      
                                                                               

                  
                                      
   
Datum
timestamp_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  Timestamp result;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;
  int tz;
  int dtype;
  int nf;
  int dterr;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char workbuf[MAXDATELEN + MAXDATEFIELDS];

  dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
  if (dterr == 0)
  {
    dterr = DecodeDateTime(field, ftype, nf, &dtype, tm, &fsec, &tz);
  }
  if (dterr != 0)
  {
    DateTimeParseError(dterr, str, "timestamp");
  }

  switch (dtype)
  {
  case DTK_DATE:
    if (tm2timestamp(tm, fsec, NULL, &result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: \"%s\"", str)));
    }
    break;

  case DTK_EPOCH:
    result = SetEpochTimestamp();
    break;

  case DTK_LATE:
    TIMESTAMP_NOEND(result);
    break;

  case DTK_EARLY:
    TIMESTAMP_NOBEGIN(result);
    break;

  default:
    elog(ERROR, "unexpected dtype %d while parsing timestamp \"%s\"", dtype, str);
    TIMESTAMP_NOEND(result);
  }

  AdjustTimestampForTypmod(&result, typmod);

  PG_RETURN_TIMESTAMP(result);
}

                   
                                         
   
Datum
timestamp_out(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  char *result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  char buf[MAXDATELEN + 1];

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    EncodeSpecialTimestamp(timestamp, buf);
  }
  else if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) == 0)
  {
    EncodeDateTime(tm, fsec, false, 0, NULL, DateStyle, buf);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                                    
   
Datum
timestamp_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  Timestamp timestamp;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;

  timestamp = (Timestamp)pq_getmsgint64(buf);

                                                       
  if (TIMESTAMP_NOT_FINITE(timestamp))
            ;
  else if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0 || !IS_VALID_TIMESTAMP(timestamp))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  AdjustTimestampForTypmod(&timestamp, typmod);

  PG_RETURN_TIMESTAMP(timestamp);
}

   
                                                           
   
Datum
timestamp_send(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, timestamp);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum
timestamptypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anytimestamp_typmodin(false, ta));
}

Datum
timestamptypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anytimestamp_typmodout(false, typmod));
}

   
                       
   
                                                                              
                                                                  
   
Datum
timestamp_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSimplify))
  {
    SupportRequestSimplify *req = (SupportRequestSimplify *)rawreq;

    ret = TemporalSimplify(MAX_TIMESTAMP_PRECISION, (Node *)req->fcall);
  }

  PG_RETURN_POINTER(ret);
}

                     
                                                
                                                    
   
Datum
timestamp_scale(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  int32 typmod = PG_GETARG_INT32(1);
  Timestamp result;

  result = timestamp;

  AdjustTimestampForTypmod(&result, typmod);

  PG_RETURN_TIMESTAMP(result);
}

   
                                                                           
                                              
   
static void
AdjustTimestampForTypmod(Timestamp *time, int32 typmod)
{
  static const int64 TimestampScales[MAX_TIMESTAMP_PRECISION + 1] = {INT64CONST(1000000), INT64CONST(100000), INT64CONST(10000), INT64CONST(1000), INT64CONST(100), INT64CONST(10), INT64CONST(1)};

  static const int64 TimestampOffsets[MAX_TIMESTAMP_PRECISION + 1] = {INT64CONST(500000), INT64CONST(50000), INT64CONST(5000), INT64CONST(500), INT64CONST(50), INT64CONST(5), INT64CONST(0)};

  if (!TIMESTAMP_NOT_FINITE(*time) && (typmod != -1) && (typmod != MAX_TIMESTAMP_PRECISION))
  {
    if (typmod < 0 || typmod > MAX_TIMESTAMP_PRECISION)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp(%d) precision must be between %d and %d", typmod, 0, MAX_TIMESTAMP_PRECISION)));
    }

    if (*time >= INT64CONST(0))
    {
      *time = ((*time + TimestampOffsets[typmod]) / TimestampScales[typmod]) * TimestampScales[typmod];
    }
    else
    {
      *time = -((((-*time) + TimestampOffsets[typmod]) / TimestampScales[typmod]) * TimestampScales[typmod]);
    }
  }
}

                    
                                      
   
Datum
timestamptz_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  TimestampTz result;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;
  int tz;
  int dtype;
  int nf;
  int dterr;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char workbuf[MAXDATELEN + MAXDATEFIELDS];

  dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
  if (dterr == 0)
  {
    dterr = DecodeDateTime(field, ftype, nf, &dtype, tm, &fsec, &tz);
  }
  if (dterr != 0)
  {
    DateTimeParseError(dterr, str, "timestamp with time zone");
  }

  switch (dtype)
  {
  case DTK_DATE:
    if (tm2timestamp(tm, fsec, &tz, &result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: \"%s\"", str)));
    }
    break;

  case DTK_EPOCH:
    result = SetEpochTimestamp();
    break;

  case DTK_LATE:
    TIMESTAMP_NOEND(result);
    break;

  case DTK_EARLY:
    TIMESTAMP_NOBEGIN(result);
    break;

  default:
    elog(ERROR, "unexpected dtype %d while parsing timestamptz \"%s\"", dtype, str);
    TIMESTAMP_NOEND(result);
  }

  AdjustTimestampForTypmod(&result, typmod);

  PG_RETURN_TIMESTAMPTZ(result);
}

   
                                                                               
                                                       
   
                                                                              
                                                    
   
static int
parse_sane_timezone(struct pg_tm *tm, text *zone)
{
  char tzname[TZ_STRLEN_MAX + 1];
  int rt;
  int tz;

  text_to_cstring_buffer(zone, tzname, sizeof(tzname));

     
                                                                        
                                                                          
                                                                            
                                                                        
                                                                            
                                                                             
                                                                             
                               
     
                                                                          
                                                                           
                                                                     
                                   
     
  if (isdigit((unsigned char)*tzname))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid input syntax for type %s: \"%s\"", "numeric time zone", tzname), errhint("Numeric time zones must have \"-\" or \"+\" as first character.")));
  }

  rt = DecodeTimezone(tzname, &tz);
  if (rt != 0)
  {
    char *lowzone;
    int type, val;
    pg_tz *tzp;

    if (rt == DTERR_TZDISP_OVERFLOW)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("numeric time zone \"%s\" out of range", tzname)));
    }
    else if (rt != DTERR_BAD_FORMAT)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", tzname)));
    }

                                                       
    lowzone = downcase_truncate_identifier(tzname, strlen(tzname), false);
    type = DecodeTimezoneAbbrev(0, lowzone, &val, &tzp);

    if (type == TZ || type == DTZ)
    {
                                     
      tz = -val;
    }
    else if (type == DYNTZ)
    {
                                                                     
      tz = DetermineTimeZoneAbbrevOffset(tm, tzname, tzp);
    }
    else
    {
                                      
      tzp = pg_tzset(tzname);
      if (tzp)
      {
        tz = DetermineTimeZoneOffset(tm, tzp);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", tzname)));
      }
    }
  }

  return tz;
}

   
                           
                                                      
   
static Timestamp
make_timestamp_internal(int year, int month, int day, int hour, int min, double sec)
{
  struct pg_tm tm;
  TimeOffset date;
  TimeOffset time;
  int dterr;
  Timestamp result;

  tm.tm_year = year;
  tm.tm_mon = month;
  tm.tm_mday = day;

     
                                                                         
                                              
     
  dterr = ValidateDate(DTK_DATE_M, false, false, false, &tm);

  if (dterr != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_FIELD_OVERFLOW), errmsg("date field value out of range: %d-%02d-%02d", year, month, day)));
  }

  if (!IS_VALID_JULIAN(tm.tm_year, tm.tm_mon, tm.tm_mday))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: %d-%02d-%02d", year, month, day)));
  }

  date = date2j(tm.tm_year, tm.tm_mon, tm.tm_mday) - POSTGRES_EPOCH_JDATE;

                               
  if (float_time_overflows(hour, min, sec))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_FIELD_OVERFLOW), errmsg("time field value out of range: %d:%02d:%02g", hour, min, sec)));
  }

                                 
  time = (((hour * MINS_PER_HOUR + min) * SECS_PER_MINUTE) * USECS_PER_SEC) + (int64)rint(sec * USECS_PER_SEC);

  result = date * USECS_PER_DAY + time;
                                
  if ((result - time) / USECS_PER_DAY != date)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: %d-%02d-%02d %d:%02d:%02g", year, month, day, hour, min, sec)));
  }

                                                                      
                                                     
  if ((result < 0 && date > 0) || (result > 0 && date < -1))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: %d-%02d-%02d %d:%02d:%02g", year, month, day, hour, min, sec)));
  }

                                                              
  if (!IS_VALID_TIMESTAMP(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: %d-%02d-%02d %d:%02d:%02g", year, month, day, hour, min, sec)));
  }

  return result;
}

   
                                            
   
Datum
make_timestamp(PG_FUNCTION_ARGS)
{
  int32 year = PG_GETARG_INT32(0);
  int32 month = PG_GETARG_INT32(1);
  int32 mday = PG_GETARG_INT32(2);
  int32 hour = PG_GETARG_INT32(3);
  int32 min = PG_GETARG_INT32(4);
  float8 sec = PG_GETARG_FLOAT8(5);
  Timestamp result;

  result = make_timestamp_internal(year, month, mday, hour, min, sec);

  PG_RETURN_TIMESTAMP(result);
}

   
                                                             
   
Datum
make_timestamptz(PG_FUNCTION_ARGS)
{
  int32 year = PG_GETARG_INT32(0);
  int32 month = PG_GETARG_INT32(1);
  int32 mday = PG_GETARG_INT32(2);
  int32 hour = PG_GETARG_INT32(3);
  int32 min = PG_GETARG_INT32(4);
  float8 sec = PG_GETARG_FLOAT8(5);
  Timestamp result;

  result = make_timestamp_internal(year, month, mday, hour, min, sec);

  PG_RETURN_TIMESTAMPTZ(timestamp2timestamptz(result));
}

   
                                         
                                                                  
   
Datum
make_timestamptz_at_timezone(PG_FUNCTION_ARGS)
{
  int32 year = PG_GETARG_INT32(0);
  int32 month = PG_GETARG_INT32(1);
  int32 mday = PG_GETARG_INT32(2);
  int32 hour = PG_GETARG_INT32(3);
  int32 min = PG_GETARG_INT32(4);
  float8 sec = PG_GETARG_FLOAT8(5);
  text *zone = PG_GETARG_TEXT_PP(6);
  TimestampTz result;
  Timestamp timestamp;
  struct pg_tm tt;
  int tz;
  fsec_t fsec;

  timestamp = make_timestamp_internal(year, month, mday, hour, min, sec);

  if (timestamp2tm(timestamp, NULL, &tt, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  tz = parse_sane_timezone(&tt, zone);

  result = dt2local(timestamp, -tz);

  if (!IS_VALID_TIMESTAMP(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_TIMESTAMPTZ(result);
}

   
                                  
                                      
   
Datum
float8_timestamptz(PG_FUNCTION_ARGS)
{
  float8 seconds = PG_GETARG_FLOAT8(0);
  TimestampTz result;

                                             
  if (isnan(seconds))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp cannot be NaN")));
  }

  if (isinf(seconds))
  {
    if (seconds < 0)
    {
      TIMESTAMP_NOBEGIN(result);
    }
    else
    {
      TIMESTAMP_NOEND(result);
    }
  }
  else
  {
                       
    if (seconds < (float8)SECS_PER_DAY * (DATETIME_MIN_JULIAN - UNIX_EPOCH_JDATE) || seconds >= (float8)SECS_PER_DAY * (TIMESTAMP_END_JULIAN - UNIX_EPOCH_JDATE))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: \"%g\"", seconds)));
    }

                                              
    seconds -= ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);

    seconds = rint(seconds * USECS_PER_SEC);
    result = (int64)seconds;

                                                                       
    if (!IS_VALID_TIMESTAMP(result))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range: \"%g\"", PG_GETARG_FLOAT8(0))));
    }
  }

  PG_RETURN_TIMESTAMP(result);
}

                     
                                         
   
Datum
timestamptz_out(PG_FUNCTION_ARGS)
{
  TimestampTz dt = PG_GETARG_TIMESTAMPTZ(0);
  char *result;
  int tz;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  const char *tzn;
  char buf[MAXDATELEN + 1];

  if (TIMESTAMP_NOT_FINITE(dt))
  {
    EncodeSpecialTimestamp(dt, buf);
  }
  else if (timestamp2tm(dt, &tz, tm, &fsec, &tzn, NULL) == 0)
  {
    EncodeDateTime(tm, fsec, true, tz, tzn, DateStyle, buf);
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                                        
   
Datum
timestamptz_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  TimestampTz timestamp;
  int tz;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;

  timestamp = (TimestampTz)pq_getmsgint64(buf);

                                                         
  if (TIMESTAMP_NOT_FINITE(timestamp))
            ;
  else if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0 || !IS_VALID_TIMESTAMP(timestamp))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  AdjustTimestampForTypmod(&timestamp, typmod);

  PG_RETURN_TIMESTAMPTZ(timestamp);
}

   
                                                               
   
Datum
timestamptz_send(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, timestamp);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum
timestamptztypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anytimestamp_typmodin(true, ta));
}

Datum
timestamptztypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anytimestamp_typmodout(true, typmod));
}

                       
                                                
                                                    
   
Datum
timestamptz_scale(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(0);
  int32 typmod = PG_GETARG_INT32(1);
  TimestampTz result;

  result = timestamp;

  AdjustTimestampForTypmod(&result, typmod);

  PG_RETURN_TIMESTAMPTZ(result);
}

                 
                                      
   
                       
                                                             
   
Datum
interval_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  Interval *result;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;
  int dtype;
  int nf;
  int range;
  int dterr;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char workbuf[256];

  tm->tm_year = 0;
  tm->tm_mon = 0;
  tm->tm_mday = 0;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  fsec = 0;

  if (typmod >= 0)
  {
    range = INTERVAL_RANGE(typmod);
  }
  else
  {
    range = INTERVAL_FULL_RANGE;
  }

  dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
  if (dterr == 0)
  {
    dterr = DecodeInterval(field, ftype, nf, range, &dtype, tm, &fsec);
  }

                                                                     
  if (dterr == DTERR_BAD_FORMAT)
  {
    dterr = DecodeISO8601Interval(str, &dtype, tm, &fsec);
  }

  if (dterr != 0)
  {
    if (dterr == DTERR_FIELD_OVERFLOW)
    {
      dterr = DTERR_INTERVAL_OVERFLOW;
    }
    DateTimeParseError(dterr, str, "interval");
  }

  result = (Interval *)palloc(sizeof(Interval));

  switch (dtype)
  {
  case DTK_DELTA:
    if (tm2interval(tm, fsec, result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
    }
    break;

  default:
    elog(ERROR, "unexpected dtype %d while parsing interval \"%s\"", dtype, str);
  }

  AdjustIntervalForTypmod(result, typmod);

  PG_RETURN_INTERVAL_P(result);
}

                  
                                         
   
Datum
interval_out(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  char *result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  char buf[MAXDATELEN + 1];

  if (interval2tm(*span, tm, &fsec) != 0)
  {
    elog(ERROR, "could not convert interval to tm");
  }

  EncodeInterval(tm, fsec, IntervalStyle, buf);

  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                                  
   
Datum
interval_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  Interval *interval;

  interval = (Interval *)palloc(sizeof(Interval));

  interval->time = pq_getmsgint64(buf);
  interval->day = pq_getmsgint(buf, sizeof(interval->day));
  interval->month = pq_getmsgint(buf, sizeof(interval->month));

  AdjustIntervalForTypmod(interval, typmod);

  PG_RETURN_INTERVAL_P(interval);
}

   
                                                         
   
Datum
interval_send(PG_FUNCTION_ARGS)
{
  Interval *interval = PG_GETARG_INTERVAL_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, interval->time);
  pq_sendint32(&buf, interval->day);
  pq_sendint32(&buf, interval->month);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                                                              
                                                                          
                                                                          
                                                                             
                                                                               
                                                                            
                                                                             
                                                                               
                                                                        
                                                 
   
Datum
intervaltypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);
  int32 *tl;
  int n;
  int32 typmod;

  tl = ArrayGetIntegerTypmods(ta, &n);

     
                                                                          
     
                                                                      
                                                                         
     
  if (n > 0)
  {
    switch (tl[0])
    {
    case INTERVAL_MASK(YEAR):
    case INTERVAL_MASK(MONTH):
    case INTERVAL_MASK(DAY):
    case INTERVAL_MASK(HOUR):
    case INTERVAL_MASK(MINUTE):
    case INTERVAL_MASK(SECOND):
    case INTERVAL_MASK(YEAR) | INTERVAL_MASK(MONTH):
    case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR):
    case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
    case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
    case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    case INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    case INTERVAL_FULL_RANGE:
                  
      break;
    default:
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid INTERVAL type modifier")));
    }
  }

  if (n == 1)
  {
    if (tl[0] != INTERVAL_FULL_RANGE)
    {
      typmod = INTERVAL_TYPMOD(INTERVAL_FULL_PRECISION, tl[0]);
    }
    else
    {
      typmod = -1;
    }
  }
  else if (n == 2)
  {
    if (tl[1] < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("INTERVAL(%d) precision must not be negative", tl[1])));
    }
    if (tl[1] > MAX_INTERVAL_PRECISION)
    {
      ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("INTERVAL(%d) precision reduced to maximum allowed, %d", tl[1], MAX_INTERVAL_PRECISION)));
      typmod = INTERVAL_TYPMOD(MAX_INTERVAL_PRECISION, tl[0]);
    }
    else
    {
      typmod = INTERVAL_TYPMOD(tl[1], tl[0]);
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid INTERVAL type modifier")));
    typmod = 0;                          
  }

  PG_RETURN_INT32(typmod);
}

Datum
intervaltypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);
  char *res = (char *)palloc(64);
  int fields;
  int precision;
  const char *fieldstr;

  if (typmod < 0)
  {
    *res = '\0';
    PG_RETURN_CSTRING(res);
  }

  fields = INTERVAL_RANGE(typmod);
  precision = INTERVAL_PRECISION(typmod);

  switch (fields)
  {
  case INTERVAL_MASK(YEAR):
    fieldstr = " year";
    break;
  case INTERVAL_MASK(MONTH):
    fieldstr = " month";
    break;
  case INTERVAL_MASK(DAY):
    fieldstr = " day";
    break;
  case INTERVAL_MASK(HOUR):
    fieldstr = " hour";
    break;
  case INTERVAL_MASK(MINUTE):
    fieldstr = " minute";
    break;
  case INTERVAL_MASK(SECOND):
    fieldstr = " second";
    break;
  case INTERVAL_MASK(YEAR) | INTERVAL_MASK(MONTH):
    fieldstr = " year to month";
    break;
  case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR):
    fieldstr = " day to hour";
    break;
  case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
    fieldstr = " day to minute";
    break;
  case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    fieldstr = " day to second";
    break;
  case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
    fieldstr = " hour to minute";
    break;
  case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    fieldstr = " hour to second";
    break;
  case INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    fieldstr = " minute to second";
    break;
  case INTERVAL_FULL_RANGE:
    fieldstr = "";
    break;
  default:
    elog(ERROR, "invalid INTERVAL typmod: 0x%x", typmod);
    fieldstr = "";
    break;
  }

  if (precision != INTERVAL_FULL_PRECISION)
  {
    snprintf(res, 64, "%s(%d)", fieldstr, precision);
  }
  else
  {
    snprintf(res, 64, "%s", fieldstr);
  }

  PG_RETURN_CSTRING(res);
}

   
                                                                           
                                                                  
                                                    
   
                                                                    
                                                                      
                                                                 
                                                                   
   
static int
intervaltypmodleastfield(int32 typmod)
{
  if (typmod < 0)
  {
    return 0;             
  }

  switch (INTERVAL_RANGE(typmod))
  {
  case INTERVAL_MASK(YEAR):
    return 5;           
  case INTERVAL_MASK(MONTH):
    return 4;            
  case INTERVAL_MASK(DAY):
    return 3;          
  case INTERVAL_MASK(HOUR):
    return 2;           
  case INTERVAL_MASK(MINUTE):
    return 1;             
  case INTERVAL_MASK(SECOND):
    return 0;             
  case INTERVAL_MASK(YEAR) | INTERVAL_MASK(MONTH):
    return 4;            
  case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR):
    return 2;           
  case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
    return 1;             
  case INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    return 0;             
  case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE):
    return 1;             
  case INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    return 0;             
  case INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND):
    return 0;             
  case INTERVAL_FULL_RANGE:
    return 0;             
  default:
    elog(ERROR, "invalid INTERVAL typmod: 0x%x", typmod);
    break;
  }
  return 0;                                              
}

   
                      
   
                                                  
   
                                                                          
                                                                              
                                                                           
   
Datum
interval_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSimplify))
  {
    SupportRequestSimplify *req = (SupportRequestSimplify *)rawreq;
    FuncExpr *expr = req->fcall;
    Node *typmod;

    Assert(list_length(expr->args) >= 2);

    typmod = (Node *)lsecond(expr->args);

    if (IsA(typmod, Const) && !((Const *)typmod)->constisnull)
    {
      Node *source = (Node *)linitial(expr->args);
      int32 new_typmod = DatumGetInt32(((Const *)typmod)->constvalue);
      bool noop;

      if (new_typmod < 0)
      {
        noop = true;
      }
      else
      {
        int32 old_typmod = exprTypmod(source);
        int old_least_field;
        int new_least_field;
        int old_precis;
        int new_precis;

        old_least_field = intervaltypmodleastfield(old_typmod);
        new_least_field = intervaltypmodleastfield(new_typmod);
        if (old_typmod < 0)
        {
          old_precis = INTERVAL_FULL_PRECISION;
        }
        else
        {
          old_precis = INTERVAL_PRECISION(old_typmod);
        }
        new_precis = INTERVAL_PRECISION(new_typmod);

           
                                                                      
                                                             
                                                                  
                                               
           
        noop = (new_least_field <= old_least_field) && (old_least_field > 0              || new_precis >= MAX_INTERVAL_PRECISION || new_precis >= old_precis);
      }
      if (noop)
      {
        ret = relabel_to_typmod(source, new_typmod);
      }
    }
  }

  PG_RETURN_POINTER(ret);
}

                    
                                              
                                                    
   
Datum
interval_scale(PG_FUNCTION_ARGS)
{
  Interval *interval = PG_GETARG_INTERVAL_P(0);
  int32 typmod = PG_GETARG_INT32(1);
  Interval *result;

  result = palloc(sizeof(Interval));
  *result = *interval;

  AdjustIntervalForTypmod(result, typmod);

  PG_RETURN_INTERVAL_P(result);
}

   
                                                                   
                                   
   
static void
AdjustIntervalForTypmod(Interval *interval, int32 typmod)
{
  static const int64 IntervalScales[MAX_INTERVAL_PRECISION + 1] = {INT64CONST(1000000), INT64CONST(100000), INT64CONST(10000), INT64CONST(1000), INT64CONST(100), INT64CONST(10), INT64CONST(1)};

  static const int64 IntervalOffsets[MAX_INTERVAL_PRECISION + 1] = {INT64CONST(500000), INT64CONST(50000), INT64CONST(5000), INT64CONST(500), INT64CONST(50), INT64CONST(5), INT64CONST(0)};

     
                                                                            
                                                        
     
  if (typmod >= 0)
  {
    int range = INTERVAL_RANGE(typmod);
    int precision = INTERVAL_PRECISION(typmod);

       
                                                                       
                                                                          
                                                                         
                                                                       
                                                                         
                                                                           
                                                                          
                                                                          
                                                                      
                                                                        
                                                                     
       
                                                                     
                                                                           
                                                                         
                                                                         
                                                                    
                                                           
       
    if (range == INTERVAL_FULL_RANGE)
    {
                         
    }
    else if (range == INTERVAL_MASK(YEAR))
    {
      interval->month = (interval->month / MONTHS_PER_YEAR) * MONTHS_PER_YEAR;
      interval->day = 0;
      interval->time = 0;
    }
    else if (range == INTERVAL_MASK(MONTH))
    {
      interval->day = 0;
      interval->time = 0;
    }
                       
    else if (range == (INTERVAL_MASK(YEAR) | INTERVAL_MASK(MONTH)))
    {
      interval->day = 0;
      interval->time = 0;
    }
    else if (range == INTERVAL_MASK(DAY))
    {
      interval->time = 0;
    }
    else if (range == INTERVAL_MASK(HOUR))
    {
      interval->time = (interval->time / USECS_PER_HOUR) * USECS_PER_HOUR;
    }
    else if (range == INTERVAL_MASK(MINUTE))
    {
      interval->time = (interval->time / USECS_PER_MINUTE) * USECS_PER_MINUTE;
    }
    else if (range == INTERVAL_MASK(SECOND))
    {
                                                               
    }
                     
    else if (range == (INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR)))
    {
      interval->time = (interval->time / USECS_PER_HOUR) * USECS_PER_HOUR;
    }
                       
    else if (range == (INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE)))
    {
      interval->time = (interval->time / USECS_PER_MINUTE) * USECS_PER_MINUTE;
    }
                       
    else if (range == (INTERVAL_MASK(DAY) | INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND)))
    {
                                                               
    }
                        
    else if (range == (INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE)))
    {
      interval->time = (interval->time / USECS_PER_MINUTE) * USECS_PER_MINUTE;
    }
                        
    else if (range == (INTERVAL_MASK(HOUR) | INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND)))
    {
                                                               
    }
                          
    else if (range == (INTERVAL_MASK(MINUTE) | INTERVAL_MASK(SECOND)))
    {
                                                               
    }
    else
    {
      elog(ERROR, "unrecognized interval typmod: %d", typmod);
    }

                                              
    if (precision != INTERVAL_FULL_PRECISION)
    {
      if (precision < 0 || precision > MAX_INTERVAL_PRECISION)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("interval(%d) precision must be between %d and %d", precision, 0, MAX_INTERVAL_PRECISION)));
      }

      if (interval->time >= INT64CONST(0))
      {
        interval->time = ((interval->time + IntervalOffsets[precision]) / IntervalScales[precision]) * IntervalScales[precision];
      }
      else
      {
        interval->time = -(((-interval->time + IntervalOffsets[precision]) / IntervalScales[precision]) * IntervalScales[precision]);
      }
    }
  }
}

   
                                                
   
Datum
make_interval(PG_FUNCTION_ARGS)
{
  int32 years = PG_GETARG_INT32(0);
  int32 months = PG_GETARG_INT32(1);
  int32 weeks = PG_GETARG_INT32(2);
  int32 days = PG_GETARG_INT32(3);
  int32 hours = PG_GETARG_INT32(4);
  int32 mins = PG_GETARG_INT32(5);
  double secs = PG_GETARG_FLOAT8(6);
  Interval *result;

     
                                                                       
                                                                       
     
  if (isinf(secs) || isnan(secs))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  result = (Interval *)palloc(sizeof(Interval));
  result->month = years * MONTHS_PER_YEAR + months;
  result->day = weeks * 7 + days;

  secs = rint(secs * USECS_PER_SEC);
  result->time = hours * ((int64)SECS_PER_HOUR * USECS_PER_SEC) + mins * ((int64)SECS_PER_MINUTE * USECS_PER_SEC) + (int64)secs;

  PG_RETURN_INTERVAL_P(result);
}

                            
                                                   
   
void
EncodeSpecialTimestamp(Timestamp dt, char *str)
{
  if (TIMESTAMP_IS_NOBEGIN(dt))
  {
    strcpy(str, EARLY);
  }
  else if (TIMESTAMP_IS_NOEND(dt))
  {
    strcpy(str, LATE);
  }
  else                       
  {
    elog(ERROR, "invalid argument for EncodeSpecialTimestamp");
  }
}

Datum
now(PG_FUNCTION_ARGS)
{
  PG_RETURN_TIMESTAMPTZ(GetCurrentTransactionStartTimestamp());
}

Datum
statement_timestamp(PG_FUNCTION_ARGS)
{
  PG_RETURN_TIMESTAMPTZ(GetCurrentStatementStartTimestamp());
}

Datum
clock_timestamp(PG_FUNCTION_ARGS)
{
  PG_RETURN_TIMESTAMPTZ(GetCurrentTimestamp());
}

Datum
pg_postmaster_start_time(PG_FUNCTION_ARGS)
{
  PG_RETURN_TIMESTAMPTZ(PgStartTime);
}

Datum
pg_conf_load_time(PG_FUNCTION_ARGS)
{
  PG_RETURN_TIMESTAMPTZ(PgReloadTime);
}

   
                                                                
   
                                                                         
                                                
   
TimestampTz
GetCurrentTimestamp(void)
{
  TimestampTz result;
  struct timeval tp;

  gettimeofday(&tp, NULL);

  result = (TimestampTz)tp.tv_sec - ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);
  result = (result * USECS_PER_SEC) + tp.tv_usec;

  return result;
}

   
                                                                                
   
TimestampTz
GetSQLCurrentTimestamp(int32 typmod)
{
  TimestampTz ts;

  ts = GetCurrentTransactionStartTimestamp();
  if (typmod >= 0)
  {
    AdjustTimestampForTypmod(&ts, typmod);
  }
  return ts;
}

   
                                                                        
   
Timestamp
GetSQLLocalTimestamp(int32 typmod)
{
  Timestamp ts;

  ts = timestamptz2timestamp(GetCurrentTransactionStartTimestamp());
  if (typmod >= 0)
  {
    AdjustTimestampForTypmod(&ts, typmod);
  }
  return ts;
}

   
                                                       
   
Datum
timeofday(PG_FUNCTION_ARGS)
{
  struct timeval tp;
  char templ[128];
  char buf[128];
  pg_time_t tt;

  gettimeofday(&tp, NULL);
  tt = (pg_time_t)tp.tv_sec;
  pg_strftime(templ, sizeof(templ), "%a %b %d %H:%M:%S.%%06d %Y %Z", pg_localtime(&tt, session_timezone));
  snprintf(buf, sizeof(buf), templ, tp.tv_usec);

  PG_RETURN_TEXT_P(cstring_to_text(buf));
}

   
                                                                        
                                          
   
                                                                     
                                                             
   
                                                                     
                                                   
   
                                                                
                                                                      
   
void
TimestampDifference(TimestampTz start_time, TimestampTz stop_time, long *secs, int *microsecs)
{
  TimestampTz diff = stop_time - start_time;

  if (diff <= 0)
  {
    *secs = 0;
    *microsecs = 0;
  }
  else
  {
    *secs = (long)(diff / USECS_PER_SEC);
    *microsecs = (int)(diff % USECS_PER_SEC);
  }
}

   
                                                                         
                                          
   
                                                                      
                                                                   
                                                                      
                                                                      
                                                                     
   
                                                                     
                                                   
   
                                                               
                                                                      
   
                                                                       
                                                  
   
long
TimestampDifferenceMilliseconds(TimestampTz start_time, TimestampTz stop_time)
{
  TimestampTz diff = stop_time - start_time;

  if (diff <= 0)
  {
    return 0;
  }
  else
  {
    return (long)((diff + 999) / 1000);
  }
}

   
                                                                           
                                                             
   
                                                                     
                                                   
   
bool
TimestampDifferenceExceeds(TimestampTz start_time, TimestampTz stop_time, int msec)
{
  TimestampTz diff = stop_time - start_time;

  return (diff >= msec * INT64CONST(1000));
}

   
                                    
   
                                                                             
                                                               
   
                                                                             
                                                                       
                                                                            
                                                                              
   
TimestampTz
time_t_to_timestamptz(pg_time_t tm)
{
  TimestampTz result;

  result = (TimestampTz)tm - ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY);
  result *= USECS_PER_SEC;

  return result;
}

   
                                    
   
                                                                
   
                                                                             
                                                                     
                                                                            
                                                                              
   
pg_time_t
timestamptz_to_time_t(TimestampTz t)
{
  pg_time_t result;

  result = (pg_time_t)(t / USECS_PER_SEC + ((POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY));

  return result;
}

   
                                                       
   
                                                                        
                                                                         
                                                              
   
const char *
timestamptz_to_str(TimestampTz t)
{
  static char buf[MAXDATELEN + 1];
  int tz;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  const char *tzn;

  if (TIMESTAMP_NOT_FINITE(t))
  {
    EncodeSpecialTimestamp(t, buf);
  }
  else if (timestamp2tm(t, &tz, tm, &fsec, &tzn, NULL) == 0)
  {
    EncodeDateTime(tm, fsec, true, tz, tzn, USE_ISO_DATES, buf);
  }
  else
  {
    strlcpy(buf, "(timestamp out of range)", sizeof(buf));
  }

  return buf;
}

void
dt2time(Timestamp jd, int *hour, int *min, int *sec, fsec_t *fsec)
{
  TimeOffset time;

  time = jd;

  *hour = time / USECS_PER_HOUR;
  time -= (*hour) * USECS_PER_HOUR;
  *min = time / USECS_PER_MINUTE;
  time -= (*min) * USECS_PER_MINUTE;
  *sec = time / USECS_PER_SEC;
  *fsec = time - (*sec * USECS_PER_SEC);
}                

   
                                                                         
   
                                                                      
                                               
            
                 
                      
   
                                                                    
   
int
timestamp2tm(Timestamp dt, int *tzp, struct pg_tm *tm, fsec_t *fsec, const char **tzn, pg_tz *attimezone)
{
  Timestamp date;
  Timestamp time;
  pg_time_t utime;

                                                       
  if (attimezone == NULL)
  {
    attimezone = session_timezone;
  }

  time = dt;
  TMODULO(time, date, USECS_PER_DAY);

  if (time < INT64CONST(0))
  {
    time += USECS_PER_DAY;
    date -= 1;
  }

                                                                
  date += POSTGRES_EPOCH_JDATE;

                                                                 
  if (date < 0 || date > (Timestamp)INT_MAX)
  {
    return -1;
  }

  j2date((int)date, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
  dt2time(time, &tm->tm_hour, &tm->tm_min, &tm->tm_sec, fsec);

                                       
  if (tzp == NULL)
  {
    tm->tm_isdst = -1;
    tm->tm_gmtoff = 0;
    tm->tm_zone = NULL;
    if (tzn != NULL)
    {
      *tzn = NULL;
    }
    return 0;
  }

     
                                                                            
                                    
     
                                                                
                                                                         
                                                                             
                                                                            
                                                           
     
  dt = (dt - *fsec) / USECS_PER_SEC + (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * SECS_PER_DAY;
  utime = (pg_time_t)dt;
  if ((Timestamp)utime == dt)
  {
    struct pg_tm *tx = pg_localtime(&utime, attimezone);

    tm->tm_year = tx->tm_year + 1900;
    tm->tm_mon = tx->tm_mon + 1;
    tm->tm_mday = tx->tm_mday;
    tm->tm_hour = tx->tm_hour;
    tm->tm_min = tx->tm_min;
    tm->tm_sec = tx->tm_sec;
    tm->tm_isdst = tx->tm_isdst;
    tm->tm_gmtoff = tx->tm_gmtoff;
    tm->tm_zone = tx->tm_zone;
    *tzp = -tm->tm_gmtoff;
    if (tzn != NULL)
    {
      *tzn = tm->tm_zone;
    }
  }
  else
  {
       
                                                    
       
    *tzp = 0;
                                               
    tm->tm_isdst = -1;
    tm->tm_gmtoff = 0;
    tm->tm_zone = NULL;
    if (tzn != NULL)
    {
      *tzn = NULL;
    }
  }

  return 0;
}

                  
                                                    
                                                                      
                                               
   
                                               
   
int
tm2timestamp(struct pg_tm *tm, fsec_t fsec, int *tzp, Timestamp *result)
{
  TimeOffset date;
  TimeOffset time;

                                               
  if (!IS_VALID_JULIAN(tm->tm_year, tm->tm_mon, tm->tm_mday))
  {
    *result = 0;                          
    return -1;
  }

  date = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - POSTGRES_EPOCH_JDATE;
  time = time2t(tm->tm_hour, tm->tm_min, tm->tm_sec, fsec);

  *result = date * USECS_PER_DAY + time;
                                
  if ((*result - time) / USECS_PER_DAY != date)
  {
    *result = 0;                          
    return -1;
  }
                                                                      
                                                     
  if ((*result < 0 && date > 0) || (*result > 0 && date < -1))
  {
    *result = 0;                          
    return -1;
  }
  if (tzp != NULL)
  {
    *result = dt2local(*result, -(*tzp));
  }

                                                              
  if (!IS_VALID_TIMESTAMP(*result))
  {
    *result = 0;                          
    return -1;
  }

  return 0;
}

                 
                                                    
   
int
interval2tm(Interval span, struct pg_tm *tm, fsec_t *fsec)
{
  TimeOffset time;
  TimeOffset tfrac;

  tm->tm_year = span.month / MONTHS_PER_YEAR;
  tm->tm_mon = span.month % MONTHS_PER_YEAR;
  tm->tm_mday = span.day;
  time = span.time;

  tfrac = time / USECS_PER_HOUR;
  time -= tfrac * USECS_PER_HOUR;
  tm->tm_hour = tfrac;
  if (!SAMESIGN(tm->tm_hour, tfrac))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }
  tfrac = time / USECS_PER_MINUTE;
  time -= tfrac * USECS_PER_MINUTE;
  tm->tm_min = tfrac;
  tfrac = time / USECS_PER_SEC;
  *fsec = time - (tfrac * USECS_PER_SEC);
  tm->tm_sec = tfrac;

  return 0;
}

int
tm2interval(struct pg_tm *tm, fsec_t fsec, Interval *span)
{
  double total_months = (double)tm->tm_year * MONTHS_PER_YEAR + tm->tm_mon;

  if (total_months > INT_MAX || total_months < INT_MIN)
  {
    return -1;
  }
  span->month = total_months;
  span->day = tm->tm_mday;
  span->time = (((((tm->tm_hour * INT64CONST(60)) + tm->tm_min) * INT64CONST(60)) + tm->tm_sec) * USECS_PER_SEC) + fsec;

  return 0;
}

static TimeOffset
time2t(const int hour, const int min, const int sec, const fsec_t fsec)
{
  return (((((hour * MINS_PER_HOUR) + min) * SECS_PER_MINUTE) + sec) * USECS_PER_SEC) + fsec;
}

static Timestamp
dt2local(Timestamp dt, int tz)
{
  dt -= (tz * USECS_PER_SEC);
  return dt;
}

                                                                               
                                    
                                                                               

Datum
timestamp_finite(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);

  PG_RETURN_BOOL(!TIMESTAMP_NOT_FINITE(timestamp));
}

Datum
interval_finite(PG_FUNCTION_ARGS)
{
  PG_RETURN_BOOL(true);
}

                                                             
                                       
                                                             

void
GetEpochTime(struct pg_tm *tm)
{
  struct pg_tm *t0;
  pg_time_t epoch = 0;

  t0 = pg_gmtime(&epoch);

  if (t0 == NULL)
  {
    elog(ERROR, "could not convert epoch to timestamp: %m");
  }

  tm->tm_year = t0->tm_year;
  tm->tm_mon = t0->tm_mon;
  tm->tm_mday = t0->tm_mday;
  tm->tm_hour = t0->tm_hour;
  tm->tm_min = t0->tm_min;
  tm->tm_sec = t0->tm_sec;

  tm->tm_year += 1900;
  tm->tm_mon++;
}

Timestamp
SetEpochTimestamp(void)
{
  Timestamp dt;
  struct pg_tm tt, *tm = &tt;

  GetEpochTime(tm);
                                               
  tm2timestamp(tm, 0, NULL, &dt);

  return dt;
}                          

   
                                                                         
                                                                
   
                                                     
   
int
timestamp_cmp_internal(Timestamp dt1, Timestamp dt2)
{
  return (dt1 < dt2) ? -1 : ((dt1 > dt2) ? 1 : 0);
}

Datum
timestamp_eq(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) == 0);
}

Datum
timestamp_ne(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) != 0);
}

Datum
timestamp_lt(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) < 0);
}

Datum
timestamp_gt(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) > 0);
}

Datum
timestamp_le(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) <= 0);
}

Datum
timestamp_ge(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) >= 0);
}

Datum
timestamp_cmp(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);

  PG_RETURN_INT32(timestamp_cmp_internal(dt1, dt2));
}

                                             
static int
timestamp_fastcmp(Datum x, Datum y, SortSupport ssup)
{
  Timestamp a = DatumGetTimestamp(x);
  Timestamp b = DatumGetTimestamp(y);

  return timestamp_cmp_internal(a, b);
}

Datum
timestamp_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = timestamp_fastcmp;
  PG_RETURN_VOID();
}

Datum
timestamp_hash(PG_FUNCTION_ARGS)
{
  return hashint8(fcinfo);
}

Datum
timestamp_hash_extended(PG_FUNCTION_ARGS)
{
  return hashint8extended(fcinfo);
}

   
                                                                
   

Datum
timestamp_eq_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) == 0);
}

Datum
timestamp_ne_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) != 0);
}

Datum
timestamp_lt_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) < 0);
}

Datum
timestamp_gt_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) > 0);
}

Datum
timestamp_le_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) <= 0);
}

Datum
timestamp_ge_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) >= 0);
}

Datum
timestamp_cmp_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = timestamp2timestamptz(timestampVal);

  PG_RETURN_INT32(timestamp_cmp_internal(dt1, dt2));
}

Datum
timestamptz_eq_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) == 0);
}

Datum
timestamptz_ne_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) != 0);
}

Datum
timestamptz_lt_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) < 0);
}

Datum
timestamptz_gt_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) > 0);
}

Datum
timestamptz_le_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) <= 0);
}

Datum
timestamptz_ge_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) >= 0);
}

Datum
timestamptz_cmp_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  Timestamp timestampVal = PG_GETARG_TIMESTAMP(1);
  TimestampTz dt2;

  dt2 = timestamp2timestamptz(timestampVal);

  PG_RETURN_INT32(timestamp_cmp_internal(dt1, dt2));
}

   
                                                  
   
                                                                          
                                                                          
                                                                              
                                                                          
                                                                           
   

static inline INT128
interval_cmp_value(const Interval *interval)
{
  INT128 span;
  int64 dayfraction;
  int64 days;

     
                                                                           
                                                                       
     
  dayfraction = interval->time % USECS_PER_DAY;
  days = interval->time / USECS_PER_DAY;
  days += interval->month * INT64CONST(30);
  days += interval->day;

                                     
  span = int64_to_int128(dayfraction);

                                                                
  int128_add_int64_mul_int64(&span, days, USECS_PER_DAY);

  return span;
}

static int
interval_cmp_internal(Interval *interval1, Interval *interval2)
{
  INT128 span1 = interval_cmp_value(interval1);
  INT128 span2 = interval_cmp_value(interval2);

  return int128_compare(span1, span2);
}

Datum
interval_eq(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_BOOL(interval_cmp_internal(interval1, interval2) == 0);
}

Datum
interval_ne(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_BOOL(interval_cmp_internal(interval1, interval2) != 0);
}

Datum
interval_lt(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_BOOL(interval_cmp_internal(interval1, interval2) < 0);
}

Datum
interval_gt(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_BOOL(interval_cmp_internal(interval1, interval2) > 0);
}

Datum
interval_le(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_BOOL(interval_cmp_internal(interval1, interval2) <= 0);
}

Datum
interval_ge(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_BOOL(interval_cmp_internal(interval1, interval2) >= 0);
}

Datum
interval_cmp(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);

  PG_RETURN_INT32(interval_cmp_internal(interval1, interval2));
}

   
                         
   
                                                                          
                                                                    
                       
   
Datum
interval_hash(PG_FUNCTION_ARGS)
{
  Interval *interval = PG_GETARG_INTERVAL_P(0);
  INT128 span = interval_cmp_value(interval);
  int64 span64;

     
                                                                            
                                                                            
                                                                       
                 
     
  span64 = int128_to_int64(span);

  return DirectFunctionCall1(hashint8, Int64GetDatumFast(span64));
}

Datum
interval_hash_extended(PG_FUNCTION_ARGS)
{
  Interval *interval = PG_GETARG_INTERVAL_P(0);
  INT128 span = interval_cmp_value(interval);
  int64 span64;

                                      
  span64 = int128_to_int64(span);

  return DirectFunctionCall2(hashint8extended, Int64GetDatumFast(span64), PG_GETARG_DATUM(1));
}

                                                                  
   
                                                                    
                                                                           
                                      
   
Datum
overlaps_timestamp(PG_FUNCTION_ARGS)
{
     
                                                                          
                                                                             
                                                        
     
  Datum ts1 = PG_GETARG_DATUM(0);
  Datum te1 = PG_GETARG_DATUM(1);
  Datum ts2 = PG_GETARG_DATUM(2);
  Datum te2 = PG_GETARG_DATUM(3);
  bool ts1IsNull = PG_ARGISNULL(0);
  bool te1IsNull = PG_ARGISNULL(1);
  bool ts2IsNull = PG_ARGISNULL(2);
  bool te2IsNull = PG_ARGISNULL(3);

#define TIMESTAMP_GT(t1, t2) DatumGetBool(DirectFunctionCall2(timestamp_gt, t1, t2))
#define TIMESTAMP_LT(t1, t2) DatumGetBool(DirectFunctionCall2(timestamp_lt, t1, t2))

     
                                                                             
                                                                            
                                      
     
  if (ts1IsNull)
  {
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }
                                
    ts1 = te1;
    te1IsNull = true;
  }
  else if (!te1IsNull)
  {
    if (TIMESTAMP_GT(ts1, te1))
    {
      Datum tt = ts1;

      ts1 = te1;
      te1 = tt;
    }
  }

                                
  if (ts2IsNull)
  {
    if (te2IsNull)
    {
      PG_RETURN_NULL();
    }
                                
    ts2 = te2;
    te2IsNull = true;
  }
  else if (!te2IsNull)
  {
    if (TIMESTAMP_GT(ts2, te2))
    {
      Datum tt = ts2;

      ts2 = te2;
      te2 = tt;
    }
  }

     
                                                                         
                                            
     
  if (TIMESTAMP_GT(ts1, ts2))
  {
       
                                                                         
                                                              
       
    if (te2IsNull)
    {
      PG_RETURN_NULL();
    }
    if (TIMESTAMP_LT(ts1, te2))
    {
      PG_RETURN_BOOL(true);
    }
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }

       
                                                                          
                                     
       
    PG_RETURN_BOOL(false);
  }
  else if (TIMESTAMP_LT(ts1, ts2))
  {
                                             
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }
    if (TIMESTAMP_LT(ts2, te1))
    {
      PG_RETURN_BOOL(true);
    }
    if (te2IsNull)
    {
      PG_RETURN_NULL();
    }

       
                                                                          
                                     
       
    PG_RETURN_BOOL(false);
  }
  else
  {
       
                                                                       
                                                                          
       
    if (te1IsNull || te2IsNull)
    {
      PG_RETURN_NULL();
    }
    PG_RETURN_BOOL(true);
  }

#undef TIMESTAMP_GT
#undef TIMESTAMP_LT
}

                                                             
                                         
                                                             

Datum
timestamp_smaller(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp result;

                                                                          
  if (timestamp_cmp_internal(dt1, dt2) < 0)
  {
    result = dt1;
  }
  else
  {
    result = dt2;
  }
  PG_RETURN_TIMESTAMP(result);
}

Datum
timestamp_larger(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp result;

  if (timestamp_cmp_internal(dt1, dt2) > 0)
  {
    result = dt1;
  }
  else
  {
    result = dt2;
  }
  PG_RETURN_TIMESTAMP(result);
}

Datum
timestamp_mi(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  if (TIMESTAMP_NOT_FINITE(dt1) || TIMESTAMP_NOT_FINITE(dt2))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("cannot subtract infinite timestamps")));
  }

  result->time = dt1 - dt2;

  result->month = 0;
  result->day = 0;

               
                                                                      
                  
     
                                      
                   
                                                     
                                                    
              
                      
                     
              
     
                                                 
     
                    
                                                      
                                                      
                                                                          
               
                          
                         
             
               
     
  result = DatumGetIntervalP(DirectFunctionCall1(interval_justify_hours, IntervalPGetDatum(result)));

  PG_RETURN_INTERVAL_P(result);
}

   
                               
   
                                                                     
                                    
   
                              
                             
   
                                                                   
                                                      
   
Datum
interval_justify_interval(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  Interval *result;
  TimeOffset wholeday;
  int32 wholemonth;

  result = (Interval *)palloc(sizeof(Interval));
  result->month = span->month;
  result->day = span->day;
  result->time = span->time;

  TMODULO(result->time, wholeday, USECS_PER_DAY);
  result->day += wholeday;                        

  wholemonth = result->day / DAYS_PER_MONTH;
  result->day -= wholemonth * DAYS_PER_MONTH;
  result->month += wholemonth;

  if (result->month > 0 && (result->day < 0 || (result->day == 0 && result->time < 0)))
  {
    result->day += DAYS_PER_MONTH;
    result->month--;
  }
  else if (result->month < 0 && (result->day > 0 || (result->day == 0 && result->time > 0)))
  {
    result->day -= DAYS_PER_MONTH;
    result->month++;
  }

  if (result->day > 0 && result->time < 0)
  {
    result->time += USECS_PER_DAY;
    result->day--;
  }
  else if (result->day < 0 && result->time > 0)
  {
    result->time -= USECS_PER_DAY;
    result->day++;
  }

  PG_RETURN_INTERVAL_P(result);
}

   
                            
   
                                                                    
                                            
                                                                    
                                           
   
Datum
interval_justify_hours(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  Interval *result;
  TimeOffset wholeday;

  result = (Interval *)palloc(sizeof(Interval));
  result->month = span->month;
  result->day = span->day;
  result->time = span->time;

  TMODULO(result->time, wholeday, USECS_PER_DAY);
  result->day += wholeday;                        

  if (result->day > 0 && result->time < 0)
  {
    result->time += USECS_PER_DAY;
    result->day--;
  }
  else if (result->day < 0 && result->time > 0)
  {
    result->time -= USECS_PER_DAY;
    result->day++;
  }

  PG_RETURN_INTERVAL_P(result);
}

   
                           
   
                                                               
                          
   
Datum
interval_justify_days(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  Interval *result;
  int32 wholemonth;

  result = (Interval *)palloc(sizeof(Interval));
  result->month = span->month;
  result->day = span->day;
  result->time = span->time;

  wholemonth = result->day / DAYS_PER_MONTH;
  result->day -= wholemonth * DAYS_PER_MONTH;
  result->month += wholemonth;

  if (result->month > 0 && result->day < 0)
  {
    result->day += DAYS_PER_MONTH;
    result->month--;
  }
  else if (result->month < 0 && result->day > 0)
  {
    result->day -= DAYS_PER_MONTH;
    result->month++;
  }

  PG_RETURN_INTERVAL_P(result);
}

                           
                                             
                                                                        
                                                  
                                                                       
                                                                
                             
                                                                   
                                           
   
Datum
timestamp_pl_interval(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  Timestamp result;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    result = timestamp;
  }
  else
  {
    if (span->month != 0)
    {
      struct pg_tm tt, *tm = &tt;
      fsec_t fsec;

      if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }

      tm->tm_mon += span->month;
      if (tm->tm_mon > MONTHS_PER_YEAR)
      {
        tm->tm_year += (tm->tm_mon - 1) / MONTHS_PER_YEAR;
        tm->tm_mon = ((tm->tm_mon - 1) % MONTHS_PER_YEAR) + 1;
      }
      else if (tm->tm_mon < 1)
      {
        tm->tm_year += tm->tm_mon / MONTHS_PER_YEAR - 1;
        tm->tm_mon = tm->tm_mon % MONTHS_PER_YEAR + MONTHS_PER_YEAR;
      }

                                                        
      if (tm->tm_mday > day_tab[isleap(tm->tm_year)][tm->tm_mon - 1])
      {
        tm->tm_mday = (day_tab[isleap(tm->tm_year)][tm->tm_mon - 1]);
      }

      if (tm2timestamp(tm, fsec, NULL, &timestamp) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
    }

    if (span->day != 0)
    {
      struct pg_tm tt, *tm = &tt;
      fsec_t fsec;
      int julian;

      if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }

                                                     
      julian = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) + span->day;
      j2date(julian, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);

      if (tm2timestamp(tm, fsec, NULL, &timestamp) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
    }

    timestamp += span->time;

    if (!IS_VALID_TIMESTAMP(timestamp))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    result = timestamp;
  }

  PG_RETURN_TIMESTAMP(result);
}

Datum
timestamp_mi_interval(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  Interval tspan;

  tspan.month = -span->month;
  tspan.day = -span->day;
  tspan.time = -span->time;

  return DirectFunctionCall2(timestamp_pl_interval, TimestampGetDatum(timestamp), PointerGetDatum(&tspan));
}

                             
                                                            
                                                                
                                                  
                                                                       
                                                                
                             
                                           
   
Datum
timestamptz_pl_interval(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  TimestampTz result;
  int tz;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    result = timestamp;
  }
  else
  {
    if (span->month != 0)
    {
      struct pg_tm tt, *tm = &tt;
      fsec_t fsec;

      if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }

      tm->tm_mon += span->month;
      if (tm->tm_mon > MONTHS_PER_YEAR)
      {
        tm->tm_year += (tm->tm_mon - 1) / MONTHS_PER_YEAR;
        tm->tm_mon = ((tm->tm_mon - 1) % MONTHS_PER_YEAR) + 1;
      }
      else if (tm->tm_mon < 1)
      {
        tm->tm_year += tm->tm_mon / MONTHS_PER_YEAR - 1;
        tm->tm_mon = tm->tm_mon % MONTHS_PER_YEAR + MONTHS_PER_YEAR;
      }

                                                        
      if (tm->tm_mday > day_tab[isleap(tm->tm_year)][tm->tm_mon - 1])
      {
        tm->tm_mday = (day_tab[isleap(tm->tm_year)][tm->tm_mon - 1]);
      }

      tz = DetermineTimeZoneOffset(tm, session_timezone);

      if (tm2timestamp(tm, fsec, &tz, &timestamp) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
    }

    if (span->day != 0)
    {
      struct pg_tm tt, *tm = &tt;
      fsec_t fsec;
      int julian;

      if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }

                                                     
      julian = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) + span->day;
      j2date(julian, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);

      tz = DetermineTimeZoneOffset(tm, session_timezone);

      if (tm2timestamp(tm, fsec, &tz, &timestamp) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
    }

    timestamp += span->time;

    if (!IS_VALID_TIMESTAMP(timestamp))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    result = timestamp;
  }

  PG_RETURN_TIMESTAMP(result);
}

Datum
timestamptz_mi_interval(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  Interval tspan;

  tspan.month = -span->month;
  tspan.day = -span->day;
  tspan.time = -span->time;

  return DirectFunctionCall2(timestamptz_pl_interval, TimestampGetDatum(timestamp), PointerGetDatum(&tspan));
}

Datum
interval_um(PG_FUNCTION_ARGS)
{
  Interval *interval = PG_GETARG_INTERVAL_P(0);
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  result->time = -interval->time;
                                         
  if (interval->time != 0 && SAMESIGN(result->time, interval->time))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }
  result->day = -interval->day;
  if (interval->day != 0 && SAMESIGN(result->day, interval->day))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }
  result->month = -interval->month;
  if (interval->month != 0 && SAMESIGN(result->month, interval->month))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  PG_RETURN_INTERVAL_P(result);
}

Datum
interval_smaller(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);
  Interval *result;

                                                                         
  if (interval_cmp_internal(interval1, interval2) < 0)
  {
    result = interval1;
  }
  else
  {
    result = interval2;
  }
  PG_RETURN_INTERVAL_P(result);
}

Datum
interval_larger(PG_FUNCTION_ARGS)
{
  Interval *interval1 = PG_GETARG_INTERVAL_P(0);
  Interval *interval2 = PG_GETARG_INTERVAL_P(1);
  Interval *result;

  if (interval_cmp_internal(interval1, interval2) > 0)
  {
    result = interval1;
  }
  else
  {
    result = interval2;
  }
  PG_RETURN_INTERVAL_P(result);
}

Datum
interval_pl(PG_FUNCTION_ARGS)
{
  Interval *span1 = PG_GETARG_INTERVAL_P(0);
  Interval *span2 = PG_GETARG_INTERVAL_P(1);
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  result->month = span1->month + span2->month;
                                         
  if (SAMESIGN(span1->month, span2->month) && !SAMESIGN(result->month, span1->month))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  result->day = span1->day + span2->day;
  if (SAMESIGN(span1->day, span2->day) && !SAMESIGN(result->day, span1->day))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  result->time = span1->time + span2->time;
  if (SAMESIGN(span1->time, span2->time) && !SAMESIGN(result->time, span1->time))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  PG_RETURN_INTERVAL_P(result);
}

Datum
interval_mi(PG_FUNCTION_ARGS)
{
  Interval *span1 = PG_GETARG_INTERVAL_P(0);
  Interval *span2 = PG_GETARG_INTERVAL_P(1);
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  result->month = span1->month - span2->month;
                                         
  if (!SAMESIGN(span1->month, span2->month) && !SAMESIGN(result->month, span1->month))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  result->day = span1->day - span2->day;
  if (!SAMESIGN(span1->day, span2->day) && !SAMESIGN(result->day, span1->day))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  result->time = span1->time - span2->time;
  if (!SAMESIGN(span1->time, span2->time) && !SAMESIGN(result->time, span1->time))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }

  PG_RETURN_INTERVAL_P(result);
}

   
                                                                    
                                                                       
                                                                       
   

Datum
interval_mul(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  float8 factor = PG_GETARG_FLOAT8(1);
  double month_remainder_days, sec_remainder, result_double;
  int32 orig_month = span->month, orig_day = span->day;
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  result_double = span->month * factor;
  if (isnan(result_double) || result_double > INT_MAX || result_double < INT_MIN)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }
  result->month = (int32)result_double;

  result_double = span->day * factor;
  if (isnan(result_double) || result_double > INT_MAX || result_double < INT_MIN)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }
  result->day = (int32)result_double;

     
                                                                            
                                                                    
                                                                          
                                                                         
                                                                             
                                                                         
                                              
     

     
                                            
     
                                                                   
                                                                            
                                                                        
                  
     
  month_remainder_days = (orig_month * factor - result->month) * DAYS_PER_MONTH;
  month_remainder_days = TSROUND(month_remainder_days);
  sec_remainder = (orig_day * factor - result->day + month_remainder_days - (int)month_remainder_days) * SECS_PER_DAY;
  sec_remainder = TSROUND(sec_remainder);

     
                                                                             
                                                                             
                                                         
     
  if (Abs(sec_remainder) >= SECS_PER_DAY)
  {
    result->day += (int)(sec_remainder / SECS_PER_DAY);
    sec_remainder -= (int)(sec_remainder / SECS_PER_DAY) * SECS_PER_DAY;
  }

                          
  result->day += (int32)month_remainder_days;
  result_double = rint(span->time * factor + sec_remainder * USECS_PER_SEC);
  if (isnan(result_double) || !FLOAT8_FITS_IN_INT64(result_double))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
  }
  result->time = (int64)result_double;

  PG_RETURN_INTERVAL_P(result);
}

Datum
mul_d_interval(PG_FUNCTION_ARGS)
{
                                                                       
  Datum factor = PG_GETARG_DATUM(0);
  Datum span = PG_GETARG_DATUM(1);

  return DirectFunctionCall2(interval_mul, span, factor);
}

Datum
interval_div(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  float8 factor = PG_GETARG_FLOAT8(1);
  double month_remainder_days, sec_remainder;
  int32 orig_month = span->month, orig_day = span->day;
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  if (factor == 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

  result->month = (int32)(span->month / factor);
  result->day = (int32)(span->day / factor);

     
                                                                            
     
  month_remainder_days = (orig_month / factor - result->month) * DAYS_PER_MONTH;
  month_remainder_days = TSROUND(month_remainder_days);
  sec_remainder = (orig_day / factor - result->day + month_remainder_days - (int)month_remainder_days) * SECS_PER_DAY;
  sec_remainder = TSROUND(sec_remainder);
  if (Abs(sec_remainder) >= SECS_PER_DAY)
  {
    result->day += (int)(sec_remainder / SECS_PER_DAY);
    sec_remainder -= (int)(sec_remainder / SECS_PER_DAY) * SECS_PER_DAY;
  }

                          
  result->day += (int32)month_remainder_days;
  result->time = rint(span->time / factor + sec_remainder * USECS_PER_SEC);

  PG_RETURN_INTERVAL_P(result);
}

   
                                                            
   
                                                                    
                                                                           
                                                                         
                                        
   

Datum
in_range_timestamptz_interval(PG_FUNCTION_ARGS)
{
  TimestampTz val = PG_GETARG_TIMESTAMPTZ(0);
  TimestampTz base = PG_GETARG_TIMESTAMPTZ(1);
  Interval *offset = PG_GETARG_INTERVAL_P(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  TimestampTz sum;

  if (int128_compare(interval_cmp_value(offset), int64_to_int128(0)) < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

                                                                
  if (sub)
  {
    sum = DatumGetTimestampTz(DirectFunctionCall2(timestamptz_mi_interval, TimestampTzGetDatum(base), IntervalPGetDatum(offset)));
  }
  else
  {
    sum = DatumGetTimestampTz(DirectFunctionCall2(timestamptz_pl_interval, TimestampTzGetDatum(base), IntervalPGetDatum(offset)));
  }

  if (less)
  {
    PG_RETURN_BOOL(val <= sum);
  }
  else
  {
    PG_RETURN_BOOL(val >= sum);
  }
}

Datum
in_range_timestamp_interval(PG_FUNCTION_ARGS)
{
  Timestamp val = PG_GETARG_TIMESTAMP(0);
  Timestamp base = PG_GETARG_TIMESTAMP(1);
  Interval *offset = PG_GETARG_INTERVAL_P(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  Timestamp sum;

  if (int128_compare(interval_cmp_value(offset), int64_to_int128(0)) < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

                                                                
  if (sub)
  {
    sum = DatumGetTimestamp(DirectFunctionCall2(timestamp_mi_interval, TimestampGetDatum(base), IntervalPGetDatum(offset)));
  }
  else
  {
    sum = DatumGetTimestamp(DirectFunctionCall2(timestamp_pl_interval, TimestampGetDatum(base), IntervalPGetDatum(offset)));
  }

  if (less)
  {
    PG_RETURN_BOOL(val <= sum);
  }
  else
  {
    PG_RETURN_BOOL(val >= sum);
  }
}

Datum
in_range_interval_interval(PG_FUNCTION_ARGS)
{
  Interval *val = PG_GETARG_INTERVAL_P(0);
  Interval *base = PG_GETARG_INTERVAL_P(1);
  Interval *offset = PG_GETARG_INTERVAL_P(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  Interval *sum;

  if (int128_compare(interval_cmp_value(offset), int64_to_int128(0)) < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

                                                                
  if (sub)
  {
    sum = DatumGetIntervalP(DirectFunctionCall2(interval_mi, IntervalPGetDatum(base), IntervalPGetDatum(offset)));
  }
  else
  {
    sum = DatumGetIntervalP(DirectFunctionCall2(interval_pl, IntervalPGetDatum(base), IntervalPGetDatum(offset)));
  }

  if (less)
  {
    PG_RETURN_BOOL(interval_cmp_internal(val, sum) <= 0);
  }
  else
  {
    PG_RETURN_BOOL(interval_cmp_internal(val, sum) >= 0);
  }
}

   
                                                                      
                            
   
                                                                      
                                                                         
                                                                        
                                                                  
   

Datum
interval_accum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  Interval *newval = PG_GETARG_INTERVAL_P(1);
  Datum *transdatums;
  int ndatums;
  Interval sumX, N;
  Interval *newsum;
  ArrayType *result;

  deconstruct_array(transarray, INTERVALOID, sizeof(Interval), false, 'd', &transdatums, NULL, &ndatums);
  if (ndatums != 2)
  {
    elog(ERROR, "expected 2-element interval array");
  }

  sumX = *(DatumGetIntervalP(transdatums[0]));
  N = *(DatumGetIntervalP(transdatums[1]));

  newsum = DatumGetIntervalP(DirectFunctionCall2(interval_pl, IntervalPGetDatum(&sumX), IntervalPGetDatum(newval)));
  N.time += 1;

  transdatums[0] = IntervalPGetDatum(newsum);
  transdatums[1] = IntervalPGetDatum(&N);

  result = construct_array(transdatums, 2, INTERVALOID, sizeof(Interval), false, 'd');

  PG_RETURN_ARRAYTYPE_P(result);
}

Datum
interval_combine(PG_FUNCTION_ARGS)
{
  ArrayType *transarray1 = PG_GETARG_ARRAYTYPE_P(0);
  ArrayType *transarray2 = PG_GETARG_ARRAYTYPE_P(1);
  Datum *transdatums1;
  Datum *transdatums2;
  int ndatums1;
  int ndatums2;
  Interval sum1, N1;
  Interval sum2, N2;

  Interval *newsum;
  ArrayType *result;

  deconstruct_array(transarray1, INTERVALOID, sizeof(Interval), false, 'd', &transdatums1, NULL, &ndatums1);
  if (ndatums1 != 2)
  {
    elog(ERROR, "expected 2-element interval array");
  }

  sum1 = *(DatumGetIntervalP(transdatums1[0]));
  N1 = *(DatumGetIntervalP(transdatums1[1]));

  deconstruct_array(transarray2, INTERVALOID, sizeof(Interval), false, 'd', &transdatums2, NULL, &ndatums2);
  if (ndatums2 != 2)
  {
    elog(ERROR, "expected 2-element interval array");
  }

  sum2 = *(DatumGetIntervalP(transdatums2[0]));
  N2 = *(DatumGetIntervalP(transdatums2[1]));

  newsum = DatumGetIntervalP(DirectFunctionCall2(interval_pl, IntervalPGetDatum(&sum1), IntervalPGetDatum(&sum2)));
  N1.time += N2.time;

  transdatums1[0] = IntervalPGetDatum(newsum);
  transdatums1[1] = IntervalPGetDatum(&N1);

  result = construct_array(transdatums1, 2, INTERVALOID, sizeof(Interval), false, 'd');

  PG_RETURN_ARRAYTYPE_P(result);
}

Datum
interval_accum_inv(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  Interval *newval = PG_GETARG_INTERVAL_P(1);
  Datum *transdatums;
  int ndatums;
  Interval sumX, N;
  Interval *newsum;
  ArrayType *result;

  deconstruct_array(transarray, INTERVALOID, sizeof(Interval), false, 'd', &transdatums, NULL, &ndatums);
  if (ndatums != 2)
  {
    elog(ERROR, "expected 2-element interval array");
  }

  sumX = *(DatumGetIntervalP(transdatums[0]));
  N = *(DatumGetIntervalP(transdatums[1]));

  newsum = DatumGetIntervalP(DirectFunctionCall2(interval_mi, IntervalPGetDatum(&sumX), IntervalPGetDatum(newval)));
  N.time -= 1;

  transdatums[0] = IntervalPGetDatum(newsum);
  transdatums[1] = IntervalPGetDatum(&N);

  result = construct_array(transdatums, 2, INTERVALOID, sizeof(Interval), false, 'd');

  PG_RETURN_ARRAYTYPE_P(result);
}

Datum
interval_avg(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  Datum *transdatums;
  int ndatums;
  Interval sumX, N;

  deconstruct_array(transarray, INTERVALOID, sizeof(Interval), false, 'd', &transdatums, NULL, &ndatums);
  if (ndatums != 2)
  {
    elog(ERROR, "expected 2-element interval array");
  }

  sumX = *(DatumGetIntervalP(transdatums[0]));
  N = *(DatumGetIntervalP(transdatums[1]));

                                               
  if (N.time == 0)
  {
    PG_RETURN_NULL();
  }

  return DirectFunctionCall2(interval_div, IntervalPGetDatum(&sumX), Float8GetDatum((double)N.time));
}

                   
                                                                
                                                                    
                                                               
            
   
Datum
timestamp_age(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Interval *result;
  fsec_t fsec, fsec1, fsec2;
  struct pg_tm tt, *tm = &tt;
  struct pg_tm tt1, *tm1 = &tt1;
  struct pg_tm tt2, *tm2 = &tt2;

  result = (Interval *)palloc(sizeof(Interval));

  if (timestamp2tm(dt1, NULL, tm1, &fsec1, NULL, NULL) == 0 && timestamp2tm(dt2, NULL, tm2, &fsec2, NULL, NULL) == 0)
  {
                                      
    fsec = fsec1 - fsec2;
    tm->tm_sec = tm1->tm_sec - tm2->tm_sec;
    tm->tm_min = tm1->tm_min - tm2->tm_min;
    tm->tm_hour = tm1->tm_hour - tm2->tm_hour;
    tm->tm_mday = tm1->tm_mday - tm2->tm_mday;
    tm->tm_mon = tm1->tm_mon - tm2->tm_mon;
    tm->tm_year = tm1->tm_year - tm2->tm_year;

                                   
    if (dt1 < dt2)
    {
      fsec = -fsec;
      tm->tm_sec = -tm->tm_sec;
      tm->tm_min = -tm->tm_min;
      tm->tm_hour = -tm->tm_hour;
      tm->tm_mday = -tm->tm_mday;
      tm->tm_mon = -tm->tm_mon;
      tm->tm_year = -tm->tm_year;
    }

                                                                  
    while (fsec < 0)
    {
      fsec += USECS_PER_SEC;
      tm->tm_sec--;
    }

    while (tm->tm_sec < 0)
    {
      tm->tm_sec += SECS_PER_MINUTE;
      tm->tm_min--;
    }

    while (tm->tm_min < 0)
    {
      tm->tm_min += MINS_PER_HOUR;
      tm->tm_hour--;
    }

    while (tm->tm_hour < 0)
    {
      tm->tm_hour += HOURS_PER_DAY;
      tm->tm_mday--;
    }

    while (tm->tm_mday < 0)
    {
      if (dt1 < dt2)
      {
        tm->tm_mday += day_tab[isleap(tm1->tm_year)][tm1->tm_mon - 1];
        tm->tm_mon--;
      }
      else
      {
        tm->tm_mday += day_tab[isleap(tm2->tm_year)][tm2->tm_mon - 1];
        tm->tm_mon--;
      }
    }

    while (tm->tm_mon < 0)
    {
      tm->tm_mon += MONTHS_PER_YEAR;
      tm->tm_year--;
    }

                                      
    if (dt1 < dt2)
    {
      fsec = -fsec;
      tm->tm_sec = -tm->tm_sec;
      tm->tm_min = -tm->tm_min;
      tm->tm_hour = -tm->tm_hour;
      tm->tm_mday = -tm->tm_mday;
      tm->tm_mon = -tm->tm_mon;
      tm->tm_year = -tm->tm_year;
    }

    if (tm2interval(tm, fsec, result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_INTERVAL_P(result);
}

                     
                                                                
                                                                    
                                                               
            
   
Datum
timestamptz_age(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  Interval *result;
  fsec_t fsec, fsec1, fsec2;
  struct pg_tm tt, *tm = &tt;
  struct pg_tm tt1, *tm1 = &tt1;
  struct pg_tm tt2, *tm2 = &tt2;
  int tz1;
  int tz2;

  result = (Interval *)palloc(sizeof(Interval));

  if (timestamp2tm(dt1, &tz1, tm1, &fsec1, NULL, NULL) == 0 && timestamp2tm(dt2, &tz2, tm2, &fsec2, NULL, NULL) == 0)
  {
                                      
    fsec = fsec1 - fsec2;
    tm->tm_sec = tm1->tm_sec - tm2->tm_sec;
    tm->tm_min = tm1->tm_min - tm2->tm_min;
    tm->tm_hour = tm1->tm_hour - tm2->tm_hour;
    tm->tm_mday = tm1->tm_mday - tm2->tm_mday;
    tm->tm_mon = tm1->tm_mon - tm2->tm_mon;
    tm->tm_year = tm1->tm_year - tm2->tm_year;

                                   
    if (dt1 < dt2)
    {
      fsec = -fsec;
      tm->tm_sec = -tm->tm_sec;
      tm->tm_min = -tm->tm_min;
      tm->tm_hour = -tm->tm_hour;
      tm->tm_mday = -tm->tm_mday;
      tm->tm_mon = -tm->tm_mon;
      tm->tm_year = -tm->tm_year;
    }

                                                                  
    while (fsec < 0)
    {
      fsec += USECS_PER_SEC;
      tm->tm_sec--;
    }

    while (tm->tm_sec < 0)
    {
      tm->tm_sec += SECS_PER_MINUTE;
      tm->tm_min--;
    }

    while (tm->tm_min < 0)
    {
      tm->tm_min += MINS_PER_HOUR;
      tm->tm_hour--;
    }

    while (tm->tm_hour < 0)
    {
      tm->tm_hour += HOURS_PER_DAY;
      tm->tm_mday--;
    }

    while (tm->tm_mday < 0)
    {
      if (dt1 < dt2)
      {
        tm->tm_mday += day_tab[isleap(tm1->tm_year)][tm1->tm_mon - 1];
        tm->tm_mon--;
      }
      else
      {
        tm->tm_mday += day_tab[isleap(tm2->tm_year)][tm2->tm_mon - 1];
        tm->tm_mon--;
      }
    }

    while (tm->tm_mon < 0)
    {
      tm->tm_mon += MONTHS_PER_YEAR;
      tm->tm_year--;
    }

       
                                                                        
       

                                      
    if (dt1 < dt2)
    {
      fsec = -fsec;
      tm->tm_sec = -tm->tm_sec;
      tm->tm_min = -tm->tm_min;
      tm->tm_hour = -tm->tm_hour;
      tm->tm_mday = -tm->tm_mday;
      tm->tm_mon = -tm->tm_mon;
      tm->tm_year = -tm->tm_year;
    }

    if (tm2interval(tm, fsec, result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_INTERVAL_P(result);
}

                                                             
                         
                                                             

                     
                                          
   
Datum
timestamp_trunc(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  Timestamp timestamp = PG_GETARG_TIMESTAMP(1);
  Timestamp result;
  int type, val;
  char *lowunits;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMP(timestamp);
  }

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);

  if (type == UNITS)
  {
    if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    switch (val)
    {
    case DTK_WEEK:
    {
      int woy;

      woy = date2isoweek(tm->tm_year, tm->tm_mon, tm->tm_mday);

         
                                                                
                                                           
                                                 
         
      if (woy >= 52 && tm->tm_mon == 1)
      {
        --tm->tm_year;
      }
      if (woy <= 1 && tm->tm_mon == MONTHS_PER_YEAR)
      {
        ++tm->tm_year;
      }
      isoweek2date(woy, &(tm->tm_year), &(tm->tm_mon), &(tm->tm_mday));
      tm->tm_hour = 0;
      tm->tm_min = 0;
      tm->tm_sec = 0;
      fsec = 0;
      break;
    }
    case DTK_MILLENNIUM:
                                             
      if (tm->tm_year > 0)
      {
        tm->tm_year = ((tm->tm_year + 999) / 1000) * 1000 - 999;
      }
      else
      {
        tm->tm_year = -((999 - (tm->tm_year - 1)) / 1000) * 1000 + 1;
      }
                     
    case DTK_CENTURY:
                                             
      if (tm->tm_year > 0)
      {
        tm->tm_year = ((tm->tm_year + 99) / 100) * 100 - 99;
      }
      else
      {
        tm->tm_year = -((99 - (tm->tm_year - 1)) / 100) * 100 + 1;
      }
                     
    case DTK_DECADE:
                                             
      if (val != DTK_MILLENNIUM && val != DTK_CENTURY)
      {
        if (tm->tm_year > 0)
        {
          tm->tm_year = (tm->tm_year / 10) * 10;
        }
        else
        {
          tm->tm_year = -((8 - (tm->tm_year - 1)) / 10) * 10;
        }
      }
                     
    case DTK_YEAR:
      tm->tm_mon = 1;
                     
    case DTK_QUARTER:
      tm->tm_mon = (3 * ((tm->tm_mon - 1) / 3)) + 1;
                     
    case DTK_MONTH:
      tm->tm_mday = 1;
                     
    case DTK_DAY:
      tm->tm_hour = 0;
                     
    case DTK_HOUR:
      tm->tm_min = 0;
                     
    case DTK_MINUTE:
      tm->tm_sec = 0;
                     
    case DTK_SECOND:
      fsec = 0;
      break;

    case DTK_MILLISEC:
      fsec = (fsec / 1000) * 1000;
      break;

    case DTK_MICROSEC:
      break;

    default:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp units \"%s\" not supported", lowunits)));
      result = 0;
    }

    if (tm2timestamp(tm, fsec, NULL, &result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp units \"%s\" not recognized", lowunits)));
    result = 0;
  }

  PG_RETURN_TIMESTAMP(result);
}

   
                                                                     
   
                                                                   
                                                   
   
static TimestampTz
timestamptz_trunc_internal(text *units, TimestampTz timestamp, pg_tz *tzp)
{
  TimestampTz result;
  int tz;
  int type, val;
  bool redotz = false;
  char *lowunits;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);

  if (type == UNITS)
  {
    if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, tzp) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    switch (val)
    {
    case DTK_WEEK:
    {
      int woy;

      woy = date2isoweek(tm->tm_year, tm->tm_mon, tm->tm_mday);

         
                                                                
                                                           
                                                 
         
      if (woy >= 52 && tm->tm_mon == 1)
      {
        --tm->tm_year;
      }
      if (woy <= 1 && tm->tm_mon == MONTHS_PER_YEAR)
      {
        ++tm->tm_year;
      }
      isoweek2date(woy, &(tm->tm_year), &(tm->tm_mon), &(tm->tm_mday));
      tm->tm_hour = 0;
      tm->tm_min = 0;
      tm->tm_sec = 0;
      fsec = 0;
      redotz = true;
      break;
    }
                                                            
    case DTK_MILLENNIUM:

         
                                                                
                                                                   
                                 
         
      if (tm->tm_year > 0)
      {
        tm->tm_year = ((tm->tm_year + 999) / 1000) * 1000 - 999;
      }
      else
      {
        tm->tm_year = -((999 - (tm->tm_year - 1)) / 1000) * 1000 + 1;
      }
                     
    case DTK_CENTURY:
                                                                
      if (tm->tm_year > 0)
      {
        tm->tm_year = ((tm->tm_year + 99) / 100) * 100 - 99;
      }
      else
      {
        tm->tm_year = -((99 - (tm->tm_year - 1)) / 100) * 100 + 1;
      }
                     
    case DTK_DECADE:

         
                                                                  
                                                      
         
      if (val != DTK_MILLENNIUM && val != DTK_CENTURY)
      {
        if (tm->tm_year > 0)
        {
          tm->tm_year = (tm->tm_year / 10) * 10;
        }
        else
        {
          tm->tm_year = -((8 - (tm->tm_year - 1)) / 10) * 10;
        }
      }
                     
    case DTK_YEAR:
      tm->tm_mon = 1;
                     
    case DTK_QUARTER:
      tm->tm_mon = (3 * ((tm->tm_mon - 1) / 3)) + 1;
                     
    case DTK_MONTH:
      tm->tm_mday = 1;
                     
    case DTK_DAY:
      tm->tm_hour = 0;
      redotz = true;                           
                                    
    case DTK_HOUR:
      tm->tm_min = 0;
                     
    case DTK_MINUTE:
      tm->tm_sec = 0;
                     
    case DTK_SECOND:
      fsec = 0;
      break;
    case DTK_MILLISEC:
      fsec = (fsec / 1000) * 1000;
      break;
    case DTK_MICROSEC:
      break;

    default:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp with time zone units \"%s\" not "
                                                                     "supported",
                                                                  lowunits)));
      result = 0;
    }

    if (redotz)
    {
      tz = DetermineTimeZoneOffset(tm, tzp);
    }

    if (tm2timestamp(tm, fsec, &tz, &result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp with time zone units \"%s\" not recognized", lowunits)));
    result = 0;
  }

  return result;
}

                       
                                                                
   
Datum
timestamptz_trunc(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz result;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMPTZ(timestamp);
  }

  result = timestamptz_trunc_internal(units, timestamp, session_timezone);

  PG_RETURN_TIMESTAMPTZ(result);
}

                            
                                                                  
   
Datum
timestamptz_trunc_zone(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(1);
  text *zone = PG_GETARG_TEXT_PP(2);
  TimestampTz result;
  char tzname[TZ_STRLEN_MAX + 1];
  char *lowzone;
  int type, val;
  pg_tz *tzp;

     
                                                                            
                              
     
  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMP(timestamp);
  }

     
                                                                       
     
  text_to_cstring_buffer(zone, tzname, sizeof(tzname));

                                                     
  lowzone = downcase_truncate_identifier(tzname, strlen(tzname), false);

  type = DecodeTimezoneAbbrev(0, lowzone, &val, &tzp);

  if (type == TZ || type == DTZ)
  {
                                                                    
    tzp = pg_tzset_offset(-val);
  }
  else if (type == DYNTZ)
  {
                                                                  
  }
  else
  {
                                    
    tzp = pg_tzset(tzname);
    if (!tzp)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", tzname)));
    }
  }

  result = timestamptz_trunc_internal(units, timestamp, tzp);

  PG_RETURN_TIMESTAMPTZ(result);
}

                    
                                          
   
Datum
interval_trunc(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  Interval *interval = PG_GETARG_INTERVAL_P(1);
  Interval *result;
  int type, val;
  char *lowunits;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;

  result = (Interval *)palloc(sizeof(Interval));

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);

  if (type == UNITS)
  {
    if (interval2tm(*interval, tm, &fsec) == 0)
    {
      switch (val)
      {
      case DTK_MILLENNIUM:
                                                             
        tm->tm_year = (tm->tm_year / 1000) * 1000;
                       
      case DTK_CENTURY:
                                                             
        tm->tm_year = (tm->tm_year / 100) * 100;
                       
      case DTK_DECADE:
                                                             
        tm->tm_year = (tm->tm_year / 10) * 10;
                       
      case DTK_YEAR:
        tm->tm_mon = 0;
                       
      case DTK_QUARTER:
        tm->tm_mon = 3 * (tm->tm_mon / 3);
                       
      case DTK_MONTH:
        tm->tm_mday = 0;
                       
      case DTK_DAY:
        tm->tm_hour = 0;
                       
      case DTK_HOUR:
        tm->tm_min = 0;
                       
      case DTK_MINUTE:
        tm->tm_sec = 0;
                       
      case DTK_SECOND:
        fsec = 0;
        break;
      case DTK_MILLISEC:
        fsec = (fsec / 1000) * 1000;
        break;
      case DTK_MICROSEC:
        break;

      default:
        if (val == DTK_WEEK)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("interval units \"%s\" not supported "
                                                                         "because months usually have fractional weeks",
                                                                      lowunits)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("interval units \"%s\" not supported", lowunits)));
        }
      }

      if (tm2interval(tm, fsec, result) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("interval out of range")));
      }
    }
    else
    {
      elog(ERROR, "could not convert interval to tm");
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("interval units \"%s\" not recognized", lowunits)));
  }

  PG_RETURN_INTERVAL_P(result);
}

               
   
                                                                                                          
                                                                               
   
int
isoweek2j(int year, int week)
{
  int day0, day4;

                                  
  day4 = date2j(year, 1, 4);

                                                    
  day0 = j2day(day4 - 1);

  return ((week - 1) * 7) + (day4 - day0);
}

                  
                                            
                                                       
                    
   
void
isoweek2date(int woy, int *year, int *mon, int *mday)
{
  j2date(isoweek2j(*year, woy), year, mon, mday);
}

                      
   
                                                                             
                                                                  
                                                                    
                                           
   
void
isoweekdate2date(int isoweek, int wday, int *year, int *mon, int *mday)
{
  int jday;

  jday = isoweek2j(*year, isoweek);
                                                                            
  if (wday > 1)
  {
    jday += wday - 2;
  }
  else
  {
    jday += 6;
  }
  j2date(jday, year, mon, mday);
}

                  
   
                                    
   
int
date2isoweek(int year, int mon, int mday)
{
  float8 result;
  int day0, day4, dayn;

                   
  dayn = date2j(year, mon, mday);

                                  
  day4 = date2j(year, 1, 4);

                                                    
  day0 = j2day(day4 - 1);

     
                                                                            
                                                           
     
  if (dayn < day4 - day0)
  {
    day4 = date2j(year - 1, 1, 4);

                                                      
    day0 = j2day(day4 - 1);
  }

  result = (dayn - (day4 - day0)) / 7 + 1;

     
                                                                            
                                       
     
  if (result >= 52)
  {
    day4 = date2j(year + 1, 1, 4);

                                                      
    day0 = j2day(day4 - 1);

    if (dayn >= day4 - day0)
    {
      result = (dayn - (day4 - day0)) / 7 + 1;
    }
  }

  return (int)result;
}

                  
   
                                 
                                                                          
   
int
date2isoyear(int year, int mon, int mday)
{
  float8 result;
  int day0, day4, dayn;

                   
  dayn = date2j(year, mon, mday);

                                  
  day4 = date2j(year, 1, 4);

                                                    
  day0 = j2day(day4 - 1);

     
                                                                            
                                                           
     
  if (dayn < day4 - day0)
  {
    day4 = date2j(year - 1, 1, 4);

                                                      
    day0 = j2day(day4 - 1);

    year--;
  }

  result = (dayn - (day4 - day0)) / 7 + 1;

     
                                                                            
                                       
     
  if (result >= 52)
  {
    day4 = date2j(year + 1, 1, 4);

                                                      
    day0 = j2day(day4 - 1);

    if (dayn >= day4 - day0)
    {
      year++;
    }
  }

  return year;
}

                     
   
                                                                            
                                                                     
   
int
date2isoyearday(int year, int mon, int mday)
{
  return date2j(year, mon, mday) - isoweek2j(date2isoyear(year, mon, mday), 1) + 1;
}

   
                            
   
                                                                             
                                                                          
                                                                             
   
                                                                        
                                                                           
                                                           
   
static float8
NonFiniteTimestampTzPart(int type, int unit, char *lowunits, bool isNegative, bool isTz)
{
  if ((type != UNITS) && (type != RESERV))
  {
    if (isTz)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp with time zone units \"%s\" not recognized", lowunits)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp units \"%s\" not recognized", lowunits)));
    }
  }

  switch (unit)
  {
                           
  case DTK_MICROSEC:
  case DTK_MILLISEC:
  case DTK_SECOND:
  case DTK_MINUTE:
  case DTK_HOUR:
  case DTK_DAY:
  case DTK_MONTH:
  case DTK_QUARTER:
  case DTK_WEEK:
  case DTK_DOW:
  case DTK_ISODOW:
  case DTK_DOY:
  case DTK_TZ:
  case DTK_TZ_MINUTE:
  case DTK_TZ_HOUR:
    return 0.0;

                                        
  case DTK_YEAR:
  case DTK_DECADE:
  case DTK_CENTURY:
  case DTK_MILLENNIUM:
  case DTK_JULIAN:
  case DTK_ISOYEAR:
  case DTK_EPOCH:
    if (isNegative)
    {
      return -get_float8_infinity();
    }
    else
    {
      return get_float8_infinity();
    }

  default:
    if (isTz)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp with time zone units \"%s\" not supported", lowunits)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp units \"%s\" not supported", lowunits)));
    }
    return 0.0;                          
  }
}

                    
                                           
   
Datum
timestamp_part(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  Timestamp timestamp = PG_GETARG_TIMESTAMP(1);
  float8 result;
  Timestamp epoch;
  int type, val;
  char *lowunits;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);
  if (type == UNKNOWN_FIELD)
  {
    type = DecodeSpecial(0, lowunits, &val);
  }

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    result = NonFiniteTimestampTzPart(type, val, lowunits, TIMESTAMP_IS_NOBEGIN(timestamp), false);
    if (result)
    {
      PG_RETURN_FLOAT8(result);
    }
    else
    {
      PG_RETURN_NULL();
    }
  }

  if (type == UNITS)
  {
    if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    switch (val)
    {
    case DTK_MICROSEC:
      result = tm->tm_sec * 1000000.0 + fsec;
      break;

    case DTK_MILLISEC:
      result = tm->tm_sec * 1000.0 + fsec / 1000.0;
      break;

    case DTK_SECOND:
      result = tm->tm_sec + fsec / 1000000.0;
      break;

    case DTK_MINUTE:
      result = tm->tm_min;
      break;

    case DTK_HOUR:
      result = tm->tm_hour;
      break;

    case DTK_DAY:
      result = tm->tm_mday;
      break;

    case DTK_MONTH:
      result = tm->tm_mon;
      break;

    case DTK_QUARTER:
      result = (tm->tm_mon - 1) / 3 + 1;
      break;

    case DTK_WEEK:
      result = (float8)date2isoweek(tm->tm_year, tm->tm_mon, tm->tm_mday);
      break;

    case DTK_YEAR:
      if (tm->tm_year > 0)
      {
        result = tm->tm_year;
      }
      else
      {
                                                    
        result = tm->tm_year - 1;
      }
      break;

    case DTK_DECADE:

         
                                                                   
                                                                   
                               
         
      if (tm->tm_year >= 0)
      {
        result = tm->tm_year / 10;
      }
      else
      {
        result = -((8 - (tm->tm_year - 1)) / 10);
      }
      break;

    case DTK_CENTURY:

              
                                                               
                                                               
                                       
              
         
      if (tm->tm_year > 0)
      {
        result = (tm->tm_year + 99) / 100;
      }
      else
      {
                                                             
        result = -((99 - (tm->tm_year - 1)) / 100);
      }
      break;

    case DTK_MILLENNIUM:
                               
      if (tm->tm_year > 0)
      {
        result = (tm->tm_year + 999) / 1000;
      }
      else
      {
        result = -((999 - (tm->tm_year - 1)) / 1000);
      }
      break;

    case DTK_JULIAN:
      result = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday);
      result += ((((tm->tm_hour * MINS_PER_HOUR) + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec + (fsec / 1000000.0)) / (double)SECS_PER_DAY;
      break;

    case DTK_ISOYEAR:
      result = date2isoyear(tm->tm_year, tm->tm_mon, tm->tm_mday);
                           
      if (result <= 0)
      {
        result -= 1;
      }
      break;

    case DTK_DOW:
    case DTK_ISODOW:
      result = j2day(date2j(tm->tm_year, tm->tm_mon, tm->tm_mday));
      if (val == DTK_ISODOW && result == 0)
      {
        result = 7;
      }
      break;

    case DTK_DOY:
      result = (date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - date2j(tm->tm_year, 1, 1) + 1);
      break;

    case DTK_TZ:
    case DTK_TZ_MINUTE:
    case DTK_TZ_HOUR:
    default:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp units \"%s\" not supported", lowunits)));
      result = 0;
    }
  }
  else if (type == RESERV)
  {
    switch (val)
    {
    case DTK_EPOCH:
      epoch = SetEpochTimestamp();
                                                      
      if (timestamp < (PG_INT64_MAX + epoch))
      {
        result = (timestamp - epoch) / 1000000.0;
      }
      else
      {
        result = ((float8)timestamp - epoch) / 1000000.0;
      }
      break;

    default:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp units \"%s\" not supported", lowunits)));
      result = 0;
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp units \"%s\" not recognized", lowunits)));
    result = 0;
  }

  PG_RETURN_FLOAT8(result);
}

                      
                                                          
   
Datum
timestamptz_part(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(1);
  float8 result;
  Timestamp epoch;
  int tz;
  int type, val;
  char *lowunits;
  double dummy;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);
  if (type == UNKNOWN_FIELD)
  {
    type = DecodeSpecial(0, lowunits, &val);
  }

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    result = NonFiniteTimestampTzPart(type, val, lowunits, TIMESTAMP_IS_NOBEGIN(timestamp), true);
    if (result)
    {
      PG_RETURN_FLOAT8(result);
    }
    else
    {
      PG_RETURN_NULL();
    }
  }

  if (type == UNITS)
  {
    if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    switch (val)
    {
    case DTK_TZ:
      result = -tz;
      break;

    case DTK_TZ_MINUTE:
      result = -tz;
      result /= MINS_PER_HOUR;
      FMODULO(result, dummy, (double)MINS_PER_HOUR);
      break;

    case DTK_TZ_HOUR:
      dummy = -tz;
      FMODULO(dummy, result, (double)SECS_PER_HOUR);
      break;

    case DTK_MICROSEC:
      result = tm->tm_sec * 1000000.0 + fsec;
      break;

    case DTK_MILLISEC:
      result = tm->tm_sec * 1000.0 + fsec / 1000.0;
      break;

    case DTK_SECOND:
      result = tm->tm_sec + fsec / 1000000.0;
      break;

    case DTK_MINUTE:
      result = tm->tm_min;
      break;

    case DTK_HOUR:
      result = tm->tm_hour;
      break;

    case DTK_DAY:
      result = tm->tm_mday;
      break;

    case DTK_MONTH:
      result = tm->tm_mon;
      break;

    case DTK_QUARTER:
      result = (tm->tm_mon - 1) / 3 + 1;
      break;

    case DTK_WEEK:
      result = (float8)date2isoweek(tm->tm_year, tm->tm_mon, tm->tm_mday);
      break;

    case DTK_YEAR:
      if (tm->tm_year > 0)
      {
        result = tm->tm_year;
      }
      else
      {
                                                    
        result = tm->tm_year - 1;
      }
      break;

    case DTK_DECADE:
                                          
      if (tm->tm_year > 0)
      {
        result = tm->tm_year / 10;
      }
      else
      {
        result = -((8 - (tm->tm_year - 1)) / 10);
      }
      break;

    case DTK_CENTURY:
                                          
      if (tm->tm_year > 0)
      {
        result = (tm->tm_year + 99) / 100;
      }
      else
      {
        result = -((99 - (tm->tm_year - 1)) / 100);
      }
      break;

    case DTK_MILLENNIUM:
                                          
      if (tm->tm_year > 0)
      {
        result = (tm->tm_year + 999) / 1000;
      }
      else
      {
        result = -((999 - (tm->tm_year - 1)) / 1000);
      }
      break;

    case DTK_JULIAN:
      result = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday);
      result += ((((tm->tm_hour * MINS_PER_HOUR) + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec + (fsec / 1000000.0)) / (double)SECS_PER_DAY;
      break;

    case DTK_ISOYEAR:
      result = date2isoyear(tm->tm_year, tm->tm_mon, tm->tm_mday);
                           
      if (result <= 0)
      {
        result -= 1;
      }
      break;

    case DTK_DOW:
    case DTK_ISODOW:
      result = j2day(date2j(tm->tm_year, tm->tm_mon, tm->tm_mday));
      if (val == DTK_ISODOW && result == 0)
      {
        result = 7;
      }
      break;

    case DTK_DOY:
      result = (date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - date2j(tm->tm_year, 1, 1) + 1);
      break;

    default:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp with time zone units \"%s\" not supported", lowunits)));
      result = 0;
    }
  }
  else if (type == RESERV)
  {
    switch (val)
    {
    case DTK_EPOCH:
      epoch = SetEpochTimestamp();
                                                      
      if (timestamp < (PG_INT64_MAX + epoch))
      {
        result = (timestamp - epoch) / 1000000.0;
      }
      else
      {
        result = ((float8)timestamp - epoch) / 1000000.0;
      }
      break;

    default:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("timestamp with time zone units \"%s\" not supported", lowunits)));
      result = 0;
    }
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("timestamp with time zone units \"%s\" not recognized", lowunits)));

    result = 0;
  }

  PG_RETURN_FLOAT8(result);
}

                   
                                          
   
Datum
interval_part(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  Interval *interval = PG_GETARG_INTERVAL_P(1);
  float8 result;
  int type, val;
  char *lowunits;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);
  if (type == UNKNOWN_FIELD)
  {
    type = DecodeSpecial(0, lowunits, &val);
  }

  if (type == UNITS)
  {
    if (interval2tm(*interval, tm, &fsec) == 0)
    {
      switch (val)
      {
      case DTK_MICROSEC:
        result = tm->tm_sec * 1000000.0 + fsec;
        break;

      case DTK_MILLISEC:
        result = tm->tm_sec * 1000.0 + fsec / 1000.0;
        break;

      case DTK_SECOND:
        result = tm->tm_sec + fsec / 1000000.0;
        break;

      case DTK_MINUTE:
        result = tm->tm_min;
        break;

      case DTK_HOUR:
        result = tm->tm_hour;
        break;

      case DTK_DAY:
        result = tm->tm_mday;
        break;

      case DTK_MONTH:
        result = tm->tm_mon;
        break;

      case DTK_QUARTER:
        result = (tm->tm_mon / 3) + 1;
        break;

      case DTK_YEAR:
        result = tm->tm_year;
        break;

      case DTK_DECADE:
                                                             
        result = tm->tm_year / 10;
        break;

      case DTK_CENTURY:
                                                             
        result = tm->tm_year / 100;
        break;

      case DTK_MILLENNIUM:
                                                             
        result = tm->tm_year / 1000;
        break;

      default:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("interval units \"%s\" not supported", lowunits)));
        result = 0;
      }
    }
    else
    {
      elog(ERROR, "could not convert interval to tm");
      result = 0;
    }
  }
  else if (type == RESERV && val == DTK_EPOCH)
  {
    result = interval->time / 1000000.0;
    result += ((double)DAYS_PER_YEAR * SECS_PER_DAY) * (interval->month / MONTHS_PER_YEAR);
    result += ((double)DAYS_PER_MONTH * SECS_PER_DAY) * (interval->month % MONTHS_PER_YEAR);
    result += ((double)SECS_PER_DAY) * interval->day;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("interval units \"%s\" not recognized", lowunits)));
    result = 0;
  }

  PG_RETURN_FLOAT8(result);
}

                    
                                                   
                                                                   
                                                                        
                                                                       
                                                                  
                       
   
Datum
timestamp_zone(PG_FUNCTION_ARGS)
{
  text *zone = PG_GETARG_TEXT_PP(0);
  Timestamp timestamp = PG_GETARG_TIMESTAMP(1);
  TimestampTz result;
  int tz;
  char tzname[TZ_STRLEN_MAX + 1];
  char *lowzone;
  int type, val;
  pg_tz *tzp;
  struct pg_tm tm;
  fsec_t fsec;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMPTZ(timestamp);
  }

     
                                                                    
                                                                            
                                                         
                                                                            
                                                                             
                                                                        
     
  text_to_cstring_buffer(zone, tzname, sizeof(tzname));

                                                     
  lowzone = downcase_truncate_identifier(tzname, strlen(tzname), false);

  type = DecodeTimezoneAbbrev(0, lowzone, &val, &tzp);

  if (type == TZ || type == DTZ)
  {
                                   
    tz = val;
    result = dt2local(timestamp, tz);
  }
  else if (type == DYNTZ)
  {
                                                                   
    if (timestamp2tm(timestamp, NULL, &tm, &fsec, NULL, tzp) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
    tz = -DetermineTimeZoneAbbrevOffset(&tm, tzname, tzp);
    result = dt2local(timestamp, tz);
  }
  else
  {
                                    
    tzp = pg_tzset(tzname);
    if (tzp)
    {
                                     
      if (timestamp2tm(timestamp, NULL, &tm, &fsec, NULL, tzp) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
      tz = DetermineTimeZoneOffset(&tm, tzp);
      if (tm2timestamp(&tm, fsec, &tz, &result) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", tzname)));
      result = 0;                          
    }
  }

  if (!IS_VALID_TIMESTAMP(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_TIMESTAMPTZ(result);
}

                     
                                                                    
   
Datum
timestamp_izone(PG_FUNCTION_ARGS)
{
  Interval *zone = PG_GETARG_INTERVAL_P(0);
  Timestamp timestamp = PG_GETARG_TIMESTAMP(1);
  TimestampTz result;
  int tz;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMPTZ(timestamp);
  }

  if (zone->month != 0 || zone->day != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("interval time zone \"%s\" must not include months or days", DatumGetCString(DirectFunctionCall1(interval_out, PointerGetDatum(zone))))));
  }

  tz = zone->time / USECS_PER_SEC;

  result = dt2local(timestamp, tz);

  if (!IS_VALID_TIMESTAMP(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_TIMESTAMPTZ(result);
}                        

                                         
   
                                                                              
                                                                           
                                                                              
                                                                              
   
bool
TimestampTimestampTzRequiresRewrite(void)
{
  long offset;

  if (pg_get_timezone_offset(session_timezone, &offset) && offset == 0)
  {
    return false;
  }
  return true;
}

                           
                                               
   
Datum
timestamp_timestamptz(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);

  PG_RETURN_TIMESTAMPTZ(timestamp2timestamptz(timestamp));
}

static TimestampTz
timestamp2timestamptz(Timestamp timestamp)
{
  TimestampTz result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    result = timestamp;
  }
  else
  {
    if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    tz = DetermineTimeZoneOffset(tm, session_timezone);

    if (tm2timestamp(tm, fsec, &tz, &result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }

  return result;
}

                           
                                               
   
Datum
timestamptz_timestamp(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(0);

  PG_RETURN_TIMESTAMP(timestamptz2timestamp(timestamp));
}

static Timestamp
timestamptz2timestamp(TimestampTz timestamp)
{
  Timestamp result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    result = timestamp;
  }
  else
  {
    if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
    if (tm2timestamp(tm, fsec, NULL, &result) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }
  return result;
}

                      
                                                                      
                                          
   
Datum
timestamptz_zone(PG_FUNCTION_ARGS)
{
  text *zone = PG_GETARG_TEXT_PP(0);
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(1);
  Timestamp result;
  int tz;
  char tzname[TZ_STRLEN_MAX + 1];
  char *lowzone;
  int type, val;
  pg_tz *tzp;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMP(timestamp);
  }

     
                                                                    
                                                                            
                                                         
                                                                            
                                                                             
                                                                        
     
  text_to_cstring_buffer(zone, tzname, sizeof(tzname));

                                                     
  lowzone = downcase_truncate_identifier(tzname, strlen(tzname), false);

  type = DecodeTimezoneAbbrev(0, lowzone, &val, &tzp);

  if (type == TZ || type == DTZ)
  {
                                   
    tz = -val;
    result = dt2local(timestamp, tz);
  }
  else if (type == DYNTZ)
  {
                                                                   
    int isdst;

    tz = DetermineTimeZoneAbbrevOffsetTS(timestamp, tzname, tzp, &isdst);
    result = dt2local(timestamp, tz);
  }
  else
  {
                                    
    tzp = pg_tzset(tzname);
    if (tzp)
    {
                                     
      struct pg_tm tm;
      fsec_t fsec;

      if (timestamp2tm(timestamp, &tz, &tm, &fsec, NULL, tzp) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
      if (tm2timestamp(&tm, fsec, NULL, &result) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", tzname)));
      result = 0;                          
    }
  }

  if (!IS_VALID_TIMESTAMP(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_TIMESTAMP(result);
}

                       
                                                                                   
                                          
   
Datum
timestamptz_izone(PG_FUNCTION_ARGS)
{
  Interval *zone = PG_GETARG_INTERVAL_P(0);
  TimestampTz timestamp = PG_GETARG_TIMESTAMPTZ(1);
  Timestamp result;
  int tz;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_TIMESTAMP(timestamp);
  }

  if (zone->month != 0 || zone->day != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("interval time zone \"%s\" must not include months or days", DatumGetCString(DirectFunctionCall1(interval_out, PointerGetDatum(zone))))));
  }

  tz = -(zone->time / USECS_PER_SEC);

  result = dt2local(timestamp, tz);

  if (!IS_VALID_TIMESTAMP(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_TIMESTAMP(result);
}

                               
                                                               
   
Datum
generate_series_timestamp(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  generate_series_timestamp_fctx *fctx;
  Timestamp result;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    Timestamp start = PG_GETARG_TIMESTAMP(0);
    Timestamp finish = PG_GETARG_TIMESTAMP(1);
    Interval *step = PG_GETARG_INTERVAL_P(2);
    MemoryContext oldcontext;
    Interval interval_zero;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                          
    fctx = (generate_series_timestamp_fctx *)palloc(sizeof(generate_series_timestamp_fctx));

       
                                                                       
                            
       
    fctx->current = start;
    fctx->finish = finish;
    fctx->step = *step;

                                        
    MemSet(&interval_zero, 0, sizeof(Interval));
    fctx->step_sign = interval_cmp_internal(&fctx->step, &interval_zero);

    if (fctx->step_sign == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("step size cannot equal zero")));
    }

    funcctx->user_fctx = fctx;
    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();

     
                                                                          
     
  fctx = funcctx->user_fctx;
  result = fctx->current;

  if (fctx->step_sign > 0 ? timestamp_cmp_internal(result, fctx->finish) <= 0 : timestamp_cmp_internal(result, fctx->finish) >= 0)
  {
                                                             
    fctx->current = DatumGetTimestamp(DirectFunctionCall2(timestamp_pl_interval, TimestampGetDatum(fctx->current), PointerGetDatum(&fctx->step)));

                                            
    SRF_RETURN_NEXT(funcctx, TimestampGetDatum(result));
  }
  else
  {
                                       
    SRF_RETURN_DONE(funcctx);
  }
}

                                 
                                                               
   
Datum
generate_series_timestamptz(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  generate_series_timestamptz_fctx *fctx;
  TimestampTz result;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    TimestampTz start = PG_GETARG_TIMESTAMPTZ(0);
    TimestampTz finish = PG_GETARG_TIMESTAMPTZ(1);
    Interval *step = PG_GETARG_INTERVAL_P(2);
    MemoryContext oldcontext;
    Interval interval_zero;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                          
    fctx = (generate_series_timestamptz_fctx *)palloc(sizeof(generate_series_timestamptz_fctx));

       
                                                                       
                            
       
    fctx->current = start;
    fctx->finish = finish;
    fctx->step = *step;

                                        
    MemSet(&interval_zero, 0, sizeof(Interval));
    fctx->step_sign = interval_cmp_internal(&fctx->step, &interval_zero);

    if (fctx->step_sign == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("step size cannot equal zero")));
    }

    funcctx->user_fctx = fctx;
    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();

     
                                                                          
     
  fctx = funcctx->user_fctx;
  result = fctx->current;

  if (fctx->step_sign > 0 ? timestamp_cmp_internal(result, fctx->finish) <= 0 : timestamp_cmp_internal(result, fctx->finish) >= 0)
  {
                                                             
    fctx->current = DatumGetTimestampTz(DirectFunctionCall2(timestamptz_pl_interval, TimestampTzGetDatum(fctx->current), PointerGetDatum(&fctx->step)));

                                            
    SRF_RETURN_NEXT(funcctx, TimestampTzGetDatum(result));
  }
  else
  {
                                       
    SRF_RETURN_DONE(funcctx);
  }
}
