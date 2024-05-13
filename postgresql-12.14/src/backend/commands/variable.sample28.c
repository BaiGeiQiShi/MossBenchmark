/*-------------------------------------------------------------------------
 *
 * variable.c
 *		Routines for handling specialized SET variables.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/variable.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_authid.h"
#include "commands/variable.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"
#include "utils/varlena.h"
#include "mb/pg_wchar.h"

/*
 * DATESTYLE
 */

/*
 * check_datestyle: GUC check_hook for datestyle
 */
bool
check_datestyle(char **newval, void **extra, GucSource source)
{
  int newDateStyle = DateStyle;
  int newDateOrder = DateOrder;
  bool have_style = false;
  bool have_order = false;
  bool ok = true;
  char *rawstring;
  int *myextra;
  char *result;
  List *elemlist;
  ListCell *l;

  /* Need a modifiable copy of string */
  rawstring = pstrdup(*newval);

  /* Parse string into list of identifiers */
  if (!SplitIdentifierString(rawstring, ',', &elemlist))
  {
    /* syntax error in list */




  }

  foreach (l, elemlist)
  {
    char *tok = (char *)lfirst(l);

    /* Ugh. Somebody ought to write a table driven version -- mjl */

    if (pg_strcasecmp(tok, "ISO") == 0)
    {
      if (have_style && newDateStyle != USE_ISO_DATES)
      {

      }
      newDateStyle = USE_ISO_DATES;
      have_style = true;
    }
    else if (pg_strcasecmp(tok, "SQL") == 0)
    {
      if (have_style && newDateStyle != USE_SQL_DATES)
      {

      }
      newDateStyle = USE_SQL_DATES;
      have_style = true;
    }
    else if (pg_strncasecmp(tok, "POSTGRES", 8) == 0)
    {
      if (have_style && newDateStyle != USE_POSTGRES_DATES)
      {

      }
      newDateStyle = USE_POSTGRES_DATES;
      have_style = true;
    }
    else if (pg_strcasecmp(tok, "GERMAN") == 0)
    {
      if (have_style && newDateStyle != USE_GERMAN_DATES)
      {

      }
      newDateStyle = USE_GERMAN_DATES;
      have_style = true;
      /* GERMAN also sets DMY, unless explicitly overridden */
      if (!have_order)
      {
        newDateOrder = DATEORDER_DMY;
      }
    }
    else if (pg_strcasecmp(tok, "YMD") == 0)
    {
      if (have_order && newDateOrder != DATEORDER_YMD)
      {

      }
      newDateOrder = DATEORDER_YMD;
      have_order = true;
    }
    else if (pg_strcasecmp(tok, "DMY") == 0 || pg_strncasecmp(tok, "EURO", 4) == 0)
    {
      if (have_order && newDateOrder != DATEORDER_DMY)
      {

      }
      newDateOrder = DATEORDER_DMY;
      have_order = true;
    }
    else if (pg_strcasecmp(tok, "MDY") == 0 || pg_strcasecmp(tok, "US") == 0 || pg_strncasecmp(tok, "NONEURO", 7) == 0)
    {
      if (have_order && newDateOrder != DATEORDER_MDY)
      {

      }
      newDateOrder = DATEORDER_MDY;
      have_order = true;
    }











































  }

  pfree(rawstring);
  list_free(elemlist);

  if (!ok)
  {


  }

  /*
   * Prepare the canonical string to return.  GUC wants it malloc'd.
   */
  result = (char *)malloc(32);
  if (!result)
  {

  }

  switch (newDateStyle)
  {
  case USE_ISO_DATES:;;
    strcpy(result, "ISO");
    break;
  case USE_SQL_DATES:;;
    strcpy(result, "SQL");
    break;
  case USE_GERMAN_DATES:;;
    strcpy(result, "German");
    break;
  default:;;;
    strcpy(result, "Postgres");
    break;
  }
  switch (newDateOrder)
  {
  case DATEORDER_YMD:;;
    strcat(result, ", YMD");
    break;
  case DATEORDER_DMY:;;
    strcat(result, ", DMY");
    break;
  default:;;;
    strcat(result, ", MDY");
    break;
  }

  free(*newval);
  *newval = result;

  /*
   * Set up the "extra" struct actually used by assign_datestyle.
   */
  myextra = (int *)malloc(2 * sizeof(int));
  if (!myextra)
  {

  }
  myextra[0] = newDateStyle;
  myextra[1] = newDateOrder;
  *extra = (void *)myextra;

  return true;
}

