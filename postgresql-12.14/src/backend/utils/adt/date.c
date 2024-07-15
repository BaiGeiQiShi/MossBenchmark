                                                                            
   
          
                                                                   
   
                                                                         
                                                                          
   
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <time.h>

#include "access/xact.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/supportnodes.h"
#include "parser/scansup.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/hashutils.h"
#include "utils/sortsupport.h"

   
                                                                           
                                                                      
   
#ifdef __FAST_MATH__
#error -ffast-math is known to break this code
#endif

static int
tm2time(struct pg_tm *tm, fsec_t fsec, TimeADT *result);
static int
tm2timetz(struct pg_tm *tm, fsec_t fsec, int tz, TimeTzADT *result);
static void
AdjustTimeForTypmod(TimeADT *time, int32 typmod);

                                                     
static int32
anytime_typmodin(bool istz, ArrayType *ta)
{
  int32 *tl;
  int n;

  tl = ArrayGetIntegerTypmods(ta, &n);

     
                                                                       
                                                        
     
  if (n != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid type modifier")));
  }

  return anytime_typmod_check(istz, tl[0]);
}

                                         
int32
anytime_typmod_check(bool istz, int32 typmod)
{
  if (typmod < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("TIME(%d)%s precision must not be negative", typmod, (istz ? " WITH TIME ZONE" : ""))));
  }
  if (typmod > MAX_TIME_PRECISION)
  {
    ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("TIME(%d)%s precision reduced to maximum allowed, %d", typmod, (istz ? " WITH TIME ZONE" : ""), MAX_TIME_PRECISION)));
    typmod = MAX_TIME_PRECISION;
  }

  return typmod;
}

                                                       
static char *
anytime_typmodout(bool istz, int32 typmod)
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
date_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  DateADT date;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;
  int tzp;
  int dtype;
  int nf;
  int dterr;
  char *field[MAXDATEFIELDS];
  int ftype[MAXDATEFIELDS];
  char workbuf[MAXDATELEN + 1];

  dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
  if (dterr == 0)
  {
    dterr = DecodeDateTime(field, ftype, nf, &dtype, tm, &fsec, &tzp);
  }
  if (dterr != 0)
  {
    DateTimeParseError(dterr, str, "date");
  }

  switch (dtype)
  {
  case DTK_DATE:
    break;

  case DTK_EPOCH:
    GetEpochTime(tm);
    break;

  case DTK_LATE:
    DATE_NOEND(date);
    PG_RETURN_DATEADT(date);

  case DTK_EARLY:
    DATE_NOBEGIN(date);
    PG_RETURN_DATEADT(date);

  default:
    DateTimeParseError(DTERR_BAD_FORMAT, str, "date");
    break;
  }

                                               
  if (!IS_VALID_JULIAN(tm->tm_year, tm->tm_mon, tm->tm_mday))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: \"%s\"", str)));
  }

  date = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - POSTGRES_EPOCH_JDATE;

                                             
  if (!IS_VALID_DATE(date))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: \"%s\"", str)));
  }

  PG_RETURN_DATEADT(date);
}

              
                                                       
   
Datum
date_out(PG_FUNCTION_ARGS)
{
  DateADT date = PG_GETARG_DATEADT(0);
  char *result;
  struct pg_tm tt, *tm = &tt;
  char buf[MAXDATELEN + 1];

  if (DATE_NOT_FINITE(date))
  {
    EncodeSpecialDate(date, buf);
  }
  else
  {
    j2date(date + POSTGRES_EPOCH_JDATE, &(tm->tm_year), &(tm->tm_mon), &(tm->tm_mday));
    EncodeDateOnly(tm, DateStyle, buf);
  }

  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                          
   
Datum
date_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  DateADT result;

  result = (DateADT)pq_getmsgint(buf, sizeof(DateADT));

                                                       
  if (DATE_NOT_FINITE(result))
            ;
  else if (!IS_VALID_DATE(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range")));
  }

  PG_RETURN_DATEADT(result);
}

   
                                                 
   
Datum
date_send(PG_FUNCTION_ARGS)
{
  DateADT date = PG_GETARG_DATEADT(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, date);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                                   
   
Datum
make_date(PG_FUNCTION_ARGS)
{
  struct pg_tm tm;
  DateADT date;
  int dterr;
  bool bc = false;

  tm.tm_year = PG_GETARG_INT32(0);
  tm.tm_mon = PG_GETARG_INT32(1);
  tm.tm_mday = PG_GETARG_INT32(2);

                                   
  if (tm.tm_year < 0)
  {
    bc = true;
    tm.tm_year = -tm.tm_year;
  }

  dterr = ValidateDate(DTK_DATE_M, false, false, bc, &tm);

  if (dterr != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_FIELD_OVERFLOW), errmsg("date field value out of range: %d-%02d-%02d", tm.tm_year, tm.tm_mon, tm.tm_mday)));
  }

                                               
  if (!IS_VALID_JULIAN(tm.tm_year, tm.tm_mon, tm.tm_mday))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: %d-%02d-%02d", tm.tm_year, tm.tm_mon, tm.tm_mday)));
  }

  date = date2j(tm.tm_year, tm.tm_mon, tm.tm_mday) - POSTGRES_EPOCH_JDATE;

                                             
  if (!IS_VALID_DATE(date))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: %d-%02d-%02d", tm.tm_year, tm.tm_mon, tm.tm_mday)));
  }

  PG_RETURN_DATEADT(date);
}

   
                                           
   
void
EncodeSpecialDate(DateADT dt, char *str)
{
  if (DATE_IS_NOBEGIN(dt))
  {
    strcpy(str, EARLY);
  }
  else if (DATE_IS_NOEND(dt))
  {
    strcpy(str, LATE);
  }
  else                       
  {
    elog(ERROR, "invalid argument for EncodeSpecialDate");
  }
}

   
                                                
   
DateADT
GetSQLCurrentDate(void)
{
  TimestampTz ts;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  ts = GetCurrentTransactionStartTimestamp();

  if (timestamp2tm(ts, &tz, tm, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  return date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - POSTGRES_EPOCH_JDATE;
}

   
                                                                 
   
TimeTzADT *
GetSQLCurrentTime(int32 typmod)
{
  TimeTzADT *result;
  TimestampTz ts;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  ts = GetCurrentTransactionStartTimestamp();

  if (timestamp2tm(ts, &tz, tm, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));
  tm2timetz(tm, fsec, tz, result);
  AdjustTimeForTypmod(&(result->time), typmod);
  return result;
}

   
                                                         
   
TimeADT
GetSQLLocalTime(int32 typmod)
{
  TimeADT result;
  TimestampTz ts;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  ts = GetCurrentTransactionStartTimestamp();

  if (timestamp2tm(ts, &tz, tm, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  tm2time(tm, fsec, &result);
  AdjustTimeForTypmod(&result, typmod);
  return result;
}

   
                                  
   

Datum
date_eq(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_BOOL(dateVal1 == dateVal2);
}

Datum
date_ne(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_BOOL(dateVal1 != dateVal2);
}

Datum
date_lt(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_BOOL(dateVal1 < dateVal2);
}

Datum
date_le(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_BOOL(dateVal1 <= dateVal2);
}

Datum
date_gt(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_BOOL(dateVal1 > dateVal2);
}

Datum
date_ge(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_BOOL(dateVal1 >= dateVal2);
}

Datum
date_cmp(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  if (dateVal1 < dateVal2)
  {
    PG_RETURN_INT32(-1);
  }
  else if (dateVal1 > dateVal2)
  {
    PG_RETURN_INT32(1);
  }
  PG_RETURN_INT32(0);
}

static int
date_fastcmp(Datum x, Datum y, SortSupport ssup)
{
  DateADT a = DatumGetDateADT(x);
  DateADT b = DatumGetDateADT(y);

  if (a < b)
  {
    return -1;
  }
  else if (a > b)
  {
    return 1;
  }
  return 0;
}

Datum
date_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = date_fastcmp;
  PG_RETURN_VOID();
}

Datum
date_finite(PG_FUNCTION_ARGS)
{
  DateADT date = PG_GETARG_DATEADT(0);

  PG_RETURN_BOOL(!DATE_NOT_FINITE(date));
}

Datum
date_larger(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_DATEADT((dateVal1 > dateVal2) ? dateVal1 : dateVal2);
}

Datum
date_smaller(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  PG_RETURN_DATEADT((dateVal1 < dateVal2) ? dateVal1 : dateVal2);
}

                                                 
   
Datum
date_mi(PG_FUNCTION_ARGS)
{
  DateADT dateVal1 = PG_GETARG_DATEADT(0);
  DateADT dateVal2 = PG_GETARG_DATEADT(1);

  if (DATE_NOT_FINITE(dateVal1) || DATE_NOT_FINITE(dateVal2))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("cannot subtract infinite dates")));
  }

  PG_RETURN_INT32((int32)(dateVal1 - dateVal2));
}

                                                      
                                                           
   
Datum
date_pli(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  int32 days = PG_GETARG_INT32(1);
  DateADT result;

  if (DATE_NOT_FINITE(dateVal))
  {
    PG_RETURN_DATEADT(dateVal);                            
  }

  result = dateVal + days;

                                                           
  if ((days >= 0 ? (result < dateVal) : (result > dateVal)) || !IS_VALID_DATE(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range")));
  }

  PG_RETURN_DATEADT(result);
}

                                                             
   
Datum
date_mii(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  int32 days = PG_GETARG_INT32(1);
  DateADT result;

  if (DATE_NOT_FINITE(dateVal))
  {
    PG_RETURN_DATEADT(dateVal);                            
  }

  result = dateVal - days;

                                                           
  if ((days >= 0 ? (result > dateVal) : (result < dateVal)) || !IS_VALID_DATE(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range")));
  }

  PG_RETURN_DATEADT(result);
}

   
                                                                        
             
   

static Timestamp
date2timestamp(DateADT dateVal)
{
  Timestamp result;

  if (DATE_IS_NOBEGIN(dateVal))
  {
    TIMESTAMP_NOBEGIN(result);
  }
  else if (DATE_IS_NOEND(dateVal))
  {
    TIMESTAMP_NOEND(result);
  }
  else
  {
       
                                                                        
                                                                          
                                              
       
    if (dateVal >= (TIMESTAMP_END_JULIAN - POSTGRES_EPOCH_JDATE))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range for timestamp")));
    }

                                                                          
    result = dateVal * USECS_PER_DAY;
  }

  return result;
}

static TimestampTz
date2timestamptz(DateADT dateVal)
{
  TimestampTz result;
  struct pg_tm tt, *tm = &tt;
  int tz;

  if (DATE_IS_NOBEGIN(dateVal))
  {
    TIMESTAMP_NOBEGIN(result);
  }
  else if (DATE_IS_NOEND(dateVal))
  {
    TIMESTAMP_NOEND(result);
  }
  else
  {
       
                                                                        
                                                                          
                                              
       
    if (dateVal >= (TIMESTAMP_END_JULIAN - POSTGRES_EPOCH_JDATE))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range for timestamp")));
    }

    j2date(dateVal + POSTGRES_EPOCH_JDATE, &(tm->tm_year), &(tm->tm_mon), &(tm->tm_mday));
    tm->tm_hour = 0;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    tz = DetermineTimeZoneOffset(tm, session_timezone);

    result = dateVal * USECS_PER_DAY + tz * USECS_PER_SEC;

       
                                                                           
                                                                        
       
    if (!IS_VALID_TIMESTAMP(result))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range for timestamp")));
    }
  }

  return result;
}

   
                              
   
                                                                   
                                                                          
                                                                           
                                                                      
                                                                     
                                             
   
