/*-------------------------------------------------------------------------
 *
 * amapi.c
 *	  Support routines for API for Postgres index access methods.
 *
 * Copyright (c) 2015-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/index/amapi.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/amapi.h"
#include "access/htup_details.h"
#include "catalog/pg_am.h"
#include "catalog/pg_opclass.h"
#include "utils/builtins.h"
#include "utils/syscache.h"

/*
 * GetIndexAmRoutine - call the specified access method handler routine to get
 * its IndexAmRoutine struct, which will be palloc'd in the caller's context.
 *
 * Note that if the amhandler function is built-in, this will not involve
 * any catalog access.  It's therefore safe to use this while bootstrapping
 * indexes for the system catalogs.  relcache.c relies on that.
 */
IndexAmRoutine *
GetIndexAmRoutine(Oid amhandler)
{
  Datum datum;
  IndexAmRoutine *routine;

  datum = OidFunctionCall0(amhandler);
  routine = (IndexAmRoutine *)DatumGetPointer(datum);

  if (routine == NULL || !IsA(routine, IndexAmRoutine))
  {

  }

  return routine;
}

/*
 * GetIndexAmRoutineByAmId - look up the handler of the index access method
 * with the given OID, and get its IndexAmRoutine struct.
 *
 * If the given OID isn't a valid index access method, returns NULL if
 * noerror is true, else throws error.
 */
IndexAmRoutine *
GetIndexAmRoutineByAmId(Oid amoid, bool noerror)
{
  HeapTuple tuple;
  Form_pg_am amform;
  regproc amhandler;

  /* Get handler function OID for the access method */
  tuple = SearchSysCache1(AMOID, ObjectIdGetDatum(amoid));
  if (!HeapTupleIsValid(tuple))
  {





  }
  amform = (Form_pg_am)GETSTRUCT(tuple);

  /* Check if it's an index access method as opposed to some other AM */
  if (amform->amtype != AMTYPE_INDEX)
  {






  }

  amhandler = amform->amhandler;

  /* Complain if handler OID is invalid */
  if (!RegProcedureIsValid(amhandler))
  {






  }

  ReleaseSysCache(tuple);

  /* And finally, call the handler function to get the API struct. */
  return GetIndexAmRoutine(amhandler);
}

/*
 * Ask appropriate access method to validate the specified opclass.
 */
Datum
amvalidate(PG_FUNCTION_ARGS)
{
  Oid opclassoid = PG_GETARG_OID(0);
  bool result;
  HeapTuple classtup;
  Form_pg_opclass classform;
  Oid amoid;
  IndexAmRoutine *amroutine;

  classtup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclassoid));
  if (!HeapTupleIsValid(classtup))
  {

  }
  classform = (Form_pg_opclass)GETSTRUCT(classtup);

  amoid = classform->opcmethod;

  ReleaseSysCache(classtup);

  amroutine = GetIndexAmRoutineByAmId(amoid, false);

  if (amroutine->amvalidate == NULL)
  {

  }

  result = amroutine->amvalidate(opclassoid);

  pfree(amroutine);

  PG_RETURN_BOOL(result);
}