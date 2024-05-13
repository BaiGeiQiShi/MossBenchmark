/*-------------------------------------------------------------------------
 *
 * reloptions.c
 *	  Core support for relation options (pg_class.reloptions)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/common/reloptions.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <float.h>

#include "access/gist_private.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/reloptions.h"
#include "access/spgist.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/tablespace.h"
#include "commands/view.h"
#include "nodes/makefuncs.h"
#include "postmaster/postmaster.h"
#include "utils/array.h"
#include "utils/attoptcache.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * Contents of pg_class.reloptions
 *
 * To add an option:
 *
 * (i) decide on a type (integer, real, bool, string), name, default value,
 * upper and lower bounds (if applicable); for strings, consider a validation
 * routine.
 * (ii) add a record below (or use add_<type>_reloption).
 * (iii) add it to the appropriate options struct (perhaps StdRdOptions)
 * (iv) add it to the appropriate handling routine (perhaps
 * default_reloptions)
 * (v) make sure the lock level is set correctly for that operation
 * (vi) don't forget to document the option
 *
 * The default choice for any new option should be AccessExclusiveLock.
 * In some cases the lock level can be reduced from there, but the lock
 * level chosen should always conflict with itself to ensure that multiple
 * changes aren't lost when we attempt concurrent changes.
 * The choice of lock level depends completely upon how that parameter
 * is used within the server, not upon how and when you'd like to change it.
 * Safety first. Existing choices are documented here, and elsewhere in
 * backend code where the parameters are used.
 *
 * In general, anything that affects the results obtained from a SELECT must be
 * protected by AccessExclusiveLock.
 *
 * Autovacuum related parameters can be set at ShareUpdateExclusiveLock
 * since they are only used by the AV procs and don't change anything
 * currently executing.
 *
 * Fillfactor can be set because it applies only to subsequent changes made to
 * data blocks, as documented in heapio.c
 *
 * n_distinct options can be set at ShareUpdateExclusiveLock because they
 * are only used during ANALYZE, which uses a ShareUpdateExclusiveLock,
 * so the ANALYZE will not be affected by in-flight changes. Changing those
 * values has no effect until the next ANALYZE, so no need for stronger lock.
 *
 * Planner-related parameters can be set with ShareUpdateExclusiveLock because
 * they only affect planning and not the correctness of the execution. Plans
 * cannot be changed in mid-flight, so changes here could not easily result in
 * new improved plans in any case. So we allow existing queries to continue
 * and existing plans to survive, a small price to pay for allowing better
 * plans to be introduced concurrently without interfering with users.
 *
 * Setting parallel_workers is safe, since it acts the same as
 * max_parallel_workers_per_gather which is a USERSET parameter that doesn't
 * affect existing plans or queries.
 *
 * vacuum_truncate can be set at ShareUpdateExclusiveLock because it
 * is only used during VACUUM, which uses a ShareUpdateExclusiveLock,
 * so the VACUUM will not be affected by in-flight changes. Changing its
 * value has no effect until the next VACUUM, so no need for stronger lock.
 */

