/*-------------------------------------------------------------------------
 *
 * inval.c
 *	  POSTGRES cache invalidation dispatcher code.
 *
 *	This is subtle stuff, so pay attention:
 *
 *	When a tuple is updated or deleted, our standard visibility rules
 *	consider that it is *still valid* so long as we are in the same command,
 *	ie, until the next CommandCounterIncrement() or transaction commit.
 *	(See access/heap/heapam_visibility.c, and note that system catalogs are
 *  generally scanned under the most current snapshot available, rather than
 *  the transaction snapshot.)	At the command boundary, the old tuple stops
 *	being valid and the new version, if any, becomes valid.  Therefore,
 *	we cannot simply flush a tuple from the system caches during
 *heap_update() or heap_delete().  The tuple is still good at that point; what's
 *more, even if we did flush it, it might be reloaded into the caches by a later
 *	request in the same command.  So the correct behavior is to keep a list
 *	of outdated (updated/deleted) tuples and then do the required cache
 *	flushes at the next command boundary.  We must also keep track of
 *	inserted tuples so that we can flush "negative" cache entries that match
 *	the new tuples; again, that mustn't happen until end of command.
 *
 *	Once we have finished the command, we still need to remember inserted
 *	tuples (including new versions of updated tuples), so that we can flush
 *	them from the caches if we abort the transaction.  Similarly, we'd
 *better be able to flush "negative" cache entries that may have been loaded in
 *	place of deleted tuples, so we still need the deleted ones too.
 *
 *	If we successfully complete the transaction, we have to broadcast all
 *	these invalidation events to other backends (via the SI message queue)
 *	so that they can flush obsolete entries from their caches.  Note we have
 *	to record the transaction commit before sending SI messages, otherwise
 *	the other backends won't see our updated tuples as good.
 *
 *	When a subtransaction aborts, we can process and discard any events
 *	it has queued.  When a subtransaction commits, we just add its events
 *	to the pending lists of the parent transaction.
 *
 *	In short, we need to remember until xact end every insert or delete
 *	of a tuple that might be in the system caches.  Updates are treated as
 *	two events, delete + insert, for simplicity.  (If the update doesn't
 *	change the tuple hash value, catcache.c optimizes this into one event.)
 *
 *	We do not need to register EVERY tuple operation in this way, just those
 *	on tuples in relations that have associated catcaches.  We do, however,
 *	have to register every operation on every tuple that *could* be in a
 *	catcache, whether or not it currently is in our cache.  Also, if the
 *	tuple is in a relation that has multiple catcaches, we need to register
 *	an invalidation message for each such catcache.  catcache.c's
 *	PrepareToInvalidateCacheTuple() routine provides the knowledge of which
 *	catcaches may need invalidation for a given tuple.
 *
 *	Also, whenever we see an operation on a pg_class, pg_attribute, or
 *	pg_index tuple, we register a relcache flush operation for the relation
 *	described by that tuple (as specified in CacheInvalidateHeapTuple()).
 *	Likewise for pg_constraint tuples for foreign keys on relations.
 *
 *	We keep the relcache flush requests in lists separate from the catcache
 *	tuple flush requests.  This allows us to issue all the pending catcache
 *	flushes before we issue relcache flushes, which saves us from loading
 *	a catcache tuple during relcache load only to flush it again right away.
 *	Also, we avoid queuing multiple relcache flush requests for the same
 *	relation, since a relcache flush is relatively expensive to do.
 *	(XXX is it worth testing likewise for duplicate catcache flush entries?
 *	Probably not.)
 *
 *	Many subsystems own higher-level caches that depend on relcache and/or
 *	catcache, and they register callbacks here to invalidate their caches.
 *	While building a higher-level cache entry, a backend may receive a
 *	callback for the being-built entry or one of its dependencies.  This
 *	implies the new higher-level entry would be born stale, and it might
 *	remain stale for the life of the backend.  Many caches do not prevent
 *	that.  They rely on DDL for can't-miss catalog changes taking
 *	AccessExclusiveLock on suitable objects.  (For a change made with less
 *	locking, backends might never read the change.)  The relation cache,
 *	however, needs to reflect changes from CREATE INDEX CONCURRENTLY no
 *later than the beginning of the next transaction.  Hence, when a relevant
 *	invalidation callback arrives during a build, relcache.c reattempts that
 *	build.  Caches with similar needs could do likewise.
 *
 *	If a relcache flush is issued for a system relation that we preload
 *	from the relcache init file, we must also delete the init file so that
 *	it will be rebuilt during the next backend restart.  The actual work of
 *	manipulating the init file is in relcache.c, but we keep track of the
 *	need for it here.
 *
 *	The request lists proper are kept in CurTransactionContext of their
 *	creating (sub)transaction, since they can be forgotten on abort of that
 *	transaction but must be kept till top-level commit otherwise.  For
 *	simplicity we keep the controlling list-of-lists in
 *TopTransactionContext.
 *
 *	Currently, inval messages are sent without regard for the possibility
 *	that the object described by the catalog tuple might be a session-local
 *	object such as a temporary table.  This is because (1) this code has
 *	no practical way to tell the difference, and (2) it is not certain that
 *	other backends don't have catalog cache or even relcache entries for
 *	such tables, anyway; there is nothing that prevents that.  It might be
 *	worth trying to avoid sending such inval traffic in the future, if those
 *	problems can be overcome cheaply.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/cache/inval.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/pg_constraint.h"
#include "miscadmin.h"
#include "storage/sinval.h"
#include "storage/smgr.h"
#include "utils/catcache.h"
#include "utils/inval.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/*
 * To minimize palloc traffic, we keep pending requests in successively-
 * larger chunks (a slightly more sophisticated version of an expansible
 * array).  All request types can be stored as SharedInvalidationMessage
 * records.  The ordering of requests within a list is never significant.
 */