double
date2timestamp_no_overflow(DateADT dateVal)
{
  double result;

  if (DATE_IS_NOBEGIN(dateVal))
  {
    result = -DBL_MAX;
  }
  else if (DATE_IS_NOEND(dateVal))
  {
    result = DBL_MAX;
  }
  else
  {
                                                                          
    result = dateVal * (double)USECS_PER_DAY;
  }

  return result;
}

   
                                            
   

Datum
date_eq_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) == 0);
}

Datum
date_ne_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) != 0);
}

Datum
date_lt_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) < 0);
}

Datum
date_gt_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) > 0);
}

Datum
date_le_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) <= 0);
}

Datum
date_ge_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) >= 0);
}

Datum
date_cmp_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp dt2 = PG_GETARG_TIMESTAMP(1);
  Timestamp dt1;

  dt1 = date2timestamp(dateVal);

  PG_RETURN_INT32(timestamp_cmp_internal(dt1, dt2));
}

Datum
date_eq_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) == 0);
}

Datum
date_ne_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) != 0);
}

Datum
date_lt_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) < 0);
}

Datum
date_gt_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) > 0);
}

Datum
date_le_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) <= 0);
}

Datum
date_ge_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) >= 0);
}

Datum
date_cmp_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz dt2 = PG_GETARG_TIMESTAMPTZ(1);
  TimestampTz dt1;

  dt1 = date2timestamptz(dateVal);

  PG_RETURN_INT32(timestamptz_cmp_internal(dt1, dt2));
}

