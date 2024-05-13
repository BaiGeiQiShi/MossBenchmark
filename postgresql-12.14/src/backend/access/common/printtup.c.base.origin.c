/*-------------------------------------------------------------------------
 *
 * printtup.c
 *	  Routines to print out tuples to the destination (both frontend
 *	  clients and standalone backends are supported here).
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/common/printtup.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/printtup.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "tcop/pquery.h"
#include "utils/lsyscache.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"

static void
printtup_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static bool
printtup(TupleTableSlot *slot, DestReceiver *self);
static bool
printtup_20(TupleTableSlot *slot, DestReceiver *self);
static bool
printtup_internal_20(TupleTableSlot *slot, DestReceiver *self);
static void
printtup_shutdown(DestReceiver *self);
static void
printtup_destroy(DestReceiver *self);

static void
SendRowDescriptionCols_2(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats);
static void
SendRowDescriptionCols_3(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats);

/* ----------------------------------------------------------------
 *		printtup / debugtup support
 * ----------------------------------------------------------------
 */

/* ----------------
 *		Private state for a printtup destination object
 *
 * NOTE: finfo is the lookup info for either typoutput or typsend, whichever
 * we are using for this column.
 * ----------------
 */
typedef struct
{                    /* Per-attribute information */
  Oid typoutput;     /* Oid for the type's text output fn */
  Oid typsend;       /* Oid for the type's binary output fn */
  bool typisvarlena; /* is it varlena (ie possibly toastable)? */
  int16 format;      /* format code for this column */
  FmgrInfo finfo;    /* Precomputed call info for output fn */
} PrinttupAttrInfo;

typedef struct
{
  DestReceiver pub;   /* publicly-known function pointers */
  Portal portal;      /* the Portal we are printing from */
  bool sendDescrip;   /* send RowDescription at startup? */
  TupleDesc attrinfo; /* The attr info we are set up for */
  int nattrs;
  PrinttupAttrInfo *myinfo; /* Cached info about each attr */
  StringInfoData buf;       /* output buffer (*not* in tmpcontext) */
  MemoryContext tmpcontext; /* Memory context for per-row workspace */
} DR_printtup;

/* ----------------
 *		Initialize: create a DestReceiver for printtup
 * ----------------
 */
DestReceiver *
printtup_create_DR(CommandDest dest)
{





















}

/*
 * Set parameters for a DestRemote (or DestRemoteExecute) receiver
 */
void
SetRemoteDestReceiverParams(DestReceiver *self, Portal portal)
{






















}

static void
printtup_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{





















































}

/*
 * SendRowDescriptionMessage --- send a RowDescription message to the frontend
 *
 * Notes: the TupleDesc has typically been manufactured by ExecTypeFromTL()
 * or some similar function; it does not contain a full set of fields.
 * The targetlist will be NIL when executing a utility function that does
 * not have a plan.  If the targetlist isn't NIL then it is a Query node's
 * targetlist; it is up to us to ignore resjunk columns in it.  The formats[]
 * array pointer might be NULL (if we are doing Describe on a prepared stmt);
 * send zeroes for the format codes in that case.
 */
void
SendRowDescriptionMessage(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats)
{


















}

/*
 * Send description for each column when using v3+ protocol
 */
static void
SendRowDescriptionCols_3(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats)
{










































































}

/*
 * Send description for each column when using v2 protocol
 */
static void
SendRowDescriptionCols_2(StringInfo buf, TupleDesc typeinfo, List *targetlist, int16 *formats)
{



















}

/*
 * Get the lookup info that printtup() needs
 */
static void
printtup_prepare_info(DR_printtup *myState, TupleDesc typeinfo, int numAttrs)
{









































}

/* ----------------
 *		printtup --- print a tuple in protocol 3.0
 * ----------------
 */
static bool
printtup(TupleTableSlot *slot, DestReceiver *self)
{














































































}

/* ----------------
 *		printtup_20 --- print a tuple in protocol 2.0
 * ----------------
 */
static bool
printtup_20(TupleTableSlot *slot, DestReceiver *self)
{











































































}

/* ----------------
 *		printtup_shutdown
 * ----------------
 */
static void
printtup_shutdown(DestReceiver *self)
{





















}

/* ----------------
 *		printtup_destroy
 * ----------------
 */
static void
printtup_destroy(DestReceiver *self)
{

}

/* ----------------
 *		printatt
 * ----------------
 */
static void
printatt(unsigned attributeId, Form_pg_attribute attributeP, char *value)
{

}

/* ----------------
 *		debugStartup - prepare to print tuples for an interactive
 *backend
 * ----------------
 */
void
debugStartup(DestReceiver *self, int operation, TupleDesc typeinfo)
{











}

/* ----------------
 *		debugtup - print one tuple for an interactive backend
 * ----------------
 */
bool
debugtup(TupleTableSlot *slot, DestReceiver *self)
{

























}

/* ----------------
 *		printtup_internal_20 --- print a binary tuple in protocol 2.0
 *
 * We use a different message type, i.e. 'B' instead of 'D' to
 * indicate a tuple in internal (binary) form.
 *
 * This is largely same as printtup_20, except we use binary formatting.
 * ----------------
 */
static bool
printtup_internal_20(TupleTableSlot *slot, DestReceiver *self)
{












































































}