typedef struct InvalidationChunk
{
  struct InvalidationChunk *next; /* list link */
  int nitems;                     /* # items currently stored in chunk */
  int maxitems;                   /* size of allocated array in this chunk */
  SharedInvalidationMessage msgs[FLEXIBLE_ARRAY_MEMBER];
} InvalidationChunk;

typedef struct InvalidationListHeader
{
  InvalidationChunk *cclist; /* list of chunks holding catcache msgs */
  InvalidationChunk *rclist; /* list of chunks holding relcache msgs */
} InvalidationListHeader;

/*----------------
 * Invalidation info is divided into two lists:
 *	1) events so far in current command, not yet reflected to caches.
 *	2) events in previous commands of current transaction; these have
 *	   been reflected to local caches, and must be either broadcast to
 *	   other backends or rolled back from local cache when we commit
 *	   or abort the transaction.
 * Actually, we need two such lists for each level of nested transaction,
 * so that we can discard events from an aborted subtransaction.  When
 * a subtransaction commits, we append its lists to the parent's lists.
 *
 * The relcache-file-invalidated flag can just be a simple boolean,
 * since we only act on it at transaction commit; we don't care which
 * command of the transaction set it.
 *----------------
 */

typedef struct TransInvalidationInfo
{
  /* Back link to parent transaction's info */
  struct TransInvalidationInfo *parent;

  /* Subtransaction nesting depth */
  int my_level;

  /* head of current-command event list */
  InvalidationListHeader CurrentCmdInvalidMsgs;

  /* head of previous-commands event list */
  InvalidationListHeader PriorCmdInvalidMsgs;

  /* init file must be invalidated? */
  bool RelcacheInitFileInval;
} TransInvalidationInfo;

static TransInvalidationInfo *transInvalInfo = NULL;

static SharedInvalidationMessage *SharedInvalidMessagesArray;
static int numSharedInvalidMessagesArray;
static int maxSharedInvalidMessagesArray;

/*
 * Dynamically-registered callback functions.  Current implementation
 * assumes there won't be enough of these to justify a dynamically resizable
 * array; it'd be easy to improve that if needed.
 *
 * To avoid searching in CallSyscacheCallbacks, all callbacks for a given
 * syscache are linked into a list pointed to by syscache_callback_links[id].
 * The link values are syscache_callback_list[] index plus 1, or 0 for none.
 */

#define MAX_SYSCACHE_CALLBACKS 64
#define MAX_RELCACHE_CALLBACKS 10

