/*-------------------------------------------------------------------------
 *
 * lsyscache.c
 *	  Convenience routines for common queries in the system catalog cache.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/lsyscache.c
 *
 * NOTES
 *	  Eventually, the index information should go through here, too.
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "bootstrap/bootstrap.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_range.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_transform.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/* Hook for plugins to get control in get_attavgwidth() */
get_attavgwidth_hook_type get_attavgwidth_hook = NULL;

/*				---------- AMOP CACHES ----------
 */

/*
 * op_in_opfamily
 *
 *		Return t iff operator 'opno' is in operator family 'opfamily'.
 *
 * This function only considers search operators, not ordering operators.
 */
bool
op_in_opfamily(Oid opno, Oid opfamily)
{

}

/*
 * get_op_opfamily_strategy
 *
 *		Get the operator's strategy number within the specified
 *opfamily, or 0 if it's not a member of the opfamily.
 *
 * This function only considers search operators, not ordering operators.
 */
int
get_op_opfamily_strategy(Oid opno, Oid opfamily)
{













}

/*
 * get_op_opfamily_sortfamily
 *
 *		If the operator is an ordering operator within the specified
 *opfamily, return its amopsortfamily OID; else return InvalidOid.
 */
Oid
get_op_opfamily_sortfamily(Oid opno, Oid opfamily)
{













}

/*
 * get_op_opfamily_properties
 *
 *		Get the operator's strategy number and declared input data types
 *		within the specified opfamily.
 *
 * Caller should already have verified that opno is a member of opfamily,
 * therefore we raise an error if the tuple is not found.
 */
void
get_op_opfamily_properties(Oid opno, Oid opfamily, bool ordering_op, int *strategy, Oid *lefttype, Oid *righttype)
{













}

/*
 * get_opfamily_member
 *		Get the OID of the operator that implements the specified
 *strategy with the specified datatypes for the specified opfamily.
 *
 * Returns InvalidOid if there is no pg_amop entry for the given keys.
 */
Oid
get_opfamily_member(Oid opfamily, Oid lefttype, Oid righttype, int16 strategy)
{













}

/*
 * get_ordering_op_properties
 *		Given the OID of an ordering operator (a btree "<" or ">"
 *operator), determine its opfamily, its declared input datatype, and its
 *		strategy number (BTLessStrategyNumber or
 *BTGreaterStrategyNumber).
 *
 * Returns true if successful, false if no matching pg_amop entry exists.
 * (This indicates that the operator is not a valid ordering operator.)
 *
 * Note: the operator could be registered in multiple families, for example
 * if someone were to build a "reverse sort" opfamily.  This would result in
 * uncertainty as to whether "ORDER BY USING op" would default to NULLS FIRST
 * or NULLS LAST, as well as inefficient planning due to failure to match up
 * pathkeys that should be the same.  So we want a determinate result here.
 * Because of the way the syscache search works, we'll use the interpretation
 * associated with the opfamily with smallest OID, which is probably
 * determinate enough.  Since there is no longer any particularly good reason
 * to build reverse-sort opfamilies, it doesn't seem worth expending any
 * additional effort on ensuring consistency.
 */
bool
get_ordering_op_properties(Oid opno, Oid *opfamily, Oid *opcintype, int16 *strategy)
{












































}

/*
 * get_equality_op_for_ordering_op
 *		Get the OID of the datatype-specific btree equality operator
 *		associated with an ordering operator (a "<" or ">" operator).
 *
 * If "reverse" isn't NULL, also set *reverse to false if the operator is "<",
 * true if it's ">"
 *
 * Returns InvalidOid if no matching equality operator can be found.
 * (This indicates that the operator is not a valid ordering operator.)
 */
Oid
get_equality_op_for_ordering_op(Oid opno, bool *reverse)
{

















}

/*
 * get_ordering_op_for_equality_op
 *		Get the OID of a datatype-specific btree ordering operator
 *		associated with an equality operator.  (If there are multiple
 *		possibilities, assume any one will do.)
 *
 * This function is used when we have to sort data before unique-ifying,
 * and don't much care which sorting op is used as long as it's compatible
 * with the intended equality operator.  Since we need a sorting operator,
 * it should be single-data-type even if the given operator is cross-type.
 * The caller specifies whether to find an op for the LHS or RHS data type.
 *
 * Returns InvalidOid if no matching ordering operator can be found.
 */
