/*-------------------------------------------------------------------------
 *
 * timestamp.c
 *	  Functions for the built-in SQL types "timestamp" and "interval".
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/timestamp.c
 *
 *-------------------------------------------------------------------------
 */

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

/*
 * gcc's -ffast-math switch breaks routines that expect exact results from
 * expressions like timeval / SECS_PER_HOUR, where timeval is double.
 */
#ifdef __FAST_MATH__
#error -ffast-math is known to break this code
#endif

#define SAMESIGN(a, b) (((a) < 0) == ((b) < 0))

/* Set at postmaster start */
TimestampTz PgStartTime;

/* Set at configuration reload */
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

/* common code for timestamptypmodin and timestamptztypmodin */
static int32
anytimestamp_typmodin(bool istz, ArrayType *ta)
{















}

/* exported so parse_expr.c can use it */
int32
anytimestamp_typmod_check(bool istz, int32 typmod)
{











}

/* common code for timestamptypmodout and timestamptztypmodout */
static char *
anytimestamp_typmodout(bool istz, int32 typmod)
{










}

/*****************************************************************************
 *	 USER I/O ROUTINES
 **
 *****************************************************************************/

/* timestamp_in()
 * Convert a string to internal form.
 */
Datum
timestamp_in(PG_FUNCTION_ARGS)
{
























































}

/* timestamp_out()
 * Convert a timestamp to external form.
 */
Datum
timestamp_out(PG_FUNCTION_ARGS)
{





















}

/*
 *		timestamp_recv			- converts external binary
 *format to timestamp
 */
Datum
timestamp_recv(PG_FUNCTION_ARGS)
{























}

/*
 *		timestamp_send			- converts timestamp to binary
 *format
 */
Datum
timestamp_send(PG_FUNCTION_ARGS)
{






}

Datum
timestamptypmodin(PG_FUNCTION_ARGS)
{



}

Datum
timestamptypmodout(PG_FUNCTION_ARGS)
{



}

/*
 * timestamp_support()
 *
 * Planner support function for the timestamp_scale() and timestamptz_scale()
 * length coercion functions (we need not distinguish them here).
 */
Datum
timestamp_support(PG_FUNCTION_ARGS)
{











}

/* timestamp_scale()
 * Adjust time type for specified scale factor.
 * Used by PostgreSQL type system to stuff columns.
 */
Datum
timestamp_scale(PG_FUNCTION_ARGS)
{









}

/*
 * AdjustTimestampForTypmod --- round off a timestamp to suit given typmod
 * Works for either timestamp or timestamptz.
 */
static void
AdjustTimestampForTypmod(Timestamp *time, int32 typmod)
{




















}

/* timestamptz_in()
 * Convert a string to internal form.
 */
Datum
timestamptz_in(PG_FUNCTION_ARGS)
{
























































}

/*
 * Try to parse a timezone specification, and return its timezone offset value
 * if it's acceptable.  Otherwise, an error is thrown.
 *
 * Note: some code paths update tm->tm_isdst, and some don't; current callers
 * don't care, so we don't bother being consistent.
 */
static int
parse_sane_timezone(struct pg_tm *tm, text *zone)
{








































































}

/*
 * make_timestamp_internal
 *		workhorse for make_timestamp and make_timestamptz
 */
static Timestamp
make_timestamp_internal(int year, int month, int day, int hour, int min, double sec)
{


























































}

/*
 * make_timestamp() - timestamp constructor
 */
Datum
make_timestamp(PG_FUNCTION_ARGS)
{











}

/*
 * make_timestamptz() - timestamp with time zone constructor
 */
Datum
make_timestamptz(PG_FUNCTION_ARGS)
{











}

/*
 * Construct a timestamp with time zone.
 *		As above, but the time zone is specified as seventh argument.
 */
Datum
make_timestamptz_at_timezone(PG_FUNCTION_ARGS)
{






























}

/*
 * to_timestamp(double precision)
 * Convert UNIX epoch to timestamptz.
 */
Datum
float8_timestamptz(PG_FUNCTION_ARGS)
{










































}

/* timestamptz_out()
 * Convert a timestamp to external form.
 */