static struct SYSCACHECALLBACK
{
  int16 id;   /* cache number */
  int16 link; /* next callback index+1 for same cache */
  SyscacheCallbackFunction function;
  Datum arg;
} syscache_callback_list[MAX_SYSCACHE_CALLBACKS];

static int16 syscache_callback_links[SysCacheSize];

static int syscache_callback_count = 0;

static struct RELCACHECALLBACK
{
  RelcacheCallbackFunction function;
  Datum arg;
} relcache_callback_list[MAX_RELCACHE_CALLBACKS];

static int relcache_callback_count = 0;

/* ----------------------------------------------------------------
 *				Invalidation list support functions
 *
 * These three routines encapsulate processing of the "chunked"
 * representation of what is logically just a list of messages.
 * ----------------------------------------------------------------
 */

/*
 * AddInvalidationMessage
 *		Add an invalidation message to a list (of chunks).
 *
 * Note that we do not pay any great attention to maintaining the original
 * ordering of the messages.
 */
static void
AddInvalidationMessage(InvalidationChunk **listHdr, SharedInvalidationMessage *msg)
{


























}

/*
 * Append one list of invalidation message chunks to another, resetting
 * the source chunk-list pointer to NULL.
 */
static void
AppendInvalidationMessageList(InvalidationChunk **destHdr, InvalidationChunk **srcHdr)
{

















}

/*
 * Process a list of invalidation messages.
 *
 * This is a macro that executes the given code fragment for each message in
 * a message chunk list.  The fragment should refer to the message as *msg.
 */
#define ProcessMessageList(listHdr, codeFragment)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    InvalidationChunk *_chunk;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    for (_chunk = (listHdr); _chunk != NULL; _chunk = _chunk->next)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      int _cindex;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
      for (_cindex = 0; _cindex < _chunk->nitems; _cindex++)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        SharedInvalidationMessage *msg = &_chunk->msgs[_cindex];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
        codeFragment;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/*
 * Process a list of invalidation messages group-wise.
 *
 * As above, but the code fragment can handle an array of messages.
 * The fragment should refer to the messages as msgs[], with n entries.
 */
#define ProcessMessageListMulti(listHdr, codeFragment)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    InvalidationChunk *_chunk;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    for (_chunk = (listHdr); _chunk != NULL; _chunk = _chunk->next)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      SharedInvalidationMessage *msgs = _chunk->msgs;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      int n = _chunk->nitems;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      codeFragment;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

/* ----------------------------------------------------------------
 *				Invalidation set support functions
 *
 * These routines understand about the division of a logical invalidation
 * list into separate physical lists for catcache and relcache entries.
 * ----------------------------------------------------------------
 */

/*
 * Add a catcache inval entry
 */
static void
AddCatcacheInvalidationMessage(InvalidationListHeader *hdr, int id, uint32 hashValue, Oid dbId)
{



















}

/*
 * Add a whole-catalog inval entry
 */
static void
AddCatalogInvalidationMessage(InvalidationListHeader *hdr, Oid dbId, Oid catId)
{









}

/*
 * Add a relcache inval entry
 */
static void
AddRelcacheInvalidationMessage(InvalidationListHeader *hdr, Oid dbId, Oid relId)
{

















}

/*
 * Add a snapshot inval entry
 */
static void
AddSnapshotInvalidationMessage(InvalidationListHeader *hdr, Oid dbId, Oid relId)
{














}

/*
 * Append one list of invalidation messages to another, resetting
 * the source list to empty.
 */
static void
AppendInvalidationMessages(InvalidationListHeader *dest, InvalidationListHeader *src)
{


}

/*
 * Execute the given function for all the messages in an invalidation list.
 * The list is not altered.
 *
 * catcache entries are processed first, for reasons mentioned above.
 */
static void
ProcessInvalidationMessages(InvalidationListHeader *hdr, void (*func)(SharedInvalidationMessage *msg))
{


}

/*
 * As above, but the function is able to process an array of messages
 * rather than just one at a time.
 */
static void
ProcessInvalidationMessagesMulti(InvalidationListHeader *hdr, void (*func)(const SharedInvalidationMessage *msgs, int n))
{


}

