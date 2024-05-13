/*-------------------------------------------------------------------------
 *
 * like_support.c
 *	  Planner support functions for LIKE, regex, and related operators.
 *
 * These routines handle special optimization of operators that can be
 * used with index scans even though they are not known to the executor's
 * indexscan machinery.  The key idea is that these operators allow us
 * to derive approximate indexscan qual clauses, such that any tuples
 * that pass the operator clause itself must also satisfy the simpler
 * indexscan condition(s).  Then we can use the indexscan machinery
 * to avoid scanning as much of the table as we'd otherwise have to,
 * while applying the original operator as a qpqual condition to ensure
 * we deliver only the tuples we want.  (In essence, we're using a regular
 * index as if it were a lossy index.)
 *
 * An example of what we're doing is
 *			textfield LIKE 'abc%def'
 * from which we can generate the indexscanable conditions
 *			textfield >= 'abc' AND textfield < 'abd'
 * which allow efficient scanning of an index on textfield.
 * (In reality, character set and collation issues make the transformation
 * from LIKE to indexscan limits rather harder than one might think ...
 * but that's the basic idea.)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/like_support.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "access/stratnum.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_type.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/pg_locale.h"
#include "utils/selfuncs.h"
#include "utils/varlena.h"

typedef enum
{
  Pattern_Type_Like,
  Pattern_Type_Like_IC,
  Pattern_Type_Regex,
  Pattern_Type_Regex_IC,
  Pattern_Type_Prefix
} Pattern_Type;

typedef enum
{
  Pattern_Prefix_None,
  Pattern_Prefix_Partial,
  Pattern_Prefix_Exact
} Pattern_Prefix_Status;

static Node *
like_regex_support(Node *rawreq, Pattern_Type ptype);
static List *
match_pattern_prefix(Node *leftop, Node *rightop, Pattern_Type ptype, Oid expr_coll, Oid opfamily, Oid indexcollation);
static double
patternsel_common(PlannerInfo *root, Oid oprid, Oid opfuncid, List *args, int varRelid, Oid collation, Pattern_Type ptype, bool negate);
static Pattern_Prefix_Status
pattern_fixed_prefix(Const *patt, Pattern_Type ptype, Oid collation, Const **prefix, Selectivity *rest_selec);
static Selectivity
prefix_selectivity(PlannerInfo *root, VariableStatData *vardata, Oid vartype, Oid opfamily, Oid collation, Const *prefixcon);
static Selectivity
like_selectivity(const char *patt, int pattlen, bool case_insensitive);
static Selectivity
regex_selectivity(const char *patt, int pattlen, bool case_insensitive, int fixed_prefix_len);
static int
pattern_char_isalpha(char c, bool is_multibyte, pg_locale_t locale, bool locale_is_c);
static Const *
make_greater_string(const Const *str_const, FmgrInfo *ltproc, Oid collation);
static Datum
string_to_datum(const char *str, Oid datatype);
static Const *
string_to_const(const char *str, Oid datatype);
static Const *
string_to_bytea_const(const char *str, size_t str_len);

/*
 * Planner support functions for LIKE, regex, and related operators
 */
Datum
textlike_support(PG_FUNCTION_ARGS)
{



}

Datum
texticlike_support(PG_FUNCTION_ARGS)
{



}

Datum
textregexeq_support(PG_FUNCTION_ARGS)
{



}

Datum
texticregexeq_support(PG_FUNCTION_ARGS)
{



}

/* Common code for the above */
static Node *
like_regex_support(Node *rawreq, Pattern_Type ptype)
{



























































}

/*
 * match_pattern_prefix
 *	  Try to generate an indexqual for a LIKE or regex operator.
 */
static List *
match_pattern_prefix(Node *leftop, Node *rightop, Pattern_Type ptype, Oid expr_coll, Oid opfamily, Oid indexcollation)
{



















































































































































































}

/*
 * patternsel_common - generic code for pattern-match restriction selectivity.
 *
 * To support using this from either the operator or function paths, caller
 * may pass either operator OID or underlying function OID; we look up the
 * latter from the former if needed.  (We could just have patternsel() call
 * get_opcode(), but the work would be wasted if we don't have a need to
 * compare a fixed prefix to the pg_statistic data.)
 *
 * Note that oprid and/or opfuncid should be for the positive-match operator
 * even when negate is true.
 */
static double
patternsel_common(PlannerInfo *root, Oid oprid, Oid opfuncid, List *args, int varRelid, Oid collation, Pattern_Type ptype, bool negate)
{










































































































































































































































































}

/*
 * Fix impedance mismatch between SQL-callable functions and patternsel_common
 */
static double
patternsel(PG_FUNCTION_ARGS, Pattern_Type ptype, bool negate)
{




















}

/*
 *		regexeqsel		- Selectivity of regular-expression
 *pattern match.
 */
