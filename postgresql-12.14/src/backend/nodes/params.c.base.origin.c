/*-------------------------------------------------------------------------
 *
 * params.c
 *	  Support for finding the values associated with Param nodes.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/nodes/params.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/bitmapset.h"
#include "nodes/params.h"
#include "storage/shmem.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"

/*
 * Allocate and initialize a new ParamListInfo structure.
 *
 * To make a new structure for the "dynamic" way (with hooks), pass 0 for
 * numParams and set numParams manually.
 */
ParamListInfo
makeParamList(int numParams)
{















}

/*
 * Copy a ParamListInfo structure.
 *
 * The result is allocated in CurrentMemoryContext.
 *
 * Note: the intent of this function is to make a static, self-contained
 * set of parameter values.  If dynamic parameter hooks are present, we
 * intentionally do not copy them into the result.  Rather, we forcibly
 * instantiate all available parameter values and copy the datum values.
 */
ParamListInfo
copyParamList(ParamListInfo from)
{








































}

/*
 * Estimate the amount of space required to serialize a ParamListInfo.
 */
Size
EstimateParamListSpace(ParamListInfo paramLI)
{














































}

/*
 * Serialize a paramListInfo structure into caller-provided storage.
 *
 * We write the number of parameters first, as a 4-byte integer, and then
 * write details for each parameter in turn.  The details for each parameter
 * consist of a 4-byte type OID, 2 bytes of flags, and then the datum as
 * serialized by datumSerialize().  The caller is responsible for ensuring
 * that there is enough storage to store the number of bytes that will be
 * written; use EstimateParamListSpace to find out how many will be needed.
 * *start_address is updated to point to the byte immediately following those
 * written.
 *
 * RestoreParamList can be used to recreate a ParamListInfo based on the
 * serialized representation; this will be a static, self-contained copy
 * just as copyParamList would create.
 */
void
SerializeParamList(ParamListInfo paramLI, char **start_address)
{

























































}

/*
 * Copy a ParamListInfo structure.
 *
 * The result is allocated in CurrentMemoryContext.
 *
 * Note: the intent of this function is to make a static, self-contained
 * set of parameter values.  If dynamic parameter hooks are present, we
 * intentionally do not copy them into the result.  Rather, we forcibly
 * instantiate all available parameter values and copy the datum values.
 */
ParamListInfo
RestoreParamList(char **start_address)
{

























}