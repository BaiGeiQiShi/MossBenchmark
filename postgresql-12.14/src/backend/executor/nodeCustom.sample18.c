/* ------------------------------------------------------------------------
 *
 * nodeCustom.c
 *		Routines to handle execution of custom scan node
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * ------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/parallel.h"
#include "executor/executor.h"
#include "executor/nodeCustom.h"
#include "nodes/execnodes.h"
#include "nodes/extensible.h"
#include "nodes/plannodes.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static TupleTableSlot *
ExecCustomScan(PlanState *pstate);

CustomScanState *
ExecInitCustomScan(CustomScan *cscan, EState *estate, int eflags)
{







































































}

static TupleTableSlot *
ExecCustomScan(PlanState *pstate)
{






}

void
ExecEndCustomScan(CustomScanState *node)
{









}

void
ExecReScanCustomScan(CustomScanState *node)
{


}

void
ExecCustomMarkPos(CustomScanState *node)
{





}

void
ExecCustomRestrPos(CustomScanState *node)
{





}

void
ExecCustomScanEstimate(CustomScanState *node, ParallelContext *pcxt)
{








}

void
ExecCustomScanInitializeDSM(CustomScanState *node, ParallelContext *pcxt)
{











}

void
ExecCustomScanReInitializeDSM(CustomScanState *node, ParallelContext *pcxt)
{










}

void
ExecCustomScanInitializeWorker(CustomScanState *node, ParallelWorkerContext *pwcxt)
{










}

void
ExecShutdownCustomScan(CustomScanState *node)
{






}