Datum
regexeqsel(PG_FUNCTION_ARGS)
{

}

/*
 *		icregexeqsel	- Selectivity of case-insensitive regex match.
 */
Datum
icregexeqsel(PG_FUNCTION_ARGS)
{

}

/*
 *		likesel			- Selectivity of LIKE pattern match.
 */
Datum
likesel(PG_FUNCTION_ARGS)
{

}

/*
 *		prefixsel			- selectivity of prefix operator
 */
Datum
prefixsel(PG_FUNCTION_ARGS)
{

}

/*
 *
 *		iclikesel			- Selectivity of ILIKE pattern
 *match.
 */
Datum
iclikesel(PG_FUNCTION_ARGS)
{

}

/*
 *		regexnesel		- Selectivity of regular-expression
 *pattern non-match.
 */
Datum
regexnesel(PG_FUNCTION_ARGS)
{

}

/*
 *		icregexnesel	- Selectivity of case-insensitive regex
 *non-match.
 */
Datum
icregexnesel(PG_FUNCTION_ARGS)
{

}

/*
 *		nlikesel		- Selectivity of LIKE pattern non-match.
 */
Datum
nlikesel(PG_FUNCTION_ARGS)
{

}

/*
 *		icnlikesel		- Selectivity of ILIKE pattern
 *non-match.
 */
Datum
icnlikesel(PG_FUNCTION_ARGS)
{

}

/*
 * patternjoinsel		- Generic code for pattern-match join
 * selectivity.
 */
static double
patternjoinsel(PG_FUNCTION_ARGS, Pattern_Type ptype, bool negate)
{


}

/*
 *		regexeqjoinsel	- Join selectivity of regular-expression pattern
 *match.
 */
Datum
regexeqjoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		icregexeqjoinsel	- Join selectivity of case-insensitive
 *regex match.
 */
Datum
icregexeqjoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		likejoinsel			- Join selectivity of LIKE
 *pattern match.
 */
Datum
likejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		prefixjoinsel			- Join selectivity of prefix
 *operator
 */
Datum
prefixjoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		iclikejoinsel			- Join selectivity of ILIKE
 *pattern match.
 */
Datum
iclikejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		regexnejoinsel	- Join selectivity of regex non-match.
 */
Datum
regexnejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		icregexnejoinsel	- Join selectivity of case-insensitive
 *regex non-match.
 */
Datum
icregexnejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		nlikejoinsel		- Join selectivity of LIKE pattern
 *non-match.
 */
Datum
nlikejoinsel(PG_FUNCTION_ARGS)
{

}

/*
 *		icnlikejoinsel		- Join selectivity of ILIKE pattern
 *non-match.
 */
Datum
icnlikejoinsel(PG_FUNCTION_ARGS)
{

}

/*-------------------------------------------------------------------------
 *
 * Pattern analysis functions
 *
 * These routines support analysis of LIKE and regular-expression patterns
 * by the planner/optimizer.  It's important that they agree with the
 * regular-expression code in backend/regex/ and the LIKE code in
 * backend/utils/adt/like.c.  Also, the computation of the fixed prefix
 * must be conservative: if we report a string longer than the true fixed
 * prefix, the query may produce actually wrong answers, rather than just
 * getting a bad selectivity estimate!
 *
 *-------------------------------------------------------------------------
 */

/*
 * Extract the fixed prefix, if any, for a pattern.
 *
 * *prefix is set to a palloc'd prefix string (in the form of a Const node),
 *	or to NULL if no fixed prefix exists for the pattern.
 * If rest_selec is not NULL, *rest_selec is set to an estimate of the
 *	selectivity of the remainder of the pattern (without any fixed prefix).
 * The prefix Const has the same type (TEXT or BYTEA) as the input pattern.
 *
 * The return value distinguishes no fixed prefix, a partial prefix,
 * or an exact-match-only pattern.
 */

static Pattern_Prefix_Status
like_fixed_prefix(Const *patt_const, bool case_insensitive, Oid collation, Const **prefix_const, Selectivity *rest_selec)
{

















































































































}

static Pattern_Prefix_Status
regex_fixed_prefix(Const *patt_const, bool case_insensitive, Oid collation, Const **prefix_const, Selectivity *rest_selec)
{




























































}

static Pattern_Prefix_Status
pattern_fixed_prefix(Const *patt, Pattern_Type ptype, Oid collation, Const **prefix, Selectivity *rest_selec)
{




























}

/*
 * Estimate the selectivity of a fixed prefix for a pattern match.
 *
 * A fixed prefix "foo" is estimated as the selectivity of the expression
 * "variable >= 'foo' AND variable < 'fop'" (see also indxpath.c).
 *
 * The selectivity estimate is with respect to the portion of the column
 * population represented by the histogram --- the caller must fold this
 * together with info about MCVs and NULLs.
 *
 * We use the >= and < operators from the specified btree opfamily to do the
 * estimation.  The given variable and Const must be of the associated
 * datatype.
 *
 * XXX Note: we make use of the upper bound to estimate operator selectivity
 * even if the locale is such that we cannot rely on the upper-bound string.
 * The selectivity only needs to be approximately right anyway, so it seems
 * more useful to use the upper-bound code than not.
 */
