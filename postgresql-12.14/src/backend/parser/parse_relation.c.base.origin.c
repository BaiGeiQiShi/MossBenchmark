/*-------------------------------------------------------------------------
 *
 * parse_relation.c
 *	  parser support routines dealing with relations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_relation.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "parser/parse_enr.h"
#include "parser/parse_relation.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

#define MAX_FUZZY_DISTANCE 3

static RangeTblEntry *
scanNameSpaceForRefname(ParseState *pstate, const char *refname, int location);
static RangeTblEntry *
scanNameSpaceForRelid(ParseState *pstate, Oid relid, int location);
static void
check_lateral_ref_ok(ParseState *pstate, ParseNamespaceItem *nsitem, int location);
static void
markRTEForSelectPriv(ParseState *pstate, RangeTblEntry *rte, int rtindex, AttrNumber col);
static void
expandRelation(Oid relid, Alias *eref, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars);
static void
expandTupleDesc(TupleDesc tupdesc, Alias *eref, int count, int offset, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars);
static int
specialAttNum(const char *attname);
static bool
isQueryUsingTempRelation_walker(Node *node, void *context);

/*
 * refnameRangeTblEntry
 *	  Given a possibly-qualified refname, look to see if it matches any RTE.
 *	  If so, return a pointer to the RangeTblEntry; else return NULL.
 *
 *	  Optionally get RTE's nesting depth (0 = current) into *sublevels_up.
 *	  If sublevels_up is NULL, only consider items at the current nesting
 *	  level.
 *
 * An unqualified refname (schemaname == NULL) can match any RTE with matching
 * alias, or matching unqualified relname in the case of alias-less relation
 * RTEs.  It is possible that such a refname matches multiple RTEs in the
 * nearest nesting level that has a match; if so, we report an error via
 * ereport().
 *
 * A qualified refname (schemaname != NULL) can only match a relation RTE
 * that (a) has no alias and (b) is for the same relation identified by
 * schemaname.refname.  In this case we convert schemaname.refname to a
 * relation OID and search by relid, rather than by alias name.  This is
 * peculiar, but it's what SQL says to do.
 */
RangeTblEntry *
refnameRangeTblEntry(ParseState *pstate, const char *schemaname, const char *refname, int location, int *sublevels_up)
{





























































}

/*
 * Search the query's table namespace for an RTE matching the
 * given unqualified refname.  Return the RTE if a unique match, or NULL
 * if no match.  Raise error if multiple matches.
 *
 * Note: it might seem that we shouldn't have to worry about the possibility
 * of multiple matches; after all, the SQL standard disallows duplicate table
 * aliases within a given SELECT level.  Historically, however, Postgres has
 * been laxer than that.  For example, we allow
 *		SELECT ... FROM tab1 x CROSS JOIN (tab2 x CROSS JOIN tab3 y) z
 * on the grounds that the aliased join (z) hides the aliases within it,
 * therefore there is no conflict between the two RTEs named "x".  However,
 * if tab3 is a LATERAL subquery, then from within the subquery both "x"es
 * are visible.  Rather than rejecting queries that used to work, we allow
 * this situation, and complain only if there's actually an ambiguous
 * reference to "x".
 */
static RangeTblEntry *
scanNameSpaceForRefname(ParseState *pstate, const char *refname, int location)
{






























}

/*
 * Search the query's table namespace for a relation RTE matching the
 * given relation OID.  Return the RTE if a unique match, or NULL
 * if no match.  Raise error if multiple matches.
 *
 * See the comments for refnameRangeTblEntry to understand why this
 * acts the way it does.
 */
static RangeTblEntry *
scanNameSpaceForRelid(ParseState *pstate, Oid relid, int location)
{































}

/*
 * Search the query's CTE namespace for a CTE matching the given unqualified
 * refname.  Return the CTE (and its levelsup count) if a match, or NULL
 * if no match.  We need not worry about multiple matches, since parse_cte.c
 * rejects WITH lists containing duplicate CTE names.
 */
CommonTableExpr *
scanNameSpaceForCTE(ParseState *pstate, const char *refname, Index *ctelevelsup)
{


















}

