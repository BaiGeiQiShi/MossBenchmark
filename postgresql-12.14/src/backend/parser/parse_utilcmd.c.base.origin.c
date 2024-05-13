/*-------------------------------------------------------------------------
 *
 * parse_utilcmd.c
 *	  Perform parse analysis work for various utility commands
 *
 * Formerly we did this work during parse_analyze() in analyze.c.  However
 * that is fairly unsafe in the presence of querytree caching, since any
 * database state that we depend on in making the transformations might be
 * obsolete by the time the utility command is executed; and utility commands
 * have no infrastructure for holding locks or rechecking plan validity.
 * Hence these functions are now called at the start of execution of their
 * respective utility commands.
 *
 * NOTE: in general we must avoid scribbling on the passed-in raw parse
 * tree, since it might be in a plan cache.  The simplest solution is
 * a quick copyObject() call before manipulating the query tree.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/parser/parse_utilcmd.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/amapi.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/reloptions.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_type.h"
#include "commands/comment.h"
#include "commands/defrem.h"
#include "commands/sequence.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "parser/parse_utilcmd.h"
#include "parser/parser.h"
#include "rewrite/rewriteManip.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/* State shared by transformCreateStmt and its subroutines */
typedef struct
{
  ParseState *pstate;            /* overall parser state */
  const char *stmtType;          /* "CREATE [FOREIGN] TABLE" or "ALTER TABLE" */
  RangeVar *relation;            /* relation to create */
  Relation rel;                  /* opened/locked rel, if ALTER */
  List *inhRelations;            /* relations to inherit from */
  bool isforeign;                /* true if CREATE/ALTER FOREIGN TABLE */
  bool isalter;                  /* true if altering existing table */
  List *columns;                 /* ColumnDef items */
  List *ckconstraints;           /* CHECK constraints */
  List *fkconstraints;           /* FOREIGN KEY constraints */
  List *ixconstraints;           /* index-creating constraints */
  List *likeclauses;             /* LIKE clauses that need post-processing */
  List *extstats;                /* cloned extended statistics */
  List *blist;                   /* "before list" of things to do before
                                  * creating the table */
  List *alist;                   /* "after list" of things to do after creating
                                  * the table */
  IndexStmt *pkey;               /* PRIMARY KEY index, if any */
  bool ispartitioned;            /* true if table is partitioned */
  PartitionBoundSpec *partbound; /* transformed FOR VALUES */
  bool ofType;                   /* true if statement contains OF typename */
} CreateStmtContext;

/* State shared by transformCreateSchemaStmt and its subroutines */
typedef struct
{
  const char *stmtType; /* "CREATE SCHEMA" or "ALTER SCHEMA" */
  char *schemaname;     /* name of schema */
  RoleSpec *authrole;   /* owner of schema */
  List *sequences;      /* CREATE SEQUENCE items */
  List *tables;         /* CREATE TABLE items */
  List *views;          /* CREATE VIEW items */
  List *indexes;        /* CREATE INDEX items */
  List *triggers;       /* CREATE TRIGGER items */
  List *grants;         /* GRANT items */
} CreateSchemaStmtContext;

static void
transformColumnDefinition(CreateStmtContext *cxt, ColumnDef *column);
static void
transformTableConstraint(CreateStmtContext *cxt, Constraint *constraint);
static void
transformTableLikeClause(CreateStmtContext *cxt, TableLikeClause *table_like_clause);
static void
transformOfType(CreateStmtContext *cxt, TypeName *ofTypename);
static CreateStatsStmt *
generateClonedExtStatsStmt(RangeVar *heapRel, Oid heapRelid, Oid source_statsid);
static List *
get_collation(Oid collation, Oid actual_datatype);
static List *
get_opclass(Oid opclass, Oid actual_datatype);
static void
transformIndexConstraints(CreateStmtContext *cxt);
static IndexStmt *
transformIndexConstraint(Constraint *constraint, CreateStmtContext *cxt);
static void
transformExtendedStatistics(CreateStmtContext *cxt);
static void
transformFKConstraints(CreateStmtContext *cxt, bool skipValidation, bool isAddConstraint);
static void
transformCheckConstraints(CreateStmtContext *cxt, bool skipValidation);
static void
transformConstraintAttrs(CreateStmtContext *cxt, List *constraintList);
static void
transformColumnType(CreateStmtContext *cxt, ColumnDef *column);
static void
setSchemaName(char *context_schema, char **stmt_schema_name);
static void
transformPartitionCmd(CreateStmtContext *cxt, PartitionCmd *cmd);
static List *
transformPartitionRangeBounds(ParseState *pstate, List *blist, Relation parent);
static void
validateInfiniteBounds(ParseState *pstate, List *blist);
static Const *
transformPartitionBoundValue(ParseState *pstate, Node *con, const char *colName, Oid colType, int32 colTypmod, Oid partCollation);

