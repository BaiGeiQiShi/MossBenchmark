/*-------------------------------------------------------------------------
 *
 * acl.c
 *	  Basic access control list data structures manipulation routines.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/acl.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_type.h"
#include "catalog/pg_class.h"
#include "commands/dbcommands.h"
#include "commands/proclang.h"
#include "commands/tablespace.h"
#include "foreign/foreign.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/hashutils.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

typedef struct
{
  const char *name;
  AclMode value;
} priv_map;

/*
 * We frequently need to test whether a given role is a member of some other
 * role.  In most of these tests the "given role" is the same, namely the
 * active current user.  So we can optimize it by keeping a cached list of
 * all the roles the "given role" is a member of, directly or indirectly.
 *
 * There are actually two caches, one computed under "has_privs" rules
 * (do not recurse where rolinherit isn't true) and one computed under
 * "is_member" rules (recurse regardless of rolinherit).
 *
 * Possibly this mechanism should be generalized to allow caching membership
 * info for multiple roles?
 *
 * The has_privs cache is:
 * cached_privs_role is the role OID the cache is for.
 * cached_privs_roles is an OID list of roles that cached_privs_role
 *		has the privileges of (always including itself).
 * The cache is valid if cached_privs_role is not InvalidOid.
 *
 * The is_member cache is similarly:
 * cached_member_role is the role OID the cache is for.
 * cached_membership_roles is an OID list of roles that cached_member_role
 *		is a member of (always including itself).
 * The cache is valid if cached_member_role is not InvalidOid.
 */
static Oid cached_privs_role = InvalidOid;
static List *cached_privs_roles = NIL;
static Oid cached_member_role = InvalidOid;
static List *cached_membership_roles = NIL;

static const char *
getid(const char *s, char *n);
static void
putid(char *p, const char *s);
static Acl *
allocacl(int n);
static void
check_acl(const Acl *acl);
static const char *
aclparse(const char *s, AclItem *aip);
static bool
aclitem_match(const AclItem *a1, const AclItem *a2);
static int
aclitemComparator(const void *arg1, const void *arg2);
static void
check_circularity(const Acl *old_acl, const AclItem *mod_aip, Oid ownerId);
static Acl *
recursive_revoke(Acl *acl, Oid grantee, AclMode revoke_privs, Oid ownerId, DropBehavior behavior);

static AclMode
convert_priv_string(text *priv_type_text);
static AclMode
convert_any_priv_string(text *priv_type_text, const priv_map *privileges);

static Oid
convert_table_name(text *tablename);
static AclMode
convert_table_priv_string(text *priv_type_text);
static AclMode
convert_sequence_priv_string(text *priv_type_text);
static AttrNumber
convert_column_name(Oid tableoid, text *column);
static AclMode
convert_column_priv_string(text *priv_type_text);
static Oid
convert_database_name(text *databasename);
static AclMode
convert_database_priv_string(text *priv_type_text);
static Oid
convert_foreign_data_wrapper_name(text *fdwname);
static AclMode
convert_foreign_data_wrapper_priv_string(text *priv_type_text);
static Oid
convert_function_name(text *functionname);
static AclMode
convert_function_priv_string(text *priv_type_text);
static Oid
convert_language_name(text *languagename);
static AclMode
convert_language_priv_string(text *priv_type_text);
static Oid
convert_schema_name(text *schemaname);
static AclMode
convert_schema_priv_string(text *priv_type_text);
static Oid
convert_server_name(text *servername);
static AclMode
convert_server_priv_string(text *priv_type_text);
static Oid
convert_tablespace_name(text *tablespacename);
static AclMode
convert_tablespace_priv_string(text *priv_type_text);
static Oid
convert_type_name(text *typename);
static AclMode
convert_type_priv_string(text *priv_type_text);
static AclMode
convert_role_priv_string(text *priv_type_text);
static AclResult
pg_role_aclcheck(Oid role_oid, Oid roleid, AclMode mode);

static void
RoleMembershipCacheCallback(Datum arg, int cacheid, uint32 hashvalue);

/*
 * getid
 *		Consumes the first alphanumeric string (identifier) found in
 *string 's', ignoring any leading white space.  If it finds a double quote it
 *returns the word inside the quotes.
 *
 * RETURNS:
 *		the string position in 's' that points to the next non-space
 *character in 's', after any quotes.  Also:
 *		- loads the identifier into 'n'.  (If no identifier is found,
 *'n' contains an empty string.)  'n' must be NAMEDATALEN bytes.
 */
static const char *
getid(const char *s, char *n)
{






































}

/*
 * Write a role name at *p, adding double quotes if needed.
 * There must be at least (2*NAMEDATALEN)+2 bytes available at *p.
 * This needs to be kept in sync with copyAclUserName in pg_dump/dumputils.c
 */
static void
putid(char *p, const char *s)
{






























}

/*
 * aclparse
 *		Consumes and parses an ACL specification of the form:
 *				[group|user] [A-Za-z0-9]*=[rwaR]*
 *		from string 's', ignoring any leading white space or white space
 *		between the optional id type keyword (group|user) and the actual
 *		ACL specification.
 *
 *		The group|user decoration is unnecessary in the roles world,
 *		but we still accept it for backward compatibility.
 *
 *		This routine is called by the parser as well as aclitemin(),
 *hence the added generality.
 *
 * RETURNS:
 *		the string position in 's' immediately following the ACL
 *		specification.  Also:
 *		- loads the structure pointed to by 'aip' with the appropriate
 *		  UID/GID, id type identifier and mode type values.
 */