static Selectivity
prefix_selectivity(PlannerInfo *root, VariableStatData *vardata, Oid vartype, Oid opfamily, Oid collation, Const *prefixcon)
{








































































}

/*
 * Estimate the selectivity of a pattern of the specified type.
 * Note that any fixed prefix of the pattern will have been removed already,
 * so actually we may be looking at just a fragment of the pattern.
 *
 * For now, we use a very simplistic approach: fixed characters reduce the
 * selectivity a good deal, character ranges reduce it a little,
 * wildcards (such as % for LIKE or .* for regex) increase it.
 */

#define FIXED_CHAR_SEL 0.20 /* about 1/5 */
#define CHAR_RANGE_SEL 0.25
#define ANY_CHAR_SEL 0.9 /* not 1, since it won't match end-of-string */
#define FULL_WILDCARD_SEL 5.0
#define PARTIAL_WILDCARD_SEL 2.0

static Selectivity
like_selectivity(const char *patt, int pattlen, bool case_insensitive)
{












































}

static Selectivity
regex_selectivity_sub(const char *patt, int pattlen, bool case_insensitive)
{














































































































}

static Selectivity
regex_selectivity(const char *patt, int pattlen, bool case_insensitive, int fixed_prefix_len)
{

































}

/*
 * Check whether char is a letter (and, hence, subject to case-folding)
 *
 * In multibyte character sets or with ICU, we can't use isalpha, and it does
 * not seem worth trying to convert to wchar_t to use iswalpha or u_isalpha.
 * Instead, just assume any non-ASCII char is potentially case-varying, and
 * hard-wire knowledge of which ASCII chars are letters.
 */
static int
pattern_char_isalpha(char c, bool is_multibyte, pg_locale_t locale, bool locale_is_c)
{






















}

/*
 * For bytea, the increment function need only increment the current byte
 * (there are no multibyte characters to worry about).
 */
static bool
byte_increment(unsigned char *ptr, int len)
{






}

/*
 * Try to generate a string greater than the given string or any
 * string it is a prefix of.  If successful, return a palloc'd string
 * in the form of a Const node; else return NULL.
 *
 * The caller must provide the appropriate "less than" comparison function
 * for testing the strings, along with the collation to use.
 *
 * The key requirement here is that given a prefix string, say "foo",
 * we must be able to generate another string "fop" that is greater than
 * all strings "foobar" starting with "foo".  We can test that we have
 * generated a string greater than the prefix string, but in non-C collations
 * that is not a bulletproof guarantee that an extension of the string might
 * not sort after it; an example is that "foo " is less than "foo!", but it
 * is not clear that a "dictionary" sort ordering will consider "foo!" less
 * than "foo bar".  CAUTION: Therefore, this function should be used only for
 * estimation purposes when working in a non-C collation.
 *
 * To try to catch most cases where an extended string might otherwise sort
 * before the result value, we determine which of the strings "Z", "z", "y",
 * and "9" is seen as largest by the collation, and append that to the given
 * prefix before trying to find a string that compares as larger.
 *
 * To search for a greater string, we repeatedly "increment" the rightmost
 * character, using an encoding-specific character incrementer function.
 * When it's no longer possible to increment the last character, we truncate
 * off that character and start incrementing the next-to-rightmost.
 * For example, if "z" were the last character in the sort order, then we
 * could produce "foo" as a string greater than "fonz".
 *
 * This could be rather slow in the worst case, but in most cases we
 * won't have to try more than one or two strings before succeeding.
 *
 * Note that it's important for the character incrementer not to be too anal
 * about producing every possible character code, since in some cases the only
 * way to get a larger string is to increment a previous character position.
 * So we don't want to spend too much time trying every possible character
 * code at the last position.  A good rule of thumb is to be sure that we
 * don't try more than 256*K values for a K-byte character (and definitely
 * not 256^K, which is what an exhaustive search would approach).
 */
static Const *
make_greater_string(const Const *str_const, FmgrInfo *ltproc, Oid collation)
{





































































































































































}

/*
 * Generate a Datum of the appropriate type from a C string.
 * Note that all of the supported types are pass-by-ref, so the
 * returned value should be pfree'd if no longer needed.
 */
static Datum
string_to_datum(const char *str, Oid datatype)
{


















}

/*
 * Generate a Const node of the appropriate type from a C string.
 */
static Const *
string_to_const(const char *str, Oid datatype)
{

































}

/*
 * Generate a Const node of bytea type from a binary C string and a length.
 */
static Const *
string_to_bytea_const(const char *str, size_t str_len)
{








}