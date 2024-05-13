/*-------------------------------------------------------------------------
 *
 * statscmds.c
 *	  Commands for creating and altering extended statistics objects
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/statscmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/relation.h"
#include "access/relscan.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "statistics/statistics.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

static char *
ChooseExtendedStatisticName(const char *name1, const char *name2, const char *label, Oid namespaceid);
static char *
ChooseExtendedStatisticNameAddition(List *exprs);

/* qsort comparator for the attnums in CreateStatistics */
static int
compare_int16(const void *a, const void *b)
{





}

/*
 *		CREATE STATISTICS
 */
ObjectAddress
CreateStatistics(CreateStatsStmt *stmt)
{

































































































































































































































































































































































}

/*
 * Guts of statistics object deletion.
 */
void
RemoveStatisticsById(Oid statsOid)
{















































}

/*
 * Update a statistics object for ALTER COLUMN TYPE on a source column.
 *
 * This could throw an error if the type change can't be supported.
 * If it can be supported, but the stats must be recomputed, a likely choice
 * would be to set the relevant column(s) of the pg_statistic_ext_data tuple
 * to null until the next ANALYZE.  (Note that the type change hasn't actually
 * happened yet, so one option that's *not* on the table is to recompute
 * immediately.)
 *
 * For both ndistinct and functional-dependencies stats, the on-disk
 * representation is independent of the source column data types, and it is
 * plausible to assume that the old statistic values will still be good for
 * the new column contents.  (Obviously, if the ALTER COLUMN TYPE has a USING
 * expression that substantially alters the semantic meaning of the column
 * values, this assumption could fail.  But that seems like a corner case
 * that doesn't justify zapping the stats in common cases.)
 *
 * For MCV lists that's not the case, as those statistics store the datums
 * internally. In this case we simply reset the statistics value to NULL.
 *
 * Note that "type change" includes collation change, which means we can rely
 * on the MCV list being consistent with the collation info in pg_attribute
 * during estimation.
 */
void
UpdateStatisticsForTypeChange(Oid statsOid, Oid relationOid, int attnum, Oid oldColumnType, Oid newColumnType)
{















































}

/*
 * Select a nonconflicting name for a new statistics.
 *
 * name1, name2, and label are used the same way as for makeObjectName(),
 * except that the label can't be NULL; digits will be appended to the label
 * if needed to create a name that is unique within the specified namespace.
 *
 * Returns a palloc'd string.
 *
 * Note: it is theoretically possible to get a collision anyway, if someone
 * else chooses the same name concurrently.  This is fairly unlikely to be
 * a problem in practice, especially if one is holding a share update
 * exclusive lock on the relation identified by name1.  However, if choosing
 * multiple names within a single command, you'd better create the new object
 * and do CommandCounterIncrement before choosing the next one!
 */
static char *
ChooseExtendedStatisticName(const char *name1, const char *name2, const char *label, Oid namespaceid)
{

























}

/*
 * Generate "name2" for a new statistics given the list of column names for it
 * This will be passed to ChooseExtendedStatisticName along with the parent
 * table name and a suitable label.
 *
 * We know that less than NAMEDATALEN characters will actually be used,
 * so we can truncate the result once we've generated that many.
 *
 * XXX see also ChooseForeignKeyConstraintNameAddition and
 * ChooseIndexNameAddition.
 */
static char *
ChooseExtendedStatisticNameAddition(List *exprs)
{



































}