/*
 * Search for a possible "future CTE", that is one that is not yet in scope
 * according to the WITH scoping rules.  This has nothing to do with valid
 * SQL semantics, but it's important for error reporting purposes.
 */
static bool
isFutureCTE(ParseState *pstate, const char *refname)
{















}

/*
 * Search the query's ephemeral named relation namespace for a relation
 * matching the given unqualified refname.
 */
bool
scanNameSpaceForENR(ParseState *pstate, const char *refname)
{

}

/*
 * searchRangeTableForRel
 *	  See if any RangeTblEntry could possibly match the RangeVar.
 *	  If so, return a pointer to the RangeTblEntry; else return NULL.
 *
 * This is different from refnameRangeTblEntry in that it considers every
 * entry in the ParseState's rangetable(s), not only those that are currently
 * visible in the p_namespace list(s).  This behavior is invalid per the SQL
 * spec, and it may give ambiguous results (there might be multiple equally
 * valid matches, but only one will be returned).  This must be used ONLY
 * as a heuristic in giving suitable error messages.  See errorMissingRTE.
 *
 * Notice that we consider both matches on actual relation (or CTE) name
 * and matches on alias.
 */
static RangeTblEntry *
searchRangeTableForRel(ParseState *pstate, RangeVar *relation)
{





























































}

/*
 * Check for relation-name conflicts between two namespace lists.
 * Raise an error if any is found.
 *
 * Note: we assume that each given argument does not contain conflicts
 * itself; we just want to know if the two can be merged together.
 *
 * Per SQL, two alias-less plain relation RTEs do not conflict even if
 * they have the same eref->aliasname (ie, same relation name), if they
 * are for different relation OIDs (implying they are in different schemas).
 *
 * We ignore the lateral-only flags in the namespace items: the lists must
 * not conflict, even when all items are considered visible.  However,
 * columns-only items should be ignored.
 */
void
checkNameSpaceConflicts(ParseState *pstate, List *namespace1, List *namespace2)
{


































}

/*
 * Complain if a namespace item is currently disallowed as a LATERAL reference.
 * This enforces both SQL:2008's rather odd idea of what to do with a LATERAL
 * reference to the wrong side of an outer join, and our own prohibition on
 * referencing the target table of an UPDATE or DELETE as a lateral reference
 * in a FROM/USING clause.
 *
 * Convenience subroutine to avoid multiple copies of a rather ugly ereport.
 */
static void
check_lateral_ref_ok(ParseState *pstate, ParseNamespaceItem *nsitem, int location)
{








}

/*
 * given an RTE, return RT index (starting with 1) of the entry,
 * and optionally get its nesting depth (0 = current).  If sublevels_up
 * is NULL, only consider rels at the current nesting level.
 * Raises error if RTE not found.
 */
int
RTERangeTablePosn(ParseState *pstate, RangeTblEntry *rte, int *sublevels_up)
{
































}

/*
 * Given an RT index and nesting depth, find the corresponding RTE.
 * This is the inverse of RTERangeTablePosn.
 */
RangeTblEntry *
GetRTEByRangeTablePosn(ParseState *pstate, int varno, int sublevels_up)
{







}

/*
 * Fetch the CTE for a CTE-reference RTE.
 *
 * rtelevelsup is the number of query levels above the given pstate that the
 * RTE came from.  Callers that don't have this information readily available
 * may pass -1 instead.
 */
CommonTableExpr *
GetCTEForRTE(ParseState *pstate, RangeTblEntry *rte, int rtelevelsup)
{































}

/*
 * updateFuzzyAttrMatchState
 *	  Using Levenshtein distance, consider if column is best fuzzy match.
 */
static void
updateFuzzyAttrMatchState(int fuzzy_rte_penalty, FuzzyAttrMatchState *fuzzystate, RangeTblEntry *rte, const char *actual, const char *match, int attnum)
{






















































































}

