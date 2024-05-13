/*-------------------------------------------------------------------------
 *
 * misc.c
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/misc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/tablespace.h"
#include "common/keywords.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "parser/scansup.h"
#include "postmaster/syslogger.h"
#include "rewrite/rewriteHandler.h"
#include "storage/fd.h"
#include "utils/lsyscache.h"
#include "utils/ruleutils.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

/*
 * Common subroutine for num_nulls() and num_nonnulls().
 * Returns true if successful, false if function should return NULL.
 * If successful, total argument count and number of nulls are
 * returned into *nargs and *nulls.
 */
static bool
count_nulls(FunctionCallInfo fcinfo, int32 *nargs, int32 *nulls)
{
  int32 count = 0;
  int i;

  /* Did we get a VARIADIC array argument, or separate arguments? */
  if (get_fn_expr_variadic(fcinfo->flinfo))
  {
    ArrayType *arr;
    int ndims, nitems, *dims;
    bits8 *bitmap;

    Assert(PG_NARGS() == 1);

    /*
     * If we get a null as VARIADIC array argument, we can't say anything
     * useful about the number of elements, so return NULL.  This behavior
     * is consistent with other variadic functions - see concat_internal.
     */
    if (PG_ARGISNULL(0))
    {
      return false;
    }

    /*
     * Non-null argument had better be an array.  We assume that any call
     * context that could let get_fn_expr_variadic return true will have
     * checked that a VARIADIC-labeled parameter actually is an array.  So
     * it should be okay to just Assert that it's an array rather than
     * doing a full-fledged error check.
     */
    Assert(OidIsValid(get_base_element_type(get_fn_expr_argtype(fcinfo->flinfo, 0))));

    /* OK, safe to fetch the array value */
    arr = PG_GETARG_ARRAYTYPE_P(0);

    /* Count the array elements */
    ndims = ARR_NDIM(arr);
    dims = ARR_DIMS(arr);
    nitems = ArrayGetNItems(ndims, dims);

    /* Count those that are NULL */
    bitmap = ARR_NULLBITMAP(arr);
    if (bitmap)
    {
      int bitmask = 1;

      for (i = 0; i < nitems; i++)
      {
        if ((*bitmap & bitmask) == 0)
        {
          count++;
        }

        bitmask <<= 1;
        if (bitmask == 0x100)
        {
          bitmap++;
          bitmask = 1;
        }
      }
    }

    *nargs = nitems;
    *nulls = count;
  }
  else
  {
    /* Separate arguments, so just count 'em */
    for (i = 0; i < PG_NARGS(); i++)
    {
      if (PG_ARGISNULL(i))
      {
        count++;
      }
    }

    *nargs = PG_NARGS();
    *nulls = count;
  }

  return true;
}

/*
 * num_nulls()
 *	Count the number of NULL arguments
 */
Datum
pg_num_nulls(PG_FUNCTION_ARGS)
{
  int32 nargs, nulls;

  if (!count_nulls(fcinfo, &nargs, &nulls))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT32(nulls);
}

/*
 * num_nonnulls()
 *	Count the number of non-NULL arguments
 */
Datum
pg_num_nonnulls(PG_FUNCTION_ARGS)
{
  int32 nargs, nulls;

  if (!count_nulls(fcinfo, &nargs, &nulls))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT32(nargs - nulls);
}

/*
 * current_database()
 *	Expose the current database to the user
 */
Datum
current_database(PG_FUNCTION_ARGS)
{
  Name db;

  db = (Name)palloc(NAMEDATALEN);

  namestrcpy(db, get_database_name(MyDatabaseId));
  PG_RETURN_NAME(db);
}

/*
 * current_query()
 *	Expose the current query to the user (useful in stored procedures)
 *	We might want to use ActivePortal->sourceText someday.
 */
Datum
current_query(PG_FUNCTION_ARGS)
{









}

/* Function to find out which databases make use of a tablespace */

Datum
pg_tablespace_databases(PG_FUNCTION_ARGS)
{
  Oid tablespaceOid = PG_GETARG_OID(0);
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  char *location;
  DIR *dirdesc;
  struct dirent *de;
  MemoryContext oldcontext;

  /* check to see if caller supports us returning a tuplestore */
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {

  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {

  }

  /* The tupdesc and tuplestore must be created in ecxt_per_query_memory */
  oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

  tupdesc = CreateTemplateTupleDesc(1);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "pg_tablespace_databases", OIDOID, -1, 0);

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);

  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  if (tablespaceOid == GLOBALTABLESPACE_OID)
  {

    /* return empty tuplestore */

  }

  if (tablespaceOid == DEFAULTTABLESPACE_OID)
  {
    location = psprintf("base");
  }
  else
  {

  }

  dirdesc = AllocateDir(location);

  if (!dirdesc)
  {
    /* the only expected error is ENOENT */





    /* return empty tuplestore */

  }

  while ((de = ReadDir(dirdesc, location)) != NULL)
  {
    Oid datOid = atooid(de->d_name);
    char *subdir;
    bool isempty;
    Datum values[1];
    bool nulls[1];

    /* this test skips . and .., but is awfully weak */
    if (!datOid)
    {
      continue;
    }

    /* if database subdir is empty, don't report tablespace as used */

    subdir = psprintf("%s/%s", location, de->d_name);
    isempty = directory_is_empty(subdir);
    pfree(subdir);

    if (isempty)
    {

    }

    values[0] = ObjectIdGetDatum(datOid);
    nulls[0] = false;

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  FreeDir(dirdesc);
  return (Datum)0;
}