Datum
timestamp_eq_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) == 0);
}

Datum
timestamp_ne_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) != 0);
}

Datum
timestamp_lt_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) < 0);
}

Datum
timestamp_gt_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) > 0);
}

Datum
timestamp_le_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) <= 0);
}

Datum
timestamp_ge_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_BOOL(timestamp_cmp_internal(dt1, dt2) >= 0);
}

Datum
timestamp_cmp_date(PG_FUNCTION_ARGS)
{
  Timestamp dt1 = PG_GETARG_TIMESTAMP(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  Timestamp dt2;

  dt2 = date2timestamp(dateVal);

  PG_RETURN_INT32(timestamp_cmp_internal(dt1, dt2));
}

Datum
timestamptz_eq_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) == 0);
}

Datum
timestamptz_ne_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) != 0);
}

Datum
timestamptz_lt_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) < 0);
}

Datum
timestamptz_gt_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) > 0);
}

Datum
timestamptz_le_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) <= 0);
}

Datum
timestamptz_ge_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_BOOL(timestamptz_cmp_internal(dt1, dt2) >= 0);
}

Datum
timestamptz_cmp_date(PG_FUNCTION_ARGS)
{
  TimestampTz dt1 = PG_GETARG_TIMESTAMPTZ(0);
  DateADT dateVal = PG_GETARG_DATEADT(1);
  TimestampTz dt2;

  dt2 = date2timestamptz(dateVal);

  PG_RETURN_INT32(timestamptz_cmp_internal(dt1, dt2));
}

   
                                       
   
                                                                             
                                                                
   
Datum
in_range_date_interval(PG_FUNCTION_ARGS)
{
  DateADT val = PG_GETARG_DATEADT(0);
  DateADT base = PG_GETARG_DATEADT(1);
  Interval *offset = PG_GETARG_INTERVAL_P(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  Timestamp valStamp;
  Timestamp baseStamp;

  valStamp = date2timestamp(val);
  baseStamp = date2timestamp(base);

  return DirectFunctionCall5(in_range_timestamp_interval, TimestampGetDatum(valStamp), TimestampGetDatum(baseStamp), IntervalPGetDatum(offset), BoolGetDatum(sub), BoolGetDatum(less));
}

                                                 
                                                     
   
                                                                            
                                                        
   
Datum
date_pl_interval(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  Timestamp dateStamp;

  dateStamp = date2timestamp(dateVal);

  return DirectFunctionCall2(timestamp_pl_interval, TimestampGetDatum(dateStamp), PointerGetDatum(span));
}

                                                        
                                                     
   
                                                                            
                                                         
   
Datum
date_mi_interval(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  Timestamp dateStamp;

  dateStamp = date2timestamp(dateVal);

  return DirectFunctionCall2(timestamp_mi_interval, TimestampGetDatum(dateStamp), PointerGetDatum(span));
}

                    
                                        
   
Datum
date_timestamp(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  Timestamp result;

  result = date2timestamp(dateVal);

  PG_RETURN_TIMESTAMP(result);
}

                    
                                        
   
Datum
timestamp_date(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  DateADT result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;

  if (TIMESTAMP_IS_NOBEGIN(timestamp))
  {
    DATE_NOBEGIN(result);
  }
  else if (TIMESTAMP_IS_NOEND(timestamp))
  {
    DATE_NOEND(result);
  }
  else
  {
    if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    result = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - POSTGRES_EPOCH_JDATE;
  }

  PG_RETURN_DATEADT(result);
}

                      
                                                       
   
Datum
date_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT dateVal = PG_GETARG_DATEADT(0);
  TimestampTz result;

  result = date2timestamptz(dateVal);

  PG_RETURN_TIMESTAMP(result);
}

                      
                                                       
   
Datum
timestamptz_date(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMP(0);
  DateADT result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  if (TIMESTAMP_IS_NOBEGIN(timestamp))
  {
    DATE_NOBEGIN(result);
  }
  else if (TIMESTAMP_IS_NOEND(timestamp))
  {
    DATE_NOEND(result);
  }
  else
  {
    if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }

    result = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - POSTGRES_EPOCH_JDATE;
  }

  PG_RETURN_DATEADT(result);
}

                                                                               
             
                                                                               