/*
 * transformCreateStmt -
 *	  parse analysis for CREATE TABLE
 *
 * Returns a List of utility commands to be done in sequence.  One of these
 * will be the transformed CreateStmt, but there may be additional actions
 * to be done before and after the actual DefineRelation() call.
 * In addition to normal utility commands such as AlterTableStmt and
 * IndexStmt, the result list may contain TableLikeClause(s), representing
 * the need to perform additional parse analysis after DefineRelation().
 *
 * SQL allows constraints to be scattered all over, so thumb through
 * the columns and collect all constraints into one place.
 * If there are any implied indices (e.g. UNIQUE or PRIMARY KEY)
 * then expand those into multiple IndexStmt blocks.
 *	  - thomas 1997-12-02
 */
List *
transformCreateStmt(CreateStmt *stmt, const char *queryString)
{






























































































































































































}

/*
 * generateSerialExtraStmts
 *		Generate CREATE SEQUENCE and ALTER SEQUENCE ... OWNED BY
 *statements to create the sequence for a serial or identity column.
 *
 * This includes determining the name the sequence will have.  The caller
 * can ask to get back the name components by passing non-null pointers
 * for snamespace_p and sname_p.
 */
static void
generateSerialExtraStmts(CreateStmtContext *cxt, ColumnDef *column, Oid seqtypid, List *seqoptions, bool for_identity, char **snamespace_p, char **sname_p)
{















































































































































}

/*
 * transformColumnDefinition -
 *		transform a single ColumnDef within CREATE TABLE
 *		Also used in ALTER TABLE ADD COLUMN
 */
static void
transformColumnDefinition(CreateStmtContext *cxt, ColumnDef *column)
{



































































































































































































































































































}

/*
 * transformTableConstraint
 *		transform a Constraint node within CREATE TABLE or ALTER TABLE
 */
static void
transformTableConstraint(CreateStmtContext *cxt, Constraint *constraint)
{
























































}

/*
 * transformTableLikeClause
 *
 * Change the LIKE <srctable> portion of a CREATE TABLE statement into
 * column definitions that recreate the user defined column portions of
 * <srctable>.  Also, if there are any LIKE options that we can't fully
 * process at this point, add the TableLikeClause to cxt->likeclauses, which
 * will cause utility.c to call expandTableLikeClause() after the new
 * table has been created.
 */
static void
transformTableLikeClause(CreateStmtContext *cxt, TableLikeClause *table_like_clause)
{






































































































































































































}

/*
 * expandTableLikeClause
 *
 * Process LIKE options that require knowing the final column numbers
 * assigned to the new table's columns.  This executes after we have
 * run DefineRelation for the new table.  It returns a list of utility
 * commands that should be run to generate indexes etc.
 */
List *
expandTableLikeClause(RangeVar *heapRel, TableLikeClause *table_like_clause)
{


































































































































































































































}

static void
transformOfType(CreateStmtContext *cxt, TypeName *ofTypename)
{










































}