/* ----------------------------------------------------------------
 *					  private support functions
 * ----------------------------------------------------------------
 */

/*
 * RegisterCatcacheInvalidation
 *
 * Register an invalidation event for a catcache tuple entry.
 */
static void
RegisterCatcacheInvalidation(int cacheId, uint32 hashValue, Oid dbId)
{

}

/*
 * RegisterCatalogInvalidation
 *
 * Register an invalidation event for all catcache entries from a catalog.
 */
static void
RegisterCatalogInvalidation(Oid dbId, Oid catId)
{

}

/*
 * RegisterRelcacheInvalidation
 *
 * As above, but register a relcache invalidation event.
 */
static void
RegisterRelcacheInvalidation(Oid dbId, Oid relId)
{




















}

/*
 * RegisterSnapshotInvalidation
 *
 * Register an invalidation event for MVCC scans against a given catalog.
 * Only needed for catalogs that don't have catcaches.
 */
static void
RegisterSnapshotInvalidation(Oid dbId, Oid relId)
{

}

/*
 * LocalExecuteInvalidationMessage
 *
 * Process a single invalidation message (which could be of any type).
 * Only the local caches are flushed; this does not transmit the message
 * to other backends.
 */
void
LocalExecuteInvalidationMessage(SharedInvalidationMessage *msg)
{





















































































}

/*
 *		InvalidateSystemCaches
 *
 *		This blows away all tuples in the system catalog caches and
 *		all the cached relation descriptors and smgr cache entries.
 *		Relation descriptors that have positive refcounts are then
 *rebuilt.
 *
 *		We call this when we see a shared-inval-queue overflow signal,
 *		since that tells us we've lost some shared-inval messages and
 *hence don't know what needs to be invalidated.
 */
void
InvalidateSystemCaches(void)
{

}

void
InvalidateSystemCachesExtended(bool debug_discard)
{



















}

/* ----------------------------------------------------------------
 *					  public functions
 * ----------------------------------------------------------------
 */

/*
 * AcceptInvalidationMessages
 *		Read and process invalidation messages from the shared
 *invalidation message queue.
 *
 * Note:
 *		This should be called as the first step in processing a
 *transaction.
 */
void
AcceptInvalidationMessages(void)
{









































}

/*
 * PrepareInvalidationState
 *		Initialize inval lists for the current (sub)transaction.
 */
static void
PrepareInvalidationState(void)
{


















}

/*
 * PostPrepare_Inval
 *		Clean up after successful PREPARE.
 *
 * Here, we want to act as though the transaction aborted, so that we will
 * undo any syscache changes it made, thereby bringing us into sync with the
 * outside world, which doesn't believe the transaction committed yet.
 *
 * If the prepared transaction is later aborted, there is nothing more to
 * do; if it commits, we will receive the consequent inval messages just
 * like everyone else.
 */
void
PostPrepare_Inval(void)
{

}

/*
 * Collect invalidation messages into SharedInvalidMessagesArray array.
 */
static void
MakeSharedInvalidMessagesArray(const SharedInvalidationMessage *msgs, int n)
{






























}

/*
 * xactGetCommittedInvalidationMessages() is executed by
 * RecordTransactionCommit() to add invalidation messages onto the
 * commit record. This applies only to commit message types, never to
 * abort records. Must always run before AtEOXact_Inval(), since that
 * removes the data we need to see.
 *
 * Remember that this runs before we have officially committed, so we
 * must not do anything here to change what might occur *if* we should
 * fail between here and the actual commit.
 *
 * see also xact_redo_commit() and xact_desc_commit()
 */
int
xactGetCommittedInvalidationMessages(SharedInvalidationMessage **msgs, bool *RelcacheInitFileInval)
{







































}

/*
 * ProcessCommittedInvalidationMessages is executed by xact_redo_commit() or
 * standby_redo() to process invalidation messages. Currently that happens
 * only at end-of-xact.
 *
 * Relcache init file invalidation requires processing both
 * before and after we send the SI messages. See AtEOXact_Inval()
 */
void
ProcessCommittedInvalidationMessages(SharedInvalidationMessage *msgs, int nmsgs, bool RelcacheInitFileInval, Oid dbid, Oid tsid)
{






































}