/*
 * scanRTEForColumn
 *	  Search the column names of a single RTE for the given name.
 *	  If found, return an appropriate Var node, else return NULL.
 *	  If the name proves ambiguous within this RTE, raise error.
 *
 * Side effect: if we find a match, mark the RTE as requiring read access
 * for the column.
 *
 * Additional side effect: if fuzzystate is non-NULL, check non-system columns
 * for an approximate match and update fuzzystate accordingly.
 */
Node *
scanRTEForColumn(ParseState *pstate, RangeTblEntry *rte, const char *colname, int location, int fuzzy_rte_penalty, FuzzyAttrMatchState *fuzzystate)
{



























































































}

/*
 * colNameToVar
 *	  Search for an unqualified column name.
 *	  If found, return the appropriate Var node (or expression).
 *	  If not found, return NULL.  If the name proves ambiguous, raise error.
 *	  If localonly is true, only names in the innermost query are
 *considered.
 */
Node *
colNameToVar(ParseState *pstate, const char *colname, bool localonly, int location)
{















































}

/*
 * searchRangeTableForCol
 *	  See if any RangeTblEntry could possibly provide the given column name
 *(or find the best match available).  Returns state with relevant details.
 *
 * This is different from colNameToVar in that it considers every entry in
 * the ParseState's rangetable(s), not only those that are currently visible
 * in the p_namespace list(s).  This behavior is invalid per the SQL spec,
 * and it may give ambiguous results (there might be multiple equally valid
 * matches, but only one will be returned).  This must be used ONLY as a
 * heuristic in giving suitable error messages.  See errorMissingColumn.
 *
 * This function is also different in that it will consider approximate
 * matches -- if the user entered an alias/column pair that is only slightly
 * different from a valid pair, we may be able to infer what they meant to
 * type and provide a reasonable hint.
 *
 * The FuzzyAttrMatchState will have 'rfirst' pointing to the best RTE
 * containing the most promising match for the alias and column name.  If
 * the alias and column names match exactly, 'first' will be InvalidAttrNumber;
 * otherwise, it will be the attribute number for the match.  In the latter
 * case, 'rsecond' may point to a second, equally close approximate match,
 * and 'second' will contain the attribute number for the second match.
 */
static FuzzyAttrMatchState *
searchRangeTableForCol(ParseState *pstate, const char *alias, const char *colname, int location)
{


























































}

/*
 * markRTEForSelectPriv
 *	   Mark the specified column of an RTE as requiring SELECT privilege
 *
 * col == InvalidAttrNumber means a "whole row" reference
 *
 * The caller should pass the actual RTE if it has it handy; otherwise pass
 * NULL, and we'll look it up here.  (This uglification of the API is
 * worthwhile because nearly all external callers have the RTE at hand.)
 */
static void
markRTEForSelectPriv(ParseState *pstate, RangeTblEntry *rte, int rtindex, AttrNumber col)
{




























































































}

/*
 * markVarForSelectPriv
 *	   Mark the RTE referenced by a Var as requiring SELECT privilege
 *
 * The caller should pass the Var's referenced RTE if it has it handy
 * (nearly all do); otherwise pass NULL.
 */
void
markVarForSelectPriv(ParseState *pstate, Var *var, RangeTblEntry *rte)
{









}

/*
 * buildRelationAliases
 *		Construct the eref column name list for a relation RTE.
 *		This code is also used for function RTEs.
 *
 * tupdesc: the physical column information
 * alias: the user-supplied alias, or NULL if none
 * eref: the eref Alias to store column names in
 *
 * eref->colnames is filled in.  Also, alias->colnames is rebuilt to insert
 * empty strings for any dropped columns, so that it will be one-to-one with
 * physical column numbers.
 *
 * It is an error for there to be more aliases present than required.
 */
static void
buildRelationAliases(TupleDesc tupdesc, Alias *alias, Alias *eref)
{

























































}

/*
 * chooseScalarFunctionAlias
 *		Select the column alias for a function in a function RTE,
 *		when the function returns a scalar type (not composite or
 *RECORD).
 *
 * funcexpr: transformed expression tree for the function call
 * funcname: function name (as determined by FigureColname)
 * alias: the user-supplied alias for the RTE, or NULL if none
 * nfuncs: the number of functions appearing in the function RTE
 *
 * Note that the name we choose might be overridden later, if the user-given
 * alias includes column alias names.  That's of no concern here.
 */
