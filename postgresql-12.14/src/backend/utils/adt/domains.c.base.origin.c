/*-------------------------------------------------------------------------
 *
 * domains.c
 *	  I/O functions for domain types.
 *
 * The output functions for a domain type are just the same ones provided
 * by its underlying base type.  The input functions, however, must be
 * prepared to apply any constraints defined by the type.  So, we create
 * special input functions that invoke the base type's input function
 * and then check the constraints.
 *
 * The overhead required for constraint checking can be high, since examining
 * the catalogs to discover the constraints for a given domain is not cheap.
 * We have three mechanisms for minimizing this cost:
 *	1.  We rely on the typcache to keep up-to-date copies of the
 *constraints.
 *	2.  In a nest of domains, we flatten the checking of all the levels
 *		into just one operation (the typcache does this for us).
 *	3.  If there are CHECK constraints, we cache a standalone ExprContext
 *		to evaluate them in.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/domains.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/expandeddatum.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/*
 * structure to cache state across multiple calls
 */
typedef struct DomainIOData
{
  Oid domain_type;
  /* Data needed to call base type's input function */
  Oid typiofunc;
  Oid typioparam;
  int32 typtypmod;
  FmgrInfo proc;
  /* Reference to cached list of constraint items to check */
  DomainConstraintRef constraint_ref;
  /* Context for evaluating CHECK constraints in */
  ExprContext *econtext;
  /* Memory context this cache is in */
  MemoryContext mcxt;
} DomainIOData;

/*
 * domain_state_setup - initialize the cache for a new domain type.
 *
 * Note: we can't re-use the same cache struct for a new domain type,
 * since there's no provision for releasing the DomainConstraintRef.
 * If a call site needs to deal with a new domain type, we just leak
 * the old struct for the duration of the query.
 */
static DomainIOData *
domain_state_setup(Oid domainType, bool binary, MemoryContext mcxt)
{













































}

/*
 * domain_check_input - apply the cached checks.
 *
 * This is roughly similar to the handling of CoerceToDomain nodes in
 * execExpr*.c, but we execute each constraint separately, rather than
 * compiling them in-line within a larger expression.
 */
static void
domain_check_input(Datum value, bool isnull, DomainIOData *my_extra)
{

































































}

/*
 * domain_in		- input routine for any domain type.
 */
Datum
domain_in(PG_FUNCTION_ARGS)
{






















































}

/*
 * domain_recv		- binary input routine for any domain type.
 */
Datum
domain_recv(PG_FUNCTION_ARGS)
{






















































}

/*
 * domain_check - check that a datum satisfies the constraints of a
 * domain.  extra and mcxt can be passed if they are available from,
 * say, a FmgrInfo structure, or they can be NULL, in which case the
 * setup is repeated for each call.
 */
void
domain_check(Datum value, bool isnull, Oid domainType, void **extra, MemoryContext mcxt)
{





























}

/*
 * errdatatype --- stores schema_name and datatype_name of a datatype
 * within the current errordata.
 */
int
errdatatype(Oid datatypeOid)
{
















}

/*
 * errdomainconstraint --- stores schema_name, datatype_name and
 * constraint_name of a domain-related constraint within the current errordata.
 */
int
errdomainconstraint(Oid datatypeOid, const char *conname)
{




}