/*
 * assign_datestyle: GUC assign_hook for datestyle
 */
void
assign_datestyle(const char *newval, void *extra)
{
  int *myextra = (int *)extra;

  DateStyle = myextra[0];
  DateOrder = myextra[1];
}

/*
 * TIMEZONE
 */

/*
 * check_timezone: GUC check_hook for timezone
 */
bool
check_timezone(char **newval, void **extra, GucSource source)
{
  pg_tz *new_tz;
  long gmtoffset;
  char *endptr;
  double hours;

  if (pg_strncasecmp(*newval, "interval", 8) == 0)
  {
    /*
     * Support INTERVAL 'foo'.  This is for SQL spec compliance, not
     * because it has any actual real-world usefulness.
     */
    const char *valueptr = *newval;
    char *val;
    Interval *interval;











    /* Check and remove trailing quote */








    /*
     * Try to parse it.  XXX an invalid interval format will result in
     * ereport(ERROR), which is not desirable for GUC.  We did what we
     * could to guard against this in flatten_set_variable_args, but a
     * string coming in from postgresql.conf might contain anything.
     */
















    /* Here we change from SQL to Unix sign convention */




  }
  else
  {
    /*
     * Try it as a numeric number of hours (possibly fractional).
     */
    hours = strtod(*newval, &endptr);
    if (endptr != *newval && *endptr == '\0')
    {
      /* Here we change from SQL to Unix sign convention */
      gmtoffset = -hours * SECS_PER_HOUR;
      new_tz = pg_tzset_offset(gmtoffset);
    }
    else
    {
      /*
       * Otherwise assume it is a timezone name, and try to load it.
       */
      new_tz = pg_tzset(*newval);

      if (!new_tz)
      {
        /* Doesn't seem to be any great value in errdetail here */

      }

      if (!pg_tz_acceptable(new_tz))
      {



      }
    }
  }

  /* Test for failure in pg_tzset_offset, which we assume is out-of-range */
  if (!new_tz)
  {


  }

  /*
   * Pass back data for assign_timezone to use
   */
  *extra = malloc(sizeof(pg_tz *));
  if (!*extra)
  {

  }
  *((pg_tz **)*extra) = new_tz;

  return true;
}

/*
 * assign_timezone: GUC assign_hook for timezone
 */
void
assign_timezone(const char *newval, void *extra)
{
  session_timezone = *((pg_tz **)extra);
}

/*
 * show_timezone: GUC show_hook for timezone
 */
const char *
show_timezone(void)
{
  const char *tzn;

  /* Always show the zone's canonical name */
  tzn = pg_get_timezone_name(session_timezone);

  if (tzn != NULL)
  {
    return tzn;
  }


}

/*
 * LOG_TIMEZONE
 *
 * For log_timezone, we don't support the interval-based methods of setting a
 * zone, which are only there for SQL spec compliance not because they're
 * actually useful.
 */

/*
 * check_log_timezone: GUC check_hook for log_timezone
 */
bool
check_log_timezone(char **newval, void **extra, GucSource source)
{
  pg_tz *new_tz;

  /*
   * Assume it is a timezone name, and try to load it.
   */
  new_tz = pg_tzset(*newval);

  if (!new_tz)
  {
    /* Doesn't seem to be any great value in errdetail here */

  }

  if (!pg_tz_acceptable(new_tz))
  {



  }

  /*
   * Pass back data for assign_log_timezone to use
   */
  *extra = malloc(sizeof(pg_tz *));
  if (!*extra)
  {

  }
  *((pg_tz **)*extra) = new_tz;

  return true;
}