Datum
time_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  TimeADT result;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;
  int tz;
  int nf;
  int dterr;
  char workbuf[MAXDATELEN + 1];
  char *field[MAXDATEFIELDS];
  int dtype;
  int ftype[MAXDATEFIELDS];

  dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
  if (dterr == 0)
  {
    dterr = DecodeTimeOnly(field, ftype, nf, &dtype, tm, &fsec, &tz);
  }
  if (dterr != 0)
  {
    DateTimeParseError(dterr, str, "time");
  }

  tm2time(tm, fsec, &result);
  AdjustTimeForTypmod(&result, typmod);

  PG_RETURN_TIMEADT(result);
}

             
                                               
   
static int
tm2time(struct pg_tm *tm, fsec_t fsec, TimeADT *result)
{
  *result = ((((tm->tm_hour * MINS_PER_HOUR + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec) * USECS_PER_SEC) + fsec;
  return 0;
}

                    
                                                              
   
bool
time_overflows(int hour, int min, int sec, fsec_t fsec)
{
                                            
  if (hour < 0 || hour > HOURS_PER_DAY || min < 0 || min >= MINS_PER_HOUR || sec < 0 || sec > SECS_PER_MINUTE || fsec < 0 || fsec > USECS_PER_SEC)
  {
    return true;
  }

     
                                                                           
                                                        
     
  if ((((((hour * MINS_PER_HOUR + min) * SECS_PER_MINUTE) + sec) * USECS_PER_SEC) + fsec) > USECS_PER_DAY)
  {
    return true;
  }

  return false;
}

                          
                                                                          
   
bool
float_time_overflows(int hour, int min, double sec)
{
                                            
  if (hour < 0 || hour > HOURS_PER_DAY || min < 0 || min >= MINS_PER_HOUR)
  {
    return true;
  }

     
                                                                             
                                                                       
                                                                          
     
  if (isnan(sec))
  {
    return true;
  }
  sec = rint(sec * USECS_PER_SEC);
  if (sec < 0 || sec > SECS_PER_MINUTE * USECS_PER_SEC)
  {
    return true;
  }

     
                                                                           
                                                                             
                                                         
     
  if (((((hour * MINS_PER_HOUR + min) * SECS_PER_MINUTE) * USECS_PER_SEC) + (int64)sec) > USECS_PER_DAY)
  {
    return true;
  }

  return false;
}

             
                                                   
   
                                                                            
                                                                          
                                                   
   
int
time2tm(TimeADT time, struct pg_tm *tm, fsec_t *fsec)
{
  tm->tm_hour = time / USECS_PER_HOUR;
  time -= tm->tm_hour * USECS_PER_HOUR;
  tm->tm_min = time / USECS_PER_MINUTE;
  time -= tm->tm_min * USECS_PER_MINUTE;
  tm->tm_sec = time / USECS_PER_SEC;
  time -= tm->tm_sec * USECS_PER_SEC;
  *fsec = time;
  return 0;
}

Datum
time_out(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  char *result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  char buf[MAXDATELEN + 1];

  time2tm(time, tm, &fsec);
  EncodeTimeOnly(tm, fsec, false, 0, DateStyle, buf);

  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                          
   
Datum
time_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  TimeADT result;

  result = pq_getmsgint64(buf);

  if (result < INT64CONST(0) || result > USECS_PER_DAY)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("time out of range")));
  }

  AdjustTimeForTypmod(&result, typmod);

  PG_RETURN_TIMEADT(result);
}

   
                                                 
   
Datum
time_send(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, time);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum
timetypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anytime_typmodin(false, ta));
}

Datum
timetypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anytime_typmodout(false, typmod));
}

   
                                   
   
Datum
make_time(PG_FUNCTION_ARGS)
{
  int tm_hour = PG_GETARG_INT32(0);
  int tm_min = PG_GETARG_INT32(1);
  double sec = PG_GETARG_FLOAT8(2);
  TimeADT time;

                               
  if (float_time_overflows(tm_hour, tm_min, sec))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_FIELD_OVERFLOW), errmsg("time field value out of range: %d:%02d:%02g", tm_hour, tm_min, sec)));
  }

                                 
  time = (((tm_hour * MINS_PER_HOUR + tm_min) * SECS_PER_MINUTE) * USECS_PER_SEC) + (int64)rint(sec * USECS_PER_SEC);

  PG_RETURN_TIMEADT(time);
}

                  
   
                                                                    
                                                                  
   
Datum
time_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSimplify))
  {
    SupportRequestSimplify *req = (SupportRequestSimplify *)rawreq;

    ret = TemporalSimplify(MAX_TIME_PRECISION, (Node *)req->fcall);
  }

  PG_RETURN_POINTER(ret);
}

                
                                                
                                                    
   
Datum
time_scale(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  int32 typmod = PG_GETARG_INT32(1);
  TimeADT result;

  result = time;
  AdjustTimeForTypmod(&result, typmod);

  PG_RETURN_TIMEADT(result);
}

                         
                                                               
                                                                 
                                                          
                                                               
                            
   
static void
AdjustTimeForTypmod(TimeADT *time, int32 typmod)
{
  static const int64 TimeScales[MAX_TIME_PRECISION + 1] = {INT64CONST(1000000), INT64CONST(100000), INT64CONST(10000), INT64CONST(1000), INT64CONST(100), INT64CONST(10), INT64CONST(1)};

  static const int64 TimeOffsets[MAX_TIME_PRECISION + 1] = {INT64CONST(500000), INT64CONST(50000), INT64CONST(5000), INT64CONST(500), INT64CONST(50), INT64CONST(5), INT64CONST(0)};

  if (typmod >= 0 && typmod <= MAX_TIME_PRECISION)
  {
    if (*time >= INT64CONST(0))
    {
      *time = ((*time + TimeOffsets[typmod]) / TimeScales[typmod]) * TimeScales[typmod];
    }
    else
    {
      *time = -((((-*time) + TimeOffsets[typmod]) / TimeScales[typmod]) * TimeScales[typmod]);
    }
  }
}

Datum
time_eq(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_BOOL(time1 == time2);
}

Datum
time_ne(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_BOOL(time1 != time2);
}

Datum
time_lt(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_BOOL(time1 < time2);
}

Datum
time_le(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_BOOL(time1 <= time2);
}

Datum
time_gt(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_BOOL(time1 > time2);
}

Datum
time_ge(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_BOOL(time1 >= time2);
}

Datum
time_cmp(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  if (time1 < time2)
  {
    PG_RETURN_INT32(-1);
  }
  if (time1 > time2)
  {
    PG_RETURN_INT32(1);
  }
  PG_RETURN_INT32(0);
}

Datum
time_hash(PG_FUNCTION_ARGS)
{
  return hashint8(fcinfo);
}

Datum
time_hash_extended(PG_FUNCTION_ARGS)
{
  return hashint8extended(fcinfo);
}