static char *
chooseScalarFunctionAlias(Node *funcexpr, char *funcname, Alias *alias, int nfuncs)
{





























}

/*
 * Open a table during parse analysis
 *
 * This is essentially just the same as table_openrv(), except that it caters
 * to some parser-specific error reporting needs, notably that it arranges
 * to include the RangeVar's parse location in any resulting error.
 *
 * Note: properly, lockmode should be declared LOCKMODE not int, but that
 * would require importing storage/lock.h into parse_relation.h.  Since
 * LOCKMODE is typedef'd as int anyway, that seems like overkill.
 */
Relation
parserOpenTable(ParseState *pstate, const RangeVar *relation, int lockmode)
{































}

/*
 * Add an entry for a relation to the pstate's range table (p_rtable).
 *
 * Note: formerly this checked for refname conflicts, but that's wrong.
 * Caller is responsible for checking for conflicts in the appropriate scope.
 */
RangeTblEntry *
addRangeTableEntry(ParseState *pstate, RangeVar *relation, Alias *alias, bool inh, bool inFromCl)
{


































































}

/*
 * Add an entry for a relation to the pstate's range table (p_rtable).
 *
 * This is just like addRangeTableEntry() except that it makes an RTE
 * given an already-open relation instead of a RangeVar reference.
 *
 * lockmode is the lock type required for query execution; it must be one
 * of AccessShareLock, RowShareLock, or RowExclusiveLock depending on the
 * RTE's role within the query.  The caller must hold that lock mode
 * or a stronger one.
 *
 * Note: properly, lockmode should be declared LOCKMODE not int, but that
 * would require importing storage/lock.h into parse_relation.h.  Since
 * LOCKMODE is typedef'd as int anyway, that seems like overkill.
 */
RangeTblEntry *
addRangeTableEntryForRelation(ParseState *pstate, Relation rel, int lockmode, Alias *alias, bool inh, bool inFromCl)
{













































}

/*
 * Add an entry for a subquery to the pstate's range table (p_rtable).
 *
 * This is just like addRangeTableEntry() except that it makes a subquery RTE.
 * Note that an alias clause *must* be supplied.
 */
RangeTblEntry *
addRangeTableEntryForSubquery(ParseState *pstate, Query *subquery, Alias *alias, bool lateral, bool inFromCl)
{


































































}

/*
 * Add an entry for a function (or functions) to the pstate's range table
 * (p_rtable).
 *
 * This is just like addRangeTableEntry() except that it makes a function RTE.
 */
RangeTblEntry *
addRangeTableEntryForFunction(ParseState *pstate, List *funcnames, List *funcexprs, List *coldeflists, RangeFunction *rangefunc, bool lateral, bool inFromCl)
{







































































































































































































































}

/*
 * Add an entry for a table function to the pstate's range table (p_rtable).
 *
 * This is much like addRangeTableEntry() except that it makes a tablefunc RTE.
 */
RangeTblEntry *
addRangeTableEntryForTableFunc(ParseState *pstate, TableFunc *tf, Alias *alias, bool lateral, bool inFromCl)
{

































































}

/*
 * Add an entry for a VALUES list to the pstate's range table (p_rtable).
 *
 * This is much like addRangeTableEntry() except that it makes a values RTE.
 */
RangeTblEntry *
addRangeTableEntryForValues(ParseState *pstate, List *exprs, List *coltypes, List *coltypmods, List *colcollations, Alias *alias, bool lateral, bool inFromCl)
{




























































}

/*
 * Add an entry for a join to the pstate's range table (p_rtable).
 *
 * This is much like addRangeTableEntry() except that it makes a join RTE.
 */
RangeTblEntry *
addRangeTableEntryForJoin(ParseState *pstate, List *colnames, JoinType jointype, List *aliasvars, Alias *alias, bool inFromCl)
{





























































}

