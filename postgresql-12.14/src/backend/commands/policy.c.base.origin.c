/*-------------------------------------------------------------------------
 *
 * policy.c
 *	  Commands for manipulating policies.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/commands/policy.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/table.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_type.h"
#include "commands/policy.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/pg_list.h"
#include "parser/parse_clause.h"
#include "parser/parse_collate.h"
#include "parser/parse_node.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "storage/lock.h"
#include "utils/acl.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
RangeVarCallbackForPolicy(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg);
static char
parse_policy_command(const char *cmd_name);
static Datum *
policy_role_list_to_array(List *roles, int *num_roles);

/*
 * Callback to RangeVarGetRelidExtended().
 *
 * Checks the following:
 *	- the relation specified is a table.
 *	- current user owns the table.
 *	- the table is not a system table.
 *
 * If any of these checks fails then an error is raised.
 */
static void
RangeVarCallbackForPolicy(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{
































}

/*
 * parse_policy_command -
 *	 helper function to convert full command strings to their char
 *	 representation.
 *
 * cmd_name - full string command name. Valid values are 'all', 'select',
 *			  'insert', 'update' and 'delete'.
 *
 */
static char
parse_policy_command(const char *cmd_name)
{

































}

/*
 * policy_role_list_to_array
 *	 helper function to convert a list of RoleSpecs to an array of
 *	 role id Datums.
 */
static Datum *
policy_role_list_to_array(List *roles, int *num_roles)
{










































}

/*
 * Load row security policy from the catalog, and store it in
 * the relation's relcache entry.
 *
 * Note that caller should have verified that pg_class.relrowsecurity
 * is true for this relation.
 */
void
RelationBuildRowSecurity(Relation relation)
{

























































































































}

/*
 * RemovePolicyById -
 *	 remove a policy by its OID.  If a policy does not exist with the
 *provided oid, then an error is raised.
 *
 * policy_id - the oid of the policy.
 */
void
RemovePolicyById(Oid policy_id)
{





























































}

/*
 * RemoveRoleFromObjectPolicy -
 *	 remove a role from a policy's applicable-roles list.
 *
 * Returns true if the role was successfully removed from the policy.
 * Returns false if the role was not removed because it would have left
 * polroles empty (which is disallowed, though perhaps it should not be).
 * On false return, the caller should instead drop the policy altogether.
 *
 * roleid - the oid of the role to remove
 * classid - should always be PolicyRelationId
 * policy_id - the oid of the policy.
 */
bool
RemoveRoleFromObjectPolicy(Oid roleid, Oid classid, Oid policy_id)
{









































































































































}

/*
 * CreatePolicy -
 *	 handles the execution of the CREATE POLICY command.
 *
 * stmt - the CreatePolicyStmt that describes the policy to create.
 */
ObjectAddress
CreatePolicy(CreatePolicyStmt *stmt)
{









































































































































































}

/*
 * AlterPolicy -
 *	 handles the execution of the ALTER POLICY command.
 *
 * stmt - the AlterPolicyStmt that describes the policy and how to alter it.
 */
ObjectAddress
AlterPolicy(AlterPolicyStmt *stmt)
{
























































































































































































































































































}

/*
 * rename_policy -
 *	 change the name of a policy on a relation
 */
ObjectAddress
rename_policy(RenameStmt *stmt)
{











































































}

/*
 * get_relation_policy_oid - Look up a policy by name to find its OID
 *
 * If missing_ok is false, throw an error if policy not found.  If
 * true, just return InvalidOid.
 */
Oid
get_relation_policy_oid(Oid relid, const char *policy_name, bool missing_ok)
{





































}

/*
 * relation_has_policies - Determine if relation has any policies
 */
bool
relation_has_policies(Relation rel)
{



















}