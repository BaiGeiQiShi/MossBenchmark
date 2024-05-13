/*
 * rewrite/rowsecurity.c
 *	  Routines to support policies for row level security (aka RLS).
 *
 * Policies in PostgreSQL provide a mechanism to limit what records are
 * returned to a user and what records a user is permitted to add to a table.
 *
 * Policies can be defined for specific roles, specific commands, or provided
 * by an extension.  Row security can also be enabled for a table without any
 * policies being explicitly defined, in which case a default-deny policy is
 * applied.
 *
 * Any part of the system which is returning records back to the user, or
 * which is accepting records from the user to add to a table, needs to
 * consider the policies associated with the table (if any).  For normal
 * queries, this is handled by calling get_row_security_policies() during
 * rewrite, for each RTE in the query.  This returns the expressions defined
 * by the table's policies as a list that is prepended to the securityQuals
 * list for the RTE.  For queries which modify the table, any WITH CHECK
 * clauses from the table's policies are also returned and prepended to the
 * list of WithCheckOptions for the Query to check each row that is being
 * added to the table.  Other parts of the system (eg: COPY) simply construct
 * a normal query and use that, if RLS is to be applied.
 *
 * The check to see if RLS should be enabled is provided through
 * check_enable_rls(), which returns an enum (defined in rowsecurity.h) to
 * indicate if RLS should be enabled (RLS_ENABLED), or bypassed (RLS_NONE or
 * RLS_NONE_ENV).  RLS_NONE_ENV indicates that RLS should be bypassed
 * in the current environment, but that may change if the row_security GUC or
 * the current role changes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pg_list.h"
#include "nodes/plannodes.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/syscache.h"
#include "tcop/utility.h"

static void
get_policies_for_relation(Relation relation, CmdType cmd, Oid user_id, List **permissive_policies, List **restrictive_policies);

static List *
sort_policies_by_name(List *policies);

static int
row_security_policy_cmp(const void *a, const void *b);

static void
add_security_quals(int rt_index, List *permissive_policies, List *restrictive_policies, List **securityQuals, bool *hasSubLinks);

static void
add_with_check_options(Relation rel, int rt_index, WCOKind kind, List *permissive_policies, List *restrictive_policies, List **withCheckOptions, bool *hasSubLinks, bool force_using);

static bool
check_role_for_policy(ArrayType *policy_roles, Oid user_id);

/*
 * hooks to allow extensions to add their own security policies
 *
 * row_security_policy_hook_permissive can be used to add policies which
 * are combined with the other permissive policies, using OR.
 *
 * row_security_policy_hook_restrictive can be used to add policies which
 * are enforced, regardless of other policies (they are combined using AND).
 */
row_security_policy_hook_type row_security_policy_hook_permissive = NULL;
row_security_policy_hook_type row_security_policy_hook_restrictive = NULL;

/*
 * Get any row security quals and WithCheckOption checks that should be
 * applied to the specified RTE.
 *
 * In addition, hasRowSecurity is set to true if row level security is enabled
 * (even if this RTE doesn't have any row security quals), and hasSubLinks is
 * set to true if any of the quals returned contain sublinks.
 */
void
get_row_security_policies(Query *root, RangeTblEntry *rte, int rt_index, List **securityQuals, List **withCheckOptions, bool *hasRowSecurity, bool *hasSubLinks)
{




































































































































































































































}

/*
 * get_policies_for_relation
 *
 * Returns lists of permissive and restrictive policies to be applied to the
 * specified relation, based on the command type and role.
 *
 * This includes any policies added by extensions.
 */
static void
get_policies_for_relation(Relation relation, CmdType cmd, Oid user_id, List **permissive_policies, List **restrictive_policies)
{




















































































































}

/*
 * sort_policies_by_name
 *
 * This is only used for restrictive policies, ensuring that any
 * WithCheckOptions they generate are applied in a well-defined order.
 * This is not necessary for permissive policies, since they are all combined
 * together using OR into a single WithCheckOption check.
 */
static List *
sort_policies_by_name(List *policies)
{




























}

/*
 * qsort comparator to sort RowSecurityPolicy entries by name
 */
static int
row_security_policy_cmp(const void *a, const void *b)
{














}

/*
 * add_security_quals
 *
 * Add security quals to enforce the specified RLS policies, restricting
 * access to existing data in a table.  If there are no policies controlling
 * access to the table, then all access is prohibited --- i.e., an implicit
 * default-deny policy is used.
 *
 * New security quals are added to securityQuals, and hasSubLinks is set to
 * true if any of the quals added contain sublink subqueries.
 */
static void
add_security_quals(int rt_index, List *permissive_policies, List *restrictive_policies, List **securityQuals, bool *hasSubLinks)
{










































































}

/*
 * add_with_check_options
 *
 * Add WithCheckOptions of the specified kind to check that new records
 * added by an INSERT or UPDATE are consistent with the specified RLS
 * policies.  Normally new data must satisfy the WITH CHECK clauses from the
 * policies.  If a policy has no explicit WITH CHECK clause, its USING clause
 * is used instead.  In the special case of an UPDATE arising from an
 * INSERT ... ON CONFLICT DO UPDATE, existing records are first checked using
 * a WCO_RLS_CONFLICT_CHECK WithCheckOption, which always uses the USING
 * clauses from RLS policies.
 *
 * New WCOs are added to withCheckOptions, and hasSubLinks is set to true if
 * any of the check clauses added contain sublink subqueries.
 */
static void
add_with_check_options(Relation rel, int rt_index, WCOKind kind, List *permissive_policies, List *restrictive_policies, List **withCheckOptions, bool *hasSubLinks, bool force_using)
{








































































































}

/*
 * check_role_for_policy -
 *	 determines if the policy should be applied for the current role
 */
static bool
check_role_for_policy(ArrayType *policy_roles, Oid user_id)
{


















}