/*
 * Add an entry for a CTE reference to the pstate's range table (p_rtable).
 *
 * This is much like addRangeTableEntry() except that it makes a CTE RTE.
 */
RangeTblEntry *
addRangeTableEntryForCTE(ParseState *pstate, CommonTableExpr *cte, Index levelsup, RangeVar *rv, bool inFromCl)
{





























































































}

/*
 * Add an entry for an ephemeral named relation reference to the pstate's
 * range table (p_rtable).
 *
 * It is expected that the RangeVar, which up until now is only known to be an
 * ephemeral named relation, will (in conjunction with the QueryEnvironment in
 * the ParseState), create a RangeTblEntry for a specific *kind* of ephemeral
 * named relation, based on enrtype.
 *
 * This is much like addRangeTableEntry() except that it makes an RTE for an
 * ephemeral named relation.
 */
RangeTblEntry *
addRangeTableEntryForENR(ParseState *pstate, RangeVar *rv, bool inFromCl)
{






















































































}

/*
 * Has the specified refname been selected FOR UPDATE/FOR SHARE?
 *
 * This is used when we have not yet done transformLockingClause, but need
 * to know the correct lock to take during initial opening of relations.
 *
 * Note: we pay no attention to whether it's FOR UPDATE vs FOR SHARE,
 * since the table-level lock is the same either way.
 */
bool
isLockedRefname(ParseState *pstate, const char *refname)
{





































}

/*
 * Add the given RTE as a top-level entry in the pstate's join list
 * and/or namespace list.  (We assume caller has checked for any
 * namespace conflicts.)  The RTE is always marked as unconditionally
 * visible, that is, not LATERAL-only.
 *
 * Note: some callers know that they can find the new ParseNamespaceItem
 * at the end of the pstate->p_namespace list.  This is a bit ugly but not
 * worth complicating this function's signature for.
 */
void
addRTEtoQuery(ParseState *pstate, RangeTblEntry *rte, bool addToJoinList, bool addToRelNameSpace, bool addToVarNameSpace)
{




















}

/*
 * expandRTE -- expand the columns of a rangetable entry
 *
 * This creates lists of an RTE's column names (aliases if provided, else
 * real names) and Vars for each column.  Only user columns are considered.
 * If include_dropped is false then dropped columns are omitted from the
 * results.  If include_dropped is true then empty strings and NULL constants
 * (not Vars!) are returned for dropped columns.
 *
 * rtindex, sublevels_up, and location are the varno, varlevelsup, and location
 * values to use in the created Vars.  Ordinarily rtindex should match the
 * actual position of the RTE in its rangetable.
 *
 * The output lists go into *colnames and *colvars.
 * If only one of the two kinds of output list is needed, pass NULL for the
 * output pointer for the unwanted one.
 */
void
expandRTE(RangeTblEntry *rte, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars)
{






































































































































































































































































































}

/*
 * expandRelation -- expandRTE subroutine
 */
static void
expandRelation(Oid relid, Alias *eref, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars)
{






}

/*
 * expandTupleDesc -- expandRTE subroutine
 *
 * Generate names and/or Vars for the first "count" attributes of the tupdesc,
 * and append them to colnames/colvars.  "offset" is added to the varattno
 * that each Var would otherwise have, and we also skip the first "offset"
 * entries in eref->colnames.  (These provisions allow use of this code for
 * an individual composite-returning function in an RTE_FUNCTION RTE.)
 */
static void
expandTupleDesc(TupleDesc tupdesc, Alias *eref, int count, int offset, int rtindex, int sublevels_up, int location, bool include_dropped, List **colnames, List **colvars)
{








































































}

/*
 * expandRelAttrs -
 *	  Workhorse for "*" expansion: produce a list of targetentries
 *	  for the attributes of the RTE
 *
 * As with expandRTE, rtindex/sublevels_up determine the varno/varlevelsup
 * fields of the Vars produced, and location sets their location.
 * pstate->p_next_resno determines the resnos assigned to the TLEs.
 * The referenced columns are marked as requiring SELECT access.
 */
