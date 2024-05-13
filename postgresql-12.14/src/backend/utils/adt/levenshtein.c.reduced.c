/*-------------------------------------------------------------------------
 *
 * levenshtein.c
 *	  Levenshtein distance implementation.
 *
 * Original author:  Joe Conway <mail@joeconway.com>
 *
 * This file is included by varlena.c twice, to provide matching code for (1)
 * Levenshtein distance with custom costings, and (2) Levenshtein distance with
 * custom costings and a "max" value above which exact distances are not
 * interesting.  Before the inclusion, we rely on the presence of the inline
 * function rest_of_char_same().
 *
 * Written based on a description of the algorithm by Michael Gilleland found
 * at http://www.merriampark.com/ld.htm.  Also looked at levenshtein.c in the
 * PHP 4.0.6 distribution for inspiration.  Configurable penalty costs
 * extension is introduced by Volkan YAZICI <volkan.yazici@gmail.com.
 *
 * Copyright (c) 2001-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	src/backend/utils/adt/levenshtein.c
 *
 *-------------------------------------------------------------------------
 */


/*
 * Calculates Levenshtein distance metric between supplied strings, which are
 * not necessarily null-terminated.
 *
 * source: source string, of length slen bytes.
 * target: target string, of length tlen bytes.
 * ins_c, del_c, sub_c: costs to charge for character insertion, deletion,
 *		and substitution respectively; (1, 1, 1) costs suffice for
 *common cases, but your mileage may vary. max_d: if provided and >= 0, maximum
 *distance we care about; see below. trusted: caller is trusted and need not
 *obey MAX_LEVENSHTEIN_STRLEN.
 *
 * One way to compute Levenshtein distance is to incrementally construct
 * an (m+1)x(n+1) matrix where cell (i, j) represents the minimum number
 * of operations required to transform the first i characters of s into
 * the first j characters of t.  The last column of the final row is the
 * answer.
 *
 * We use that algorithm here with some modification.  In lieu of holding
 * the entire array in memory at once, we'll just use two arrays of size
 * m+1 for storing accumulated values. At each step one array represents
 * the "previous" row and one is the "current" row of the notional large
 * array.
 *
 * If max_d >= 0, we only need to provide an accurate answer when that answer
 * is less than or equal to max_d.  From any cell in the matrix, there is
 * theoretical "minimum residual distance" from that cell to the last column
 * of the final row.  This minimum residual distance is zero when the
 * untransformed portions of the strings are of equal length (because we might
 * get lucky and find all the remaining characters matching) and is otherwise
 * based on the minimum number of insertions or deletions needed to make them
 * equal length.  The residual distance grows as we move toward the upper
 * right or lower left corners of the matrix.  When the max_d bound is
 * usefully tight, we can use this property to avoid computing the entirety
 * of each row; instead, we maintain a start_column and stop_column that
 * identify the portion of the matrix close to the diagonal which can still
 * affect the final answer.
 */
int
#ifdef LEVENSHTEIN_LESS_EQUAL
varstr_levenshtein_less_equal(const char *source, int slen, const char *target,
                              int tlen, int ins_c, int del_c, int sub_c,
                              int max_d, bool trusted)
#else
varstr_levenshtein(const char *source, int slen, const char *target, int tlen,
                   int ins_c, int del_c, int sub_c, bool trusted)
#endif
{



























































































































































































































































































































}
