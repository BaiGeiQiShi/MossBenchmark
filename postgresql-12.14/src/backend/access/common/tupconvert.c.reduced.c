/*-------------------------------------------------------------------------
 *
 * tupconvert.c
 *	  Tuple conversion support.
 *
 * These functions provide conversion between rowtypes that are logically
 * equivalent but might have columns in a different order or different sets of
 * dropped columns.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/common/tupconvert.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/tupconvert.h"
#include "executor/tuptable.h"
#include "utils/builtins.h"

/*
 * The conversion setup routines have the following common API:
 *
 * The setup routine checks whether the given source and destination tuple
 * descriptors are logically compatible.  If not, it throws an error.
 * If so, it returns NULL if they are physically compatible (ie, no conversion
 * is needed), else a TupleConversionMap that can be used by
 * execute_attr_map_tuple to perform the conversion.
 *
 * The TupleConversionMap, if needed, is palloc'd in the caller's memory
 * context.  Also, the given tuple descriptors are referenced by the map,
 * so they must survive as long as the map is needed.
 *
 * The caller must supply a suitable primary error message to be used if
 * a compatibility error is thrown.  Recommended coding practice is to use
 * gettext_noop() on this string, so that it is translatable but won't
 * actually be translated unless the error gets thrown.
 *
 *
 * Implementation notes:
 *
 * The key component of a TupleConversionMap is an attrMap[] array with
 * one entry per output column.  This entry contains the 1-based index of
 * the corresponding input column, or zero to force a NULL value (for
 * a dropped output column).  The TupleConversionMap also contains workspace
 * arrays.
 */

/*
 * Set up for tuple conversion, matching input and output columns by
 * position.  (Dropped columns are ignored in both input and output.)
 *
 * Note: the errdetail messages speak of indesc as the "returned" rowtype,
 * outdesc as the "expected" rowtype.  This is okay for current uses but
 * might need generalization in future.
 */
TupleConversionMap *
convert_tuples_by_position(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{


























































































































}

/*
 * Set up for tuple conversion, matching input and output columns by name.
 * (Dropped columns are ignored in both input and output.)	This is intended
 * for use when the rowtypes are related by inheritance, so we expect an exact
 * match of both type and typmod.  The error messages will be a bit unhelpful
 * unless both rowtypes are named composite types.
 */
TupleConversionMap *
convert_tuples_by_name(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{



























}

/*
 * Return a palloc'd bare attribute map for tuple conversion, matching input
 * and output columns by name.  (Dropped columns are ignored in both input and
 * output.)  This is normally a subroutine for convert_tuples_by_name, but can
 * be used standalone.
 */
AttrNumber *
convert_tuples_by_name_map(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{
  AttrNumber *attrMap;
  int outnatts;
  int innatts;
  int i;
  int nextindesc = -1;

  outnatts = outdesc->natts;
  innatts = indesc->natts;

  attrMap = (AttrNumber *)palloc0(outnatts * sizeof(AttrNumber));
  for (i = 0; i < outnatts; i++) {
    Form_pg_attribute outatt = TupleDescAttr(outdesc, i);
    char *attname;
    Oid atttypid;
    int32 atttypmod;
    int j;

    if (outatt->attisdropped) {

    attname = NameStr(outatt->attname);
    atttypid = outatt->atttypid;
    atttypmod = outatt->atttypmod;

    /*
     * Now search for an attribute with the same name in the indesc. It
     * seems likely that a partitioned table will have the attributes in
     * the same order as the partition, so the search below is optimized
     * for that case.  It is possible that columns are dropped in one of
     * the relations, but not the other, so we use the 'nextindesc'
     * counter to track the starting point of the search.  If the inner
     * loop encounters dropped columns then it will have to skip over
     * them, but it should leave 'nextindesc' at the correct position for
     * the next outer loop.
     */
    for (j = 0; j < innatts; j++) {
      Form_pg_attribute inatt;

      nextindesc++;
      if (nextindesc >= innatts) {


      inatt = TupleDescAttr(indesc, nextindesc);
      if (inatt->attisdropped) {

      if (strcmp(attname, NameStr(inatt->attname)) == 0) {
        /* Found it, check type */
        if (atttypid != inatt->atttypid || atttypmod != inatt->atttypmod) {

                                 attname, format_type_be(outdesc->tdtypeid), format_type_be(indesc->tdtypeid))));
        }
        attrMap[i] = inatt->attnum;
        break;
      }
    }
    if (attrMap[i] == 0) {

  }
  return attrMap;
}

/*
 * Returns mapping created by convert_tuples_by_name_map, or NULL if no
 * conversion not required. This is a convenience routine for
 * convert_tuples_by_name() and other functions.
 */
AttrNumber *
convert_tuples_by_name_map_if_req(TupleDesc indesc, TupleDesc outdesc, const char *msg)
{






















































}

/*
 * Perform conversion of a tuple according to the map.
 */
HeapTuple
execute_attr_map_tuple(HeapTuple tuple, TupleConversionMap *map)
{





























}

/*
 * Perform conversion of a tuple slot according to the map.
 */
TupleTableSlot *
execute_attr_map_slot(AttrNumber *attrMap, TupleTableSlot *in_slot, TupleTableSlot *out_slot)
{









































}

/*
 * Perform conversion of bitmap of columns according to the map.
 *
 * The input and output bitmaps are offset by
 * FirstLowInvalidHeapAttributeNumber to accommodate system cols, like the
 * column-bitmaps in RangeTblEntry.
 */
Bitmapset *
execute_attr_map_cols(Bitmapset *in_cols, TupleConversionMap *map)
{






































}

/*
 * Free a TupleConversionMap structure.
 */
void
free_conversion_map(TupleConversionMap *map)
{







}
