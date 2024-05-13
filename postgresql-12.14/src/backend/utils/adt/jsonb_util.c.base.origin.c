/*-------------------------------------------------------------------------
 *
 * jsonb_util.c
 *	  converting between Jsonb and JsonbValues, and iterating.
 *
 * Copyright (c) 2014-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonb_util.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/jsonb.h"
#include "utils/memutils.h"
#include "utils/varlena.h"

/*
 * Maximum number of elements in an array (or key/value pairs in an object).
 * This is limited by two things: the size of the JEntry array must fit
 * in MaxAllocSize, and the number of elements (or pairs) must fit in the bits
 * reserved for that in the JsonbContainer.header field.
 *
 * (The total size of an array's or object's elements is also limited by
 * JENTRY_OFFLENMASK, but we're not concerned about that here.)
 */
#define JSONB_MAX_ELEMS (Min(MaxAllocSize / sizeof(JsonbValue), JB_CMASK))
#define JSONB_MAX_PAIRS (Min(MaxAllocSize / sizeof(JsonbPair), JB_CMASK))

static void
fillJsonbValue(JsonbContainer *container, int index, char *base_addr, uint32 offset, JsonbValue *result);
static bool
equalsJsonbScalarValue(JsonbValue *a, JsonbValue *b);
static int
compareJsonbScalarValue(JsonbValue *a, JsonbValue *b);
static Jsonb *
convertToJsonb(JsonbValue *val);
static void
convertJsonbValue(StringInfo buffer, JEntry *header, JsonbValue *val, int level);
static void
convertJsonbArray(StringInfo buffer, JEntry *header, JsonbValue *val, int level);
static void
convertJsonbObject(StringInfo buffer, JEntry *header, JsonbValue *val, int level);
static void
convertJsonbScalar(StringInfo buffer, JEntry *header, JsonbValue *scalarVal);

static int
reserveFromBuffer(StringInfo buffer, int len);
static void
appendToBuffer(StringInfo buffer, const char *data, int len);
static void
copyToBuffer(StringInfo buffer, int offset, const char *data, int len);
static short
padBufferToInt(StringInfo buffer);

static JsonbIterator *
iteratorFromContainer(JsonbContainer *container, JsonbIterator *parent);
static JsonbIterator *
freeAndGetParent(JsonbIterator *it);
static JsonbParseState *
pushState(JsonbParseState **pstate);
static void
appendKey(JsonbParseState *pstate, JsonbValue *scalarVal);
static void
appendValue(JsonbParseState *pstate, JsonbValue *scalarVal);
static void
appendElement(JsonbParseState *pstate, JsonbValue *scalarVal);
static int
lengthCompareJsonbStringValue(const void *a, const void *b);
static int
lengthCompareJsonbPair(const void *a, const void *b, void *arg);
static void
uniqueifyJsonbObject(JsonbValue *object);
static JsonbValue *
pushJsonbValueScalar(JsonbParseState **pstate, JsonbIteratorToken seq, JsonbValue *scalarVal);

/*
 * Turn an in-memory JsonbValue into a Jsonb for on-disk storage.
 *
 * There isn't a JsonbToJsonbValue(), because generally we find it more
 * convenient to directly iterate through the Jsonb representation and only
 * really convert nested scalar values.  JsonbIteratorNext() does this, so that
 * clients of the iteration code don't have to directly deal with the binary
 * representation (JsonbDeepContains() is a notable exception, although all
 * exceptions are internal to this module).  In general, functions that accept
 * a JsonbValue argument are concerned with the manipulation of scalar values,
 * or simple containers of scalar values, where it would be inconvenient to
 * deal with a great amount of other state.
 */
Jsonb *
JsonbValueToJsonb(JsonbValue *val)
{
































}

/*
 * Get the offset of the variable-length portion of a Jsonb node within
 * the variable-length-data part of its container.  The node is identified
 * by index within the container's JEntry array.
 */
uint32
getJsonbOffset(const JsonbContainer *jc, int index)
{


















}