static const char *
aclparse(const char *s, AclItem *aip)
{























































































































}

/*
 * allocacl
 *		Allocates storage for a new Acl with 'n' entries.
 *
 * RETURNS:
 *		the new Acl
 */
static Acl *
allocacl(int n)
{
















}

/*
 * Create a zero-entry ACL
 */
Acl *
make_empty_acl(void)
{

}

/*
 * Copy an ACL
 */
Acl *
aclcopy(const Acl *orig_acl)
{







}

/*
 * Concatenate two ACLs
 *
 * This is a bit cheesy, since we may produce an ACL with redundant entries.
 * Be careful what the result is used for!
 */
Acl *
aclconcat(const Acl *left_acl, const Acl *right_acl)
{









}

/*
 * Merge two ACLs
 *
 * This produces a properly merged ACL with no redundant entries.
 * Returns NULL on NULL input.
 */
Acl *
aclmerge(const Acl *left_acl, const Acl *right_acl, Oid ownerId)
{








































}

/*
 * Sort the items in an ACL (into an arbitrary but consistent order)
 */
void
aclitemsort(Acl *acl)
{




}

/*
 * Check if two ACLs are exactly equal
 *
 * This will not detect equality if the two arrays contain the same items
 * in different orders.  To handle that case, sort both inputs first,
 * using aclitemsort().
 */
bool
aclequal(const Acl *left_acl, const Acl *right_acl)
{































}

/*
 * Verify that an ACL array is acceptable (one-dimensional and has no nulls)
 */
static void
check_acl(const Acl *acl)
{












}

/*
 * aclitemin
 *		Allocates storage for, and fills in, a new AclItem given a
 *string 's' that contains an ACL specification.  See aclparse for details.
 *
 * RETURNS:
 *		the new AclItem
 */
Datum
aclitemin(PG_FUNCTION_ARGS)
{















}

/*
 * aclitemout
 *		Allocates storage for, and fills in, a new null-delimited string
 *		containing a formatted ACL specification.  See aclparse for
 *details.
 *
 * RETURNS:
 *		the new string
 */
Datum
aclitemout(PG_FUNCTION_ARGS)
{




























































}

/*
 * aclitem_match
 *		Two AclItems are considered to match iff they have the same
 *		grantee and grantor; the privileges are ignored.
 */
static bool
aclitem_match(const AclItem *a1, const AclItem *a2)
{

}

/*
 * aclitemComparator
 *		qsort comparison function for AclItems
 */
static int
aclitemComparator(const void *arg1, const void *arg2)
{




























}

/*
 * aclitem equality operator
 */
Datum
aclitem_eq(PG_FUNCTION_ARGS)
{






}

/*
 * aclitem hash function
 *
 * We make aclitems hashable not so much because anyone is likely to hash
 * them, as because we want array equality to work on aclitem arrays, and
 * with the typcache mechanism we must have a hash or btree opclass.
 */
Datum
hash_aclitem(PG_FUNCTION_ARGS)
{




}

/*
 * 64-bit hash function for aclitem.
 *
 * Similar to hash_aclitem, but accepts a seed and returns a uint64 value.
 */
Datum
hash_aclitem_extended(PG_FUNCTION_ARGS)
{





}

/*
 * acldefault()  --- create an ACL describing default access permissions
 *
 * Change this routine if you want to alter the default access policy for
 * newly-created objects (or any object with a NULL acl entry).  When
 * you make a change here, don't forget to update the GRANT man page,
 * which explains all the default permissions.
 *
 * Note that these are the hard-wired "defaults" that are used in the
 * absence of any pg_default_acl entry.
 */
Acl *
acldefault(ObjectType objtype, Oid ownerId)
{











































































































}

/*
 * SQL-accessible version of acldefault().  Hackish mapping from "char" type to
 * OBJECT_* values.
 */
Datum
acldefault_sql(PG_FUNCTION_ARGS)
{















































}

/*
 * Update an ACL array to add or remove specified privileges.
 *
 *	old_acl: the input ACL array
 *	mod_aip: defines the privileges to be added, removed, or substituted
 *	modechg: ACL_MODECHG_ADD, ACL_MODECHG_DEL, or ACL_MODECHG_EQL
 *	ownerId: Oid of object owner
 *	behavior: RESTRICT or CASCADE behavior for recursive removal
 *
 * ownerid and behavior are only relevant when the update operation specifies
 * deletion of grant options.
 *
 * The result is a modified copy; the input object is not changed.
 *
 * NB: caller is responsible for having detoasted the input ACL, if needed.
 */
Acl *
aclupdate(const Acl *old_acl, const AclItem *mod_aip, int modechg, Oid ownerId, DropBehavior behavior)
{




























































































}

/*
 * Update an ACL array to reflect a change of owner to the parent object
 *
 *	old_acl: the input ACL array (must not be NULL)
 *	oldOwnerId: Oid of the old object owner
 *	newOwnerId: Oid of the new object owner
 *
 * The result is a modified copy; the input object is not changed.
 *
 * NB: caller is responsible for having detoasted the input ACL, if needed.
 */
Acl *
aclnewowner(const Acl *old_acl, Oid oldOwnerId, Oid newOwnerId)
{



























































































}