/*
 * assign_log_timezone: GUC assign_hook for log_timezone
 */
void
assign_log_timezone(const char *newval, void *extra)
{
  log_timezone = *((pg_tz **)extra);
}

/*
 * show_log_timezone: GUC show_hook for log_timezone
 */
const char *
show_log_timezone(void)
{
  const char *tzn;

  /* Always show the zone's canonical name */
  tzn = pg_get_timezone_name(log_timezone);

  if (tzn != NULL)
  {
    return tzn;
  }


}

/*
 * SET TRANSACTION READ ONLY and SET TRANSACTION READ WRITE
 *
 * We allow idempotent changes (r/w -> r/w and r/o -> r/o) at any time, and
 * we also always allow changes from read-write to read-only.  However,
 * read-only may be changed to read-write only when in a top-level transaction
 * that has not yet taken an initial snapshot.  Can't do it in a hot standby,
 * either.
 *
 * If we are not in a transaction at all, just allow the change; it means
 * nothing since XactReadOnly will be reset by the next StartTransaction().
 * The IsTransactionState() test protects us against trying to check
 * RecoveryInProgress() in contexts where shared memory is not accessible.
 * (Similarly, if we're restoring state in a parallel worker, just allow
 * the change.)
 */
bool
check_transaction_read_only(bool *newval, void **extra, GucSource source)
{
  if (*newval == false && XactReadOnly && IsTransactionState() && !InitializingParallelWorker)
  {
    /* Can't go to r/w mode inside a r/o transaction */
    if (IsSubTransaction())
    {
      GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
      GUC_check_errmsg("cannot set transaction read-write mode inside a read-only transaction");
      return false;
    }
    /* Top level transaction can't change to r/w after first snapshot. */
    if (FirstSnapshotSet)
    {
      GUC_check_errcode(ERRCODE_ACTIVE_SQL_TRANSACTION);
      GUC_check_errmsg("transaction read-write mode must be set before any query");
      return false;
    }
    /* Can't go to r/w mode while recovery is still active */
    if (RecoveryInProgress())
    {



    }
  }

  return true;
}

/*
 * SET TRANSACTION ISOLATION LEVEL
 *
 * We allow idempotent changes at any time, but otherwise this can only be
 * changed in a toplevel transaction that has not yet taken a snapshot.
 *
 * As in check_transaction_read_only, allow it if not inside a transaction.
 */
bool
check_XactIsoLevel(int *newval, void **extra, GucSource source)
{
  int newXactIsoLevel = *newval;

  if (newXactIsoLevel != XactIsoLevel && IsTransactionState())
  {
    if (FirstSnapshotSet)
    {



    }
    /* We ignore a subtransaction setting it to the existing value. */
    if (IsSubTransaction())
    {



    }
    /* Can't go to serializable mode while recovery is still active */
    if (newXactIsoLevel == XACT_SERIALIZABLE && RecoveryInProgress())
    {




    }
  }

  return true;
}

/*
 * SET TRANSACTION [NOT] DEFERRABLE
 */

bool
check_transaction_deferrable(bool *newval, void **extra, GucSource source)
{
  if (IsSubTransaction())
  {



  }
  if (FirstSnapshotSet)
  {



  }

  return true;
}

/*
 * Random number seed
 *
 * We can't roll back the random sequence on error, and we don't want
 * config file reloads to affect it, so we only want interactive SET SEED
 * commands to set it.  We use the "extra" storage to ensure that rollbacks
 * don't try to do the operation again.
 */

bool
check_random_seed(double *newval, void **extra, GucSource source)
{
  *extra = malloc(sizeof(int));
  if (!*extra)
  {

  }
  /* Arm the assign only if source of value is an interactive SET */
  *((int *)*extra) = (source >= PGC_S_INTERACTIVE);

  return true;
}

