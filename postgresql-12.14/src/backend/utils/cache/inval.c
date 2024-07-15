                                                                            
   
           
                                                  
   
                                           
   
                                                                     
                                                                            
                                                                       
                                                                           
                                                                             
                                                                            
                                                                       
                                                                              
                                                                          
                                                                            
                                                                           
                                                                       
                                                                     
                                                                            
                                                                    
   
                                                                         
                                                                           
                                                                             
                                                                          
                                                                   
   
                                                                         
                                                                          
                                                                            
                                                                          
                                                            
   
                                                                       
                                                                         
                                                   
   
                                                                       
                                                                          
                                                                        
                                                                           
   
                                                                            
                                                                           
                                                                        
                                                                        
                                                                           
                                                                 
                                                                           
                                                      
   
                                                                      
                                                                           
                                                                         
                                                                    
   
                                                                           
                                                                           
                                                                         
                                                                            
                                                                        
                                                                   
                                                                           
                  
   
                                                                          
                                                                          
                                                                      
                                                                        
                                                                        
                                                                         
                                                                 
                                                                          
                                                                        
                                                                             
                                                                       
                                                                            
                                                        
   
                                                                       
                                                                          
                                                                           
                                                                         
                     
   
                                                                       
                                                                           
                                                                      
                                                                              
   
                                                                         
                                                                           
                                                                        
                                                                           
                                                                        
                                                                          
                                                                            
                                     
   
   
                                                                         
                                                                        
   
                  
                                     
   
                                                                            
   
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

   
                                                                         
                                                                         
                                                                         
                                                                          
   
typedef struct InvalidationChunk
{
  struct InvalidationChunk *next;                
  int nitems;                                                            
  int maxitems;                                                              
  SharedInvalidationMessage msgs[FLEXIBLE_ARRAY_MEMBER];
} InvalidationChunk;

typedef struct InvalidationListHeader
{
  InvalidationChunk *cclist;                                           
  InvalidationChunk *rclist;                                           
} InvalidationListHeader;

                   
                                                
                                                                     
                                                                     
                                                                      
                                                                    
                                
                                                                          
                                                                       
                                                                        
   
                                                                    
                                                                      
                                      
                   
   

typedef struct TransInvalidationInfo
{
                                              
  struct TransInvalidationInfo *parent;

                                    
  int my_level;

                                          
  InvalidationListHeader CurrentCmdInvalidMsgs;

                                            
  InvalidationListHeader PriorCmdInvalidMsgs;

                                      
  bool RelcacheInitFileInval;
} TransInvalidationInfo;

static TransInvalidationInfo *transInvalInfo = NULL;

static SharedInvalidationMessage *SharedInvalidMessagesArray;
static int numSharedInvalidMessagesArray;
static int maxSharedInvalidMessagesArray;

   
                                                                      
                                                                             
                                                  
   
                                                                          
                                                                              
                                                                             
   

#define MAX_SYSCACHE_CALLBACKS 64
#define MAX_RELCACHE_CALLBACKS 10