/*
 * When granting grant options, we must disallow attempts to set up circular
 * chains of grant options.  Suppose A (the object owner) grants B some
 * privileges with grant option, and B re-grants them to C.  If C could
 * grant the privileges to B as well, then A would be unable to effectively
 * revoke the privileges from B, since recursive_revoke would consider that
 * B still has 'em from C.
 *
 * We check for this by recursively deleting all grant options belonging to
 * the target grantee, and then seeing if the would-be grantor still has the
 * grant option or not.
 */
static void
check_circularity(const Acl *old_acl, const AclItem *mod_aip, Oid ownerId)
{





















































}

/*
 * Ensure that no privilege is "abandoned".  A privilege is abandoned
 * if the user that granted the privilege loses the grant option.  (So
 * the chain through which it was granted is broken.)  Either the
 * abandoned privileges are revoked as well, or an error message is
 * printed, depending on the drop behavior option.
 *
 *	acl: the input ACL list
 *	grantee: the user from whom some grant options have been revoked
 *	revoke_privs: the grant options being revoked
 *	ownerId: Oid of object owner
 *	behavior: RESTRICT or CASCADE behavior for recursive removal
 *
 * The input Acl object is pfree'd if replaced.
 */
static Acl *
recursive_revoke(Acl *acl, Oid grantee, AclMode revoke_privs, Oid ownerId, DropBehavior behavior)
{

















































}

/*
 * aclmask --- compute bitmask of all privileges held by roleid.
 *
 * When 'how' = ACLMASK_ALL, this simply returns the privilege bits
 * held by the given roleid according to the given ACL list, ANDed
 * with 'mask'.  (The point of passing 'mask' is to let the routine
 * exit early if all privileges of interest have been found.)
 *
 * When 'how' = ACLMASK_ANY, returns as soon as any bit in the mask
 * is known true.  (This lets us exit soonest in cases where the
 * caller is only going to test for zero or nonzero result.)
 *
 * Usage patterns:
 *
 * To see if any of a set of privileges are held:
 *		if (aclmask(acl, roleid, ownerId, privs, ACLMASK_ANY) != 0)
 *
 * To see if all of a set of privileges are held:
 *		if (aclmask(acl, roleid, ownerId, privs, ACLMASK_ALL) == privs)
 *
 * To determine exactly which of a set of privileges are held:
 *		heldprivs = aclmask(acl, roleid, ownerId, privs, ACLMASK_ALL);
 */
AclMode
aclmask(const Acl *acl, Oid roleid, Oid ownerId, AclMode mask, AclMaskHow how)
{



















































































}

/*
 * aclmask_direct --- compute bitmask of all privileges held by roleid.
 *
 * This is exactly like aclmask() except that we consider only privileges
 * held *directly* by roleid, not those inherited via role membership.
 */
static AclMode
aclmask_direct(const Acl *acl, Oid roleid, Oid ownerId, AclMode mask, AclMaskHow how)
{






















































}

/*
 * aclmembers
 *		Find out all the roleids mentioned in an Acl.
 *		Note that we do not distinguish grantors from grantees.
 *
 * *roleids is set to point to a palloc'd array containing distinct OIDs
 * in sorted order.  The length of the array is the function result.
 */
int
aclmembers(const Acl *acl, Oid **roleids)
{























































}

/*
 * aclinsert (exported function)
 */
Datum
aclinsert(PG_FUNCTION_ARGS)
{



}

Datum
aclremove(PG_FUNCTION_ARGS)
{



}

Datum
aclcontains(PG_FUNCTION_ARGS)
{
















}

Datum
makeaclitem(PG_FUNCTION_ARGS)
{

















}

static AclMode
convert_priv_string(text *priv_type_text)
{





























































}

/*
 * convert_any_priv_string: recognize privilege strings for has_foo_privilege
 *
 * We accept a comma-separated list of case-insensitive privilege names,
 * producing a bitmask of the OR'd privilege bits.  We are liberal about
 * whitespace between items, not so much about whitespace within items.
 * The allowed privilege names are given as an array of priv_map structs,
 * terminated by one with a NULL name pointer.
 */
static AclMode
convert_any_priv_string(text *priv_type_text, const priv_map *privileges)
{















































}

static const char *
convert_aclright_to_string(int aclright)
{






























}

/*----------
 * Convert an aclitem[] to a table.
 *
 * Example:
 *
 * aclexplode('{=r/joe,foo=a*w/joe}'::aclitem[])
 *
 * returns the table
 *
 * {{ OID(joe), 0::OID,   'SELECT', false },
 *	{ OID(joe), OID(foo), 'INSERT', true },
 *	{ OID(joe), OID(foo), 'UPDATE', false }}
 *----------
 */
Datum
aclexplode(PG_FUNCTION_ARGS)
{

















































































}

/*
 * has_table_privilege variants
 *		These are all named "has_table_privilege" at the SQL level.
 *		They take various combinations of relation name, relation OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.  The variants that take a relation OID
 *		return NULL if the OID doesn't exist (rather than failing, as
 *		they did before Postgres 8.4).
 */

/*
 * has_table_privilege_name_name
 *		Check user privileges on a table given
 *		name username, text tablename, and text priv name.
 */
Datum
has_table_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_table_privilege_name
 *		Check user privileges on a table given
 *		text tablename and text priv name.
 *		current_user is assumed
 */
Datum
has_table_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_table_privilege_name_id
 *		Check user privileges on a table given
 *		name usename, table oid, and text priv name.
 */
Datum
has_table_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_table_privilege_id
 *		Check user privileges on a table given
 *		table oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_table_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_table_privilege_id_name
 *		Check user privileges on a table given
 *		roleid, text tablename, and text priv name.
 */
