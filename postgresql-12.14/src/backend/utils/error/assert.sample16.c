/*-------------------------------------------------------------------------
 *
 * assert.c
 *	  Assert code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/error/assert.c
 *
 * NOTE
 *	  This should eventually work with elog()
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

/*
 * ExceptionalCondition - Handles the failure of an Assert()
 */
void
ExceptionalCondition(const char *conditionName, const char *errorType, const char *fileName, int lineNumber)
{






















}