Datum
timestamptz_out(PG_FUNCTION_ARGS)
{























}

/*
 *		timestamptz_recv			- converts external
 *binary format to timestamptz
 */
Datum
timestamptz_recv(PG_FUNCTION_ARGS)
{
























}

/*
 *		timestamptz_send			- converts timestamptz
 *to binary format
 */
Datum
timestamptz_send(PG_FUNCTION_ARGS)
{






}

Datum
timestamptztypmodin(PG_FUNCTION_ARGS)
{



}

Datum
timestamptztypmodout(PG_FUNCTION_ARGS)
{



}

/* timestamptz_scale()
 * Adjust time type for specified scale factor.
 * Used by PostgreSQL type system to stuff columns.
 */
Datum
timestamptz_scale(PG_FUNCTION_ARGS)
{









}

/* interval_in()
 * Convert a string to internal form.
 *
 * External format(s):
 *	Uses the generic date/time parsing and decoding routines.
 */
Datum
interval_in(PG_FUNCTION_ARGS)
{









































































}

/* interval_out()
 * Convert a time span to external form.
 */
Datum
interval_out(PG_FUNCTION_ARGS)
{















}

/*
 *		interval_recv			- converts external binary
 *format to interval
 */
Datum
interval_recv(PG_FUNCTION_ARGS)
{

















}

/*
 *		interval_send			- converts interval to binary
 *format
 */
Datum
interval_send(PG_FUNCTION_ARGS)
{








}

/*
 * The interval typmod stores a "range" in its high 16 bits and a "precision"
 * in its low 16 bits.  Both contribute to defining the resolution of the
 * type.  Range addresses resolution granules larger than one second, and
 * precision specifies resolution below one second.  This representation can
 * express all SQL standard resolutions, but we implement them all in terms of
 * truncating rightward from some position.  Range is a bitmap of permitted
 * fields, but only the temporally-smallest such field is significant to our
 * calculations.  Precision is a count of sub-second decimal places to retain.
 * Setting all bits (INTERVAL_FULL_PRECISION) gives the same truncation
 * semantics as choosing MAX_INTERVAL_PRECISION.
 */
Datum
intervaltypmodin(PG_FUNCTION_ARGS)
{








































































}

Datum
intervaltypmodout(PG_FUNCTION_ARGS)
{











































































}

/*
 * Given an interval typmod value, return a code for the least-significant
 * field that the typmod allows to be nonzero, for instance given
 * INTERVAL DAY TO HOUR we want to identify "hour".
 *
 * The results should be ordered by field significance, which means
 * we can't use the dt.h macros YEAR etc, because for some odd reason
 * they aren't ordered that way.  Instead, arbitrarily represent
 * SECOND = 0, MINUTE = 1, HOUR = 2, DAY = 3, MONTH = 4, YEAR = 5.
 */
static int
intervaltypmodleastfield(int32 typmod)
{








































}

/*
 * interval_support()
 *
 * Planner support function for interval_scale().
 *
 * Flatten superfluous calls to interval_scale().  The interval typmod is
 * complex to permit accepting and regurgitating all SQL standard variations.
 * For truncation purposes, it boils down to a single, simple granularity.
 */
Datum
interval_support(PG_FUNCTION_ARGS)
{



























































}

/* interval_scale()
 * Adjust interval type for specified fields.
 * Used by PostgreSQL type system to stuff columns.
 */
Datum
interval_scale(PG_FUNCTION_ARGS)
{










}

/*
 *	Adjust interval for specified precision, in both YEAR to SECOND
 *	range and sub-second precision.
 */
static void
AdjustIntervalForTypmod(Interval *interval, int32 typmod)
{



























































































































}

/*
 * make_interval - numeric Interval constructor
 */
Datum
make_interval(PG_FUNCTION_ARGS)
{


























}

/* EncodeSpecialTimestamp()
 * Convert reserved timestamp data type to string.
 */
void
EncodeSpecialTimestamp(Timestamp dt, char *str)
{












}

Datum
now(PG_FUNCTION_ARGS)
{

}

Datum
statement_timestamp(PG_FUNCTION_ARGS)
{

}

Datum
clock_timestamp(PG_FUNCTION_ARGS)
{

}