/*
 * Get the length of the variable-length portion of a Jsonb node.
 * The node is identified by index within the container's JEntry array.
 */
uint32
getJsonbLength(const JsonbContainer *jc, int index)
{



















}

/*
 * BT comparator worker function.  Returns an integer less than, equal to, or
 * greater than zero, indicating whether a is less than, equal to, or greater
 * than b.  Consistent with the requirements for a B-Tree operator class
 *
 * Strings are compared lexically, in contrast with other places where we use a
 * much simpler comparator logic for searching through Strings.  Since this is
 * called from B-Tree support function 1, we're careful about not leaking
 * memory here.
 */
int
compareJsonbContainers(JsonbContainer *a, JsonbContainer *b)
{
























































































































}

/*
 * Find value in object (i.e. the "value" part of some key/value pair in an
 * object), or find a matching element if we're looking through an array.  Do
 * so on the basis of equality of the object keys only, or alternatively
 * element values only, with a caller-supplied value "key".  The "flags"
 * argument allows the caller to specify which container types are of interest.
 *
 * This exported utility function exists to facilitate various cases concerned
 * with "containment".  If asked to look through an object, the caller had
 * better pass a Jsonb String, because their keys can only be strings.
 * Otherwise, for an array, any type of JsonbValue will do.
 *
 * In order to proceed with the search, it is necessary for callers to have
 * both specified an interest in exactly one particular container type with an
 * appropriate flag, as well as having the pointed-to Jsonb container be of
 * one of those same container types at the top level. (Actually, we just do
 * whichever makes sense to save callers the trouble of figuring it out - at
 * most one can make sense, because the container either points to an array
 * (possibly a "raw scalar" pseudo array) or an object.)
 *
 * Note that we can return a jbvBinary JsonbValue if this is called on an
 * object, but we never do so on an array.  If the caller asks to look through
 * a container type that is not of the type pointed to by the container,
 * immediately fall through and return NULL.  If we cannot find the value,
 * return NULL.  Otherwise, return palloc()'d copy of value.
 */
JsonbValue *
findJsonbValueFromContainer(JsonbContainer *container, uint32 flags, JsonbValue *key)
{





















































































}

/*
 * Get i-th value of a Jsonb array.
 *
 * Returns palloc()'d copy of the value, or NULL if it does not exist.
 */
JsonbValue *
getIthJsonbValueFromContainer(JsonbContainer *container, uint32 i)
{






















}

/*
 * A helper function to fill in a JsonbValue to represent an element of an
 * array, or a key or value of an object.
 *
 * The node's JEntry is at container->children[index], and its variable-length
 * data is at base_addr + offset.  We make the caller determine the offset
 * since in many cases the caller can amortize that work across multiple
 * children.  When it can't, it can just call getJsonbOffset().
 *
 * A nested array or object will be returned as jbvBinary, ie. it won't be
 * expanded.
 */
static void
fillJsonbValue(JsonbContainer *container, int index, char *base_addr, uint32 offset, JsonbValue *result)
{




































}

/*
 * Push JsonbValue into JsonbParseState.
 *
 * Used when parsing JSON tokens to form Jsonb, or when converting an in-memory
 * JsonbValue to a Jsonb.
 *
 * Initial state of *JsonbParseState is NULL, since it'll be allocated here
 * originally (caller will get JsonbParseState back by reference).
 *
 * Only sequential tokens pertaining to non-container types should pass a
 * JsonbValue.  There is one exception -- WJB_BEGIN_ARRAY callers may pass a
 * "raw scalar" pseudo array to append it - the actual scalar should be passed
 * next and it will be added as the only member of the array.
 *
 * Values of type jvbBinary, which are rolled up arrays and objects,
 * are unpacked before being added to the result.
 */
JsonbValue *
pushJsonbValue(JsonbParseState **pstate, JsonbIteratorToken seq, JsonbValue *jbval)
{



















}

/*
 * Do the actual pushing, with only scalar or pseudo-scalar-array values
 * accepted.
 */