/*
 * Generate an IndexStmt node using information from an already existing index
 * "source_idx".
 *
 * heapRel is stored into the IndexStmt's relation field, but we don't use it
 * otherwise; some callers pass NULL, if they don't need it to be valid.
 * (The target relation might not exist yet, so we mustn't try to access it.)
 *
 * Attribute numbers in expression Vars are adjusted according to attmap.
 *
 * If constraintOid isn't NULL, we store the OID of any constraint associated
 * with the index there.
 *
 * Unlike transformIndexConstraint, we don't make any effort to force primary
 * key columns to be NOT NULL.  The larger cloning process this is part of
 * should have cloned their NOT NULL status separately (and DefineIndex will
 * complain if that fails to happen).
 */
IndexStmt *
generateClonedIndexStmt(RangeVar *heapRel, Relation source_idx, const AttrNumber *attmap, int attmap_length, Oid *constraintOid)
{


























































































































































































































































































































































}

/*
 * Generate a CreateStatsStmt node using information from an already existing
 * extended statistic "source_statsid", for the rel identified by heapRel and
 * heapRelid.
 */
static CreateStatsStmt *
generateClonedExtStatsStmt(RangeVar *heapRel, Oid heapRelid, Oid source_statsid)
{














































































}

/*
 * get_collation		- fetch qualified name of a collation
 *
 * If collation is InvalidOid or is the default for the given actual_datatype,
 * then the return value is NIL.
 */
static List *
get_collation(Oid collation, Oid actual_datatype)
{





























}

/*
 * get_opclass			- fetch qualified name of an index operator
 * class
 *
 * If the opclass is the default for the given actual_datatype, then
 * the return value is NIL.
 */
static List *
get_opclass(Oid opclass, Oid actual_datatype)
{






















}

/*
 * transformIndexConstraints
 *		Handle UNIQUE, PRIMARY KEY, EXCLUDE constraints, which create
 *indexes. We also merge in any index definitions arising from LIKE ...
 *INCLUDING INDEXES.
 */
static void
transformIndexConstraints(CreateStmtContext *cxt)
{





















































































}

/*
 * transformIndexConstraint
 *		Transform one UNIQUE, PRIMARY KEY, or EXCLUDE constraint for
 *		transformIndexConstraints.
 *
 * We return an IndexStmt.  For a PRIMARY KEY constraint, we additionally
 * produce NOT NULL constraints, either by marking ColumnDefs in cxt->columns
 * as is_not_null or by adding an ALTER TABLE SET NOT NULL command to
 * cxt->alist.
 */
static IndexStmt *
transformIndexConstraint(Constraint *constraint, CreateStmtContext *cxt)
{









































































































































































































































































































































































































































































































}

/*
 * transformExtendedStatistics
 *     Handle extended statistic objects
 *
 * Right now, there's nothing to do here, so we just append the list to
 * the existing "after" list.
 */
static void
transformExtendedStatistics(CreateStmtContext *cxt)
{

}

/*
 * transformCheckConstraints
 *		handle CHECK constraints
 *
 * Right now, there's nothing to do here when called from ALTER TABLE,
 * but the other constraint-transformation functions are called in both
 * the CREATE TABLE and ALTER TABLE paths, so do the same here, and just
 * don't do anything if we're not authorized to skip validation.
 */
static void
transformCheckConstraints(CreateStmtContext *cxt, bool skipValidation)
{






















}

/*
 * transformFKConstraints
 *		handle FOREIGN KEY constraints
 */
static void
transformFKConstraints(CreateStmtContext *cxt, bool skipValidation, bool isAddConstraint)
{























































}

/*
 * transformIndexStmt - parse analysis for CREATE INDEX and ALTER TABLE
 *
 * Note: this is a no-op for an index not using either index expressions or
 * a predicate expression.  There are several code paths that create indexes
 * without bothering to call this, because they know they don't have any
 * such expressions to deal with.
 *
 * To avoid race conditions, it's important that this function rely only on
 * the passed-in relid (and not on stmt->relation) to determine the target
 * relation.
 */
IndexStmt *
transformIndexStmt(Oid relid, IndexStmt *stmt, const char *queryString)
{























































































}

