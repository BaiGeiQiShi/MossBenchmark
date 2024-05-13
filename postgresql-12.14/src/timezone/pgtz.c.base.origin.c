/*-------------------------------------------------------------------------
 *
 * pgtz.c
 *	  Timezone Library Integration Functions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/timezone/pgtz.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "datatype/timestamp.h"
#include "miscadmin.h"
#include "pgtz.h"
#include "storage/fd.h"
#include "utils/hsearch.h"

/* Current session timezone (controlled by TimeZone GUC) */
pg_tz *session_timezone = NULL;

/* Current log timezone (controlled by log_timezone GUC) */
pg_tz *log_timezone = NULL;

static bool
scan_directory_ci(const char *dirname, const char *fname, int fnamelen, char *canonname, int canonnamelen);

/*
 * Return full pathname of timezone data directory
 */
static const char *
pg_TZDIR(void)
{



















}

/*
 * Given a timezone name, open() the timezone data file.  Return the
 * file descriptor if successful, -1 if not.
 *
 * The input name is searched for case-insensitively (we assume that the
 * timezone database does not contain case-equivalent names).
 *
 * If "canonname" is not NULL, then on success the canonical spelling of the
 * given name is stored there (the buffer must be > TZ_STRLEN_MAX bytes!).
 */
int
pg_open_tzfile(const char *name, char *canonname)
{













































































}

/*
 * Scan specified directory for a case-insensitive match to fname
 * (of length fnamelen --- fname may not be null terminated!).  If found,* copy the actual filename into canonname and return true.
 */
static bool
scan_directory_ci(const char *dirname, const char *fname, int fnamelen, char *canonname, int canonnamelen)
{





























}

/*
 * We keep loaded timezones in a hashtable so we don't have to
 * load and parse the TZ definition file every time one is selected.
 * Because we want timezone names to be found case-insensitively,* the hash key is the uppercased name of the zone.
 */
typedef struct
{
  /* tznameupper contains the all-upper-case name of the timezone */
  char tznameupper[TZ_STRLEN_MAX + 1];
  pg_tz tz;
} pg_tz_cache;

static HTAB *timezone_cache = NULL;

static bool
init_timezone_hashtable(void)
{














}

/*
 * Load a timezone from file or from cache.
 * Does not verify that the timezone is acceptable!
 *
 * "GMT" is always interpreted as the tzparse() definition, without attempting
 * to load a definition from the filesystem.  This has a number of benefits:
 * 1. It's guaranteed to succeed, so we don't have the failure mode wherein
 * the bootstrap default timezone setting doesn't work (as could happen if
 * the OS attempts to supply a leap-second-aware version of "GMT").
 * 2. Because we aren't accessing the filesystem, we can safely initialize
 * the "GMT" zone definition before my_exec_path is known.
 * 3. It's quick enough that we don't waste much time when the bootstrap
 * default timezone setting is later overridden from postgresql.conf.
 */
pg_tz *
pg_tzset(const char *name)
{







































































}

/*
 * Load a fixed-GMT-offset timezone.
 * This is used for SQL-spec SET TIME ZONE INTERVAL 'foo' cases.
 * It's otherwise equivalent to pg_tzset().
 *
 * The GMT offset is specified in seconds, positive values meaning west of
 * Greenwich (ie, POSIX not ISO sign convention).  However, we use ISO
 * sign convention in the displayable abbreviation for the zone.
 *
 * Caution: this can fail (return NULL) if the specified offset is outside
 * the range allowed by the zic library.
 */
pg_tz *
pg_tzset_offset(long gmtoffset)
{

























}

/*
 * Initialize timezone library
 *
 * This is called before GUC variable initialization begins.  Its purpose
 * is to ensure that log_timezone has a valid value before any logging GUC
 * variables could become set to values that require elog.c to provide
 * timestamps (e.g., log_line_prefix).  We may as well initialize
 * session_timezone to something valid, too.
 */
void
pg_timezone_initialize(void)
{









}

/*
 * Functions to enumerate available timezones
 *
 * Note that pg_tzenumerate_next() will return a pointer into the pg_tzenum
 * structure, so the data is only valid up to the next call.
 *
 * All data is allocated using palloc in the current context.
 */
#define MAX_TZDIR_DEPTH 10

struct pg_tzenum
{
  int baselen;
  int depth;
  DIR *dirdesc[MAX_TZDIR_DEPTH];
  char *dirname[MAX_TZDIR_DEPTH];
  struct pg_tz tz;
};

/* typedef pg_tzenum is declared in pgtime.h */

pg_tzenum *
pg_tzenumerate_start(void)
{












}

void
pg_tzenumerate_end(pg_tzenum *dir)
{







}

pg_tz *
pg_tzenumerate_next(pg_tzenum *dir)
{










































































}