Oid
get_ordering_op_for_equality_op(Oid opno, bool use_lhs_type)
{







































}

/*
 * get_mergejoin_opfamilies
 *		Given a putatively mergejoinable operator, return a list of the
 *OIDs of the btree opfamilies in which it represents equality.
 *
 * It is possible (though at present unusual) for an operator to be equality
 * in more than one opfamily, hence the result is a list.  This also lets us
 * return NIL if the operator is not found in any opfamilies.
 *
 * The planner currently uses simple equal() tests to compare the lists
 * returned by this function, which makes the list order relevant, though
 * strictly speaking it should not be.  Because of the way syscache list
 * searches are handled, in normal operation the result will be sorted by OID
 * so everything works fine.  If running with system index usage disabled,
 * the result ordering is unspecified and hence the planner might fail to
 * recognize optimization opportunities ... but that's hardly a scenario in
 * which performance is good anyway, so there's no point in expending code
 * or cycles here to guarantee the ordering in that case.
 */
List *
get_mergejoin_opfamilies(Oid opno)
{

























}

/*
 * get_compatible_hash_operators
 *		Get the OID(s) of hash equality operator(s) compatible with the
 *given operator, but operating on its LHS and/or RHS datatype.
 *
 * An operator for the LHS type is sought and returned into *lhs_opno if
 * lhs_opno isn't NULL.  Similarly, an operator for the RHS type is sought
 * and returned into *rhs_opno if rhs_opno isn't NULL.
 *
 * If the given operator is not cross-type, the results should be the same
 * operator, but in cross-type situations they will be different.
 *
 * Returns true if able to find the requested operator(s), false if not.
 * (This indicates that the operator should not have been marked oprcanhash.)
 */
bool
get_compatible_hash_operators(Oid opno, Oid *lhs_opno, Oid *rhs_opno)
{




















































































}

/*
 * get_op_hash_functions
 *		Get the OID(s) of the standard hash support function(s)
 *compatible with the given operator, operating on its LHS and/or RHS datatype
 *as required.
 *
 * A function for the LHS type is sought and returned into *lhs_procno if
 * lhs_procno isn't NULL.  Similarly, a function for the RHS type is sought
 * and returned into *rhs_procno if rhs_procno isn't NULL.
 *
 * If the given operator is not cross-type, the results should be the same
 * function, but in cross-type situations they will be different.
 *
 * Returns true if able to find the requested function(s), false if not.
 * (This indicates that the operator should not have been marked oprcanhash.)
 */
bool
get_op_hash_functions(Oid opno, RegProcedure *lhs_procno, RegProcedure *rhs_procno)
{












































































}

/*
 * get_op_btree_interpretation
 *		Given an operator's OID, find out which btree opfamilies it
 *belongs to, and what properties it has within each one.  The results are
 *returned as a palloc'd list of OpBtreeInterpretation structs.
 *
 * In addition to the normal btree operators, we consider a <> operator to be
 * a "member" of an opfamily if its negator is an equality operator of the
 * opfamily.  ROWCOMPARE_NE is returned as the strategy number for this case.
 */
List *
get_op_btree_interpretation(Oid opno)
{




















































































}

/*
 * equality_ops_are_compatible
 *		Return true if the two given equality operators have compatible
 *		semantics.
 *
 * This is trivially true if they are the same operator.  Otherwise,
 * we look to see if they can be found in the same btree or hash opfamily.
 * Either finding allows us to assume that they have compatible notions
 * of equality.  (The reason we need to do these pushups is that one might
 * be a cross-type operator; for instance int24eq vs int4eq.)
 */
bool
equality_ops_are_compatible(Oid opno1, Oid opno2)
{



































}

/*				---------- AMPROC CACHES ----------
 */

/*
 * get_opfamily_proc
 *		Get the OID of the specified support function
 *		for the specified opfamily and datatypes.
 *
 * Returns InvalidOid if there is no pg_amproc entry for the given keys.
 */
