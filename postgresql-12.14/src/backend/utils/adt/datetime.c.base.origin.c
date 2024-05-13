/*-------------------------------------------------------------------------
 *
 * datetime.c
 *	  Support functions for date/time types.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/datetime.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "common/string.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/memutils.h"
#include "utils/tzparser.h"

static int
DecodeNumber(int flen, char *field, bool haveTextMonth, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits);
static int
DecodeNumberField(int len, char *str, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits);
static int
DecodeTime(char *str, int fmask, int range, int *tmask, struct pg_tm *tm, fsec_t *fsec);
static const datetkn *
datebsearch(const char *key, const datetkn *base, int nel);
static int
DecodeDate(char *str, int fmask, int *tmask, bool *is2digits, struct pg_tm *tm);
static char *
AppendSeconds(char *cp, int sec, fsec_t fsec, int precision, bool fillzeros);
static void
AdjustFractSeconds(double frac, struct pg_tm *tm, fsec_t *fsec, int scale);
static void
AdjustFractDays(double frac, struct pg_tm *tm, fsec_t *fsec, int scale);
static int
DetermineTimeZoneOffsetInternal(struct pg_tm *tm, pg_tz *tzp, pg_time_t *tp);
static bool
DetermineTimeZoneAbbrevOffsetInternal(pg_time_t t, const char *abbr, pg_tz *tzp, int *offset, int *isdst);
static pg_tz *
FetchDynamicTimeZone(TimeZoneAbbrevTable *tbl, const datetkn *tp);

const int day_tab[2][13] = {{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0}, {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0}};

const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

const char *const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", NULL};

/*****************************************************************************
 *	 PRIVATE ROUTINES
 **
 *****************************************************************************/

/*
 * datetktbl holds date/time keywords.
 *
 * Note that this table must be strictly alphabetically ordered to allow an
 * O(ln(N)) search algorithm to be used.
 *
 * The token field must be NUL-terminated; we truncate entries to TOKMAXLEN
 * characters to fit.
 *
 * The static table contains no TZ, DTZ, or DYNTZ entries; rather those
 * are loaded from configuration files and stored in zoneabbrevtbl, whose
 * abbrevs[] field has the same format as the static datetktbl.
 */
static const datetkn datetktbl[] = {
    /* token, type, value */
    {EARLY, RESERV, DTK_EARLY},                                                                                                                                                                                                          /* "-infinity" reserved for "early time" */
    {DA_D, ADBC, AD},                                                                                                                                                                                                                    /* "ad" for years > 0 */
    {"allballs", RESERV, DTK_ZULU},                                                                                                                                                                                                      /* 00:00:00 */
    {"am", AMPM, AM}, {"apr", MONTH, 4}, {"april", MONTH, 4}, {"at", IGNORE_DTF, 0},                                                                                                                                                     /* "at" (throwaway) */
    {"aug", MONTH, 8}, {"august", MONTH, 8}, {DB_C, ADBC, BC},                                                                                                                                                                           /* "bc" for years <= 0 */
    {"d", UNITS, DTK_DAY},                                                                                                                                                                                                               /* "day of month" for ISO input */
    {"dec", MONTH, 12}, {"december", MONTH, 12}, {"dow", UNITS, DTK_DOW},                                                                                                                                                                /* day of week */
    {"doy", UNITS, DTK_DOY},                                                                                                                                                                                                             /* day of year */
    {"dst", DTZMOD, SECS_PER_HOUR}, {EPOCH, RESERV, DTK_EPOCH},                                                                                                                                                                          /* "epoch" reserved for system epoch time */
    {"feb", MONTH, 2}, {"february", MONTH, 2}, {"fri", DOW, 5}, {"friday", DOW, 5}, {"h", UNITS, DTK_HOUR},                                                                                                                              /* "hour" */
    {LATE, RESERV, DTK_LATE},                                                                                                                                                                                                            /* "infinity" reserved for "late time" */
    {"isodow", UNITS, DTK_ISODOW},                                                                                                                                                                                                       /* ISO day of week, Sunday == 7 */
    {"isoyear", UNITS, DTK_ISOYEAR},                                                                                                                                                                                                     /* year in terms of the ISO week date */
    {"j", UNITS, DTK_JULIAN}, {"jan", MONTH, 1}, {"january", MONTH, 1}, {"jd", UNITS, DTK_JULIAN}, {"jul", MONTH, 7}, {"julian", UNITS, DTK_JULIAN}, {"july", MONTH, 7}, {"jun", MONTH, 6}, {"june", MONTH, 6}, {"m", UNITS, DTK_MONTH}, /* "month" for ISO input */
    {"mar", MONTH, 3}, {"march", MONTH, 3}, {"may", MONTH, 5}, {"mm", UNITS, DTK_MINUTE},                                                                                                                                                /* "minute" for ISO input */
    {"mon", DOW, 1}, {"monday", DOW, 1}, {"nov", MONTH, 11}, {"november", MONTH, 11}, {NOW, RESERV, DTK_NOW},                                                                                                                            /* current transaction time */
    {"oct", MONTH, 10}, {"october", MONTH, 10}, {"on", IGNORE_DTF, 0},                                                                                                                                                                   /* "on" (throwaway) */
    {"pm", AMPM, PM}, {"s", UNITS, DTK_SECOND},                                                                                                                                                                                          /* "seconds" for ISO input */
    {"sat", DOW, 6}, {"saturday", DOW, 6}, {"sep", MONTH, 9}, {"sept", MONTH, 9}, {"september", MONTH, 9}, {"sun", DOW, 0}, {"sunday", DOW, 0}, {"t", ISOTIME, DTK_TIME},                                                                /* Filler for ISO time fields */
    {"thu", DOW, 4}, {"thur", DOW, 4}, {"thurs", DOW, 4}, {"thursday", DOW, 4}, {TODAY, RESERV, DTK_TODAY},                                                                                                                              /* midnight */
    {TOMORROW, RESERV, DTK_TOMORROW},                                                                                                                                                                                                    /* tomorrow midnight */
    {"tue", DOW, 2}, {"tues", DOW, 2}, {"tuesday", DOW, 2}, {"wed", DOW, 3}, {"wednesday", DOW, 3}, {"weds", DOW, 3}, {"y", UNITS, DTK_YEAR},                                                                                            /* "year" for ISO input */
    {YESTERDAY, RESERV, DTK_YESTERDAY}                                                                                                                                                                                                   /* yesterday midnight */
};