Datum
has_table_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_table_privilege_id_id
 *		Check user privileges on a table given
 *		roleid, table oid, and text priv name.
 */
Datum
has_table_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_table_privilege family.
 */

/*
 * Given a table name expressed as a string, look it up and return Oid
 */
static Oid
convert_table_name(text *tablename)
{






}

/*
 * convert_table_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_table_priv_string(text *priv_type_text)
{




}

/*
 * has_sequence_privilege variants
 *		These are all named "has_sequence_privilege" at the SQL level.
 *		They take various combinations of relation name, relation OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.  The variants that take a relation OID
 *		return NULL if the OID doesn't exist.
 */

/*
 * has_sequence_privilege_name_name
 *		Check user privileges on a sequence given
 *		name username, text sequencename, and text priv name.
 */
Datum
has_sequence_privilege_name_name(PG_FUNCTION_ARGS)
{



















}

/*
 * has_sequence_privilege_name
 *		Check user privileges on a sequence given
 *		text sequencename and text priv name.
 *		current_user is assumed
 */
Datum
has_sequence_privilege_name(PG_FUNCTION_ARGS)
{


















}

/*
 * has_sequence_privilege_name_id
 *		Check user privileges on a sequence given
 *		name usename, sequence oid, and text priv name.
 */
Datum
has_sequence_privilege_name_id(PG_FUNCTION_ARGS)
{























}

/*
 * has_sequence_privilege_id
 *		Check user privileges on a sequence given
 *		sequence oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_sequence_privilege_id(PG_FUNCTION_ARGS)
{






















}

/*
 * has_sequence_privilege_id_name
 *		Check user privileges on a sequence given
 *		roleid, text sequencename, and text priv name.
 */
Datum
has_sequence_privilege_id_name(PG_FUNCTION_ARGS)
{

















}

/*
 * has_sequence_privilege_id_id
 *		Check user privileges on a sequence given
 *		roleid, sequence oid, and text priv name.
 */
Datum
has_sequence_privilege_id_id(PG_FUNCTION_ARGS)
{





















}

/*
 * convert_sequence_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_sequence_priv_string(text *priv_type_text)
{



}

/*
 * has_any_column_privilege variants
 *		These are all named "has_any_column_privilege" at the SQL level.
 *		They take various combinations of relation name, relation OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege for any column of the table, false if not.  The
 *variants that take a relation OID return NULL if the OID doesn't exist.
 */

/*
 * has_any_column_privilege_name_name
 *		Check user privileges on any column of a table given
 *		name username, text tablename, and text priv name.
 */
Datum
has_any_column_privilege_name_name(PG_FUNCTION_ARGS)
{




















}

/*
 * has_any_column_privilege_name
 *		Check user privileges on any column of a table given
 *		text tablename and text priv name.
 *		current_user is assumed
 */
Datum
has_any_column_privilege_name(PG_FUNCTION_ARGS)
{



















}

/*
 * has_any_column_privilege_name_id
 *		Check user privileges on any column of a table given
 *		name usename, table oid, and text priv name.
 */
Datum
has_any_column_privilege_name_id(PG_FUNCTION_ARGS)
{























}

/*
 * has_any_column_privilege_id
 *		Check user privileges on any column of a table given
 *		table oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_any_column_privilege_id(PG_FUNCTION_ARGS)
{






















}

/*
 * has_any_column_privilege_id_name
 *		Check user privileges on any column of a table given
 *		roleid, text tablename, and text priv name.
 */
Datum
has_any_column_privilege_id_name(PG_FUNCTION_ARGS)
{


















}

/*
 * has_any_column_privilege_id_id
 *		Check user privileges on any column of a table given
 *		roleid, table oid, and text priv name.
 */
Datum
has_any_column_privilege_id_id(PG_FUNCTION_ARGS)
{





















}

/*
 * has_column_privilege variants
 *		These are all named "has_column_privilege" at the SQL level.
 *		They take various combinations of relation name, relation OID,
 *		column name, column attnum, user name, user OID, or
 *		implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.  The variants that take a relation OID
 *		return NULL (rather than throwing an error) if that relation OID
 *		doesn't exist.  Likewise, the variants that take an integer
 *attnum return NULL (rather than throwing an error) if there is no such
 *		pg_attribute entry.  All variants return NULL if an attisdropped
 *		column is selected.  These rules are meant to avoid unnecessary
 *		failures in queries that scan pg_attribute.
 */

/*
 * column_privilege_check: check column privileges, but don't throw an error
 *		for dropped column or table
 *
 * Returns 1 if have the privilege, 0 if not, -1 if dropped column/table.
 */
static int
column_privilege_check(Oid tableoid, AttrNumber attnum, Oid roleid, AclMode mode)
{





















































}

/*
 * has_column_privilege_name_name_name
 *		Check user privileges on a column given
 *		name username, text tablename, text colname, and text priv name.
 */
Datum
has_column_privilege_name_name_name(PG_FUNCTION_ARGS)
{





















}

/*
 * has_column_privilege_name_name_attnum
 *		Check user privileges on a column given
 *		name username, text tablename, int attnum, and text priv name.
 */
Datum
has_column_privilege_name_name_attnum(PG_FUNCTION_ARGS)
{



















}

/*
 * has_column_privilege_name_id_name
 *		Check user privileges on a column given
 *		name username, table oid, text colname, and text priv name.
 */