Oid
get_opfamily_proc(Oid opfamily, Oid lefttype, Oid righttype, int16 procnum)
{













}

/*				---------- ATTRIBUTE CACHES ----------
 */

/*
 * get_attname
 *		Given the relation id and the attribute number, return the
 *"attname" field from the attribute relation as a palloc'ed string.
 *
 * If no such attribute exists and missing_ok is true, NULL is returned;
 * otherwise a not-intended-for-user-consumption error is thrown.
 */
char *
get_attname(Oid relid, AttrNumber attnum, bool missing_ok)
{


















}

/*
 * get_attnum
 *
 *		Given the relation id and the attribute name,
 *		return the "attnum" field from the attribute relation.
 *
 *		Returns InvalidAttrNumber if the attr doesn't exist (or is
 *dropped).
 */
AttrNumber
get_attnum(Oid relid, const char *attname)
{
















}

/*
 * get_attstattarget
 *
 *		Given the relation id and the attribute number,
 *		return the "attstattarget" field from the attribute relation.
 *
 *		Errors if not found.
 */
int
get_attstattarget(Oid relid, AttrNumber attnum)
{













}

/*
 * get_attgenerated
 *
 *		Given the relation id and the attribute number,
 *		return the "attgenerated" field from the attribute relation.
 *
 *		Errors if not found.
 *
 *		Since not generated is represented by '\0', this can also be
 *used as a Boolean test.
 */
char
get_attgenerated(Oid relid, AttrNumber attnum)
{













}

/*
 * get_atttype
 *
 *		Given the relation OID and the attribute number with the
 *relation, return the attribute type OID.
 */
Oid
get_atttype(Oid relid, AttrNumber attnum)
{
















}

/*
 * get_atttypetypmodcoll
 *
 *		A three-fer: given the relation id and the attribute number,
 *		fetch atttypid, atttypmod, and attcollation in a single cache
 *lookup.
 *
 * Unlike the otherwise-similar get_atttype, this routine
 * raises an error if it can't obtain the information.
 */
void
get_atttypetypmodcoll(Oid relid, AttrNumber attnum, Oid *typid, int32 *typmod, Oid *collid)
{














}

/*				---------- COLLATION CACHE ----------
 */

/*
 * get_collation_name
 *		Returns the name of a given pg_collation entry.
 *
 * Returns a palloc'd copy of the string, or NULL if no such collation.
 *
 * NOTE: since collation name is not unique, be wary of code that uses this
 * for anything except preparing error messages.
 */
char *
get_collation_name(Oid colloid)
{
















}

bool
get_collation_isdeterministic(Oid colloid)
{













}

/*				---------- CONSTRAINT CACHE ----------
 */

/*
 * get_constraint_name
 *		Returns the name of a given pg_constraint entry.
 *
 * Returns a palloc'd copy of the string, or NULL if no such constraint.
 *
 * NOTE: since constraint name is not unique, be wary of code that uses this
 * for anything except preparing error messages.
 */
char *
get_constraint_name(Oid conoid)
{
















}

/*				---------- LANGUAGE CACHE ----------
 */

char *
get_language_name(Oid langoid, bool missing_ok)
{


















}

/*				---------- OPCLASS CACHE ----------
 */

/*
 * get_opclass_family
 *
 *		Returns the OID of the operator family the opclass belongs to.
 */
Oid
get_opclass_family(Oid opclass)
{














}

/*
 * get_opclass_input_type
 *
 *		Returns the OID of the datatype the opclass indexes.
 */
Oid
get_opclass_input_type(Oid opclass)
{














}

/*
 * get_opclass_opfamily_and_input_type
 *
 *		Returns the OID of the operator family the opclass belongs to,
 *				the OID of the datatype the opclass indexes
 */
bool
get_opclass_opfamily_and_input_type(Oid opclass, Oid *opfamily, Oid *opcintype)
{

















}

/*				---------- OPERATOR CACHE ----------
 */

/*
 * get_opcode
 *
 *		Returns the regproc id of the routine used to implement an
 *		operator given the operator oid.
 */
RegProcedure
get_opcode(Oid opno)
{
















}