static const int szdatetktbl = sizeof datetktbl / sizeof datetktbl[0];

/*
 * deltatktbl: same format as datetktbl, but holds keywords used to represent
 * time units (eg, for intervals, and for EXTRACT).
 */
static const datetkn deltatktbl[] = {
    /* token, type, value */
    {"@", IGNORE_DTF, 0},                                                                                                                                                                                                       /* postgres relative prefix */
    {DAGO, AGO, 0},                                                                                                                                                                                                             /* "ago" indicates negative time offset */
    {"c", UNITS, DTK_CENTURY},                                                                                                                                                                                                  /* "century" relative */
    {"cent", UNITS, DTK_CENTURY},                                                                                                                                                                                               /* "century" relative */
    {"centuries", UNITS, DTK_CENTURY},                                                                                                                                                                                          /* "centuries" relative */
    {DCENTURY, UNITS, DTK_CENTURY},                                                                                                                                                                                             /* "century" relative */
    {"d", UNITS, DTK_DAY},                                                                                                                                                                                                      /* "day" relative */
    {DDAY, UNITS, DTK_DAY},                                                                                                                                                                                                     /* "day" relative */
    {"days", UNITS, DTK_DAY},                                                                                                                                                                                                   /* "days" relative */
    {"dec", UNITS, DTK_DECADE},                                                                                                                                                                                                 /* "decade" relative */
    {DDECADE, UNITS, DTK_DECADE},                                                                                                                                                                                               /* "decade" relative */
    {"decades", UNITS, DTK_DECADE},                                                                                                                                                                                             /* "decades" relative */
    {"decs", UNITS, DTK_DECADE},                                                                                                                                                                                                /* "decades" relative */
    {"h", UNITS, DTK_HOUR},                                                                                                                                                                                                     /* "hour" relative */
    {DHOUR, UNITS, DTK_HOUR},                                                                                                                                                                                                   /* "hour" relative */
    {"hours", UNITS, DTK_HOUR},                                                                                                                                                                                                 /* "hours" relative */
    {"hr", UNITS, DTK_HOUR},                                                                                                                                                                                                    /* "hour" relative */
    {"hrs", UNITS, DTK_HOUR},                                                                                                                                                                                                   /* "hours" relative */
    {"m", UNITS, DTK_MINUTE},                                                                                                                                                                                                   /* "minute" relative */
    {"microsecon", UNITS, DTK_MICROSEC},                                                                                                                                                                                        /* "microsecond" relative */
    {"mil", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                             /* "millennium" relative */
    {"millennia", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                       /* "millennia" relative */
    {DMILLENNIUM, UNITS, DTK_MILLENNIUM},                                                                                                                                                                                       /* "millennium" relative */
    {"millisecon", UNITS, DTK_MILLISEC},                                                                                                                                                                                        /* relative */
    {"mils", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                            /* "millennia" relative */
    {"min", UNITS, DTK_MINUTE},                                                                                                                                                                                                 /* "minute" relative */
    {"mins", UNITS, DTK_MINUTE},                                                                                                                                                                                                /* "minutes" relative */
    {DMINUTE, UNITS, DTK_MINUTE},                                                                                                                                                                                               /* "minute" relative */
    {"minutes", UNITS, DTK_MINUTE},                                                                                                                                                                                             /* "minutes" relative */
    {"mon", UNITS, DTK_MONTH},                                                                                                                                                                                                  /* "months" relative */
    {"mons", UNITS, DTK_MONTH},                                                                                                                                                                                                 /* "months" relative */
    {DMONTH, UNITS, DTK_MONTH},                                                                                                                                                                                                 /* "month" relative */
    {"months", UNITS, DTK_MONTH}, {"ms", UNITS, DTK_MILLISEC}, {"msec", UNITS, DTK_MILLISEC}, {DMILLISEC, UNITS, DTK_MILLISEC}, {"mseconds", UNITS, DTK_MILLISEC}, {"msecs", UNITS, DTK_MILLISEC}, {"qtr", UNITS, DTK_QUARTER}, /* "quarter" relative */
    {DQUARTER, UNITS, DTK_QUARTER},                                                                                                                                                                                             /* "quarter" relative */
    {"s", UNITS, DTK_SECOND}, {"sec", UNITS, DTK_SECOND}, {DSECOND, UNITS, DTK_SECOND}, {"seconds", UNITS, DTK_SECOND}, {"secs", UNITS, DTK_SECOND}, {DTIMEZONE, UNITS, DTK_TZ},                                                /* "timezone" time offset */
    {"timezone_h", UNITS, DTK_TZ_HOUR},                                                                                                                                                                                         /* timezone hour units */
    {"timezone_m", UNITS, DTK_TZ_MINUTE},                                                                                                                                                                                       /* timezone minutes units */
    {"us", UNITS, DTK_MICROSEC},                                                                                                                                                                                                /* "microsecond" relative */
    {"usec", UNITS, DTK_MICROSEC},                                                                                                                                                                                              /* "microsecond" relative */
    {DMICROSEC, UNITS, DTK_MICROSEC},                                                                                                                                                                                           /* "microsecond" relative */
    {"useconds", UNITS, DTK_MICROSEC},                                                                                                                                                                                          /* "microseconds" relative */
    {"usecs", UNITS, DTK_MICROSEC},                                                                                                                                                                                             /* "microseconds" relative */
    {"w", UNITS, DTK_WEEK},                                                                                                                                                                                                     /* "week" relative */
    {DWEEK, UNITS, DTK_WEEK},                                                                                                                                                                                                   /* "week" relative */
    {"weeks", UNITS, DTK_WEEK},                                                                                                                                                                                                 /* "weeks" relative */
    {"y", UNITS, DTK_YEAR},                                                                                                                                                                                                     /* "year" relative */
    {DYEAR, UNITS, DTK_YEAR},                                                                                                                                                                                                   /* "year" relative */
    {"years", UNITS, DTK_YEAR},                                                                                                                                                                                                 /* "years" relative */
    {"yr", UNITS, DTK_YEAR},                                                                                                                                                                                                    /* "year" relative */
    {"yrs", UNITS, DTK_YEAR}                                                                                                                                                                                                    /* "years" relative */
};

static const int szdeltatktbl = sizeof deltatktbl / sizeof deltatktbl[0];

static TimeZoneAbbrevTable *zoneabbrevtbl = NULL;

/* Caches of recent lookup results in the above tables */

static const datetkn *datecache[MAXDATEFIELDS] = {NULL};

static const datetkn *deltacache[MAXDATEFIELDS] = {NULL};

static const datetkn *abbrevcache[MAXDATEFIELDS] = {NULL};

/*
 * Calendar time to Julian date conversions.
 * Julian date is commonly used in astronomical applications,
 *	since it is numerically accurate and computationally simple.
 * The algorithms here will accurately convert between Julian day
 *	and calendar date for all non-negative Julian days
 *	(i.e. from Nov 24, -4713 on).
 *
 * Rewritten to eliminate overflow problems. This now allows the
 * routines to work correctly for all Julian day counts from
 * 0 to 2147483647	(Nov 24, -4713 to Jun 3, 5874898) assuming
 * a 32-bit integer. Longer types should also work to the limits
 * of their precision.
 *
 * Actually, date2j() will work sanely, in the sense of producing
 * valid negative Julian dates, significantly before Nov 24, -4713.
 * We rely on it to do so back to Nov 1, -4713; see IS_VALID_JULIAN()
 * and associated commentary in timestamp.h.
 */

int
date2j(int y, int m, int d)
{




















} /* date2j() */

void
j2date(int jd, int *year, int *month, int *day)
{





















} /* j2date() */

/*
 * j2day - convert Julian date to day-of-week (0..6 == Sun..Sat)
 *
 * Note: various places use the locution j2day(date - 1) to produce a
 * result according to the convention 0..6 = Mon..Sun.  This is a bit of
 * a crock, but will work as long as the computation here is just a modulo.
 */
int
j2day(int date)
{









} /* j2day() */

/*
 * GetCurrentDateTime()
 *
 * Get the transaction start time ("now()") broken down as a struct pg_tm.
 */
void
GetCurrentDateTime(struct pg_tm *tm)
{





}

/*
 * GetCurrentTimeUsec()
 *
 * Get the transaction start time ("now()") broken down as a struct pg_tm,
 * including fractional seconds and timezone offset.
 */
void
GetCurrentTimeUsec(struct pg_tm *tm, fsec_t *fsec, int *tzp)
{








}

/*
 * Append seconds and fractional seconds (if any) at *cp.
 *
 * precision is the max number of fraction digits, fillzeros says to
 * pad to two integral-seconds digits.
 *
 * Returns a pointer to the new end of string.  No NUL terminator is put
 * there; callers are responsible for NUL terminating str themselves.
 *
 * Note that any sign is stripped from the input seconds values.
 */
static char *
AppendSeconds(char *cp, int sec, fsec_t fsec, int precision, bool fillzeros)
{

































































}

/*
 * Variant of above that's specialized to timestamp case.
 *
 * Returns a pointer to the new end of string.  No NUL terminator is put
 * there; callers are responsible for NUL terminating str themselves.
 */
static char *
AppendTimestampSeconds(char *cp, struct pg_tm *tm, fsec_t fsec)
{

}

/*
 * Multiply frac by scale (to produce seconds) and add to *tm & *fsec.
 * We assume the input frac is less than 1 so overflow is not an issue.
 */
static void
AdjustFractSeconds(double frac, struct pg_tm *tm, fsec_t *fsec, int scale)
{











}

/* As above, but initial scale produces days */
static void
AdjustFractDays(double frac, struct pg_tm *tm, fsec_t *fsec, int scale)
{











}

/* Fetch a fractional-second value with suitable error checking */
static int
ParseFractionalSecond(char *cp, fsec_t *fsec)
{













}

/* ParseDateTime()
 *	Break string into tokens based on a date/time context.
 *	Returns 0 if successful, DTERR code if bogus input detected.
 *
 * timestr - the input string
 * workbuf - workspace for field string storage. This must be
 *	 larger than the largest legal input for this datetime type --
 *	 some additional space will be needed to NUL terminate fields.
 * buflen - the size of workbuf
 * field[] - pointers to field strings are returned in this array
 * ftype[] - field type indicators are returned in this array
 * maxfields - dimensions of the above two arrays
 * *numfields - set to the actual number of fields detected
 *
 * The fields extracted from the input are stored as separate,
 * null-terminated strings in the workspace at workbuf. Any text is
 * converted to lower case.
 *
 * Several field types are assigned:
 *	DTK_NUMBER - digits and (possibly) a decimal point
 *	DTK_DATE - digits and two delimiters, or digits and text
 *	DTK_TIME - digits, colon delimiters, and possibly a decimal point
 *	DTK_STRING - text (no digits or punctuation)
 *	DTK_SPECIAL - leading "+" or "-" followed by text
 *	DTK_TZ - leading "+" or "-" followed by digits (also eats ':', '.', '-')
 *
 * Note that some field types can hold unexpected items:
 *	DTK_NUMBER can hold date fields (yy.ddd)
 *	DTK_STRING can hold months (January) and time zones (PST)
 *	DTK_DATE can hold time zone names (America/New_York, GMT-8)
 */
int
ParseDateTime(const char *timestr, char *workbuf, size_t buflen, char **field, int *ftype, int maxfields, int *numfields)
{

























































































































































































































}

/* DecodeDateTime()
 * Interpret previously parsed fields for general date and time.
 * Return 0 if full date, 1 if only time, and negative DTERR code if problems.
 * (Currently, all callers treat 1 as an error return too.)
 *
 *		External format(s):
 *				"<weekday> <month>-<day>-<year>
 *<hour>:<minute>:<second>" "Fri Feb-7-1997 15:23:27" "Feb-7-1997 15:23:27"
 *				"2-7-1997 15:23:27"
 *				"1997-2-7 15:23:27"
 *				"1997.038 15:23:27"		(day of year
 *1-366) Also supports input in compact time: "970207 152327" "97038 152327"
 *				"20011225T040506.789-07"
 *
 * Use the system-provided functions to get the current time zone
 * if not specified in the input string.
 *
 * If the date is outside the range of pg_time_t (in practice that could only
 * happen if pg_time_t is just 32 bits), then assume UTC time zone - thomas
 * 1997-05-27
 */
int
DecodeDateTime(char **field, int *ftype, int nf, int *dtype, struct pg_tm *tm, fsec_t *fsec, int *tzp)
{













































































































































































































































































































































































































































































































































































































































































































































}

/* DetermineTimeZoneOffset()
 *
 * Given a struct pg_tm in which tm_year, tm_mon, tm_mday, tm_hour, tm_min,
 * and tm_sec fields are set, and a zic-style time zone definition, determine
 * the applicable GMT offset and daylight-savings status at that time.
 * Set the struct pg_tm's tm_isdst field accordingly, and return the GMT
 * offset as the function result.
 *
 * Note: if the date is out of the range we can deal with, we return zero
 * as the GMT offset and set tm_isdst = 0.  We don't throw an error here,
 * though probably some higher-level code will.
 */
int
DetermineTimeZoneOffset(struct pg_tm *tm, pg_tz *tzp)
{



}

/* DetermineTimeZoneOffsetInternal()
 *
 * As above, but also return the actual UTC time imputed to the date/time
 * into *tp.
 *
 * In event of an out-of-range date, we punt by returning zero into *tp.
 * This is okay for the immediate callers but is a good reason for not
 * exposing this worker function globally.
 *
 * Note: it might seem that we should use mktime() for this, but bitter
 * experience teaches otherwise.  This code is much faster than most versions
 * of mktime(), anyway.
 */
static int
DetermineTimeZoneOffsetInternal(struct pg_tm *tm, pg_tz *tzp, pg_time_t *tp)
{



















































































































}

/* DetermineTimeZoneAbbrevOffset()
 *
 * Determine the GMT offset and DST flag to be attributed to a dynamic
 * time zone abbreviation, that is one whose meaning has changed over time.
 * *tm contains the local time at which the meaning should be determined,
 * and tm->tm_isdst receives the DST flag.
 *
 * This differs from the behavior of DetermineTimeZoneOffset() in that a
 * standard-time or daylight-time abbreviation forces use of the corresponding
 * GMT offset even when the zone was then in DS or standard time respectively.
 * (However, that happens only if we can match the given abbreviation to some
 * abbreviation that appears in the IANA timezone data.  Otherwise, we fall
 * back to doing DetermineTimeZoneOffset().)
 */
int
DetermineTimeZoneAbbrevOffset(struct pg_tm *tm, const char *abbr, pg_tz *tzp)
{


























}

/* DetermineTimeZoneAbbrevOffsetTS()
 *
 * As above but the probe time is specified as a TimestampTz (hence, UTC time),
 * and DST status is returned into *isdst rather than into tm->tm_isdst.
 */
int
DetermineTimeZoneAbbrevOffsetTS(TimestampTz ts, const char *abbr, pg_tz *tzp, int *isdst)
{


























}

/* DetermineTimeZoneAbbrevOffsetInternal()
 *
 * Workhorse for above two functions: work from a pg_time_t probe instant.
 * On success, return GMT offset and DST status into *offset and *isdst.
 */
static bool
DetermineTimeZoneAbbrevOffsetInternal(pg_time_t t, const char *abbr, pg_tz *tzp, int *offset, int *isdst)
{



















}

/* DecodeTimeOnly()
 * Interpret parsed string as time fields only.
 * Returns 0 if successful, DTERR code if bogus input detected.
 *
 * Note that support for time zone is here for
 * SQL TIME WITH TIME ZONE, but it reveals
 * bogosity with SQL date/time standards, since
 * we must infer a time zone from current time.
 * - thomas 2000-03-10
 * Allow specifying date to get a better time zone,
 * if time zones are allowed. - thomas 2001-12-26
 */
int
DecodeTimeOnly(char **field, int *ftype, int nf, int *dtype, struct pg_tm *tm, fsec_t *fsec, int *tzp)
{






















































































































































































































































































































































































































































































































































































































































































}

/* DecodeDate()
 * Decode date string which includes delimiters.
 * Return 0 if okay, a DTERR code if not.
 *
 *	str: field to be parsed
 *	fmask: bitmask for field types already seen
 *	*tmask: receives bitmask for fields found here
 *	*is2digits: set to true if we find 2-digit year
 *	*tm: field values are stored into appropriate members of this struct
 */
static int
DecodeDate(char *str, int fmask, int *tmask, bool *is2digits, struct pg_tm *tm)
{























































































































}

/* ValidateDate()
 * Check valid year/month/day values, handle BC and DOY cases
 * Return 0 if okay, a DTERR code if not.
 */
int
ValidateDate(int fmask, bool isjulian, bool is2digits, bool bc, struct pg_tm *tm)
{
















































































}

/* DecodeTime()
 * Decode time string which includes delimiters.
 * Return 0 if okay, a DTERR code if not.
 *
 * Only check the lower limit on hours, since this same code can be
 * used to represent time spans.
 */
static int
DecodeTime(char *str, int fmask, int range, int *tmask, struct pg_tm *tm, fsec_t *fsec)
{


















































































}

/* DecodeNumber()
 * Interpret plain numeric field as a date value in context.
 * Return 0 if okay, a DTERR code if not.
 */
static int
DecodeNumber(int flen, char *str, bool haveTextMonth, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits)
{



















































































































































































}

/* DecodeNumberField()
 * Interpret numeric string as a concatenated date or time field.
 * Return a DTK token (>= 0) if successful, a DTERR code (< 0) if not.
 *
 * Use the context of previously decoded fields to help with
 * the interpretation.
 */
static int
DecodeNumberField(int len, char *str, int fmask, int *tmask, struct pg_tm *tm, fsec_t *fsec, bool *is2digits)
{















































































}

/* DecodeTimezone()
 * Interpret string as a numeric timezone.
 *
 * Return 0 if okay (and set *tzp), a DTERR code if not okay.
 */
int
DecodeTimezone(char *str, int *tzp)
{












































































}

/* DecodeTimezoneAbbrev()
 * Interpret string as a timezone abbreviation, if possible.
 *
 * Returns an abbreviation type (TZ, DTZ, or DYNTZ), or UNKNOWN_FIELD if
 * string is not any known abbreviation.  On success, set *offset and *tz to
 * represent the UTC offset (for TZ or DTZ) or underlying zone (for DYNTZ).
 * Note that full timezone names (such as America/New_York) are not handled
 * here, mostly for historical reasons.
 *
 * Given string must be lowercased already.
 *
 * Implement a cache lookup since it is likely that dates
 *	will be related in format.
 */
int
DecodeTimezoneAbbrev(int field, char *lowtoken, int *offset, pg_tz **tz)
{







































}

/* DecodeSpecial()
 * Decode text string using lookup table.
 *
 * Recognizes the keywords listed in datetktbl.
 * Note: at one time this would also recognize timezone abbreviations,
 * but no more; use DecodeTimezoneAbbrev for that.
 *
 * Given string must be lowercased already.
 *
 * Implement a cache lookup since it is likely that dates
 *	will be related in format.
 */
int
DecodeSpecial(int field, char *lowtoken, int *val)
{






















}

/* ClearPgTm
 *
 * Zero out a pg_tm and associated fsec_t
 */
static inline void
ClearPgTm(struct pg_tm *tm, fsec_t *fsec)
{







}

/* DecodeInterval()
 * Interpret previously parsed fields for general time interval.
 * Returns 0 if successful, DTERR code if bogus input detected.
 * dtype, tm, fsec are output parameters.
 *
 * Allow "date" field DTK_DATE since this could be just
 *	an unsigned floating point number. - thomas 1997-11-16
 *
 * Allow ISO-style time span, with implicit units on number of days
 *	preceding an hh:mm:ss field. - thomas 1998-04-30
 */
int
DecodeInterval(char **field, int *ftype, int nf, int range, int *dtype, struct pg_tm *tm, fsec_t *fsec)
{




















































































































































































































































































































































































































}

/*
 * Helper functions to avoid duplicated code in DecodeISO8601Interval.
 *
 * Parse a decimal value and break it into integer and fractional parts.
 * Returns 0 or DTERR code.
 */
static int
ParseISO8601Number(char *str, char **endptr, int *ipart, double *fpart)
{





























}

/*
 * Determine number of integral digits in a valid ISO 8601 number field
 * (we should ignore sign and any fraction part)
 */
static int
ISO8601IntegerWidth(char *fieldstart)
{






}

/* DecodeISO8601Interval()
 *	Decode an ISO 8601 time interval of the "format with designators"
 *	(section 4.4.3.2) or "alternative format" (section 4.4.3.3)
 *	Examples:  P1D	for 1 day
 *			   PT1H for 1 hour
 *			   P2Y6M7DT1H30M for 2 years, 6 months, 7 days 1 hour 30
 *min P0002-06-07T01:30:00 the same value in alternative format
 *
 * Returns 0 if successful, DTERR code if bogus input detected.
 * Note: error code should be DTERR_BAD_FORMAT if input doesn't look like
 * ISO8601, otherwise this could cause unexpected error messages.
 * dtype, tm, fsec are output parameters.
 *
 *	A couple exceptions from the spec:
 *	 - a week field ('W') may coexist with other units
 *	 - allows decimals in fields other than the least significant unit.
 */
int
DecodeISO8601Interval(char *str, int *dtype, struct pg_tm *tm, fsec_t *fsec)
{


































































































































































































































}

/* DecodeUnits()
 * Decode text string using lookup table.
 *
 * This routine recognizes keywords associated with time interval units.
 *
 * Given string must be lowercased already.
 *
 * Implement a cache lookup since it is likely that dates
 *	will be related in format.
 */
int
DecodeUnits(int field, char *lowtoken, int *val)
{






















} /* DecodeUnits() */

/*
 * Report an error detected by one of the datetime input processing routines.
 *
 * dterr is the error code, str is the original input string, datatype is
 * the name of the datatype we were trying to accept.
 *
 * Note: it might seem useless to distinguish DTERR_INTERVAL_OVERFLOW and
 * DTERR_TZDISP_OVERFLOW from DTERR_FIELD_OVERFLOW, but SQL99 mandates three
 * separate SQLSTATE codes, so ...
 */
void
DateTimeParseError(int dterr, const char *str, const char *datatype)
{




















}

/* datebsearch()
 * Binary search -- from Knuth (6.2.1) Algorithm B.  Special case like this
 * is WAY faster than the generic bsearch().
 */
static const datetkn *
datebsearch(const char *key, const datetkn *base, int nel)
{






























}

/* EncodeTimezone()
 *		Copies representation of a numeric timezone offset to str.
 *
 * Returns a pointer to the new end of string.  No NUL terminator is put
 * there; callers are responsible for NUL terminating str themselves.
 */
static char *
EncodeTimezone(char *str, int tz, int style)
{






























}

/* EncodeDateOnly()
 * Encode date as local time.
 */
void
EncodeDateOnly(struct pg_tm *tm, int style, char *str)
{



































































}

/* EncodeTimeOnly()
 * Encode time fields only.
 *
 * tm and fsec are the value to encode, print_tz determines whether to include
 * a time zone (the difference between time and timetz types), tz is the
 * numeric time zone offset, style is the date style, str is where to write the
 * output.
 */
void
EncodeTimeOnly(struct pg_tm *tm, fsec_t fsec, bool print_tz, int tz, int style, char *str)
{










}

/* EncodeDateTime()
 * Encode date and time interpreted as local time.
 *
 * tm and fsec are the value to encode, print_tz determines whether to include
 * a time zone (the difference between timestamp and timestamptz types), tz is
 * the numeric time zone offset, tzn is the textual time zone, which if
 * specified will be used instead of tz by some styles, style is the date
 * style, str is where to write the output.
 *
 * Supported date styles:
 *	Postgres - day mon hh:mm:ss yyyy tz
 *	SQL - mm/dd/yyyy hh:mm:ss.ss tz
 *	ISO - yyyy-mm-dd hh:mm:ss+/-tz
 *	German - dd.mm.yyyy hh:mm:ss tz
 *	XSD - yyyy-mm-ddThh:mm:ss.ss+/-tz
 */
void
EncodeDateTime(struct pg_tm *tm, fsec_t fsec, bool print_tz, int tz, const char *tzn, int style, char *str)
{



































































































































































}

/*
 * Helper functions to avoid duplicated code in EncodeInterval.
 */

/* Append an ISO-8601-style interval field, but only if value isn't zero */
static char *
AddISO8601IntPart(char *cp, int value, char units)
{






}

/* Append a postgres-style interval field, but only if value isn't zero */
static char *
AddPostgresIntPart(char *cp, int value, const char *units, bool *is_zero, bool *is_before)
{













}

/* Append a verbose-style interval field, but only if value isn't zero */
static char *
AddVerboseIntPart(char *cp, int value, const char *units, bool *is_zero, bool *is_before)
{

















}

/* EncodeInterval()
 * Interpret time structure as a delta time and convert to string.
 *
 * Support "traditional Postgres" and ISO-8601 styles.
 * Actually, afaik ISO does not address time interval formatting,
 *	but this looks similar to the spec for absolute date/time.
 * - thomas 1998-04-30
 *
 * Actually, afaik, ISO 8601 does specify formats for "time
 * intervals...[of the]...format with time-unit designators", which
 * are pretty ugly.  The format looks something like
 *	   P1Y1M1DT1H1M1.12345S
 * but useful for exchanging data with computers instead of humans.
 * - ron 2003-07-14
 *
 * And ISO's SQL 2008 standard specifies standards for
 * "year-month literal"s (that look like '2-3') and
 * "day-time literal"s (that look like ('4 5:6:7')
 */
void
EncodeInterval(struct pg_tm *tm, fsec_t fsec, int style, char *str)
{




















































































































































































}

/*
 * We've been burnt by stupid errors in the ordering of the datetkn tables
 * once too often.  Arrange to check them during postmaster start.
 */
static bool
CheckDateTokenTable(const char *tablename, const datetkn *base, int nel)
{





















}

bool
CheckDateTokenTables(void)
{








}

/*
 * Common code for temporal prosupport functions: simplify, if possible,
 * a call to a temporal type's length-coercion function.
 *
 * Types time, timetz, timestamp and timestamptz each have a range of allowed
 * precisions.  An unspecified precision is rigorously equivalent to the
 * highest specifiable precision.  We can replace the function call with a
 * no-op RelabelType if it is coercing to the same or higher precision as the
 * input is known to have.
 *
 * The input Node is always a FuncExpr, but to reduce the #include footprint
 * of datetime.h, we declare it as Node *.
 *
 * Note: timestamp_scale throws an error when the typmod is out of range, but
 * we can't get there from a cast: our typmodin will have caught it already.
 */
Node *
TemporalSimplify(int32 max_precis, Node *node)
{





















}

/*
 * This function gets called during timezone config file load or reload
 * to create the final array of timezone tokens.  The argument array
 * is already sorted in name order.
 *
 * The result is a TimeZoneAbbrevTable (which must be a single malloc'd chunk)
 * or NULL on malloc failure.  No other error conditions are defined.
 */
TimeZoneAbbrevTable *
ConvertTimeZoneAbbrevs(struct tzEntry *abbrevs, int n)
{








































































}

/*
 * Install a TimeZoneAbbrevTable as the active table.
 *
 * Caller is responsible that the passed table doesn't go away while in use.
 */
void
InstallTimeZoneAbbrevs(TimeZoneAbbrevTable *tbl)
{



}

/*
 * Helper subroutine to locate pg_tz timezone for a dynamic abbreviation.
 */
static pg_tz *
FetchDynamicTimeZone(TimeZoneAbbrevTable *tbl, const datetkn *tp)
{























}

/*
 * This set-returning function reads all the available time zone abbreviations
 * and returns a set of (abbrev, utc_offset, is_dst).
 */
Datum
pg_timezone_abbrevs(PG_FUNCTION_ARGS)
{




















































































































}

/*
 * This set-returning function reads all the available full time zones
 * and returns a set of (name, abbrev, utc_offset, is_dst).
 */
Datum
pg_timezone_names(PG_FUNCTION_ARGS)
{




























































































}