/*
 * AtEOXact_Inval
 *		Process queued-up invalidation messages at end of main
 *transaction.
 *
 * If isCommit, we must send out the messages in our PriorCmdInvalidMsgs list
 * to the shared invalidation message queue.  Note that these will be read
 * not only by other backends, but also by our own backend at the next
 * transaction start (via AcceptInvalidationMessages).  This means that
 * we can skip immediate local processing of anything that's still in
 * CurrentCmdInvalidMsgs, and just send that list out too.
 *
 * If not isCommit, we are aborting, and must locally process the messages
 * in PriorCmdInvalidMsgs.  No messages need be sent to other backends,
 * since they'll not have seen our changed tuples anyway.  We can forget
 * about CurrentCmdInvalidMsgs too, since those changes haven't touched
 * the caches yet.
 *
 * In any case, reset the various lists to empty.  We need not physically
 * free memory here, since TopTransactionContext is about to be emptied
 * anyway.
 *
 * Note:
 *		This should be called as the last step in processing a
 *transaction.
 */
void
AtEOXact_Inval(bool isCommit)
{







































}

/*
 * AtEOSubXact_Inval
 *		Process queued-up invalidation messages at end of
 *subtransaction.
 *
 * If isCommit, process CurrentCmdInvalidMsgs if any (there probably aren't),
 * and then attach both CurrentCmdInvalidMsgs and PriorCmdInvalidMsgs to the
 * parent's PriorCmdInvalidMsgs list.
 *
 * If not isCommit, we are aborting, and must locally process the messages
 * in PriorCmdInvalidMsgs.  No messages need be sent to other backends.
 * We can forget about CurrentCmdInvalidMsgs too, since those changes haven't
 * touched the caches yet.
 *
 * In any case, pop the transaction stack.  We need not physically free memory
 * here, since CurTransactionContext is about to be emptied anyway
 * (if aborting).  Beware of the possibility of aborting the same nesting
 * level twice, though.
 */
void
AtEOSubXact_Inval(bool isCommit)
{



























































}

/*
 * CommandEndInvalidationMessages
 *		Process queued-up invalidation messages at end of one command
 *		in a transaction.
 *
 * Here, we send no messages to the shared queue, since we don't know yet if
 * we will commit.  We do need to locally process the CurrentCmdInvalidMsgs
 * list, so as to flush our caches of any entries we have outdated in the
 * current command.  We then move the current-cmd list over to become part
 * of the prior-cmds list.
 *
 * Note:
 *		This should be called during CommandCounterIncrement(),
 *		after we have advanced the command ID.
 */
void
CommandEndInvalidationMessages(void)
{












}

/*
 * CacheInvalidateHeapTuple
 *		Register the given tuple for invalidation at end of command
 *		(ie, current command is creating or outdating this tuple).
 *		Also, detect whether a relcache invalidation is implied.
 *
 * For an insert or delete, tuple is the target tuple and newtuple is NULL.
 * For an update, we are called just once, with tuple being the old tuple
 * version and newtuple the new version.  This allows avoidance of duplicate
 * effort during an update.
 */
void
CacheInvalidateHeapTuple(Relation relation, HeapTuple tuple, HeapTuple newtuple)
{
































































































































}

/*
 * CacheInvalidateCatalog
 *		Register invalidation of the whole content of a system catalog.
 *
 * This is normally used in VACUUM FULL/CLUSTER, where we haven't so much
 * changed any tuples as moved them around.  Some uses of catcache entries
 * expect their TIDs to be correct, so we have to blow away the entries.
 *
 * Note: we expect caller to verify that the rel actually is a system
 * catalog.  If it isn't, no great harm is done, just a wasted sinval message.
 */
void
CacheInvalidateCatalog(Oid catalogId)
{














}

/*
 * CacheInvalidateRelcache
 *		Register invalidation of the specified relation's relcache entry
 *		at end of command.
 *
 * This is used in places that need to force relcache rebuild but aren't
 * changing any of the tuples recognized as contributors to the relcache
 * entry by CacheInvalidateHeapTuple.  (An example is dropping an index.)
 */
void
CacheInvalidateRelcache(Relation relation)
{
















}

