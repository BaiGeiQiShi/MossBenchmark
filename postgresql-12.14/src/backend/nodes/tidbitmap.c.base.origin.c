/*-------------------------------------------------------------------------
 *
 * tidbitmap.c
 *	  PostgreSQL tuple-id (TID) bitmap package
 *
 * This module provides bitmap data structures that are spiritually
 * similar to Bitmapsets, but are specially adapted to store sets of
 * tuple identifiers (TIDs), or ItemPointers.  In particular, the division
 * of an ItemPointer into BlockNumber and OffsetNumber is catered for.
 * Also, since we wish to be able to store very large tuple sets in
 * memory with this data structure, we support "lossy" storage, in which
 * we no longer remember individual tuple offsets on a page but only the
 * fact that a particular page needs to be visited.
 *
 * The "lossy" storage uses one bit per disk page, so at the standard 8K
 * BLCKSZ, we can represent all pages in 64Gb of disk space in about 1Mb
 * of memory.  People pushing around tables of that size should have a
 * couple of Mb to spare, so we don't worry about providing a second level
 * of lossiness.  In theory we could fall back to page ranges at some
 * point, but for now that seems useless complexity.
 *
 * We also support the notion of candidate matches, or rechecking.  This
 * means we know that a search need visit only some tuples on a page,
 * but we are not certain that all of those tuples are real matches.
 * So the eventual heap scan must recheck the quals for these tuples only,
 * rather than rechecking the quals for all tuples on the page as in the
 * lossy-bitmap case.  Rechecking can be specified when TIDs are inserted
 * into a bitmap, and it can also happen internally when we AND a lossy
 * and a non-lossy page.
 *
 *
 * Copyright (c) 2003-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/nodes/tidbitmap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "nodes/bitmapset.h"
#include "nodes/tidbitmap.h"
#include "storage/lwlock.h"
#include "utils/dsa.h"
#include "utils/hashutils.h"

/*
 * The maximum number of tuples per page is not large (typically 256 with
 * 8K pages, or 1024 with 32K pages).  So there's not much point in making
 * the per-page bitmaps variable size.  We just legislate that the size
 * is this:
 */
#define MAX_TUPLES_PER_PAGE MaxHeapTuplesPerPage

/*
 * When we have to switch over to lossy storage, we use a data structure
 * with one bit per page, where all pages having the same number DIV
 * PAGES_PER_CHUNK are aggregated into one chunk.  When a chunk is present
 * and has the bit set for a given page, there must not be a per-page entry
 * for that page in the page table.
 *
 * We actually store both exact pages and lossy chunks in the same hash
 * table, using identical data structures.  (This is because the memory
 * management for hashtables doesn't easily/efficiently allow space to be
 * transferred easily from one hashtable to another.)  Therefore it's best
 * if PAGES_PER_CHUNK is the same as MAX_TUPLES_PER_PAGE, or at least not
 * too different.  But we also want PAGES_PER_CHUNK to be a power of 2 to
 * avoid expensive integer remainder operations.  So, define it like this:
 */
#define PAGES_PER_CHUNK (BLCKSZ / 32)

/* We use BITS_PER_BITMAPWORD and typedef bitmapword from nodes/bitmapset.h */

#define WORDNUM(x) ((x) / BITS_PER_BITMAPWORD)
#define BITNUM(x) ((x) % BITS_PER_BITMAPWORD)

/* number of active words for an exact page: */
#define WORDS_PER_PAGE ((MAX_TUPLES_PER_PAGE - 1) / BITS_PER_BITMAPWORD + 1)
/* number of active words for a lossy chunk: */
#define WORDS_PER_CHUNK ((PAGES_PER_CHUNK - 1) / BITS_PER_BITMAPWORD + 1)

