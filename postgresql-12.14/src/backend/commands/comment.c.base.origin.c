/*-------------------------------------------------------------------------
 *
 * comment.c
 *
 * PostgreSQL object comments utility code.
 *
 * Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/commands/comment.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_description.h"
#include "catalog/pg_shdescription.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

/*
 * CommentObject --
 *
 * This routine is used to add the associated comment into
 * pg_description for the object specified by the given SQL command.
 */
ObjectAddress
CommentObject(CommentStmt *stmt)
{


















































































}

/*
 * CreateComments --
 *
 * Create a comment for the specified object descriptor.  Inserts a new
 * pg_description tuple, or replaces an existing one with the same key.
 *
 * If the comment given is null or an empty string, instead delete any
 * existing comment for the specified key.
 */
void
CreateComments(Oid oid, Oid classoid, int32 subid, const char *comment)
{











































































}

/*
 * CreateSharedComments --
 *
 * Create a comment for the specified shared object descriptor.  Inserts a
 * new pg_shdescription tuple, or replaces an existing one with the same key.
 *
 * If the comment given is null or an empty string, instead delete any
 * existing comment for the specified key.
 */
void
CreateSharedComments(Oid oid, Oid classoid, const char *comment)
{









































































}

/*
 * DeleteComments -- remove comments for an object
 *
 * If subid is nonzero then only comments matching it will be removed.
 * If subid is zero, all comments matching the oid/classoid will be removed
 * (this corresponds to deleting a whole object).
 */
void
DeleteComments(Oid oid, Oid classoid, int32 subid)
{


































}

/*
 * DeleteSharedComments -- remove comments for a shared object
 */
void
DeleteSharedComments(Oid oid, Oid classoid)
{























}

/*
 * GetComment -- get the comment for an object, or null if not found.
 */
char *
GetComment(Oid oid, Oid classoid, int32 subid)
{







































}