/*
 * CacheInvalidateRelcacheAll
 *		Register invalidation of the whole relcache at the end of
 *command.
 *
 * This is used by alter publication as changes in publications may affect
 * large number of tables.
 */
void
CacheInvalidateRelcacheAll(void)
{



}

/*
 * CacheInvalidateRelcacheByTuple
 *		As above, but relation is identified by passing its pg_class
 *tuple.
 */
void
CacheInvalidateRelcacheByTuple(HeapTuple classTuple)
{
















}

/*
 * CacheInvalidateRelcacheByRelid
 *		As above, but relation is identified by passing its OID.
 *		This is the least efficient of the three options; use one of
 *		the above routines if you have a Relation or pg_class tuple.
 */
void
CacheInvalidateRelcacheByRelid(Oid relid)
{











}

/*
 * CacheInvalidateSmgr
 *		Register invalidation of smgr references to a physical relation.
 *
 * Sending this type of invalidation msg forces other backends to close open
 * smgr entries for the rel.  This should be done to flush dangling open-file
 * references when the physical rel is being dropped or truncated.  Because
 * these are nontransactional (i.e., not-rollback-able) operations, we just
 * send the inval message immediately without any queuing.
 *
 * Note: in most cases there will have been a relcache flush issued against
 * the rel at the logical level.  We need a separate smgr-level flush because
 * it is possible for backends to have open smgr entries for rels they don't
 * have a relcache entry for, e.g. because the only thing they ever did with
 * the rel is write out dirty shared buffers.
 *
 * Note: because these messages are nontransactional, they won't be captured
 * in commit/abort WAL entries.  Instead, calls to CacheInvalidateSmgr()
 * should happen in low-level smgr.c routines, which are executed while
 * replaying WAL as well as when creating it.
 *
 * Note: In order to avoid bloating SharedInvalidationMessage, we store only
 * three bytes of the backend ID using what would otherwise be padding space.
 * Thus, the maximum possible backend ID is 2^23-1.
 */
void
CacheInvalidateSmgr(RelFileNodeBackend rnode)
{










}

/*
 * CacheInvalidateRelmap
 *		Register invalidation of the relation mapping for a database,
 *		or for the shared catalogs if databaseId is zero.
 *
 * Sending this type of invalidation msg forces other backends to re-read
 * the indicated relation mapping file.  It is also necessary to send a
 * relcache inval for the specific relations whose mapping has been altered,
 * else the relcache won't get updated with the new filenode data.
 *
 * Note: because these messages are nontransactional, they won't be captured
 * in commit/abort WAL entries.  Instead, calls to CacheInvalidateRelmap()
 * should happen in low-level relmapper.c routines, which are executed while
 * replaying WAL as well as when creating it.
 */
void
CacheInvalidateRelmap(Oid databaseId)
{








}

/*
 * CacheRegisterSyscacheCallback
 *		Register the specified function to be called for all future
 *		invalidation events in the specified cache.  The cache ID and
 *the hash value of the tuple being invalidated will be passed to the function.
 *
 * NOTE: Hash value zero will be passed if a cache reset request is received.
 * In this case the called routines should flush all cached state.
 * Yes, there's a possibility of a false match to zero, but it doesn't seem
 * worth troubling over, especially since most of the current callees just
 * flush all cached state anyway.
 */
void
CacheRegisterSyscacheCallback(int cacheid, SyscacheCallbackFunction func, Datum arg)
{
































}

/*
 * CacheRegisterRelcacheCallback
 *		Register the specified function to be called for all future
 *		relcache invalidation events.  The OID of the relation being
 *		invalidated will be passed to the function.
 *
 * NOTE: InvalidOid will be passed if a cache reset request is received.
 * In this case the called routines should flush all cached state.
 */
void
CacheRegisterRelcacheCallback(RelcacheCallbackFunction func, Datum arg)
{









}

/*
 * CallSyscacheCallbacks
 *
 * This is exported so that CatalogCacheFlushCatalog can call it, saving
 * this module from knowing which catcache IDs correspond to which catalogs.
 */
void
CallSyscacheCallbacks(int cacheid, uint32 hashvalue)
{
















}