/*
 * The hashtable entries are represented by this data structure.  For
 * an exact page, blockno is the page number and bit k of the bitmap
 * represents tuple offset k+1.  For a lossy chunk, blockno is the first
 * page in the chunk (this must be a multiple of PAGES_PER_CHUNK) and
 * bit k represents page blockno+k.  Note that it is not possible to
 * have exact storage for the first page of a chunk if we are using
 * lossy storage for any page in the chunk's range, since the same
 * hashtable entry has to serve both purposes.
 *
 * recheck is used only on exact pages --- it indicates that although
 * only the stated tuples need be checked, the full index qual condition
 * must be checked for each (ie, these are candidate matches).
 */
typedef struct PagetableEntry
{
  BlockNumber blockno; /* page number (hashtable key) */
  char status;         /* hash entry status */
  bool ischunk;        /* T = lossy storage, F = exact */
  bool recheck;        /* should the tuples be rechecked? */
  bitmapword words[Max(WORDS_PER_PAGE, WORDS_PER_CHUNK)];
} PagetableEntry;

/*
 * Holds array of pagetable entries.
 */
typedef struct PTEntryArray
{
  pg_atomic_uint32 refcount; /* no. of iterator attached */
  PagetableEntry ptentry[FLEXIBLE_ARRAY_MEMBER];
} PTEntryArray;

/*
 * We want to avoid the overhead of creating the hashtable, which is
 * comparatively large, when not necessary. Particularly when we are using a
 * bitmap scan on the inside of a nestloop join: a bitmap may well live only
 * long enough to accumulate one entry in such cases.  We therefore avoid
 * creating an actual hashtable until we need two pagetable entries.  When
 * just one pagetable entry is needed, we store it in a fixed field of
 * TIDBitMap.  (NOTE: we don't get rid of the hashtable if the bitmap later
 * shrinks down to zero or one page again.  So, status can be TBM_HASH even
 * when nentries is zero or one.)
 */
typedef enum
{
  TBM_EMPTY,    /* no hashtable, nentries == 0 */
  TBM_ONE_PAGE, /* entry1 contains the single entry */
  TBM_HASH      /* pagetable is valid, entry1 is not */
} TBMStatus;

/*
 * Current iterating state of the TBM.
 */
typedef enum
{
  TBM_NOT_ITERATING,     /* not yet converted to page and chunk array */
  TBM_ITERATING_PRIVATE, /* converted to local page and chunk array */
  TBM_ITERATING_SHARED   /* converted to shared page and chunk array */
} TBMIteratingState;

/*
 * Here is the representation for a whole TIDBitMap:
 */
struct TIDBitmap
{
  NodeTag type;                     /* to make it a valid Node */
  MemoryContext mcxt;               /* memory context containing me */
  TBMStatus status;                 /* see codes above */
  struct pagetable_hash *pagetable; /* hash table of PagetableEntry's */
  int nentries;                     /* number of entries in pagetable */
  int maxentries;                   /* limit on same to meet maxbytes */
  int npages;                       /* number of exact entries in pagetable */
  int nchunks;                      /* number of lossy entries in pagetable */
  TBMIteratingState iterating;      /* tbm_begin_iterate called? */
  uint32 lossify_start;             /* offset to start lossifying hashtable at */
  PagetableEntry entry1;            /* used when status == TBM_ONE_PAGE */
  /* these are valid when iterating is true: */
  PagetableEntry **spages;     /* sorted exact-page list, or NULL */
  PagetableEntry **schunks;    /* sorted lossy-chunk list, or NULL */
  dsa_pointer dsapagetable;    /* dsa_pointer to the element array */
  dsa_pointer dsapagetableold; /* dsa_pointer to the old element array */
  dsa_pointer ptpages;         /* dsa_pointer to the page array */
  dsa_pointer ptchunks;        /* dsa_pointer to the chunk array */
  dsa_area *dsa;               /* reference to per-query dsa area */
};

/*
 * When iterating over a bitmap in sorted order, a TBMIterator is used to
 * track our progress.  There can be several iterators scanning the same
 * bitmap concurrently.  Note that the bitmap becomes read-only as soon as
 * any iterator is created.
 */