static JsonbValue *
pushJsonbValueScalar(JsonbParseState **pstate, JsonbIteratorToken seq, JsonbValue *scalarVal)
{













































































}

/*
 * pushJsonbValue() worker:  Iteration-like forming of Jsonb
 */
static JsonbParseState *
pushState(JsonbParseState **pstate)
{




}

/*
 * pushJsonbValue() worker:  Append a pair key to state when generating a Jsonb
 */
static void
appendKey(JsonbParseState *pstate, JsonbValue *string)
{


















}

/*
 * pushJsonbValue() worker:  Append a pair value to state when generating a
 * Jsonb
 */
static void
appendValue(JsonbParseState *pstate, JsonbValue *scalarVal)
{





}

/*
 * pushJsonbValue() worker:  Append an element to state when generating a Jsonb
 */
static void
appendElement(JsonbParseState *pstate, JsonbValue *scalarVal)
{
















}

/*
 * Given a JsonbContainer, expand to JsonbIterator to iterate over items
 * fully expanded to in-memory representation for manipulation.
 *
 * See JsonbIteratorNext() for notes on memory management.
 */
JsonbIterator *
JsonbIteratorInit(JsonbContainer *container)
{

}

/*
 * Get next JsonbValue while iterating
 *
 * Caller should initially pass their own, original iterator.  They may get
 * back a child iterator palloc()'d here instead.  The function can be relied
 * on to free those child iterators, lest the memory allocated for highly
 * nested objects become unreasonable, but only if callers don't end iteration
 * early (by breaking upon having found something in a search, for example).
 *
 * Callers in such a scenario, that are particularly sensitive to leaking
 * memory in a long-lived context may walk the ancestral tree from the final
 * iterator we left them with to its oldest ancestor, pfree()ing as they go.
 * They do not have to free any other memory previously allocated for iterators
 * but not accessible as direct ancestors of the iterator they're last passed
 * back.
 *
 * Returns "Jsonb sequential processing" token value.  Iterator "state"
 * reflects the current stage of the process in a less granular fashion, and is
 * mostly used here to track things internally with respect to particular
 * iterators.
 *
 * Clients of this function should not have to handle any jbvBinary values
 * (since recursive calls will deal with this), provided skipNested is false.
 * It is our job to expand the jbvBinary representation without bothering them
 * with it.  However, clients should not take it upon themselves to touch array
 * or Object element/pair buffers, since their element/pair pointers are
 * garbage.  Also, *val will not be set when returning WJB_END_ARRAY or
 * WJB_END_OBJECT, on the assumption that it's only useful to access values
 * when recursing in.
 */
JsonbIteratorToken
JsonbIteratorNext(JsonbIterator **it, JsonbValue *val, bool skipNested)
{






































































































































}

/*
 * Initialize an iterator for iterating all elements in a container.
 */
static JsonbIterator *
iteratorFromContainer(JsonbContainer *container, JsonbIterator *parent)
{































}

/*
 * JsonbIteratorNext() worker:	Return parent, while freeing memory for current
 * iterator
 */
static JsonbIterator *
freeAndGetParent(JsonbIterator *it)
{




}

/*
 * Worker for "contains" operator's function
 *
 * Formally speaking, containment is top-down, unordered subtree isomorphism.
 *
 * Takes iterators that belong to some container type.  These iterators
 * "belong" to those values in the sense that they've just been initialized in
 * respect of them by the caller (perhaps in a nested fashion).
 *
 * "val" is lhs Jsonb, and mContained is rhs Jsonb when called from top level.
 * We determine if mContained is contained within val.
 */
bool
JsonbDeepContains(JsonbIterator **val, JsonbIterator **mContained)
{



































































































































































































































































}

/*
 * Hash a JsonbValue scalar value, mixing the hash value into an existing
 * hash provided by the caller.
 *
 * Some callers may wish to independently XOR in JB_FOBJECT and JB_FARRAY
 * flags.
 */
void
JsonbHashScalarValue(const JsonbValue *scalarVal, uint32 *hash)
{
































}

