/*-------------------------------------------------------------------------
 *
 * brin_validate.c
 *	  Opclass validator for BRIN.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_validate.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/amvalidate.h"
#include "access/brin_internal.h"
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/regproc.h"

/*
 * Validator for a BRIN opclass.
 *
 * Some of the checks done here cover the whole opfamily, and therefore are
 * redundant when checking each opclass in a family.  But they don't run long
 * enough to be much of a problem, so we accept the duplication rather than
 * complicate the amvalidate API.
 */
bool
brinvalidate(Oid opclassoid)
{















































































































































































}
