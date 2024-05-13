/*-------------------------------------------------------------------------
 *
 * extensible.c
 *	  Support for extensible node types
 *
 * Loadable modules can define what are in effect new types of nodes using
 * the routines in this file.  All such nodes are flagged T_ExtensibleNode,
 * with the extnodename field distinguishing the specific type.  Use
 * RegisterExtensibleNodeMethods to register a new type of extensible node,
 * and GetExtensibleNodeMethods to get information about a previously
 * registered type of extensible node.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/nodes/extensible.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/extensible.h"
#include "utils/hsearch.h"

static HTAB *extensible_node_methods = NULL;
static HTAB *custom_scan_methods = NULL;

typedef struct
{
  char extnodename[EXTNODENAME_MAX_LEN];
  const void *extnodemethods;
} ExtensibleNodeEntry;

/*
 * An internal function to register a new callback structure
 */
static void
RegisterExtensibleNodeEntry(HTAB **p_htable, const char *htable_label, const char *extnodename, const void *extnodemethods)
{


























}

/*
 * Register a new type of extensible node.
 */
void
RegisterExtensibleNodeMethods(const ExtensibleNodeMethods *methods)
{

}

/*
 * Register a new type of custom scan node
 */
void
RegisterCustomScanMethods(const CustomScanMethods *methods)
{

}

/*
 * An internal routine to get an ExtensibleNodeEntry by the given identifier
 */
static const void *
GetExtensibleNodeEntry(HTAB *htable, const char *extnodename, bool missing_ok)
{
















}

/*
 * Get the methods for a given type of extensible node.
 */
const ExtensibleNodeMethods *
GetExtensibleNodeMethods(const char *extnodename, bool missing_ok)
{

}

/*
 * Get the methods for a given name of CustomScanMethods
 */
const CustomScanMethods *
GetCustomScanMethods(const char *CustomName, bool missing_ok)
{

}