Datum
has_column_privilege_name_id_name(PG_FUNCTION_ARGS)
{



















}

/*
 * has_column_privilege_name_id_attnum
 *		Check user privileges on a column given
 *		name username, table oid, int attnum, and text priv name.
 */
Datum
has_column_privilege_name_id_attnum(PG_FUNCTION_ARGS)
{

















}

/*
 * has_column_privilege_id_name_name
 *		Check user privileges on a column given
 *		oid roleid, text tablename, text colname, and text priv name.
 */
Datum
has_column_privilege_id_name_name(PG_FUNCTION_ARGS)
{



















}

/*
 * has_column_privilege_id_name_attnum
 *		Check user privileges on a column given
 *		oid roleid, text tablename, int attnum, and text priv name.
 */
Datum
has_column_privilege_id_name_attnum(PG_FUNCTION_ARGS)
{

















}

/*
 * has_column_privilege_id_id_name
 *		Check user privileges on a column given
 *		oid roleid, table oid, text colname, and text priv name.
 */
Datum
has_column_privilege_id_id_name(PG_FUNCTION_ARGS)
{

















}

/*
 * has_column_privilege_id_id_attnum
 *		Check user privileges on a column given
 *		oid roleid, table oid, int attnum, and text priv name.
 */
Datum
has_column_privilege_id_id_attnum(PG_FUNCTION_ARGS)
{















}

/*
 * has_column_privilege_name_name
 *		Check user privileges on a column given
 *		text tablename, text colname, and text priv name.
 *		current_user is assumed
 */
Datum
has_column_privilege_name_name(PG_FUNCTION_ARGS)
{




















}

/*
 * has_column_privilege_name_attnum
 *		Check user privileges on a column given
 *		text tablename, int attnum, and text priv name.
 *		current_user is assumed
 */
Datum
has_column_privilege_name_attnum(PG_FUNCTION_ARGS)
{


















}

/*
 * has_column_privilege_id_name
 *		Check user privileges on a column given
 *		table oid, text colname, and text priv name.
 *		current_user is assumed
 */
Datum
has_column_privilege_id_name(PG_FUNCTION_ARGS)
{


















}

/*
 * has_column_privilege_id_attnum
 *		Check user privileges on a column given
 *		table oid, int attnum, and text priv name.
 *		current_user is assumed
 */