/*
 * get_opname
 *	  returns the name of the operator with the given opno
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such operator.
 */
char *
get_opname(Oid opno)
{
















}

/*
 * get_op_rettype
 *		Given operator oid, return the operator's result type.
 */
Oid
get_op_rettype(Oid opno)
{
















}

/*
 * op_input_types
 *
 *		Returns the left and right input datatypes for an operator
 *		(InvalidOid if not relevant).
 */
void
op_input_types(Oid opno, Oid *lefttype, Oid *righttype)
{












}

/*
 * op_mergejoinable
 *
 * Returns true if the operator is potentially mergejoinable.  (The planner
 * will fail to find any mergejoin plans unless there are suitable btree
 * opfamily entries for this operator and associated sortops.  The pg_operator
 * flag is just a hint to tell the planner whether to bother looking.)
 *
 * In some cases (currently only array_eq and record_eq), mergejoinability
 * depends on the specific input data type the operator is invoked for, so
 * that must be passed as well. We currently assume that only one input's type
 * is needed to check this --- by convention, pass the left input's data type.
 */
bool
op_mergejoinable(Oid opno, Oid inputtype)
{







































}

/*
 * op_hashjoinable
 *
 * Returns true if the operator is hashjoinable.  (There must be a suitable
 * hash opfamily entry for this operator if it is so marked.)
 *
 * In some cases (currently only array_eq), hashjoinability depends on the
 * specific input data type the operator is invoked for, so that must be
 * passed as well.  We currently assume that only one input's type is needed
 * to check this --- by convention, pass the left input's data type.
 */
bool
op_hashjoinable(Oid opno, Oid inputtype)
{



























}

/*
 * op_strict
 *
 * Get the proisstrict flag for the operator's underlying function.
 */
bool
op_strict(Oid opno)
{








}

/*
 * op_volatile
 *
 * Get the provolatile flag for the operator's underlying function.
 */
char
op_volatile(Oid opno)
{








}

/*
 * get_commutator
 *
 *		Returns the corresponding commutator of an operator.
 */
Oid
get_commutator(Oid opno)
{
















}

/*
 * get_negator
 *
 *		Returns the corresponding negator of an operator.
 */
Oid
get_negator(Oid opno)
{
















}

/*
 * get_oprrest
 *
 *		Returns procedure id for computing selectivity of an operator.
 */
RegProcedure
get_oprrest(Oid opno)
{
















}

/*
 * get_oprjoin
 *
 *		Returns procedure id for computing selectivity of a join.
 */
RegProcedure
get_oprjoin(Oid opno)
{
















}

/*				---------- FUNCTION CACHE ----------
 */

/*
 * get_func_name
 *	  returns the name of the function with the given funcid
 *
 * Note: returns a palloc'd copy of the string, or NULL if no such function.
 */
char *
get_func_name(Oid funcid)
{
















}

/*
 * get_func_namespace
 *
 *		Returns the pg_namespace OID associated with a given function.
 */
Oid
get_func_namespace(Oid funcid)
{
















}

/*
 * get_func_rettype
 *		Given procedure id, return the function's result type.
 */
Oid
get_func_rettype(Oid funcid)
{












}

/*
 * get_func_nargs
 *		Given procedure id, return the number of arguments.
 */
int
get_func_nargs(Oid funcid)
{












}

/*
 * get_func_signature
 *		Given procedure id, return the function's argument and result
 *types. (The return value is the result type.)
 *
 * The arguments are returned as a palloc'd array.
 */
Oid
get_func_signature(Oid funcid, Oid **argtypes, int *nargs)
{




















}

/*
 * get_func_variadictype
 *		Given procedure id, return the function's provariadic field.
 */
Oid
get_func_variadictype(Oid funcid)
{












}

/*
 * get_func_retset
 *		Given procedure id, return the function's proretset flag.
 */
bool
get_func_retset(Oid funcid)
{












}

/*
 * func_strict
 *		Given procedure id, return the function's proisstrict flag.
 */
bool
func_strict(Oid funcid)
{












}

/*
 * func_volatile
 *		Given procedure id, return the function's provolatile flag.
 */
char
func_volatile(Oid funcid)
{












}