Datum
time_larger(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_TIMEADT((time1 > time2) ? time1 : time2);
}

Datum
time_smaller(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);

  PG_RETURN_TIMEADT((time1 < time2) ? time1 : time2);
}

                                                             
   
                                                                    
                                                                           
                                      
   
Datum
overlaps_time(PG_FUNCTION_ARGS)
{
     
                                                                             
                                                         
     
  Datum ts1 = PG_GETARG_DATUM(0);
  Datum te1 = PG_GETARG_DATUM(1);
  Datum ts2 = PG_GETARG_DATUM(2);
  Datum te2 = PG_GETARG_DATUM(3);
  bool ts1IsNull = PG_ARGISNULL(0);
  bool te1IsNull = PG_ARGISNULL(1);
  bool ts2IsNull = PG_ARGISNULL(2);
  bool te2IsNull = PG_ARGISNULL(3);

#define TIMEADT_GT(t1, t2) (DatumGetTimeADT(t1) > DatumGetTimeADT(t2))
#define TIMEADT_LT(t1, t2) (DatumGetTimeADT(t1) < DatumGetTimeADT(t2))

     
                                                                             
                                                                            
                                      
     
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
    if (TIMEADT_GT(ts1, te1))
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
    if (TIMEADT_GT(ts2, te2))
    {
      Datum tt = ts2;

      ts2 = te2;
      te2 = tt;
    }
  }

     
                                                                         
                                            
     
  if (TIMEADT_GT(ts1, ts2))
  {
       
                                                                         
                                                              
       
    if (te2IsNull)
    {
      PG_RETURN_NULL();
    }
    if (TIMEADT_LT(ts1, te2))
    {
      PG_RETURN_BOOL(true);
    }
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }

       
                                                                          
                                     
       
    PG_RETURN_BOOL(false);
  }
  else if (TIMEADT_LT(ts1, ts2))
  {
                                             
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }
    if (TIMEADT_LT(ts2, te1))
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

#undef TIMEADT_GT
#undef TIMEADT_LT
}

                    
                                        
   
Datum
timestamp_time(PG_FUNCTION_ARGS)
{
  Timestamp timestamp = PG_GETARG_TIMESTAMP(0);
  TimeADT result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_NULL();
  }

  if (timestamp2tm(timestamp, NULL, tm, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

     
                                                                 
                                 
     
  result = ((((tm->tm_hour * MINS_PER_HOUR + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec) * USECS_PER_SEC) + fsec;

  PG_RETURN_TIMEADT(result);
}

                      
                                          
   
Datum
timestamptz_time(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMP(0);
  TimeADT result;
  struct pg_tm tt, *tm = &tt;
  int tz;
  fsec_t fsec;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_NULL();
  }

  if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

     
                                                                 
                                 
     
  result = ((((tm->tm_hour * MINS_PER_HOUR + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec) * USECS_PER_SEC) + fsec;

  PG_RETURN_TIMEADT(result);
}

                        
                                                 
   
Datum
datetime_timestamp(PG_FUNCTION_ARGS)
{
  DateADT date = PG_GETARG_DATEADT(0);
  TimeADT time = PG_GETARG_TIMEADT(1);
  Timestamp result;

  result = date2timestamp(date);
  if (!TIMESTAMP_NOT_FINITE(result))
  {
    result += time;
    if (!IS_VALID_TIMESTAMP(result))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
    }
  }

  PG_RETURN_TIMESTAMP(result);
}

                   
                                       
   
Datum
time_interval(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  result->time = time;
  result->day = 0;
  result->month = 0;

  PG_RETURN_INTERVAL_P(result);
}

                   
                                       
   
                                                                            
                                                                         
                                                                            
                                                
   
Datum
interval_time(PG_FUNCTION_ARGS)
{
  Interval *span = PG_GETARG_INTERVAL_P(0);
  TimeADT result;
  int64 days;

  result = span->time;
  if (result >= USECS_PER_DAY)
  {
    days = result / USECS_PER_DAY;
    result -= days * USECS_PER_DAY;
  }
  else if (result < 0)
  {
    days = (-result + USECS_PER_DAY - 1) / USECS_PER_DAY;
    result += days * USECS_PER_DAY;
  }

  PG_RETURN_TIMEADT(result);
}

                  
                                              
   
Datum
time_mi_time(PG_FUNCTION_ARGS)
{
  TimeADT time1 = PG_GETARG_TIMEADT(0);
  TimeADT time2 = PG_GETARG_TIMEADT(1);
  Interval *result;

  result = (Interval *)palloc(sizeof(Interval));

  result->month = 0;
  result->day = 0;
  result->time = time1 - time2;

  PG_RETURN_INTERVAL_P(result);
}

                      
                         
   
Datum
time_pl_interval(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  TimeADT result;

  result = time + span->time;
  result -= result / USECS_PER_DAY * USECS_PER_DAY;
  if (result < INT64CONST(0))
  {
    result += USECS_PER_DAY;
  }

  PG_RETURN_TIMEADT(result);
}

                      
                                
   
Datum
time_mi_interval(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  TimeADT result;

  result = time - span->time;
  result -= result / USECS_PER_DAY * USECS_PER_DAY;
  if (result < INT64CONST(0))
  {
    result += USECS_PER_DAY;
  }

  PG_RETURN_TIMEADT(result);
}

   
                                       
   
Datum
in_range_time_interval(PG_FUNCTION_ARGS)
{
  TimeADT val = PG_GETARG_TIMEADT(0);
  TimeADT base = PG_GETARG_TIMEADT(1);
  Interval *offset = PG_GETARG_INTERVAL_P(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  TimeADT sum;

     
                                                                            
                                                                 
     
  if (offset->time < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

     
                                                                        
                                                                             
                                                                             
                                                  
     
  if (sub)
  {
    sum = base - offset->time;
  }
  else
  {
    sum = base + offset->time;
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
time_part(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  TimeADT time = PG_GETARG_TIMEADT(1);
  float8 result;
  int type, val;
  char *lowunits;

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);
  if (type == UNKNOWN_FIELD)
  {
    type = DecodeSpecial(0, lowunits, &val);
  }

  if (type == UNITS)
  {
    fsec_t fsec;
    struct pg_tm tt, *tm = &tt;

    time2tm(time, tm, &fsec);

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

    case DTK_TZ:
    case DTK_TZ_MINUTE:
    case DTK_TZ_HOUR:
    case DTK_DAY:
    case DTK_MONTH:
    case DTK_QUARTER:
    case DTK_YEAR:
    case DTK_DECADE:
    case DTK_CENTURY:
    case DTK_MILLENNIUM:
    case DTK_ISOYEAR:
    default:
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"time\" units \"%s\" not recognized", lowunits)));
      result = 0;
    }
  }
  else if (type == RESERV && val == DTK_EPOCH)
  {
    result = time / 1000000.0;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"time\" units \"%s\" not recognized", lowunits)));
    result = 0;
  }

  PG_RETURN_FLOAT8(result);
}

                                                                               
                            
                                                                               

               
                                               
   
static int
tm2timetz(struct pg_tm *tm, fsec_t fsec, int tz, TimeTzADT *result)
{
  result->time = ((((tm->tm_hour * MINS_PER_HOUR + tm->tm_min) * SECS_PER_MINUTE) + tm->tm_sec) * USECS_PER_SEC) + fsec;
  result->zone = tz;

  return 0;
}

