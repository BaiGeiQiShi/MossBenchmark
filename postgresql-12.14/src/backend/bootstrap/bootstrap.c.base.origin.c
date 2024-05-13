/*-------------------------------------------------------------------------
 *
 * bootstrap.c
 *	  routines to support running postgres in 'bootstrap' mode
 *	bootstrap mode is used to create the initial template database
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/bootstrap/bootstrap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#include <signal.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "bootstrap/bootstrap.h"
#include "catalog/index.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "common/link-canary.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "pg_getopt.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "postmaster/startup.h"
#include "postmaster/walwriter.h"
#include "replication/walreceiver.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/condition_variable.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/rel.h"
#include "utils/relmapper.h"

uint32 bootstrap_data_checksum_version = 0; /* No checksum */

#define ALLOC(t, c) ((t *)MemoryContextAllocZero(TopMemoryContext, (unsigned)(c) * sizeof(t)))

static void
CheckerModeMain(void);
static void
BootstrapModeMain(void);
static void
bootstrap_signals(void);
static void
ShutdownAuxiliaryProcess(int code, Datum arg);
static Form_pg_attribute
AllocateAttribute(void);
static Oid
gettype(char *type);
static void
cleanup(void);

/* ----------------
 *		global variables
 * ----------------
 */

AuxProcType MyAuxProcType = NotAnAuxProcess; /* declared in miscadmin.h */

Relation boot_reldesc; /* current relation descriptor */

Form_pg_attribute attrtypes[MAXATTR]; /* points to attribute info */
int numattr;                          /* number of attributes for cur. rel */

/*
 * Basic information associated with each type.  This is used before
 * pg_type is filled, so it has to cover the datatypes used as column types
 * in the core "bootstrapped" catalogs.
 *
 *		XXX several of these input/output functions do catalog scans
 *			(e.g., F_REGPROCIN scans pg_proc).  this obviously
 *creates some order dependencies in the catalog creation process.
 */
struct typinfo
{
  char name[NAMEDATALEN];
  Oid oid;
  Oid elem;
  int16 len;
  bool byval;
  char align;
  char storage;
  Oid collation;
  Oid inproc;
  Oid outproc;
};

static const struct typinfo TypInfo[] = {{"bool", BOOLOID, 0, 1, true, 'c', 'p', InvalidOid, F_BOOLIN, F_BOOLOUT}, {"bytea", BYTEAOID, 0, -1, false, 'i', 'x', InvalidOid, F_BYTEAIN, F_BYTEAOUT}, {"char", CHAROID, 0, 1, true, 'c', 'p', InvalidOid, F_CHARIN, F_CHAROUT}, {"int2", INT2OID, 0, 2, true, 's', 'p', InvalidOid, F_INT2IN, F_INT2OUT}, {"int4", INT4OID, 0, 4, true, 'i', 'p', InvalidOid, F_INT4IN, F_INT4OUT}, {"float4", FLOAT4OID, 0, 4, FLOAT4PASSBYVAL, 'i', 'p', InvalidOid, F_FLOAT4IN, F_FLOAT4OUT}, {"name", NAMEOID, CHAROID, NAMEDATALEN, false, 'c', 'p', C_COLLATION_OID, F_NAMEIN, F_NAMEOUT}, {"regclass", REGCLASSOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGCLASSIN, F_REGCLASSOUT}, {"regproc", REGPROCOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGPROCIN, F_REGPROCOUT}, {"regtype", REGTYPEOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGTYPEIN, F_REGTYPEOUT}, {"regrole", REGROLEOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGROLEIN, F_REGROLEOUT},
    {"regnamespace", REGNAMESPACEOID, 0, 4, true, 'i', 'p', InvalidOid, F_REGNAMESPACEIN, F_REGNAMESPACEOUT}, {"text", TEXTOID, 0, -1, false, 'i', 'x', DEFAULT_COLLATION_OID, F_TEXTIN, F_TEXTOUT}, {"oid", OIDOID, 0, 4, true, 'i', 'p', InvalidOid, F_OIDIN, F_OIDOUT}, {"tid", TIDOID, 0, 6, false, 's', 'p', InvalidOid, F_TIDIN, F_TIDOUT}, {"xid", XIDOID, 0, 4, true, 'i', 'p', InvalidOid, F_XIDIN, F_XIDOUT}, {"cid", CIDOID, 0, 4, true, 'i', 'p', InvalidOid, F_CIDIN, F_CIDOUT}, {"pg_node_tree", PGNODETREEOID, 0, -1, false, 'i', 'x', DEFAULT_COLLATION_OID, F_PG_NODE_TREE_IN, F_PG_NODE_TREE_OUT}, {"int2vector", INT2VECTOROID, INT2OID, -1, false, 'i', 'p', InvalidOid, F_INT2VECTORIN, F_INT2VECTOROUT}, {"oidvector", OIDVECTOROID, OIDOID, -1, false, 'i', 'p', InvalidOid, F_OIDVECTORIN, F_OIDVECTOROUT}, {"_int4", INT4ARRAYOID, INT4OID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}, {"_text", 1009, TEXTOID, -1, false, 'i', 'x', DEFAULT_COLLATION_OID, F_ARRAY_IN, F_ARRAY_OUT},
    {"_oid", 1028, OIDOID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}, {"_char", 1002, CHAROID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}, {"_aclitem", 1034, ACLITEMOID, -1, false, 'i', 'x', InvalidOid, F_ARRAY_IN, F_ARRAY_OUT}};

static const int n_types = sizeof(TypInfo) / sizeof(struct typinfo);