/*
 * func_parallel
 *		Given procedure id, return the function's proparallel flag.
 */
char
func_parallel(Oid funcid)
{












}

/*
 * get_func_prokind
 *	   Given procedure id, return the routine kind.
 */
char
get_func_prokind(Oid funcid)
{












}

/*
 * get_func_leakproof
 *	   Given procedure id, return the function's leakproof field.
 */
bool
get_func_leakproof(Oid funcid)
{












}

/*
 * get_func_support
 *
 *		Returns the support function OID associated with a given
 *function, or InvalidOid if there is none.
 */
RegProcedure
get_func_support(Oid funcid)
{
















}

/*				---------- RELATION CACHE ----------
 */

/*
 * get_relname_relid
 *		Given name and namespace of a relation, look up the OID.
 *
 * Returns InvalidOid if there is no such relation.
 */
Oid
get_relname_relid(const char *relname, Oid relnamespace)
{

}

#ifdef NOT_USED
/*
 * get_relnatts
 *
 *		Returns the number of attributes for a given relation.
 */
int
get_relnatts(Oid relid)
{
  HeapTuple tp;

  tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_class reltup = (Form_pg_class)GETSTRUCT(tp);
    int result;

    result = reltup->relnatts;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidAttrNumber;
  }
}
#endif

/*
 * get_rel_name
 *		Returns the name of a given relation.
 *
 * Returns a palloc'd copy of the string, or NULL if no such relation.
 *
 * NOTE: since relation name is not unique, be wary of code that uses this
 * for anything except preparing error messages.
 */
char *
get_rel_name(Oid relid)
{
















}

/*
 * get_rel_namespace
 *
 *		Returns the pg_namespace OID associated with a given relation.
 */
Oid
get_rel_namespace(Oid relid)
{
















}

/*
 * get_rel_type_id
 *
 *		Returns the pg_type OID associated with a given relation.
 *
 * Note: not all pg_class entries have associated pg_type OIDs; so be
 * careful to check for InvalidOid result.
 */
Oid
get_rel_type_id(Oid relid)
{
















}

/*
 * get_rel_relkind
 *
 *		Returns the relkind associated with a given relation.
 */
char
get_rel_relkind(Oid relid)
{
















}

/*
 * get_rel_relispartition
 *
 *		Returns the relispartition flag associated with a given
 *relation.
 */
bool
get_rel_relispartition(Oid relid)
{
















}

/*
 * get_rel_tablespace
 *
 *		Returns the pg_tablespace OID associated with a given relation.
 *
 * Note: InvalidOid might mean either that we couldn't find the relation,
 * or that it is in the database's default tablespace.
 */
Oid
get_rel_tablespace(Oid relid)
{
















}

/*
 * get_rel_persistence
 *
 *		Returns the relpersistence associated with a given relation.
 */
char
get_rel_persistence(Oid relid)
{














}

/*				---------- TRANSFORM CACHE ----------
 */

Oid
get_transform_fromsql(Oid typid, Oid langid, List *trftypes)
{




















}

Oid
get_transform_tosql(Oid typid, Oid langid, List *trftypes)
{




















}

/*				---------- TYPE CACHE ----------
 */

/*
 * get_typisdefined
 *
 *		Given the type OID, determine whether the type is defined
 *		(if not, it's only a shell).
 */
bool
get_typisdefined(Oid typid)
{
















}

/*
 * get_typlen
 *
 *		Given the type OID, return the length of the type.
 */
int16
get_typlen(Oid typid)
{
















}

/*
 * get_typbyval
 *
 *		Given the type OID, determine whether the type is returned by
 *value or not.  Returns true if by value, false if by reference.
 */
bool
get_typbyval(Oid typid)
{
















}

/*
 * get_typlenbyval
 *
 *		A two-fer: given the type OID, return both typlen and typbyval.
 *
 *		Since both pieces of info are needed to know how to copy a
 *Datum, many places need both.  Might as well get them with one cache lookup
 *		instead of two.  Also, this routine raises an error instead of
 *		returning a bogus value when given a bad type OID.
 */
void
get_typlenbyval(Oid typid, int16 *typlen, bool *typbyval)
{












}