static struct SYSCACHECALLBACK
{
  int16 id;                     
  int16 link;                                           
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

                                                                    
                                          
   
                                                                
                                                                
                                                                    
   

   
                          
                                                       
   
                                                                           
                             
   
static void
AddInvalidationMessage(InvalidationChunk **listHdr, SharedInvalidationMessage *msg)
{
  InvalidationChunk *chunk = *listHdr;

  if (chunk == NULL)
  {
                                                  
#define FIRSTCHUNKSIZE 32
    chunk = (InvalidationChunk *)MemoryContextAlloc(CurTransactionContext, offsetof(InvalidationChunk, msgs) + FIRSTCHUNKSIZE * sizeof(SharedInvalidationMessage));
    chunk->nitems = 0;
    chunk->maxitems = FIRSTCHUNKSIZE;
    chunk->next = *listHdr;
    *listHdr = chunk;
  }
  else if (chunk->nitems >= chunk->maxitems)
  {
                                                       
    int chunksize = 2 * chunk->maxitems;

    chunk = (InvalidationChunk *)MemoryContextAlloc(CurTransactionContext, offsetof(InvalidationChunk, msgs) + chunksize * sizeof(SharedInvalidationMessage));
    chunk->nitems = 0;
    chunk->maxitems = chunksize;
    chunk->next = *listHdr;
    *listHdr = chunk;
  }
                                          
  chunk->msgs[chunk->nitems] = *msg;
  chunk->nitems++;
}

   
                                                                        
                                          
   
static void
AppendInvalidationMessageList(InvalidationChunk **destHdr, InvalidationChunk **srcHdr)
{
  InvalidationChunk *chunk = *srcHdr;

  if (chunk == NULL)
  {
    return;                    
  }

  while (chunk->next != NULL)
  {
    chunk = chunk->next;
  }

  chunk->next = *destHdr;

  *destHdr = *srcHdr;

  *srcHdr = NULL;
}

   
                                            
   
                                                                             
                                                                            
   
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

                                                                    
                                         
   
                                                                          
                                                                        
                                                                    
   

   
                              
   
static void
AddCatcacheInvalidationMessage(InvalidationListHeader *hdr, int id, uint32 hashValue, Oid dbId)
{
  SharedInvalidationMessage msg;

  Assert(id < CHAR_MAX);
  msg.cc.id = (int8)id;
  msg.cc.dbId = dbId;
  msg.cc.hashValue = hashValue;

     
                                                                     
                                                                         
                                                                     
                                                                        
                                                                             
                                                                             
           
     
  VALGRIND_MAKE_MEM_DEFINED(&msg, sizeof(msg));

  AddInvalidationMessage(&hdr->cclist, &msg);
}

   
                                   
   
static void
AddCatalogInvalidationMessage(InvalidationListHeader *hdr, Oid dbId, Oid catId)
{
  SharedInvalidationMessage msg;

  msg.cat.id = SHAREDINVALCATALOG_ID;
  msg.cat.dbId = dbId;
  msg.cat.catId = catId;
                                                                 
  VALGRIND_MAKE_MEM_DEFINED(&msg, sizeof(msg));

  AddInvalidationMessage(&hdr->cclist, &msg);
}

   
                              
   
static void
AddRelcacheInvalidationMessage(InvalidationListHeader *hdr, Oid dbId, Oid relId)
{
  SharedInvalidationMessage msg;

     
                                                                            
                                                                          
                                                           
     
  ProcessMessageList(hdr->rclist, if (msg->rc.id == SHAREDINVALRELCACHE_ID && (msg->rc.relId == relId || msg->rc.relId == InvalidOid)) return);

                        
  msg.rc.id = SHAREDINVALRELCACHE_ID;
  msg.rc.dbId = dbId;
  msg.rc.relId = relId;
                                                                 
  VALGRIND_MAKE_MEM_DEFINED(&msg, sizeof(msg));

  AddInvalidationMessage(&hdr->rclist, &msg);
}

   
                              
   
static void
AddSnapshotInvalidationMessage(InvalidationListHeader *hdr, Oid dbId, Oid relId)
{
  SharedInvalidationMessage msg;

                                  
                                                                       
  ProcessMessageList(hdr->rclist, if (msg->sn.id == SHAREDINVALSNAPSHOT_ID && msg->sn.relId == relId) return);

                        
  msg.sn.id = SHAREDINVALSNAPSHOT_ID;
  msg.sn.dbId = dbId;
  msg.sn.relId = relId;
                                                                 
  VALGRIND_MAKE_MEM_DEFINED(&msg, sizeof(msg));

  AddInvalidationMessage(&hdr->rclist, &msg);
}

   
                                                                  
                             
   
static void
AppendInvalidationMessages(InvalidationListHeader *dest, InvalidationListHeader *src)
{
  AppendInvalidationMessageList(&dest->cclist, &src->cclist);
  AppendInvalidationMessageList(&dest->rclist, &src->rclist);
}

   
                                                                            
                            
   
                                                                      
   
static void
ProcessInvalidationMessages(InvalidationListHeader *hdr, void (*func)(SharedInvalidationMessage *msg))
{
  ProcessMessageList(hdr->cclist, func(msg));
  ProcessMessageList(hdr->rclist, func(msg));
}

   
                                                                      
                                   
   
static void
ProcessInvalidationMessagesMulti(InvalidationListHeader *hdr, void (*func)(const SharedInvalidationMessage *msgs, int n))
{
  ProcessMessageListMulti(hdr->cclist, func(msgs, n));
  ProcessMessageListMulti(hdr->rclist, func(msgs, n));
}

                                                                    
                                   
                                                                    
   

   
                                
   
                                                              
   
static void
RegisterCatcacheInvalidation(int cacheId, uint32 hashValue, Oid dbId)
{
  AddCatcacheInvalidationMessage(&transInvalInfo->CurrentCmdInvalidMsgs, cacheId, hashValue, dbId);
}

   
                               
   
                                                                           
   
static void
RegisterCatalogInvalidation(Oid dbId, Oid catId)
{
  AddCatalogInvalidationMessage(&transInvalInfo->CurrentCmdInvalidMsgs, dbId, catId);
}

   
                                
   
                                                         
   
static void
RegisterRelcacheInvalidation(Oid dbId, Oid relId)
{
  AddRelcacheInvalidationMessage(&transInvalInfo->CurrentCmdInvalidMsgs, dbId, relId);

     
                                                                       
                                                                            
                                                                          
                                                  
     
  (void)GetCurrentCommandId(true);

     
                                                                            
                                                                             
                                                                             
                                                                 
     
  if (relId == InvalidOid || RelationIdIsInInitFile(relId))
  {
    transInvalInfo->RelcacheInitFileInval = true;
  }
}

   
                                
   
                                                                          
                                                       
   
static void
RegisterSnapshotInvalidation(Oid dbId, Oid relId)
{
  AddSnapshotInvalidationMessage(&transInvalInfo->CurrentCmdInvalidMsgs, dbId, relId);
}

   
                                   
   
                                                                       
                                                                         
                      
   
void
LocalExecuteInvalidationMessage(SharedInvalidationMessage *msg)
{
  if (msg->id >= 0)
  {
    if (msg->cc.dbId == MyDatabaseId || msg->cc.dbId == InvalidOid)
    {
      InvalidateCatalogSnapshot();

      SysCacheInvalidate(msg->cc.id, msg->cc.hashValue);

      CallSyscacheCallbacks(msg->cc.id, msg->cc.hashValue);
    }
  }
  else if (msg->id == SHAREDINVALCATALOG_ID)
  {
    if (msg->cat.dbId == MyDatabaseId || msg->cat.dbId == InvalidOid)
    {
      InvalidateCatalogSnapshot();

      CatalogCacheFlushCatalog(msg->cat.catId);

                                                                          
    }
  }
  else if (msg->id == SHAREDINVALRELCACHE_ID)
  {
    if (msg->rc.dbId == MyDatabaseId || msg->rc.dbId == InvalidOid)
    {
      int i;

      if (msg->rc.relId == InvalidOid)
      {
        RelationCacheInvalidate(false);
      }
      else
      {
        RelationCacheInvalidateEntry(msg->rc.relId);
      }

      for (i = 0; i < relcache_callback_count; i++)
      {
        struct RELCACHECALLBACK *ccitem = relcache_callback_list + i;

        ccitem->function(ccitem->arg, msg->rc.relId);
      }
    }
  }
  else if (msg->id == SHAREDINVALSMGR_ID)
  {
       
                                                                          
                                            
       
    RelFileNodeBackend rnode;

    rnode.node = msg->sm.rnode;
    rnode.backend = (msg->sm.backend_hi << 16) | (int)msg->sm.backend_lo;
    smgrclosenode(rnode);
  }
  else if (msg->id == SHAREDINVALRELMAP_ID)
  {
                                                                 
    if (msg->rm.dbId == InvalidOid)
    {
      RelationMapInvalidate(true);
    }
    else if (msg->rm.dbId == MyDatabaseId)
    {
      RelationMapInvalidate(false);
    }
  }
  else if (msg->id == SHAREDINVALSNAPSHOT_ID)
  {
                                                                 
    if (msg->sn.dbId == InvalidOid)
    {
      InvalidateCatalogSnapshot();
    }
    else if (msg->sn.dbId == MyDatabaseId)
    {
      InvalidateCatalogSnapshot();
    }
  }
  else
  {
    elog(FATAL, "unrecognized SI message ID: %d", msg->id);
  }
}

   
                           
   
                                                                
                                                                
                                                                        
   
                                                                   
                                                                        
                                             
   
void
InvalidateSystemCaches(void)
{
  InvalidateSystemCachesExtended(false);
}

void
InvalidateSystemCachesExtended(bool debug_discard)
{
  int i;

  InvalidateCatalogSnapshot();
  ResetCatalogCaches();
  RelationCacheInvalidate(debug_discard);                               

  for (i = 0; i < syscache_callback_count; i++)
  {
    struct SYSCACHECALLBACK *ccitem = syscache_callback_list + i;

    ccitem->function(ccitem->arg, ccitem->id, 0);
  }

  for (i = 0; i < relcache_callback_count; i++)
  {
    struct RELCACHECALLBACK *ccitem = relcache_callback_list + i;

    ccitem->function(ccitem->arg, InvalidOid);
  }
}

                                                                    
                          
                                                                    
   

   
                              
                                                                        
                   
   
         
                                                                         
   
void
AcceptInvalidationMessages(void)
{
  ReceiveSharedInvalidMessages(LocalExecuteInvalidationMessage, InvalidateSystemCaches);

     
                                                                    
     
                                                                        
                                                                           
                                                                            
                                                    
     
                                                                             
                                                                       
                                                                             
                                                                         
                                                                        
                                                        
     
#if defined(CLOBBER_CACHE_ALWAYS)
  {
    static bool in_recursion = false;

    if (!in_recursion)
    {
      in_recursion = true;
      InvalidateSystemCachesExtended(true);
      in_recursion = false;
    }
  }
#elif defined(CLOBBER_CACHE_RECURSIVELY)
  {
    static int recursion_depth = 0;

                                                                        
    if (recursion_depth < 3)
    {
      recursion_depth++;
      InvalidateSystemCachesExtended(true);
      recursion_depth--;
    }
  }
#endif
}

   
                            
                                                             
   
static void
PrepareInvalidationState(void)
{
  TransInvalidationInfo *myInfo;

  if (transInvalInfo != NULL && transInvalInfo->my_level == GetCurrentTransactionNestLevel())
  {
    return;
  }

  myInfo = (TransInvalidationInfo *)MemoryContextAllocZero(TopTransactionContext, sizeof(TransInvalidationInfo));
  myInfo->parent = transInvalInfo;
  myInfo->my_level = GetCurrentTransactionNestLevel();

     
                                                                            
            
     
  Assert(transInvalInfo == NULL || myInfo->my_level > transInvalInfo->my_level);

  transInvalInfo = myInfo;
}

   
                     
                                       
   
                                                                           
                                                                             
                                                                       
   
                                                                          
                                                                         
                       
   
void
PostPrepare_Inval(void)
{
  AtEOXact_Inval(false);
}

   
                                                                        
   
static void
MakeSharedInvalidMessagesArray(const SharedInvalidationMessage *msgs, int n)
{
     
                                                        
     
  if (SharedInvalidMessagesArray == NULL)
  {
    maxSharedInvalidMessagesArray = FIRSTCHUNKSIZE;
    numSharedInvalidMessagesArray = 0;

       
                                                                           
                                                                           
       
    SharedInvalidMessagesArray = palloc(maxSharedInvalidMessagesArray * sizeof(SharedInvalidationMessage));
  }

  if ((numSharedInvalidMessagesArray + n) > maxSharedInvalidMessagesArray)
  {
    while ((numSharedInvalidMessagesArray + n) > maxSharedInvalidMessagesArray)
    {
      maxSharedInvalidMessagesArray *= 2;
    }

    SharedInvalidMessagesArray = repalloc(SharedInvalidMessagesArray, maxSharedInvalidMessagesArray * sizeof(SharedInvalidationMessage));
  }

     
                                          
     
  memcpy(SharedInvalidMessagesArray + numSharedInvalidMessagesArray, msgs, n * sizeof(SharedInvalidationMessage));
  numSharedInvalidMessagesArray += n;
}

   
                                                         
                                                                   
                                                                      
                                                                      
                                    
   
                                                                      
                                                                       
                                            
   
                                                      
   
int
xactGetCommittedInvalidationMessages(SharedInvalidationMessage **msgs, bool *RelcacheInitFileInval)
{
  MemoryContext oldcontext;

                                                                          
  if (transInvalInfo == NULL)
  {
    *RelcacheInitFileInval = false;
    *msgs = NULL;
    return 0;
  }

                               
  Assert(transInvalInfo->my_level == 1 && transInvalInfo->parent == NULL);

     
                                                                         
                                                                             
                   
     
  *RelcacheInitFileInval = transInvalInfo->RelcacheInitFileInval;

     
                                                                           
                                                                             
                                                                            
                                                                             
                                                                           
                           
     
  oldcontext = MemoryContextSwitchTo(CurTransactionContext);

  ProcessInvalidationMessagesMulti(&transInvalInfo->CurrentCmdInvalidMsgs, MakeSharedInvalidMessagesArray);
  ProcessInvalidationMessagesMulti(&transInvalInfo->PriorCmdInvalidMsgs, MakeSharedInvalidMessagesArray);
  MemoryContextSwitchTo(oldcontext);

  Assert(!(numSharedInvalidMessagesArray > 0 && SharedInvalidMessagesArray == NULL));

  *msgs = SharedInvalidMessagesArray;

  return numSharedInvalidMessagesArray;
}

   
                                                                             
                                                                           
                        
   
                                                            
                                                                  
   
void
ProcessCommittedInvalidationMessages(SharedInvalidationMessage *msgs, int nmsgs, bool RelcacheInitFileInval, Oid dbid, Oid tsid)
{
  if (nmsgs <= 0)
  {
    return;
  }

  elog(trace_recovery(DEBUG4), "replaying commit with %d messages%s", nmsgs, (RelcacheInitFileInval ? " and relcache file invalidation" : ""));

  if (RelcacheInitFileInval)
  {
    elog(trace_recovery(DEBUG4), "removing relcache init files for database %u", dbid);

       
                                                                         
                                                                           
                                                                   
                                                                         
                                                             
       
    if (OidIsValid(dbid))
    {
      DatabasePath = GetDatabasePath(dbid, tsid);
    }

    RelationCacheInitFilePreInvalidate();

    if (OidIsValid(dbid))
    {
      pfree(DatabasePath);
      DatabasePath = NULL;
    }
  }

  SendSharedInvalidMessages(msgs, nmsgs);

  if (RelcacheInitFileInval)
  {
    RelationCacheInitFilePostInvalidate();
  }
}

   
                  
                                                                        
   
                                                                              
                                                                           
                                                                       
                                                                        
                                                                      
                                                           
   
                                                                           
                                                                        
                                                                         
                                                                        
                   
   
                                                                          
                                                                        
           
   
         
                                                                        
   
void
AtEOXact_Inval(bool isCommit)
{
                                 
  if (transInvalInfo == NULL)
  {
    return;
  }

                               
  Assert(transInvalInfo->my_level == 1 && transInvalInfo->parent == NULL);

  if (isCommit)
  {
       
                                                                           
                                                                        
                            
       
    if (transInvalInfo->RelcacheInitFileInval)
    {
      RelationCacheInitFilePreInvalidate();
    }

    AppendInvalidationMessages(&transInvalInfo->PriorCmdInvalidMsgs, &transInvalInfo->CurrentCmdInvalidMsgs);

    ProcessInvalidationMessagesMulti(&transInvalInfo->PriorCmdInvalidMsgs, SendSharedInvalidMessages);

    if (transInvalInfo->RelcacheInitFileInval)
    {
      RelationCacheInitFilePostInvalidate();
    }
  }
  else
  {
    ProcessInvalidationMessages(&transInvalInfo->PriorCmdInvalidMsgs, LocalExecuteInvalidationMessage);
  }

                                         
  transInvalInfo = NULL;
  SharedInvalidMessagesArray = NULL;
  numSharedInvalidMessagesArray = 0;
}

   
                     
                                                                      
   
                                                                              
                                                                             
                                      
   
                                                                           
                                                                        
                                                                              
                           
   
                                                                               
                                                                   
                                                                          
                        
   
void
AtEOSubXact_Inval(bool isCommit)
{
  int my_level;
  TransInvalidationInfo *myInfo = transInvalInfo;

                                  
  if (myInfo == NULL)
  {
    return;
  }

                                                                 
  my_level = GetCurrentTransactionNestLevel();
  if (myInfo->my_level != my_level)
  {
    Assert(myInfo->my_level < my_level);
    return;
  }

  if (isCommit)
  {
                                                             
    CommandEndInvalidationMessages();

       
                                                                        
                                                                         
                                                                         
              
       
    if (myInfo->parent == NULL || myInfo->parent->my_level < my_level - 1)
    {
      myInfo->my_level--;
      return;
    }

                                             
    AppendInvalidationMessages(&myInfo->parent->PriorCmdInvalidMsgs, &myInfo->PriorCmdInvalidMsgs);

                                                             
    if (myInfo->RelcacheInitFileInval)
    {
      myInfo->parent->RelcacheInitFileInval = true;
    }

                                         
    transInvalInfo = myInfo->parent;

                                                
    pfree(myInfo);
  }
  else
  {
    ProcessInvalidationMessages(&myInfo->PriorCmdInvalidMsgs, LocalExecuteInvalidationMessage);

                                         
    transInvalInfo = myInfo->parent;

                                                
    pfree(myInfo);
  }
}

   
                                  
                                                                  
                      
   
                                                                             
                                                                            
                                                                          
                                                                           
                           
   
         
                                                            
                                           
   
void
CommandEndInvalidationMessages(void)
{
     
                                                                           
                                                                            
                                                 
     
  if (transInvalInfo == NULL)
  {
    return;
  }

  ProcessInvalidationMessages(&transInvalInfo->CurrentCmdInvalidMsgs, LocalExecuteInvalidationMessage);
  AppendInvalidationMessages(&transInvalInfo->PriorCmdInvalidMsgs, &transInvalInfo->CurrentCmdInvalidMsgs);
}

   
                            
                                                                
                                                               
                                                             
   
                                                                            
                                                                          
                                                                             
                            
   
void
CacheInvalidateHeapTuple(Relation relation, HeapTuple tuple, HeapTuple newtuple)
{
  Oid tupleRelId;
  Oid databaseId;
  Oid relationId;

                                   
  if (IsBootstrapProcessingMode())
  {
    return;
  }

     
                                                                            
                                                                            
                          
     
  if (!IsCatalogRelation(relation))
  {
    return;
  }

     
                                                                     
                                                      
     
  if (IsToastRelation(relation))
  {
    return;
  }

     
                                                                   
                                          
     
  PrepareInvalidationState();

     
                                         
     
  tupleRelId = RelationGetRelid(relation);
  if (RelationInvalidatesSnapshotsOnly(tupleRelId))
  {
    databaseId = IsSharedRelation(tupleRelId) ? InvalidOid : MyDatabaseId;
    RegisterSnapshotInvalidation(databaseId, tupleRelId);
  }
  else
  {
    PrepareToInvalidateCacheTuple(relation, tuple, newtuple, RegisterCatcacheInvalidation);
  }

     
                                                                             
                                                     
     
                                                                           
                                                                     
     
  if (tupleRelId == RelationRelationId)
  {
    Form_pg_class classtup = (Form_pg_class)GETSTRUCT(tuple);

    relationId = classtup->oid;
    if (classtup->relisshared)
    {
      databaseId = InvalidOid;
    }
    else
    {
      databaseId = MyDatabaseId;
    }
  }
  else if (tupleRelId == AttributeRelationId)
  {
    Form_pg_attribute atttup = (Form_pg_attribute)GETSTRUCT(tuple);

    relationId = atttup->attrelid;

       
                                                                         
                                                                           
                                                                       
                                                                  
                                                                         
                                                                         
                                                                         
                                                 
       
    databaseId = MyDatabaseId;
  }
  else if (tupleRelId == IndexRelationId)
  {
    Form_pg_index indextup = (Form_pg_index)GETSTRUCT(tuple);

       
                                                                           
                                                                          
                                                                        
                                                
       
    relationId = indextup->indexrelid;
    databaseId = MyDatabaseId;
  }
  else if (tupleRelId == ConstraintRelationId)
  {
    Form_pg_constraint constrtup = (Form_pg_constraint)GETSTRUCT(tuple);

       
                                                                      
                                                   
       
    if (constrtup->contype == CONSTRAINT_FOREIGN && OidIsValid(constrtup->conrelid))
    {
      relationId = constrtup->conrelid;
      databaseId = MyDatabaseId;
    }
    else
    {
      return;
    }
  }
  else
  {
    return;
  }

     
                                                              
     
  RegisterRelcacheInvalidation(databaseId, relationId);
}

   
                          
                                                                    
   
                                                                          
                                                                           
                                                                         
   
                                                                      
                                                                               
   
void
CacheInvalidateCatalog(Oid catalogId)
{
  Oid databaseId;

  PrepareInvalidationState();

  if (IsSharedRelation(catalogId))
  {
    databaseId = InvalidOid;
  }
  else
  {
    databaseId = MyDatabaseId;
  }

  RegisterCatalogInvalidation(databaseId, catalogId);
}

   
                           
                                                                     
                       
   
                                                                         
                                                                         
                                                                          
   
void
CacheInvalidateRelcache(Relation relation)
{
  Oid databaseId;
  Oid relationId;

  PrepareInvalidationState();

  relationId = RelationGetRelid(relation);
  if (relation->rd_rel->relisshared)
  {
    databaseId = InvalidOid;
  }
  else
  {
    databaseId = MyDatabaseId;
  }

  RegisterRelcacheInvalidation(databaseId, relationId);
}

   
                              
                                                                       
   
                                                                           
                           
   
void
CacheInvalidateRelcacheAll(void)
{
  PrepareInvalidationState();

  RegisterRelcacheInvalidation(InvalidOid, InvalidOid);
}

   
                                  
                                                                        
   
void
CacheInvalidateRelcacheByTuple(HeapTuple classTuple)
{
  Form_pg_class classtup = (Form_pg_class)GETSTRUCT(classTuple);
  Oid databaseId;
  Oid relationId;

  PrepareInvalidationState();

  relationId = classtup->oid;
  if (classtup->relisshared)
  {
    databaseId = InvalidOid;
  }
  else
  {
    databaseId = MyDatabaseId;
  }
  RegisterRelcacheInvalidation(databaseId, relationId);
}

   
                                  
                                                             
                                                                 
                                                                 
   
void
CacheInvalidateRelcacheByRelid(Oid relid)
{
  HeapTuple tup;

  PrepareInvalidationState();

  tup = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  CacheInvalidateRelcacheByTuple(tup);
  ReleaseSysCache(tup);
}

   
                       
                                                                     
   
                                                                             
                                                                              
                                                                            
                                                                            
                                                           
   
                                                                            
                                                                              
                                                                             
                                                                             
                                              
   
                                                                             
                                                                         
                                                                        
                                              
   
                                                                             
                                                                              
                                                    
   
void
CacheInvalidateSmgr(RelFileNodeBackend rnode)
{
  SharedInvalidationMessage msg;

  msg.sm.id = SHAREDINVALSMGR_ID;
  msg.sm.backend_hi = rnode.backend >> 16;
  msg.sm.backend_lo = rnode.backend & 0xffff;
  msg.sm.rnode = rnode.node;
                                                                 
  VALGRIND_MAKE_MEM_DEFINED(&msg, sizeof(msg));

  SendSharedInvalidMessages(&msg, 1);
}

   
                         
                                                                  
                                                      
   
                                                                          
                                                                        
                                                                             
                                                                   
   
                                                                             
                                                                           
                                                                             
                                              
   
void
CacheInvalidateRelmap(Oid databaseId)
{
  SharedInvalidationMessage msg;

  msg.rm.id = SHAREDINVALRELMAP_ID;
  msg.rm.dbId = databaseId;
                                                                 
  VALGRIND_MAKE_MEM_DEFINED(&msg, sizeof(msg));

  SendSharedInvalidMessages(&msg, 1);
}

   
                                 
                                                                
                                                                      
                                                                    
              
   
                                                                              
                                                                   
                                                                            
                                                                           
                                  
   
void
CacheRegisterSyscacheCallback(int cacheid, SyscacheCallbackFunction func, Datum arg)
{
  if (cacheid < 0 || cacheid >= SysCacheSize)
  {
    elog(FATAL, "invalid cache ID: %d", cacheid);
  }
  if (syscache_callback_count >= MAX_SYSCACHE_CALLBACKS)
  {
    elog(FATAL, "out of syscache_callback_list slots");
  }

  if (syscache_callback_links[cacheid] == 0)
  {
                                       
    syscache_callback_links[cacheid] = syscache_callback_count + 1;
  }
  else
  {
                                                                       
    int i = syscache_callback_links[cacheid] - 1;

    while (syscache_callback_list[i].link > 0)
    {
      i = syscache_callback_list[i].link - 1;
    }
    syscache_callback_list[i].link = syscache_callback_count + 1;
  }

  syscache_callback_list[syscache_callback_count].id = cacheid;
  syscache_callback_list[syscache_callback_count].link = 0;
  syscache_callback_list[syscache_callback_count].function = func;
  syscache_callback_list[syscache_callback_count].arg = arg;

  ++syscache_callback_count;
}

   
                                 
                                                                
                                                                 
                                                
   
                                                                         
                                                                   
   
void
CacheRegisterRelcacheCallback(RelcacheCallbackFunction func, Datum arg)
{
  if (relcache_callback_count >= MAX_RELCACHE_CALLBACKS)
  {
    elog(FATAL, "out of relcache_callback_list slots");
  }

  relcache_callback_list[relcache_callback_count].function = func;
  relcache_callback_list[relcache_callback_count].arg = arg;

  ++relcache_callback_count;
}

   
                         
   
                                                                         
                                                                             
   
void
CallSyscacheCallbacks(int cacheid, uint32 hashvalue)
{
  int i;

  if (cacheid < 0 || cacheid >= SysCacheSize)
  {
    elog(ERROR, "invalid cache ID: %d", cacheid);
  }

  i = syscache_callback_links[cacheid] - 1;
  while (i >= 0)
  {
    struct SYSCACHECALLBACK *ccitem = syscache_callback_list + i;

    Assert(ccitem->id == cacheid);
    ccitem->function(ccitem->arg, cacheid, hashvalue);
    i = ccitem->link - 1;
  }
}