/*
 * Hash a value to a 64-bit value, with a seed. Otherwise, similar to
 * JsonbHashScalarValue.
 */
void
JsonbHashScalarValueExtended(const JsonbValue *scalarVal, uint64 *hash, uint64 seed)
{































}

/*
 * Are two scalar JsonbValues of the same type a and b equal?
 */
static bool
equalsJsonbScalarValue(JsonbValue *aScalar, JsonbValue *bScalar)
{



















}

/*
 * Compare two scalar JsonbValues, returning -1, 0, or 1.
 *
 * Strings are compared using the default collation.  Used by B-tree
 * operators, where a lexical sort order is generally expected.
 */
static int
compareJsonbScalarValue(JsonbValue *aScalar, JsonbValue *bScalar)
{





























}

/*
 * Functions for manipulating the resizable buffer used by convertJsonb and
 * its subroutines.
 */

/*
 * Reserve 'len' bytes, at the end of the buffer, enlarging it if necessary.
 * Returns the offset to the reserved area. The caller is expected to fill
 * the reserved area later with copyToBuffer().
 */
static int
reserveFromBuffer(StringInfo buffer, int len)
{


















}

/*
 * Copy 'len' bytes to a previously reserved area in buffer.
 */
static void
copyToBuffer(StringInfo buffer, int offset, const char *data, int len)
{

}

/*
 * A shorthand for reserveFromBuffer + copyToBuffer.
 */
static void
appendToBuffer(StringInfo buffer, const char *data, int len)
{




}

/*
 * Append padding, so that the length of the StringInfo is int-aligned.
 * Returns the number of padding bytes appended.
 */
static short
padBufferToInt(StringInfo buffer)
{













}

/*
 * Given a JsonbValue, convert to Jsonb. The result is palloc'd.
 */
static Jsonb *
convertToJsonb(JsonbValue *val)
{


























}

/*
 * Subroutine of convertJsonb: serialize a single JsonbValue into buffer.
 *
 * The JEntry header for this node is returned in *header.  It is filled in
 * with the length of this value and appropriate type bits.  If we wish to
 * store an end offset rather than a length, it is the caller's responsibility
 * to adjust for that.
 *
 * If the value is an array or an object, this recurses. 'level' is only used
 * for debugging purposes.
 */
static void
convertJsonbValue(StringInfo buffer, JEntry *header, JsonbValue *val, int level)
{






























}

static void
convertJsonbArray(StringInfo buffer, JEntry *pheader, JsonbValue *val, int level)
{















































































}

static void
convertJsonbObject(StringInfo buffer, JEntry *pheader, JsonbValue *val, int level)
{
















































































































}

static void
convertJsonbScalar(StringInfo buffer, JEntry *jentry, JsonbValue *scalarVal)
{































}

/*
 * Compare two jbvString JsonbValue values, a and b.
 *
 * This is a special qsort() comparator used to sort strings in certain
 * internal contexts where it is sufficient to have a well-defined sort order.
 * In particular, object pair keys are sorted according to this criteria to
 * facilitate cheap binary searches where we don't care about lexical sort
 * order.
 *
 * a and b are first sorted based on their length.  If a tie-breaker is
 * required, only then do we consider string binary equality.
 */
static int
lengthCompareJsonbStringValue(const void *a, const void *b)
{

















}

/*
 * qsort_arg() comparator to compare JsonbPair values.
 *
 * Third argument 'binequal' may point to a bool. If it's set, *binequal is set
 * to true iff a and b have full binary equality, since some callers have an
 * interest in whether the two values are equal or merely equivalent.
 *
 * N.B: String comparisons here are "length-wise"
 *
 * Pairs with equals keys are ordered such that the order field is respected.
 */
static int
lengthCompareJsonbPair(const void *a, const void *b, void *binequal)
{




















}

/*
 * Sort and unique-ify pairs in JsonbValue object
 */
static void
uniqueifyJsonbObject(JsonbValue *object)
{





























}