Datum
timetz_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  TimeTzADT *result;
  fsec_t fsec;
  struct pg_tm tt, *tm = &tt;
  int tz;
  int nf;
  int dterr;
  char workbuf[MAXDATELEN + 1];
  char *field[MAXDATEFIELDS];
  int dtype;
  int ftype[MAXDATEFIELDS];

  dterr = ParseDateTime(str, workbuf, sizeof(workbuf), field, ftype, MAXDATEFIELDS, &nf);
  if (dterr == 0)
  {
    dterr = DecodeTimeOnly(field, ftype, nf, &dtype, tm, &fsec, &tz);
  }
  if (dterr != 0)
  {
    DateTimeParseError(dterr, str, "time with time zone");
  }

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));
  tm2timetz(tm, fsec, tz, result);
  AdjustTimeForTypmod(&(result->time), typmod);

  PG_RETURN_TIMETZADT_P(result);
}

Datum
timetz_out(PG_FUNCTION_ARGS)
{
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(0);
  char *result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;
  char buf[MAXDATELEN + 1];

  timetz2tm(time, tm, &fsec, &tz);
  EncodeTimeOnly(tm, fsec, true, tz, DateStyle, buf);

  result = pstrdup(buf);
  PG_RETURN_CSTRING(result);
}

   
                                                              
   
Datum
timetz_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  TimeTzADT *result;

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = pq_getmsgint64(buf);

  if (result->time < INT64CONST(0) || result->time > USECS_PER_DAY)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("time out of range")));
  }

  result->zone = pq_getmsgint(buf, sizeof(result->zone));

                                                                          
  if (result->zone <= -TZDISP_LIMIT || result->zone >= TZDISP_LIMIT)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TIME_ZONE_DISPLACEMENT_VALUE), errmsg("time zone displacement out of range")));
  }

  AdjustTimeForTypmod(&(result->time), typmod);

  PG_RETURN_TIMETZADT_P(result);
}

   
                                                     
   
Datum
timetz_send(PG_FUNCTION_ARGS)
{
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint64(&buf, time->time);
  pq_sendint32(&buf, time->zone);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

Datum
timetztypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);

  PG_RETURN_INT32(anytime_typmodin(true, ta));
}

Datum
timetztypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);

  PG_RETURN_CSTRING(anytime_typmodout(true, typmod));
}

               
                                                                  
   
int
timetz2tm(TimeTzADT *time, struct pg_tm *tm, fsec_t *fsec, int *tzp)
{
  TimeOffset trem = time->time;

  tm->tm_hour = trem / USECS_PER_HOUR;
  trem -= tm->tm_hour * USECS_PER_HOUR;
  tm->tm_min = trem / USECS_PER_MINUTE;
  trem -= tm->tm_min * USECS_PER_MINUTE;
  tm->tm_sec = trem / USECS_PER_SEC;
  *fsec = trem - tm->tm_sec * USECS_PER_SEC;

  if (tzp != NULL)
  {
    *tzp = time->zone;
  }

  return 0;
}

                  
                                                
                                                    
   
Datum
timetz_scale(PG_FUNCTION_ARGS)
{
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(0);
  int32 typmod = PG_GETARG_INT32(1);
  TimeTzADT *result;

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = time->time;
  result->zone = time->zone;

  AdjustTimeForTypmod(&(result->time), typmod);

  PG_RETURN_TIMETZADT_P(result);
}

static int
timetz_cmp_internal(TimeTzADT *time1, TimeTzADT *time2)
{
  TimeOffset t1, t2;

                                                     
  t1 = time1->time + (time1->zone * USECS_PER_SEC);
  t2 = time2->time + (time2->zone * USECS_PER_SEC);

  if (t1 > t2)
  {
    return 1;
  }
  if (t1 < t2)
  {
    return -1;
  }

     
                                                                      
                                                                   
     
  if (time1->zone > time2->zone)
  {
    return 1;
  }
  if (time1->zone < time2->zone)
  {
    return -1;
  }

  return 0;
}

Datum
timetz_eq(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_BOOL(timetz_cmp_internal(time1, time2) == 0);
}

Datum
timetz_ne(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_BOOL(timetz_cmp_internal(time1, time2) != 0);
}

Datum
timetz_lt(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_BOOL(timetz_cmp_internal(time1, time2) < 0);
}

Datum
timetz_le(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_BOOL(timetz_cmp_internal(time1, time2) <= 0);
}

Datum
timetz_gt(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_BOOL(timetz_cmp_internal(time1, time2) > 0);
}

Datum
timetz_ge(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_BOOL(timetz_cmp_internal(time1, time2) >= 0);
}

Datum
timetz_cmp(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);

  PG_RETURN_INT32(timetz_cmp_internal(time1, time2));
}