struct TBMIterator
{
  TIDBitmap *tbm;          /* TIDBitmap we're iterating over */
  int spageptr;            /* next spages index */
  int schunkptr;           /* next schunks index */
  int schunkbit;           /* next bit to check in current schunk */
  TBMIterateResult output; /* MUST BE LAST (because variable-size) */
};

/*
 * Holds the shared members of the iterator so that multiple processes
 * can jointly iterate.
 */
typedef struct TBMSharedIteratorState
{
  int nentries;          /* number of entries in pagetable */
  int maxentries;        /* limit on same to meet maxbytes */
  int npages;            /* number of exact entries in pagetable */
  int nchunks;           /* number of lossy entries in pagetable */
  dsa_pointer pagetable; /* dsa pointers to head of pagetable data */
  dsa_pointer spages;    /* dsa pointer to page array */
  dsa_pointer schunks;   /* dsa pointer to chunk array */
  LWLock lock;           /* lock to protect below members */
  int spageptr;          /* next spages index */
  int schunkptr;         /* next schunks index */
  int schunkbit;         /* next bit to check in current schunk */
} TBMSharedIteratorState;

/*
 * pagetable iteration array.
 */
typedef struct PTIterationArray
{
  pg_atomic_uint32 refcount;        /* no. of iterator attached */
  int index[FLEXIBLE_ARRAY_MEMBER]; /* index array */
} PTIterationArray;

/*
 * same as TBMIterator, but it is used for joint iteration, therefore this
 * also holds a reference to the shared state.
 */
struct TBMSharedIterator
{
  TBMSharedIteratorState *state; /* shared state */
  PTEntryArray *ptbase;          /* pagetable element array */
  PTIterationArray *ptpages;     /* sorted exact page index list */
  PTIterationArray *ptchunks;    /* sorted lossy page index list */
  TBMIterateResult output;       /* MUST BE LAST (because variable-size) */
};

/* Local function prototypes */
static void
tbm_union_page(TIDBitmap *a, const PagetableEntry *bpage);
static bool
tbm_intersect_page(TIDBitmap *a, PagetableEntry *apage, const TIDBitmap *b);
static const PagetableEntry *
tbm_find_pageentry(const TIDBitmap *tbm, BlockNumber pageno);
static PagetableEntry *
tbm_get_pageentry(TIDBitmap *tbm, BlockNumber pageno);
static bool
tbm_page_is_lossy(const TIDBitmap *tbm, BlockNumber pageno);
static void
tbm_mark_page_lossy(TIDBitmap *tbm, BlockNumber pageno);
static void
tbm_lossify(TIDBitmap *tbm);
static int
tbm_comparator(const void *left, const void *right);
static int
tbm_shared_comparator(const void *left, const void *right, void *arg);

/* define hashtable mapping block numbers to PagetableEntry's */
#define SH_USE_NONDEFAULT_ALLOCATOR
#define SH_PREFIX pagetable
#define SH_ELEMENT_TYPE PagetableEntry
#define SH_KEY_TYPE BlockNumber
#define SH_KEY blockno
#define SH_HASH_KEY(tb, key) murmurhash32(key)
#define SH_EQUAL(tb, a, b) a == b
#define SH_SCOPE static inline
#define SH_DEFINE
#define SH_DECLARE
#include "lib/simplehash.h"

/*
 * tbm_create - create an initially-empty bitmap
 *
 * The bitmap will live in the memory context that is CurrentMemoryContext
 * at the time of this call.  It will be limited to (approximately) maxbytes
 * total memory consumption.  If the DSA passed to this function is not NULL
 * then the memory for storing elements of the underlying page table will
 * be allocated from the DSA.
 */
TIDBitmap *
tbm_create(long maxbytes, dsa_area *dsa)
{

















}

/*
 * Actually create the hashtable.  Since this is a moderately expensive
 * proposition, we don't do it until we have to.
 */
static void
tbm_create_pagetable(TIDBitmap *tbm)
{




















}

/*
 * tbm_free - free a TIDBitmap
 */