/*
 * get_typlenbyvalalign
 *
 *		A three-fer: given the type OID, return typlen, typbyval,
 *typalign.
 */
void
get_typlenbyvalalign(Oid typid, int16 *typlen, bool *typbyval, char *typalign)
{













}

/*
 * getTypeIOParam
 *		Given a pg_type row, select the type OID to pass to I/O
 *functions
 *
 * Formerly, all I/O functions were passed pg_type.typelem as their second
 * parameter, but we now have a more complex rule about what to pass.
 * This knowledge is intended to be centralized here --- direct references
 * to typelem elsewhere in the code are wrong, if they are associated with
 * I/O calls and not with actual subscripting operations!  (But see
 * bootstrap.c's boot_get_type_io_data() if you need to change this.)
 *
 * As of PostgreSQL 8.1, output functions receive only the value itself
 * and not any auxiliary parameters, so the name of this routine is now
 * a bit of a misnomer ... it should be getTypeInputParam.
 */
Oid
getTypeIOParam(HeapTuple typeTuple)
{














}

/*
 * get_type_io_data
 *
 *		A six-fer:	given the type OID, return typlen, typbyval,
 *typalign, typdelim, typioparam, and IO function OID. The IO function returned
 *is controlled by IOFuncSelector
 */
void
get_type_io_data(Oid typid, IOFuncSelector which_func, int16 *typlen, bool *typbyval, char *typalign, char *typdelim, Oid *typioparam, Oid *func)
{
























































}

#ifdef NOT_USED
char
get_typalign(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    char result;

    result = typtup->typalign;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return 'i';
  }
}
#endif

char
get_typstorage(Oid typid)
{
















}

/*
 * get_typdefault
 *	  Given a type OID, return the type's default value, if any.
 *
 *	  The result is a palloc'd expression node tree, or NULL if there
 *	  is no defined default for the datatype.
 *
 * NB: caller should be prepared to coerce result to correct datatype;
 * the returned expression tree might produce something of the wrong type.
 */
Node *
get_typdefault(Oid typid)
{




















































}

/*
 * getBaseType
 *		If the given type is a domain, return its base type;
 *		otherwise return the type's own OID.
 */
Oid
getBaseType(Oid typid)
{



}

/*
 * getBaseTypeAndTypmod
 *		If the given type is a domain, return its base type and typmod;
 *		otherwise return the type's own OID, and leave *typmod
 *unchanged.
 *
 * Note that the "applied typmod" should be -1 for every domain level
 * above the bottommost; therefore, if the passed-in typid is indeed
 * a domain, *typmod should be -1.
 */
Oid
getBaseTypeAndTypmod(Oid typid, int32 *typmod)
{





























}

/*
 * get_typavgwidth
 *
 *	  Given a type OID and a typmod value (pass -1 if typmod is unknown),
 *	  estimate the average width of values of the type.  This is used by
 *	  the planner, which doesn't require absolutely correct results;
 *	  it's OK (and expected) to guess if we don't know for sure.
 */
int32
get_typavgwidth(Oid typid, int32 typmod)
{
















































}

/*
 * get_typtype
 *
 *		Given the type OID, find if it is a basic type, a complex type,
 *etc. It returns the null char if the cache lookup fails...
 */
char
get_typtype(Oid typid)
{
















}

/*
 * type_is_rowtype
 *
 *		Convenience function to determine whether a type OID represents
 *		a "rowtype" type --- either RECORD or a named composite type
 *		(including a domain over a named composite type).
 */
bool
type_is_rowtype(Oid typid)
{


















}

/*
 * type_is_enum
 *	  Returns true if the given type is an enum type.
 */
bool
type_is_enum(Oid typid)
{

}

/*
 * type_is_range
 *	  Returns true if the given type is a range type.
 */
bool
type_is_range(Oid typid)
{

}

/*
 * get_type_category_preferred
 *
 *		Given the type OID, fetch its category and preferred-type
 *status. Throws error on failure.
 */
void
get_type_category_preferred(Oid typid, char *typcategory, bool *typispreferred)
{












}

/*
 * get_typ_typrelid
 *
 *		Given the type OID, get the typrelid (InvalidOid if not a
 *complex type).
 */
