/*
 *	pg_upgrade_support.c
 *
 *	server-side functions to set backend global variables
 *	to control oid and relfilenode assignment, and do other special
 *	hacks needed for pg_upgrade.
 *
 *	Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *	src/backend/utils/adt/pg_upgrade_support.c
 */

#include "postgres.h"

#include "catalog/binary_upgrade.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "commands/extension.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"

#define CHECK_IS_BINARY_UPGRADE                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  do {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (!IsBinaryUpgrade)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      ereport(ERROR, (errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM), (errmsg("function can only be called when server is in "                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                          "binary upgrade mode"))));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

Datum
binary_upgrade_set_next_pg_type_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_array_pg_type_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_toast_pg_type_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_heap_pg_class_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_index_pg_class_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_toast_pg_class_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_pg_enum_oid(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_next_pg_authid_oid(PG_FUNCTION_ARGS)
{





}

Datum
binary_upgrade_create_empty_extension(PG_FUNCTION_ARGS)
{



















































}

Datum
binary_upgrade_set_record_init_privs(PG_FUNCTION_ARGS)
{






}

Datum
binary_upgrade_set_missing_value(PG_FUNCTION_ARGS)
{










}