static relopt_bool boolRelOpts[] = {{{"autosummarize", "Enables automatic summarization on this BRIN index", RELOPT_KIND_BRIN, AccessExclusiveLock}, false}, {{"autovacuum_enabled", "Enables autovacuum in this relation", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, true}, {{"user_catalog_table", "Declare a table as an additional catalog table, e.g. for the purpose of logical replication", RELOPT_KIND_HEAP, AccessExclusiveLock}, false}, {{"fastupdate", "Enables \"fast update\" feature for this GIN index", RELOPT_KIND_GIN, AccessExclusiveLock}, true}, {{"security_barrier", "View acts as a row security barrier", RELOPT_KIND_VIEW, AccessExclusiveLock}, false}, {{"vacuum_index_cleanup", "Enables index vacuuming and index cleanup", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, true}, {{"vacuum_truncate", "Enables vacuum to truncate empty pages at the end of this table", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, true},
    /* list terminator */
    {{NULL}}};

static relopt_int intRelOpts[] = {{{
                                       "fillfactor", "Packs table pages only to this percentage", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock /* since it applies only to
                                                                                                                                              * later inserts */
                                   },
                                      HEAP_DEFAULT_FILLFACTOR, HEAP_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs btree index pages only to this percentage", RELOPT_KIND_BTREE, ShareUpdateExclusiveLock /* since it applies only to
                                                                                                                       * later inserts */
     },
        BTREE_DEFAULT_FILLFACTOR, BTREE_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs hash index pages only to this percentage", RELOPT_KIND_HASH, ShareUpdateExclusiveLock /* since it applies only to
                                                                                                                     * later inserts */
     },
        HASH_DEFAULT_FILLFACTOR, HASH_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs gist index pages only to this percentage", RELOPT_KIND_GIST, ShareUpdateExclusiveLock /* since it applies only to
                                                                                                                     * later inserts */
     },
        GIST_DEFAULT_FILLFACTOR, GIST_MIN_FILLFACTOR, 100},
    {{
         "fillfactor", "Packs spgist index pages only to this percentage", RELOPT_KIND_SPGIST, ShareUpdateExclusiveLock /* since it applies only
                                                                                                                         * to later inserts */
     },
        SPGIST_DEFAULT_FILLFACTOR, SPGIST_MIN_FILLFACTOR, 100},
    {{"autovacuum_vacuum_threshold", "Minimum number of tuple updates or deletes prior to vacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, INT_MAX}, {{"autovacuum_analyze_threshold", "Minimum number of tuple inserts, updates or deletes prior to analyze", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, -1, 0, INT_MAX}, {{"autovacuum_vacuum_cost_limit", "Vacuum cost amount available before napping, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 1, 10000}, {{"autovacuum_freeze_min_age", "Minimum age at which VACUUM should freeze a table row, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 1000000000}, {{"autovacuum_multixact_freeze_min_age", "Minimum multixact age at which VACUUM should freeze a row multixact's, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 1000000000},
    {{"autovacuum_freeze_max_age", "Age at which to autovacuum a table to prevent transaction ID wraparound", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 100000, 2000000000}, {{"autovacuum_multixact_freeze_max_age", "Multixact age at which to autovacuum a table to prevent multixact wraparound", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 10000, 2000000000}, {{"autovacuum_freeze_table_age", "Age at which VACUUM should perform a full table sweep to freeze row versions", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 2000000000}, {{"autovacuum_multixact_freeze_table_age", "Age of multixact at which VACUUM should perform a full table sweep to freeze row versions", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0, 2000000000},
    {{"log_autovacuum_min_duration", "Sets the minimum execution time above which autovacuum actions will be logged", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, -1, INT_MAX}, {{"toast_tuple_target", "Sets the target tuple length at which external columns will be toasted", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, TOAST_TUPLE_TARGET, 128, TOAST_TUPLE_TARGET_MAIN}, {{"pages_per_range", "Number of pages that each page range covers in a BRIN index", RELOPT_KIND_BRIN, AccessExclusiveLock}, 128, 1, 131072}, {{"gin_pending_list_limit", "Maximum size of the pending list for this GIN index, in kilobytes.", RELOPT_KIND_GIN, AccessExclusiveLock}, -1, 64, MAX_KILOBYTES},
    {{"effective_io_concurrency", "Number of simultaneous requests that can be handled efficiently by the disk subsystem.", RELOPT_KIND_TABLESPACE, ShareUpdateExclusiveLock},
#ifdef USE_PREFETCH
        -1, 0, MAX_IO_CONCURRENCY
#else
        0, 0, 0
#endif
    },
    {{"parallel_workers", "Number of parallel processes that can be used per executor node for this relation.", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, -1, 0, 1024},

    /* list terminator */
    {{NULL}}};

static relopt_real realRelOpts[] = {{{"autovacuum_vacuum_cost_delay", "Vacuum cost delay in milliseconds, for autovacuum", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0.0, 100.0}, {{"autovacuum_vacuum_scale_factor", "Number of tuple updates or deletes prior to vacuum as a fraction of reltuples", RELOPT_KIND_HEAP | RELOPT_KIND_TOAST, ShareUpdateExclusiveLock}, -1, 0.0, 100.0}, {{"autovacuum_analyze_scale_factor", "Number of tuple inserts, updates or deletes prior to analyze as a fraction of reltuples", RELOPT_KIND_HEAP, ShareUpdateExclusiveLock}, -1, 0.0, 100.0}, {{"seq_page_cost", "Sets the planner's estimate of the cost of a sequentially fetched disk page.", RELOPT_KIND_TABLESPACE, ShareUpdateExclusiveLock}, -1, 0.0, DBL_MAX}, {{"random_page_cost", "Sets the planner's estimate of the cost of a nonsequentially fetched disk page.", RELOPT_KIND_TABLESPACE, ShareUpdateExclusiveLock}, -1, 0.0, DBL_MAX},
    {{"n_distinct", "Sets the planner's estimate of the number of distinct values appearing in a column (excluding child relations).", RELOPT_KIND_ATTRIBUTE, ShareUpdateExclusiveLock}, 0, -1.0, DBL_MAX}, {{"n_distinct_inherited", "Sets the planner's estimate of the number of distinct values appearing in a column (including child relations).", RELOPT_KIND_ATTRIBUTE, ShareUpdateExclusiveLock}, 0, -1.0, DBL_MAX}, {{"vacuum_cleanup_index_scale_factor", "Number of tuple inserts prior to index cleanup as a fraction of reltuples.", RELOPT_KIND_BTREE, ShareUpdateExclusiveLock}, -1, 0.0, 1e10},
    /* list terminator */
    {{NULL}}};

static relopt_string stringRelOpts[] = {{{"buffering", "Enables buffering build for this GiST index", RELOPT_KIND_GIST, AccessExclusiveLock}, 4, false, gistValidateBufferingOption, "auto"}, {{"check_option", "View has WITH CHECK OPTION defined (local or cascaded).", RELOPT_KIND_VIEW, AccessExclusiveLock}, 0, true, validateWithCheckOption, NULL},
    /* list terminator */
    {{NULL}}};

static relopt_gen **relOpts = NULL;
static bits32 last_assigned_kind = RELOPT_KIND_LAST_DEFAULT;

static int num_custom_options = 0;
static relopt_gen **custom_options = NULL;
static bool need_initialization = true;

static void
initialize_reloptions(void);
static void
parse_one_reloption(relopt_value *option, char *text_str, int text_len, bool validate);

/*
 * initialize_reloptions
 *		initialization routine, must be called before parsing
 *
 * Initialize the relOpts array and fill each variable's type and name length.
 */
static void
initialize_reloptions(void)
{












































































}

/*
 * add_reloption_kind
 *		Create a new relopt_kind value, to be used in custom reloptions
 *by user-defined AMs.
 */
relopt_kind
add_reloption_kind(void)
{







}

/*
 * add_reloption
 *		Add an already-created custom reloption to the list, and
 *recompute the main parser table.
 */
static void
add_reloption(relopt_gen *newoption)
{























}

/*
 * allocate_reloption
 *		Allocate a new reloption and initialize the type-agnostic fields
 *		(for types other than string)
 */
static relopt_gen *
allocate_reloption(bits32 kinds, int type, const char *name, const char *desc)
{


















































}

/*
 * add_bool_reloption
 *		Add a new boolean reloption
 */
void
add_bool_reloption(bits32 kinds, const char *name, const char *desc, bool default_val)
{






}

/*
 * add_int_reloption
 *		Add a new integer reloption
 */
void
add_int_reloption(bits32 kinds, const char *name, const char *desc, int default_val, int min_val, int max_val)
{








}

/*
 * add_real_reloption
 *		Add a new float reloption
 */
void
add_real_reloption(bits32 kinds, const char *name, const char *desc, double default_val, double min_val, double max_val)
{








}

/*
 * add_string_reloption
 *		Add a new string reloption
 *
 * "validator" is an optional function pointer that can be used to test the
 * validity of the values.  It must elog(ERROR) when the argument string is
 * not acceptable for the variable.  Note that the default value must pass
 * the validation.
 */
void
add_string_reloption(bits32 kinds, const char *name, const char *desc, const char *default_val, validate_string_relopt validator)
{
























}

/*
 * Transform a relation options list (list of DefElem) into the text array
 * format that is kept in pg_class.reloptions, including only those options
 * that are in the passed namespace.  The output values do not include the
 * namespace.
 *
 * This is used for three cases: CREATE TABLE/INDEX, ALTER TABLE SET, and
 * ALTER TABLE RESET.  In the ALTER cases, oldOptions is the existing
 * reloptions value (possibly NULL), and we replace or remove entries
 * as needed.
 *
 * If acceptOidsOff is true, then we allow oids = false, but throw error when
 * on. This is solely needed for backwards compatibility.
 *
 * Note that this is not responsible for determining whether the options
 * are valid, but it does check that namespaces for all the options given are
 * listed in validnsps.  The NULL namespace is always valid and need not be
 * explicitly listed.  Passing a NULL pointer means that only the NULL
 * namespace is valid.
 *
 * Both oldOptions and the result are text arrays (or NULL for "default"),
 * but we declare them as Datums to avoid including array.h in reloptions.h.
 */
Datum
transformRelOptions(Datum oldOptions, List *defList, const char *namspace, char *validnsps[], bool acceptOidsOff, bool isReset)
{





















































































































































































}

/*
 * Convert the text-array format of reloptions into a List of DefElem.
 * This is the inverse of transformRelOptions().
 */
List *
untransformRelOptions(Datum options)
{

































}

/*
 * Extract and parse reloptions from a pg_class tuple.
 *
 * This is a low-level routine, expected to be used by relcache code and
 * callers that do not have a table's relcache entry (e.g. autovacuum).  For
 * other uses, consider grabbing the rd_options pointer from the relcache entry
 * instead.
 *
 * tupdesc is pg_class' tuple descriptor.  amoptions is a pointer to the index
 * AM's options parser function in the case of a tuple corresponding to an
 * index, or NULL otherwise.
 */
bytea *
extractRelOptions(HeapTuple tuple, TupleDesc tupdesc, amoptions_function amoptions)
{







































}

/*
 * Interpret reloptions that are given in text-array format.
 *
 * options is a reloption text array as constructed by transformRelOptions.
 * kind specifies the family of options to be processed.
 *
 * The return value is a relopt_value * array on which the options actually
 * set in the options array are marked with isset=true.  The length of this
 * array is returned in *numrelopts.  Options not set are also present in the
 * array; this is so that the caller can easily locate the default values.
 *
 * If there are no options of the given kind, numrelopts is set to 0 and NULL
 * is returned (unless options are illegally supplied despite none being
 * defined, in which case an error occurs).
 *
 * Note: values of type int, bool and real are allocated as part of the
 * returned array.  Values of type string are allocated separately and must
 * be freed by the caller.
 */
relopt_value *
parseRelOptions(Datum options, bool validate, relopt_kind kind, int *numrelopts)
{























































































}

/*
 * Subroutine for parseRelOptions, to parse and validate a single option's
 * value
 */
static void
parse_one_reloption(relopt_value *option, char *text_str, int text_len, bool validate)
{



















































































}

/*
 * Given the result from parseRelOptions, allocate a struct that's of the
 * specified base size plus any extra space that's needed for string variables.
 *
 * "base" should be sizeof(struct) of the reloptions struct (StdRdOptions or
 * equivalent).
 */
void *
allocateReloptStruct(Size base, relopt_value *options, int numoptions)
{












}

/*
 * Given the result of parseRelOptions and a parsing table, fill in the
 * struct (previously allocated with allocateReloptStruct) with the parsed
 * values.
 *
 * rdopts is the pointer to the allocated struct to be filled.
 * basesize is the sizeof(struct) that was passed to allocateReloptStruct.
 * options, of length numoptions, is parseRelOptions' output.
 * elems, of length numelems, is the table describing the allowed options.
 * When validate is true, it is expected that all options appear in elems.
 */
void
fillRelOptions(void *rdopts, Size basesize, relopt_value *options, int numoptions, bool validate, const relopt_parse_elt *elems, int numelems)
{



































































}

/*
 * Option parser for anything that uses StdRdOptions.
 */
bytea *
default_reloptions(Datum reloptions, bool validate, relopt_kind kind)
{






















}

/*
 * Option parser for views
 */
bytea *
view_reloptions(Datum reloptions, bool validate)
{




















}

/*
 * Parse options for heaps, views and toast tables.
 */
bytea *
heap_reloptions(char relkind, Datum reloptions, bool validate)
{























}

/*
 * Parse options for indexes.
 *
 *	amoptions	index AM's option parser function
 *	reloptions	options as text[] datum
 *	validate	error flag
 */
bytea *
index_reloptions(amoptions_function amoptions, Datum reloptions, bool validate)
{









}

/*
 * Option parser for attribute reloptions
 */
bytea *
attribute_reloptions(Datum reloptions, bool validate)
{




















}

/*
 * Option parser for tablespace reloptions
 */
bytea *
tablespace_reloptions(Datum reloptions, bool validate)
{




















}

/*
 * Determine the required LOCKMODE from an option list.
 *
 * Called from AlterTableGetLockLevel(), see that function
 * for a longer explanation of how this works.
 */
LOCKMODE
AlterTableGetRelOptionsLockLevel(List *defList)
{































}