Datum
pg_postmaster_start_time(PG_FUNCTION_ARGS)
{

}

Datum
pg_conf_load_time(PG_FUNCTION_ARGS)
{

}

/*
 * GetCurrentTimestamp -- get the current operating system time
 *
 * Result is in the form of a TimestampTz value, and is expressed to the
 * full precision of the gettimeofday() syscall
 */
TimestampTz
GetCurrentTimestamp(void)
{









}

/*
 * GetSQLCurrentTimestamp -- implements CURRENT_TIMESTAMP, CURRENT_TIMESTAMP(n)
 */
TimestampTz
GetSQLCurrentTimestamp(int32 typmod)
{








}

/*
 * GetSQLLocalTimestamp -- implements LOCALTIMESTAMP, LOCALTIMESTAMP(n)
 */
Timestamp
GetSQLLocalTimestamp(int32 typmod)
{








}

/*
 * timeofday(*) -- returns the current time as a text.
 */
Datum
timeofday(PG_FUNCTION_ARGS)
{











}

/*
 * TimestampDifference -- convert the difference between two timestamps
 *		into integer seconds and microseconds
 *
 * This is typically used to calculate a wait timeout for select(2),
 * which explains the otherwise-odd choice of output format.
 *
 * Both inputs must be ordinary finite timestamps (in current usage,
 * they'll be results from GetCurrentTimestamp()).
 *
 * We expect start_time <= stop_time.  If not, we return zeros,
 * since then we're already past the previously determined stop_time.
 */
void
TimestampDifference(TimestampTz start_time, TimestampTz stop_time, long *secs, int *microsecs)
{












}

/*
 * TimestampDifferenceMilliseconds -- convert the difference between two
 * 		timestamps into integer milliseconds
 *
 * This is typically used to calculate a wait timeout for WaitLatch()
 * or a related function.  The choice of "long" as the result type
 * is to harmonize with that.  It is caller's responsibility that the
 * input timestamps not be so far apart as to risk overflow of "long"
 * (which'd happen at about 25 days on machines with 32-bit "long").
 *
 * Both inputs must be ordinary finite timestamps (in current usage,
 * they'll be results from GetCurrentTimestamp()).
 *
 * We expect start_time <= stop_time.  If not, we return zero,
 * since then we're already past the previously determined stop_time.
 *
 * Note we round up any fractional millisecond, since waiting for just
 * less than the intended timeout is undesirable.
 */
long
TimestampDifferenceMilliseconds(TimestampTz start_time, TimestampTz stop_time)
{










}

/*
 * TimestampDifferenceExceeds -- report whether the difference between two
 *		timestamps is >= a threshold (expressed in milliseconds)
 *
 * Both inputs must be ordinary finite timestamps (in current usage,
 * they'll be results from GetCurrentTimestamp()).
 */
bool
TimestampDifferenceExceeds(TimestampTz start_time, TimestampTz stop_time, int msec)
{



}

/*
 * Convert a time_t to TimestampTz.
 *
 * We do not use time_t internally in Postgres, but this is provided for use
 * by functions that need to interpret, say, a stat(2) result.
 *
 * To avoid having the function's ABI vary depending on the width of time_t,
 * we declare the argument as pg_time_t, which is cast-compatible with
 * time_t but always 64 bits wide (unless the platform has no 64-bit type).
 * This detail should be invisible to callers, at least at source code level.
 */
TimestampTz
time_t_to_timestamptz(pg_time_t tm)
{






}

/*
 * Convert a TimestampTz to time_t.
 *
 * This too is just marginally useful, but some places need it.
 *
 * To avoid having the function's ABI vary depending on the width of time_t,
 * we declare the result as pg_time_t, which is cast-compatible with
 * time_t but always 64 bits wide (unless the platform has no 64-bit type).
 * This detail should be invisible to callers, at least at source code level.
 */
pg_time_t
timestamptz_to_time_t(TimestampTz t)
{





}

/*
 * Produce a C-string representation of a TimestampTz.
 *
 * This is mostly for use in emitting messages.  The primary difference
 * from timestamptz_out is that we force the output format to ISO.  Note
 * also that the result is in a static buffer, not pstrdup'd.
 */
