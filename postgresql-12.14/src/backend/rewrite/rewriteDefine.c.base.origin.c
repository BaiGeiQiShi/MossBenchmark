/*-------------------------------------------------------------------------
 *
 * rewriteDefine.c
 *	  routines for defining a rewrite rule
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/rewrite/rewriteDefine.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_rewrite.h"
#include "catalog/storage.h"
#include "commands/policy.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_utilcmd.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rewriteSupport.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

static void
checkRuleResultList(List *targetList, TupleDesc resultDesc, bool isSelect, bool requireColumnNameMatch);
static bool
setRuleCheckAsUser_walker(Node *node, Oid *context);
static void
setRuleCheckAsUser_Query(Query *qry, Oid userid);

/*
 * InsertRule -
 *	  takes the arguments and inserts them as a row into the system
 *	  relation "pg_rewrite"
 */
static Oid
InsertRule(const char *rulname, int evtype, Oid eventrel_oid, bool evinstead, Node *event_qual, List *action, bool replace)
{



















































































































}

/*
 * DefineRule
 *		Execute a CREATE RULE command.
 */
ObjectAddress
DefineRule(RuleStmt *stmt, const char *queryString)
{















}

/*
 * DefineQueryRewrite
 *		Create a rule
 *
 * This is essentially the same as DefineRule() except that the rule's
 * action and qual have already been passed through parse analysis.
 */
ObjectAddress
DefineQueryRewrite(const char *rulename, Oid event_relid, Node *event_qual, CmdType event_type, bool is_instead, bool replace, List *action)
{










































































































































































































































































































































































































}

/*
 * checkRuleResultList
 *		Verify that targetList produces output compatible with a
 *tupledesc
 *
 * The targetList might be either a SELECT targetlist, or a RETURNING list;
 * isSelect tells which.  This is used for choosing error messages.
 *
 * A SELECT targetlist may optionally require that column names match.
 */
static void
checkRuleResultList(List *targetList, TupleDesc resultDesc, bool isSelect, bool requireColumnNameMatch)
{

















































































}

/*
 * setRuleCheckAsUser
 *		Recursively scan a query or expression tree and set the
 *checkAsUser field to the given userid in all rtable entries.
 *
 * Note: for a view (ON SELECT rule), the checkAsUser field of the OLD
 * RTE entry will be overridden when the view rule is expanded, and the
 * checkAsUser field of the NEW entry is irrelevant because that entry's
 * requiredPerms bits will always be zero.  However, for other types of rules
 * it's important to set these fields to match the rule owner.  So we just set
 * them always.
 */
void
setRuleCheckAsUser(Node *node, Oid userid)
{

}

static bool
setRuleCheckAsUser_walker(Node *node, Oid *context)
{










}

static void
setRuleCheckAsUser_Query(Query *qry, Oid userid)
{































}

/*
 * Change the firing semantics of an existing rule.
 */
void
EnableDisableRule(Relation rel, const char *rulename, char fires_when)
{






















































}

/*
 * Perform permissions and integrity checks before acquiring a relation lock.
 */
static void
RangeVarCallbackForRenameRule(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{




























}

/*
 * Rename an existing rewrite rule.
 */
ObjectAddress
RenameRewriteRule(RangeVar *relation, const char *oldName, const char *newName)
{



































































}