/*
 * transformRuleStmt -
 *	  transform a CREATE RULE Statement. The action is a list of parse
 *	  trees which is transformed into a list of query trees, and we also
 *	  transform the WHERE clause if any.
 *
 * actions and whereClause are output parameters that receive the
 * transformed results.
 *
 * Note that we must not scribble on the passed-in RuleStmt, so we do
 * copyObject() on the actions and WHERE clause.
 */
void
transformRuleStmt(RuleStmt *stmt, const char *queryString, List **actions, Node **whereClause)
{



































































































































































































































































}

/*
 * transformAlterTableStmt -
 *		parse analysis for ALTER TABLE
 *
 * Returns a List of utility commands to be done in sequence.  One of these
 * will be the transformed AlterTableStmt, but there may be additional actions
 * to be done before and after the actual AlterTable() call.
 *
 * To avoid race conditions, it's important that this function rely only on
 * the passed-in relid (and not on stmt->relation) to determine the target
 * relation.
 */
List *
transformAlterTableStmt(Oid relid, AlterTableStmt *stmt, const char *queryString)
{






























































































































































































































































































































































}

/*
 * Preprocess a list of column constraint clauses
 * to attach constraint attributes to their primary constraint nodes
 * and detect inconsistent/misplaced constraint attributes.
 *
 * NOTE: currently, attributes are only supported for FOREIGN KEY, UNIQUE,
 * EXCLUSION, and PRIMARY KEY constraints, but someday they ought to be
 * supported for other constraint types.
 */
static void
transformConstraintAttrs(CreateStmtContext *cxt, List *constraintList)
{






























































































}

/*
 * Special handling of type definition for a column
 */
static void
transformColumnType(CreateStmtContext *cxt, ColumnDef *column)
{



















}

/*
 * transformCreateSchemaStmt -
 *	  analyzes the CREATE SCHEMA statement
 *
 * Split the schema element list into individual commands and place
 * them in the result list in an order such that there are no forward
 * references (e.g. GRANT to a table created later in the list). Note
 * that the logic we use for determining forward references is
 * presently quite incomplete.
 *
 * SQL also allows constraints to make forward references, so thumb through
 * the table columns and move forward references to a posterior alter-table
 * command.
 *
 * The result is a list of parse nodes that still need to be analyzed ---
 * but we can't analyze the later commands until we've executed the earlier
 * ones, because of possible inter-object references.
 *
 * Note: this breaks the rules a little bit by modifying schema-name fields
 * within passed-in structs.  However, the transformation would be the same
 * if done over, so it should be all right to scribble on the input to this
 * extent.
 */
List *
transformCreateSchemaStmt(CreateSchemaStmt *stmt)
{































































































}

/*
 * setSchemaName
 *		Set or check schema name in an element of a CREATE SCHEMA
 *command
 */
static void
setSchemaName(char *context_schema, char **stmt_schema_name)
{








}

/*
 * transformPartitionCmd
 *		Analyze the ATTACH/DETACH PARTITION command
 *
 * In case of the ATTACH PARTITION command, cxt->partbound is set to the
 * transformed value of cmd->bound.
 */
static void
transformPartitionCmd(CreateStmtContext *cxt, PartitionCmd *cmd)
{




























}

/*
 * transformPartitionBound
 *
 * Transform a partition bound specification
 */
PartitionBoundSpec *
transformPartitionBound(ParseState *pstate, Relation parent, PartitionBoundSpec *spec)
{



































































































































}

/*
 * transformPartitionRangeBounds
 *		This converts the expressions for range partition bounds from
 *the raw grammar representation to PartitionRangeDatum structs
 */
static List *
transformPartitionRangeBounds(ParseState *pstate, List *blist, Relation parent)
{




































































































}

/*
 * validateInfiniteBounds
 *
 * Check that a MAXVALUE or MINVALUE specification in a partition bound is
 * followed only by more of the same.
 */
static void
validateInfiniteBounds(ParseState *pstate, List *blist)
{



























}

/*
 * Transform one constant in a partition bound spec
 */
static Const *
transformPartitionBoundValue(ParseState *pstate, Node *val, const char *colName, Oid colType, int32 colTypmod, Oid partCollation)
{












































































}