List *
expandRelAttrs(ParseState *pstate, RangeTblEntry *rte, int rtindex, int sublevels_up, int location)
{





























}

/*
 * get_rte_attribute_name
 *		Get an attribute name from a RangeTblEntry
 *
 * This is unlike get_attname() because we use aliases if available.
 * In particular, it will work on an RTE for a subselect or join, whereas
 * get_attname() only works on real relations.
 *
 * "*" is returned if the given attnum is InvalidAttrNumber --- this case
 * occurs when a Var represents a whole tuple of a relation.
 *
 * It is caller's responsibility to not call this on a dropped attribute.
 * (You will get some answer for such cases, but it might not be sensible.)
 */
char *
get_rte_attribute_name(RangeTblEntry *rte, AttrNumber attnum)
{



































}

/*
 * get_rte_attribute_type
 *		Get attribute type/typmod/collation information from a
 *RangeTblEntry
 */
void
get_rte_attribute_type(RangeTblEntry *rte, AttrNumber attnum, Oid *vartype, int32 *vartypmod, Oid *varcollid)
{






































































































































































}

/*
 * get_rte_attribute_is_dropped
 *		Check whether attempted attribute ref is to a dropped column
 */
bool
get_rte_attribute_is_dropped(RangeTblEntry *rte, AttrNumber attnum)
{



























































































































}

/*
 * Given a targetlist and a resno, return the matching TargetEntry
 *
 * Returns NULL if resno is not present in list.
 *
 * Note: we need to search, rather than just indexing with list_nth(),
 * because not all tlists are sorted by resno.
 */
TargetEntry *
get_tle_by_resno(List *tlist, AttrNumber resno)
{












}

/*
 * Given a Query and rangetable index, return relation's RowMarkClause if any
 *
 * Returns NULL if relation is not selected FOR UPDATE/SHARE
 */
RowMarkClause *
get_parse_rowmark(Query *qry, Index rtindex)
{












}

/*
 *	given relation and att name, return attnum of variable
 *
 *	Returns InvalidAttrNumber if the attr doesn't exist (or is dropped).
 *
 *	This should only be used if the relation is already
 *	table_open()'ed.  Use the cache version get_attnum()
 *	for access to non-opened relations.
 */
int
attnameAttNum(Relation rd, const char *attname, bool sysColOK)
{






















}

/* specialAttNum()
 *
 * Check attribute name to see if it is "special", e.g. "xmin".
 * - thomas 2000-02-07
 *
 * Note: this only discovers whether the name could be a system attribute.
 * Caller needs to ensure that it really is an attribute of the rel.
 */
static int
specialAttNum(const char *attname)
{








}

/*
 * given attribute id, return name of that attribute
 *
 *	This should only be used if the relation is already
 *	table_open()'ed.  Use the cache version get_atttype()
 *	for access to non-opened relations.
 */
const NameData *
attnumAttName(Relation rd, int attid)
{












}

/*
 * given attribute id, return type of that attribute
 *
 *	This should only be used if the relation is already
 *	table_open()'ed.  Use the cache version get_atttype()
 *	for access to non-opened relations.
 */
Oid
attnumTypeId(Relation rd, int attid)
{












}

/*
 * given attribute id, return collation of that attribute
 *
 *	This should only be used if the relation is already table_open()'ed.
 */
Oid
attnumCollationId(Relation rd, int attid)
{










}

/*
 * Generate a suitable error about a missing RTE.
 *
 * Since this is a very common type of error, we work rather hard to
 * produce a helpful message.
 */
void
errorMissingRTE(ParseState *pstate, RangeVar *relation)
{


































}

/*
 * Generate a suitable error about a missing column.
 *
 * Since this is a very common type of error, we work rather hard to
 * produce a helpful message.
 */
void
errorMissingColumn(ParseState *pstate, const char *relname, const char *colname, int location)
{











































}

/*
 * Examine a fully-parsed query, and return true iff any relation underlying
 * the query is a temporary relation (table, view, or materialized view).
 */
bool
isQueryUsingTempRelation(Query *query)
{

}

static bool
isQueryUsingTempRelation_walker(Node *node, void *context)
{































}