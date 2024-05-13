/*-------------------------------------------------------------------------
 *
 * date.c
 *	  implements DATE and TIME data types specified in SQL standard
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/date.c
 *
 *-------------------------------------------------------------------------
 */

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

/*
 * gcc's -ffast-math switch breaks routines that expect exact results from
 * expressions like timeval / SECS_PER_HOUR, where timeval is double.
 */
#ifdef __FAST_MATH__
#error -ffast-math is known to break this code
#endif

static int
tm2time(struct pg_tm *tm, fsec_t fsec, TimeADT *result);
static int
tm2timetz(struct pg_tm *tm, fsec_t fsec, int tz, TimeTzADT *result);
static void
AdjustTimeForTypmod(TimeADT *time, int32 typmod);

/* common code for timetypmodin and timetztypmodin */
static int32
anytime_typmodin(bool istz, ArrayType *ta)
{















}

/* exported so parse_expr.c can use it */
int32
anytime_typmod_check(bool istz, int32 typmod)
{











}

/* common code for timetypmodout and timetztypmodout */
static char *
anytime_typmodout(bool istz, int32 typmod)
{










}

/*****************************************************************************
 *	 Date ADT
 *****************************************************************************/

/* date_in()
 * Given date text string, convert to internal date format.
 */
Datum
date_in(PG_FUNCTION_ARGS)
{



























































}

/* date_out()
 * Given internal format date, convert to text string.
 */
Datum
date_out(PG_FUNCTION_ARGS)
{

















}

/*
 *		date_recv			- converts external binary
 *format to date
 */
Datum
date_recv(PG_FUNCTION_ARGS)
{














}

/*
 *		date_send			- converts date to binary format
 */
Datum
date_send(PG_FUNCTION_ARGS)
{






}

/*
 *		make_date			- date constructor
 */
Datum
make_date(PG_FUNCTION_ARGS)
{






































}

/*
 * Convert reserved date values to string.
 */
void
EncodeSpecialDate(DateADT dt, char *str)
{












}

/*
 * GetSQLCurrentDate -- implements CURRENT_DATE
 */
DateADT
GetSQLCurrentDate(void)
{













}

/*
 * GetSQLCurrentTime -- implements CURRENT_TIME, CURRENT_TIME(n)
 */
TimeTzADT *
GetSQLCurrentTime(int32 typmod)
{

















}

/*
 * GetSQLLocalTime -- implements LOCALTIME, LOCALTIME(n)
 */
TimeADT
GetSQLLocalTime(int32 typmod)
{
















}

/*
 * Comparison functions for dates
 */

Datum
date_eq(PG_FUNCTION_ARGS)
{




}

Datum
date_ne(PG_FUNCTION_ARGS)
{




}

Datum
date_lt(PG_FUNCTION_ARGS)
{




}

Datum
date_le(PG_FUNCTION_ARGS)
{




}

Datum
date_gt(PG_FUNCTION_ARGS)
{




}

Datum
date_ge(PG_FUNCTION_ARGS)
{




}

Datum
date_cmp(PG_FUNCTION_ARGS)
{












}

static int
date_fastcmp(Datum x, Datum y, SortSupport ssup)
{












}

Datum
date_sortsupport(PG_FUNCTION_ARGS)
{




}

Datum
date_finite(PG_FUNCTION_ARGS)
{



}

Datum
date_larger(PG_FUNCTION_ARGS)
{




}

Datum
date_smaller(PG_FUNCTION_ARGS)
{




}

/* Compute difference between two dates in days.
 */
Datum
date_mi(PG_FUNCTION_ARGS)
{









}

/* Add a number of days to a date, giving a new date.
 * Must handle both positive and negative numbers of days.
 */
Datum
date_pli(PG_FUNCTION_ARGS)
{


















}

/* Subtract a number of days from a date, giving a new date.
 */
Datum
date_mii(PG_FUNCTION_ARGS)
{


















}

/*
 * Internal routines for promoting date to timestamp and timestamp with
 * time zone
 */

static Timestamp
date2timestamp(DateADT dateVal)
{



























}

