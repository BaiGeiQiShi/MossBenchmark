/*-------------------------------------------------------------------------
 *
 * pg_subscription.c
 *		replication subscriptions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/backend/catalog/pg_subscription.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"

#include "catalog/indexing.h"
#include "catalog/pg_type.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"

#include "nodes/makefuncs.h"

#include "storage/lmgr.h"

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_lsn.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static List *
textarray_to_stringlist(ArrayType *textarray);

/*
 * Fetch the subscription from the syscache.
 */
Subscription *
GetSubscription(Oid subid, bool missing_ok)
{
























































}

/*
 * Return number of subscriptions defined in given database.
 * Used by dropdb() to check if database can indeed be dropped.
 */
int
CountDBSubscriptions(Oid dbid)
{






















}

/*
 * Free memory allocated by subscription struct.
 */
void
FreeSubscription(Subscription *sub)
{








}

/*
 * get_subscription_oid - given a subscription name, look up the OID
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return InvalidOid.
 */
Oid
get_subscription_oid(const char *subname, bool missing_ok)
{








}

/*
 * get_subscription_name - given a subscription OID, look up the name
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return NULL.
 */
char *
get_subscription_name(Oid subid, bool missing_ok)
{





















}

/*
 * Convert text array to list of strings.
 *
 * Note: the resulting list of strings is pallocated here.
 */
static List *
textarray_to_stringlist(ArrayType *textarray)
{

















}

/*
 * Add new state record for a subscription table.
 */
void
AddSubscriptionRelState(Oid subid, Oid relid, char state, XLogRecPtr sublsn)
{








































}

/*
 * Update the state of a subscription table.
 */
void
UpdateSubscriptionRelState(Oid subid, Oid relid, char state, XLogRecPtr sublsn)
{










































}

/*
 * Get state of subscription table.
 *
 * Returns SUBREL_STATE_UNKNOWN when not found and missing_ok is true.
 */
char
GetSubscriptionRelState(Oid subid, Oid relid, XLogRecPtr *sublsn, bool missing_ok)
{










































}

/*
 * Drop subscription relation mapping. These can be for a particular
 * subscription, or for a particular relation, or both.
 */
void
RemoveSubscriptionRel(Oid subid, Oid relid)
{



























}

/*
 * Get all relations for subscription.
 *
 * Returned list is palloc'ed in current memory context.
 */
List *
GetSubscriptionRelations(Oid subid)
{











































}

/*
 * Get all relations for subscription that are not in a ready state.
 *
 * Returned list is palloc'ed in current memory context.
 */
List *
GetSubscriptionNotReadyRelations(Oid subid)
{













































}