const char *
timestamptz_to_str(TimestampTz t)
{




















}

void
dt2time(Timestamp jd, int *hour, int *min, int *sec, fsec_t *fsec)
{










} /* dt2time() */

/*
 * timestamp2tm() - Convert timestamp data type to POSIX time structure.
 *
 * Note that year is _not_ 1900-based, but is an explicit full value.
 * Also, month is one-based, _not_ zero-based.
 * Returns:
 *	 0 on success
 *	-1 on out of range
 *
 * If attimezone is NULL, the global timezone setting will be used.
 */
int
timestamp2tm(Timestamp dt, int *tzp, struct pg_tm *tm, fsec_t *fsec, const char **tzn, pg_tz *attimezone)
{




























































































}

/* tm2timestamp()
 * Convert a tm structure to a timestamp data type.
 * Note that year is _not_ 1900-based, but is an explicit full value.
 * Also, month is one-based, _not_ zero-based.
 *
 * Returns -1 on failure (value out of range).
 */
int
tm2timestamp(struct pg_tm *tm, fsec_t fsec, int *tzp, Timestamp *result)
{








































}

/* interval2tm()
 * Convert an interval data type to a tm structure.
 */
int
interval2tm(Interval span, struct pg_tm *tm, fsec_t *fsec)
{























}

int
tm2interval(struct pg_tm *tm, fsec_t fsec, Interval *span)
{











}

static TimeOffset
time2t(const int hour, const int min, const int sec, const fsec_t fsec)
{

}

static Timestamp
dt2local(Timestamp dt, int tz)
{


}

/*****************************************************************************
 *	 PUBLIC ROUTINES
 **
 *****************************************************************************/

Datum
timestamp_finite(PG_FUNCTION_ARGS)
{



}

Datum
interval_finite(PG_FUNCTION_ARGS)
{

}

/*----------------------------------------------------------
 *	Relational operators for timestamp.
 *---------------------------------------------------------*/

void
GetEpochTime(struct pg_tm *tm)
{



















}

Timestamp
SetEpochTimestamp(void)
{








} /* SetEpochTimestamp() */

/*
 * We are currently sharing some code between timestamp and timestamptz.
 * The comparison functions are among them. - thomas 2001-09-25
 *
 *		timestamp_relop - is timestamp1 relop timestamp2
 */
int
timestamp_cmp_internal(Timestamp dt1, Timestamp dt2)
{

}

