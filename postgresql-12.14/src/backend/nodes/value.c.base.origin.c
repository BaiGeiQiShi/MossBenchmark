/*-------------------------------------------------------------------------
 *
 * value.c
 *	  implementation of Value nodes
 *
 *
 * Copyright (c) 2003-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/value.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/parsenodes.h"

/*
 *	makeInteger
 */
Value *
makeInteger(int i)
{





}

/*
 *	makeFloat
 *
 * Caller is responsible for passing a palloc'd string.
 */
Value *
makeFloat(char *numericStr)
{





}

/*
 *	makeString
 *
 * Caller is responsible for passing a palloc'd string.
 */
Value *
makeString(char *str)
{





}

/*
 *	makeBitString
 *
 * Caller is responsible for passing a palloc'd string.
 */
Value *
makeBitString(char *str)
{





}