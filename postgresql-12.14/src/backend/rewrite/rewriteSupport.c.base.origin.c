/*-------------------------------------------------------------------------
 *
 * rewriteSupport.c
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/rewrite/rewriteSupport.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_rewrite.h"
#include "rewrite/rewriteSupport.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

/*
 * Is there a rule by the given name?
 */
bool
IsDefinedRewriteRule(Oid owningRel, const char *ruleName)
{

}

/*
 * SetRelationRuleStatus
 *		Set the value of the relation's relhasrules field in pg_class.
 *
 * NOTE: caller must be holding an appropriate lock on the relation.
 *
 * NOTE: an important side-effect of this operation is that an SI invalidation
 * message is sent out to all backends --- including me --- causing relcache
 * entries to be flushed or updated with the new set of rules for the table.
 * This must happen even if we find that no change is needed in the pg_class
 * row.
 */
void
SetRelationRuleStatus(Oid relationId, bool relHasRules)
{






























}

/*
 * Find rule oid.
 *
 * If missing_ok is false, throw an error if rule name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_rewrite_oid(Oid relid, const char *rulename, bool missing_ok)
{



















}