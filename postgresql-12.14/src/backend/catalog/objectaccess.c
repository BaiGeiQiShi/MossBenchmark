/* -------------------------------------------------------------------------
 *
 * objectaccess.c
 *		functions for object_access_hook on various events
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"

/*
 * Hook on object accesses.  This is intended as infrastructure for security
 * and logging plugins.
 */
object_access_hook_type object_access_hook = NULL;

/*
 * RunObjectPostCreateHook
 *
 * It is entrypoint of OAT_POST_CREATE event
 */
void
RunObjectPostCreateHook(Oid classId, Oid objectId, int subId, bool is_internal)
{









}

/*
 * RunObjectDropHook
 *
 * It is entrypoint of OAT_DROP event
 */
void
RunObjectDropHook(Oid classId, Oid objectId, int subId, int dropflags)
{









}

/*
 * RunObjectPostAlterHook
 *
 * It is entrypoint of OAT_POST_ALTER event
 */
void
RunObjectPostAlterHook(Oid classId, Oid objectId, int subId, Oid auxiliaryId, bool is_internal)
{










}

/*
 * RunNamespaceSearchHook
 *
 * It is entrypoint of OAT_NAMESPACE_SEARCH event
 */
bool
RunNamespaceSearchHook(Oid objectId, bool ereport_on_violation)
{












}

/*
 * RunFunctionExecuteHook
 *
 * It is entrypoint of OAT_FUNCTION_EXECUTE event
 */
void
RunFunctionExecuteHook(Oid objectId)
{




}