struct typmap
{ /* a hack */
  Oid am_oid;
  FormData_pg_type am_typ;
};

static struct typmap **Typ = NULL;
static struct typmap *Ap = NULL;

static Datum values[MAXATTR]; /* current row's attribute values */
static bool Nulls[MAXATTR];

static MemoryContext nogc = NULL; /* special no-gc mem context */

/*
 *	At bootstrap time, we first declare all the indices to be built, and
 *	then build them.  The IndexList structure stores enough information
 *	to allow us to build the indices after they've been declared.
 */

typedef struct _IndexList
{
  Oid il_heap;
  Oid il_ind;
  IndexInfo *il_info;
  struct _IndexList *il_next;
} IndexList;

static IndexList *ILHead = NULL;

/*
 *	 AuxiliaryProcessMain
 *
 *	 The main entry point for auxiliary processes, such as the bgwriter,
 *	 walwriter, walreceiver, bootstrapper and the shared memory checker
 *code.
 *
 *	 This code is here just because of historical reasons.
 */
void
AuxiliaryProcessMain(int argc, char *argv[])
{




























































































































































































































































































}

/*
 * In shared memory checker mode, all we really want to do is create shared
 * memory and semaphores (just to prove we can do it with the current GUC
 * settings).  Since, in fact, that was already done by BaseInit(),
 * we have nothing more to do here.
 */
static void
CheckerModeMain(void)
{

}

/*
 *	 The main entry point for running the backend in bootstrap mode
 *
 *	 The bootstrap mode is used to initialize the template database.
 *	 The bootstrap backend doesn't speak SQL, but instead expects
 *	 commands in a special bootstrap language.
 */
static void
BootstrapModeMain(void)
{












































}

/* ----------------------------------------------------------------
 *						misc functions
 * ----------------------------------------------------------------
 */

/*
 * Set up signal handling for a bootstrap process
 */
static void
bootstrap_signals(void)
{











}

/*
 * Begin shutdown of an auxiliary process.  This is approximately the equivalent
 * of ShutdownPostgres() in postinit.c.  We can't run transactions in an
 * auxiliary process, so most of the work of AbortTransaction() is not needed,
 * but we do need to make sure we've released any LWLocks we are holding.
 * (This is only critical during an error exit.)
 */
static void
ShutdownAuxiliaryProcess(int code, Datum arg)
{



}

/* ----------------------------------------------------------------
 *				MANUAL BACKEND INTERACTIVE INTERFACE COMMANDS
 * ----------------------------------------------------------------
 */

/* ----------------
 *		boot_openrel
 * ----------------
 */
void
boot_openrel(char *relname)
{































































}

/* ----------------
 *		closerel
 * ----------------
 */
void
closerel(char *name)
{

























}

/* ----------------
 * DEFINEATTR()
 *
 * define a <field,type> pair
 * if there are n fields in a relation to be created, this routine
 * will be called n times
 * ----------------
 */
void
DefineAttr(char *name, char *type, int attnum, int nullness)
{

















































































































}

/* ----------------
 *		InsertOneTuple
 *
 * If objectid is not zero, it is a specific OID to assign to the tuple.
 * Otherwise, an OID will be assigned (if necessary) by heap_insert.
 * ----------------
 */
void
InsertOneTuple(void)
{





















}

/* ----------------
 *		InsertOneValue
 * ----------------
 */
void
InsertOneValue(char *value, int i)
{
























}

/* ----------------
 *		InsertOneNull
 * ----------------
 */
void
InsertOneNull(int i)
{








}

/* ----------------
 *		cleanup
 * ----------------
 */
static void
cleanup(void)
{




}

/* ----------------
 *		gettype
 *
 * NB: this is really ugly; it will return an integer index into TypInfo[],
 * and not an OID at all, until the first reference to a type not known in
 * TypInfo[].  At that point it will read and cache pg_type in the Typ array,
 * and subsequently return a real OID (and set the global pointer Ap to
 * point at the found row in Typ).  So caller must check whether Typ is
 * still NULL to determine what the return value is!
 * ----------------
 */
static Oid
gettype(char *type)
{























































}

/* ----------------
 *		boot_get_type_io_data
 *
 * Obtain type I/O information at bootstrap time.  This intentionally has
 * almost the same API as lsyscache.c's get_type_io_data, except that
 * we only support obtaining the typinput and typoutput routines, not
 * the binary I/O routines.  It is exported so that array_in and array_out
 * can be made to work during early bootstrap.
 * ----------------
 */
void
boot_get_type_io_data(Oid typid, int16 *typlen, bool *typbyval, char *typalign, char *typdelim, Oid *typioparam, Oid *typinput, Oid *typoutput)
{







































































}

/* ----------------
 *		AllocateAttribute
 *
 * Note: bootstrap never sets any per-column ACLs, so we only need
 * ATTRIBUTE_FIXED_PART_SIZE space per attribute.
 * ----------------
 */
static Form_pg_attribute
AllocateAttribute(void)
{

}

/*
 *	index_register() -- record an index that has been set up for building
 *						later.
 *
 *		At bootstrap time, we define a bunch of indexes on system
 *catalogs. We postpone actually building the indexes until just before we're
 *		finished with initialization, however.  This is because the
 *indexes themselves have catalog entries, and those have to be included in the
 *		indexes on those catalogs.  Doing it in two phases is the
 *simplest way of making sure the indexes have the right contents at the end.
 */
void
index_register(Oid heap, Oid ind, IndexInfo *indexInfo)
{





































}

/*
 * build_indices -- fill in all the indexes registered earlier
 */
void
build_indices(void)
{














}