static TimestampTz
date2timestamptz(DateADT dateVal)
{











































}

/*
 * date2timestamp_no_overflow
 *
 * This is chartered to produce a double value that is numerically
 * equivalent to the corresponding Timestamp value, if the date is in the
 * valid range of Timestamps, but in any case not throw an overflow error.
 * We can do this since the numerical range of double is greater than
 * that of non-erroneous timestamps.  The results are currently only
 * used for statistical estimation purposes.
 */
double
date2timestamp_no_overflow(DateADT dateVal)
{

















}

/*
 * Crosstype comparison functions for dates
 */

Datum
date_eq_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_ne_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_lt_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_gt_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_le_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_ge_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_cmp_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
date_eq_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
date_ne_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
date_lt_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
date_gt_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
date_le_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
date_ge_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
date_cmp_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_eq_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_ne_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_lt_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_gt_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_le_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_ge_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_cmp_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_eq_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_ne_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_lt_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_gt_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_le_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_ge_date(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_cmp_date(PG_FUNCTION_ARGS)
{







}

/*
 * in_range support function for date.
 *
 * We implement this by promoting the dates to timestamp (without time zone)
 * and then using the timestamp-and-interval in_range function.
 */
Datum
in_range_date_interval(PG_FUNCTION_ARGS)
{












}

/* Add an interval to a date, giving a new date.
 * Must handle both positive and negative intervals.
 *
 * We implement this by promoting the date to timestamp (without time zone)
 * and then using the timestamp plus interval function.
 */
Datum
date_pl_interval(PG_FUNCTION_ARGS)
{







}

/* Subtract an interval from a date, giving a new date.
 * Must handle both positive and negative intervals.
 *
 * We implement this by promoting the date to timestamp (without time zone)
 * and then using the timestamp minus interval function.
 */
Datum
date_mi_interval(PG_FUNCTION_ARGS)
{







}

/* date_timestamp()
 * Convert date to timestamp data type.
 */
Datum
date_timestamp(PG_FUNCTION_ARGS)
{






}

/* timestamp_date()
 * Convert timestamp to date data type.
 */
Datum
timestamp_date(PG_FUNCTION_ARGS)
{
























}

/* date_timestamptz()
 * Convert date to timestamp with time zone data type.
 */
Datum
date_timestamptz(PG_FUNCTION_ARGS)
{






}

/* timestamptz_date()
 * Convert timestamp with time zone to date data type.
 */
Datum
timestamptz_date(PG_FUNCTION_ARGS)
{

























}

/*****************************************************************************
 *	 Time ADT
 *****************************************************************************/

Datum
time_in(PG_FUNCTION_ARGS)
{































}

/* tm2time()
 * Convert a tm structure to a time data type.
 */
static int
tm2time(struct pg_tm *tm, fsec_t fsec, TimeADT *result)
{


}

/* time_overflows()
 * Check to see if a broken-down time-of-day is out of range.
 */
bool
time_overflows(int hour, int min, int sec, fsec_t fsec)
{
















}

/* float_time_overflows()
 * Same, when we have seconds + fractional seconds as one "double" value.
 */
bool
float_time_overflows(int hour, int min, double sec)
{
































}

/* time2tm()
 * Convert time data type to POSIX time structure.
 *
 * For dates within the range of pg_time_t, convert to the local time zone.
 * If out of this range, leave as UTC (in practice that could only happen
 * if pg_time_t is just 32 bits) - thomas 97/05/27
 */
int
time2tm(TimeADT time, struct pg_tm *tm, fsec_t *fsec)
{








}

Datum
time_out(PG_FUNCTION_ARGS)
{











}

/*
 *		time_recv			- converts external binary
 *format to time
 */
Datum
time_recv(PG_FUNCTION_ARGS)
{


















}

/*
 *		time_send			- converts time to binary format
 */
Datum
time_send(PG_FUNCTION_ARGS)
{






}

Datum
timetypmodin(PG_FUNCTION_ARGS)
{



}

Datum
timetypmodout(PG_FUNCTION_ARGS)
{



}

/*
 *		make_time			- time constructor
 */
Datum
make_time(PG_FUNCTION_ARGS)
{















}

/* time_support()
 *
 * Planner support function for the time_scale() and timetz_scale()
 * length coercion functions (we need not distinguish them here).
 */
Datum
time_support(PG_FUNCTION_ARGS)
{











}

/* time_scale()
 * Adjust time type for specified scale factor.
 * Used by PostgreSQL type system to stuff columns.
 */
Datum
time_scale(PG_FUNCTION_ARGS)
{








}

/* AdjustTimeForTypmod()
 * Force the precision of the time value to a specified value.
 * Uses *exactly* the same code as in AdjustTimestampForTypmod()
 * but we make a separate copy because those types do not
 * have a fundamental tie together but rather a coincidence of
 * implementation. - thomas
 */
static void
AdjustTimeForTypmod(TimeADT *time, int32 typmod)
{















}

Datum
time_eq(PG_FUNCTION_ARGS)
{




}

Datum
time_ne(PG_FUNCTION_ARGS)
{




}

Datum
time_lt(PG_FUNCTION_ARGS)
{




}

Datum
time_le(PG_FUNCTION_ARGS)
{




}

Datum
time_gt(PG_FUNCTION_ARGS)
{




}

Datum
time_ge(PG_FUNCTION_ARGS)
{




}

Datum
time_cmp(PG_FUNCTION_ARGS)
{












}

Datum
time_hash(PG_FUNCTION_ARGS)
{

}

Datum
time_hash_extended(PG_FUNCTION_ARGS)
{

}

Datum
time_larger(PG_FUNCTION_ARGS)
{




}

Datum
time_smaller(PG_FUNCTION_ARGS)
{




}

/* overlaps_time() --- implements the SQL OVERLAPS operator.
 *
 * Algorithm is per SQL spec.  This is much harder than you'd think
 * because the spec requires us to deliver a non-null answer in some cases
 * where some of the inputs are null.
 */
Datum
overlaps_time(PG_FUNCTION_ARGS)
{


































































































































}

/* timestamp_time()
 * Convert timestamp to time data type.
 */
Datum
timestamp_time(PG_FUNCTION_ARGS)
{






















}

/* timestamptz_time()
 * Convert timestamptz to time data type.
 */
Datum
timestamptz_time(PG_FUNCTION_ARGS)
{























}

/* datetime_timestamp()
 * Convert date and time to timestamp data type.
 */
Datum
datetime_timestamp(PG_FUNCTION_ARGS)
{















}

/* time_interval()
 * Convert time to interval data type.
 */
Datum
time_interval(PG_FUNCTION_ARGS)
{










}

/* interval_time()
 * Convert interval to time data type.
 *
 * This is defined as producing the fractional-day portion of the interval.
 * Therefore, we can just ignore the months field.  It is not real clear
 * what to do with negative intervals, but we choose to subtract the floor,
 * so that, say, '-2 hours' becomes '22:00:00'.
 */
Datum
interval_time(PG_FUNCTION_ARGS)
{

















}

/* time_mi_time()
 * Subtract two times to produce an interval.
 */
Datum
time_mi_time(PG_FUNCTION_ARGS)
{











}

/* time_pl_interval()
 * Add interval to time.
 */
Datum
time_pl_interval(PG_FUNCTION_ARGS)
{












}

/* time_mi_interval()
 * Subtract interval from time.
 */
Datum
time_mi_interval(PG_FUNCTION_ARGS)
{












}

/*
 * in_range support function for time.
 */
Datum
in_range_time_interval(PG_FUNCTION_ARGS)
{







































}

/* time_part()
 * Extract specified field from time type.
 */
Datum
time_part(PG_FUNCTION_ARGS)
{






































































}

/*****************************************************************************
 *	 Time With Time Zone ADT
 *****************************************************************************/

/* tm2timetz()
 * Convert a tm structure to a time data type.
 */
static int
tm2timetz(struct pg_tm *tm, fsec_t fsec, int tz, TimeTzADT *result)
{




}

Datum
timetz_in(PG_FUNCTION_ARGS)
{
































}

Datum
timetz_out(PG_FUNCTION_ARGS)
{












}

/*
 *		timetz_recv			- converts external binary
 *format to timetz
 */
Datum
timetz_recv(PG_FUNCTION_ARGS)
{




























}

/*
 *		timetz_send			- converts timetz to binary
 *format
 */
Datum
timetz_send(PG_FUNCTION_ARGS)
{







}

Datum
timetztypmodin(PG_FUNCTION_ARGS)
{



}

Datum
timetztypmodout(PG_FUNCTION_ARGS)
{



}

/* timetz2tm()
 * Convert TIME WITH TIME ZONE data type to POSIX time structure.
 */
int
timetz2tm(TimeTzADT *time, struct pg_tm *tm, fsec_t *fsec, int *tzp)
{















}

/* timetz_scale()
 * Adjust time type for specified scale factor.
 * Used by PostgreSQL type system to stuff columns.
 */
Datum
timetz_scale(PG_FUNCTION_ARGS)
{












}

static int
timetz_cmp_internal(TimeTzADT *time1, TimeTzADT *time2)
{





























}

Datum
timetz_eq(PG_FUNCTION_ARGS)
{




}

Datum
timetz_ne(PG_FUNCTION_ARGS)
{




}

Datum
timetz_lt(PG_FUNCTION_ARGS)
{




}

Datum
timetz_le(PG_FUNCTION_ARGS)
{




}

Datum
timetz_gt(PG_FUNCTION_ARGS)
{




}

Datum
timetz_ge(PG_FUNCTION_ARGS)
{




}

Datum
timetz_cmp(PG_FUNCTION_ARGS)
{




}

Datum
timetz_hash(PG_FUNCTION_ARGS)
{










}

Datum
timetz_hash_extended(PG_FUNCTION_ARGS)
{








}

Datum
timetz_larger(PG_FUNCTION_ARGS)
{













}

Datum
timetz_smaller(PG_FUNCTION_ARGS)
{













}

/* timetz_pl_interval()
 * Add interval to timetz.
 */
Datum
timetz_pl_interval(PG_FUNCTION_ARGS)
{
















}

/* timetz_mi_interval()
 * Subtract interval from timetz.
 */
Datum
timetz_mi_interval(PG_FUNCTION_ARGS)
{
















}

/*
 * in_range support function for timetz.
 */
Datum
in_range_timetz_interval(PG_FUNCTION_ARGS)
{








































}

/* overlaps_timetz() --- implements the SQL OVERLAPS operator.
 *
 * Algorithm is per SQL spec.  This is much harder than you'd think
 * because the spec requires us to deliver a non-null answer in some cases
 * where some of the inputs are null.
 */
Datum
overlaps_timetz(PG_FUNCTION_ARGS)
{


































































































































}

Datum
timetz_time(PG_FUNCTION_ARGS)
{







}

Datum
time_timetz(PG_FUNCTION_ARGS)
{
















}

/* timestamptz_timetz()
 * Convert timestamp to timetz data type.
 */
Datum
timestamptz_timetz(PG_FUNCTION_ARGS)
{





















}

/* datetimetz_timestamptz()
 * Convert date and timetz to timestamp with time zone data type.
 * Timestamp is stored in GMT, so add the time zone
 * stored with the timetz to the result.
 * - thomas 2000-03-10
 */
Datum
datetimetz_timestamptz(PG_FUNCTION_ARGS)
{




































}

/* timetz_part()
 * Extract specified field from time type.
 */
Datum
timetz_part(PG_FUNCTION_ARGS)
{



















































































}

/* timetz_zone()
 * Encode time with time zone type with specified time zone.
 * Applies DST rules as of the current date.
 */
Datum
timetz_zone(PG_FUNCTION_ARGS)
{











































































}

/* timetz_izone()
 * Encode time with time zone type with specified time interval as time zone.
 */
Datum
timetz_izone(PG_FUNCTION_ARGS)
{



























}