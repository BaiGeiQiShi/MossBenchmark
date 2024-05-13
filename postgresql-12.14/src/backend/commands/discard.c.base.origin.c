/*-------------------------------------------------------------------------
 *
 * discard.c
 *	  The implementation of the DISCARD command
 *
 * Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/discard.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/namespace.h"
#include "commands/async.h"
#include "commands/discard.h"
#include "commands/prepare.h"
#include "commands/sequence.h"
#include "utils/guc.h"
#include "utils/portal.h"

static void
DiscardAll(bool isTopLevel);

/*
 * DISCARD { ALL | SEQUENCES | TEMP | PLANS }
 */
void
DiscardCommand(DiscardStmt *stmt, bool isTopLevel)
{





















}

static void
DiscardAll(bool isTopLevel)
{



















}