/*
 * pg_tablespace_location - get location for a tablespace
 */
Datum
pg_tablespace_location(PG_FUNCTION_ARGS)
{












































































}

/*
 * pg_sleep - delay for N seconds
 */
Datum
pg_sleep(PG_FUNCTION_ARGS)
{
  float8 secs = PG_GETARG_FLOAT8(0);
  float8 endtime;

  /*
   * We sleep using WaitLatch, to ensure that we'll wake up promptly if an
   * important signal (such as SIGALRM or SIGINT) arrives.  Because
   * WaitLatch's upper limit of delay is INT_MAX milliseconds, and the user
   * might ask for more than that, we sleep for at most 10 minutes and then
   * loop.
   *
   * By computing the intended stop time initially, we avoid accumulation of
   * extra delay across multiple sleeps.  This also ensures we won't delay
   * less than the specified time when WaitLatch is terminated early by a
   * non-query-canceling signal such as SIGHUP.
   */
#define GetNowFloat() ((float8)GetCurrentTimestamp() / 1000000.0)

  endtime = GetNowFloat() + secs;

  for (;;)
  {
    float8 delay;
    long delay_ms;

    CHECK_FOR_INTERRUPTS();

    delay = endtime - GetNowFloat();
    if (delay >= 600.0)
    {

    }
    else if (delay > 0.0)
    {
      delay_ms = (long)ceil(delay * 1000.0);
    }
    else
    {
      break;
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, delay_ms, WAIT_EVENT_PG_SLEEP);
    ResetLatch(MyLatch);
  }

  PG_RETURN_VOID();
}

/* Function to return the list of grammar keywords */
Datum
pg_get_keywords(PG_FUNCTION_ARGS)
{




























































}

/*
 * Return the type of the argument.
 */
Datum
pg_typeof(PG_FUNCTION_ARGS)
{
  PG_RETURN_OID(get_fn_expr_argtype(fcinfo->flinfo, 0));
}

/*
 * Implementation of the COLLATE FOR expression; returns the collation
 * of the argument.
 */
Datum
pg_collation_for(PG_FUNCTION_ARGS)
{
  Oid typeid;
  Oid collid;

  typeid = get_fn_expr_argtype(fcinfo->flinfo, 0);
  if (!typeid)
  {

  }
  if (!type_is_collatable(typeid) && typeid != UNKNOWNOID)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be(typeid))));
  }

  collid = PG_GET_COLLATION();
  if (!collid)
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_TEXT_P(cstring_to_text(generate_collation_name(collid)));
}

/*
 * pg_relation_is_updatable - determine which update events the specified
 * relation supports.
 *
 * This relies on relation_is_updatable() in rewriteHandler.c, which see
 * for additional information.
 */
Datum
pg_relation_is_updatable(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  bool include_triggers = PG_GETARG_BOOL(1);

  PG_RETURN_INT32(relation_is_updatable(reloid, NIL, include_triggers, NULL));
}

/*
 * pg_column_is_updatable - determine whether a column is updatable
 *
 * This function encapsulates the decision about just what
 * information_schema.columns.is_updatable actually means.  It's not clear
 * whether deletability of the column's relation should be required, so
 * we want that decision in C code where we could change it without initdb.
 */
Datum
pg_column_is_updatable(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  AttrNumber attnum = PG_GETARG_INT16(1);
  AttrNumber col = attnum - FirstLowInvalidHeapAttributeNumber;
  bool include_triggers = PG_GETARG_BOOL(2);
  int events;

  /* System columns are never updatable */
  if (attnum <= 0)
  {

  }

  events = relation_is_updatable(reloid, NIL, include_triggers, bms_make_singleton(col));

  /* We require both updatability and deletability of the relation */
#define REQ_EVENTS ((1 << CMD_UPDATE) | (1 << CMD_DELETE))

  PG_RETURN_BOOL((events & REQ_EVENTS) == REQ_EVENTS);
}

/*
 * Is character a valid identifier start?
 * Must match scan.l's {ident_start} character class.
 */
static bool
is_ident_start(unsigned char c)
{















}

/*
 * Is character a valid identifier continuation?
 * Must match scan.l's {ident_cont} character class.
 */
static bool
is_ident_cont(unsigned char c)
{







}

/*
 * parse_ident - parse a SQL qualified identifier into separate identifiers.
 * When strict mode is active (second parameter), then any chars after
 * the last identifier are disallowed.
 */
Datum
parse_ident(PG_FUNCTION_ARGS)
{































































































































}

/*
 * pg_current_logfile
 *
 * Report current log file used by log collector by scanning current_logfiles.
 */
Datum
pg_current_logfile(PG_FUNCTION_ARGS)
{












































































}

/*
 * Report current log file used by log collector (1 argument version)
 *
 * note: this wrapper is necessary to pass the sanity check in opr_sanity,
 * which checks that all built-in functions that share the implementing C
 * function take the same number of arguments
 */
Datum
pg_current_logfile_1arg(PG_FUNCTION_ARGS)
{

}

/*
 * SQL wrapper around RelationGetReplicaIndex().
 */
Datum
pg_get_replica_identity_index(PG_FUNCTION_ARGS)
{
















}