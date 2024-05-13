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













































































































































































































}

/*
 * assign_datestyle: GUC assign_hook for datestyle
 */
void
assign_datestyle(const char *newval, void *extra)
{




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


















































































































}

/*
 * assign_timezone: GUC assign_hook for timezone
 */
void
assign_timezone(const char *newval, void *extra)
{

}

/*
 * show_timezone: GUC show_hook for timezone
 */
const char *
show_timezone(void)
{











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































}

/*
 * assign_log_timezone: GUC assign_hook for log_timezone
 */
void
assign_log_timezone(const char *newval, void *extra)
{

}

/*
 * show_log_timezone: GUC show_hook for log_timezone
 */
const char *
show_log_timezone(void)
{











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




























}

/*
 * SET TRANSACTION [NOT] DEFERRABLE
 */

bool
check_transaction_deferrable(bool *newval, void **extra, GucSource source)
{














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









}

void
assign_random_seed(double newval, void *extra)
{






}

const char *
show_random_seed(void)
{

}

/*
 * SET CLIENT_ENCODING
 */

bool
check_client_encoding(char **newval, void **extra, GucSource source)
{










































































}

void
assign_client_encoding(const char *newval, void *extra)
{































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
























































}

void
assign_session_authorization(const char *newval, void *extra)
{









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














































































}

void
assign_role(const char *newval, void *extra)
{



}

const char *
show_role(void)
{














}