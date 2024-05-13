/*-------------------------------------------------------------------------
 *
 * analyze.c
 *	  the Postgres statistics generator
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/analyze.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/genam.h"
#include "access/multixact.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/tupconvert.h"
#include "access/tuptoaster.h"
#include "access/visibilitymap.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_statistic_ext.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/attoptcache.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/sampling.h"
#include "utils/sortsupport.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

/* Per-index data for ANALYZE */
typedef struct AnlIndexData
{
  IndexInfo *indexInfo;        /* BuildIndexInfo result */
  double tupleFract;           /* fraction of rows for partial index */
  VacAttrStats **vacattrstats; /* index attrs to analyze */
  int attr_cnt;
} AnlIndexData;

/* Default statistics target (GUC parameter) */
int default_statistics_target = 100;

/* A few variables that don't seem worth passing around as parameters */
static MemoryContext anl_context = NULL;
static BufferAccessStrategy vac_strategy;

static void
do_analyze_rel(Relation onerel, VacuumParams *params, List *va_cols, AcquireSampleRowsFunc acquirefunc, BlockNumber relpages, bool inh, bool in_outer_xact, int elevel);
static void
compute_index_stats(Relation onerel, double totalrows, AnlIndexData *indexdata, int nindexes, HeapTuple *rows, int numrows, MemoryContext col_context);
static VacAttrStats *
examine_attribute(Relation onerel, int attnum, Node *index_expr);
static int
acquire_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows);
static int
compare_rows(const void *a, const void *b);
static int
acquire_inherited_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows);
static void
update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats);
static Datum
std_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull);
static Datum
ind_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull);

/*
 *	analyze_rel() -- analyze one relation
 *
 * relid identifies the relation to analyze.  If relation is supplied, use
 * the name therein for reporting any failure to open/lock the rel; do not
 * use it once we've successfully opened the rel, since it might be stale.
 */
void
analyze_rel(Oid relid, RangeVar *relation, VacuumParams *params, List *va_cols, bool in_outer_xact, BufferAccessStrategy bstrategy)
{




































































































































































}

/*
 *	do_analyze_rel() -- analyze one relation, recursively or not
 *
 * Note that "acquirefunc" is only relevant for the non-inherited case.
 * For the inherited case, acquire_inherited_sample_rows() determines the
 * appropriate acquirefunc for each child table.
 */
static void
do_analyze_rel(Relation onerel, VacuumParams *params, List *va_cols, AcquireSampleRowsFunc acquirefunc, BlockNumber relpages, bool inh, bool in_outer_xact, int elevel)
{

































































































































































































































































































































































































}

/*
 * Compute statistics about indexes of a relation
 */
static void
compute_index_stats(Relation onerel, double totalrows, AnlIndexData *indexdata, int nindexes, HeapTuple *rows, int numrows, MemoryContext col_context)
{

























































































































































}

/*
 * examine_attribute -- pre-analysis of a single column
 *
 * Determine whether the column is analyzable; if so, create and initialize
 * a VacAttrStats struct for it.  If not, return NULL.
 *
 * If index_expr isn't NULL, then we're trying to analyze an expression index,
 * and index_expr is the expression tree representing the column's data.
 */
static VacAttrStats *
examine_attribute(Relation onerel, int attnum, Node *index_expr)
{









































































































}

/*
 * acquire_sample_rows -- acquire a random sample of rows from the table
 *
 * Selected rows are returned in the caller-allocated array rows[], which
 * must have at least targrows entries.
 * The actual number of rows selected is returned as the function result.
 * We also estimate the total numbers of live and dead rows in the table,
 * and return them into *totalrows and *totaldeadrows, respectively.
 *
 * The returned list of tuples is in order by physical position in the table.
 * (We will rely on this later to derive correlation estimates.)
 *
 * As of May 2004 we use a new two-stage method:  Stage one selects up
 * to targrows random blocks (or all blocks, if there aren't so many).
 * Stage two scans these blocks and uses the Vitter algorithm to create
 * a random sample of targrows rows (or less, if there are less in the
 * sample of blocks).  The two stages are executed simultaneously: each
 * block is processed as soon as stage one returns its number and while
 * the rows are read stage two controls which ones are to be inserted
 * into the sample.
 *
 * Although every row has an equal chance of ending up in the final
 * sample, this sampling method is not perfect: not every possible
 * sample has an equal chance of being selected.  For large relations
 * the number of different blocks represented by the sample tends to be
 * too small.  We can live with that for now.  Improvements are welcome.
 *
 * An important property of this sampling method is that because we do
 * look at a statistically unbiased set of blocks, we should get
 * unbiased estimates of the average numbers of live and dead rows per
 * block.  The previous sampling method put too much credence in the row
 * density near the start of the table.
 */
static int
acquire_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows)
{

































































































































}

/*
 * qsort comparator for sorting rows[] array
 */
static int
compare_rows(const void *a, const void *b)
{
























}

/*
 * acquire_inherited_sample_rows -- acquire sample rows from inheritance tree
 *
 * This has the same API as acquire_sample_rows, except that rows are
 * collected from all inheritance children as well as the specified table.
 * We fail and return zero if there are no inheritance children, or if all
 * children are foreign tables that don't support ANALYZE.
 */
static int
acquire_inherited_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows)
{



































































































































































































}