Datum
has_column_privilege_id_attnum(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_column_privilege family.
 */

/*
 * Given a table OID and a column name expressed as a string, look it up
 * and return the column number.  Returns InvalidAttrNumber in cases
 * where caller should return NULL instead of failing.
 */
static AttrNumber
convert_column_name(Oid tableoid, text *column)
{
















































}

/*
 * convert_column_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_column_priv_string(text *priv_type_text)
{



}

/*
 * has_database_privilege variants
 *		These are all named "has_database_privilege" at the SQL level.
 *		They take various combinations of database name, database OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not, or NULL if object doesn't exist.
 */

/*
 * has_database_privilege_name_name
 *		Check user privileges on a database given
 *		name username, text databasename, and text priv name.
 */
Datum
has_database_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_database_privilege_name
 *		Check user privileges on a database given
 *		text databasename and text priv name.
 *		current_user is assumed
 */
Datum
has_database_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_database_privilege_name_id
 *		Check user privileges on a database given
 *		name usename, database oid, and text priv name.
 */
Datum
has_database_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_database_privilege_id
 *		Check user privileges on a database given
 *		database oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_database_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_database_privilege_id_name
 *		Check user privileges on a database given
 *		roleid, text databasename, and text priv name.
 */
Datum
has_database_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_database_privilege_id_id
 *		Check user privileges on a database given
 *		roleid, database oid, and text priv name.
 */
Datum
has_database_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_database_privilege family.
 */

/*
 * Given a database name expressed as a string, look it up and return Oid
 */
static Oid
convert_database_name(text *databasename)
{



}

/*
 * convert_database_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_database_priv_string(text *priv_type_text)
{



}

/*
 * has_foreign_data_wrapper_privilege variants
 *		These are all named "has_foreign_data_wrapper_privilege" at the
 *SQL level. They take various combinations of foreign-data wrapper name, fdw
 *OID, user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.
 */

/*
 * has_foreign_data_wrapper_privilege_name_name
 *		Check user privileges on a foreign-data wrapper given
 *		name username, text fdwname, and text priv name.
 */
Datum
has_foreign_data_wrapper_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_foreign_data_wrapper_privilege_name
 *		Check user privileges on a foreign-data wrapper given
 *		text fdwname and text priv name.
 *		current_user is assumed
 */
Datum
has_foreign_data_wrapper_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_foreign_data_wrapper_privilege_name_id
 *		Check user privileges on a foreign-data wrapper given
 *		name usename, foreign-data wrapper oid, and text priv name.
 */
Datum
has_foreign_data_wrapper_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_foreign_data_wrapper_privilege_id
 *		Check user privileges on a foreign-data wrapper given
 *		foreign-data wrapper oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_foreign_data_wrapper_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_foreign_data_wrapper_privilege_id_name
 *		Check user privileges on a foreign-data wrapper given
 *		roleid, text fdwname, and text priv name.
 */
Datum
has_foreign_data_wrapper_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_foreign_data_wrapper_privilege_id_id
 *		Check user privileges on a foreign-data wrapper given
 *		roleid, fdw oid, and text priv name.
 */
Datum
has_foreign_data_wrapper_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_foreign_data_wrapper_privilege family.
 */

/*
 * Given a FDW name expressed as a string, look it up and return Oid
 */
static Oid
convert_foreign_data_wrapper_name(text *fdwname)
{



}

/*
 * convert_foreign_data_wrapper_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_foreign_data_wrapper_priv_string(text *priv_type_text)
{



}

/*
 * has_function_privilege variants
 *		These are all named "has_function_privilege" at the SQL level.
 *		They take various combinations of function name, function OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not, or NULL if object doesn't exist.
 */

/*
 * has_function_privilege_name_name
 *		Check user privileges on a function given
 *		name username, text functionname, and text priv name.
 */
Datum
has_function_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_function_privilege_name
 *		Check user privileges on a function given
 *		text functionname and text priv name.
 *		current_user is assumed
 */
Datum
has_function_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_function_privilege_name_id
 *		Check user privileges on a function given
 *		name usename, function oid, and text priv name.
 */
Datum
has_function_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_function_privilege_id
 *		Check user privileges on a function given
 *		function oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_function_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_function_privilege_id_name
 *		Check user privileges on a function given
 *		roleid, text functionname, and text priv name.
 */
Datum
has_function_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_function_privilege_id_id
 *		Check user privileges on a function given
 *		roleid, function oid, and text priv name.
 */
Datum
has_function_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_function_privilege family.
 */

/*
 * Given a function name expressed as a string, look it up and return Oid
 */
static Oid
convert_function_name(text *functionname)
{











}

/*
 * convert_function_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_function_priv_string(text *priv_type_text)
{



}

/*
 * has_language_privilege variants
 *		These are all named "has_language_privilege" at the SQL level.
 *		They take various combinations of language name, language OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not, or NULL if object doesn't exist.
 */

/*
 * has_language_privilege_name_name
 *		Check user privileges on a language given
 *		name username, text languagename, and text priv name.
 */
Datum
has_language_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_language_privilege_name
 *		Check user privileges on a language given
 *		text languagename and text priv name.
 *		current_user is assumed
 */
Datum
has_language_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_language_privilege_name_id
 *		Check user privileges on a language given
 *		name usename, language oid, and text priv name.
 */
Datum
has_language_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_language_privilege_id
 *		Check user privileges on a language given
 *		language oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_language_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_language_privilege_id_name
 *		Check user privileges on a language given
 *		roleid, text languagename, and text priv name.
 */
Datum
has_language_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_language_privilege_id_id
 *		Check user privileges on a language given
 *		roleid, language oid, and text priv name.
 */
Datum
has_language_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_language_privilege family.
 */

/*
 * Given a language name expressed as a string, look it up and return Oid
 */
static Oid
convert_language_name(text *languagename)
{



}

/*
 * convert_language_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_language_priv_string(text *priv_type_text)
{



}

/*
 * has_schema_privilege variants
 *		These are all named "has_schema_privilege" at the SQL level.
 *		They take various combinations of schema name, schema OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not, or NULL if object doesn't exist.
 */

/*
 * has_schema_privilege_name_name
 *		Check user privileges on a schema given
 *		name username, text schemaname, and text priv name.
 */
Datum
has_schema_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_schema_privilege_name
 *		Check user privileges on a schema given
 *		text schemaname and text priv name.
 *		current_user is assumed
 */
Datum
has_schema_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_schema_privilege_name_id
 *		Check user privileges on a schema given
 *		name usename, schema oid, and text priv name.
 */
Datum
has_schema_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_schema_privilege_id
 *		Check user privileges on a schema given
 *		schema oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_schema_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_schema_privilege_id_name
 *		Check user privileges on a schema given
 *		roleid, text schemaname, and text priv name.
 */
Datum
has_schema_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_schema_privilege_id_id
 *		Check user privileges on a schema given
 *		roleid, schema oid, and text priv name.
 */
Datum
has_schema_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_schema_privilege family.
 */

/*
 * Given a schema name expressed as a string, look it up and return Oid
 */
static Oid
convert_schema_name(text *schemaname)
{



}

/*
 * convert_schema_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_schema_priv_string(text *priv_type_text)
{



}

/*
 * has_server_privilege variants
 *		These are all named "has_server_privilege" at the SQL level.
 *		They take various combinations of foreign server name,
 *		server OID, user name, user OID, or implicit user =
 *current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.
 */

/*
 * has_server_privilege_name_name
 *		Check user privileges on a foreign server given
 *		name username, text servername, and text priv name.
 */
Datum
has_server_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_server_privilege_name
 *		Check user privileges on a foreign server given
 *		text servername and text priv name.
 *		current_user is assumed
 */
Datum
has_server_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_server_privilege_name_id
 *		Check user privileges on a foreign server given
 *		name usename, foreign server oid, and text priv name.
 */
Datum
has_server_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_server_privilege_id
 *		Check user privileges on a foreign server given
 *		server oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_server_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_server_privilege_id_name
 *		Check user privileges on a foreign server given
 *		roleid, text servername, and text priv name.
 */
Datum
has_server_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_server_privilege_id_id
 *		Check user privileges on a foreign server given
 *		roleid, server oid, and text priv name.
 */
Datum
has_server_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_server_privilege family.
 */

/*
 * Given a server name expressed as a string, look it up and return Oid
 */
static Oid
convert_server_name(text *servername)
{



}

/*
 * convert_server_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_server_priv_string(text *priv_type_text)
{



}

/*
 * has_tablespace_privilege variants
 *		These are all named "has_tablespace_privilege" at the SQL level.
 *		They take various combinations of tablespace name, tablespace
 *OID, user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.
 */

/*
 * has_tablespace_privilege_name_name
 *		Check user privileges on a tablespace given
 *		name username, text tablespacename, and text priv name.
 */
Datum
has_tablespace_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_tablespace_privilege_name
 *		Check user privileges on a tablespace given
 *		text tablespacename and text priv name.
 *		current_user is assumed
 */
Datum
has_tablespace_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_tablespace_privilege_name_id
 *		Check user privileges on a tablespace given
 *		name usename, tablespace oid, and text priv name.
 */
Datum
has_tablespace_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_tablespace_privilege_id
 *		Check user privileges on a tablespace given
 *		tablespace oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_tablespace_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_tablespace_privilege_id_name
 *		Check user privileges on a tablespace given
 *		roleid, text tablespacename, and text priv name.
 */
Datum
has_tablespace_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_tablespace_privilege_id_id
 *		Check user privileges on a tablespace given
 *		roleid, tablespace oid, and text priv name.
 */
Datum
has_tablespace_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_tablespace_privilege family.
 */

/*
 * Given a tablespace name expressed as a string, look it up and return Oid
 */
static Oid
convert_tablespace_name(text *tablespacename)
{



}

/*
 * convert_tablespace_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_tablespace_priv_string(text *priv_type_text)
{



}

/*
 * has_type_privilege variants
 *		These are all named "has_type_privilege" at the SQL level.
 *		They take various combinations of type name, type OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not, or NULL if object doesn't exist.
 */

/*
 * has_type_privilege_name_name
 *		Check user privileges on a type given
 *		name username, text typename, and text priv name.
 */
Datum
has_type_privilege_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * has_type_privilege_name
 *		Check user privileges on a type given
 *		text typename and text priv name.
 *		current_user is assumed
 */
Datum
has_type_privilege_name(PG_FUNCTION_ARGS)
{














}

/*
 * has_type_privilege_name_id
 *		Check user privileges on a type given
 *		name usename, type oid, and text priv name.
 */
Datum
has_type_privilege_name_id(PG_FUNCTION_ARGS)
{


















}

/*
 * has_type_privilege_id
 *		Check user privileges on a type given
 *		type oid, and text priv name.
 *		current_user is assumed
 */
Datum
has_type_privilege_id(PG_FUNCTION_ARGS)
{

















}

/*
 * has_type_privilege_id_name
 *		Check user privileges on a type given
 *		roleid, text typename, and text priv name.
 */
Datum
has_type_privilege_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * has_type_privilege_id_id
 *		Check user privileges on a type given
 *		roleid, type oid, and text priv name.
 */
Datum
has_type_privilege_id_id(PG_FUNCTION_ARGS)
{
















}

/*
 *		Support routines for has_type_privilege family.
 */

/*
 * Given a type name expressed as a string, look it up and return Oid
 */
static Oid
convert_type_name(text *typename)
{











}

/*
 * convert_type_priv_string
 *		Convert text string to AclMode value.
 */
static AclMode
convert_type_priv_string(text *priv_type_text)
{



}

/*
 * pg_has_role variants
 *		These are all named "pg_has_role" at the SQL level.
 *		They take various combinations of role name, role OID,
 *		user name, user OID, or implicit user = current_user.
 *
 *		The result is a boolean value: true if user has the indicated
 *		privilege, false if not.
 */

/*
 * pg_has_role_name_name
 *		Check user privileges on a role given
 *		name username, name rolename, and text priv name.
 */
Datum
pg_has_role_name_name(PG_FUNCTION_ARGS)
{















}

/*
 * pg_has_role_name
 *		Check user privileges on a role given
 *		name rolename and text priv name.
 *		current_user is assumed
 */
Datum
pg_has_role_name(PG_FUNCTION_ARGS)
{














}

/*
 * pg_has_role_name_id
 *		Check user privileges on a role given
 *		name usename, role oid, and text priv name.
 */
Datum
pg_has_role_name_id(PG_FUNCTION_ARGS)
{













}

/*
 * pg_has_role_id
 *		Check user privileges on a role given
 *		role oid, and text priv name.
 *		current_user is assumed
 */
Datum
pg_has_role_id(PG_FUNCTION_ARGS)
{












}

/*
 * pg_has_role_id_name
 *		Check user privileges on a role given
 *		roleid, name rolename, and text priv name.
 */
Datum
pg_has_role_id_name(PG_FUNCTION_ARGS)
{













}

/*
 * pg_has_role_id_id
 *		Check user privileges on a role given
 *		roleid, role oid, and text priv name.
 */
Datum
pg_has_role_id_id(PG_FUNCTION_ARGS)
{











}

/*
 *		Support routines for pg_has_role family.
 */

/*
 * convert_role_priv_string
 *		Convert text string to AclMode value.
 *
 * We use USAGE to denote whether the privileges of the role are accessible
 * (has_privs), MEMBER to denote is_member, and MEMBER WITH GRANT OPTION
 * (or ADMIN OPTION) to denote is_admin.  There is no ACL bit corresponding
 * to MEMBER so we cheat and use ACL_CREATE for that.  This convention
 * is shared only with pg_role_aclcheck, below.
 */
static AclMode
convert_role_priv_string(text *priv_type_text)
{



}

/*
 * pg_role_aclcheck
 *		Quick-and-dirty support for pg_has_role
 */
static AclResult
pg_role_aclcheck(Oid role_oid, Oid roleid, AclMode mode)
{



























}

/*
 * initialization function (called by InitPostgres)
 */
void
initialize_acl(void)
{










}

/*
 * RoleMembershipCacheCallback
 *		Syscache inval callback function
 */
static void
RoleMembershipCacheCallback(Datum arg, int cacheid, uint32 hashvalue)
{



}

/* Check if specified role has rolinherit set */
static bool
has_rolinherit(Oid roleid)
{










}

/*
 * Get a list of roles that the specified roleid has the privileges of
 *
 * This is defined not to recurse through roles that don't have rolinherit
 * set; for such roles, membership implies the ability to do SET ROLE, but
 * the privileges are not available until you've done so.
 *
 * Since indirect membership testing is relatively expensive, we cache
 * a list of memberships.  Hence, the result is only guaranteed good until
 * the next call of roles_has_privs_of()!
 *
 * For the benefit of select_best_grantor, the result is defined to be
 * in breadth-first order, ie, closer relationships earlier.
 */
static List *
roles_has_privs_of(Oid roleid)
{







































































}

/*
 * Get a list of roles that the specified roleid is a member of
 *
 * This is defined to recurse through roles regardless of rolinherit.
 *
 * Since indirect membership testing is relatively expensive, we cache
 * a list of memberships.  Hence, the result is only guaranteed good until
 * the next call of roles_is_member_of()!
 */
static List *
roles_is_member_of(Oid roleid)
{

































































}

/*
 * Does member have the privileges of role (directly or indirectly)?
 *
 * This is defined not to recurse through roles that don't have rolinherit
 * set; for such roles, membership implies the ability to do SET ROLE, but
 * the privileges are not available until you've done so.
 */
bool
has_privs_of_role(Oid member, Oid role)
{

















}

/*
 * Is member a member of role (directly or indirectly)?
 *
 * This is defined to recurse through roles regardless of rolinherit.
 */
bool
is_member_of_role(Oid member, Oid role)
{

















}

/*
 * check_is_member_of_role
 *		is_member_of_role with a standard permission-violation error if
 *not
 */
void
check_is_member_of_role(Oid member, Oid role)
{




}

/*
 * Is member a member of role, not considering superuserness?
 *
 * This is identical to is_member_of_role except we ignore superuser
 * status.
 */
bool
is_member_of_role_nosuper(Oid member, Oid role)
{











}

/*
 * Is member an admin of role?	That is, is member the role itself (subject to
 * restrictions below), a member (directly or indirectly) WITH ADMIN OPTION,
 * or a superuser?
 */
bool
is_admin_of_role(Oid member, Oid role)
{


















































































}

/* does what it says ... */
static int
count_one_bits(AclMode mask)
{












}

/*
 * Select the effective grantor ID for a GRANT or REVOKE operation.
 *
 * The grantor must always be either the object owner or some role that has
 * been explicitly granted grant options.  This ensures that all granted
 * privileges appear to flow from the object owner, and there are never
 * multiple "original sources" of a privilege.  Therefore, if the would-be
 * grantor is a member of a role that has the needed grant options, we have
 * to do the grant as that role instead.
 *
 * It is possible that the would-be grantor is a member of several roles
 * that have different subsets of the desired grant options, but no one
 * role has 'em all.  In this case we pick a role with the largest number
 * of desired options.  Ties are broken in favor of closer ancestors.
 *
 * roleId: the role attempting to do the GRANT/REVOKE
 * privileges: the privileges to be granted/revoked
 * acl: the ACL of the object in question
 * ownerId: the role owning the object in question
 * *grantorId: receives the OID of the role to do the grant as
 * *grantOptions: receives the grant options actually held by grantorId
 *
 * If no grant options exist, we set grantorId to roleId, grantOptions to 0.
 */
void
select_best_grantor(Oid roleId, AclMode privileges, const Acl *acl, Oid ownerId, Oid *grantorId, AclMode *grantOptions)
{





























































}

/*
 * get_role_oid - Given a role name, look up the role's OID.
 *
 * If missing_ok is false, throw an error if role name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_role_oid(const char *rolname, bool missing_ok)
{








}

/*
 * get_role_oid_or_public - As above, but return ACL_ID_PUBLIC if the
 *		role name is "public".
 */
Oid
get_role_oid_or_public(const char *rolname)
{






}

/*
 * Given a RoleSpec node, return the OID it corresponds to.  If missing_ok is
 * true, return InvalidOid if the role does not exist.
 *
 * PUBLIC is always disallowed here.  Routines wanting to handle the PUBLIC
 * case must check the case separately.
 */
Oid
get_rolespec_oid(const RoleSpec *role, bool missing_ok)
{



























}

/*
 * Given a RoleSpec node, return the pg_authid HeapTuple it corresponds to.
 * Caller must ReleaseSysCache when done with the result tuple.
 */
HeapTuple
get_rolespec_tuple(const RoleSpec *role)
{







































}

/*
 * Given a RoleSpec, returns a palloc'ed copy of the corresponding role's name.
 */
char *
get_rolespec_name(const RoleSpec *role)
{










}

/*
 * Given a RoleSpec, throw an error if the name is reserved, using detail_msg,
 * if provided (which must be already translated).
 *
 * If node is NULL, no error is thrown.  If detail_msg is NULL then no detail
 * message is provided.
 */
void
check_rolespec_name(const RoleSpec *role, const char *detail_msg)
{





















}