Oid
get_typ_typrelid(Oid typid)
{
















}

/*
 * get_element_type
 *
 *		Given the type OID, get the typelem (InvalidOid if not an array
 *type).
 *
 * NB: this only considers varlena arrays to be true arrays; InvalidOid is
 * returned if the input is a fixed-length array type.
 */
Oid
get_element_type(Oid typid)
{























}

/*
 * get_array_type
 *
 *		Given the type OID, get the corresponding "true" array type.
 *		Returns InvalidOid if no array type can be found.
 */
Oid
get_array_type(Oid typid)
{










}

/*
 * get_promoted_array_type
 *
 *		The "promoted" type is what you'd get from an ARRAY(SELECT ...)
 *		construct, that is, either the corresponding "true" array type
 *		if the input is a scalar type that has such an array type,
 *		or the same type if the input is already a "true" array type.
 *		Returns InvalidOid if neither rule is satisfied.
 */
Oid
get_promoted_array_type(Oid typid)
{











}

/*
 * get_base_element_type
 *		Given the type OID, get the typelem, looking "through" any
 *domain to its underlying array type.
 *
 * This is equivalent to get_element_type(getBaseType(typid)), but avoids
 * an extra cache lookup.  Note that it fails to provide any information
 * about the typmod of the array.
 */
Oid
get_base_element_type(Oid typid)
{






































}

/*
 * getTypeInputInfo
 *
 *		Get info needed for converting values of a type to internal form
 */
void
getTypeInputInfo(Oid type, Oid *typInput, Oid *typIOParam)
{























}

/*
 * getTypeOutputInfo
 *
 *		Get info needed for printing values of a type
 */
void
getTypeOutputInfo(Oid type, Oid *typOutput, bool *typIsVarlena)
{























}

/*
 * getTypeBinaryInputInfo
 *
 *		Get info needed for binary input of values of a type
 */
void
getTypeBinaryInputInfo(Oid type, Oid *typReceive, Oid *typIOParam)
{























}

/*
 * getTypeBinaryOutputInfo
 *
 *		Get info needed for binary output of values of a type
 */
void
getTypeBinaryOutputInfo(Oid type, Oid *typSend, bool *typIsVarlena)
{























}

/*
 * get_typmodin
 *
 *		Given the type OID, return the type's typmodin procedure, if
 *any.
 */
Oid
get_typmodin(Oid typid)
{
















}

#ifdef NOT_USED
/*
 * get_typmodout
 *
 *		Given the type OID, return the type's typmodout procedure, if
 *any.
 */
Oid
get_typmodout(Oid typid)
{
  HeapTuple tp;

  tp = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typid));
  if (HeapTupleIsValid(tp))
  {
    Form_pg_type typtup = (Form_pg_type)GETSTRUCT(tp);
    Oid result;

    result = typtup->typmodout;
    ReleaseSysCache(tp);
    return result;
  }
  else
  {
    return InvalidOid;
  }
}
#endif /* NOT_USED */

/*
 * get_typcollation
 *
 *		Given the type OID, return the type's typcollation attribute.
 */
Oid
get_typcollation(Oid typid)
{
















}

/*
 * type_is_collatable
 *
 *		Return whether the type cares about collations
 */
bool
type_is_collatable(Oid typid)
{

}

/*				---------- STATISTICS CACHE ----------
 */

/*
 * get_attavgwidth
 *
 *	  Given the table and attribute number of a column, get the average
 *	  width of entries in the column.  Return zero if no data available.
 *
 * Currently this is only consulted for individual tables, not for inheritance
 * trees, so we don't need an "inh" parameter.
 *
 * Calling a hook at this point looks somewhat strange, but is required
 * because the optimizer calls this function without any other way for
 * plug-ins to control the result.
 */
int32
get_attavgwidth(Oid relid, AttrNumber attnum)
{






















}

