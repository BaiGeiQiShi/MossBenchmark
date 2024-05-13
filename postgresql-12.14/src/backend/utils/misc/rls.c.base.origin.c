/*-------------------------------------------------------------------------
 *
 * rls.c
 *		  RLS-related utility functions.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *		  src/backend/utils/misc/rls.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "access/transam.h"
#include "catalog/namespace.h"
#include "catalog/pg_class.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rls.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

/*
 * check_enable_rls
 *
 * Determine, based on the relation, row_security setting, and current role,
 * if RLS is applicable to this query.  RLS_NONE_ENV indicates that, while
 * RLS is not to be added for this query, a change in the environment may change
 * that.  RLS_NONE means that RLS is not on the relation at all and therefore
 * we don't need to worry about it.  RLS_ENABLED means RLS should be implemented
 * for the table and the plan cache needs to be invalidated if the environment
 * changes.
 *
 * Handle checking as another role via checkAsUser (for views, etc).  Pass
 * InvalidOid to check the current user.
 *
 * If noError is set to 'true' then we just return RLS_ENABLED instead of doing
 * an ereport() if the user has attempted to bypass RLS and they are not
 * allowed to.  This allows users to check if RLS is enabled without having to
 * deal with the actual error case (eg: error cases which are trying to decide
 * if the user should get data from the relation back as part of the error).
 */
int
check_enable_rls(Oid relid, Oid checkAsUser, bool noError)
{























































































}

/*
 * row_security_active
 *
 * check_enable_rls wrapped as a SQL callable function except
 * RLS_NONE_ENV and RLS_NONE are the same for this purpose.
 */
Datum
row_security_active(PG_FUNCTION_ARGS)
{






}

Datum
row_security_active_name(PG_FUNCTION_ARGS)
{












}