void
tbm_free(TIDBitmap *tbm)
{













}

/*
 * tbm_free_shared_area - free shared state
 *
 * Free shared iterator state, Also free shared pagetable and iterator arrays
 * memory if they are not referred by any of the shared iterator i.e recount
 * is becomes 0.
 */
void
tbm_free_shared_area(dsa_area *dsa, dsa_pointer dp)
{































}

/*
 * tbm_add_tuples - add some tuple IDs to a TIDBitmap
 *
 * If recheck is true, then the recheck flag will be set in the
 * TBMIterateResult when any of these tuples are reported out.
 */
void
tbm_add_tuples(TIDBitmap *tbm, const ItemPointer tids, int ntids, bool recheck)
{





























































}

/*
 * tbm_add_page - add a whole page to a TIDBitmap
 *
 * This causes the whole page to be reported (with the recheck flag)
 * when the TIDBitmap is scanned.
 */
void
tbm_add_page(TIDBitmap *tbm, BlockNumber pageno)
{







}

/*
 * tbm_union - set union
 *
 * a is modified in-place, b is not changed
 */
void
tbm_union(TIDBitmap *a, const TIDBitmap *b)
{























}

/* Process one page of b during a union op */
static void
tbm_union_page(TIDBitmap *a, const PagetableEntry *bpage)
{























































}

/*
 * tbm_intersect - set intersection
 *
 * a is modified in-place, b is not changed
 */
void
tbm_intersect(TIDBitmap *a, const TIDBitmap *b)
{















































}

/*
 * Process one page of a during an intersection op
 *
 * Returns true if apage is now empty and should be deleted from a
 */
static bool
tbm_intersect_page(TIDBitmap *a, PagetableEntry *apage, const TIDBitmap *b)
{












































































}

/*
 * tbm_is_empty - is a TIDBitmap completely empty?
 */
bool
tbm_is_empty(const TIDBitmap *tbm)
{

}

/*
 * tbm_begin_iterate - prepare to iterate through a TIDBitmap
 *
 * The TBMIterator struct is created in the caller's memory context.
 * For a clean shutdown of the iteration, call tbm_end_iterate; but it's
 * okay to just allow the memory context to be released, too.  It is caller's
 * responsibility not to touch the TBMIterator anymore once the TIDBitmap
 * is freed.
 *
 * NB: after this is called, it is no longer allowed to modify the contents
 * of the bitmap.  However, you can call this multiple times to scan the
 * contents repeatedly, including parallel scans.
 */
TBMIterator *
tbm_begin_iterate(TIDBitmap *tbm)
{




































































}

/*
 * tbm_prepare_shared_iterate - prepare shared iteration state for a TIDBitmap.
 *
 * The necessary shared state will be allocated from the DSA passed to
 * tbm_create, so that multiple processes can attach to it and iterate jointly.
 *
 * This will convert the pagetable hash into page and chunk array of the index
 * into pagetable array.
 */
dsa_pointer
tbm_prepare_shared_iterate(TIDBitmap *tbm)
{


















































































































































}

/*
 * tbm_extract_page_tuple - extract the tuple offsets from a page
 *
 * The extracted offsets are stored into TBMIterateResult.
 */
static inline int
tbm_extract_page_tuple(PagetableEntry *page, TBMIterateResult *output)
{
























}

/*
 *	tbm_advance_schunkbit - Advance the schunkbit
 */
static inline void
tbm_advance_schunkbit(PagetableEntry *chunk, int *schunkbitp)
{















}

/*
 * tbm_iterate - scan through next page of a TIDBitmap
 *
 * Returns a TBMIterateResult representing one page, or NULL if there are
 * no more pages to scan.  Pages are guaranteed to be delivered in numerical
 * order.  If result->ntuples < 0, then the bitmap is "lossy" and failed to
 * remember the exact tuples to look at on this page --- the caller must
 * examine all tuples on the page and check if they meet the intended
 * condition.  If result->recheck is true, only the indicated tuples need
 * be examined, but the condition must be rechecked anyway.  (For ease of
 * testing, recheck is always set true when ntuples < 0.)
 */