/*
 * get_attstatsslot
 *
 *		Extract the contents of a "slot" of a pg_statistic tuple.
 *		Returns true if requested slot type was found, else false.
 *
 * Unlike other routines in this file, this takes a pointer to an
 * already-looked-up tuple in the pg_statistic cache.  We do this since
 * most callers will want to extract more than one value from the cache
 * entry, and we don't want to repeat the cache lookup unnecessarily.
 * Also, this API allows this routine to be used with statistics tuples
 * that have been provided by a stats hook and didn't really come from
 * pg_statistic.
 *
 * sslot: pointer to output area (typically, a local variable in the caller).
 * statstuple: pg_statistic tuple to be examined.
 * reqkind: STAKIND code for desired statistics slot kind.
 * reqop: STAOP value wanted, or InvalidOid if don't care.
 * flags: bitmask of ATTSTATSSLOT_VALUES and/or ATTSTATSSLOT_NUMBERS.
 *
 * If a matching slot is found, true is returned, and *sslot is filled thus:
 * staop: receives the actual STAOP value.
 * stacoll: receives the actual STACOLL value.
 * valuetype: receives actual datatype of the elements of stavalues.
 * values: receives pointer to an array of the slot's stavalues.
 * nvalues: receives number of stavalues.
 * numbers: receives pointer to an array of the slot's stanumbers (as float4).
 * nnumbers: receives number of stanumbers.
 *
 * valuetype/values/nvalues are InvalidOid/NULL/0 if ATTSTATSSLOT_VALUES
 * wasn't specified.  Likewise, numbers/nnumbers are NULL/0 if
 * ATTSTATSSLOT_NUMBERS wasn't specified.
 *
 * If no matching slot is found, false is returned, and *sslot is zeroed.
 *
 * Note that the current API doesn't allow for searching for a slot with
 * a particular collation.  If we ever actually support recording more than
 * one collation, we'll have to extend the API, but for now simple is good.
 *
 * The data referred to by the fields of sslot is locally palloc'd and
 * is independent of the original pg_statistic tuple.  When the caller
 * is done with it, call free_attstatsslot to release the palloc'd data.
 *
 * If it's desirable to call free_attstatsslot when get_attstatsslot might
 * not have been called, memset'ing sslot to zeroes will allow that.
 */
bool
get_attstatsslot(AttStatsSlot *sslot, HeapTuple statstuple, int reqkind, Oid reqop, int flags)
{






























































































































}

/*
 * free_attstatsslot
 *		Free data allocated by get_attstatsslot
 */
void
free_attstatsslot(AttStatsSlot *sslot)
{















}

/*				---------- PG_NAMESPACE CACHE ----------
 */

/*
 * get_namespace_name
 *		Returns the name of a given namespace
 *
 * Returns a palloc'd copy of the string, or NULL if no such namespace.
 */
char *
get_namespace_name(Oid nspid)
{
















}

/*
 * get_namespace_name_or_temp
 *		As above, but if it is this backend's temporary namespace,
 *return "pg_temp" instead.
 */
char *
get_namespace_name_or_temp(Oid nspid)
{








}

/*				---------- PG_RANGE CACHE ----------
 */

/*
 * get_range_subtype
 *		Returns the subtype of a given range type
 *
 * Returns InvalidOid if the type is not a range type.
 */
Oid
get_range_subtype(Oid rangeOid)
{
















}

/*
 * get_range_collation
 *		Returns the collation of a given range type
 *
 * Returns InvalidOid if the type is not a range type,
 * or if its subtype is not collatable.
 */
Oid
get_range_collation(Oid rangeOid)
{
















}

/*				---------- PG_INDEX CACHE ----------
 */

/*
 * get_index_column_opclass
 *
 *		Given the index OID and column number,
 *		return opclass of the index column
 *			or InvalidOid if the index was not found
 *				or column is non-key one.
 */
Oid
get_index_column_opclass(Oid index_oid, int attno)
{






































}

/*
 * get_index_isreplident
 *
 *		Given the index OID, return pg_index.indisreplident.
 */
bool
get_index_isreplident(Oid index_oid)
{















}

/*
 * get_index_isvalid
 *
 *		Given the index OID, return pg_index.indisvalid.
 */
bool
get_index_isvalid(Oid index_oid)
{















}

/*
 * get_index_isclustered
 *
 *		Given the index OID, return pg_index.indisclustered.
 */
bool
get_index_isclustered(Oid index_oid)
{















}