Datum
timetz_hash(PG_FUNCTION_ARGS)
{
  TimeTzADT *key = PG_GETARG_TIMETZADT_P(0);
  uint32 thash;

     
                                                                           
                                           
     
  thash = DatumGetUInt32(DirectFunctionCall1(hashint8, Int64GetDatumFast(key->time)));
  thash ^= DatumGetUInt32(hash_uint32(key->zone));
  PG_RETURN_UINT32(thash);
}

Datum
timetz_hash_extended(PG_FUNCTION_ARGS)
{
  TimeTzADT *key = PG_GETARG_TIMETZADT_P(0);
  Datum seed = PG_GETARG_DATUM(1);
  uint64 thash;

                                    
  thash = DatumGetUInt64(DirectFunctionCall2(hashint8extended, Int64GetDatumFast(key->time), seed));
  thash ^= DatumGetUInt64(hash_uint32_extended(key->zone, DatumGetInt64(seed)));
  PG_RETURN_UINT64(thash);
}

Datum
timetz_larger(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);
  TimeTzADT *result;

  if (timetz_cmp_internal(time1, time2) > 0)
  {
    result = time1;
  }
  else
  {
    result = time2;
  }
  PG_RETURN_TIMETZADT_P(result);
}

Datum
timetz_smaller(PG_FUNCTION_ARGS)
{
  TimeTzADT *time1 = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *time2 = PG_GETARG_TIMETZADT_P(1);
  TimeTzADT *result;

  if (timetz_cmp_internal(time1, time2) < 0)
  {
    result = time1;
  }
  else
  {
    result = time2;
  }
  PG_RETURN_TIMETZADT_P(result);
}

                        
                           
   
Datum
timetz_pl_interval(PG_FUNCTION_ARGS)
{
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  TimeTzADT *result;

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = time->time + span->time;
  result->time -= result->time / USECS_PER_DAY * USECS_PER_DAY;
  if (result->time < INT64CONST(0))
  {
    result->time += USECS_PER_DAY;
  }

  result->zone = time->zone;

  PG_RETURN_TIMETZADT_P(result);
}

                        
                                  
   
Datum
timetz_mi_interval(PG_FUNCTION_ARGS)
{
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(0);
  Interval *span = PG_GETARG_INTERVAL_P(1);
  TimeTzADT *result;

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = time->time - span->time;
  result->time -= result->time / USECS_PER_DAY * USECS_PER_DAY;
  if (result->time < INT64CONST(0))
  {
    result->time += USECS_PER_DAY;
  }

  result->zone = time->zone;

  PG_RETURN_TIMETZADT_P(result);
}

   
                                         
   
Datum
in_range_timetz_interval(PG_FUNCTION_ARGS)
{
  TimeTzADT *val = PG_GETARG_TIMETZADT_P(0);
  TimeTzADT *base = PG_GETARG_TIMETZADT_P(1);
  Interval *offset = PG_GETARG_INTERVAL_P(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  TimeTzADT sum;

     
                                                                            
                                                                     
     
  if (offset->time < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

     
                                                                            
                                                                             
                                                                             
                                                  
     
  if (sub)
  {
    sum.time = base->time - offset->time;
  }
  else
  {
    sum.time = base->time + offset->time;
  }
  sum.zone = base->zone;

  if (less)
  {
    PG_RETURN_BOOL(timetz_cmp_internal(val, &sum) <= 0);
  }
  else
  {
    PG_RETURN_BOOL(timetz_cmp_internal(val, &sum) >= 0);
  }
}

                                                               
   
                                                                    
                                                                           
                                      
   
Datum
overlaps_timetz(PG_FUNCTION_ARGS)
{
     
                                                                            
                                                                   
     
  Datum ts1 = PG_GETARG_DATUM(0);
  Datum te1 = PG_GETARG_DATUM(1);
  Datum ts2 = PG_GETARG_DATUM(2);
  Datum te2 = PG_GETARG_DATUM(3);
  bool ts1IsNull = PG_ARGISNULL(0);
  bool te1IsNull = PG_ARGISNULL(1);
  bool ts2IsNull = PG_ARGISNULL(2);
  bool te2IsNull = PG_ARGISNULL(3);

#define TIMETZ_GT(t1, t2) DatumGetBool(DirectFunctionCall2(timetz_gt, t1, t2))
#define TIMETZ_LT(t1, t2) DatumGetBool(DirectFunctionCall2(timetz_lt, t1, t2))

     
                                                                             
                                                                            
                                      
     
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
    if (TIMETZ_GT(ts1, te1))
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
    if (TIMETZ_GT(ts2, te2))
    {
      Datum tt = ts2;

      ts2 = te2;
      te2 = tt;
    }
  }

     
                                                                         
                                            
     
  if (TIMETZ_GT(ts1, ts2))
  {
       
                                                                         
                                                              
       
    if (te2IsNull)
    {
      PG_RETURN_NULL();
    }
    if (TIMETZ_LT(ts1, te2))
    {
      PG_RETURN_BOOL(true);
    }
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }

       
                                                                          
                                     
       
    PG_RETURN_BOOL(false);
  }
  else if (TIMETZ_LT(ts1, ts2))
  {
                                             
    if (te1IsNull)
    {
      PG_RETURN_NULL();
    }
    if (TIMETZ_LT(ts2, te1))
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

#undef TIMETZ_GT
#undef TIMETZ_LT
}

Datum
timetz_time(PG_FUNCTION_ARGS)
{
  TimeTzADT *timetz = PG_GETARG_TIMETZADT_P(0);
  TimeADT result;

                                                      
  result = timetz->time;

  PG_RETURN_TIMEADT(result);
}

Datum
time_timetz(PG_FUNCTION_ARGS)
{
  TimeADT time = PG_GETARG_TIMEADT(0);
  TimeTzADT *result;
  struct pg_tm tt, *tm = &tt;
  fsec_t fsec;
  int tz;

  GetCurrentDateTime(tm);
  time2tm(time, tm, &fsec);
  tz = DetermineTimeZoneOffset(tm, session_timezone);

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = time;
  result->zone = tz;

  PG_RETURN_TIMETZADT_P(result);
}

                        
                                          
   
Datum
timestamptz_timetz(PG_FUNCTION_ARGS)
{
  TimestampTz timestamp = PG_GETARG_TIMESTAMP(0);
  TimeTzADT *result;
  struct pg_tm tt, *tm = &tt;
  int tz;
  fsec_t fsec;

  if (TIMESTAMP_NOT_FINITE(timestamp))
  {
    PG_RETURN_NULL();
  }

  if (timestamp2tm(timestamp, &tz, tm, &fsec, NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  tm2timetz(tm, fsec, tz, result);

  PG_RETURN_TIMETZADT_P(result);
}

                            
                                                                  
                                                    
                                         
                       
   
Datum
datetimetz_timestamptz(PG_FUNCTION_ARGS)
{
  DateADT date = PG_GETARG_DATEADT(0);
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(1);
  TimestampTz result;

  if (DATE_IS_NOBEGIN(date))
  {
    TIMESTAMP_NOBEGIN(result);
  }
  else if (DATE_IS_NOEND(date))
  {
    TIMESTAMP_NOEND(result);
  }
  else
  {
       
                                                                        
                                                                          
                                              
       
    if (date >= (TIMESTAMP_END_JULIAN - POSTGRES_EPOCH_JDATE))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range for timestamp")));
    }
    result = date * USECS_PER_DAY + time->time + time->zone * USECS_PER_SEC;

       
                                                                           
                                                                        
       
    if (!IS_VALID_TIMESTAMP(result))
    {
      ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range for timestamp")));
    }
  }

  PG_RETURN_TIMESTAMP(result);
}

                 
                                           
   
Datum
timetz_part(PG_FUNCTION_ARGS)
{
  text *units = PG_GETARG_TEXT_PP(0);
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(1);
  float8 result;
  int type, val;
  char *lowunits;

  lowunits = downcase_truncate_identifier(VARDATA_ANY(units), VARSIZE_ANY_EXHDR(units), false);

  type = DecodeUnits(0, lowunits, &val);
  if (type == UNKNOWN_FIELD)
  {
    type = DecodeSpecial(0, lowunits, &val);
  }

  if (type == UNITS)
  {
    double dummy;
    int tz;
    fsec_t fsec;
    struct pg_tm tt, *tm = &tt;

    timetz2tm(time, tm, &fsec, &tz);

    switch (val)
    {
    case DTK_TZ:
      result = -tz;
      break;

    case DTK_TZ_MINUTE:
      result = -tz;
      result /= SECS_PER_MINUTE;
      FMODULO(result, dummy, (double)SECS_PER_MINUTE);
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
    case DTK_MONTH:
    case DTK_QUARTER:
    case DTK_YEAR:
    case DTK_DECADE:
    case DTK_CENTURY:
    case DTK_MILLENNIUM:
    default:
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"time with time zone\" units \"%s\" not recognized", lowunits)));
      result = 0;
    }
  }
  else if (type == RESERV && val == DTK_EPOCH)
  {
    result = time->time / 1000000.0 + time->zone;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"time with time zone\" units \"%s\" not recognized", lowunits)));
    result = 0;
  }

  PG_RETURN_FLOAT8(result);
}

                 
                                                             
                                             
   