void
assign_random_seed(double newval, void *extra)
{
  /* We'll do this at most once for any setting of the GUC variable */
  if (*((int *)extra))
  {

  }
  *((int *)extra) = 0;
}

const char *
show_random_seed(void)
{
  return "unavailable";
}

/*
 * SET CLIENT_ENCODING
 */

bool
check_client_encoding(char **newval, void **extra, GucSource source)
{
  int encoding;
  const char *canonical_name;

  /* Look up the encoding by name */
  encoding = pg_valid_client_encoding(*newval);
  if (encoding < 0)
  {

  }

  /* Get the canonical name (no aliases, uniform case) */
  canonical_name = pg_encoding_to_char(encoding);

  /*
   * If we are not within a transaction then PrepareClientEncoding will not
   * be able to look up the necessary conversion procs.  If we are still
   * starting up, it will return "OK" anyway, and InitializeClientEncoding
   * will fix things once initialization is far enough along.  After
   * startup, we'll fail.  This would only happen if someone tries to change
   * client_encoding in postgresql.conf and then SIGHUP existing sessions.
   * It seems like a bad idea for client_encoding to change that way anyhow,
   * so we don't go out of our way to support it.
   *
   * Note: in the postmaster, or any other process that never calls
   * InitializeClientEncoding, PrepareClientEncoding will always succeed,
   * and so will SetClientEncoding; but they won't do anything, which is OK.
   */
  if (PrepareClientEncoding(encoding) < 0)
  {












  }

  /*
   * Replace the user-supplied string with the encoding's canonical name.
   * This gets rid of aliases and case-folding variations.
   *
   * XXX Although canonicalizing seems like a good idea in the abstract, it
   * breaks pre-9.1 JDBC drivers, which expect that if they send "UNICODE"
   * as the client_encoding setting then it will read back the same way. As
   * a workaround, don't replace the string if it's "UNICODE".  Remove that
   * hack when pre-9.1 JDBC drivers are no longer in use.
   */
  if (strcmp(*newval, canonical_name) != 0 && strcmp(*newval, "UNICODE") != 0)
  {
    free(*newval);
    *newval = strdup(canonical_name);
    if (!*newval)
    {

    }
  }

  /*
   * Save the encoding's ID in *extra, for use by assign_client_encoding.
   */
  *extra = malloc(sizeof(int));
  if (!*extra)
  {

  }
  *((int *)*extra) = encoding;

  return true;
}

void
assign_client_encoding(const char *newval, void *extra)
{
  int encoding = *((int *)extra);

  /*
   * Parallel workers send data to the leader, not the client.  They always
   * send data using the database encoding.
   */
  if (IsParallelWorker())
  {
    /*
     * During parallel worker startup, we want to accept the leader's
     * client_encoding setting so that anyone who looks at the value in
     * the worker sees the same value that they would see in the leader.
     */
    if (InitializingParallelWorker)
    {
      return;
    }

    /*
     * A change other than during startup, for example due to a SET clause
     * attached to a function definition, should be rejected, as there is
     * nothing we can do inside the worker to make it take effect.
     */

  }

  /* We do not expect an error if PrepareClientEncoding succeeded */
  if (SetClientEncoding(encoding) < 0)
  {

  }
}

/*
 * SET SESSION AUTHORIZATION
 */

typedef struct
{
  /* This is the "extra" state for both SESSION AUTHORIZATION and ROLE */
  Oid roleid;
  bool is_superuser;
} role_auth_extra;