/*
 *	update_attstats() -- update attribute statistics for one relation
 *
 *		Statistics are stored in several places: the pg_class row for
 *the relation has stats about the whole relation, and there is a pg_statistic
 *row for each (non-system) attribute that has ever been analyzed.  The pg_class
 *values are updated by VACUUM, not here.
 *
 *		pg_statistic rows are just added or updated normally.  This
 *means that pg_statistic will probably contain some deleted rows at the
 *		completion of a vacuum cycle, unless it happens to get vacuumed
 *last.
 *
 *		To keep things simple, we punt for pg_statistic, and don't try
 *		to compute or store rows for pg_statistic itself in
 *pg_statistic. This could possibly be made to work, but it's not worth the
 *trouble. Note analyze_rel() has seen to it that we won't come here when
 *		vacuuming pg_statistic itself.
 *
 *		Note: there would be a race condition here if two backends could
 *		ANALYZE the same table concurrently.  Presently, we lock that
 *out by taking a self-exclusive lock on the relation in analyze_rel().
 */
static void
update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats)
{





















































































































}

/*
 * Standard fetch function for use by compute_stats subroutines.
 *
 * This exists to provide some insulation between compute_stats routines
 * and the actual storage of the sample data.
 */
static Datum
std_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull)
{





}

/*
 * Fetch function for analyzing index expressions.
 *
 * We have not bothered to construct index tuples, instead the data is
 * just in Datum arrays.
 */
static Datum
ind_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull)
{






}

/*==========================================================================
 *
 * Code below this point represents the "standard" type-specific statistics
 * analysis algorithms.  This code can be replaced on a per-data-type basis
 * by setting a nonzero value in pg_type.typanalyze.
 *
 *==========================================================================
 */

/*
 * To avoid consuming too much memory during analysis and/or too much space
 * in the resulting pg_statistic rows, we ignore varlena datums that are wider
 * than WIDTH_THRESHOLD (after detoasting!).  This is legitimate for MCV
 * and distinct-value calculations since a wide value is unlikely to be
 * duplicated at all, much less be a most-common value.  For the same reason,
 * ignoring wide values will not affect our estimates of histogram bin
 * boundaries very much.
 */
#define WIDTH_THRESHOLD 1024

#define swapInt(a, b)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    _tmp = a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    a = b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    b = _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)
#define swapDatum(a, b)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    Datum _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    _tmp = a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    a = b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    b = _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

/*
 * Extra information used by the default analysis routines
 */
typedef struct
{
  int count; /* # of duplicates */
  int first; /* values[] index of first occurrence */
} ScalarMCVItem;

typedef struct
{
  SortSupport ssup;
  int *tupnoLink;
} CompareScalarsContext;

static void
compute_trivial_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static void
compute_distinct_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static void
compute_scalar_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static int
compare_scalars(const void *a, const void *b, void *arg);
static int
compare_mcvs(const void *a, const void *b);
static int
analyze_mcv_list(int *mcv_counts, int num_mcv, double stadistinct, double stanullfrac, int samplerows, double totalrows);

/*
 * std_typanalyze -- the default type-specific typanalyze function
 */
bool
std_typanalyze(VacAttrStats *stats)
{


































































}

/*
 *	compute_trivial_stats() -- compute very basic column statistics
 *
 *	We use this when we cannot find a hash "=" operator for the datatype.
 *
 *	We determine the fraction of non-null rows and the average datum width.
 */
static void
compute_trivial_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{








































































}

/*
 *	compute_distinct_stats() -- compute column statistics including
 *ndistinct
 *
 *	We use this when we can find only an "=" operator for the datatype.
 *
 *	We determine the fraction of non-null rows, the average width, the
 *	most common values, and the (estimated) number of distinct values.
 *
 *	The most common values are determined by brute force: we keep a list
 *	of previously seen values, ordered by number of times seen, as we scan
 *	the samples.  A newly seen value is inserted just after the last
 *	multiply-seen value, causing the bottommost (oldest) singly-seen value
 *	to drop off the list.  The accuracy of this method, and also its cost,
 *	depend mainly on the length of the list we are willing to keep.
 */
static void
compute_distinct_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{

















































































































































































































































































































































}

/*
 *	compute_scalar_stats() -- compute column statistics
 *
 *	We use this when we can find "=" and "<" operators for the datatype.
 *
 *	We determine the fraction of non-null rows, the average width, the
 *	most common values, the (estimated) number of distinct values, the
 *	distribution histogram, and the correlation of physical to logical
 *order.
 *
 *	The desired stats can be determined fairly easily after sorting the
 *	data values into order.
 */
static void
compute_scalar_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{












































































































































































































































































































































































































































































































































}

/*
 * qsort_arg comparator for sorting ScalarItems
 *
 * Aside from sorting the items, we update the tupnoLink[] array
 * whenever two ScalarItems are found to contain equal datums.  The array
 * is indexed by tupno; for each ScalarItem, it contains the highest
 * tupno that that item's datum has been found to be equal to.  This allows
 * us to avoid additional comparisons in compute_scalar_stats().
 */
static int
compare_scalars(const void *a, const void *b, void *arg)
{





























}

/*
 * qsort comparator for sorting ScalarMCVItems by position
 */
static int
compare_mcvs(const void *a, const void *b)
{




}

/*
 * Analyze the list of common values in the sample and decide how many are
 * worth storing in the table's MCV list.
 *
 * mcv_counts is assumed to be a list of the counts of the most common values
 * seen in the sample, starting with the most common.  The return value is the
 * number that are significantly more common than the values not in the list,
 * and which are therefore deemed worth storing in the table's MCV list.
 */
static int
analyze_mcv_list(int *mcv_counts, int num_mcv, double stadistinct, double stanullfrac, int samplerows, double totalrows)
{















































































































}