Datum
timestamp_eq(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_ne(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_lt(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_gt(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_le(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_ge(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_cmp(PG_FUNCTION_ARGS)
{




}

/* note: this is used for timestamptz also */
static int
timestamp_fastcmp(Datum x, Datum y, SortSupport ssup)
{




}

Datum
timestamp_sortsupport(PG_FUNCTION_ARGS)
{




}

Datum
timestamp_hash(PG_FUNCTION_ARGS)
{

}

Datum
timestamp_hash_extended(PG_FUNCTION_ARGS)
{

}

/*
 * Cross-type comparison functions for timestamp vs timestamptz
 */

Datum
timestamp_eq_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_ne_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_lt_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_gt_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_le_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_ge_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamp_cmp_timestamptz(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_eq_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_ne_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_lt_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_gt_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_le_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_ge_timestamp(PG_FUNCTION_ARGS)
{







}

Datum
timestamptz_cmp_timestamp(PG_FUNCTION_ARGS)
{







}

/*
 *		interval_relop	- is interval1 relop interval2
 *
 * Interval comparison is based on converting interval values to a linear
 * representation expressed in the units of the time field (microseconds,
 * in the case of integer timestamps) with days assumed to be always 24 hours
 * and months assumed to be always 30 days.  To avoid overflow, we need a
 * wider-than-int64 datatype for the linear representation, so use INT128.
 */

static inline INT128
interval_cmp_value(const Interval *interval)
{




















}

static int
interval_cmp_internal(Interval *interval1, Interval *interval2)
{




}

Datum
interval_eq(PG_FUNCTION_ARGS)
{




}

Datum
interval_ne(PG_FUNCTION_ARGS)
{




}

Datum
interval_lt(PG_FUNCTION_ARGS)
{




}

Datum
interval_gt(PG_FUNCTION_ARGS)
{




}

Datum
interval_le(PG_FUNCTION_ARGS)
{




}

Datum
interval_ge(PG_FUNCTION_ARGS)
{




}

Datum
interval_cmp(PG_FUNCTION_ARGS)
{




}

/*
 * Hashing for intervals
 *
 * We must produce equal hashvals for values that interval_cmp_internal()
 * considers equal.  So, compute the net span the same way it does,
 * and then hash that.
 */
Datum
interval_hash(PG_FUNCTION_ARGS)
{













}

Datum
interval_hash_extended(PG_FUNCTION_ARGS)
{








}

/* overlaps_timestamp() --- implements the SQL OVERLAPS operator.
 *
 * Algorithm is per SQL spec.  This is much harder than you'd think
 * because the spec requires us to deliver a non-null answer in some cases
 * where some of the inputs are null.
 */
Datum
overlaps_timestamp(PG_FUNCTION_ARGS)
{



































































































































}

/*----------------------------------------------------------
 *	"Arithmetic" operators on date/times.
 *---------------------------------------------------------*/

Datum
timestamp_smaller(PG_FUNCTION_ARGS)
{














}

Datum
timestamp_larger(PG_FUNCTION_ARGS)
{













}

Datum
timestamp_mi(PG_FUNCTION_ARGS)
{












































}

/*
 *	interval_justify_interval()
 *
 *	Adjust interval so 'month', 'day', and 'time' portions are within
 *	customary bounds.  Specifically:
 *
 *		0 <= abs(time) < 24 hours
 *		0 <= abs(day)  < 30 days
 *
 *	Also, the sign bit on all three fields is made equal, so either
 *	all three fields are negative or all are positive.
 */
Datum
interval_justify_interval(PG_FUNCTION_ARGS)
{








































}

/*
 *	interval_justify_hours()
 *
 *	Adjust interval so 'time' contains less than a whole day, adding
 *	the excess to 'day'.  This is useful for
 *	situations (such as non-TZ) where '1 day' = '24 hours' is valid,
 *	e.g. interval subtraction and division.
 */
Datum
interval_justify_hours(PG_FUNCTION_ARGS)
{
























}

/*
 *	interval_justify_days()
 *
 *	Adjust interval so 'day' contains less than 30 days, adding
 *	the excess to 'month'.
 */
Datum
interval_justify_days(PG_FUNCTION_ARGS)
{

























}

/* timestamp_pl_interval()
 * Add an interval to a timestamp data type.
 * Note that interval has provisions for qualitative year/month and day
 *	units, so try to do the right thing with them.
 * To add a month, increment the month, and use the same day of month.
 * Then, if the next month has fewer days, set the day of month
 *	to the last day of month.
 * To add a day, increment the mday, and use the same time of day.
 * Lastly, add in the "quantitative time".
 */
Datum
timestamp_pl_interval(PG_FUNCTION_ARGS)
{












































































}

Datum
timestamp_mi_interval(PG_FUNCTION_ARGS)
{









}

/* timestamptz_pl_interval()
 * Add an interval to a timestamp with time zone data type.
 * Note that interval has provisions for qualitative year/month
 *	units, so try to do the right thing with them.
 * To add a month, increment the month, and use the same day of month.
 * Then, if the next month has fewer days, set the day of month
 *	to the last day of month.
 * Lastly, add in the "quantitative time".
 */
Datum
timestamptz_pl_interval(PG_FUNCTION_ARGS)
{

















































































}

Datum
timestamptz_mi_interval(PG_FUNCTION_ARGS)
{









}

Datum
interval_um(PG_FUNCTION_ARGS)
{























}

Datum
interval_smaller(PG_FUNCTION_ARGS)
{














}

Datum
interval_larger(PG_FUNCTION_ARGS)
{













}

Datum
interval_pl(PG_FUNCTION_ARGS)
{


























}

Datum
interval_mi(PG_FUNCTION_ARGS)
{


























}

/*
 *	There is no interval_abs():  it is unclear what value to return:
 *	  http://archives.postgresql.org/pgsql-general/2009-10/msg01031.php
 *	  http://archives.postgresql.org/pgsql-general/2009-11/msg00041.php
 */

Datum
interval_mul(PG_FUNCTION_ARGS)
{


































































}

Datum
mul_d_interval(PG_FUNCTION_ARGS)
{





}

Datum
interval_div(PG_FUNCTION_ARGS)
{


































}

/*
 * in_range support functions for timestamps and intervals.
 *
 * Per SQL spec, we support these with interval as the offset type.
 * The spec's restriction that the offset not be negative is a bit hard to
 * decipher for intervals, but we choose to interpret it the same as our
 * interval comparison operators would.
 */

Datum
in_range_timestamptz_interval(PG_FUNCTION_ARGS)
{






























}

Datum
in_range_timestamp_interval(PG_FUNCTION_ARGS)
{






























}

Datum
in_range_interval_interval(PG_FUNCTION_ARGS)
{






























}

/*
 * interval_accum, interval_accum_inv, and interval_avg implement the
 * AVG(interval) aggregate.
 *
 * The transition datatype for this aggregate is a 2-element array of
 * intervals, where the first is the running sum and the second contains
 * the number of values so far in its 'time' field.  This is a bit ugly
 * but it beats inventing a specialized datatype for the purpose.
 */

Datum
interval_accum(PG_FUNCTION_ARGS)
{


























}

Datum
interval_combine(PG_FUNCTION_ARGS)
{







































}

Datum
interval_accum_inv(PG_FUNCTION_ARGS)
{


























}

Datum
interval_avg(PG_FUNCTION_ARGS)
{





















}

/* timestamp_age()
 * Calculate time difference while retaining year/month fields.
 * Note that this does not result in an accurate absolute time span
 *	since year and month are out of context once the arithmetic
 *	is done.
 */
Datum
timestamp_age(PG_FUNCTION_ARGS)
{





































































































}

/* timestamptz_age()
 * Calculate time difference while retaining year/month fields.
 * Note that this does not result in an accurate absolute time span
 *	since year and month are out of context once the arithmetic
 *	is done.
 */
Datum
timestamptz_age(PG_FUNCTION_ARGS)
{











































































































}

/*----------------------------------------------------------
 *	Conversion operators.
 *---------------------------------------------------------*/

/* timestamp_trunc()
 * Truncate timestamp to specified units.
 */
Datum
timestamp_trunc(PG_FUNCTION_ARGS)
{






































































































































}

/*
 * Common code for timestamptz_trunc() and timestamptz_trunc_zone().
 *
 * tzp identifies the zone to truncate with respect to.  We assume
 * infinite timestamps have already been rejected.
 */
static TimestampTz
timestamptz_trunc_internal(text *units, TimestampTz timestamp, pg_tz *tzp)
{
















































































































































}

/* timestamptz_trunc()
 * Truncate timestamptz to specified units in session timezone.
 */
Datum
timestamptz_trunc(PG_FUNCTION_ARGS)
{












}

/* timestamptz_trunc_zone()
 * Truncate timestamptz to specified units in specified timezone.
 */
Datum
timestamptz_trunc_zone(PG_FUNCTION_ARGS)
{


















































}

/* interval_trunc()
 * Extract specified field from interval.
 */
Datum
interval_trunc(PG_FUNCTION_ARGS)
{






















































































}

/* isoweek2j()
 *
 *	Return the Julian day which corresponds to the first day (Monday) of the
 *given ISO 8601 year and week. Julian days are used to convert between ISO week
 *dates and Gregorian dates.
 */
int
isoweek2j(int year, int week)
{









}

/* isoweek2date()
 * Convert ISO week of year number to date.
 * The year field must be specified with the ISO year!
 * karel 2000/08/07
 */
void
isoweek2date(int woy, int *year, int *mon, int *mday)
{

}

/* isoweekdate2date()
 *
 *	Convert an ISO 8601 week date (ISO year, ISO week) into a Gregorian
 *date. Gregorian day of week sent so weekday strings can be supplied. Populates
 *year, mon, and mday with the correct Gregorian values. year must be passed in
 *as the ISO year.
 */
void
isoweekdate2date(int isoweek, int wday, int *year, int *mon, int *mday)
{













}

/* date2isoweek()
 *
 *	Returns ISO week number of year.
 */
int
date2isoweek(int year, int mon, int mday)
{












































}

/* date2isoyear()
 *
 *	Returns ISO 8601 year number.
 *	Note: zero or negative results follow the year-zero-exists convention.
 */
int
date2isoyear(int year, int mon, int mday)
{














































}

/* date2isoyearday()
 *
 *	Returns the ISO 8601 day-of-year, given a Gregorian year, month and day.
 *	Possible return values are 1 through 371 (364 in non-leap years).
 */
int
date2isoyearday(int year, int mon, int mday)
{

}

/*
 * NonFiniteTimestampTzPart
 *
 *	Used by timestamp_part and timestamptz_part when extracting from
 *infinite timestamp[tz].  Returns +/-Infinity if that is the appropriate
 *result, otherwise returns zero (which should be taken as meaning to return
 *NULL).
 *
 *	Errors thrown here for invalid units should exactly match those that
 *	would be thrown in the calling functions, else there will be unexpected
 *	discrepancies between finite- and infinite-input cases.
 */
static float8
NonFiniteTimestampTzPart(int type, int unit, char *lowunits, bool isNegative, bool isTz)
{




























































}

/* timestamp_part()
 * Extract specified field from timestamp.
 */
Datum
timestamp_part(PG_FUNCTION_ARGS)
{







































































































































































































}

/* timestamptz_part()
 * Extract specified field from timestamp with time zone.
 */
Datum
timestamptz_part(PG_FUNCTION_ARGS)
{










































































































































































































}

/* interval_part()
 * Extract specified field from interval.
 */
Datum
interval_part(PG_FUNCTION_ARGS)
{


































































































}

/*	timestamp_zone()
 *	Encode timestamp type with specified time zone.
 *	This function is just timestamp2timestamptz() except instead of
 *	shifting to the global timezone, we shift to the specified timezone.
 *	This is different from the other AT TIME ZONE cases because instead
 *	of shifting _to_ a new time zone, it sets the time to _be_ the
 *	specified timezone.
 */
Datum
timestamp_zone(PG_FUNCTION_ARGS)
{













































































}

/* timestamp_izone()
 * Encode timestamp type with specified time interval as time zone.
 */
Datum
timestamp_izone(PG_FUNCTION_ARGS)
{

























} /* timestamp_izone() */

/* TimestampTimestampTzRequiresRewrite()
 *
 * Returns false if the TimeZone GUC setting causes timestamp_timestamptz and
 * timestamptz_timestamp to be no-ops, where the return value has the same
 * bits as the argument.  Since project convention is to assume a GUC changes
 * no more often than STABLE functions change, the answer is valid that long.
 */
bool
TimestampTimestampTzRequiresRewrite(void)
{







}

/* timestamp_timestamptz()
 * Convert local timestamp to timestamp at GMT
 */
Datum
timestamp_timestamptz(PG_FUNCTION_ARGS)
{



}

static TimestampTz
timestamp2timestamptz(Timestamp timestamp)
{

























}

/* timestamptz_timestamp()
 * Convert timestamp at GMT to local timestamp
 */
Datum
timestamptz_timestamp(PG_FUNCTION_ARGS)
{



}

static Timestamp
timestamptz2timestamp(TimestampTz timestamp)
{





















}

/* timestamptz_zone()
 * Evaluate timestamp with time zone type at the specified time zone.
 * Returns a timestamp without time zone.
 */
Datum
timestamptz_zone(PG_FUNCTION_ARGS)
{











































































}

/* timestamptz_izone()
 * Encode timestamp with time zone type with specified time interval as time
 * zone. Returns a timestamp without time zone.
 */
Datum
timestamptz_izone(PG_FUNCTION_ARGS)
{

























}

/* generate_series_timestamp()
 * Generate the set of timestamps from start to finish by step
 */
Datum
generate_series_timestamp(PG_FUNCTION_ARGS)
{



































































}

/* generate_series_timestamptz()
 * Generate the set of timestamps from start to finish by step
 */
Datum
generate_series_timestamptz(PG_FUNCTION_ARGS)
{



































































}