/*-------------------------------------------------------------------------
 *
 * execJunk.c
 *	  Junk attribute support stuff....
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execJunk.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "executor/executor.h"

/*-------------------------------------------------------------------------
 *		XXX this stuff should be rewritten to take advantage
 *			of ExecProject() and the ProjectionInfo node.
 *			-cim 6/3/91
 *
 * An attribute of a tuple living inside the executor, can be
 * either a normal attribute or a "junk" attribute. "junk" attributes
 * never make it out of the executor, i.e. they are never printed,
 * returned or stored on disk. Their only purpose in life is to
 * store some information useful only to the executor, mainly the values
 * of system attributes like "ctid", or sort key columns that are not to
 * be output.
 *
 * The general idea is the following: A target list consists of a list of
 * TargetEntry nodes containing expressions. Each TargetEntry has a field
 * called 'resjunk'. If the value of this field is true then the
 * corresponding attribute is a "junk" attribute.
 *
 * When we initialize a plan we call ExecInitJunkFilter to create a filter.
 *
 * We then execute the plan, treating the resjunk attributes like any others.
 *
 * Finally, when at the top level we get back a tuple, we can call
 * ExecFindJunkAttribute/ExecGetJunkAttribute to retrieve the values of the
 * junk attributes we are interested in, and ExecFilterJunk to remove all the
 * junk attributes from a tuple.  This new "clean" tuple is then printed,
 * inserted, or updated.
 *
 *-------------------------------------------------------------------------
 */

/*
 * ExecInitJunkFilter
 *
 * Initialize the Junk filter.
 *
 * The source targetlist is passed in.  The output tuple descriptor is
 * built from the non-junk tlist entries.
 * An optional resultSlot can be passed as well; otherwise, we create one.
 */
JunkFilter *
ExecInitJunkFilter(List *targetList, TupleTableSlot *slot)
{












}

/*
 * ExecInitJunkFilterInsertion
 *
 * Initialize a JunkFilter for insertions into a table.
 *
 * Here, we are given the target "clean" tuple descriptor rather than
 * inferring it from the targetlist.  Although the target descriptor can
 * contain deleted columns, that is not of concern here, since the targetlist
 * should contain corresponding NULL constants (cf. ExecCheckPlanOutput).
 * It is assumed that the caller has checked that the table's columns match up
 * with the non-junk columns of the targetlist.
 */
JunkFilter *
ExecInitJunkFilterInsertion(List *targetList, TupleDesc cleanTupType, TupleTableSlot *slot)
{





























































}

/*
 * ExecInitJunkFilterConversion
 *
 * Initialize a JunkFilter for rowtype conversions.
 *
 * Here, we are given the target "clean" tuple descriptor rather than
 * inferring it from the targetlist.  The target descriptor can contain
 * deleted columns.  It is assumed that the caller has checked that the
 * non-deleted columns match up with the non-junk columns of the targetlist.
 */
JunkFilter *
ExecInitJunkFilterConversion(List *targetList, TupleDesc cleanTupType, TupleTableSlot *slot)
{




































































}

/*
 * ExecFindJunkAttribute
 *
 * Locate the specified junk attribute in the junk filter's targetlist,
 * and return its resno.  Returns InvalidAttrNumber if not found.
 */
AttrNumber
ExecFindJunkAttribute(JunkFilter *junkfilter, const char *attrName)
{

}

/*
 * ExecFindJunkAttributeInTlist
 *
 * Find a junk attribute given a subplan's targetlist (not necessarily
 * part of a JunkFilter).
 */
AttrNumber
ExecFindJunkAttributeInTlist(List *targetlist, const char *attrName)
{














}

/*
 * ExecGetJunkAttribute
 *
 * Given a junk filter's input tuple (slot) and a junk attribute's number
 * previously found by ExecFindJunkAttribute, extract & return the value and
 * isNull flag of the attribute.
 */
Datum
ExecGetJunkAttribute(TupleTableSlot *slot, AttrNumber attno, bool *isNull)
{



}

/*
 * ExecFilterJunk
 *
 * Construct and return a slot with all the junk attributes removed.
 */
TupleTableSlot *
ExecFilterJunk(JunkFilter *junkfilter, TupleTableSlot *slot)
{























































}