TBMIterateResult *
tbm_iterate(TBMIterator *iterator)
{








































































}

/*
 *	tbm_shared_iterate - scan through next page of a TIDBitmap
 *
 *	As above, but this will iterate using an iterator which is shared
 *	across multiple processes.  We need to acquire the iterator LWLock,
 *	before accessing the shared members.
 */
TBMIterateResult *
tbm_shared_iterate(TBMSharedIterator *iterator)
{























































































}

/*
 * tbm_end_iterate - finish an iteration over a TIDBitmap
 *
 * Currently this is just a pfree, but it might do more someday.  (For
 * instance, it could be useful to count open iterators and allow the
 * bitmap to return to read/write status when there are no more iterators.)
 */
void
tbm_end_iterate(TBMIterator *iterator)
{

}

/*
 * tbm_end_shared_iterate - finish a shared iteration over a TIDBitmap
 *
 * This doesn't free any of the shared state associated with the iterator,
 * just our backend-private state.
 */
void
tbm_end_shared_iterate(TBMSharedIterator *iterator)
{

}

/*
 * tbm_find_pageentry - find a PagetableEntry for the pageno
 *
 * Returns NULL if there is no non-lossy entry for the pageno.
 */
static const PagetableEntry *
tbm_find_pageentry(const TIDBitmap *tbm, BlockNumber pageno)
{




























}

/*
 * tbm_get_pageentry - find or create a PagetableEntry for the pageno
 *
 * If new, the entry is marked as an exact (non-chunk) entry.
 *
 * This may cause the table to exceed the desired memory size.  It is
 * up to the caller to call tbm_lossify() at the next safe point if so.
 */
static PagetableEntry *
tbm_get_pageentry(TIDBitmap *tbm, BlockNumber pageno)
{









































}

/*
 * tbm_page_is_lossy - is the page marked as lossily stored?
 */
static bool
tbm_page_is_lossy(const TIDBitmap *tbm, BlockNumber pageno)
{



























}

/*
 * tbm_mark_page_lossy - mark the page number as lossily stored
 *
 * This may cause the table to exceed the desired memory size.  It is
 * up to the caller to call tbm_lossify() at the next safe point if so.
 */
static void
tbm_mark_page_lossy(TIDBitmap *tbm, BlockNumber pageno)
{


































































}

/*
 * tbm_lossify - lose some information to get back under the memory limit
 */
static void
tbm_lossify(TIDBitmap *tbm)
{



































































}

/*
 * qsort comparator to handle PagetableEntry pointers.
 */
static int
tbm_comparator(const void *left, const void *right)
{












}

/*
 * As above, but this will get index into PagetableEntry array.  Therefore,
 * it needs to get actual PagetableEntry using the index before comparing the
 * blockno.
 */
static int
tbm_shared_comparator(const void *left, const void *right, void *arg)
{













}

/*
 *	tbm_attach_shared_iterate
 *
 *	Allocate a backend-private iterator and attach the shared iterator state
 *	to it so that multiple processed can iterate jointly.
 *
 *	We also converts the DSA pointers to local pointers and store them into
 *	our private iterator.
 */
TBMSharedIterator *
tbm_attach_shared_iterate(dsa_area *dsa, dsa_pointer dp)
{

























}

/*
 * pagetable_allocate
 *
 * Callback function for allocating the memory for hashtable elements.
 * Allocate memory for hashtable elements, using DSA if available.
 */
static inline void *
pagetable_allocate(pagetable_hash *pagetable, Size size)
{

















}

/*
 * pagetable_free
 *
 * Callback function for freeing hash table elements.
 */
static inline void
pagetable_free(pagetable_hash *pagetable, void *pointer)
{












}

/*
 * tbm_calculate_entries
 *
 * Estimate number of hashtable entries we can have within maxbytes.
 */
long
tbm_calculate_entries(double maxbytes)
{













}