Datum
timetz_zone(PG_FUNCTION_ARGS)
{
  text *zone = PG_GETARG_TEXT_PP(0);
  TimeTzADT *t = PG_GETARG_TIMETZADT_P(1);
  TimeTzADT *result;
  int tz;
  char tzname[TZ_STRLEN_MAX + 1];
  char *lowzone;
  int type, val;
  pg_tz *tzp;

     
                                                                    
                                                                            
                                                         
                                                                            
                                                                             
                                                                        
     
  text_to_cstring_buffer(zone, tzname, sizeof(tzname));

                                                     
  lowzone = downcase_truncate_identifier(tzname, strlen(tzname), false);

  type = DecodeTimezoneAbbrev(0, lowzone, &val, &tzp);

  if (type == TZ || type == DTZ)
  {
                                   
    tz = -val;
  }
  else if (type == DYNTZ)
  {
                                                                 
    pg_time_t now = (pg_time_t)time(NULL);
    struct pg_tm *tm;

    tm = pg_localtime(&now, tzp);
    tm->tm_year += 1900;                               
    tm->tm_mon += 1;
    tz = DetermineTimeZoneAbbrevOffset(tm, tzname, tzp);
  }
  else
  {
                                    
    tzp = pg_tzset(tzname);
    if (tzp)
    {
                                                                    
      pg_time_t now = (pg_time_t)time(NULL);
      struct pg_tm *tm;

      tm = pg_localtime(&now, tzp);
      tz = -tm->tm_gmtoff;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("time zone \"%s\" not recognized", tzname)));
      tz = 0;                          
    }
  }

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = t->time + (t->zone - tz) * USECS_PER_SEC;
  while (result->time < INT64CONST(0))
  {
    result->time += USECS_PER_DAY;
  }
  while (result->time >= USECS_PER_DAY)
  {
    result->time -= USECS_PER_DAY;
  }

  result->zone = tz;

  PG_RETURN_TIMETZADT_P(result);
}

                  
                                                                              
   
Datum
timetz_izone(PG_FUNCTION_ARGS)
{
  Interval *zone = PG_GETARG_INTERVAL_P(0);
  TimeTzADT *time = PG_GETARG_TIMETZADT_P(1);
  TimeTzADT *result;
  int tz;

  if (zone->month != 0 || zone->day != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("interval time zone \"%s\" must not include months or days", DatumGetCString(DirectFunctionCall1(interval_out, PointerGetDatum(zone))))));
  }

  tz = -(zone->time / USECS_PER_SEC);

  result = (TimeTzADT *)palloc(sizeof(TimeTzADT));

  result->time = time->time + (time->zone - tz) * USECS_PER_SEC;
  while (result->time < INT64CONST(0))
  {
    result->time += USECS_PER_DAY;
  }
  while (result->time >= USECS_PER_DAY)
  {
    result->time -= USECS_PER_DAY;
  }

  result->zone = tz;

  PG_RETURN_TIMETZADT_P(result);
}