bool
check_session_authorization(char **newval, void **extra, GucSource source)
{
  HeapTuple roleTup;
  Form_pg_authid roleform;
  Oid roleid;
  bool is_superuser;
  role_auth_extra *myextra;

  /* Do nothing for the boot_val default of NULL */
  if (*newval == NULL)
  {
    return true;
  }

  if (!IsTransactionState())
  {
    /*
     * Can't do catalog lookups, so fail.  The result of this is that
     * session_authorization cannot be set in postgresql.conf, which seems
     * like a good thing anyway, so we don't work hard to avoid it.
     */

  }

  /* Look up the username */
  roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(*newval));
  if (!HeapTupleIsValid(roleTup))
  {
    /*
     * When source == PGC_S_TEST, we don't throw a hard error for a
     * nonexistent user name, only a NOTICE.  See comments in guc.h.
     */







  }

  roleform = (Form_pg_authid)GETSTRUCT(roleTup);
  roleid = roleform->oid;
  is_superuser = roleform->rolsuper;

  ReleaseSysCache(roleTup);

  /* Set up "extra" struct for assign_session_authorization to use */
  myextra = (role_auth_extra *)malloc(sizeof(role_auth_extra));
  if (!myextra)
  {

  }
  myextra->roleid = roleid;
  myextra->is_superuser = is_superuser;
  *extra = (void *)myextra;

  return true;
}

void
assign_session_authorization(const char *newval, void *extra)
{
  role_auth_extra *myextra = (role_auth_extra *)extra;

  /* Do nothing for the boot_val default of NULL */
  if (!myextra)
  {
    return;
  }

  SetSessionAuthorization(myextra->roleid, myextra->is_superuser);
}

/*
 * SET ROLE
 *
 * The SQL spec requires "SET ROLE NONE" to unset the role, so we hardwire
 * a translation of "none" to InvalidOid.  Otherwise this is much like
 * SET SESSION AUTHORIZATION.
 */
extern char *role_string; /* in guc.c */

bool
check_role(char **newval, void **extra, GucSource source)
{
  HeapTuple roleTup;
  Oid roleid;
  bool is_superuser;
  role_auth_extra *myextra;
  Form_pg_authid roleform;

  if (strcmp(*newval, "none") == 0)
  {
    /* hardwired translation */
    roleid = InvalidOid;
    is_superuser = false;
  }
  else
  {
    if (!IsTransactionState())
    {
      /*
       * Can't do catalog lookups, so fail.  The result of this is that
       * role cannot be set in postgresql.conf, which seems like a good
       * thing anyway, so we don't work hard to avoid it.
       */

    }

    /*
     * When source == PGC_S_TEST, we don't throw a hard error for a
     * nonexistent user name or insufficient privileges, only a NOTICE.
     * See comments in guc.h.
     */

    /* Look up the username */
    roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(*newval));
    if (!HeapTupleIsValid(roleTup))
    {







    }

    roleform = (Form_pg_authid)GETSTRUCT(roleTup);
    roleid = roleform->oid;
    is_superuser = roleform->rolsuper;

    ReleaseSysCache(roleTup);

    /*
     * Verify that session user is allowed to become this role, but skip
     * this in parallel mode, where we must blindly recreate the parallel
     * leader's state.
     */
    if (!InitializingParallelWorker && !is_member_of_role(GetSessionUserId(), roleid))
    {








    }
  }

  /* Set up "extra" struct for assign_role to use */
  myextra = (role_auth_extra *)malloc(sizeof(role_auth_extra));
  if (!myextra)
  {

  }
  myextra->roleid = roleid;
  myextra->is_superuser = is_superuser;
  *extra = (void *)myextra;

  return true;
}

void
assign_role(const char *newval, void *extra)
{
  role_auth_extra *myextra = (role_auth_extra *)extra;

  SetCurrentRoleId(myextra->roleid, myextra->is_superuser);
}

const char *
show_role(void)
{
  /*
   * Check whether SET ROLE is active; if not return "none".  This is a
   * kluge to deal with the fact that SET SESSION AUTHORIZATION logically
   * resets SET ROLE to NONE, but we cannot set the GUC role variable from
   * assign_session_authorization (because we haven't got enough info to
   * call set_config_option).
   */
  if (!OidIsValid(GetCurrentRoleId()))
  {
    return "none";
  }

  /* Otherwise we can just use the GUC string */

}